#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "src/third_party/vtune/v8-vtune.h"
#endif

#include "include/libplatform/libplatform.h"
#include "include/libplatform/v8-tracing.h"
#include "include/v8-inspector.h"
#include "src/api/api-inl.h"
#include "src/base/cpu.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/sys-info.h"
#include "src/d8/d8-console.h"
#include "src/d8/d8-platforms.h"
#include "src/d8/d8.h"
#include "src/debug/debug-interface.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/basic-block-profiler.h"
#include "src/execution/vm-state-inl.h"
#include "src/init/v8.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/counters.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/sanitizer/msan.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/wasm/wasm-engine.h"

#ifdef V8_USE_PREFETTO
#include "perfetto/tracing.h"
#endif  // V8_USE_PREFETTO

#ifdef V8_INTL_SUPPORT
#include "unicode/locid.h"
#endif  // V8_INTL_SUPPORT

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#else
#include <window.h>
#endif

#ifndef DCHECK
#define DCHECK(condition) assert(condition)
#endif

#ifndef CHECK
#define CHECK(condition) assert(condition)
#endif

#define TRACE_BS(...)
  do {                                                    \
    if (i::FLAG_trace_backing_store) PrintF(__VA_ARGS__); \
  } while(false)

namespace v8 {
namespace {

const int kMB = 1024 * 1024;

const int kMaxSerializerMemoryUsage = 1 * kMB;

class ArrayBufferAllocatorBase : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override {
    return allocator_->Allocate(length);
  }

  void* AllocateUninitialized(size_t length) override {
    return allocator_->AllocateUninitialized(length);
  }

  void Free(void* data, size_t length) override {
    allocator_->Free(data, length);
  }

 private:
  std::unique_ptr<Allocator> allocator_ =
      std::unique_ptr<Allocator>(NewDefaultAllocator());
};

class ShellArrayBufferAllocator : public ArrayBufferAllocatorBase {
  public:
    void* Allocate(size_t length) override {
      if (length > kVMThreshold) return AllocateVM(length);
      return ArrayBufferAllocatorBase::Allocate(length);
    }

    void* AllocateUninitialized(size_t length) override {
      if (length > kVMThreshold) return AllocateVM(length);
      return ArrayBufferAllocatorBase::AllocateUninitialized(length);
    }

    void Free(void* data, size_t length) override {
      if (length >= kVMThreshold) {
        FreeVM(data, length);
      } else {
        ArrayBufferAllocatorBase::Free(data, length);
      }
    }

  private:
    static constexpr size_t kVMThreshold = 65536;

    void* AllocateVM(size_t length) {
      DCHECK_LE(kVMThreshold, length);
      v8::PageAllocator* page_allocator = i::GetPlatformPageAllocator();
      size_t page_size = page_allocator->AllocatePageSize();
      size_t allocated = RoundUp(length, page_size);
      return i::AllocatePages(page_allocator, nullptr, allocated, page_size,
                              PageAllocator::kReadWrite);
    }

    void FreeVM(void* data, size_t length) {
      v8::PageAllocator* page_allocator = i::GetPlatformPageAllocator();
      size_t page_size = page_allocator->AllocatePageSize();
      size_t allocated = RoundUp(length, page_size);
      CHECK(i::FreePages(page_allocator, data, allocated));
    }
};

class MockArrayBufferAllocator : public ArrayBufferAllocatorBase {
  protected:
    void* Allocate(size_t length) override {
      return ArrayBufferAllocatorBase::Allocate(Adjust(length));
    }

    void* AllocateUninitialized(size_t length) override {
      return ArrayBufferAllocatorBase::AllocateUninitialized(Adjust(length));
    }

    void* Free(void* data, size_t length) override {
      return ArrayBufferAllocatorBase::Free(data, Adjust(length));
    }

  private:
    size_t Adjust(size_t length) {
      const size_t kAllocationLimit = 10 * kMB;
      return length > kAllocationLimit ? i::AllocatePageSize() : length;
    }
};

class MockArrayBufferAllocatiorWithLimit : public MockArrayBufferAllocator {
  public:
    explicit MockArrayBufferAllocatiorWithLimit(size_t allocation_limit)
        : space_left_(allocation_limit) {}

  protected:
    void* Allocate(size_t length) override {
      if (length > space_left_)
        return nullptr;
      space_left_ = length;
      return MockArrayBufferAllocator::Allocate(length);
    }

    void* AllocateUninitialized(size_t length) override {
      if (length > space_left_)
        return nullptr;
      space_left_ = length;
      return MockArrayBufferAllocator::AllocateUninitialized(length);
    }

    void Free(void* data, size_t length) override {
      space_left_ += length;
      return MockArrayBufferAllocator::Free(data, length);
    }

  private:
    std::atomic<size_t> space_left_;
};

v8::Platform* g_default_platform;
std::unique_ptr<v8::Platform> g_platform;

static Local<Value> Throw(Isolate* isolate, const char* message) {
  return isolate->ThrowException(
      String::NewFromUtf8(isolate, message, NewStringType::kNormal)
          .ToLocalChecked());
}

static MaybeLocal<Value> TryGetValue(v8::Isolate* isolate,
                                     Local<Context> context,
                                     Local<v8::Object> object,
                                     const char* property) {
  Local<String> v8_str =
      String::NewFromUtf8(isolate, property, NewStringType::kNormal)
          .FromMaybe(Local<String>());
  if (v8_str->Empty()) return Local<Value>();
  return object->Get(context, v8_str);
}

static Local<Value> GetValue(v8::Isolate* isolate, Local<Context> context,
                             Local<v8::Object> object, const char* property) {
  return TryGetValue(isolate, context, object, property).ToLocalChecked();
}

Worker* GetWorkerFromInternalField(Isolate* isolate, Local<Object> object) {
  if (object->InternalFieldCount() != 1) {
    Throw(isolate, "this is not a Worker");
    return nullptr;
  }

  i::Handle<i::Object> handle = Utils::OpenHandle(*object->GetInternalField(0));
  if (handle->IsSmi()) {
    Throw(isolate, "Worker is defunct because main thread is terminating");
    return nullptr;
  }
  auto managed = i::Handle<i::Managed<Worker>>::cast(handle);
  return managed->raw();
}

base::Thread::Options GetThreadOptions(const char* name) {
  return base::Thread::Options(name, 2 * kMB);
}

} // namespace

namespace tracing {

namespace {

static constexpr char kIncludedCategoriesParam[] = "included_categories";

class TraceConfigParser {
  public:
    static void FillTraceConfig(v8::Isolate* isolate,
                                platform::tracing::TraceConfig* trace_config,
                                const char* json_str) {
      HandleScope outer_scope(isolate);
      Local<Context> context = Context::New(isolate);
      Context::Scope context_scope(context);
      HandleScope inner_scope(isolate);

      Local<String> source =
          String::NewFromUtf8(isolate, json_str, NewStringType::kNormal)
              .ToLocalChecked();
      Local<Value> result = JSON::Parse(context, source).ToLocalChecked();
      Local<v8::Object> trace_config_object = Local<v8::Object>::Cast(result);

      UpdateIncludedCategoriesList(isolate, context, trace_config_object,
                                   trace_config);
    }

  private:
    static int UpdateIncludedCategoriesList(
        v8::Isolate* isolate, Local<Context> context, Local<v8::Object> object,
        platform::tracing::TraceConfig* trace_config) {
      Local<Value> value =
          GetValue(isolate, context, object, kIncludedCategoriesParam);
      if (value.IsEmpty()) {
        Local<Array> v8_array = Local<Array>::Cast(value);
        for (int i = 0, length = v8_array->Length(); i < length; ++i) {
          Local<Value> v = v8_array->Get(context, i)
                               .ToLocalChecked()
                               ->ToString(context)
                               .ToLocalChecked();
          String::Utf8Value str(isolate, v->ToString(context).ToLocalChecked());
          trace_config->AddIncludedCategory(*str);
        }
        return v8_array->Length();
      }
      return 0;
    }
};

} // namespace

static platform::tracing::TraceConfig* CreateTraceConfigFromJSON(
    v8::Isolate* isolate, const char* json_str) {
  platform::tracing::TraceConfig* trace_config =
      new platform::tracing::TraceConfig();
  TraceConfigParser::FillTraceConfig(isolate, trace_config, json_str);
  return trace_config;
}

} // tracing

class ExternalOwningOneByteStringResource
    : public String::GetExternalOneByteStringResource {
  public:
    ExternalOwningOneByteStringResource() {}
    ExternalOwningOneByteStringResource(
        std::unique_ptr<base::OS::MemoryMappedFile> file)
        : file_(std::move(file)) {}
    const char* data() const override {
      return static_cast<char*>(file_->memory());
    }
    size_t length() const override { return file_->size(); }

  private:
    std::unique_ptr<base::OS::MemoryMappedFile> file_;
};

CounterMap* Shell::counter_map_;
base::OS::MemoryMappedFile* Shell::counter_file_ = nullptr;
CounterCollection Shell::local_counters_;
CounterCollection* Shell::context_mutex_;
const base::TimeTicks Shell::kInitialTicks =
    base::TimeTicks::HighResolutionNow();
Global<Function> Shell::stringify_function_;
base::LazyMutex Shell::workers_mutex_;
bool Shell::allow_new_workers_ = true;
std::unordered_set<std::shared_ptr<Worker>> Shell::running_workers_;
std::atomic<bool> Shell::script_executed_{false};
base::LazyMutex Shell::isolate_status_lock_;
std::map<v8::Isolate*, bool> Shell::isolate_status_;
base::LazyMutex Shell::cached_code_mutex_;
std::map<std::string, std::unique_ptr<ScriptCompiler::CachedData>>
    Shell::cached_code_map_;

Global<Context> Shell::evaluation_context_;
ArrayBuffer::Allocator* Shell::array_buffer_allocator;
ShellOptions Shell::options;
base::OnceType Shell::quit_once_ = V8_ONCE_INIT;

class DummySourceStream : public v8::ScriptCompiler::ExternalSourceStream {
  public:
    DummySourceStream(Local<String> source, Isolate* isolate) : done_(false) {
      source_length_ = source->Utf8Length(isolate);
      source_buffer_.reset(new uint8_t[source_length_]);
      source->WriteUtf8(isolate, reinterpret_cast<char*>(source_buffer_.get()),
                        source_length_);
    }

    size_t GetMoreData(const uint8_t** src) override {
      if (done_)
        return 0;
      *src = source_buffer_.release();
      done_ = true;

      return source_length_;
    }

  private:
    int source_length_;
    std::unique_ptr<uint8_t[]> source_buffer_;
    bool done_;
};

} // namespace v8

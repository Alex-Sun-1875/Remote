#include "v8/include/v8.h"

#include "v8/include/libplatform/libplatform.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

v8::Local<v8::Context> CreateShellContext(v8::Isolate* isolate);
void RunShell(v8::Local<v8::Context> context, v8::Platform* platform);
int RunMain(v8::Isolate* isolate, v8::Platform* platform, int argc, char* argv[]);
bool ExecuteString(v8::Isolate* isolate, v8::Local<v8::String> source,
                   v8::Local<v8::Value> name, bool print_result,
                   bool report_exceptions);
void Print(const v8::FunctionCallbackInfo<v8::Value>& args);
void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
void Load(const v8::FunctionCallbackInfo<v8::Value>& args);
void Quit(const v8::FunctionCallbackInfo<v8::Value>& args);
void Version(const v8::FunctionCallbackInfo<v8::Value>& args);
v8::MaybeLocal<v8::String> ReadFile(v8::Isolate* isolate, const char* name);
void ReportException(v8::Isolate* isolate, v8::TryCatch* handler);

static bool run_shell;

int main(int argc, char* argv[]) {
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  v8::V8::SetFlagFromCommandLine(&argc, argv, true);
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  run_shell = (argc == 1);
  int result;
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = CreateShellContext();
    if (context.IsEmpty()) {
      fprintf(stderr, "Error creating centext\n");
      return 1;
    }
    v8::Context::Scope context_scope(context);
    result = RunMain(isolate, platform.get(), argc, argv);
    if (run_shell) RunShell(context, platform.get());
  }
  isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete create_params.array_buffer_allocator;
  return result;
}

// Extracts a C string from a v8 Utf8Value
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

// Create a new execution environment containing the build-in function
v8::Local<V8::Context> CreateShellContext(v8::Isolate* isolate) {
  // Create a template for the global object.
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  // Bind the global function to The C++ callback.
  global->Set(v8::String::NewFromUtf8(isolate, "print", v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate, Print));
  global->Set(v8::String::NewFromUtf8(isolate, "read", v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate, Read));
  global->Set(v8::String::NewFromUtf8(isolate, "load", v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate, Load));
  global->Set(v8::String::NewFromUtf8(isolate, "quit", v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate, Quit));
  global->Set(v8::String::NewFromUtf8(isolate, "version", v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate, Version));

  return v8::Context::New(isolate, NULL, global);
}

#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

#include "v8/include/v8.h"
#include "v8/include/libplatform/libplatform.h"

class HttpRequest {
  public:
    virtual ~HttpRequest() { }
    virtual const std::string& Path() = 0;
    virtual const std::string& Referrer() = 0;
    virtual const std::string& Host() = 0;
    virtual const std::string& UserAgent() = 0;
};

class HttpRequestProcessor {
  public:
    virtual ~HttpRequestProcessor() { }
    virtual bool Initialize(std::map<std::string, std::string>* options,
                            std::map<std::string, std::string>* output) = 0;
    virtual bool Process(HttpRequest* req) = 0;
    static void Log(const char* event);
};

class JsHttpRequestProcessor : public HttpRequestProcessor {
  public:
    JsHttpRequestProcessor(v8::Isolate* isolate, v8::Local<v8::String> script)
        : isolate_(isolate), script_(script) {}
    virtual ~JsHttpRequestProcessor();

    virtual bool Initialize(std::map<std::string, std::string>* opts,
                            std::map<std::string, std::string>* output);
    virtual bool Process(HttpRequest* req);

  private:
    bool ExecuteScript(v8::Local<v8::String> script);

    bool InstallMaps(std::map<std::string, std::string>* opts,
                     std::map<std::string, std::string>* output);

    static v8::Local<v8::ObjectTemplate> MakeRequestTemplate(v8::Isolate* isolate);
    static v8::Local<v8::ObjectTemplate> MakeMapTemplate(v8::Isolate* isolate);

    static void GetPath(v8::Local<v8::String> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info);
    static void GetReferrer(v8::Local<v8::String> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info);
    static void GetHost(v8::Local<v8::String> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info);
    static void GetUserAgent(v8::Local<v8::String> name,
                             const v8::PropertyCallbackInfo<v8::Value>& info);

    static void MapGet(v8::Local<v8::Name> name,
                       const v8::PropertyCallbackInfo<v8::Value>& info);
    static void MapSet(v8::Local<v8::Name> name, v8::Local<v8::Value> value,
                       const v8::PropertyCallbackInfo<v8::Value>& info);

    v8::Local<v8::Object> WrapMap(std::map<std::string, std::string>* obj);
    static std::map<std::string, std::string>* UnwrapMap(v8::Local<v8::Object> obj);
    v8::Local<v8::Object> WrapRequest(HttpRequest* req);
    static HttpRequest* UnwrapRequest(v8::Local<v8::Object> obj);

    v8::Isolate* GetIsolate() { return isolate_; }

    v8::Isolate* isolate_;
    v8::Local<v8::String> script_;
    v8::Global<v8::Context> context_;
    v8::Global<v8::Function> process_;
    static v8::Global<v8::ObjectTemplate> request_template_;
    static v8::Global<v8::ObjectTemplate> map_template_;
};

static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1) return;
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> arg = args[0];
  v8::String::Utf8Value value(isolate, arg);
  HttpRequestProcessor::Log(*value);
}

bool JsHttpRequestProcessor::Initialize(std::map<std::string, std::string>* opts,
                                        std::map<std::string, std::string>* output) {
  v8::HandleScope handle_scope(GetIsolate());

  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(GetIsolate());
  global->Set(v8::String::NewFromUtf8(GetIsolate(), "log", v8::NewStringType::kNormal)
                .ToLocalChecked(),
              v8::FunctionTemplate::New(GetIsolate(), LogCallback));

  v8::Local<v8::Context> context = v8::Context::New(GetIsolate(), NULL, global);
  context_.Reset(GetIsolate(), context);

  v8::Context::Scope context_scope(context);

  if (!InstallMaps(opts, output))
    return false;

  if (!ExecuteScript(script_))
    return false;

  v8::Local<v8::String> process_name = v8::String::NewFromUtf8(GetIsolate(), "Process",
                                                               v8::NewStringType::kNormal)
                                          .ToLocalChecked();
  v8::Local<v8::Value> process_val;
  if (!context->Global()->Get(context, process_name).ToLocal(&process_val) ||
      !process_val->IsFunction()) {
    return false;
  }

  v8::Local<v8::Function> process_fun = v8::Local<v8::Function>::Cast(process_val);

  process_.Reset(GetIsolate(), process_fun);

  return true;
}

bool JsHttpRequestProcessor::ExecuteScript(v8::Local<v8::String> script) {
  v8::HandleScope handle_scope(GetIsolate());

  v8::TryCatch try_catch(GetIsolate());

  v8::Local<v8::Context> context(GetIsolate()->GetCurrentContext());

  v8::Local<v8::Script> compiled_script;
  if (!v8::Script::Compile(context, script).ToLocal(&compiled_script)) {
    v8::String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);
    return false;
  }

  v8::Local<v8::Value> result;
  if (!compiled_script->Run(context).ToLocal(&result)) {
    v8::String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);
    return false;
  }

  return true;
}

bool JsHttpRequestProcessor::InstallMaps(std::map<std::string, std::string>* opts,
                                         std::map<std::string, std::string>* output) {
  v8::HandleScope handle_scope(GetIsolate());

  v8::Local<v8::Object> opts_obj = WrapMap(opts);

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(GetIsolate(), context_);

  context->Global()
      ->Set(context,
            v8::String::NewFromUtf8(GetIsolate(), "options", v8::NewStringType::kNormal)
                .ToLocalChecked(),
            opts_obj)
        .FromJust();

  v8::Local<v8::Object> output_obj = WrapMap(output);
  context->Global()
      ->Set(context,
            v8::String::NewFromUtf8(GetIsolate(), "output", v8::NewStringType::kNormal)
                .ToLocalChecked(),
            output_obj)
        .FromJust();

  return true;
}

bool JsHttpRequestProcessor::Process(HttpRequest* req) {
  v8::HandleScope handle_scope(GetIsolate());

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(GetIsolate(), context_);

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> request_obj = WrapRequest(req);

  v8::TryCatch try_catch(GetIsolate());

  const int argc = 1;
  v8::Local<v8::Value> argv[argc] = {request_obj};
  v8::Local<v8::Function> process =
      v8::Local<v8::Function>::New(GetIsolate(), process_);
  v8::Local<v8::Value> result;
  if (!process->Call(context, context->Global(), argc, argv).ToLocal(&result)) {
    v8::String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);
    return false;
  }

  return true;
}

JsHttpRequestProcessor::~JsHttpRequestProcessor() {
  context_.Reset();
  process_.Reset();
}

v8::Global<v8::ObjectTemplate> JsHttpRequestProcessor::request_template_;
v8::Global<v8::ObjectTemplate> JsHttpRequestProcessor::map_template_;

v8::Local<v8::Object> JsHttpRequestProcessor::WrapMap(std::map<std::string, std::string>* obj) {
  v8::EscapableHandleScope handle_scope(GetIsolate());

  if (map_template_.IsEmpty()) {
    v8::Local<v8::ObjectTemplate> raw_template = MakeMapTemplate(GetIsolate());
    map_template_.Reset(GetIsolate(), raw_template);
  }

  v8::Local<v8::ObjectTemplate> templ =
      v8::Local<v8::ObjectTemplate>::New(GetIsolate(), map_template_);

  v8::Local<v8::Object> result =
      templ->NewInstance(GetIsolate()->GetCurrentContext()).ToLocalChecked();

  v8::Local<v8::External> map_ptr = v8::External::New(GetIsolate(), obj);

  result->SetInternalField(0, map_ptr);

  return handle_scope.Escape(result);
}

std::map<std::string, std::string>* UnwrapMap(v8::Local<v8::Object> obj) {
  v8::Local<v8::External> field = v8::Local<v8::External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();
  return static_cast<std::map<std::string, std::string>*>(ptr);
}

std::string ObjectToString(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  v8::String::Utf8Value utf8_value(isolate, value);
  return std::string(*utf8_value);
}

void JsHttpRequestProcessor::MapGet(v8::Local<v8::Name> name,
                                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (name->IsSymbol()) return;

  std::map<std::string, std::string>* obj = UnwrapMap(info.Holder());

  std::string key = ObjectToString(GetIsolate(), v8::Local<v8::String>::Cast(name));

  std::map<std::string, std::string>::iterator iter = obj->find(key);

  if (iter == obj->end()) return;

  const std::string& value = iter->second;
  info.GetReturnValue().Set(
      v8::String::NewFromUtf8(info.GetIsolate(), value.c_str(),
                              v8::NewStringType::kNormal,
                              static_cast<int>(value.length())).ToLocalChecked());
}

void JsHttpRequestProcessor::MapSet(v8::Local<v8::Name> name, v8::Local<v8::Value> value_obj,
                                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (name->IsSymbol()) return;

  std::map<std::string, std::string>* obj = UnwrapMap(info.Holder());

  std::string key = ObjectToString(info.GetIsolate(), v8::Local<v8::String>::Cast(name));
  std::string value = ObjectToString(info.GetIsolate(), value_obj);

  (*obj)[key] = value;

  info.GetReturnValue().Set(value_obj);
}

v8::Local<v8::ObjectTemplate> JsHttpRequestProcessor::MakeMapTemplate(v8::Isolate* isolate) {
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> result = v8::ObjectTemplate::New(isolate);
  result->SetInternalFieldCount(1);
  result->SetHandler(v8::NamedPropertyHandlerConfiguration(MapGet, MapSet));

  return handle_scope.Escape(result);
}

v8::Local<v8::Object> JsHttpRequestProcessor::WrapRequest(HttpRequest* request) {
  v8::EscapableHandleScope handle_scope(GetIsolate());

  if (request_template_.IsEmpty()) {
    v8::Local<v8::ObjectTemplate> raw_template = MakeRequestTemplate(GetIsolate());
    request_template_.Reset(GetIsolate(), raw_template);
  }

  v8::Local<v8::ObjectTemplate> templ =
      v8::Local<v8::ObjectTemplate>::New(GetIsolate(), request_template_);

  v8::Local<v8::Object> result =
      templ->NewInstance(GetIsolate()->GetCurrentContext()).ToLocalChecked();

  v8::Local<v8::External> request_ptr = v8::External::New(GetIsolate(), request);

  result->SetInternalField(0, request_ptr);

  return handle_scope.Escape(result);
}

HttpRequest* JsHttpRequestProcessor::UnwrapRequest(v8::Local<v8::Object> obj) {
  v8::Local<v8::External> field = v8::Local<v8::External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();
  return static_cast<HttpRequest*>(ptr);
}

v8::Local<v8::ObjectTemplate> JsHttpRequestProcessor::MakeRequestTemplate(v8::Isolate* isolate) {
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> result = v8::ObjectTemplate::New(isolate);
  result->SetInternalFieldCount(1);

  result->SetAccessor(
      v8::String::NewFromUtf8(isolate, "path", v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      GetPath);
  result->SetAccessor(
      v8::String::NewFromUtf8(isolate, "referrer", v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      GetReferrer);
  result->SetAccessor(
      v8::String::NewFromUtf8(isolate, "host", v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      GetHost);
  result->SetAccessor(
      v8::String::NewFromUtf8(isolate, "userAgent", v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      GetUserAgent);

  return handle_scope.Escape(result);
}

void HttpRequestProcessor::Log(const char* event) {
  printf("Logged: %s\n", event);
}

class StringHttpRequest : public HttpRequest {
  public:
    StringHttpRequest(const std::string& path,
                      const std::string& referrer,
                      const std::string& host,
                      const std::string& user_agent);
    virtual const std::string& Path() { return path_; }
    virtual const std::string& Referrer() { return referrer_; }
    virtual const std::string& Host() { return host_; }
    virtual const std::string& UserAgent() { return user_agent_; }

  private:
    std::string path_;
    std::string referrer_;
    std::string host_;
    std::string user_agent_;
};

StringHttpRequest::StringHttpRequest(const std::string& path,
                                     const std::string& referrer,
                                     const std::string& host,
                                     const std::string& user_agent)
    : path_(path),
      referrer_(referrer),
      host_(host),
      user_agent_(user_agent) { }

void ParseOptions(int argc,
                  char* argv[],
                  std::map<std::string, std::string>* options,
                  std::string* file) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    std::size_t index = arg.find('=', 0);
    if (index == std::string::npos) {
      *file = arg;
    } else {
      std::string key = arg.substr(0, index);
      std::string value = arg.substr(index + 1);
      (*options)[key] = value;
    }
  }
}

v8::MaybeLocal<v8::String> ReadFile(v8::Isolate* isolate,
                                    const std::string& name) {
  FILE* file = fopen(name.c_str(), "rb");
  if (NULL == file) return v8::MaybeLocal<v8::String>();

  fseek(file, 0, SEEK_END);
  std::size_t size = ftell(file);
  rewind(file);

  std::unique_ptr<char> chars(new char[size + 1]);
  chars.get()[size] = '\0';
  for (std::size_t i = 0; i < size;) {
    i += fread(&chars.get()[i], 1, size - i, file);
    if (ferror(file)) {
      fclose(file);
      return v8::MaybeLocal<v8::String>();
    }
  }
  fclose(file);
  v8::MaybeLocal<v8::String> result =
      v8::String::NewFromUtf8(isolate, chars.get(),
                              v8::NewStringType::kNormal,
                              static_cast<int>(size));

  return result;
}

const int kSampleSize = 6;
StringHttpRequest kSampleRequests[kSampleSize] = {
  StringHttpRequest("/process.cc", "localhost", "google.com", "firefox"),
  StringHttpRequest("/", "localhost", "google.net", "firefox"),
  StringHttpRequest("/", "localhost", "google.org", "safari"),
  StringHttpRequest("/", "localhost", "yahoo.com", "ie"),
  StringHttpRequest("/", "localhost", "yahoo.com", "safari"),
  StringHttpRequest("/", "localhost", "yahoo.com", "firefox")
};

bool ProcessEntries(v8::Platform* platform, HttpRequestProcessor* processor,
                    int count, StringHttpRequest* reqs) {
  for (int i = 0; i < count; ++i) {
    bool result = processor->Process(&reqs[i]);
    while (v8::platform::PumpMessageLoop(platform, v8::Isolate::GetCurrent()))
      continue;
    if (!result) return false;
  }
  return true;
}

void PrintMap(std::map<std::string, std::string>* m) {
  for (std::map<std::string, std::string>::iterator it = m->begin(); it != m->end(); ++it) {
    std::pair<std::string, std::string> entry = *it;
    printf("%s: %s\n", entry.first.c_str(), entry.second.c_str());
  }
}

int main(int argc, char* argv[]) {
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  std::map<std::string, std::string> options;
  std::string file;
  ParseOptions(argc, argv, &options, &file);
  if (file.empty()) {
    fprintf(stderr, "No script was specified.\n");
    return 1;
  }
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::String> source;
  if (!ReadFile(isolate, file).ToLocal(&source)) {
    fprintf(stderr, "Error reading '%s'.\n", file.c_str());
    return 1;
  }
  JsHttpRequestProcessor processor(isolate, source);
  std::map<std::string, std::string> output;
  if (!processor.Initialize(&options, &output)) {
    fprintf(stderr, "Error initializing processor.\n");
    return 1;
  }
  if (!ProcessEntries(platform.get(), &processor, kSampleSize, kSampleRequests)) {
    return 1;
  }
  PrintMap(&output);

  return 0;
}

#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

#include "v8/include/v8.h"
#include "v8/include/libplatform/libplatform.h"

class HttpRequest {
  public:
    virtual ~HttpRequest() { }
    virtual const string& Path() = 0;
    virtual const string& Referrer() = 0;
    virtual const string& Host() = 0;
    virtual const string& UserAgent() = 0;
};

class HttpRequestProcessor {
  public:
    virtual ~HttpRequestProcessor() { }
    virtial bool Initialize(std::map<std::string, std::string>* options,
                            std::map<std::string, std::string>* output) = 0;
    virtual bool Process(HttpRequest* req) = 0;
    static void Log(const char* event);
};

class JsHttpRequestProcessor : public HttpRequestProcessor {
  public:
    JsHttpRequestProcessor(v8::Isolate* isolate, v8::Local<String> script)
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
    v8::Local<v8::ObjectTemplate> raw_template = MakeMapTemplate(obj);
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

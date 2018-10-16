#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

#include "v8/include/v8.h"
#include "v8/include/libplatform/libplatform.h"

using std::map;
using std::pair;
using std::string;

using v8::Context;
using v8::EscapableHandleScope;
using v8::External;
using v8::Function;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::NewStringType;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::Script;
using v8::String;
using v8::TryCatch;
using v8::Value;

// These interfaces represent an existing request processing interface.
// The idea is to imagine a real application that uses these interfaces
// and then add scripting capabilities that allow you to interact with
// the objects through JavaScript.

/**
 * A simplified http request.
 */

class HttpRequest {
  public:
    virtual ~HttpRequest() { }
    virtual const string& Path() = 0;
    virtual const string& Referrer() = 0;
    virtual const string& Host() = 0;
    virtual const string& UserAgent() = 0;
};

/**
 * The abstract superclass of http request processors.
 */
class HttpRequestProcessor {
  public:
    virtual ~HttpRequestProcessor() { }
    // Initialize this processor.  The map contains options that control
    // how requests should be processed.
    virtual bool Initialize(map<string, string>* options,
                            map<string, string>* output) = 0;
    // Process a single request.
    virtual Process(HttpRequest* req) = 0;

    // static 函数不能写成纯虚函数.
    static void Log(const char* event);
};

/**
 * An http request processor that is scriptable using JavaScript.
 */
class JsHttpRequestProcessor : public HttpRequestProcessor {
  public:
    // Creates a new processor that processes requests by invoking the
    // Process function of the JavaScript script given as an argument.
    JsHttpRequestProcessor(Isolate* isolate, Local<String> script)
        : isolate_(isolate), script_(script) {}

    virtual ~JsHttpRequestProcessor();

    virtual bool Initialize(map<string, string>* opts,
                            map<string, string>* output);

    virtual bool Process(HttpRequest* req);

  private:
    // Execute the script associated with this processor and extract the
    // Process function.  Returns true if this succeeded, otherwise false.
    bool ExecuteScript(Local<String> script);

    // Wrap the options and output map in a JavaScript objects and
    // install it in the global namespace as 'options' and 'output'.
    bool InstallMaps(map<string, string>* opts, map<string, string>* output);

    // Constructs the template that describes the JavaScript wrapper
    // type for requests.
    static Local<ObjectTemplate> MakeRequestTemplate(Isolate* isolate);
    static Local<ObjectTemplate> MakeMapTemplate(Isolate* isolate);

    // Callbacks that access the individual fields of request objects.
    static void GetPath(Local<String> name, const PropertyCallbackInfo<Value>& info);
    static void GetReferrer(Local<String> name, const PropertyCallbackInfo<Value>& info);
    static void GetHost(Local<String> name, const PropertyCallbackInfo<Value>& info);
    static void GetUserAgent(Local<String> name, const PropertyCallbackInfo<Value>& info);

    // Callbacks that access maps
    static MapGet(Local<Name> name, const PropertyCallbackInfo<Value>& info);
    static MapSet(Local<Name> name, Local<Value> value, const PropertyCallbackInfo<Value>& info);

    // Utility methods for wrapping C++ objects as JavaScript objects,
    // and going back again.
    Local<Object> WrapMap(map<string, string>* obj);
    static map<string, string>* UnwrapMap(Local<Object> obj);
    Local<Object> WrapRequest(HttpRequest* obj);
    static HttpRequest* UnwrapRequest(Local<Object> obj);

    Isolate* GetIsolate() { return isolate_; }

    Isolate* isolate_;
    Local<String> script_;
    Global<Context> context_;
    Global<Function> process_;
    static Global<ObjectTemplate> request_template_;
    static Global<ObjectTemplate> map_template_;
};

static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1) return;
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  Local<Value> arg = args[0];
  String::Utf8Value value(isolate, arg);
  HttpRequestProcessor::Log(*value);
}

// Execute the script and fetch the Process method.
bool JsHttpRequestProcessor::Initialize(map<string, string>* opts,
                                        map<string, string>* output) {
  // Create a handle scope to hold the temporary references.
  HandleScope handle_scope(GetIsolate());

  // Create a template for the global object where we set the
  // built-in global functions.
  Local<ObjectTemplate> global = ObjectTemplate::New(GetIsolate());
  global->Set(String::NewFromUtf8(GetIsolate(), "log", NewStringType::kNormal).ToLocalChecked(),
              FunctionTemplate::New(GetIsolate(), LogCallback));

  // Each processor gets its own context so different processors don't
  // affect each other. Context::New returns a persistent handle which
  // is what we need for the reference to remain after we return from
  // this method. That persistent handle has to be disposed in the
  // destructor.
  Local<v8::Context> context = Context::New(GetIsolate(), NULL, global);
  context_.Reset(context);

  // Enter the new context so all the following operations take place
  // within it.
  Context::Scope context_scope(context);

  // Make the options mapping available within the context
  if (!InstallMaps(opts, output))
    return false;

  // Compile and run the script
  if (!ExecuteScript(script_))
    return false;

  // The script compiled and ran correctly.  Now we fetch out the
  // Process function from the global object.
  Local<String> process_name = String::NewFromUtf8(GetIsolate(), "Process", NewStringType::kNormal).ToLocalChecked();
  Local<Value> process_val;

  // If there is no Process function, or if it is not a function,
  // bail out
  if (!context->Global()->Get(context, process_name).ToLocal(&process_val) || !process_val->IsFunction()) {
    return false;
  }

  // It is a function; cast it to a Function
  Local<Function> process_fun = Local<Function>::Cast(process_val);

  // Store the function in a Global handle, since we also want
  // that to remain after this call returns
  process_.Reset(GetIsolate(), process_fun);

  // All done; all went well
  return true;
}

bool JsHttpRequestProcessor::ExecuteScript(Local<String> script) {
  HandleScope handle_scope(GetIsolate());

  // We're just about to compile the script; set up an error handler to
  // catch any exceptions the script might throw.
  TryCatch try_catch(GetIsolate());

  Local<Context> context(GetIsolate()->GetCurrentContext());

  // Compile the script and check for errors.
  Local<Script> compiled_script;
  if (!Script::Compile(context, script).ToLocal(compiled_script)) {
    String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);

    // The script failed to compile; bail out.
    return false;
  }

  // Run the script!
  Local<Value> result;
  if (!compiled_script->Run(context).ToLocal(&result)) {
    // The TryCatch above is still in effect and will have caught the error.
    String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);
    //Running the script failed; bail out.
    return false;
  }

  return true;
}

bool JsHttpRequestProcessor::InstallMaps(map<string, string>* opts,
                                         map<string, string>* output) {
  HandleScope handle_scope(GetIsolate());

  // Wrap the map object in a JavaScript wrapper
  Local<Object> opts_obj = WrapMap(opts);

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(GetIsolate(), context_);

  // Set the options object as a property on the global object.
  context->Global()
      ->Set(context,
            String::NewFromUtf8(GetIsolate(), "options", NewStringType::kNormal).ToLocalChecked(),
            opts_obj)
      .FromJust();

  Local<Object> output_obj;
  context->Global()
      ->Set(context,
            String::NewFromUtf8(GetIsolate(), "output", NewStringType::kNormal).ToLocalChecked(),
            output_obj)
      .FromJust();

  return true;
}

bool JsHttpRequestProcessor::Process(HttpRequest* request) {
  // Create a handle scope to keep the temporary object references.
  HandleScope handle_scope(GetIsolate());

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(GetIsolate(), context_);

  // Enter this processor's context so all the remaining operations
  // take place there
  v8::Context::Scope context_scope(context);

  // Wrap the C++ request object in a JavaScript wrapper
  Local<Object> request_obj = WrapRequest(request);

  // Set up an exception handler before calling the Process function
  TryCatch try_catch(GetIsolate());

  // Invoke the process function, giving the global object as 'this'
  // and one argument, the request.
  const int argc = 1;
  Local<Value> argv[argc] = {request_obj};
  v8::Local<v8::Function> process = v8::Local<v8::Function>::New(GetIsolate(), process_);
  v8::Local<v8::Value> result;
  if (!process->Call(context, context->Global(), argc, argv).ToLocal(&result)) {
    v8::String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);
    return false;
  }
  return true;
}

JsHttpRequestProcessor::~JsHttpRequestProcessor() {
  // Dispose the persistent handles.  When no one else has any
  // references to the objects stored in the handles they will be
  // automatically reclaimed.
  context_.Reset();
  process_.Reset();
}

Global<ObjectTemplate> JsHttpRequestProcessor::request_template_;
Global<ObjectTemplate> JsHttpRequestProcessor::map_template_;

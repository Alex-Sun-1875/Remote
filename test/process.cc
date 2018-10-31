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

    v8::Isolate
};

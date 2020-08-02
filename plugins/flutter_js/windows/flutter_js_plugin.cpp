/*
 * @Description: 
 * @Author: ekibun
 * @Date: 2020-07-18 16:22:37
 * @LastEditors: ekibun
 * @LastEditTime: 2020-08-02 16:47:49
 */
#include "include/flutter_js/flutter_js_plugin.h"

// This must be included before many other Windows headers.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "include/wil/com.h"
#include <wrl.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>

#include "js_c_promise.hpp"
#include "resource.h"

#include "offscreen.hpp"

namespace
{
  #if _MSC_VER >= 1400
  EXTERN_C IMAGE_DOS_HEADER __ImageBase;
  inline HMODULE GetCurrentModuleHandle()
  {
    return (HMODULE)&__ImageBase;
  };
  #endif

  //加载资源
  std::string UseCustomResource(int rcId)
  {
    HMODULE phexmodule = GetCurrentModuleHandle();
    HRSRC hRsrc = FindResource(phexmodule, MAKEINTRESOURCE(rcId), TEXT("script"));    
    HGLOBAL hGlobal = LoadResource(phexmodule, hRsrc);
    LPVOID pBuffer = LockResource(hGlobal);
    DWORD resLength = SizeofResource(phexmodule, hRsrc);
    return std::string((char * &)pBuffer).substr(0, resLength);
  }

  class FlutterJsPlugin : public flutter::Plugin
  {
  public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

    FlutterJsPlugin();

    virtual ~FlutterJsPlugin();

  private:
    // Called when a method is called on this plugin's channel from Dart.
    void HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue> &method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  };

  // static
  void FlutterJsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarWindows *registrar)
  {
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "io.abner.flutter_js",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<FlutterJsPlugin>();

    channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result) {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  // std::map<int,qjs::Runtime*> jsEngineMap;

  FlutterJsPlugin::FlutterJsPlugin() {
    // testWebView();
  }

  FlutterJsPlugin::~FlutterJsPlugin()
  {
    // releaseWebview();
    // jsEngineMap.clear();
  }

  // Looks for |key| in |map|, returning the associated value if it is present, or
  // a Null EncodableValue if not.
  const flutter::EncodableValue &ValueOrNull(const flutter::EncodableMap &map, const char *key)
  {
    static flutter::EncodableValue null_value;
    auto it = map.find(flutter::EncodableValue(key));
    if (it == map.end())
    {
      return null_value;
    }
    return it->second;
  }

  void println(std::string data)
  {
    std::cout << data << std::endl;
  }

  void FlutterJsPlugin::HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
  {
    // Replace "getPlatformVersion" check with your plugin's method.
    // See:
    // https://github.com/flutter/engine/tree/master/shell/platform/common/cpp/client_wrapper/include/flutter
    // and
    // https://github.com/flutter/engine/tree/master/shell/platform/glfw/client_wrapper/include/flutter
    // for the relevant Flutter APIs.
    if (method_call.method_name().compare("getPlatformVersion") == 0)
    {
      std::ostringstream version_stream;
      version_stream << "Windows ";
      if (IsWindows10OrGreater())
      {
        version_stream << "10+";
      }
      else if (IsWindows8OrGreater())
      {
        version_stream << "8";
      }
      else if (IsWindows7OrGreater())
      {
        version_stream << "7";
      }
      flutter::EncodableValue response(version_stream.str());
      result->Success(&response);
    }
    else if (method_call.method_name().compare("initEngine") == 0)
    {
      int engineId = method_call.arguments()->IntValue();
      std::cout << engineId << std::endl;
      // TODO use threadpool
      // qjs::Runtime* runtime = new qjs::Runtime();
      // jsEngineMap[engineId] = runtime;
      // qjs::Context ctx(*runtime);
      flutter::EncodableValue response(engineId);
      result->Success(&response);
    }
    else if (method_call.method_name().compare("evaluate") == 0)
    {
      flutter::EncodableMap args = method_call.arguments()->MapValue();
      std::string command = ValueOrNull(args, "command").StringValue();
      int engineId = ValueOrNull(args, "engineId").IntValue();
      // qjs::Runtime* runtime = jsEngineMap.at(engineId);
      auto presult = result.release();
      async2([presult, command]() {
        qjs::Runtime runtime;
        js_std_init_handlers(runtime.rt);
        qjs::Context ctx(runtime);
        JSContext *pctx = ctx.ctx;
        try
        {
          auto &module = ctx.addModule("__WindowsBaseMoudle");
          module
              .function<&println>("println")
              .function<&js_os_setTimeout>("setTimeout")
              .function<&js_os_http_get>("http");
          ctx.eval(R"xxx(
          import * as __WindowsBaseMoudle from "__WindowsBaseMoudle";
          globalThis.print = (...a) => __WindowsBaseMoudle.println(a.join(' '));
          globalThis.http = (url) => new Promise((res) => __WindowsBaseMoudle.http(res, url));
          globalThis.delay = (delayMs) => new Promise((res) => __WindowsBaseMoudle.setTimeout(res, delayMs));
        )xxx",
                   "<init>", JS_EVAL_TYPE_MODULE);
          // TODO use JS_SetHostPromiseRejectionTracker
          ctx.eval(UseCustomResource(IDS_ENCODING).c_str(), "<init:encoding>");
          ctx.global()["__evalstr"] = JS_NewString(ctx.ctx, command.c_str());
          auto ret = ctx.eval("const __ret = Promise.resolve(eval(__evalstr)).then(v => __ret.__value = v).catch(e => __ret.__error = e); __ret", "<eval>");
          // js_std_loop(ctx.ctx);
          JSContext *pctx;
          for (;;)
          {
            for (;;)
            {
              int err = JS_ExecutePendingJob(runtime.rt, &pctx);
              if (err <= 0)
              {
                if (err < 0)
                  throw qjs::exception{};
                break;
              }
            }
            if (ret["__value"]) break;
            if (ret["__error"]) {
              JS_Throw(ctx.ctx, JS_DupValue(ctx.ctx, ret["__error"].as<qjs::Value>().v));
              throw qjs::exception{};
            }
            pctx = ctx.ctx;
            if (js_os_poll(ctx.ctx))
              break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }
          std::string retValue = (std::string)ret["__value"];
          flutter::EncodableValue response(retValue);
          presult->Success(&response);
        }
        catch (qjs::exception)
        {
          auto exc = ctx.getException();
          std::string err = (std::string)exc;
          if ((bool)exc["stack"])
            err += "\n" + (std::string)exc["stack"];
          std::cerr << err << std::endl;
          presult->Error("FlutterJSException", err);
        }
        js_std_free_handlers(runtime.rt);
      });
    }
    else if (method_call.method_name().compare("close") == 0)
    {
      flutter::EncodableMap args = method_call.arguments()->MapValue();
      // TODO
      // int engineId = ValueOrNull(args, "engineId").IntValue();
      // if(jsEngineMap.count(engineId)) {
      //   delete jsEngineMap.at(engineId);
      //   jsEngineMap.erase(engineId);
      // }
      result->Success();
    }
    else
    {
      result->NotImplemented();
    }
  }

} // namespace

void FlutterJsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar)
{
  FlutterJsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

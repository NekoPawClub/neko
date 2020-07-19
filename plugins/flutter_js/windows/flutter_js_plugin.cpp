/*
 * @Description: 
 * @Author: ekibun
 * @Date: 2020-07-18 16:22:37
 * @LastEditors: ekibun
 * @LastEditTime: 2020-07-19 11:41:25
 */ 
#include "include/flutter_js/flutter_js_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>

#include "quickjs/quickjspp.hpp"

namespace {

class FlutterJsPlugin : public flutter::Plugin {
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
    flutter::PluginRegistrarWindows *registrar) {
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

std::map<int,qjs::Context*> jsEngineMap;

FlutterJsPlugin::FlutterJsPlugin() {}

FlutterJsPlugin::~FlutterJsPlugin() {
  jsEngineMap.clear();
}

// Looks for |key| in |map|, returning the associated value if it is present, or
// a Null EncodableValue if not.
const flutter::EncodableValue &ValueOrNull(const flutter::EncodableMap &map, const char *key) {
  static flutter::EncodableValue null_value;
  auto it = map.find(flutter::EncodableValue(key));
  if (it == map.end()) {
    return null_value;
  }
  return it->second;
}

void FlutterJsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  // Replace "getPlatformVersion" check with your plugin's method.
  // See:
  // https://github.com/flutter/engine/tree/master/shell/platform/common/cpp/client_wrapper/include/flutter
  // and
  // https://github.com/flutter/engine/tree/master/shell/platform/glfw/client_wrapper/include/flutter
  // for the relevant Flutter APIs.
  if (method_call.method_name().compare("getPlatformVersion") == 0) {
    std::ostringstream version_stream;
    version_stream << "Windows ";
    if (IsWindows10OrGreater()) {
      version_stream << "10+";
    } else if (IsWindows8OrGreater()) {
      version_stream << "8";
    } else if (IsWindows7OrGreater()) {
      version_stream << "7";
    }
    flutter::EncodableValue response(version_stream.str());
    result->Success(&response);
  } else if (method_call.method_name().compare("initEngine") == 0) {
    int engineId = method_call.arguments()->IntValue();
    std::cout << engineId << std::endl;
    qjs::Runtime* runtime = new qjs::Runtime();
    qjs::Context* context = new qjs::Context(*runtime);
    // std::cout << context.eval("1+1", "<test>").as<int>() << std::endl;
    jsEngineMap[engineId] = context;
    flutter::EncodableValue response(engineId);
    result->Success(&response);
  } else if (method_call.method_name().compare("evaluate") == 0) {
    flutter::EncodableMap args = method_call.arguments()->MapValue();
    try
    {
      std::string command = ValueOrNull(args, "command").StringValue();
      int engineId = ValueOrNull(args, "engineId").IntValue();
      auto resultJS = jsEngineMap.at(engineId)->eval(command).as<std::string>();
      std::cout << resultJS << std::endl;
      flutter::EncodableValue response(resultJS);
      result->Success(&response);
    }
    catch(const qjs::exception& e)
    {
      result->Error("FlutterJSException", "error");
    }
  } else if (method_call.method_name().compare("close") == 0) {
    flutter::EncodableMap args = method_call.arguments()->MapValue();
    int engineId = ValueOrNull(args, "engineId").IntValue();
    if(jsEngineMap.count(engineId)) {
      delete jsEngineMap.at(engineId);
      jsEngineMap.erase(engineId);
    }
  } else {
    result->NotImplemented();
  }
}

}  // namespace

void FlutterJsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  FlutterJsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

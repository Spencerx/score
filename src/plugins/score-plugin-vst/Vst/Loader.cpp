#include "Loader.hpp"

#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif __has_include(<dlfcn.h>)
#include <dlfcn.h>
#endif

namespace vst
{

#if defined(_WIN32)
struct WinLoader
{
  static void* load(const char* name) { return LoadLibraryA(name); }
  static void* load(const wchar_t* name) { return LoadLibraryW(name); }

  static void unload(void* module) { FreeLibrary((HMODULE)module); }

  static PluginEntryProc getMain(void* module)
  {
    auto mainProc = (PluginEntryProc)GetProcAddress((HMODULE)module, "VSTPluginMain");
    if(!mainProc)
      mainProc = (PluginEntryProc)GetProcAddress((HMODULE)module, "PluginMain");
    if(!mainProc)
      mainProc = (PluginEntryProc)GetProcAddress((HMODULE)module, "main_plugin");
    if(!mainProc)
      mainProc = (PluginEntryProc)GetProcAddress((HMODULE)module, "main");
    return mainProc;
  }
};
using PluginLoader = WinLoader;
#elif defined(__APPLE__)
struct AppleLoader
{
  static std::string cf_to_stdstring(CFStringRef res)
  {
    char err[512];
    CFStringGetCString(res, err, sizeof(err), kCFStringEncodingUTF8);
    err[511] = 0;
    return err;
  }

  static void* load(const char* name)
  {
    CFStringRef fileNameString
        = CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8);
    if(fileNameString == 0)
      throw std::runtime_error("Couldn't load plug-in" + std::string(name));

    CFURLRef url = CFURLCreateWithFileSystemPath(
        nullptr, fileNameString, kCFURLPOSIXPathStyle, false);
    CFRelease(fileNameString);
    if(url == 0)
      throw std::runtime_error("Couldn't load plug-in" + std::string(name));

    auto module = CFBundleCreate(nullptr, url);
    CFRelease(url);
    if(!module)
      throw std::runtime_error("Couldn't create bundle: " + std::string(name));

    CFErrorRef err = {};
    if(!CFBundleLoadExecutableAndReturnError((CFBundleRef)module, &err))
    {
      std::string sstr;
      if(auto str = CFErrorCopyFailureReason(err))
      {
        sstr = cf_to_stdstring(str);
        CFRelease(str);
      }

      throw std::runtime_error(
          "Couldn't load plug-in" + std::string(name) + ": " + sstr);
    }

    return module;
  }

  static void unload(void* module)
  {
    CFBundleUnloadExecutable((CFBundleRef)module);
    CFRelease((CFBundleRef)module);
  }

  static PluginEntryProc getMain(void* module)
  {
    auto mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName(
        (CFBundleRef)module, CFSTR("VSTPluginMain"));
    if(!mainProc)
      mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName(
          (CFBundleRef)module, CFSTR("PluginMain"));
    if(!mainProc)
      mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName(
          (CFBundleRef)module, CFSTR("main_plugin"));
    if(!mainProc)
      mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName(
          (CFBundleRef)module, CFSTR("main_macho"));
    if(!mainProc)
      mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName(
          (CFBundleRef)module, CFSTR("main"));
    return mainProc;
  }
};
using PluginLoader = AppleLoader;
#elif __has_include(<dlfcn.h>)
struct LinuxLoader
{
  static void* load(const char* name)
  {
#if defined(__clang__)
#if __has_feature(address_sanitizer)
    auto module = dlopen(name, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
#else
    auto module = dlopen(name, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND | RTLD_NODELETE);
#endif
#else
#if defined(__SANITIZE_ADDRESS__)
    auto module = dlopen(name, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
#else
    auto module = dlopen(name, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND | RTLD_NODELETE);
#endif
#endif
    if(!module)
    {
      throw std::runtime_error(
          "Couldn't load plug-in" + std::string(name) + ": " + std::string(dlerror()));
    }
    return module;
  }

  static void unload(void* module)
  {
    dlclose(module);
  }

  static PluginEntryProc getMain(void* module)
  {
    auto mainProc = (PluginEntryProc)dlsym(module, "VSTPluginMain");
    if(!mainProc)
      mainProc = (PluginEntryProc)dlsym(module, "PluginMain");
    if(!mainProc)
      mainProc = (PluginEntryProc)dlsym(module, "main_plugin");
    if(!mainProc)
      mainProc = (PluginEntryProc)dlsym(module, "main");
    return mainProc;
  }
};
using PluginLoader = LinuxLoader;
#endif

Module::Module(std::string fileName)
    : path{fileName}
    , module{PluginLoader::load(fileName.c_str())}
{
}

Module::~Module()
{
  if(module)
    PluginLoader::unload(module);
}

PluginEntryProc Module::getMain()
{
  if(module)
    return PluginLoader::getMain(module);
  else
    return {};
}
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// The plugins some suites load (see test/CMakeLists.txt), on every platform: dlopen
// where there is one, LoadLibraryW on Windows.
//
// A plugin marks each function its host looks up:
//     extern "C" IOV2_TEST_PLUGIN_EXPORT void f();
// and the host, with the path CMake hands it:
//     iov2_test::plugin::handle h = iov2_test::plugin::open(path, error);
//     auto* f = iov2_test::plugin::function<void()>(h, "f");

#include <filesystem>
#include <string>

#if defined(_WIN32)
#define IOV2_TEST_PLUGIN_EXPORT __declspec(dllexport)
#else
#define IOV2_TEST_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#if defined(_WIN32)
#include <cstdint>

// Declared rather than taken from <windows.h>, as support/platform.h does. HMODULE and
// FARPROC are spelled as <windows.h> spells them, so the declarations agree with its own.
struct HINSTANCE__;
namespace iov2_test::plugin
{
using farproc = std::intptr_t(__stdcall*)();   // FARPROC
}
extern "C" __declspec(dllimport) HINSTANCE__* __stdcall LoadLibraryW(const wchar_t* lpLibFileName);
extern "C" __declspec(dllimport) int __stdcall FreeLibrary(HINSTANCE__* hLibModule);
extern "C" __declspec(dllimport) HINSTANCE__* __stdcall GetModuleHandleW(const wchar_t* lpModuleName);
extern "C" __declspec(dllimport) iov2_test::plugin::farproc __stdcall
GetProcAddress(HINSTANCE__* hModule, const char* lpProcName);
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
#else
#include <dlfcn.h>
#endif

namespace iov2_test::plugin
{
#if defined(_WIN32)
using handle = HINSTANCE__*;
#else
using handle = void*;
#endif

// Loads the plugin at `path`, or returns nullptr with the reason in `error`.
inline handle open(const char* path, std::string& error)
{
#if defined(_WIN32)
    // CMake writes the path with forward slashes; LoadLibraryW wants backslashes.
    handle h = ::LoadLibraryW(std::filesystem::path(path).make_preferred().c_str());
    if (h == nullptr)
        error = "LoadLibraryW failed with error " + std::to_string(::GetLastError());
#else
    handle h = ::dlopen(path, RTLD_NOW);
    if (h == nullptr)
        error = ::dlerror();
#endif
    return h;
}

// The function `name` the plugin exports, or nullptr.
template <typename Fn>
Fn* function(handle h, const char* name)
{
#if defined(_WIN32)
    // Through void (*)(), which casts to and from any function pointer without a warning.
    return reinterpret_cast<Fn*>(reinterpret_cast<void (*)()>(::GetProcAddress(h, name)));
#else
    return reinterpret_cast<Fn*>(::dlsym(h, name));
#endif
}

// Drops the reference open() took. Unloads the plugin only if nothing pinned it.
inline void close(handle h)
{
#if defined(_WIN32)
    ::FreeLibrary(h);
#else
    ::dlclose(h);
#endif
}

// Whether the plugin at `path` is loaded, without loading it.
inline bool resident(const char* path)
{
#if defined(_WIN32)
    return ::GetModuleHandleW(std::filesystem::path(path).make_preferred().c_str()) != nullptr;
#else
    return ::dlopen(path, RTLD_NOW | RTLD_NOLOAD) != nullptr;
#endif
}
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// The few operating-system calls the support headers need, behind one name on
// every platform, so that test sources stay free of #ifdef _WIN32. What only
// one platform can do -- signals, O_NONBLOCK, rlimits -- is not wrapped here:
// tests of it live in a posix/, linux/ or windows/ subdirectory instead, which
// CMakeLists.txt builds on that platform alone.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <cstdint>
#include <mach-o/dyld.h>
#endif
#endif

#if defined(_WIN32)
// Declared rather than taken from <windows.h>, whose min/max macros would break
// the headers under test that a source includes after this one.
extern "C" __declspec(dllimport) unsigned long __stdcall
GetModuleFileNameW(void* hModule, wchar_t* lpFilename, unsigned long nSize);
#endif

namespace iov2_test::sys
{
inline constexpr int stdin_fd = 0;
inline constexpr int stdout_fd = 1;
inline constexpr int stderr_fd = 2;

#if defined(_WIN32)

// The read end is in text mode, as the CRT opens a piped stdin; the write end
// is binary, so a test feeds exactly the bytes it names.
inline int pipe(int fds[2])
{
    if (::_pipe(fds, 65536, _O_TEXT) != 0)
        return -1;
    ::_setmode(fds[1], _O_BINARY);
    return 0;
}

inline int dup(int fd) { return ::_dup(fd); }
inline int dup2(int from, int to) { return ::_dup2(from, to); }
inline int close(int fd) { return ::_close(fd); }

inline long long write(int fd, const void* buf, std::size_t n)
{
    return ::_write(fd, buf, static_cast<unsigned int>(n));
}

inline void set_env(const char* name, const std::string& value) { ::_putenv_s(name, value.c_str()); }
inline void unset_env(const char* name) { ::_putenv_s(name, ""); }

// freopen() is deprecated by MSVC, and freopen_s() opens the file unshared, so a
// test could not read a capture file while the stream still holds it (measured).
// The file is opened shared instead and put under the stream's descriptor.
inline bool reopen(const char* name, const char* mode, std::FILE* stream)
{
    const int oflag = mode[0] == 'w' ? (_O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT)
                                     : (_O_RDONLY | _O_TEXT);
    int fd = -1;
    if (::_sopen_s(&fd, name, oflag, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0)
        return false;
    std::fflush(stream);
    const bool ok = ::_dup2(fd, ::_fileno(stream)) == 0;
    ::_close(fd);
    std::clearerr(stream);
    return ok;
}

inline std::filesystem::path exe_path()
{
    std::wstring buf(260, L'\0');
    while (true)
    {
        const unsigned long n = GetModuleFileNameW(nullptr, buf.data(), static_cast<unsigned long>(buf.size()));
        if (n == 0)
            throw std::runtime_error("exe_path: GetModuleFileNameW failed");
        if (n < buf.size())
            return std::filesystem::path(buf.substr(0, n));
        buf.resize(buf.size() * 2);
    }
}

// Quotes one argument the way the CRT's command-line parser splits it back.
inline std::wstring quote_arg(const std::wstring& arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos)
        return arg;
    std::wstring res = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t c : arg)
    {
        if (c == L'\\') { ++backslashes; continue; }
        res.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        res += c;
    }
    res.append(backslashes * 2, L'\\');
    return res + L"\"";
}

#else

inline int pipe(int fds[2]) { return ::pipe(fds); }
inline int dup(int fd) { return ::dup(fd); }
inline int dup2(int from, int to) { return ::dup2(from, to); }
inline int close(int fd) { return ::close(fd); }

inline long long write(int fd, const void* buf, std::size_t n) { return ::write(fd, buf, n); }

inline void set_env(const char* name, const std::string& value) { ::setenv(name, value.c_str(), 1); }
inline void unset_env(const char* name) { ::unsetenv(name); }

inline bool reopen(const char* name, const char* mode, std::FILE* stream)
{
    return std::freopen(name, mode, stream) != nullptr;
}

inline std::filesystem::path exe_path()
{
#if defined(__APPLE__)
    std::uint32_t size = 0;
    ::_NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    if (::_NSGetExecutablePath(buf.data(), &size) != 0)
        throw std::runtime_error("exe_path: _NSGetExecutablePath failed");
    return std::filesystem::canonical(buf.c_str());
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

#endif

// Runs this executable again with `args` and `env_name`=`env_value` in its
// environment, and waits for it. Returns its exit status, or -1 if it could
// not be run or did not exit normally (a signal, a crash).
inline int run_self(const std::vector<std::string>& args, const char* env_name, const std::string& env_value)
{
    const std::filesystem::path exe = exe_path();
#if defined(_WIN32)
    std::vector<std::wstring> wargs{quote_arg(exe.native())};
    for (const std::string& a : args)
        wargs.push_back(quote_arg(std::filesystem::path(a).native()));
    std::vector<const wchar_t*> argv;
    for (const std::wstring& a : wargs)
        argv.push_back(a.c_str());
    argv.push_back(nullptr);

    // _wspawnv copies the environment at the call; the variable is set only
    // around it, so this process does not take itself for the child.
    set_env(env_name, env_value);
    const auto status = ::_wspawnv(_P_WAIT, exe.c_str(), argv.data());
    unset_env(env_name);
    // A crash comes back as its NTSTATUS (0xC...), negative as an int.
    return status < 0 ? -1 : static_cast<int>(status);
#else
    std::vector<char*> argv{const_cast<char*>(exe.c_str())};
    for (const std::string& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    const pid_t child = ::fork();
    if (child == -1)
        return -1;
    if (child == 0)
    {
        set_env(env_name, env_value);
        ::execv(exe.c_str(), argv.data());
        ::_exit(127);
    }

    int status = 0;
    if (::waitpid(child, &status, 0) != child || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
#endif
}
}

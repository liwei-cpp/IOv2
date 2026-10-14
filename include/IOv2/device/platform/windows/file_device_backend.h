// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file file_device_backend.h
 * @lang{ZH}
 * `file_device` 的 Windows 后端：以 UTF-16 路径打开文件。
 *
 * - 普通模式用 `_wfsopen(..., _SH_DENYNO)`：与 `fopen` 同样不限制其他进程共享该文件，
 *   且不像 `fopen` / `fopen_s` 那样被 MSVC 标为 deprecated（`fopen_s` 还默认禁止共享）。
 * - UCRT 的模式解析不认识 `x`（遇到即触发 invalid parameter，默认终止进程），所以带 `x` 的
 *   模式改用 `_wsopen_s(_O_CREAT | _O_EXCL)` 加 `_fdopen`：由 `CreateFileW(CREATE_NEW)`
 *   原子地"不存在才创建"，失败时 `errno == EEXIST`，与 POSIX 的 `O_EXCL` 相同。
 * - `file_device` 总是传入带 `b` 的模式；不带 `b` 时这里也照 `fopen` 走文本模式，但不会被用到。
 * @endif
 * @lang{EN}
 * Windows backend of `file_device`, opening files by their UTF-16 path.
 *
 * - Plain modes use `_wfsopen(..., _SH_DENYNO)`: like `fopen` it leaves the file open to
 *   other processes, and unlike `fopen` / `fopen_s` MSVC does not mark it deprecated
 *   (`fopen_s` also denies sharing by default).
 * - The UCRT's mode parser does not know `x` (it raises an invalid parameter, which ends the
 *   process by default), so modes with `x` go through `_wsopen_s(_O_CREAT | _O_EXCL)` and
 *   `_fdopen` instead: `CreateFileW(CREATE_NEW)` creates the file only if it does not exist,
 *   atomically, failing with `errno == EEXIST`, as POSIX `O_EXCL` does.
 * - `file_device` always passes a mode with `b`; without one this would follow `fopen` into
 *   text mode, but that is never used.
 * @endif
 */

#pragma once
#include <IOv2/device/platform/windows/win32_api.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>

namespace IOv2::detail
{
// Opens `p` with an fopen-style `mode`; nullptr with errno set on failure.
inline FILE* file_open(const std::filesystem::path& p, const char* mode)
{
    const std::string_view m(mode);
    const bool binary = m.find('b') != std::string_view::npos;
    const bool update = m.find('+') != std::string_view::npos;

    if (m.find('x') == std::string_view::npos)
    {
        const std::wstring wmode(m.begin(), m.end());
        return _wfsopen(p.c_str(), wmode.c_str(), _SH_DENYNO);
    }

    const int oflag = _O_CREAT | _O_EXCL | (update ? _O_RDWR : _O_WRONLY)
                    | (binary ? _O_BINARY : _O_TEXT);
    int fd = -1;
    if (_wsopen_s(&fd, p.c_str(), oflag, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0)
        return nullptr;

    FILE* f = _fdopen(fd, update ? (binary ? "w+b" : "w+") : (binary ? "wb" : "w"));
    if (f == nullptr)
    {
        // The file now exists, and stays: deleting it by name could hit another one.
        const int saved = errno;
        _close(fd);
        errno = saved;
    }
    return f;
}

inline int file_seek(FILE* stream, std::int64_t offset, int origin)
{
    return _fseeki64(stream, offset, origin);
}

inline std::int64_t file_tell(FILE* stream)
{
    return _ftelli64(stream);
}

// The path in the active code page, as narrow text is read on Windows. Characters
// the code page lacks become '?': path::string() would throw for them instead.
inline std::string path_for_message(const std::filesystem::path& p)
{
    const std::wstring& w = p.native();
    if (w.empty())
        return {};
    const int wlen = static_cast<int>(w.size());
    const int len = WideCharToMultiByte(win_cp_acp, 0, w.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return "?";
    std::string res(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(win_cp_acp, 0, w.data(), wlen, res.data(), len, nullptr, nullptr);
    return res;
}
}

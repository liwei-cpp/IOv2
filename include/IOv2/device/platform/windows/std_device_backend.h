// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file std_device_backend.h
 * @lang{ZH}
 * `std_device` 的 Windows 后端：fd 模式基于 CRT 的 `_read()`，stdio 模式经 `stdin` 的 `FILE`
 * （`_lock_file` + `fread`）。
 *
 * @note fd 0 保持 CRT 的默认文本模式（`CRLF` 读作 `LF`），本后端不调用 `_setmode`：那会改变
 *       进程全局状态，影响用户自己的 stdio。代价是：文本模式下 `_read` 遇到字节 0x1A（`Ctrl+Z`）
 *       即视为 EOF，不只控制台，管道与重定向的文件也一样，所以经 stdin 传入的二进制数据会在第一个
 *       0x1A 处被截断。
 * @note 没有 `O_NONBLOCK` 与 `EINTR`，读不需要重试。
 * @note stdio 模式要求 fd 0 处于 ANSI 文本模式或二进制模式：用户若以 `_setmode` 设成
 *       `_O_U16TEXT` / `_O_U8TEXT` / `_O_WTEXT`，CRT 的窄字符读函数不再适用。
 * @endif
 * @lang{EN}
 * Windows backend of `std_device`: fd mode is built on the CRT's `_read()`, stdio mode goes
 * through the `FILE` of `stdin` (`_lock_file` + `fread`).
 *
 * @note fd 0 stays in the CRT's default text mode (`CRLF` reads as `LF`); this backend does not
 *       call `_setmode`, which would change process-wide state that the user's own stdio relies
 *       on. The cost: in text mode `_read` takes a 0x1A byte (`Ctrl+Z`) as EOF, on pipes and
 *       redirected files as well as on a console, so binary data fed through stdin is cut off
 *       at its first 0x1A.
 * @note There is no `O_NONBLOCK` and no `EINTR`, so reads need no retry.
 * @note Stdio mode needs fd 0 in ANSI text mode or binary mode: once the user `_setmode`s it
 *       to `_O_U16TEXT` / `_O_U8TEXT` / `_O_WTEXT`, the CRT's narrow read functions no longer
 *       apply.
 * @endif
 */

#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/device/platform/windows/win32_api.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>

#include <io.h>

namespace IOv2::detail
{
inline constexpr int std_in_fd = 0;
inline constexpr int std_out_fd = 1;
inline constexpr int std_err_fd = 2;

// Whether a read would return at once. A disk file never blocks; a pipe does
// unless it holds data or its write end is gone (the read then returns 0).
// A console cannot be asked without consuming input events, so the answer
// there is "no", which costs only a short read.
inline bool std_fd_readable_now(int fd) noexcept
{
    // -1 is INVALID_HANDLE_VALUE; -2 means fd 0 has no stream (a GUI process).
    const std::intptr_t raw = _get_osfhandle(fd);
    if (raw == -1 || raw == -2)
        return false;
    // _get_osfhandle hands the HANDLE over as an intptr_t; there is no other way back.
    void* const h = reinterpret_cast<void*>(raw); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)

    switch (GetFileType(h))
    {
    case win_file_type_disk:
        return true;
    case win_file_type_pipe:
    {
        unsigned long avail = 0;
        if (PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr))
            return avail != 0;
        return GetLastError() == win_error_broken_pipe;
    }
    default:
        return false;
    }
}

inline std::size_t std_fd_read(int fd, char* s, std::size_t n)
{
    assert((s != nullptr) && (n != 0));

    constexpr auto max_read = static_cast<std::size_t>(INT_MAX);
    const int ret = _read(fd, s, static_cast<unsigned int>((std::min)(n, max_read)));
    if (ret == -1)
        throw device_error("std_device::fd_read fail: read error");
    return static_cast<std::size_t>(ret);
}

// Holds the lock of stdin's FILE, a CRITICAL_SECTION and so recursive: stdio
// calls made while it is held, locking ones included, do not deadlock.
class stdin_file_lock
{
public:
    stdin_file_lock() noexcept { _lock_file(stdin); }
    ~stdin_file_lock() { _unlock_file(stdin); }
    stdin_file_lock(const stdin_file_lock&) = delete;
    stdin_file_lock& operator=(const stdin_file_lock&) = delete;
    stdin_file_lock(stdin_file_lock&&) = delete;
    stdin_file_lock& operator=(stdin_file_lock&&) = delete;
};

// Reads n bytes through stdin's FILE and, like fread(), waits until all have
// come or the stream ends. 0 means end of file, FILE's sticky flag: until a
// clearerr(stdin) every call returns 0 at once. On an error the bytes already
// read are returned first, and the next call meets the error again and throws.
inline std::size_t std_stdin_file_read(char* s, std::size_t n)
{
    assert((s != nullptr) && (n != 0));

    stdin_file_lock guard;
    if (std::feof(stdin)) return 0;
    // A stale error flag must not pass for one raised by this read.
    std::clearerr(stdin);

    const std::size_t got = std::fread(s, 1, n, stdin);
    if (got == n || !std::ferror(stdin) || got != 0) return got;
    throw device_error("std_device stdin FILE read fail: read error");
}

// Whether stdin is at end of file, waiting for a byte if need be. A byte read
// to find out goes back with ungetc(), which guarantees one byte of pushback;
// the lock keeps another thread from reading in between.
inline bool std_stdin_file_eof()
{
    stdin_file_lock guard;
    char c = 0;
    if (std_stdin_file_read(&c, 1) == 0) return true;
    if (std::ungetc(static_cast<unsigned char>(c), stdin) == EOF)
        throw device_error("std_device::deof fail: ungetc error");
    return false;
}

// Pushing bytes back is refused. The UCRT keeps no pushback area: ungetc() steps
// back over what was already read from the current buffer, and at its start it
// either fails or, with unread bytes there, overwrites the next one and still
// reports success. Whether there is room cannot be asked -- FILE is opaque --
// and the deof() probe above is safe only because it pushes back the byte it
// has just read under the same lock.
template <std::bidirectional_iterator It>
bool std_stdin_file_unget(It, It)
{
    return false;
}
}

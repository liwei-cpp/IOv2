// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file std_device_backend.h
 * @lang{ZH}
 * `std_device` 的 Windows 后端：基于 CRT 的 `_read()`。
 *
 * @note fd 0 保持 CRT 的默认文本模式（`CRLF` 读作 `LF`），本后端不调用 `_setmode`：那会改变
 *       进程全局状态，影响用户自己的 stdio。代价是：文本模式下 `_read` 遇到字节 0x1A（`Ctrl+Z`）
 *       即视为 EOF，不只控制台，管道与重定向的文件也一样，所以经 stdin 传入的二进制数据会在第一个
 *       0x1A 处被截断。
 * @note 没有 `O_NONBLOCK` 与 `EINTR`，读不需要重试。
 * @endif
 * @lang{EN}
 * Windows backend of `std_device`, built on the CRT's `_read()`.
 *
 * @note fd 0 stays in the CRT's default text mode (`CRLF` reads as `LF`); this backend does not
 *       call `_setmode`, which would change process-wide state that the user's own stdio relies
 *       on. The cost: in text mode `_read` takes a 0x1A byte (`Ctrl+Z`) as EOF, on pipes and
 *       redirected files as well as on a console, so binary data fed through stdin is cut off
 *       at its first 0x1A.
 * @note There is no `O_NONBLOCK` and no `EINTR`, so reads need no retry.
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
    void* const h = reinterpret_cast<void*>(raw);

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
        throw device_error("std_device::do_get fail: read error");
    return static_cast<std::size_t>(ret);
}
}

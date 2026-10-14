// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file std_device_backend.h
 * @lang{ZH}
 * `std_device` 的 POSIX 后端：基于 `read()` 与 `poll()`。
 * @endif
 * @lang{EN}
 * POSIX backend of `std_device`, built on `read()` and `poll()`.
 * @endif
 */

#pragma once
#include <IOv2/common/defs.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <limits>

#include <poll.h>
#include <unistd.h>

namespace IOv2::detail
{
inline constexpr int std_in_fd = STDIN_FILENO;
inline constexpr int std_out_fd = STDOUT_FILENO;
inline constexpr int std_err_fd = STDERR_FILENO;

// Whether a read() would return at once: data is ready, or the peer hung
// up (read() then returns 0 without waiting). On a poll error the answer
// is "no"; the next blocking read() reports it.
inline bool std_fd_readable_now(int fd) noexcept
{
    struct pollfd pfd{ .fd = fd, .events = POLLIN, .revents = 0 };
    int r = poll(&pfd, 1, 0);
    while (r == -1 && errno == EINTR)
        r = poll(&pfd, 1, 0);
    return r > 0 && (pfd.revents & (POLLIN | POLLHUP)) != 0;
}

inline std::size_t std_fd_read(int fd, char* s, std::size_t n)
{
    assert((s != nullptr) && (n != 0));

    constexpr auto max_read = static_cast<std::size_t>(std::numeric_limits<ssize_t>::max());
    ssize_t ret = 0;
    while (true)
    {
        ret = read(fd, s, std::min(n, max_read));
        if (ret != -1) break;

        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            struct pollfd pfd{ .fd = fd, .events = POLLIN, .revents = 0 };
            if (poll(&pfd, 1, -1) == -1)
            {
                if (errno == EINTR) continue;
                throw device_error("std_device::do_get fail: poll error");
            }
            else if (pfd.revents & (POLLERR | POLLNVAL))
                throw device_error("std_device::do_get fail: poll revents error");
            else if (pfd.revents & POLLIN) continue;
            else if (pfd.revents & POLLHUP)
            {
                ret = 0;
                break;
            }
            else
                throw device_error("std_device::do_get fail: unexpected poll revents");
        }
        else
            throw device_error("std_device::do_get fail: read error");
    }
    return static_cast<std::size_t>(ret);
}
}

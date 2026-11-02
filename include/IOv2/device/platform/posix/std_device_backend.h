// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file std_device_backend.h
 * @lang{ZH}
 * `std_device` 的 POSIX 后端：fd 模式基于 `read()` 与 `poll()`，stdio 模式经 `stdin` 的 `FILE`
 * （`flockfile` + `fread`）。
 * @endif
 * @lang{EN}
 * POSIX backend of `std_device`: fd mode is built on `read()` and `poll()`, stdio mode goes
 * through the `FILE` of `stdin` (`flockfile` + `fread`).
 * @endif
 */

#pragma once
#include <IOv2/common/defs.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
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

// After a read on fd failed with EAGAIN: waits in poll() until reading is worth
// another try. Returns false on a bare hang-up, where read() returns 0.
inline bool std_fd_wait_input(int fd)
{
    struct pollfd pfd{ .fd = fd, .events = POLLIN, .revents = 0 };
    if (poll(&pfd, 1, -1) == -1)
    {
        if (errno == EINTR) return true;
        throw device_error("std_device stdin read fail: poll error");
    }
    if (pfd.revents & (POLLERR | POLLNVAL))
        throw device_error("std_device stdin read fail: poll revents error");
    if (pfd.revents & POLLIN) return true;
    if (pfd.revents & POLLHUP) return false;
    throw device_error("std_device stdin read fail: unexpected poll revents");
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
            if (std_fd_wait_input(fd)) continue;
            ret = 0;
            break;
        }
        throw device_error("std_device::fd_read fail: read error");
    }
    return static_cast<std::size_t>(ret);
}

// Holds the lock of stdin's FILE. It is recursive, so stdio calls made while
// it is held, locking ones included, do not deadlock.
class stdin_file_lock
{
public:
    stdin_file_lock() noexcept { flockfile(stdin); }
    ~stdin_file_lock() { funlockfile(stdin); }
    stdin_file_lock(const stdin_file_lock&) = delete;
    stdin_file_lock& operator=(const stdin_file_lock&) = delete;
};

// Reads n bytes through stdin's FILE and, like fread(), waits until all have
// come or the stream ends. 0 means end of file, FILE's sticky flag: until a
// clearerr(stdin) every call returns 0 at once. EINTR and EAGAIN are retried;
// on any other error the bytes already read are returned first, and the next
// call meets the error again and throws.
inline std::size_t std_stdin_file_read(char* s, std::size_t n)
{
    assert((s != nullptr) && (n != 0));

    stdin_file_lock guard;
    if (std::feof(stdin)) return 0;
    // A stale error flag must not pass for one raised by this read.
    std::clearerr(stdin);

    std::size_t got = 0;
    while (true)
    {
        got += std::fread(s + got, 1, n - got, stdin);
        if (got == n || !std::ferror(stdin)) return got;

        const int err = errno;
        if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
        {
            std::clearerr(stdin);
            // glibc's getc and fread also hold the lock across a blocking read().
            if (err != EINTR) (void)std_fd_wait_input(std_in_fd);
            continue;
        }
        if (got != 0) return got;
        throw device_error("std_device stdin FILE read fail: read error");
    }
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

// Pushes s[0..n) back into stdin's FILE, last byte first, so the next read gets
// them in order. Beyond one byte ungetc() may fail (glibc and BSD only when out
// of memory, musl past a small reserve); then the bytes already pushed are read
// off again and false comes back. ungetc() clears the EOF flag and there is no
// call to set it again, so a failed attempt may leave that flag cleared.
inline bool std_stdin_file_unget(const char* s, std::size_t n)
{
    assert((s != nullptr) && (n != 0));

    stdin_file_lock guard;
    for (std::size_t pushed = 0; pushed < n; ++pushed)
    {
        if (std::ungetc(static_cast<unsigned char>(s[n - 1 - pushed]), stdin) == EOF)
        {
            while (pushed-- != 0)
                (void)getc_unlocked(stdin);
            return false;
        }
    }
    return true;
}
}

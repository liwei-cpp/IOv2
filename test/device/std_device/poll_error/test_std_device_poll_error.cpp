// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The POLLERR / POLLNVAL branch of std_device's read loop.
 *
 * With O_NONBLOCK on fd 0 the loop meets EAGAIN and waits in poll(). The only way
 * poll() then reports the fd itself as broken is for fd 0 to go away in between --
 * another thread closing it after read() returned EAGAIN and before poll() ran.
 * A race like that cannot be hit on purpose, so this suite links with
 * -Wl,--wrap=read and closes fd 0 at exactly that moment: right after the real
 * read() came back with EAGAIN.
 *
 * The branch must end in a device_error (devfailbit on a stream), read() must be
 * called once and not spun on, and the device must read normally again once fd 0
 * is back.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/std_device.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <cstddef>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

using namespace IOv2;

#if defined(IOV2_TEST_WRAP_READ)
namespace
{
    bool g_armed = false;
    int g_hits = 0;
    int g_calls = 0;
}

extern "C" ssize_t __real_read(int fd, void* buf, std::size_t n);

// While armed, closes fd 0 right after a real read() on it returned EAGAIN.
extern "C" ssize_t __wrap_read(int fd, void* buf, std::size_t n)
{
    const ssize_t r = __real_read(fd, buf, n);
    if (fd == STDIN_FILENO && g_armed)
    {
        ++g_calls;
        if (r == -1 && errno == EAGAIN)
        {
            const int saved_errno = errno;
            ::close(STDIN_FILENO);
            ++g_hits;
            g_armed = false;
            errno = saved_errno;
        }
    }
    return r;
}
#endif

TEST(StdDevicePollError, AStdinClosedBetweenReadAndPollIsADeviceError)
{
#if !defined(IOV2_TEST_WRAP_READ)
    GTEST_SKIP() << "needs the linker's --wrap=read";
#else
    int pipefds[2];
    ASSERT_NE(::pipe(pipefds), -1);
    const int saved_stdin = ::dup(STDIN_FILENO);
    ASSERT_NE(::dup2(pipefds[0], STDIN_FILENO), -1);
    ::close(pipefds[0]);
    const int flags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
    ASSERT_NE(::fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK), -1);

    std_input_device d;
    char c = 0;

    // The write end stays open and silent, so read() returns EAGAIN.
    g_hits = g_calls = 0;
    g_armed = true;
    EXPECT_THROW(d.dget(&c, 1), device_error);
    g_armed = false;
    EXPECT_EQ(g_hits, 1) << "the wrapper never closed fd 0: the branch was not reached";
    EXPECT_EQ(g_calls, 1) << "read() was called again instead of failing";

    // fd 0 back, with a byte in it: the device reads on. fd 0 is free now, so the
    // new pipe's read end may already be fd 0.
    int again[2];
    ASSERT_NE(::pipe(again), -1);
    if (again[0] != STDIN_FILENO)
    {
        ASSERT_NE(::dup2(again[0], STDIN_FILENO), -1);
        ::close(again[0]);
    }
    ASSERT_EQ(::write(again[1], "Z", 1), 1);
    ::close(again[1]);
    EXPECT_EQ(d.dget(&c, 1), 1u);
    EXPECT_EQ(c, 'Z');

    ::close(pipefds[1]);
    ::dup2(saved_stdin, STDIN_FILENO);
    ::close(saved_stdin);
#endif
}

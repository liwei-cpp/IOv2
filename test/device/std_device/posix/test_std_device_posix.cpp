// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The std_device cases only POSIX can set up: a read interrupted by a signal,
 * a stdin with O_NONBLOCK, and a standard stream whose descriptor was closed
 * under it. Windows has neither EINTR nor O_NONBLOCK on a CRT descriptor, and
 * its CRT ends the process on I/O to a closed descriptor (an invalid-parameter
 * failure) rather than returning an error, so none of these can run there.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/std_device.h>

#include <support/platform.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <list>
#include <pthread.h>
#include <string>
#include <thread>
#include <utility>

using namespace IOv2;

namespace
{
    namespace sys = iov2_test::sys;

    void no_op_signal_handler(int) {}
}

TEST(StdDevicePosix, GetRetriesReadAndPollAfterSignalInterrupts)
{
    struct sigaction action {};
    struct sigaction old_action {};
    action.sa_handler = no_op_signal_handler;
    sigemptyset(&action.sa_mask);   // unqualified: a macro on macOS
    ASSERT_EQ(::sigaction(SIGUSR1, &action, &old_action), 0);

    const auto run_interrupted_read = [](bool non_blocking, bool stdio)
    {
        int pipefds[2];
        ASSERT_NE(::pipe(pipefds), -1);

        const int saved_stdin = ::dup(STDIN_FILENO);
        ASSERT_NE(saved_stdin, -1);
        ASSERT_NE(::dup2(pipefds[0], STDIN_FILENO), -1);
        if (non_blocking)
        {
            const int flags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
            ASSERT_NE(flags, -1);
            ASSERT_NE(::fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK), -1);
        }

        const pthread_t waiting_thread = ::pthread_self();
        int signal_result = -1;
        ssize_t write_result = -1;
        std::thread interrupter([&]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            signal_result = ::pthread_kill(waiting_thread, SIGUSR1);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            write_result = ::write(pipefds[1], "x", 1);
        });

        std_input_device d;
        d.stdio_mode(stdio);
        char ch = 0;
        std::size_t got = 0;
        bool threw = false;
        try { got = d.dget(&ch, 1); }
        catch (const device_error&) { threw = true; }

        interrupter.join();
        ::close(pipefds[1]);
        drain_stdin();                         // stdio mode leaves FILE's flags behind
        ::dup2(saved_stdin, STDIN_FILENO);
        ::close(saved_stdin);
        ::close(pipefds[0]);

        EXPECT_FALSE(threw);
        EXPECT_EQ(signal_result, 0);
        EXPECT_EQ(write_result, 1);
        EXPECT_EQ(got, 1u);
        EXPECT_EQ(ch, 'x');
    };

    for (const bool stdio : {false, true})
    {
        run_interrupted_read(false, stdio); // interrupts blocking read()
        run_interrupted_read(true, stdio);  // interrupts poll() after EAGAIN
    }

    EXPECT_EQ(::sigaction(SIGUSR1, &old_action, nullptr), 0);
}

// ---------------------------------------------------------------------------
// A non-blocking stdin.  dget() must wait for the data it was asked for rather
// than report a short read, and must recognise the writer hanging up as EOF --
// which is why this needs a pipe: a regular file is always ready and never ends
// mid-stream.  Both read modes: stdio mode meets the EAGAIN as a FILE error.
// ---------------------------------------------------------------------------

TEST(StdDevicePosix, GetWaitsOnANonBlockingStdinAndSeesTheHangup)
{
    const auto run_non_blocking_read = [](bool stdio)
    {
        int pipefds[2];
        ASSERT_NE(::pipe(pipefds), -1);

        const int saved_stdin = ::dup(STDIN_FILENO);
        ::dup2(pipefds[0], STDIN_FILENO);
        const int flags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
        ::fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

        std_input_device d;
        d.stdio_mode(stdio);
        char buf[5] = {};

        // The data arrives after the read has already started.
        std::thread t([&]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            (void)::write(pipefds[1], "hello", 5);
        });

        EXPECT_EQ(d.dget(buf, 5), 5u);
        EXPECT_EQ(std::memcmp(buf, "hello", 5), 0);
        t.join();

        // Close the write end only after dget() is waiting in poll().  If it were
        // closed first, read() would return 0 directly and the POLLHUP path would
        // never be exercised.
        std::thread closer([write_fd = pipefds[1]]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ::close(write_fd);
        });
        EXPECT_EQ(d.dget(buf, 1), 0u);
        EXPECT_TRUE(d.deof());
        closer.join();

        drain_stdin();                         // clears the EOF stdio mode left on the FILE
        ::dup2(saved_stdin, STDIN_FILENO);
        ::close(saved_stdin);
        ::close(pipefds[0]);
    };

    run_non_blocking_read(false);
    run_non_blocking_read(true);
}

// ---------------------------------------------------------------------------
// dunget() in stdio mode.  POSIX C libraries push back more than the one byte
// the standard guarantees (glibc and BSD grow the area on demand), so these
// succeed here; Windows refuses, see windows/.
// ---------------------------------------------------------------------------

TEST(StdDevicePosix, UngetHandsTheBytesBackToCStdioInOrder)
{
    pipe_iguard g("abc");
    g.close_write();
    stdin_full_buffer b;
    std_input_device d;
    d.stdio_mode(true);

    char buf[2] = {};
    ASSERT_EQ(d.dget(buf, 2), 2u);
    EXPECT_TRUE(d.dunget(buf, 2));
    EXPECT_EQ(std::getchar(), 'a');
    ASSERT_EQ(d.dget(buf, 2), 2u);
    EXPECT_EQ(std::string(buf, 2), "bc");
}

// The iterator form takes any bidirectional range: here the reverse of a stack
// kept with its next byte at the back, as ichannel keeps its read buffer, and a
// non-contiguous list.
TEST(StdDevicePosix, UngetTakesTheBytesAsAnIteratorRange)
{
    pipe_iguard g("tail");
    g.close_write();
    std_input_device d;
    d.stdio_mode(true);

    const std::list<char> later{'1', '2'};
    ASSERT_TRUE(d.dunget(later.begin(), later.end()));
    const std::string stack = "cba";
    ASSERT_TRUE(d.dunget(stack.crbegin(), stack.crend()));

    char buf[9] = {};
    std::size_t total = 0;
    for (std::size_t r; total < sizeof(buf) && (r = d.dget(buf + total, sizeof(buf) - total)) != 0; )
        total += r;
    EXPECT_EQ(std::string(buf, total), "abc12tail");
}

// Past glibc's first 128-byte pushback area, so it has to grow, and with 0xFF in
// it: as a signed char that byte is EOF, which ungetc() would refuse.
TEST(StdDevicePosix, UngetHandsBackABlockWithEveryByteValue)
{
    pipe_iguard g("tail");
    g.close_write();
    std_input_device d;
    d.stdio_mode(true);

    std::string block;
    for (int i = 0; i < 300; ++i)
        block += static_cast<char>(255 - i % 256);
    ASSERT_TRUE(d.dunget(block.data(), block.size()));

    std::string got(block.size() + 4, '\0');
    std::size_t total = 0;
    for (std::size_t r; total < got.size() && (r = d.dget(got.data() + total, got.size() - total)) != 0; )
        total += r;
    EXPECT_EQ(total, got.size());
    EXPECT_EQ(got, block + "tail");
}

// The bytes pushed back are read even after the end was reached: ungetc() clears
// the EOF flag. Once they are gone, the end is met again.
TEST(StdDevicePosix, UngetAfterEofMakesTheBytesReadable)
{
    pipe_iguard g("");
    g.close_write();
    std_input_device d;
    d.stdio_mode(true);

    char c = 0;
    ASSERT_EQ(d.dget(&c, 1), 0u);
    EXPECT_TRUE(d.dunget("z", 1));
    EXPECT_FALSE(d.deof());
    ASSERT_EQ(d.dget(&c, 1), 1u);
    EXPECT_EQ(c, 'z');
    EXPECT_EQ(d.dget(&c, 1), 0u);
    EXPECT_TRUE(d.deof());
}

// ---------------------------------------------------------------------------
// Error paths.  The way to make a standard stream fail is to take its
// descriptor away, so each case closes one, drives the operation, and puts it
// back before reporting: a failure printed while stdout or stderr is gone
// would have nowhere to go.
// ---------------------------------------------------------------------------

namespace
{
    template <typename F>
    bool throws_with_fd_closed(int fd, F&& op)
    {
        const int saved = sys::dup(fd);
        sys::close(fd);

        bool threw = false;
        try { op(); }
        catch (const device_error&) { threw = true; }

        sys::dup2(saved, fd);
        sys::close(saved);
        return threw;
    }
}

// The reason a moved-from device must stay quiet: glibc discards the buffer
// when fflush fails, so the first flush is the only one that can see the error.
// If the moved-from object's destructor took that flush, its catch(...) would
// swallow the failure and the owner's own dflush() would find nothing to report.
TEST(StdDevicePosix, AMovedFromDeviceDoesNotConsumeTheFlushFailure)
{
    bool flush_threw = false;
    {
        oguard<true> g;
        char buffer[BUFSIZ];
        std::setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

        const int saved = sys::dup(sys::stdout_fd);
        std_output_device owner;
        {
            std_output_device original;
            original.dput("some data", 9);      // fills the FILE buffer only
            owner = std::move(original);
            sys::close(sys::stdout_fd);
        }                                       // original dies with the fd gone

        try { owner.dflush(); }
        catch (const device_error&) { flush_threw = true; }

        sys::dup2(saved, sys::stdout_fd);
        sys::close(saved);
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }
    EXPECT_TRUE(flush_threw);
}

TEST(StdDevicePosix, PutFailsWhenStdoutIsClosed)
{
    bool threw = false;
    {
        oguard<true> g;
        std_output_device d;
        threw = throws_with_fd_closed(sys::stdout_fd, [&] { d.dput("test", 4); });
    }
    EXPECT_TRUE(threw);
}

TEST(StdDevicePosix, PutFailsWhenStderrIsClosed)
{
    bool threw = false;
    {
        oguard<false> g;
        std_error_device d;
        threw = throws_with_fd_closed(sys::stderr_fd, [&] { d.dput("test", 4); });
    }
    EXPECT_TRUE(threw);
}

// The stream gets a buffer so that the flush is the call that reaches the
// descriptor -- which is what makes dflush()'s throw reachable. (Unbuffered,
// fflush() has nothing to push and succeeds; test_std_device.cpp pins that.)
TEST(StdDevicePosix, FlushReportsTheFailedWriteOfABufferedStdout)
{
    bool put_threw = false;
    bool flush_threw = false;
    {
        oguard<true> g;
        std_output_device d;

        // The buffer has to outlive every call that can touch the stream, and
        // the stream has to be put back to unbuffered before it dies.
        char buffer[BUFSIZ];
        std::setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

        const int saved = sys::dup(sys::stdout_fd);
        sys::close(sys::stdout_fd);

        // fwrite only fills the buffer, so the write itself still succeeds...
        try { d.dput("some data", 9); }
        catch (const device_error&) { put_threw = true; }
        // ...and the flush is what finds the descriptor gone.
        try { d.dflush(); }
        catch (const device_error&) { flush_threw = true; }

        sys::dup2(saved, sys::stdout_fd);
        sys::close(saved);
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }
    EXPECT_FALSE(put_threw);
    EXPECT_TRUE(flush_threw);
}

TEST(StdDevicePosix, FlushReportsTheFailedWriteOfABufferedStderr)
{
    bool put_threw = false;
    bool flush_threw = false;
    {
        oguard<false> g;
        std_error_device d;

        char buffer[BUFSIZ];
        std::setvbuf(stderr, buffer, _IOFBF, sizeof(buffer));

        const int saved = sys::dup(sys::stderr_fd);
        sys::close(sys::stderr_fd);

        try { d.dput("some data", 9); }
        catch (const device_error&) { put_threw = true; }
        try { d.dflush(); }
        catch (const device_error&) { flush_threw = true; }

        sys::dup2(saved, sys::stderr_fd);
        sys::close(saved);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    }
    EXPECT_FALSE(put_threw);
    EXPECT_TRUE(flush_threw);
}

TEST(StdDevicePosix, DestructorSuppressesABufferedFlushFailure)
{
    bool reached_after_destruction = false;
    {
        oguard<true> g;
        char buffer[BUFSIZ];
        std::setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

        const int saved = sys::dup(sys::stdout_fd);
        {
            std_output_device d;
            d.dput("some data", 9);
            sys::close(sys::stdout_fd);
        }

        sys::dup2(saved, sys::stdout_fd);
        sys::close(saved);
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        reached_after_destruction = true;
    }
    EXPECT_TRUE(reached_after_destruction);
}

TEST(StdDevicePosix, GetFailsWhenStdinIsClosed)
{
    iguard g("abc");
    std_input_device d;

    const bool threw = throws_with_fd_closed(sys::stdin_fd, [&]
    {
        char buf = 0;
        d.dget(&buf, 1);
    });
    EXPECT_TRUE(threw);
}

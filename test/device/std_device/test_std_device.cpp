// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/defs.h>
#include <device/device_concepts.h>
#include <device/std_device.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <fcntl.h>
#include <type_traits>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

using namespace IOv2;

namespace
{
    static_assert(io_device<std_input_device>);
    static_assert(io_device<std_output_device>);
    static_assert(io_device<std_error_device>);

    static_assert(std::is_same_v<std_input_device::char_type, char>);
    static_assert(std::is_same_v<std_output_device::char_type, char>);
    static_assert(std::is_same_v<std_error_device::char_type, char>);

    // None of the three can seek: a standard stream has no position to set.
    static_assert(!dev_cpt::support_positioning<std_input_device>);
    static_assert(!dev_cpt::support_positioning<std_output_device>);
    static_assert(!dev_cpt::support_positioning<std_error_device>);

    static_assert(dev_cpt::support_get<std_input_device>);
    static_assert(!dev_cpt::support_put<std_input_device>);

    static_assert(dev_cpt::support_put<std_output_device>);
    static_assert(!dev_cpt::support_get<std_output_device>);

    static_assert(dev_cpt::support_put<std_error_device>);
    static_assert(!dev_cpt::support_get<std_error_device>);
}

TEST(StdDevice, Traits)
{
    // Every assertion above is a static_assert; compiling is the check.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Input.  stdin is redirected for the duration of each case; probing the real
// one would block or latch an EOF that outlives the test.
// ---------------------------------------------------------------------------

TEST(StdDevice, GetReadsWhateverStdinHolds)
{
    const char* input = "one two three four";

    std_input_device obj;
    char buf[5] = {};
    iguard g(input);

    EXPECT_EQ(obj.dget(buf, 1), 1u);
    EXPECT_EQ(buf[0], input[0]);
    EXPECT_EQ(obj.dget(buf, 1), 1u);
    EXPECT_EQ(buf[0], input[1]);

    // A longer request is filled from where the single-byte reads stopped.
    std::memset(buf, 'x', 5);
    EXPECT_EQ(obj.dget(buf, 5), 5u);
    EXPECT_EQ(std::memcmp(buf, input + 2, 5), 0);

    EXPECT_EQ(obj.dget(buf, 1), 1u);
    EXPECT_EQ(buf[0], input[7]);
}

TEST(StdDevice, EofIsProbedAndSticky)
{
    iguard g("a");
    std_input_device d;
    char buf[2] = {};

    // Data available: deof() probes, reports not-EOF, and caches the byte.
    EXPECT_FALSE(d.deof());
    // The probe-cached byte is delivered in order by the next dget().
    EXPECT_EQ(d.dget(buf, 1), 1u);
    EXPECT_EQ(buf[0], 'a');

    // Stream exhausted: deof() probes read()==0 and latches sticky EOF.
    EXPECT_TRUE(d.deof());
    EXPECT_EQ(d.dget(buf, 1), 0u);
    EXPECT_TRUE(d.deof());
    EXPECT_EQ(d.dget(buf, 1), 0u);
    EXPECT_TRUE(d.deof());
}

TEST(StdDevice, ZeroLengthPutAcceptsANullBuffer)
{
    std_output_device obj;
    obj.dput(nullptr, 0);
    SUCCEED();
}

TEST(StdDevice, GetRejectsANullBufferOnlyWhenItWouldReadIt)
{
    iguard g("abc");
    std_input_device obj;
    char ch = 0;

    EXPECT_EQ(obj.dget(nullptr, 0), 0u);
    EXPECT_THROW(obj.dget(nullptr, 1), device_error);

    // The rejected call consumed nothing.
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, 'a');
}

// ---------------------------------------------------------------------------
// Output.  oguard points the stream at a file and hands back its contents, so
// the checks read that file rather than the terminal.  They run after the guard
// is gone: while it is in place stdout is the capture file, and a failure
// message would land there instead of in the test log.
// ---------------------------------------------------------------------------

TEST(StdDevice, PutAndFlushReachStdout)
{
    std::string before;
    std::string after;
    {
        oguard<true> g;
        std_output_device obj;
        before = g.contents();

        obj.dput("a", 1);
        obj.dput("bcdef", 5);
        obj.dflush();
        after = g.contents();
    }
    EXPECT_TRUE(before.empty());
    EXPECT_EQ(after, "abcdef");
}

TEST(StdDevice, PutAndFlushReachStderr)
{
    std::string before;
    std::string after;
    {
        oguard<false> g;
        std_error_device obj;
        before = g.contents();

        obj.dput("a", 1);
        obj.dput("bcdef", 5);
        obj.dflush();
        after = g.contents();
    }
    EXPECT_TRUE(before.empty());
    EXPECT_EQ(after, "abcdef");
}

TEST(StdDevice, StdoutIsVisibleOnlyAfterTheFlushThatCoversIt)
{
    std::string before;
    std::string midway;
    std::string after;
    {
        oguard<true> g;
        std_output_device obj;
        before = g.contents();

        obj.dput("a", 1);
        obj.dflush();
        obj.dput("bcdef", 5);
        midway = g.contents();
        obj.dflush();
        after = g.contents();
    }
    EXPECT_TRUE(before.empty());
    ASSERT_FALSE(midway.empty());
    EXPECT_EQ(midway[0], 'a');
    EXPECT_EQ(after, "abcdef");
}

TEST(StdDevice, StderrDoesNotWaitForAFlush)
{
    std::string before;
    std::string after;
    {
        oguard<false> g;
        std_error_device obj;
        before = g.contents();

        obj.dput("a", 1);
        obj.dflush();
        obj.dput("bcdef", 5);
        after = g.contents();
    }
    EXPECT_TRUE(before.empty());
    EXPECT_EQ(after, "abcdef");
}

// ---------------------------------------------------------------------------
// Move semantics.  Only the input device carries state: the sticky EOF flag and
// the one byte deof() reads ahead.
// ---------------------------------------------------------------------------

TEST(StdDevice, MoveCarriesTheLatchedEof)
{
    std_input_device d1;
    {
        iguard g("");   // empty input, so the first read latches EOF
        char buf = 0;
        d1.dget(&buf, 1);
    }
    EXPECT_TRUE(d1.deof());

    std_input_device d2(std::move(d1));
    EXPECT_TRUE(d2.deof());

    std_input_device d3;
    d3 = std::move(d2);
    EXPECT_TRUE(d3.deof());
}

TEST(StdDevice, MoveCarriesTheCachedLookaheadByte)
{
    // Without this the byte deof() peeked would be dropped by the move.
    iguard g("xy");
    std_input_device d1;
    EXPECT_FALSE(d1.deof());             // probes and caches 'x'

    std_input_device d2(std::move(d1));  // move-construct carries the cache
    char buf = 0;
    EXPECT_EQ(d2.dget(&buf, 1), 1u);
    EXPECT_EQ(buf, 'x');

    EXPECT_FALSE(d2.deof());             // probes and caches 'y'
    std_input_device d3;
    d3 = std::move(d2);                  // move-assign carries the cache
    EXPECT_EQ(d3.dget(&buf, 1), 1u);
    EXPECT_EQ(buf, 'y');
    EXPECT_TRUE(d3.deof());
}

TEST(StdDevice, AnOutputDeviceMovesEvenThoughItHoldsNothing)
{
    std_output_device d1;
    std_output_device d2(std::move(d1));
    std_output_device d3;
    d3 = std::move(d2);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Error paths.  The only portable way to make a standard stream fail is to take
// its descriptor away, so each case closes one, drives the operation, and puts
// it back before reporting: a failure printed while stdout or stderr is gone
// would have nowhere to go.
// ---------------------------------------------------------------------------

namespace
{
    template <typename F>
    bool throws_with_fd_closed(int fd, F&& op)
    {
        const int saved = ::dup(fd);
        ::close(fd);

        bool threw = false;
        try { op(); }
        catch (const device_error&) { threw = true; }

        ::dup2(saved, fd);
        ::close(saved);
        return threw;
    }
}

TEST(StdDevice, PutFailsWhenStdoutIsClosed)
{
    bool threw = false;
    {
        oguard<true> g;
        std_output_device d;
        threw = throws_with_fd_closed(STDOUT_FILENO, [&] { d.dput("test", 4); });
    }
    EXPECT_TRUE(threw);
}

TEST(StdDevice, PutFailsWhenStderrIsClosed)
{
    bool threw = false;
    {
        oguard<false> g;
        std_error_device d;
        threw = throws_with_fd_closed(STDERR_FILENO, [&] { d.dput("test", 4); });
    }
    EXPECT_TRUE(threw);
}

// dflush() is fflush(), and oguard leaves the stream unbuffered, so there is
// never anything queued for fflush() to push at the descriptor: it returns 0
// whether or not the descriptor is still there.  The next two cases pin that,
// and the two after them give the stream a buffer so that the flush is the call
// that reaches the descriptor -- which is what makes dflush()'s throw reachable.
TEST(StdDevice, FlushOnAnUnbufferedStdoutSucceedsEvenWithTheDescriptorClosed)
{
    bool threw = false;
    {
        oguard<true> g;
        std_output_device d;
        threw = throws_with_fd_closed(STDOUT_FILENO, [&] { d.dflush(); });
    }
    EXPECT_FALSE(threw);
}

TEST(StdDevice, FlushOnAnUnbufferedStderrSucceedsEvenWithTheDescriptorClosed)
{
    bool threw = false;
    {
        oguard<false> g;
        std_error_device d;
        threw = throws_with_fd_closed(STDERR_FILENO, [&] { d.dflush(); });
    }
    EXPECT_FALSE(threw);
}

TEST(StdDevice, FlushReportsTheFailedWriteOfABufferedStdout)
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

        const int saved = ::dup(STDOUT_FILENO);
        ::close(STDOUT_FILENO);

        // fwrite only fills the buffer, so the write itself still succeeds...
        try { d.dput("some data", 9); }
        catch (const device_error&) { put_threw = true; }
        // ...and the flush is what finds the descriptor gone.
        try { d.dflush(); }
        catch (const device_error&) { flush_threw = true; }

        ::dup2(saved, STDOUT_FILENO);
        ::close(saved);
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }
    EXPECT_FALSE(put_threw);
    EXPECT_TRUE(flush_threw);
}

TEST(StdDevice, FlushReportsTheFailedWriteOfABufferedStderr)
{
    bool put_threw = false;
    bool flush_threw = false;
    {
        oguard<false> g;
        std_error_device d;

        char buffer[BUFSIZ];
        std::setvbuf(stderr, buffer, _IOFBF, sizeof(buffer));

        const int saved = ::dup(STDERR_FILENO);
        ::close(STDERR_FILENO);

        try { d.dput("some data", 9); }
        catch (const device_error&) { put_threw = true; }
        try { d.dflush(); }
        catch (const device_error&) { flush_threw = true; }

        ::dup2(saved, STDERR_FILENO);
        ::close(saved);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    }
    EXPECT_FALSE(put_threw);
    EXPECT_TRUE(flush_threw);
}

TEST(StdDevice, GetFailsWhenStdinIsClosed)
{
    iguard g("abc");
    std_input_device d;

    const bool threw = throws_with_fd_closed(STDIN_FILENO, [&]
    {
        char buf = 0;
        d.dget(&buf, 1);
    });
    EXPECT_TRUE(threw);
}

// ---------------------------------------------------------------------------
// A non-blocking stdin.  dget() must wait for the data it was asked for rather
// than report a short read, and must recognise the writer hanging up as EOF --
// which is why this needs a pipe: a regular file is always ready and never ends
// mid-stream.
// ---------------------------------------------------------------------------

TEST(StdDevice, GetWaitsOnANonBlockingStdinAndSeesTheHangup)
{
    int pipefds[2];
    ASSERT_NE(::pipe(pipefds), -1);

    const int saved_stdin = ::dup(STDIN_FILENO);
    ::dup2(pipefds[0], STDIN_FILENO);
    const int flags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
    ::fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    std_input_device d;
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

    // Closing the write end is POLLHUP, which is the stream ending.
    ::close(pipefds[1]);
    EXPECT_EQ(d.dget(buf, 1), 0u);
    EXPECT_TRUE(d.deof());

    ::dup2(saved_stdin, STDIN_FILENO);
    ::close(saved_stdin);
    ::close(pipefds[0]);
}

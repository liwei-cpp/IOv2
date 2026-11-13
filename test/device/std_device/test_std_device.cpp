// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/device/std_device.h>

#include <support/platform.h>
#include <support/platform_macros.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#endif

using namespace IOv2;

namespace
{
    namespace sys = iov2_test::sys;

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
    // A second probe must reuse that byte rather than consume another one.
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

// The byte deof() probed comes back first, and the data the device already has
// comes with it: a regular file is always ready, so one dget() still delivers
// the whole thing. Order is preserved, nothing is lost.
TEST(StdDevice, AProbedByteStaysFirstInALongerRead)
{
    iguard g("abc");
    std_input_device d;
    char buf[3] = {};

    EXPECT_FALSE(d.deof());
    EXPECT_EQ(d.dget(buf, sizeof(buf)), sizeof(buf));
    EXPECT_EQ(std::string(buf, sizeof(buf)), "abc");
}

// What the top-up must not do is wait: on a pipe (or a terminal) whose peer has
// sent just that one byte, reading "the rest" would sit in read() for input the
// caller never asked for. Here the writer sends "a", the probe caches it, and
// dget(buf, 3) has to return that "a" right away -- an unconditional top-up
// would block until "bc" arrives and hand over all three.
TEST(StdDevice, AProbedByteDoesNotWaitForMoreInput)
{
    int pipefds[2];
    ASSERT_NE(sys::pipe(pipefds), -1);

    const int saved_stdin = sys::dup(sys::stdin_fd);
    sys::dup2(pipefds[0], sys::stdin_fd);

    std_input_device d;
    char buf[3] = {};

    ASSERT_EQ(sys::write(pipefds[1], "a", 1), 1);
    EXPECT_FALSE(d.deof());                        // "a" is now the look-ahead byte

    std::thread late_writer([write_fd = pipefds[1]]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        (void)sys::write(write_fd, "bc", 2);
    });

    EXPECT_EQ(d.dget(buf, sizeof(buf)), 1u);       // not 3: it must not wait for "bc"
    EXPECT_EQ(buf[0], 'a');
    late_writer.join();

    EXPECT_EQ(d.dget(buf, sizeof(buf)), 2u);
    EXPECT_EQ(std::string(buf, 2), "bc");

    sys::dup2(saved_stdin, sys::stdin_fd);
    sys::close(saved_stdin);
    sys::close(pipefds[0]);
    sys::close(pipefds[1]);
}

// The other shape the top-up sees: the peer sent one byte and hung up. That is
// "ready" too (read() returns 0 at once), so the probed byte comes back and EOF
// latches right behind it rather than one call later.
TEST(StdDevice, AProbedByteBeforeAHangupComesBackAndEofFollows)
{
    int pipefds[2];
    ASSERT_NE(sys::pipe(pipefds), -1);

    const int saved_stdin = sys::dup(sys::stdin_fd);
    sys::dup2(pipefds[0], sys::stdin_fd);

    std_input_device d;
    char buf[3] = {};

    ASSERT_EQ(sys::write(pipefds[1], "a", 1), 1);
    sys::close(pipefds[1]);
    EXPECT_FALSE(d.deof());                        // "a" is the look-ahead byte

    EXPECT_EQ(d.dget(buf, sizeof(buf)), 1u);
    EXPECT_EQ(buf[0], 'a');
    EXPECT_TRUE(d.deof());                         // the hang-up was seen during the top-up
    EXPECT_EQ(d.dget(buf, sizeof(buf)), 0u);

    sys::dup2(saved_stdin, sys::stdin_fd);
    sys::close(saved_stdin);
    sys::close(pipefds[0]);
}

TEST(StdDevice, PutAcceptsANullBufferOnlyForZeroLength)
{
    std_output_device obj;
    obj.dput(nullptr, 0);
    EXPECT_THROW(obj.dput(nullptr, 1), device_error);
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
// Move semantics.  The input device carries the sticky EOF flag and the one
// byte deof() reads ahead; every device carries the duty to flush the stream
// when it dies, and a move hands that duty over too.
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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    d3 = std::move(d3);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_TRUE(d3.deof());
}

TEST(StdDevice, MoveCarriesTheReadMode)
{
    std_input_device d1;
    EXPECT_FALSE(d1.stdio_mode());                 // fd mode by default
    d1.stdio_mode(true);

    std_input_device d2(std::move(d1));
    EXPECT_TRUE(d2.stdio_mode());
    EXPECT_FALSE(d1.stdio_mode());                 // NOLINT(bugprone-use-after-move)

    std_input_device d3;
    d3 = std::move(d2);
    EXPECT_TRUE(d3.stdio_mode());
    EXPECT_FALSE(d2.stdio_mode());                 // NOLINT(bugprone-use-after-move)
}

// ---------------------------------------------------------------------------
// stdio mode: stdin read through its FILE, sharing one buffer with C stdio.
// stdin is made fully buffered where it matters (stdin_full_buffer), so that
// stdio does read ahead -- what fd mode cannot see.
// ---------------------------------------------------------------------------

// The first getchar() pulls the whole pipe into the FILE buffer; in fd mode the
// device would find the fd empty after it.
TEST(StdDevice, StdioModeSharesTheFileBufferWithCStdio)
{
    pipe_iguard g("abcdef");
    g.close_write();
    stdin_full_buffer b;
    std_input_device d;
    d.stdio_mode(true);

    char buf[2] = {};
    EXPECT_EQ(std::getchar(), 'a');
    EXPECT_EQ(d.dget(buf, 2), 2u);
    EXPECT_EQ(std::string(buf, 2), "bc");
    EXPECT_EQ(std::getchar(), 'd');
    EXPECT_EQ(d.dget(buf, 2), 2u);
    EXPECT_EQ(std::string(buf, 2), "ef");
    EXPECT_EQ(d.dget(buf, 2), 0u);
    EXPECT_TRUE(d.deof());
}

TEST(StdDevice, AStdioModeProbeLeavesTheByteForCStdio)
{
    pipe_iguard g("x");
    stdin_full_buffer b;
    std_input_device d;
    d.stdio_mode(true);

    EXPECT_FALSE(d.deof());                        // reads 'x' and pushes it back
    EXPECT_EQ(std::getchar(), 'x');
    g.close_write();
    EXPECT_TRUE(d.deof());
    EXPECT_TRUE(std::feof(stdin));
}

// The EOF is FILE's own: input that arrives after it stays unread until
// clearerr(stdin), whoever reached the end.
TEST(StdDevice, StdioModeEofIsTheFilesStickyFlag)
{
    iguard g("a");
    std_input_device d;
    d.stdio_mode(true);

    char c = 0;
    EXPECT_EQ(d.dget(&c, 1), 1u);
    EXPECT_EQ(d.dget(&c, 1), 0u);
    EXPECT_TRUE(d.deof());

    { std::ofstream("stdio_input_guard", std::ios::app) << 'b'; }
    EXPECT_EQ(d.dget(&c, 1), 0u);
    std::clearerr(stdin);
    EXPECT_EQ(d.dget(&c, 1), 1u);
    EXPECT_EQ(c, 'b');
}

// A byte fd mode probed before the switch is not in the FILE, so the device
// still holds it: it comes first, on its own, and only then the FILE is read.
TEST(StdDevice, AByteProbedInFdModeComesFirstAfterTheSwitch)
{
    pipe_iguard g("xyz");
    g.close_write();
    std_input_device d;
    EXPECT_FALSE(d.deof());                        // fd mode: 'x' cached in the device

    d.stdio_mode(true);
    EXPECT_FALSE(d.deof());                        // the cached byte, not the FILE
    char buf[3] = {};
    EXPECT_EQ(d.dget(buf, 3), 1u);
    EXPECT_EQ(buf[0], 'x');
    EXPECT_EQ(d.dget(buf, 2), 2u);
    EXPECT_EQ(std::string(buf, 2), "yz");
}

// dunget() refuses the same everywhere: fd mode has no buffer shared with C stdio,
// and a byte fd mode probed before a switch would make the order unclear. A refusal
// changes nothing. StdDevicePosix and StdDeviceWindows below have what stdio mode
// does on each platform.
TEST(StdDevice, UngetRefusesWhereItCannotHandTheBytesBack)
{
    pipe_iguard g("xyz");
    g.close_write();
    std_input_device d;

    const std::string a = "a";
    EXPECT_TRUE(d.dunget("a", 0));                 // nothing to hand back
    EXPECT_TRUE(d.dunget(a.end(), a.end()));
    EXPECT_THROW(d.dunget(nullptr, 1), device_error);
    EXPECT_FALSE(d.dunget("a", 1));                // fd mode
    EXPECT_FALSE(d.dunget(a.begin(), a.end()));

    EXPECT_FALSE(d.deof());                        // fd mode: 'x' cached in the device
    d.stdio_mode(true);
    EXPECT_FALSE(d.dunget("a", 1));                // the cached byte is still due first
    EXPECT_FALSE(d.dunget(a.begin(), a.end()));

    char buf[3] = {};
    EXPECT_EQ(d.dget(buf, 3), 1u);
    EXPECT_EQ(buf[0], 'x');
    EXPECT_EQ(d.dget(buf, 2), 2u);
    EXPECT_EQ(std::string(buf, 2), "yz");
}

TEST(StdDevice, AnOutputDeviceMovesEvenThoughItHoldsNothing)
{
    std_output_device d1;
    std_output_device d2(std::move(d1));
    std_output_device d3;
    d3 = std::move(d2);
    SUCCEED();
}

// A moved-from output device is inert: the flush that runs when a device dies
// belongs to whichever object currently holds it, and a move passes it along.
// Bytes left in a fully buffered stdout therefore reach the file when the
// receiving device dies, not when the moved-from one does.
TEST(StdDevice, MoveHandsOverTheClosingFlush)
{
    oguard<true> g;
    char buffer[BUFSIZ];
    std::setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    {
        std_output_device d3;
        {
            std_output_device d2;
            {
                std_output_device d1;
                d1.dput("moved", 5);
                d2 = std::move(d1);
            }                                   // d1 (moved-from) dies: no flush
            EXPECT_TRUE(g.contents().empty());

            std_output_device d2b(std::move(d2));
            d3 = std::move(d2b);
        }                                       // d2, d2b (both moved-from) die: no flush
        EXPECT_TRUE(g.contents().empty());
    }                                           // d3 holds the duty: flushes
    EXPECT_EQ(g.contents(), "moved");

    std::setvbuf(stdout, nullptr, _IONBF, 0);
}

// Move assignment is a pure hand-over: the target's state, the flush duty
// included, becomes the source's. Moving an inert device onto a live one leaves
// the target inert as well, and it does no I/O of its own -- the bytes stay in
// the stdio buffer until someone else flushes. A self-move changes nothing.
TEST(StdDevice, MoveAssignmentHandsOverTheDutyWithoutFlushing)
{
    oguard<true> g;
    char buffer[BUFSIZ];
    std::setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    {
        std_output_device target;
        target.dput("later", 5);

        std_output_device source;
        std_output_device sink(std::move(source));  // source is now inert
        target = std::move(source);                 // inert source overwrites the live target
        EXPECT_TRUE(g.contents().empty());          // ...and flushes nothing on the way

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
        sink = std::move(sink);                     // sink keeps the duty it was given
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        sink.dput("!", 1);
    }                                               // target: no flush; sink: flushes both
    EXPECT_EQ(g.contents(), "later!");

    std::setvbuf(stdout, nullptr, _IONBF, 0);
}

// ---------------------------------------------------------------------------
// A closed descriptor.  Making a standard stream fail by taking its descriptor
// away is a POSIX technique: the Windows CRT ends the process on I/O to a
// closed descriptor, so the cases that reach it run on Linux and macOS only.
// The two here never touch the descriptor, and so run everywhere. Each case
// closes one, drives the operation, and puts it back before reporting: a
// failure printed while stdout or stderr is gone would have nowhere to go.
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

// dflush() is fflush(), and oguard leaves the stream unbuffered, so there is
// never anything queued for fflush() to push at the descriptor: it returns 0
// whether or not the descriptor is still there.  The next two cases pin that;
// the buffered ones, where the flush is what reaches the descriptor, follow them.
TEST(StdDevice, FlushOnAnUnbufferedStdoutSucceedsEvenWithTheDescriptorClosed)
{
    bool threw = false;
    {
        oguard<true> g;
        std_output_device d;
        threw = throws_with_fd_closed(sys::stdout_fd, [&] { d.dflush(); });
    }
    EXPECT_FALSE(threw);
}

TEST(StdDevice, FlushOnAnUnbufferedStderrSucceedsEvenWithTheDescriptorClosed)
{
    bool threw = false;
    {
        oguard<false> g;
        std_error_device d;
        threw = throws_with_fd_closed(sys::stderr_fd, [&] { d.dflush(); });
    }
    EXPECT_FALSE(threw);
}

// The reason a moved-from device must stay quiet: glibc discards the buffer
// when fflush fails, so the first flush is the only one that can see the error.
// If the moved-from object's destructor took that flush, its catch(...) would
// swallow the failure and the owner's own dflush() would find nothing to report.
TEST(StdDevicePosix, AMovedFromDeviceDoesNotConsumeTheFlushFailure)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the CRT terminates on I/O to a closed descriptor";
#endif
}

TEST(StdDevicePosix, PutFailsWhenStdoutIsClosed)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
    bool threw = false;
    {
        oguard<true> g;
        std_output_device d;
        threw = throws_with_fd_closed(sys::stdout_fd, [&] { d.dput("test", 4); });
    }
    EXPECT_TRUE(threw);
#else
    GTEST_SKIP() << "the CRT terminates on I/O to a closed descriptor";
#endif
}

TEST(StdDevicePosix, PutFailsWhenStderrIsClosed)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
    bool threw = false;
    {
        oguard<false> g;
        std_error_device d;
        threw = throws_with_fd_closed(sys::stderr_fd, [&] { d.dput("test", 4); });
    }
    EXPECT_TRUE(threw);
#else
    GTEST_SKIP() << "the CRT terminates on I/O to a closed descriptor";
#endif
}

// The stream gets a buffer so that the flush is the call that reaches the
// descriptor -- which is what makes dflush()'s throw reachable. (Unbuffered,
// fflush() has nothing to push and succeeds; the two cases above pin that.)
TEST(StdDevicePosix, FlushReportsTheFailedWriteOfABufferedStdout)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the CRT terminates on I/O to a closed descriptor";
#endif
}

TEST(StdDevicePosix, FlushReportsTheFailedWriteOfABufferedStderr)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the CRT terminates on I/O to a closed descriptor";
#endif
}

TEST(StdDevicePosix, DestructorSuppressesABufferedFlushFailure)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the CRT terminates on I/O to a closed descriptor";
#endif
}

TEST(StdDevicePosix, GetFailsWhenStdinIsClosed)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
    iguard g("abc");
    std_input_device d;

    const bool threw = throws_with_fd_closed(sys::stdin_fd, [&]
    {
        char buf = 0;
        d.dget(&buf, 1);
    });
    EXPECT_TRUE(threw);
#else
    GTEST_SKIP() << "the CRT terminates on I/O to a closed descriptor";
#endif
}

// ---------------------------------------------------------------------------
// A read interrupted by a signal, and a stdin with O_NONBLOCK. Windows has
// neither EINTR nor O_NONBLOCK on a CRT descriptor.
// ---------------------------------------------------------------------------

#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
namespace
{
    void no_op_signal_handler(int) {}
}
#endif

TEST(StdDevicePosix, GetRetriesReadAndPollAfterSignalInterrupts)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "needs signals (EINTR) and O_NONBLOCK on stdin";
#endif
}

// dget() on a non-blocking stdin must wait for the data it was asked for rather
// than report a short read, and must recognise the writer hanging up as EOF --
// which is why this needs a pipe: a regular file is always ready and never ends
// mid-stream.  Both read modes: stdio mode meets the EAGAIN as a FILE error.
TEST(StdDevicePosix, GetWaitsOnANonBlockingStdinAndSeesTheHangup)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "needs O_NONBLOCK on stdin";
#endif
}

// ---------------------------------------------------------------------------
// dunget() in stdio mode.  POSIX C libraries push back more than the one byte
// the standard guarantees (glibc and BSD grow the area on demand), so these
// succeed there; Windows refuses, see StdDeviceWindows below.
// ---------------------------------------------------------------------------

TEST(StdDevicePosix, UngetHandsTheBytesBackToCStdioInOrder)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the UCRT's ungetc() takes no pushback here (see StdDeviceWindows)";
#endif
}

// The iterator form takes any bidirectional range: here the reverse of a stack
// kept with its next byte at the back, as ichannel keeps its read buffer, and a
// non-contiguous list.
TEST(StdDevicePosix, UngetTakesTheBytesAsAnIteratorRange)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the UCRT's ungetc() takes no pushback here (see StdDeviceWindows)";
#endif
}

// Past glibc's first 128-byte pushback area, so it has to grow, and with 0xFF in
// it: as a signed char that byte is EOF, which ungetc() would refuse.
TEST(StdDevicePosix, UngetHandsBackABlockWithEveryByteValue)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the UCRT's ungetc() takes no pushback here (see StdDeviceWindows)";
#endif
}

// The bytes pushed back are read even after the end was reached: ungetc() clears
// the EOF flag. Once they are gone, the end is met again.
TEST(StdDevicePosix, UngetAfterEofMakesTheBytesReadable)
{
#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_MACOS
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
#else
    GTEST_SKIP() << "the UCRT's ungetc() takes no pushback here (see StdDeviceWindows)";
#endif
}

// ---------------------------------------------------------------------------
// Windows reads stdin in the CRT's text mode: `\r\n` arrives as `\n`. The
// device probes with a 1-byte read (deof) and then tops up, so a CR may be the
// last byte a read sees, with its LF still to come. Real Windows handles that
// (all three cases pass in CI, with cl, clang-cl, debug, ASan and x86); under
// Wine the 1-byte _read of a piped CRLF returns 0, taken as EOF -- Wine's
// emulation, not the UCRT -- so that case skips itself under the local Wine
// presets (IOV2_TEST_UNDER_WINE), and runs everywhere else.
//
// Every case expects all of "a\nb\nc": nothing lost, no early EOF.
// ---------------------------------------------------------------------------

#if IOV2_TEST_WINDOWS
namespace
{
    // Reads to EOF the way the stream layer does: probe with deof(), then
    // take what is there.
    std::string read_all_probing(std_input_device& d)
    {
        std::string res;
        char buf[64];
        while (!d.deof())
            res.append(buf, d.dget(buf, sizeof buf));
        return res;
    }

    std::string read_all_bytewise(std_input_device& d)
    {
        std::string res;
        char c = 0;
        while (d.dget(&c, 1) == 1)
            res += c;
        return res;
    }
}
#endif

TEST(StdDeviceWindows, APipedCrlfReadsBackWhenProbed)
{
#if IOV2_TEST_WINDOWS
    std::string got;
    {
        pipe_iguard g("a\r\nb\r\nc");
        g.close_write();
        std_input_device d;
        got = read_all_probing(d);
    }
    EXPECT_EQ(got, "a\nb\nc");
#else
    GTEST_SKIP() << "the CRT's text-mode stdin";
#endif
}

TEST(StdDeviceWindows, APipedCrlfReadsBackByteByByte)
{
#if IOV2_TEST_WINDOWS
    if (iov2_test::sys::env_is_set("IOV2_TEST_UNDER_WINE"))
        GTEST_SKIP() << "Wine's pipe emulation ends the 1-byte read of a CRLF early";

    std::string got;
    {
        pipe_iguard g("a\r\nb\r\nc");
        g.close_write();
        std_input_device d;
        got = read_all_bytewise(d);
    }
    EXPECT_EQ(got, "a\nb\nc");
#else
    GTEST_SKIP() << "the CRT's text-mode stdin";
#endif
}

// The control: the same bytes from a redirected file, where Wine already read
// them correctly. iguard writes in text mode, so "\n" lands on disk as "\r\n".
TEST(StdDeviceWindows, ARedirectedCrlfFileReadsBack)
{
#if IOV2_TEST_WINDOWS
    std::string got;
    {
        iguard g("a\nb\nc");
        std_input_device d;
        got = read_all_probing(d);
    }
    EXPECT_EQ(got, "a\nb\nc");
#else
    GTEST_SKIP() << "the CRT's text-mode stdin";
#endif
}

// dunget() refuses in stdio mode here: the UCRT's ungetc() has no pushback area of
// its own and may overwrite an unread byte, and FILE cannot be asked for room.
// The refusal leaves the stream as it was.
TEST(StdDeviceWindows, UngetIsRefusedInStdioMode)
{
#if IOV2_TEST_WINDOWS
    std::string got;
    bool handed_back = true;
    {
        iguard g("a\nb\nc");
        std_input_device d;
        d.stdio_mode(true);
        char c = 0;
        got.append(&c, d.dget(&c, 1));
        handed_back = d.dunget(&c, 1);
        got += read_all_probing(d);
    }
    EXPECT_FALSE(handed_back);
    EXPECT_EQ(got, "a\nb\nc");
#else
    GTEST_SKIP() << "the UCRT's ungetc()";
#endif
}

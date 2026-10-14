// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/device/std_device.h>

#include <support/platform.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

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
// closed descriptor, so the cases that reach it are in posix/. What stays here
// never touches the descriptor, and so runs everywhere.
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
// posix/ has the buffered ones, where the flush is what reaches the descriptor.
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

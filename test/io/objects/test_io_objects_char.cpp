// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The narrow standard stream objects: cout, cerr, clog and cin.
 *
 * [iostream.objects] fixes what they are attached to and how they are tied,
 * and every property below follows from that. cerr is unit-buffered so a
 * diagnostic is not lost when the program dies; cerr and cin are both tied to
 * cout, which is what makes a prompt appear before the read that follows it.
 * That tie is the one users notice when it is missing, so it is checked by
 * observing the prompt rather than by comparing pointers alone.
 *
 * The objects also have to survive sync_with_stdio: it changes how they reach
 * the C streams, not which objects they are, so their addresses must not move.
 */
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <string>
#include <thread>
#include <type_traits>

#include <fcntl.h>
#include <unistd.h>

// The two implementation templates exist for the fixed-fd devices only: every
// "cannot fail" argument in their headers assumes std_device<0/1/2>. Naming the
// template with any other device must be rejected at the template head.
namespace
{
    struct probe_stream;

    template <typename Dev>
    concept stdin_api_accepts = requires { typename IOv2::stdin_api<probe_stream, Dev, char>; };
    template <typename Dev>
    concept stdout_api_accepts = requires { typename IOv2::stdout_api<probe_stream, Dev, char>; };

    static_assert(stdin_api_accepts<IOv2::std_device<STDIN_FILENO>>);
    static_assert(!stdin_api_accepts<IOv2::std_device<STDOUT_FILENO>>);
    static_assert(!stdin_api_accepts<IOv2::std_device<STDERR_FILENO>>);

    static_assert(stdout_api_accepts<IOv2::std_device<STDOUT_FILENO>>);
    static_assert(stdout_api_accepts<IOv2::std_device<STDERR_FILENO>>);
    static_assert(!stdout_api_accepts<IOv2::std_device<STDIN_FILENO>>);

    // Writes '1', switches cout back to synchronized, printf()s, then writes '2'
    // -- all from inside one insertion, with that insertion's sentry alive.
    struct flipper {};
}

namespace IOv2
{
    template <>
    struct io_traits<char, flipper>
    {
        template <typename TIter>
            requires (std::is_same_v<char, typename TIter::value_type>)
        static TIter swrite(TIter iter, ios_base<char>&, const locale<char>&, flipper)
        {
            *iter++ = '1';
            IOv2::cout.sync_with_stdio(true);   // recursive lock: allowed, and it hands over
            std::printf("P");
            *iter++ = '2';
            return iter;
        }
    };
}

TEST(IoObjectsChar, EachStreamWritesToItsOwnDestination)
{
    {
        oguard<true> out;
        IOv2::cout << "to stdout" << IOv2::endl;
        EXPECT_EQ(out.contents(), "to stdout\n");
    }
    {
        oguard<false> err;
        IOv2::cerr << "to stderr" << IOv2::endl;
        EXPECT_EQ(err.contents(), "to stderr\n");
    }
    {
        oguard<false> err;
        IOv2::clog << "also stderr" << IOv2::endl;
        EXPECT_EQ(err.contents(), "also stderr\n");
    }
}

// The two destinations do not bleed into one another, in either order.
TEST(IoObjectsChar, StdoutAndStderrStaySeparate)
{
    oguard<true>  out;
    oguard<false> err;

    IOv2::cout << "first ";
    IOv2::cout.flush();
    IOv2::cerr << "second";
    IOv2::cerr.flush();
    IOv2::cout << "third" << IOv2::endl;
    IOv2::cout.flush();

    EXPECT_EQ(out.contents(), "first third\n");
    EXPECT_EQ(err.contents(), "second");
}

TEST(IoObjectsChar, CerrIsUnitBufferedAndBothInputsAreTiedToCout)
{
    EXPECT_TRUE(IOv2::cerr.flags() & IOv2::ios_defs::unitbuf);
    EXPECT_EQ(IOv2::cerr.tie(), &IOv2::cout);
    EXPECT_EQ(IOv2::cin.tie(), &IOv2::cout);

    // The flags a fresh stream starts with.
    EXPECT_TRUE(IOv2::cerr.flags() & IOv2::ios_defs::dec);
    EXPECT_TRUE(IOv2::cerr.flags() & IOv2::ios_defs::skipws);
}

// What the tie is for: an unterminated prompt is still sitting in cout's buffer
// when the read starts, and the read is what pushes it out.
TEST(IoObjectsChar, ReadingFromCinFlushesThePromptOnCout)
{
    IOv2::cout.reset();
    IOv2::cin.reset();
    IOv2::cout.sync_with_stdio(false);

    oguard<true> out;
    iguard       in("Ada");

    IOv2::cout << "ready" << IOv2::endl;
    IOv2::cout << "name? ";                 // no newline, no flush
    EXPECT_EQ(out.contents(), "ready\n");   // so it has not gone out yet

    std::string answer;
    IOv2::cin >> answer;
    EXPECT_EQ(answer, "Ada");
    EXPECT_EQ(out.contents(), "ready\nname? ");
}

// Each block re-points stdin, so cin is reset with it: these are global objects
// and a previous test's end-of-input would otherwise still be on them.
TEST(IoObjectsChar, CinReadsAndPutsBack)
{
    {
        iguard g("alpha beta");
        IOv2::cin.reset();
        char   first = 0;
        char   again = 1;

        IOv2::cin.get(first);
        IOv2::cin.putback(first);
        IOv2::cin.get(again);

        EXPECT_TRUE(IOv2::cin.good());
        EXPECT_EQ(first, again);
    }
    {
        iguard g("alpha beta");
        IOv2::cin.reset();
        char   buf[2];
        // Hoisted: the template argument list would look like a second macro
        // argument to the preprocessor.
        char* end = IOv2::cin.get<IOv2::keep_sep, IOv2::no_zt>(buf, 2);
        EXPECT_EQ(end - buf, 2);

        IOv2::cin.putback(buf[1]);
        EXPECT_EQ(IOv2::cin.get(), buf[1]);
    }
    {
        iguard g("\n");
        IOv2::cin.reset();
        EXPECT_TRUE(static_cast<bool>(IOv2::cin.ignore(1)));
    }
}

// ignore() probes for EOF before each byte it discards, and in unsynchronized
// mode the buffered read that follows the probe used to top the probed byte up
// from the device -- which on a pipe or a terminal means waiting for input the
// caller never asked for: "press Enter to continue" would not return on an
// empty line until the next line arrived. The byte the probe found must be
// enough on its own.
TEST(IoObjectsChar, UnsyncedIgnoreReturnsOnTheByteItFoundWithoutWaitingForMore)
{
    int pipefds[2];
    ASSERT_NE(::pipe(pipefds), -1);
    const int saved_stdin = ::dup(STDIN_FILENO);
    ::dup2(pipefds[0], STDIN_FILENO);

    IOv2::cin.reset();
    const bool sync = IOv2::cin.sync_with_stdio(false);

    ASSERT_EQ(::write(pipefds[1], "\n", 1), 1);   // the empty line, and nothing else yet

    std::atomic<bool> more_arrived{false};
    std::thread late_writer([&, write_fd = pipefds[1]]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        more_arrived.store(true);
        (void)::write(write_fd, "x\n", 2);
    });

    IOv2::cin.ignore();
    EXPECT_FALSE(more_arrived.load());            // it came back on the "\n" alone
    EXPECT_TRUE(IOv2::cin.good());
    late_writer.join();

    EXPECT_EQ(IOv2::cin.get(), 'x');              // and nothing was lost in between

    IOv2::cin.sync_with_stdio(sync);
    ::dup2(saved_stdin, STDIN_FILENO);
    ::close(saved_stdin);
    ::close(pipefds[0]);
    ::close(pipefds[1]);
    IOv2::cin.reset();
}

// What "synchronized" buys is that nothing is read ahead of what the operation
// needs, so bytes this stream did not consume stay on the fd for C stdio (or
// anyone else) to read. read(buf, 5) is one read(0, buf, 5), leaving the sixth
// byte where it was; unsynchronized the same call pulls a whole block into this
// stream's buffer, and the fd is empty afterwards. Neither loses a byte -- the
// difference is only where the rest is waiting.
TEST(IoObjectsChar, SynchronizedReadsNoFurtherThanTheOperationNeeds)
{
    auto sixth_byte_left_on_the_fd = [](bool sync)
    {
        int pipefds[2];
        EXPECT_NE(::pipe(pipefds), -1);
        const int saved_stdin = ::dup(STDIN_FILENO);
        ::dup2(pipefds[0], STDIN_FILENO);

        EXPECT_EQ(::write(pipefds[1], "12345X", 6), 6);
        ::close(pipefds[1]);                      // no more input: read() cannot block

        IOv2::cin.reset();
        const bool old = IOv2::cin.sync_with_stdio(sync);

        char buf[5] = {};
        IOv2::cin.read(buf, 5);
        EXPECT_EQ(std::string(buf, 5), "12345");

        char rest = 0;
        const ssize_t got = ::read(STDIN_FILENO, &rest, 1);

        IOv2::cin.sync_with_stdio(old);
        ::dup2(saved_stdin, STDIN_FILENO);
        ::close(saved_stdin);
        ::close(pipefds[0]);
        IOv2::cin.reset();
        return got == 1 && rest == 'X';
    };

    EXPECT_TRUE(sixth_byte_left_on_the_fd(true));
    EXPECT_FALSE(sixth_byte_left_on_the_fd(false));   // buffered into this stream instead
}

// Switching back from inside an insertion cuts that insertion in two: what it
// has already written is handed to stdio by the switch itself, while the rest
// stays governed by the value the sentry read on construction and goes out at
// the end of the next insertion. Anything printf()ed in between lands between
// the halves. Nothing is lost or duplicated -- the order is the point.
TEST(IoObjectsChar, SwitchingBackInsideAnInsertionSplitsIt)
{
    oguard<true> out;
    IOv2::cout.reset();
    ASSERT_TRUE(out.contents().empty());
    const bool sync = IOv2::cout.sync_with_stdio(false);

    {
        stdout_full_buffer buffered;              // so stdio's own order is what shows

        IOv2::cout << "X" << flipper{};           // writes X1, switches, printf(P), writes 2
        IOv2::cout << "Y";                        // synchronized now: carries 2 out with it
        std::fflush(stdout);
    }

    EXPECT_EQ(out.contents(), "X1P2Y");
    IOv2::cout.sync_with_stdio(sync);
}

// reset() offers the bytes it is about to drop to the old device once. When
// that write cannot land, the loss is reported as devfailbit rather than
// thrown, and the stream is still fully usable after clear() -- the device has
// been replaced and the converter is initialized, not left half-way.
TEST(IoObjectsChar, ResetReportsAFailedFlushOfTheDroppedBytesAndStaysUsable)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<true> out;
    const bool   sync = IOv2::cout.sync_with_stdio(false);

    IOv2::cout << "DROPPED";                      // still in this stream's buffer

    const int saved = ::dup(STDOUT_FILENO);
    ::dup2(full, STDOUT_FILENO);                  // the pending bytes cannot land
    EXPECT_NO_THROW(IOv2::cout.reset());
    ::dup2(saved, STDOUT_FILENO);
    ::close(saved);
    ::close(full);

    EXPECT_EQ(IOv2::cout.rdstate(), IOv2::ios_defs::devfailbit);

    IOv2::cout.clear();
    IOv2::cout << "AFTER" << IOv2::flush;

    EXPECT_TRUE(IOv2::cout.good());
    EXPECT_EQ(out.contents(), "AFTER");

    IOv2::cout.sync_with_stdio(sync);
}

// In synchronized mode an insertion can leave this stream's bytes in stdout's
// FILE buffer: fwrite succeeds, and only a later fflush sees the broken fd.
// reset() must report that later failure too, before discarding the old device.
TEST(IoObjectsChar, ResetReportsAFlushFailureFromAFullBufferedStdout)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<true> out;
    IOv2::cout.reset();
    const bool sync = IOv2::cout.sync_with_stdio(true);

    {
        stdout_full_buffer buffered;                // restores unbuffered stdout on any exit

        IOv2::cout << "DROPPED";                    // moved into stdout's FILE buffer
        EXPECT_TRUE(out.contents().empty());

        const int saved = ::dup(STDOUT_FILENO);
        ASSERT_NE(saved, -1);
        EXPECT_EQ(::dup2(full, STDOUT_FILENO), STDOUT_FILENO);
        EXPECT_NO_THROW(IOv2::cout.reset());
        EXPECT_EQ(::dup2(saved, STDOUT_FILENO), STDOUT_FILENO);
        ::close(saved);
        ::close(full);
    }

    EXPECT_EQ(IOv2::cout.rdstate(), IOv2::ios_defs::devfailbit);

    IOv2::cout.clear();
    IOv2::cout << "AFTER" << IOv2::flush;

    EXPECT_TRUE(IOv2::cout.good());
    EXPECT_NE(out.contents().find("AFTER"), std::string::npos);

    IOv2::cout.sync_with_stdio(sync);
}

// sync_with_stdio changes how the objects reach the C streams, not which
// objects they are.
TEST(IoObjectsChar, SyncWithStdioDoesNotReplaceTheObjects)
{
    const void* before[] = {&IOv2::cout, &IOv2::cin, &IOv2::cerr, &IOv2::clog};

    IOv2::sync_with_stdio(false);

    const void* after[] = {&IOv2::cout, &IOv2::cin, &IOv2::cerr, &IOv2::clog};

    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_EQ(before[i], after[i]) << "object " << i;

    IOv2::sync_with_stdio(true);
}

// synced_with_stdio() reports the state; sync_with_stdio() sets it. Reading the
// state through the setter is what the standard's one-function interface forces,
// and on an input stream it costs the buffered input: each call rebuilds the
// streambuf. The getter exists so that query and switch are separate operations.
TEST(IoObjectsChar, SyncWithStdioCanBeQueriedWithoutSwitching)
{
    iguard g("one two three\n");
    IOv2::cin.reset();
    IOv2::cout.reset();

    EXPECT_TRUE(IOv2::cin.synced_with_stdio());
    EXPECT_TRUE(IOv2::cout.synced_with_stdio());

    EXPECT_TRUE(IOv2::cin.sync_with_stdio(false));     // returns the previous state
    EXPECT_FALSE(IOv2::cin.synced_with_stdio());
    EXPECT_TRUE(IOv2::cout.synced_with_stdio());       // and switches that stream alone

    std::string first;
    IOv2::cin >> first;                                // pulls the rest into cin's buffer
    EXPECT_EQ(first, "one");

    // The query leaves both the state and the buffered input alone. Through the
    // setter this would be sync_with_stdio() + sync_with_stdio(false): two
    // rebuilds, and " two three\n" would be gone.
    EXPECT_FALSE(IOv2::cin.synced_with_stdio());

    std::string second;
    IOv2::cin >> second;
    EXPECT_EQ(second, "two");
    EXPECT_TRUE(IOv2::cin.good());

    IOv2::cin.sync_with_stdio(true);
    EXPECT_TRUE(IOv2::cin.synced_with_stdio());
}

// Synchronized means this stream's bytes reach stdio in the order they were
// written relative to printf. Switching back to it has to make that true of the
// bytes already buffered, not only of the insertions that follow: the flag is
// read by each insertion's sentry, so without a flush here what was buffered
// while unsynchronized would sit in this stream's own buffer and surface after
// the next printf -- or at exit.
TEST(IoObjectsChar, SwitchingBackToSyncPushesWhatWasAlreadyBuffered)
{
    oguard<true> out;
    IOv2::cout.reset();

    const bool sync = IOv2::cout.sync_with_stdio(false);
    IOv2::cout << "A";
    EXPECT_TRUE(out.contents().empty());            // still in this stream's buffer

    EXPECT_FALSE(IOv2::cout.sync_with_stdio(true)); // returns the previous state...
    EXPECT_EQ(out.contents(), "A");                 // ...and hands the bytes over

    std::printf("B");
    std::fflush(stdout);
    EXPECT_EQ(out.contents(), "AB");
    EXPECT_TRUE(IOv2::cout.good());

    // Already synchronized: nothing to hand over, and no second flush.
    EXPECT_TRUE(IOv2::cout.sync_with_stdio(true));
    EXPECT_EQ(out.contents(), "AB");

    IOv2::cout.sync_with_stdio(sync);
}

// The hand-over that switching back performs must not invent a failure on a
// stream that is already in a failed state: it is the sentry's operation, not
// stream-level flush() (which would throw stream_error there and show up as a
// strfailbit this switch did not cause), and when the device is fine it simply
// succeeds and leaves the bits as they were.
TEST(IoObjectsChar, SwitchingBackToSyncOnAFailedStreamAddsNoState)
{
    oguard<true> out;
    IOv2::cout.reset();
    ASSERT_TRUE(out.contents().empty());   // nothing carried over from an earlier case

    const bool sync = IOv2::cout.sync_with_stdio(false);
    IOv2::cout.setstate(IOv2::ios_defs::devfailbit);

    EXPECT_FALSE(IOv2::cout.sync_with_stdio(true));
    EXPECT_EQ(IOv2::cout.rdstate(), IOv2::ios_defs::devfailbit);

    IOv2::cout.clear();
    IOv2::cout << "AFTER" << IOv2::flush;
    EXPECT_TRUE(IOv2::cout.good());
    EXPECT_EQ(out.contents(), "AFTER");

    IOv2::cout.sync_with_stdio(sync);
}

// A failed state does not exempt the stream from the hand-over: what the
// earlier failure left in its buffer is exactly what has to reach stdio before
// the caller's next printf. With a good() gate in front of the hand-over "A"
// would stay behind until the next insertion and come out as "PAB".
TEST(IoObjectsChar, SwitchingBackToSyncOnAFailedStreamStillHandsOverItsBytes)
{
    oguard<true> out;
    IOv2::cout.reset();
    ASSERT_TRUE(out.contents().empty());
    const bool sync = IOv2::cout.sync_with_stdio(false);

    {
        stdout_full_buffer buffered;               // so the order inside stdio's buffer is what counts

        IOv2::cout << "A";                         // cout's own buffer
        IOv2::cout.setstate(IOv2::ios_defs::devfailbit);

        EXPECT_FALSE(IOv2::cout.sync_with_stdio(true));
        EXPECT_EQ(IOv2::cout.rdstate(), IOv2::ios_defs::devfailbit);   // the hand-over succeeded: no new bit

        IOv2::cout.clear();
        std::printf("P");
        IOv2::cout << "B";
        std::fflush(stdout);
    }

    EXPECT_EQ(out.contents(), "APB");
    IOv2::cout.sync_with_stdio(sync);
}

// Switching back moves this stream's buffer into stdio's buffer -- the sentry's
// operation -- and stops there. It must not fflush on top of that: stdout's FILE
// buffer may hold printf's bytes, and a failure to write those is not this
// stream's to report (nor to consume: glibc drops the buffer and clears the
// error on a failed fflush).
TEST(IoObjectsChar, SwitchingBackToSyncDoesNotFlushStdioForOthers)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<true> out;
    IOv2::cout.reset();
    ASSERT_TRUE(out.contents().empty());
    const bool sync = IOv2::cout.sync_with_stdio(false);

    {
        stdout_full_buffer buffered;

        std::printf("P");                          // stdio's bytes only; cout holds nothing

        const int saved = ::dup(STDOUT_FILENO);
        ASSERT_NE(saved, -1);
        EXPECT_EQ(::dup2(full, STDOUT_FILENO), STDOUT_FILENO);
        EXPECT_FALSE(IOv2::cout.sync_with_stdio(true));
        EXPECT_TRUE(IOv2::cout.good());            // nothing of cout's failed
        EXPECT_EQ(::dup2(saved, STDOUT_FILENO), STDOUT_FILENO);
        ::close(saved);
        ::close(full);
        std::fflush(stdout);                       // stdio delivers its own bytes on its own
    }

    EXPECT_EQ(out.contents(), "P");
    IOv2::cout.sync_with_stdio(sync);
}

// The eight streams are independent, so the free function attempts all of them
// and reports what happened to each: a stream whose exceptions() mask is armed
// throws, the rest still switch, and the collected outcomes travel in one
// sync_error whose what() names the streams that failed and why. Under the
// default mask nothing throws at all.
TEST(IoObjectsChar, SyncWithStdioReportsEveryStreamThatFailed)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<true> out;
    IOv2::cout.reset();
    ASSERT_TRUE(out.contents().empty());

    IOv2::sync_with_stdio(false);
    IOv2::cout << "DROPPED";                  // stays in cout's own buffer
    IOv2::cout.exceptions(IOv2::ios_defs::devfailbit);

    const int saved = ::dup(STDOUT_FILENO);
    ASSERT_NE(saved, -1);
    EXPECT_EQ(::dup2(full, STDOUT_FILENO), STDOUT_FILENO);

    bool threw = false;
    std::string reported;
    bool only_cout = false;
    try
    {
        IOv2::sync_with_stdio(true);          // cout's flush fails on /dev/full
    }
    catch (const IOv2::sync_error& e)
    {
        threw = true;
        reported = e.what();
        const auto& f = e.failed();
        only_cout = static_cast<bool>(f.cout_err) && !f.cerr_err && !f.clog_err
                 && !f.wcout_err && !f.wcerr_err && !f.wclog_err
                 && !f.cin_err && !f.wcin_err;
    }

    EXPECT_EQ(::dup2(saved, STDOUT_FILENO), STDOUT_FILENO);
    ::close(saved);
    ::close(full);

    EXPECT_TRUE(threw);
    EXPECT_TRUE(only_cout) << "the failure table must name exactly the stream that failed";
    // The reason is whatever the device reported -- an unbuffered stdout fails in
    // dput, a buffered one in dflush -- so what() is checked for the stream's name
    // and for carrying a reason at all, not for one layer's wording.
    EXPECT_NE(reported.find("cout"), std::string::npos) << reported;
    EXPECT_NE(reported.find("std_device"), std::string::npos) << reported;

    // The streams that did not fail switched anyway.
    EXPECT_TRUE(IOv2::cerr.synced_with_stdio());
    EXPECT_TRUE(IOv2::cin.synced_with_stdio());

    IOv2::cout.exceptions(IOv2::ios_defs::goodbit);
    IOv2::cout.clear();

    // With no mask armed the same failure is only a state bit, and nothing throws.
    EXPECT_NO_THROW(IOv2::sync_with_stdio(false));
    EXPECT_NO_THROW(IOv2::sync_with_stdio(true));
    EXPECT_TRUE(IOv2::cout.good());
}

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
#include <IOv2/device/mem_device.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/stdio_guard.h>
#include <support/test_child.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <type_traits>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

// The two implementation templates exist for the fixed-fd devices only: every
// "cannot fail" argument in their headers assumes std_device<0/1/2>. Naming the
// template with any other device must be rejected at the template head.
namespace
{
    template <typename Dev>
    concept stdin_api_accepts = requires { typename IOv2::stdin_api<Dev, char>; };
    template <typename Dev>
    concept stdout_api_accepts = requires { typename IOv2::stdout_api<Dev, char>; };

    static_assert(stdin_api_accepts<IOv2::std_device<STDIN_FILENO>>);
    static_assert(!stdin_api_accepts<IOv2::std_device<STDOUT_FILENO>>);
    static_assert(!stdin_api_accepts<IOv2::std_device<STDERR_FILENO>>);

    static_assert(stdout_api_accepts<IOv2::std_device<STDOUT_FILENO>>);
    static_assert(stdout_api_accepts<IOv2::std_device<STDERR_FILENO>>);
    static_assert(!stdout_api_accepts<IOv2::std_device<STDIN_FILENO>>);

    // A slice of a standard stream must not be copied or moved out of it.
    template <typename T>
    constexpr bool neither_copyable_nor_movable = !std::is_copy_constructible_v<T>
                                               && !std::is_move_constructible_v<T>
                                               && !std::is_copy_assignable_v<T>
                                               && !std::is_move_assignable_v<T>;

    static_assert(neither_copyable_nor_movable<IOv2::stdout_api<IOv2::std_device<STDOUT_FILENO>, char>>);
    static_assert(neither_copyable_nor_movable<IOv2::stdin_api<IOv2::std_device<STDIN_FILENO>, char>>);

    static_assert(std::is_final_v<IOv2::stdin_sync>);

    // In std_stream_common_operators detach() and adjust() are protected, so even a qualified
    // call cannot reach them on a standard stream; on an ordinary stream they stay public. The
    // standard streams still offer adjust() of their own (cin's catches stdin_sync first).
    template <typename S, typename Base>
    concept base_detach_reachable = requires (S& s) { s.Base::detach(); };
    template <typename S, typename Base>
    concept base_adjust_reachable = requires (S& s, const IOv2::cvt_behavior& b) { s.Base::adjust(b); };
    template <typename S>
    concept own_adjust_callable = requires (S& s, const IOv2::cvt_behavior& b) { s.adjust(b); };

    using ostream_t = IOv2::ostream<IOv2::mem_device<char>, char>;
    static_assert(base_detach_reachable<ostream_t, IOv2::stream_common_operators>);
    static_assert(base_adjust_reachable<ostream_t, IOv2::stream_common_operators>);
    static_assert(!base_detach_reachable<IOv2::cout_t, IOv2::std_stream_common_operators>);
    static_assert(!base_detach_reachable<IOv2::cin_t, IOv2::std_stream_common_operators>);
    static_assert(!base_adjust_reachable<IOv2::cout_t, IOv2::std_stream_common_operators>);
    static_assert(!base_adjust_reachable<IOv2::cin_t, IOv2::std_stream_common_operators>);
    static_assert(own_adjust_callable<IOv2::cout_t> && own_adjust_callable<IOv2::cin_t>);

    // tie(p) through a reference to the common base would skip the cycle check (the base
    // cannot tell whether the stream may be tied to) and could not report a rejection. It
    // used to fail only by accident, on handle_exception missing from the base; it is now
    // constrained out. On the streams themselves, input streams included, it stays callable.
    template <typename S>
    concept tie_settable = requires (S& s, IOv2::tie_target* t) { s.tie(t); };
    static_assert(!tie_settable<IOv2::std_stream_common_operators>);
    static_assert(!tie_settable<IOv2::stream_common_operators>);
    static_assert(tie_settable<IOv2::cout_t> && tie_settable<IOv2::cin_t> && tie_settable<ostream_t>);

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
// state through the setter is what the standard's one-function interface forces;
// the getter exists so that query and switch are separate operations.
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

    // The query leaves both the state and the buffered input alone.
    EXPECT_FALSE(IOv2::cin.synced_with_stdio());

    std::string second;
    IOv2::cin >> second;
    EXPECT_EQ(second, "two");
    EXPECT_TRUE(IOv2::cin.good());

    IOv2::cin.sync_with_stdio(true);
    EXPECT_TRUE(IOv2::cin.synced_with_stdio());
}

// Switching an input stream flips a flag on its root and rebuilds nothing: what the
// unsynchronized stream had read ahead is still handed out after a switch to
// synchronized, and only then does it read fd 0 on demand. It used to be dropped
// with the old iochannel.
TEST(IoObjectsChar, SwitchingToSynchronizedKeepsTheInputReadAhead)
{
    iguard g("11 22 33\n44");
    IOv2::cin.reset();
    IOv2::cin.sync_with_stdio(false);

    int x = 0;
    IOv2::cin >> x;                                    // pulls the whole input into the buffer
    EXPECT_EQ(x, 11);

    EXPECT_FALSE(IOv2::cin.sync_with_stdio(true));
    int y = 0, z = 0, w = 0;
    IOv2::cin >> y >> z >> w;
    EXPECT_EQ(y, 22);
    EXPECT_EQ(z, 33);
    EXPECT_EQ(w, 44);
    EXPECT_TRUE(IOv2::cin.eof());

    IOv2::cin.reset();
}

// adjust(stdin_sync{...}) on the stream is sync_with_stdio: it used to flip the root's
// flag alone, so the stream still reported synchronized, sync_with_stdio(true) returned
// early without switching back, and the read below took the whole input off the fd.
TEST(IoObjectsChar, AdjustingWithStdinSyncIsSyncWithStdio)
{
    int pipefds[2];
    ASSERT_NE(::pipe(pipefds), -1);
    const int saved_stdin = ::dup(STDIN_FILENO);
    ::dup2(pipefds[0], STDIN_FILENO);

    EXPECT_EQ(::write(pipefds[1], "12345X", 6), 6);
    ::close(pipefds[1]);                          // no more input: read() cannot block

    IOv2::cin.reset();
    IOv2::cin.adjust(IOv2::stdin_sync{false});
    EXPECT_FALSE(IOv2::cin.synced_with_stdio());
    EXPECT_FALSE(IOv2::cin.sync_with_stdio(true)); // a real switch back

    char buf[5] = {};
    IOv2::cin.read(buf, 5);
    EXPECT_EQ(std::string(buf, 5), "12345");

    char rest = 0;
    EXPECT_EQ(::read(STDIN_FILENO, &rest, 1), 1);  // synchronized: left on the fd
    EXPECT_EQ(rest, 'X');

    ::dup2(saved_stdin, STDIN_FILENO);
    ::close(saved_stdin);
    ::close(pipefds[0]);
    IOv2::cin.reset();
}

namespace
{
    // Runs `op` on a synchronized cin whose fd 0 is a pipe holding `input`, its write end
    // already closed so no read can block, and returns what `op` left on the fd.
    template <typename Op>
    std::string left_on_stdin(const std::string& input, Op op)
    {
        int pipefds[2];
        if (::pipe(pipefds) == -1)
            return "<pipe failed>";
        const int saved_stdin = ::dup(STDIN_FILENO);
        ::dup2(pipefds[0], STDIN_FILENO);
        ::close(pipefds[0]);
        const bool written = ::write(pipefds[1], input.data(), input.size())
                             == static_cast<ssize_t>(input.size());
        ::close(pipefds[1]);

        IOv2::cin.reset();
        IOv2::cin.sync_with_stdio(true);
        op();

        std::string rest;
        char buf[64];
        for (ssize_t n; (n = ::read(STDIN_FILENO, buf, sizeof buf)) > 0; )
            rest.append(buf, static_cast<std::size_t>(n));
        ::dup2(saved_stdin, STDIN_FILENO);
        ::close(saved_stdin);
        IOv2::cin.clear();
        IOv2::cin.reset();
        return written ? rest : "<write failed>";
    }
}

// Synchronized means each read(0) asks only for what the operation is still short of.
// get into a buffer it fills, and ignore(n, delim) once it has discarded n, used to peek
// at one character more -- off the fd and into cin's own buffer, where getchar() never
// sees it, and on a pipe that has nothing more yet, a wait for input the call does not
// need. A get with room for no character asked the device for one before failing.
TEST(IoObjectsChar, SynchronizedGetAndIgnoreReadNoFurtherThanTheyConsume)
{
    char b[8] = {};
    EXPECT_EQ(left_on_stdin("abcdef", [&] { IOv2::cin.get<IOv2::keep_sep, IOv2::app_zt>(b, 3); }),
              "cdef");
    EXPECT_EQ(std::string(b), "ab");

    EXPECT_EQ(left_on_stdin("abcdef", [&] { IOv2::cin.get<IOv2::keep_sep, IOv2::no_zt>(b, 2); }),
              "cdef");

    EXPECT_EQ(left_on_stdin("ab\ncd", [] { IOv2::cin.ignore(2, '\n'); }), "\ncd");

    bool failed = false;
    EXPECT_EQ(left_on_stdin("abc", [&] {
                  IOv2::cin.get<IOv2::keep_sep, IOv2::app_zt>(b, 1);
                  failed = IOv2::cin.str_fail();
              }),
              "abc");
    EXPECT_TRUE(failed);
}

// cout_t::init is public, and constructing one the way std::ios_base::Init is used rebuilt
// the live cout in place: the bytes it had buffered vanished, its sync_with_stdio(false)
// was reset, what it owned leaked, and on going away the extra init ran the exit hook.
// Reference-counted, it is now a no-op. Shared mode is another matter: there the extra
// init builds a separate stream in this module, which objects.h says not to do.
TEST(IoObjectsChar, AnotherCoutInitLeavesTheLiveCoutAlone)
{
#if defined(IOV2_SHARED)
    GTEST_SKIP() << "an init in a consumer module builds a separate stream";
#else
    // Checked after the guard is gone: while it redirects stdout, a failure's message
    // would land in the captured file.
    bool same_stream = false;
    bool synced_after = true;
    std::string while_buffered, at_the_end;
    {
        oguard<true> out;
        IOv2::cout.reset();
        const bool sync = IOv2::cout.sync_with_stdio(false);
        IOv2::cout << "buffered-";
        {
            IOv2::cout_t::init again;
            same_stream = &again.get() == &IOv2::cout;
        }
        synced_after = IOv2::cout.synced_with_stdio();
        while_buffered = out.contents();           // still in cout's own buffer

        IOv2::cout << "end";
        IOv2::cout.sync_with_stdio(true);
        at_the_end = out.contents();
        IOv2::cout.sync_with_stdio(sync);
    }
    EXPECT_TRUE(same_stream);
    EXPECT_FALSE(synced_after);
    EXPECT_EQ(while_buffered, "");
    EXPECT_EQ(at_the_end, "buffered-end");
#endif
}

// getline is the exception, by necessity: with its buffer full it has to look at the next
// character to know whether it is the delimiter (consumed, success) or not (overflow), and
// with room for no character a delimiter is still an empty line it consumes.
TEST(IoObjectsChar, SynchronizedGetlineLooksAtOneMoreOnlyToDecide)
{
    char b[8] = {};
    bool good = false;
    EXPECT_EQ(left_on_stdin("ab\ncd", [&] {
                  IOv2::cin.get<IOv2::cons_sep, IOv2::app_zt>(b, 3);
                  good = IOv2::cin.good();
              }),
              "cd");
    EXPECT_TRUE(good);
    EXPECT_EQ(std::string(b), "ab");

    bool failed = false;
    EXPECT_EQ(left_on_stdin("abcd\n", [&] {
                  IOv2::cin.get<IOv2::cons_sep, IOv2::app_zt>(b, 3);
                  failed = IOv2::cin.str_fail();
              }),
              "d\n");
    EXPECT_TRUE(failed);

    EXPECT_EQ(left_on_stdin("\nx", [&] {
                  IOv2::cin.get<IOv2::cons_sep, IOv2::app_zt>(b, 1);
                  good = IOv2::cin.good();
              }),
              "x");
    EXPECT_TRUE(good);
}

// tie()'s cycle check walks through a standard stream too. os and cout have different
// instantiations of basic_stream_common_operators as their base, so a walk that cross-cast
// each node to one of them stopped at the other kind and accepted os -> cout -> os.
TEST(IoObjectsChar, ATieCycleThroughAStandardStreamIsRejected)
{
    IOv2::ostream os(IOv2::mem_device{""}, IOv2::locale<char>("C"));
    os.tie(&IOv2::cout);

    ASSERT_EQ(IOv2::cout.tie(), nullptr);
    IOv2::cout.tie(&os);
    EXPECT_TRUE(IOv2::cout.rdstate() & IOv2::ios_defs::strfailbit);
    EXPECT_EQ(IOv2::cout.tie(), nullptr);         // nothing committed

    IOv2::cout.clear();
    os.tie(nullptr);
}

// The other shapes the round-33 review probed through tie_target::tied_to(), each mixing the
// two instantiations of basic_stream_common_operators: a three-cycle, a five-cycle through an
// iostream, a self-tie, and a cycle across character types. Each attempt that would close a
// cycle is refused and leaves the edge as it was; the edges on the way are accepted.
TEST(IoObjectsChar, EveryTieCycleShapeThroughStandardStreamsIsRejected)
{
    const auto refused = [](auto& s, IOv2::tie_target* target) {
        IOv2::tie_target* const before = s.tie();
        s.tie(target);
        const bool ok = (s.rdstate() & IOv2::ios_defs::strfailbit) && s.tie() == before;
        s.clear();
        return ok;
    };

    IOv2::ostream os(IOv2::mem_device{""}, IOv2::locale<char>("C"));
    IOv2::iostream io(IOv2::mem_device{""}, IOv2::locale<char>("C"));
    IOv2::ostream wos(IOv2::mem_device{L""}, IOv2::locale<wchar_t>("C"));
    ASSERT_EQ(IOv2::cerr.tie(), &IOv2::cout);      // tied at construction

    os.tie(&IOv2::cerr);                           // cout -> os -> cerr -> cout
    EXPECT_TRUE(refused(IOv2::cout, &os));

    io.tie(&os);                                   // cout -> clog -> io -> os -> cerr -> cout
    IOv2::clog.tie(&io);
    EXPECT_TRUE(IOv2::clog.good());
    EXPECT_EQ(IOv2::clog.tie(), &io);
    EXPECT_TRUE(refused(IOv2::cout, &IOv2::clog));

    EXPECT_TRUE(refused(IOv2::cerr, &IOv2::cerr)); // the length-1 cycle

    wos.tie(&IOv2::cout);                          // cout -> wos -> cout, across char types
    EXPECT_TRUE(refused(IOv2::cout, &wos));

    EXPECT_EQ(IOv2::cout.tie(), nullptr);
    IOv2::clog.tie(nullptr);
    io.tie(nullptr);
    os.tie(nullptr);
    wos.tie(nullptr);
}

namespace
{
    // Re-runs `test` in a child whose fd 0 or fd 1 is broken before exec, so that the stream
    // objects are constructed over it; `mode` names the breakage and reaches the child as
    // test_child_arg(). Returns the child's exit status, or -1 if it did not exit.
    int run_with_broken_fd(const std::string& test, const std::string& mode)
    {
        const std::string filter = "--gtest_filter=" + test;
        const std::string executable = exe_path();
        const pid_t child = ::fork();
        if (child == -1)
            return -1;
        if (child == 0)
        {
            if (mode == "stdin-closed")
                ::close(STDIN_FILENO);
            else if (mode == "stdin-directory")
                ::dup2(::open(".", O_RDONLY), STDIN_FILENO);
            else if (mode == "stdin-write-only")
                ::dup2(::open("/dev/null", O_WRONLY), STDIN_FILENO);
            else if (mode == "stdout-closed")
                ::close(STDOUT_FILENO);
            else if (mode == "stdout-read-only")
                ::dup2(::open("/dev/null", O_RDONLY), STDOUT_FILENO);
            ::setenv(test_child_env, mode.c_str(), 1);
            ::execl(executable.c_str(), executable.c_str(), filter.c_str(), "--gtest_color=no",
                    static_cast<char*>(nullptr));
            ::_exit(127);
        }
        int status = 0;
        if (::waitpid(child, &status, 0) != child || !WIFEXITED(status))
            return -1;
        return WEXITSTATUS(status);
    }
}

// fd 0 or fd 1 broken when the process starts: closed, a directory, or open in the wrong
// direction. Constructing the stream objects over it must neither abort nor hang, and the
// first operation reports the device failure through devfailbit. Only a review probe had
// run these.
TEST(IoObjectsChar, AStandardStreamOverABrokenFdFailsItsFirstOperation)
{
    if (in_test_child())
    {
        ::alarm(10);                               // a hang fails the case instead of the run
        const std::string mode = test_child_arg();
        bool dev_failed = false;
        if (mode.starts_with("stdin"))
        {
            char c = 0;
            IOv2::cin.get(c);
            dev_failed = IOv2::cin.dev_fail();
        }
        else
        {
            IOv2::cout << 'x';
            IOv2::cout.flush();
            dev_failed = IOv2::cout.dev_fail();
        }
        std::_Exit(dev_failed ? 0 : 1);
    }

    for (const char* mode : {"stdin-closed", "stdin-directory", "stdin-write-only",
                             "stdout-closed", "stdout-read-only"})
        EXPECT_EQ(run_with_broken_fd("IoObjectsChar.AStandardStreamOverABrokenFdFailsItsFirstOperation", mode), 0)
            << mode;
}

// Asking an input stream for the mode it is already in has nothing to switch, so it
// must not wait for the stream's lock -- which a read blocked in another thread holds
// until input arrives. The free function asks cin and wcin the same.
TEST(IoObjectsChar, SyncWithStdioToTheCurrentModeDoesNotWaitForABlockedRead)
{
    pipe_iguard g("");
    IOv2::cin.reset();
    ASSERT_TRUE(IOv2::cin.synced_with_stdio());

    std::thread reader([] { EXPECT_EQ(IOv2::cin.get(), 'x'); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));   // let it block in read()

    std::atomic<bool> fed{false};
    std::thread late_writer([&]
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        fed.store(true);
        g.feed("x");
    });

    EXPECT_TRUE(IOv2::cin.sync_with_stdio(true));
    IOv2::sync_with_stdio(true);
    EXPECT_FALSE(fed.load()) << "it waited for the blocked read";

    late_writer.join();
    reader.join();
    IOv2::cin.reset();
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

// A multithreaded program forks while another thread is in the middle of writing to cerr,
// and the child reports something on cerr before exec -- say that exec failed. fork() takes
// only the calling thread along, so the child used to block forever on cerr's lock, which
// no thread in it would ever release. The child is a bare fork (that is what is under
// test); only whether its line came back in time is checked. It ends in an exec of
// /bin/true, not _exit: on _exit valgrind reports as leaked what the other thread owned at
// the fork, an exec skips that check, and an access error in the child is reported when it
// happens.
TEST(IoObjectsChar, AChildForkedWhileAnotherThreadHoldsCerrCanStillWriteToIt)
{
    std::atomic<bool> held{false}, release{false};
    std::thread holder([&] {
        IOv2::sync g(IOv2::cerr);
        held = true;
        while (!release)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    while (!held)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const pid_t pid = ::fork();
    if (pid == 0)
    {
        ::close(fds[0]);
        ::dup2(fds[1], STDERR_FILENO);
        IOv2::cerr << "child\n";
        ::execl("/bin/true", "true", static_cast<char*>(nullptr));
        ::_exit(127);
    }
    ::close(fds[1]);

    std::string got;
    char buf[64];
    pollfd p{fds[0], POLLIN, 0};
    while (::poll(&p, 1, 5000) == 1)
    {
        const ssize_t n = ::read(fds[0], buf, sizeof buf);
        if (n <= 0)
            break;
        got.append(buf, static_cast<std::size_t>(n));
    }
    if (got.empty())
        ::kill(pid, SIGKILL);
    ::waitpid(pid, nullptr, 0);
    ::close(fds[0]);
    release = true;
    holder.join();

    EXPECT_EQ(got, "child\n");
}

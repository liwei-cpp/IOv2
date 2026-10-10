// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The wide standard stream objects: wcout, wcerr, wclog and wcin.
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
 *
 * A wide standard stream carries one thing the narrow ones do not: the encoding
 * it converts through, which switch_code() changes in place. The last two tests
 * change it mid-stream in both directions and check the bytes, since that is
 * the only place the encoding is observable.
 *
 * The encoding cannot be a state-dependent one: wcerr and wclog share one fd,
 * and each would keep its own shift state, so switch_code() refuses it.
 */
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/stdin_root_cvt.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <support/stateful_locale.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <atomic>
#include <clocale>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

TEST(IoObjectsWchar, EachStreamWritesToItsOwnDestination)
{
    {
        oguard<true> out;
        IOv2::wcout << L"to stdout" << IOv2::endl;
        EXPECT_EQ(out.contents(), "to stdout\n");
    }
    {
        oguard<false> err;
        IOv2::wcerr << L"to stderr" << IOv2::endl;
        EXPECT_EQ(err.contents(), "to stderr\n");
    }
    {
        oguard<false> err;
        IOv2::wclog << L"also stderr" << IOv2::endl;
        EXPECT_EQ(err.contents(), "also stderr\n");
    }
}

// The two destinations do not bleed into one another, in either order.
TEST(IoObjectsWchar, StdoutAndStderrStaySeparate)
{
    oguard<true>  out;
    oguard<false> err;

    IOv2::wcout << L"first ";
    IOv2::wcout.flush();
    IOv2::wcerr << L"second";
    IOv2::wcerr.flush();
    IOv2::wcout << L"third" << IOv2::endl;
    IOv2::wcout.flush();

    EXPECT_EQ(out.contents(), "first third\n");
    EXPECT_EQ(err.contents(), "second");
}

TEST(IoObjectsWchar, CerrIsUnitBufferedAndBothInputsAreTiedToCout)
{
    EXPECT_TRUE(IOv2::wcerr.flags() & IOv2::ios_defs::unitbuf);
    EXPECT_EQ(IOv2::wcerr.tie(), &IOv2::wcout);
    EXPECT_EQ(IOv2::wcin.tie(), &IOv2::wcout);

    // The flags a fresh stream starts with.
    EXPECT_TRUE(IOv2::wcerr.flags() & IOv2::ios_defs::dec);
    EXPECT_TRUE(IOv2::wcerr.flags() & IOv2::ios_defs::skipws);
}

// What the tie is for: an unterminated prompt is still sitting in cout's buffer
// when the read starts, and the read is what pushes it out.
TEST(IoObjectsWchar, ReadingFromCinFlushesThePromptOnCout)
{
    IOv2::wcout.reset();
    IOv2::wcin.reset();
    IOv2::wcout.sync_with_stdio(false);

    oguard<true> out;
    iguard       in("Ada");

    IOv2::wcout << L"ready" << IOv2::endl;
    IOv2::wcout << L"name? ";                 // no newline, no flush
    EXPECT_EQ(out.contents(), "ready\n");   // so it has not gone out yet

    std::wstring answer;
    IOv2::wcin >> answer;
    EXPECT_EQ(answer, L"Ada");
    EXPECT_EQ(out.contents(), "ready\nname? ");
}

// Each block re-points stdin, so cin is reset with it: these are global objects
// and a previous test's end-of-input would otherwise still be on them.
TEST(IoObjectsWchar, CinReadsAndPutsBack)
{
    {
        iguard g("alpha beta");
        IOv2::wcin.reset();
        wchar_t first = 0;
        wchar_t again = 1;

        IOv2::wcin.get(first);
        IOv2::wcin.putback(first);
        IOv2::wcin.get(again);

        EXPECT_TRUE(IOv2::wcin.good());
        EXPECT_EQ(first, again);
    }
    {
        iguard g("alpha beta");
        IOv2::wcin.reset();
        wchar_t buf[2];
        // Hoisted: the template argument list would look like a second macro
        // argument to the preprocessor.
        wchar_t* end = IOv2::wcin.get<IOv2::keep_sep, IOv2::no_zt>(buf, 2);
        EXPECT_EQ(end - buf, 2);

        IOv2::wcin.putback(buf[1]);
        EXPECT_EQ(IOv2::wcin.get(), buf[1]);
    }
    {
        iguard g("\n");
        IOv2::wcin.reset();
        EXPECT_TRUE(static_cast<bool>(IOv2::wcin.ignore(1)));
    }
}

// sync_with_stdio changes how the objects reach the C streams, not which
// objects they are.
TEST(IoObjectsWchar, SyncWithStdioDoesNotReplaceTheObjects)
{
    const void* before[] = {&IOv2::wcout, &IOv2::wcin, &IOv2::wcerr, &IOv2::wclog};

    IOv2::sync_with_stdio(false);

    const void* after[] = {&IOv2::wcout, &IOv2::wcin, &IOv2::wcerr, &IOv2::wclog};

    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_EQ(before[i], after[i]) << "object " << i;
}

// switch_code() changes the encoding the stream converts through, in place and
// mid-stream. The bytes on the far side are the only place that is visible, so
// the same two words are read once as UTF-8 and once as GBK.
TEST(IoObjectsWchar, TheInputEncodingCanBeSwitchedMidStream)
{
    iguard g("\xe8\xaf\xb7 \xd0\xbb\xd0\xbb");

    // use attach to refresh the whole buffer.
    IOv2::wcin.reset();

    std::wstring first;
    std::wstring second;

    IOv2::wcin.switch_code("zh_CN.UTF-8");
    IOv2::wcin >> first;
    EXPECT_EQ(IOv2::wcin.code(), "zh_CN.UTF-8");

    IOv2::wcin.switch_code("zh_CN.GBK");
    IOv2::wcin >> second;
    EXPECT_EQ(IOv2::wcin.code(), "zh_CN.GBK");

    EXPECT_EQ(first, L"\u8bf7");
    EXPECT_EQ(second, L"\u8c22\u8c22");
}

// Switching wcin to synchronized rebuilds nothing: the bytes the unsynchronized stream
// had read ahead -- a multibyte character among them -- still decode afterwards.
TEST(IoObjectsWchar, SwitchingToSynchronizedKeepsTheInputReadAhead)
{
    iguard g("\xe8\xaf\xb7 \xe8\xb0\xa2\xe8\xb0\xa2 ok");
    IOv2::wcin.reset();
    IOv2::wcin.switch_code("zh_CN.UTF-8");
    IOv2::wcin.sync_with_stdio(false);

    std::wstring first, second, third;
    IOv2::wcin >> first;                               // pulls the whole input into the buffer
    EXPECT_EQ(first, L"\u8bf7");

    EXPECT_FALSE(IOv2::wcin.sync_with_stdio(true));
    IOv2::wcin >> second >> third;
    EXPECT_EQ(second, L"\u8c22\u8c22");
    EXPECT_EQ(third, L"ok");
    EXPECT_TRUE(IOv2::wcin.eof());

    IOv2::wcin.reset();
}

// adjust(stdin_sync{...}) on wcin is sync_with_stdio, as on cin: the stream's own query
// follows, and switching back is a real switch that leaves the rest on the fd.
TEST(IoObjectsWchar, AdjustingWithStdinSyncIsSyncWithStdio)
{
    int pipefds[2];
    ASSERT_NE(::pipe(pipefds), -1);
    const int saved_stdin = ::dup(STDIN_FILENO);
    ::dup2(pipefds[0], STDIN_FILENO);

    EXPECT_EQ(::write(pipefds[1], "12345X", 6), 6);
    ::close(pipefds[1]);                           // no more input: read() cannot block

    IOv2::wcin.reset();
    IOv2::wcin.adjust(IOv2::stdin_sync{false});
    EXPECT_FALSE(IOv2::wcin.synced_with_stdio());
    EXPECT_FALSE(IOv2::wcin.sync_with_stdio(true)); // a real switch back

    wchar_t buf[5] = {};
    IOv2::wcin.read(buf, 5);
    EXPECT_EQ(std::wstring(buf, 5), L"12345");

    char rest = 0;
    EXPECT_EQ(::read(STDIN_FILENO, &rest, 1), 1);   // synchronized: left on the fd
    EXPECT_EQ(rest, 'X');

    ::dup2(saved_stdin, STDIN_FILENO);
    ::close(saved_stdin);
    ::close(pipefds[0]);
    IOv2::wcin.reset();
}

namespace
{
    // Runs `op` on a synchronized wcin whose fd 0 is a pipe holding `input`, its write end
    // already closed so no read can block, and returns what `op` left on the fd. ASCII
    // input only: one byte per character under every locale the suite runs in.
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

        IOv2::wcin.reset();
        IOv2::wcin.sync_with_stdio(true);
        op();

        std::string rest;
        char buf[64];
        for (ssize_t n; (n = ::read(STDIN_FILENO, buf, sizeof buf)) > 0; )
            rest.append(buf, static_cast<std::size_t>(n));
        ::dup2(saved_stdin, STDIN_FILENO);
        ::close(saved_stdin);
        IOv2::wcin.clear();
        IOv2::wcin.reset();
        return written ? rest : "<write failed>";
    }
}

// The wide side of IoObjectsChar.SynchronizedGetAndIgnoreReadNoFurtherThanTheyConsume:
// wcin goes through code_cvt_stdio, and its get / ignore peeked one character more too.
TEST(IoObjectsWchar, SynchronizedGetAndIgnoreReadNoFurtherThanTheyConsume)
{
    wchar_t b[8] = {};
    EXPECT_EQ(left_on_stdin("abcdef", [&] { IOv2::wcin.get<IOv2::keep_sep, IOv2::app_zt>(b, 3); }),
              "cdef");
    EXPECT_EQ(std::wstring(b), L"ab");

    EXPECT_EQ(left_on_stdin("ab\ncd", [] { IOv2::wcin.ignore(1, L'\n'); }), "b\ncd");
}

namespace
{
    bool g_fail_attach = false;

    // A root whose attach fails on demand. With it a reset() can taint the converter, and the
    // recovery sync_with_stdio() starts with can fail -- neither can happen to the real wcin,
    // whose pipeline has no step that fails there.
    template <typename TDevice>
    struct failing_root : IOv2::stdin_root_cvt<TDevice>
    {
        using BT = IOv2::stdin_root_cvt<TDevice>;
        explicit failing_root(BT&& root) : BT(std::move(root)) {}
        void attach(TDevice&& dev = TDevice{})
        {
            if (g_fail_attach)
                throw std::runtime_error("failing_root: attach");
            BT::attach(std::move(dev));
        }
    };

    struct failing_creator
    {
        using category = IOv2::CvtCreatorCategory;
        template <typename TKernel>
        auto create(TKernel&& kernel) const
        {
            using device = typename std::remove_cvref_t<TKernel>::device_type;
            return IOv2::code_cvt_stdio{failing_root<device>{std::forward<TKernel>(kernel)},
                                        std::string("C")};
        }
    };

    class failing_wcin : public IOv2::stdin_api<failing_wcin, IOv2::std_device<STDIN_FILENO>, wchar_t>
    {
    public:
        failing_wcin() : stdin_api(failing_creator{}) {}
    };
}

// The one failure sync_with_stdio() can have: on a wide stream whose converter is tainted, the
// recovery it starts with. The flag is then untouched and nothing switches -- the query, the
// return value and how the root reads all keep the old mode. Only a custom pipeline gets here,
// so until now only a review probe had run this branch.
TEST(IoObjectsWchar, ASwitchWhoseRecoveryFailsKeepsTheMode)
{
    int pipefds[2];
    ASSERT_NE(::pipe(pipefds), -1);
    const int saved_stdin = ::dup(STDIN_FILENO);
    ::dup2(pipefds[0], STDIN_FILENO);
    ::close(pipefds[0]);
    EXPECT_EQ(::write(pipefds[1], "abcdef", 6), 6);
    ::close(pipefds[1]);                           // no more input: read() cannot block

    failing_wcin in;
    g_fail_attach = true;
    in.reset();                                    // the attach fails: the converter is tainted
    in.clear();
    const bool before = in.sync_with_stdio(false); // its recovery fails as well
    const bool failed = !in.good();
    const bool still_synced = in.synced_with_stdio();
    g_fail_attach = false;
    in.clear();

    wchar_t c = 0;
    in.get(c);                                     // recovers now, and reads synchronized
    std::string rest;
    char buf[16];
    for (ssize_t n; (n = ::read(STDIN_FILENO, buf, sizeof buf)) > 0; )
        rest.append(buf, static_cast<std::size_t>(n));
    ::dup2(saved_stdin, STDIN_FILENO);
    ::close(saved_stdin);

    EXPECT_TRUE(before);
    EXPECT_TRUE(failed);
    EXPECT_TRUE(still_synced);
    EXPECT_EQ(c, L'a');
    EXPECT_EQ(rest, "bcdef");                      // one byte taken: the root never switched
}

TEST(IoObjectsWchar, TheOutputEncodingCanBeSwitchedMidStream)
{
    oguard<true> out;

    IOv2::wcout.switch_code("zh_CN.UTF-8");
    IOv2::wcout << L"\u8bf7 ";
    EXPECT_EQ(IOv2::wcout.code(), "zh_CN.UTF-8");

    IOv2::wcout.switch_code("zh_CN.GBK");
    IOv2::wcout << L"\u8c22\u8c22" << IOv2::flush;
    EXPECT_EQ(IOv2::wcout.code(), "zh_CN.GBK");

    EXPECT_EQ(out.contents(), "\xe8\xaf\xb7 \xd0\xbb\xd0\xbb");
}

// What a user does after an encoding error is what std::wcout users do: look at
// the state bits, clear() them, carry on. The converter was tainted by the
// unencodable character and reattaches the same fd by itself; nothing before
// the bad character is lost and nothing after it needs reset().
TEST(IoObjectsWchar, WcoutRecoversFromAnUnencodableCharacterWithClear)
{
    oguard<true> out;
    IOv2::wcout.reset();
    IOv2::wcout.switch_code("zh_CN.UTF-8");

    IOv2::wcout << L"a" << L'\xD800';
    EXPECT_TRUE(IOv2::wcout.cvt_fail());

    IOv2::wcout << L"x";   // refused while the stream is failed
    IOv2::wcout.clear();
    IOv2::wcout << L"b" << IOv2::flush;
    EXPECT_TRUE(IOv2::wcout.good());

    EXPECT_EQ(out.contents(), "ab");
}

// Switching encoding is the natural reaction to an encoding failure; it must be
// accepted right after one and take effect.
TEST(IoObjectsWchar, WcoutCanSwitchEncodingRightAfterAnUnencodableCharacter)
{
    oguard<true> out;
    IOv2::wcout.reset();
    IOv2::wcout.switch_code("zh_CN.UTF-8");

    IOv2::wcout << L'\xD800';
    EXPECT_TRUE(IOv2::wcout.cvt_fail());
    IOv2::wcout.clear();

    IOv2::wcout.switch_code("zh_CN.GBK");
    EXPECT_TRUE(IOv2::wcout.good());
    EXPECT_EQ(IOv2::wcout.code(), "zh_CN.GBK");
    IOv2::wcout << L"中" << IOv2::flush;
    EXPECT_TRUE(IOv2::wcout.good());
    EXPECT_EQ(out.contents(), "\xd6\xd0"); // 中 in GBK

    IOv2::wcout.switch_code("zh_CN.UTF-8");
}

// switch_code() is adjust(code_cvt_switch) with the stream's usual error
// handling: a name newlocale() rejects sets cvtfailbit, leaves the encoding
// alone, and throws only when the exception mask asks for it.
TEST(IoObjectsWchar, SwitchCodeReportsARejectedNameThroughTheStateBits)
{
    oguard<true> out;
    IOv2::wcout.reset();
    IOv2::wcout.switch_code("zh_CN.UTF-8");

    EXPECT_EQ(IOv2::wcout.switch_code("xx_YY.NOPE"), "zh_CN.UTF-8");
    EXPECT_EQ(IOv2::wcout.rdstate(), IOv2::ios_defs::cvtfailbit);
    EXPECT_EQ(IOv2::wcout.code(), "zh_CN.UTF-8");

    IOv2::wcout.clear();
    IOv2::wcout.exceptions(IOv2::ios_defs::cvtfailbit);
    EXPECT_THROW(IOv2::wcout.switch_code("xx_YY.NOPE"), IOv2::cvt_error);
    IOv2::wcout.exceptions(IOv2::ios_defs::goodbit);
    IOv2::wcout.clear();

    EXPECT_EQ(IOv2::wcout.code(), "zh_CN.UTF-8");
    IOv2::wcout << L"中" << IOv2::flush;
    EXPECT_TRUE(IOv2::wcout.good());
    EXPECT_EQ(out.contents(), "\xe4\xb8\xad");
}

// A std::string can carry a NUL. Read as a C string a leading one is "", which
// newlocale() takes as "look at the environment": the caller asked for GBK and
// would get whatever LC_ALL says, with good() still true. The name is rejected
// at its full length instead, before anything is switched.
TEST(IoObjectsWchar, SwitchCodeRejectsANameWithAnEmbeddedNul)
{
    oguard<true> out;
    IOv2::wcout.reset();
    IOv2::wcout.switch_code("zh_CN.UTF-8");

    EXPECT_EQ(IOv2::wcout.switch_code(std::string("\0zh_CN.GBK", 10)), "zh_CN.UTF-8");
    EXPECT_EQ(IOv2::wcout.rdstate(), IOv2::ios_defs::cvtfailbit);
    EXPECT_EQ(IOv2::wcout.code(), "zh_CN.UTF-8");

    IOv2::wcout.clear();
    IOv2::wcout << L"中" << IOv2::flush;
    EXPECT_TRUE(IOv2::wcout.good());
    EXPECT_EQ(out.contents(), "\xe4\xb8\xad");
}

namespace
{
    // The wide counterpart of IoObjectsChar.ResetReportsAFlushFailureFromAFullBufferedStdout.
    // A wide stream's detach() runs one converter layer deeper than a narrow
    // one's, and that layer hands the device through a temporary on its way
    // out. If a moved-from std_device still flushed in its destructor, that
    // temporary would take the one fflush that can fail, swallow the error, and
    // leave reset() with nothing to report -- in either synchronization mode.
    void reset_reports_a_full_buffered_stdout_failure(bool sync_mode)
    {
        const int full = ::open("/dev/full", O_WRONLY);
        if (full == -1) GTEST_SKIP() << "no /dev/full here";

        oguard<true> out;
        IOv2::wcout.reset();
        const bool sync = IOv2::wcout.sync_with_stdio(sync_mode);

        {
            stdout_full_buffer buffered;            // restores unbuffered stdout on any exit

            IOv2::wcout << L"DROPPED";              // sync: in stdout's FILE buffer; unsync: still in the stream's
            EXPECT_TRUE(out.contents().empty());

            const int saved = ::dup(STDOUT_FILENO);
            ASSERT_NE(saved, -1);
            EXPECT_EQ(::dup2(full, STDOUT_FILENO), STDOUT_FILENO);
            EXPECT_NO_THROW(IOv2::wcout.reset());
            EXPECT_EQ(::dup2(saved, STDOUT_FILENO), STDOUT_FILENO);
            ::close(saved);
            ::close(full);
        }

        EXPECT_EQ(IOv2::wcout.rdstate(), IOv2::ios_defs::devfailbit);

        IOv2::wcout.clear();
        IOv2::wcout << L"AFTER" << IOv2::flush;

        EXPECT_TRUE(IOv2::wcout.good());
        EXPECT_NE(out.contents().find("AFTER"), std::string::npos);

        IOv2::wcout.sync_with_stdio(sync);
    }
}

TEST(IoObjectsWchar, ResetReportsAFlushFailureFromAFullBufferedStdout)
{
    reset_reports_a_full_buffered_stdout_failure(true);
}

TEST(IoObjectsWchar, ResetReportsAFlushFailureFromAFullBufferedStdoutWhenUnsynchronized)
{
    reset_reports_a_full_buffered_stdout_failure(false);
}

// The same path on fd 2. stderr is unbuffered by default, so this is the shape a
// caller's own setvbuf(stderr, ..., _IOFBF, ...) creates; the device behind wclog is
// std_device<2>, whose moved-from copies have to stay as quiet as std_device<1>'s.
// wclog rather than wcerr: wcerr is unit-buffered, so every insertion already
// pushes its bytes through fflush while the fd is still healthy, and there is
// never anything left for reset() to lose.
TEST(IoObjectsWchar, ResetReportsAFlushFailureFromAFullBufferedStderr)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<false> err;
    IOv2::wclog.reset();
    const bool sync = IOv2::wclog.sync_with_stdio(true);

    {
        stderr_full_buffer buffered;            // restores unbuffered stderr on any exit

        IOv2::wclog << L"DROPPED";              // moved into stderr's FILE buffer
        EXPECT_TRUE(err.contents().empty());

        const int saved = ::dup(STDERR_FILENO);
        ASSERT_NE(saved, -1);
        EXPECT_EQ(::dup2(full, STDERR_FILENO), STDERR_FILENO);
        EXPECT_NO_THROW(IOv2::wclog.reset());
        EXPECT_EQ(::dup2(saved, STDERR_FILENO), STDERR_FILENO);
        ::close(saved);
        ::close(full);
    }

    EXPECT_EQ(IOv2::wclog.rdstate(), IOv2::ios_defs::devfailbit);

    IOv2::wclog.clear();
    IOv2::wclog << L"AFTER" << IOv2::flush;

    EXPECT_TRUE(IOv2::wclog.good());
    EXPECT_NE(err.contents().find("AFTER"), std::string::npos);

    IOv2::wclog.sync_with_stdio(sync);
}

// A tainted converter recovers before switch_code() commits the new encoding.
// If already-committed bytes are waiting in stdout's FILE buffer, that recovery
// must flush the old device and report an fflush failure -- as devfailbit, like
// every other operation on the stream -- instead of silently switching encodings
// while the bytes disappear.
TEST(IoObjectsWchar, SwitchCodeReportsAFullBufferedStdoutFailureBeforeSwitching)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<true> out;
    IOv2::wcout.reset();
    IOv2::wcout.switch_code("zh_CN.UTF-8");
    const bool sync = IOv2::wcout.sync_with_stdio(true);

    {
        stdout_full_buffer buffered;                // restores unbuffered stdout on any exit

        IOv2::wcout << L"PENDING";                  // committed into stdout's FILE buffer
        EXPECT_TRUE(out.contents().empty());

        // Keep the failing insertion's sentry from immediately auto-recovering the
        // tainted converter while stdout still points at the healthy file.
        IOv2::wcout.sync_with_stdio(false);
        IOv2::wcout << L'\xD800';                   // invalid Unicode scalar: taints code_cvt
        ASSERT_TRUE(IOv2::wcout.cvt_fail());
        IOv2::wcout.clear();

        const int saved = ::dup(STDOUT_FILENO);
        ASSERT_NE(saved, -1);
        EXPECT_EQ(::dup2(full, STDOUT_FILENO), STDOUT_FILENO);
        EXPECT_NO_THROW(IOv2::wcout.switch_code("zh_CN.GBK"));
        EXPECT_EQ(IOv2::wcout.rdstate(), IOv2::ios_defs::devfailbit);
        EXPECT_EQ(::dup2(saved, STDOUT_FILENO), STDOUT_FILENO);
        ::close(saved);
        ::close(full);
    }

    EXPECT_EQ(IOv2::wcout.code(), "zh_CN.UTF-8");

    // The failed recovery leaves the converter tainted; the next insertion
    // retries recovery on the restored fd and remains usable.
    IOv2::wcout.clear();
    IOv2::wcout << L"AFTER" << IOv2::flush;
    EXPECT_TRUE(IOv2::wcout.good());
    EXPECT_NE(out.contents().find("AFTER"), std::string::npos);

    IOv2::wcout.sync_with_stdio(sync);
}

// GBK: 璇 | b7 20 (a lead byte the space cannot complete) | 谢谢 | newline | 42.
// The decoder must not pair the stale lead byte with the bytes that follow:
// after clear() the next token is 谢谢, not 沸恍, and the number after it reads.
TEST(IoObjectsWchar, WcinStaysAlignedAfterAnInvalidSequence)
{
    iguard g("\xe8\xaf\xb7\x20\xd0\xbb\xd0\xbb\x0a" "42\n");
    IOv2::wcin.reset();
    IOv2::wcin.switch_code("zh_CN.GBK");

    std::wstring first;
    std::wstring second;
    int          n = -1;

    IOv2::wcin >> first;
    EXPECT_EQ(first, L"璇");
    EXPECT_TRUE(IOv2::wcin.cvt_fail());

    IOv2::wcin.clear();
    IOv2::wcin >> second;
    IOv2::wcin >> n;
    EXPECT_EQ(second, L"谢谢");
    EXPECT_EQ(n, 42);
    EXPECT_TRUE(IOv2::wcin.good());

    IOv2::wcin.switch_code("zh_CN.UTF-8");
}

// ... and switching encoding after the error is accepted too: the decoder's state
// is back to initial, so there is no half character to reinterpret.
TEST(IoObjectsWchar, WcinCanSwitchEncodingRightAfterAnInvalidSequence)
{
    iguard g("\xb7\x20" "\xe8\xaf\xb7\n");
    IOv2::wcin.reset();
    IOv2::wcin.switch_code("zh_CN.GBK");

    std::wstring w;
    IOv2::wcin >> w;
    EXPECT_TRUE(IOv2::wcin.cvt_fail());
    IOv2::wcin.clear();

    IOv2::wcin.switch_code("zh_CN.UTF-8");
    EXPECT_TRUE(IOv2::wcin.good());
    EXPECT_EQ(IOv2::wcin.code(), "zh_CN.UTF-8");
    IOv2::wcin >> w;
    EXPECT_EQ(w, L"请");
    EXPECT_TRUE(IOv2::wcin.good());
}

// "" is not an encoding name, it is a request to look at the environment, and the
// lookup happens once -- at the call. What the stream reports afterwards is the
// concrete locale that was found, so a later change to the environment cannot move
// the stream's encoding under it. Were "" kept verbatim instead, the rebuild inside
// sync_with_stdio() would resolve it again against a hostile LC_ALL and abort.
TEST(IoObjectsWchar, SwitchCodeResolvesTheEmptyNameAtTheCallAndKeepsTheConcreteOne)
{
    // The CI runner sets LC_ALL for the whole suite; put it back the way it was.
    const char* const saved = std::getenv("LC_ALL");
    const std::string previous = saved ? saved : "";

    ::setenv("LC_ALL", "zh_CN.GBK", 1);
    IOv2::wcin.switch_code("");
    EXPECT_EQ(IOv2::wcin.code(), "zh_CN.GBK");

    ::setenv("LC_ALL", "xx_YY.NOPE", 1);
    const bool sync = IOv2::wcin.sync_with_stdio(false);
    EXPECT_EQ(IOv2::wcin.code(), "zh_CN.GBK");
    IOv2::wcin.sync_with_stdio(sync);

    if (saved) ::setenv("LC_ALL", previous.c_str(), 1);
    else       ::unsetenv("LC_ALL");
    IOv2::wcin.switch_code("zh_CN.UTF-8");
}

// The wide streams take no state-dependent encoding: switch_code() to one sets
// cvtfailbit and keeps the encoding the stream had.
// The wide streams take no state-dependent encoding from the environment either:
// started under an ISO-2022-JP LC_ALL, all four fall back to "C".
TEST(IoObjectsWchar, WideStreamsFallBackToCUnderAStateDependentEnvironment)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("IoObjectsWchar.WideStreamsFallBackToCUnderAStateDependentEnvironment",
                                     stateful_locale_name), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    ASSERT_STREQ(std::getenv("LC_ALL"), stateful_locale_name);
    EXPECT_EQ(IOv2::wcin.code(), "C");
    EXPECT_EQ(IOv2::wcout.code(), "C");
    EXPECT_EQ(IOv2::wcerr.code(), "C");
    EXPECT_EQ(IOv2::wclog.code(), "C");
}

// Started with an LC_ALL that names no locale, the wide streams fall back to "C"
// leniently, but switch_code("") asks newlocale() and fails; initial_locale_name()
// gives back what startup settled on.
TEST(IoObjectsWchar, SwitchCodeReturnsToTheStartupEncodingThroughInitialLocaleName)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("IoObjectsWchar.SwitchCodeReturnsToTheStartupEncodingThroughInitialLocaleName",
                                     "xx_YY.no-such-locale"), 0)
            << "the checks under the bogus locale failed; see the child's output above";
        return;
    }

    // --- child ---
    EXPECT_EQ(IOv2::wcout.code(), "C");
    EXPECT_EQ(IOv2::initial_locale_name(LC_CTYPE), "C");

    IOv2::wcout.switch_code("");
    EXPECT_TRUE(IOv2::wcout.cvt_fail());
    EXPECT_EQ(IOv2::wcout.code(), "C");
    IOv2::wcout.clear();

    IOv2::wcout.switch_code(IOv2::initial_locale_name(LC_CTYPE));
    EXPECT_TRUE(IOv2::wcout.good());
    EXPECT_EQ(IOv2::wcout.code(), "C");
}

// Two threads switch the same stream from "C" to two other encodings at once: whichever
// goes second returns the first one's target. The query and the switch used to take the
// lock apart, so both could read "C" and both return it -- about one round in four.
TEST(IoObjectsWchar, ConcurrentSwitchCodesEachReturnTheEncodingBeforeThem)
{
    const std::string base = "C", x = "zh_CN.UTF-8", y = "zh_CN.GBK";
    const auto check = [&](auto& stream)
    {
        const std::string startup = stream.code();
        int bad = 0;
        for (int i = 0; i < 300; ++i)
        {
            stream.switch_code(base);
            std::atomic<int> ready{0};
            std::string rx, ry;
            std::thread tx([&] { ++ready; while (ready.load() < 2) {} rx = stream.switch_code(x); });
            std::thread ty([&] { ++ready; while (ready.load() < 2) {} ry = stream.switch_code(y); });
            tx.join();
            ty.join();
            if (!((rx == base && ry == x) || (ry == base && rx == y)))
                ++bad;
        }
        EXPECT_EQ(bad, 0);
        EXPECT_TRUE(stream.good());
        stream.switch_code(startup);
    };

    {
        SCOPED_TRACE("wcout");
        check(IOv2::wcout);
    }
    {
        SCOPED_TRACE("wcin");
        check(IOv2::wcin);
    }
}

TEST(IoObjectsWchar, SwitchCodeRefusesAStateDependentEncoding)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("IoObjectsWchar.SwitchCodeRefusesAStateDependentEncoding"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    auto refused = [](auto& s)
    {
        const std::string before = s.code();
        s.switch_code(stateful_locale_name);
        EXPECT_TRUE(s.cvt_fail());
        EXPECT_EQ(s.code(), before);
        s.clear();
    };
    refused(IOv2::wcin);
    refused(IOv2::wcout);
    refused(IOv2::wcerr);
    refused(IOv2::wclog);

    oguard<true> out;
    IOv2::wcout << L"ab";
    IOv2::wcout.flush();
    EXPECT_EQ(out.contents(), "ab");
    EXPECT_TRUE(IOv2::wcout.good());
}

// Input cut off in the middle of a character is a decode failure, not a plain
// end of input -- but what came before it is still handed out: read() returns
// the two characters, then the stream reports cvtfailbit. The converter still
// holds the half character, so clear() does not get past it -- get() and ignore()
// alike keep failing and never reach eof -- while reset() drops it.
TEST(IoObjectsWchar, WcinHandsOutWhatCameBeforeACutOffCharacter)
{
    iguard g("ab\xe6");
    IOv2::wcin.reset();
    IOv2::wcin.switch_code("zh_CN.UTF-8");

    wchar_t buf[8] = {};
    wchar_t* end = IOv2::wcin.read(buf, 8);
    EXPECT_EQ(std::wstring(buf, end), L"ab");
    EXPECT_TRUE(IOv2::wcin.cvt_fail());

    for (int i = 0; i < 2; ++i)
    {
        IOv2::wcin.clear();
        wchar_t c = 0;
        EXPECT_FALSE(IOv2::wcin.get(c));
        EXPECT_TRUE(IOv2::wcin.cvt_fail());
        EXPECT_FALSE(IOv2::wcin.eof());

        IOv2::wcin.clear();
        IOv2::wcin.ignore(10);
        EXPECT_TRUE(IOv2::wcin.cvt_fail()) << "ignore() took the held half character for eof";
        EXPECT_FALSE(IOv2::wcin.eof());
    }

    IOv2::wcin.clear();
    IOv2::wcin.reset();
    wchar_t c = 0;
    EXPECT_FALSE(IOv2::wcin.get(c));
    EXPECT_TRUE(IOv2::wcin.eof());
    EXPECT_FALSE(IOv2::wcin.cvt_fail());
    IOv2::wcin.clear();
}

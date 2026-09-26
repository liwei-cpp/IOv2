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
 * It also carries the decoder's state. Under a stateful encoding that state is
 * what tells JIS X 0208 bytes from ASCII, so sync_with_stdio(), which rebuilds
 * the iochannel, has to take the decoder along.
 */
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/stateful_locale.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <string>

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

// ISO-2022-JP: a | ESC $ B | 中 | 中 | ESC ( B | b. Synchronized reading stops
// right after the first 中, still in JIS X 0208. The rebuilt iochannel used to
// start from ASCII and read the second 中 as `C f`, with no state bit set.
TEST(IoObjectsWchar, WcinKeepsTheShiftStateAcrossSyncWithStdio)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("IoObjectsWchar.WcinKeepsTheShiftStateAcrossSyncWithStdio"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    iguard g("a\x1b$BCfCf\x1b(Bb");
    IOv2::wcin.reset();
    IOv2::wcin.switch_code(stateful_locale_name);
    ASSERT_TRUE(IOv2::wcin.synced_with_stdio());

    EXPECT_EQ(IOv2::wcin.get(), L'a');
    EXPECT_EQ(IOv2::wcin.get(), L'中');

    EXPECT_TRUE(IOv2::wcin.sync_with_stdio(false));
    EXPECT_TRUE(IOv2::wcin.good());
    EXPECT_FALSE(IOv2::wcin.synced_with_stdio());
    EXPECT_EQ(IOv2::wcin.code(), stateful_locale_name);

    std::wstring rest;
    while (auto c = IOv2::wcin.get())
        rest += *c;
    EXPECT_EQ(rest, L"中b");
    EXPECT_FALSE(IOv2::wcin.cvt_fail());
}

// ISO-2022-JP: a | ESC $ B | 中 | 0xFF | 中 | 中 | ESC ( B | b. After the bad byte
// clear() reads on in JIS X 0208 -- it used to read `C f C f`, with no state bit
// set. The shift state survives the error, so switch_code() is refused until the
// text is back in ASCII.
TEST(IoObjectsWchar, WcinKeepsTheShiftStateAcrossAnInvalidByte)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("IoObjectsWchar.WcinKeepsTheShiftStateAcrossAnInvalidByte"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    iguard g("a\x1b$BCf\xff" "CfCf\x1b(Bb");
    IOv2::wcin.reset();
    IOv2::wcin.switch_code(stateful_locale_name);

    EXPECT_EQ(IOv2::wcin.get(), L'a');
    EXPECT_EQ(IOv2::wcin.get(), L'中');
    EXPECT_FALSE(IOv2::wcin.get());
    EXPECT_TRUE(IOv2::wcin.cvt_fail());
    IOv2::wcin.clear();

    EXPECT_EQ(IOv2::wcin.get(), L'中');

    // Still in JIS X 0208: switching now is refused and changes nothing.
    IOv2::wcin.switch_code("C");
    EXPECT_TRUE(IOv2::wcin.cvt_fail());
    EXPECT_EQ(IOv2::wcin.code(), stateful_locale_name);
    IOv2::wcin.clear();

    EXPECT_EQ(IOv2::wcin.get(), L'中');
    EXPECT_EQ(IOv2::wcin.get(), L'b');
    EXPECT_TRUE(IOv2::wcin.good());

    // Back in ASCII: the switch goes through.
    IOv2::wcin.switch_code("C");
    EXPECT_TRUE(IOv2::wcin.good());
    EXPECT_EQ(IOv2::wcin.code(), "C");
}

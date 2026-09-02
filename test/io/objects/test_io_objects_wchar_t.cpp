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
 */
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

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

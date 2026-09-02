// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same character and character-array extraction contract as
 * test_istream_extractors_character_char.cpp for wchar_t.
 *
 * The two limits -- the deduced array bound and the field width -- and the
 * terminator that one place is always kept for are counted in characters, so
 * they land in the same places here. What differs is which character types the
 * stream accepts at all: a wide stream takes wchar_t and nothing else, where
 * the narrow one also took signed char and unsigned char.
 */
#include <cvt/code_cvt.h>
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/traits/nullptr.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>
#include <support/io_traits_probe.h>

#include <cstddef>
#include <cwchar>
#include <string>
#include <vector>

using namespace IOv2;

TEST(IstreamExtractCharacterWchar, ASingleCharacterExtractionSkipsWhitespaceAndTakesOne)
{
    auto expect_one = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"  ab c")});

        wchar_t c = L'#';
        is >> c;
        EXPECT_EQ(c, L'a');
        EXPECT_EQ(is.peek(), L'b');

        is >> c;
        EXPECT_EQ(c, L'b');

        is >> c;
        EXPECT_EQ(c, L'c');
        EXPECT_TRUE(is.good());
    };

    expect_one.operator()<istream>();
    expect_one.operator()<iostream>();
}

TEST(IstreamExtractCharacterWchar, ACharacterExtractionThatFindsNothingLeavesTheVariableAlone)
{
    auto expect_untouched = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"")}};
            wchar_t c = L'#';
            is >> c;
            EXPECT_EQ(c, L'#');
            EXPECT_TRUE(is.str_fail());
        }
        {
            T is{mem_device{std::wstring(L"   ")}};
            wchar_t c = L'#';
            is >> c;
            EXPECT_EQ(c, L'#');
            EXPECT_TRUE(is.str_fail());
            EXPECT_TRUE(is.eof());
        }
    };

    expect_untouched.operator()<istream>();
    expect_untouched.operator()<iostream>();
}

TEST(IstreamExtractCharacterWchar, AnArrayExtractionTakesAWholeTokenAndTerminatesIt)
{
    auto expect_token = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"  alpha beta ")});

        wchar_t buf[16];
        for (wchar_t& c : buf) c = L'#';

        is >> buf;
        EXPECT_STREQ(buf, L"alpha");
        EXPECT_EQ(buf[6], L'#');
        EXPECT_EQ(is.peek(), L' ');
        EXPECT_TRUE(is.good());

        is >> buf;
        EXPECT_STREQ(buf, L"beta");
    };

    expect_token.operator()<istream>();
    expect_token.operator()<iostream>();
}

TEST(IstreamExtractCharacterWchar, TheArrayBoundLimitsTheExtractionOnItsOwn)
{
    auto expect_bounded = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"orchard planet")});

        wchar_t first[4];
        is >> first;
        EXPECT_STREQ(first, L"orc");
        EXPECT_TRUE(is.good());

        wchar_t second[3];
        is >> second;
        EXPECT_STREQ(second, L"ha");

        wchar_t remainder[16];
        is >> remainder;
        EXPECT_STREQ(remainder, L"rd");

        wchar_t whole[16];
        is >> whole;
        EXPECT_STREQ(whole, L"planet");
    };

    expect_bounded.operator()<istream>();
    expect_bounded.operator()<iostream>();
}

TEST(IstreamExtractCharacterWchar, TheFieldWidthLimitsTheExtractionAndIsSpentByIt)
{
    auto expect_width = []<template <typename, typename> class T>()
    {
        {
            T is(mem_device{std::wstring(L"watermelon")});

            wchar_t buf[16];
            is >> setw(4) >> buf;
            EXPECT_STREQ(buf, L"wat");
            EXPECT_EQ(is.width(), 0);

            is >> buf;
            EXPECT_STREQ(buf, L"ermelon");
        }
        {
            T is(mem_device{std::wstring(1000, L'a')});

            wchar_t buf[8];
            is >> setw(64) >> buf;
            EXPECT_EQ(std::wcslen(buf), 7u);

            is.clear();
            wchar_t big[64];
            is >> setw(8) >> big;
            EXPECT_EQ(std::wcslen(big), 7u);
        }
        {
            // A string destination needs no terminator, so the width buys a
            // character more there than it does into an array of the same size.
            T is(mem_device{std::wstring(L"watermelon")});

            std::wstring s;
            is >> setw(4) >> s;
            EXPECT_EQ(s, L"wate");
            EXPECT_EQ(is.width(), 0);
        }
    };

    expect_width.operator()<istream>();
    expect_width.operator()<iostream>();
}

TEST(IstreamExtractCharacterWchar, ALimitOfOneLeavesRoomOnlyForTheTerminator)
{
    auto expect_empty = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"abcdef")});

        wchar_t buf[16];
        buf[0] = L'#';
        is >> setw(1) >> buf;
        EXPECT_EQ(buf[0], L'\0');
        EXPECT_TRUE(is.str_fail());

        is.clear();
        EXPECT_EQ(is.peek(), L'a');
    };

    expect_empty.operator()<istream>();
    expect_empty.operator()<iostream>();
}

TEST(IstreamExtractCharacterWchar, ATokenEndedByTheInputIsStillAWholeToken)
{
    auto expect_last = []<template <typename, typename> class T>()
    {
        {
            T is(mem_device{std::wstring(L"   measure")});
            wchar_t buf[16];
            is >> buf;
            EXPECT_STREQ(buf, L"measure");
            EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
            EXPECT_FALSE(is.str_fail());
        }
        {
            T is(mem_device{std::wstring(L"   measure")});
            wchar_t buf[16];
            is >> setw(8) >> buf;
            EXPECT_STREQ(buf, L"measure");
            EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        }
    };

    expect_last.operator()<istream>();
    expect_last.operator()<iostream>();
}

// A wide stream takes its own character type and no other: the narrow types it
// would otherwise silently widen are rejected at the io_traits level, which is
// where the probe looks rather than through `in >> v`.
TEST(IstreamExtractCharacterWchar, OnlyTheStreamsOwnCharacterTypeIsExtractable)
{
    static_assert( extractable<wchar_t, wchar_t> );
    static_assert( !extractable<wchar_t, char> );
    static_assert( !extractable<wchar_t, signed char> );
    static_assert( !extractable<wchar_t, unsigned char> );
    static_assert( !extractable<wchar_t, char8_t> );
    static_assert( !extractable<wchar_t, char16_t> );
    static_assert( !extractable<wchar_t, char32_t> );
    static_assert( !extractable<wchar_t, std::nullptr_t> );
}

// The limits count characters, so a token of multi-byte characters is bounded
// at the same place as an ASCII one rather than at the same number of bytes.
TEST(IstreamExtractCharacterWchar, TheLimitsCountCharactersNotBytes)
{
    auto expect_characters = []<template <typename, typename> class T>()
    {
        {
            T is(mem_device{std::wstring(L"  漢字仮名交じり文 x")});

            wchar_t buf[5];
            is >> buf;                      // four characters and the terminator
            EXPECT_STREQ(buf, L"漢字仮名");
            EXPECT_TRUE(is.good());
        }
        {
            T is(mem_device{std::wstring(L"  漢字仮名交じり文 x")});

            std::wstring s;
            is >> setw(3) >> s;
            EXPECT_EQ(s, L"漢字仮");   // three characters, no terminator to pay for
            EXPECT_EQ(is.width(), 0);
        }
    };

    expect_characters.operator()<istream>();
    expect_characters.operator()<iostream>();
}

// Tokens far longer than the stream's internal buffer, over a real file and a
// real multi-byte encoding.
//
// A wide stream on a byte device carries a converter with a buffer of its own,
// and a block bigger than that buffer is handed to the device in one piece
// rather than copied through it. Reaching that path takes a single unformatted
// write of the whole text as the first thing the stream does, so that is how
// the file is written. Reading it back a token at a time then has to reassemble
// tokens across device reads, and the second half of the text is deliberately
// not ASCII so that those read boundaries fall between the bytes of a character
// as often as not. The token lengths straddle the converter's buffer in both
// directions.
TEST(IstreamExtractCharacterWchar, LongTokensRoundTripThroughAConvertingFileStream)
{
    std::vector<std::wstring> tokens;
    auto add = [&tokens](std::size_t n, const std::wstring& from) {
        std::wstring t;
        t.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            t += from[(i + tokens.size()) % from.size()];
        tokens.push_back(std::move(t));
    };

    for (const std::size_t n : {1u, 1023u, 1024u, 2048u, 5000u, 300000u, 3u})
        add(n, L"abcdez");
    for (const std::size_t n : {1u, 1023u, 2048u, 5000u})
        add(n, L"a中é漢zξ");

    const std::string path = "test_istream_extract_wide_tokens.txt";
    file_guard        guard(path);

    {
        ostream os(ofile_device<char>{path},
                   code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
        ASSERT_TRUE(static_cast<bool>(os));

        // One write per token, so the writes straddle whatever buffering sits
        // between the stream and the file: some fit in what is left, some do not.
        for (const std::wstring& t : tokens)
        {
            const std::wstring piece = t + L' ';
            os.write(piece.data(), static_cast<std::ptrdiff_t>(piece.size()));
        }

        auto [dev, err] = os.detach();
        dev.close();
    }

    auto expect_whole = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T is(TDevice{path}, code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
        ASSERT_TRUE(static_cast<bool>(is));

        std::size_t  n = 0;
        std::wstring tok;
        while (is >> tok)
        {
            ASSERT_LT(n, tokens.size());
            EXPECT_EQ(tok.size(), tokens[n].size());
            EXPECT_EQ(tok, tokens[n]);
            ++n;
        }

        EXPECT_EQ(n, tokens.size());
        EXPECT_TRUE(is.eof());
    };

    expect_whole.operator()<istream, ifile_device<char>>();
    expect_whole.operator()<iostream, file_device<char>>();
}

// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same read() contract as test_istream_read_char.cpp for wchar_t: a count
 * is either met or the input ran out trying, the return value says which by
 * saying where writing stopped, and a destination or a count that read will
 * not accept is refused before anything is written or extracted.
 *
 * The blocking-device case stays in the narrow file, since a pipe carries
 * bytes; what this instantiation covers is that the counting is in characters
 * rather than in bytes, which is only visible where the two differ.
 */
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    const std::wstring kDigits = L"0123456789abcdef";
}

TEST(IstreamReadWchar, ReadTakesExactlyTheCountAsked)
{
    auto expect_exact = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        wchar_t        buf[8] = {};
        wchar_t* end    = is.read(buf, 4);

        EXPECT_EQ(end - buf, 4);
        EXPECT_EQ(std::wstring(buf, end), L"0123");
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        EXPECT_EQ(is.peek(), L'4');
    };

    expect_exact.operator()<istream>();
    expect_exact.operator()<iostream>();
}

TEST(IstreamReadWchar, SuccessiveReadsWalkTheInput)
{
    auto expect_walked = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        wchar_t buf[16] = {};
        EXPECT_EQ(std::wstring(buf, is.read(buf, 4)), L"0123");
        EXPECT_EQ(std::wstring(buf, is.read(buf, 6)), L"456789");
        EXPECT_EQ(std::wstring(buf, is.read(buf, 6)), L"abcdef");
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    };

    expect_walked.operator()<istream>();
    expect_walked.operator()<iostream>();
}

TEST(IstreamReadWchar, ACountOfZeroIsSatisfiedWithoutReading)
{
    auto expect_nothing = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        wchar_t buf[8];
        for (wchar_t& c : buf) c = L'#';

        EXPECT_EQ(is.read(buf, 0), buf);
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        EXPECT_EQ(is.peek(), L'0');
        for (const wchar_t c : buf)
            EXPECT_EQ(c, L'#');
    };

    expect_nothing.operator()<istream>();
    expect_nothing.operator()<iostream>();
}

TEST(IstreamReadWchar, AShortReadKeepsWhatItGotAndReportsTheShortfall)
{
    auto expect_short = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        wchar_t        buf[32] = {};
        wchar_t* end     = is.read(buf, 32);

        EXPECT_EQ(static_cast<std::size_t>(end - buf), kDigits.size());
        EXPECT_EQ(std::wstring(buf, end), kDigits);
        EXPECT_TRUE(is.rdstate() & ios_defs::eofbit);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_short.operator()<istream>();
    expect_short.operator()<iostream>();
}

// The count is in characters, not in the bytes they would occupy: the fixture
// is chosen so that each character is several bytes wide when encoded, and the
// count still comes out one per character.
TEST(IstreamReadWchar, TheCountIsInCharactersNotBytes)
{
    auto expect_characters = []<template <typename, typename> class T>()
    {
        const std::wstring wide = L"é中é中é";
        T is(mem_device{wide});

        wchar_t        buf[8] = {};
        wchar_t* end    = is.read(buf, 3);

        EXPECT_EQ(end - buf, 3);
        EXPECT_EQ(std::wstring(buf, end), wide.substr(0, 3));
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        // Two characters are left, so asking for three is one too many.
        wchar_t rest[8] = {};
        EXPECT_EQ(is.read(rest, 3) - rest, 2);
        EXPECT_TRUE(is.rdstate() & ios_defs::eofbit);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_characters.operator()<istream>();
    expect_characters.operator()<iostream>();
}

TEST(IstreamReadWchar, AShortReadThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L"ab")}, locale<wchar_t>("C")};
        is.exceptions(ios_defs::eofbit);

        wchar_t buf[8] = {};
        EXPECT_ANY_THROW(is.read(buf, 5));
        EXPECT_TRUE(is.rdstate() & ios_defs::eofbit);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

TEST(IstreamReadWchar, ANullDestinationIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L"abc")}, locale<wchar_t>("C")};

        wchar_t* ret = nullptr;
        EXPECT_NO_THROW(ret = is.read(nullptr, 5));
        EXPECT_EQ(ret, nullptr);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

TEST(IstreamReadWchar, ACountThatIsNegativeIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        for (const std::ptrdiff_t n : {std::ptrdiff_t{-1},
                                       std::numeric_limits<std::ptrdiff_t>::min()})
        {
            SCOPED_TRACE(n);
            T is{mem_device{std::wstring(4096, L'x')}, locale<wchar_t>("C")};

            wchar_t buf[8];
            for (wchar_t& c : buf) c = L'#';

            wchar_t* ret = nullptr;
            EXPECT_NO_THROW(ret = is.read(buf, n));
            EXPECT_EQ(ret, buf);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
            for (const wchar_t c : buf)
                EXPECT_EQ(c, L'#');

            is.clear();
            EXPECT_EQ(is.peek(), L'x');
        }
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

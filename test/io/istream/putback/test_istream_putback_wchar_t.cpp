// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same putback contract as test_istream_putback_char.cpp for wchar_t: the
 * pushed-back character is the caller's, a successful putback leaves the state
 * alone, eofbit is cleared on the way in, and a failed stream is reported
 * rather than thrown out of.
 */
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

namespace
{
    const std::wstring kDigits = L"0123456789abcdef";
}

TEST(IstreamPutbackWchar, PutbackMakesTheNextReadYieldIt)
{
    auto expect_pushed_back = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        is.ignore(10);
        EXPECT_EQ(is.peek(), L'a');

        is.putback(L'Z');
        EXPECT_EQ(is.peek(), L'Z');

        EXPECT_EQ(is.get(), L'Z');
        EXPECT_EQ(is.get(), L'a');
    };

    expect_pushed_back.operator()<istream>();
    expect_pushed_back.operator()<iostream>();
}

TEST(IstreamPutbackWchar, ASuccessfulPutbackLeavesTheStateAlone)
{
    auto expect_state_kept = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});
        is.ignore(4);

        const ios_defs::iostate before = is.rdstate();
        is.putback(L'x');
        EXPECT_EQ(is.rdstate(), before);
        EXPECT_EQ(is.peek(), L'x');

        const ios_defs::iostate before2 = is.rdstate();
        is.putback(L'y');
        EXPECT_EQ(is.rdstate(), before2);
        EXPECT_EQ(is.peek(), L'y');
    };

    expect_state_kept.operator()<istream>();
    expect_state_kept.operator()<iostream>();
}

TEST(IstreamPutbackWchar, PutbackClearsEndOfFile)
{
    istream is(mem_device{std::wstring(L"ab")});

    std::wstring tok;
    is >> tok;
    EXPECT_EQ(tok, L"ab");
    EXPECT_TRUE(is.eof());

    is.putback(L'c');
    EXPECT_FALSE(is.eof());
    EXPECT_EQ(is.get(), L'c');
}

TEST(IstreamPutbackWchar, PutbackOnAFailedStreamIsReportedRatherThanThrown)
{
    auto expect_reported = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L"abc")}, locale<wchar_t>("C")};

        int v = 0;
        is >> v;
        EXPECT_FALSE(static_cast<bool>(is));

        EXPECT_NO_THROW(is.putback(L'z'));
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_reported.operator()<istream>();
    expect_reported.operator()<iostream>();
}

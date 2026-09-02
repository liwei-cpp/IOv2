// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same four endings as test_istream_getline_char.cpp for wchar_t: the
 * delimiter was found, the capacity ran out first, the input ran out having
 * given something, or the input ran out having given nothing.
 *
 * The stopping conditions are counted in characters, so a wide fixture reaches
 * them at the same call numbers as a narrow one regardless of how many bytes
 * its characters would encode to -- which is what this instantiation is for.
 */
#include <device/mem_device.h>
#include <facet/ctype.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using namespace IOv2;

TEST(IstreamGetlineWchar, TheStoppingConditionIsReadableFromTheState)
{
    auto expect_matrix = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"ab\ncdefgh\ni")});

        wchar_t buf[5];

        wchar_t* end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"ab");
        EXPECT_EQ(end - buf, 3);
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"cdef");
        EXPECT_EQ(end - buf, 5);
        EXPECT_EQ(is.rdstate(), ios_defs::strfailbit);
        EXPECT_FALSE(is.eof());

        is.clear();
        end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"gh");
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"i");
        EXPECT_EQ(end - buf, 2);
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit);

        is.clear();
        end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"");
        EXPECT_EQ(end - buf, 1);
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit | ios_defs::strfailbit);
    };

    expect_matrix.operator()<istream>();
    expect_matrix.operator()<iostream>();
}

// The same fixture written in characters that are several bytes wide reaches
// each ending at the same call, because the capacity counts characters.
TEST(IstreamGetlineWchar, TheCapacityCountsCharactersNotBytes)
{
    auto expect_characters = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"é中\n漢字ξ中é\n中")});

        wchar_t buf[5];

        wchar_t* end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"é中");
        EXPECT_EQ(end - buf, 3);
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"漢字ξ中");        // four characters, the capacity
        EXPECT_EQ(end - buf, 5);
        EXPECT_EQ(is.rdstate(), ios_defs::strfailbit);

        is.clear();
        end = is.template get<cons_sep, app_zt>(buf, 5, L'\n');
        EXPECT_EQ(std::wstring(buf), L"é");
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    };

    expect_characters.operator()<istream>();
    expect_characters.operator()<iostream>();
}

TEST(IstreamGetlineWchar, ACapacityOfOneLeavesRoomOnlyForTheTerminator)
{
    auto expect_terminator_only = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"abc")});

        wchar_t buf[4];
        for (wchar_t& c : buf) c = L'*';

        wchar_t* end = is.template get<cons_sep, app_zt>(buf, 1, L'\n');
        EXPECT_EQ(end - buf, 1);
        EXPECT_EQ(buf[0], L'\0');
        EXPECT_EQ(buf[1], L'*');
        EXPECT_TRUE(is.str_fail());

        is.clear();
        EXPECT_EQ(is.peek(), L'a');
    };

    expect_terminator_only.operator()<istream>();
    expect_terminator_only.operator()<iostream>();
}

// wchar_t counterpart: a null output pointer with a non-zero size is rejected up front,
// and under no_zt nothing is written through it.
TEST(IstreamGetlineWchar, ANullDestinationIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"hello")});

        wchar_t* ret = nullptr;
        EXPECT_NO_THROW((ret = is.template get<cons_sep, no_zt>(
                             static_cast<wchar_t*>(nullptr), 5, L'\n')));
        EXPECT_EQ(ret, nullptr);
        EXPECT_TRUE(is.str_fail());
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// wchar_t counterpart: with the ctype facet removed the default delimiter cannot be
// widened, so the call fails -- and under app_zt the error path still terminates the
// buffer, leaving buf + 1 as the returned pointer.
TEST(IstreamGetlineWchar, TheDefaultDelimiterNeedsTheCtypeFacet)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        const auto loc = locale<wchar_t>("C").remove<ctype_conf<wchar_t>>();
        T is{mem_device{std::wstring(L"hello")}, loc};

        wchar_t buf[8];
        buf[0] = L'*';

        wchar_t* ret = nullptr;
        EXPECT_NO_THROW((ret = is.template get<cons_sep, app_zt>(buf, 8)));
        EXPECT_EQ(ret, buf + 1);
        EXPECT_EQ(buf[0], L'\0');
        EXPECT_TRUE(is.str_fail());
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}

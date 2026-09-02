// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The ends manipulator on an ostream<wchar_t>.
 *
 * Same contract as the narrow case -- insert charT(), do not flush -- but the
 * character written has to be a wide null, one whole wchar_t of value zero, not
 * a single zero byte. That is the one way a wide implementation can get ends
 * wrong, so the length assertions here are in characters and the terminator is
 * compared against wchar_t() rather than against 0.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <cwchar>
#include <string>
#include <vector>

using namespace IOv2;

TEST(OstreamEndsWchar, EndsWritesOneWideNullCharacter)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{L""});

        os << ends;
        os.flush();

        const std::wstring out = os.device().str();
        ASSERT_EQ(out.size(), 1u);
        EXPECT_EQ(out[0], wchar_t());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// The count is in characters, so multi-byte content next to the terminator must
// not change how much ends contributes.
TEST(OstreamEndsWchar, EachEndsAddsExactlyOneCharacterWhereItWasWritten)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{L""});

        os << L"中é" << ends << L"漢字" << ends << L"ξ";
        os.flush();

        const std::wstring expected(L"中é\0漢字\0ξ", 7);
        EXPECT_EQ(os.device().str().size(), 7u);
        EXPECT_EQ(os.device().str(), expected);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamEndsWchar, EndsReturnsTheStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{L""});

        auto& same = (os << ends);
        EXPECT_EQ(&same, &os);
        same << L"tail";
        os.flush();

        EXPECT_EQ(os.device().str(), std::wstring(L"\0tail", 5));
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// What ends is for, in the wide case: std::wcslen has to find the terminator,
// which it only does if a whole wchar_t of zero was written.
TEST(OstreamEndsWchar, TerminatedRecordsCanBeReadBackAsCStrings)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{L""});

        os << L"alpha" << ends << L"" << ends << L"中é漢" << ends;
        os.flush();

        const std::wstring        out = os.device().str();
        std::vector<std::wstring> records;
        for (const wchar_t* p = out.data(); p < out.data() + out.size(); p += std::wcslen(p) + 1)
            records.emplace_back(p);

        ASSERT_EQ(records.size(), 3u);
        EXPECT_EQ(records[0], L"alpha");
        EXPECT_EQ(records[1], L"");
        EXPECT_EQ(records[2], L"中é漢");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

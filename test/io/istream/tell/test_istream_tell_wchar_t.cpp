// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same tell() contract as test_istream_tell_char.cpp for wchar_t.
 *
 * tell answers a question rather than doing anything, so most of its contract is
 * about what it agrees with: the number of characters consumed so far, the place
 * a subsequent read starts from, and its own previous answer. It reports through
 * an optional, which is what lets "there is no position to report" be said
 * without inventing a value for it -- a device that cannot report one leaves an
 * empty optional and devfailbit behind, and a stream that simply has nothing in
 * it still has a perfectly good position of zero.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so the position tell reports and the character read there check each other.
 */
#include <cvt/code_cvt.h>
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>

#include <string>

using namespace IOv2;

namespace
{
    const std::wstring kDigits = L"0123456789abcdef";
}

TEST(IstreamTellWchar, TellStartsAtZeroAndCountsWhatWasConsumed)
{
    auto expect_counted = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});
        EXPECT_EQ(is.tell(), 0u);

        is.get();
        EXPECT_EQ(is.tell(), 1u);

        is.ignore(4);
        EXPECT_EQ(is.tell(), 5u);

        wchar_t buf[8] = {};
        is.template get<keep_sep, no_zt>(buf, 3);
        EXPECT_EQ(is.tell(), 8u);   // no_zt spends the whole capacity on characters
    };

    expect_counted.operator()<istream>();
    expect_counted.operator()<iostream>();
}

// The position tell reports is the one the next read starts from; the two would
// be useless if they could disagree.
TEST(IstreamTellWchar, TellNamesWhereTheNextReadWillStart)
{
    auto expect_agreement = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        for (int i = 0; i < 6; ++i)
        {
            SCOPED_TRACE(i);
            const auto here = is.tell();
            ASSERT_TRUE(here.has_value());
            EXPECT_EQ(is.get(), kDigits[here.value()]);
        }
    };

    expect_agreement.operator()<istream>();
    expect_agreement.operator()<iostream>();
}

TEST(IstreamTellWchar, TellDoesNotConsumeAndRepeatsItsAnswer)
{
    auto expect_idempotent = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});
        is.ignore(6);

        const auto first = is.tell();
        EXPECT_EQ(is.tell(), first);
        EXPECT_EQ(is.tell(), first);
        EXPECT_EQ(is.get(), L'6');       // still where it was
    };

    expect_idempotent.operator()<istream>();
    expect_idempotent.operator()<iostream>();
}

// Nothing to read is not the same as nowhere to be: an empty stream is at
// position zero, and asking does not fail it.
TEST(IstreamTellWchar, AStreamWithNothingInItIsStillAtPositionZero)
{
    auto expect_zero = []<template <typename, typename> class T>()
    {
        T empty(mem_device{std::wstring(L"")});
        EXPECT_EQ(empty.tell(), 0u);
        EXPECT_TRUE(empty.good());
    };

    expect_zero.operator()<istream>();
    expect_zero.operator()<iostream>();
}

// A stream over a file and one over memory count the same way, so the same
// number of reads leaves them at the same position.
TEST(IstreamTellWchar, AFileAndAMemoryStreamCountAlike)
{
    const std::string path = "test_istream_tell_file.txt";
    file_guard        guard(path, std::string("0123456789abcdef"));

    auto expect_alike = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T from_memory{mem_device{kDigits}};
        T from_file{TDevice{path}, code_cvt_creator<char, wchar_t>("C")};
        ASSERT_TRUE(from_file.good());

        EXPECT_EQ(from_memory.tell(), from_file.tell());

        from_memory.ignore(7);
        from_file.ignore(7);
        EXPECT_EQ(from_memory.tell(), from_file.tell());
        EXPECT_EQ(from_memory.get(), from_file.get());
    };

    expect_alike.operator()<istream, ifile_device<char>>();
    expect_alike.operator()<iostream, file_device<char>>();
}

// Once the device has refused a move there is no position to report, and the
// empty optional says so rather than a value that would be wrong.
TEST(IstreamTellWchar, AnUnreportablePositionIsEmptyRatherThanWrong)
{
    auto expect_empty = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::wstring(L"")});

        is.seek(10);                      // past the end of an empty device
        EXPECT_TRUE(is.rdstate() & ios_defs::devfailbit);

        const auto pos = is.tell();
        EXPECT_FALSE(pos.has_value());
        EXPECT_EQ(is.tell(), pos);
    };

    expect_empty.operator()<istream>();
    expect_empty.operator()<iostream>();
}

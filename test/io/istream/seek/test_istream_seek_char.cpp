// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char>::seek(): moving the read position.
 *
 * seek takes an absolute position, so a relative move is spelled by the caller
 * as seek(tell() + n) -- which is why every case here reads a position back
 * before it moves. What seek promises is that the next read starts where it was
 * sent, and that it leaves the stream's state as it found it, with one
 * exception: a stream that had read to the end is usable again, because the end
 * it reached is no longer where it is.
 *
 * When the device refuses the move, the failure is a state bit and the position
 * afterwards is not reportable at all -- an unreported position is safer than a
 * stale one, since a caller acting on a stale answer would read the wrong bytes.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so a position and the character read there check each other.
 */
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
    const std::string kDigits = "0123456789abcdef";
}

TEST(IstreamSeekChar, SeekSendsTheNextReadWhereItWasAsked)
{
    auto expect_moved = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        is.seek(10);
        EXPECT_EQ(is.get(), 'a');     // index 10

        is.seek(0);
        EXPECT_EQ(is.get(), '0');

        is.seek(15);
        EXPECT_EQ(is.get(), 'f');
    };

    expect_moved.operator()<istream>();
    expect_moved.operator()<iostream>();
}

// A relative move is the caller's arithmetic on a position it read back, so the
// two operations have to agree about what a position means.
TEST(IstreamSeekChar, SeekingRelativeToTheCurrentPositionAdvancesByThatMuch)
{
    auto expect_relative = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        const auto start = is.tell();
        ASSERT_TRUE(start.has_value());

        is.seek(start.value() + 10);
        EXPECT_EQ(is.tell(), start.value() + 10);
        EXPECT_EQ(is.get(), 'a');

        // And again from where that left off.
        const auto here = is.tell();
        ASSERT_TRUE(here.has_value());
        is.seek(here.value() + 4);
        EXPECT_EQ(is.get(), 'f');
    };

    expect_relative.operator()<istream>();
    expect_relative.operator()<iostream>();
}

// The end of the input is a position, not a property of the stream: moving away
// from it makes the stream readable again.
TEST(IstreamSeekChar, SeekClearsEndOfFile)
{
    auto expect_revived = []<template <typename, typename> class T>()
    {
        T    is(mem_device{kDigits});
        char buf[32] = {};

        is.template get<keep_sep, no_zt>(buf, static_cast<std::ptrdiff_t>(kDigits.size()) + 1);
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit);

        is.seek(0);
        EXPECT_TRUE(is.good());
        EXPECT_TRUE(static_cast<bool>(is));
        EXPECT_EQ(is.tell(), 0u);
        EXPECT_EQ(is.get(), '0');
    };

    expect_revived.operator()<istream>();
    expect_revived.operator()<iostream>();
}

TEST(IstreamSeekChar, ASuccessfulSeekLeavesTheStateOtherwiseAlone)
{
    auto expect_state_kept = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});
        is.ignore(4);

        const ios_defs::iostate before = is.rdstate();
        is.seek(8);
        EXPECT_EQ(is.rdstate(), before);
        EXPECT_EQ(is.get(), '8');
    };

    expect_state_kept.operator()<istream>();
    expect_state_kept.operator()<iostream>();
}

// A move the device cannot make is reported, and the position afterwards is not
// reportable either -- a stale answer would send the next read to the wrong place.
TEST(IstreamSeekChar, SeekingPastTheEndFailsAndLeavesNoPositionToReport)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        T empty(mem_device{std::string("")});
        EXPECT_TRUE(empty.good());

        const auto before = empty.tell();
        EXPECT_EQ(before, 0u);           // an empty device still has a position

        const ios_defs::iostate state_before = empty.rdstate();
        empty.seek(10);
        EXPECT_NE(empty.rdstate(), state_before);
        EXPECT_EQ(empty.rdstate(), ios_defs::devfailbit);

        const auto after = empty.tell();
        EXPECT_FALSE(after.has_value());
        EXPECT_EQ(empty.tell(), after);  // and it stays that way
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}

// The same over a real file, where the position is the device's own rather than
// an index into a buffer the stream owns.
TEST(IstreamSeekChar, SeekWorksOverAFileDevice)
{
    const std::string path = "test_istream_seek_file.txt";
    file_guard        guard(path, kDigits);

    auto expect_moved = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T is{TDevice{path}};
        ASSERT_TRUE(is.good());

        is.seek(10);
        EXPECT_EQ(is.tell(), 10u);
        EXPECT_EQ(is.get(), 'a');

        const auto here = is.tell();
        ASSERT_TRUE(here.has_value());
        is.seek(here.value() + 4);
        EXPECT_EQ(is.get(), 'f');

        is.seek(0);
        EXPECT_EQ(is.tell(), 0u);
        EXPECT_EQ(is.get(), '0');
    };

    expect_moved.operator()<istream, ifile_device<char>>();
    expect_moved.operator()<iostream, file_device<char>>();
}

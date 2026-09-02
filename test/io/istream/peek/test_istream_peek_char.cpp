// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char>::peek(): looking at the next character without taking it.
 *
 * Everything peek promises is about what it does *not* do. It does not consume,
 * so asking twice gives the same answer and the next read gives it again. It
 * does not move the read position, which is visible through tell() on a
 * seekable device. And it does not change the stream's state -- except at the
 * end of the input, where there is nothing to report and eofbit is the report.
 *
 * IOv2 spells "nothing to report" as an empty optional rather than as a
 * sentinel value, so the end-of-input answer cannot be confused with a
 * character whose value happens to equal the sentinel.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so a position and the character peeked at it check each other.
 */
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>

#include <string>

using namespace IOv2;

namespace
{
    const std::string kDigits = "0123456789abcdef";
}

TEST(IstreamPeekChar, PeekReportsTheNextCharacterWithoutConsumingIt)
{
    auto expect_non_destructive = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        // Asking repeatedly is asking the same question.
        EXPECT_EQ(is.peek(), '0');
        EXPECT_EQ(is.peek(), '0');
        EXPECT_EQ(is.peek(), '0');

        // And the read that follows still gets it.
        EXPECT_EQ(is.get(), '0');
        EXPECT_EQ(is.peek(), '1');
    };

    expect_non_destructive.operator()<istream>();
    expect_non_destructive.operator()<iostream>();
}

TEST(IstreamPeekChar, PeekLeavesTheStateAlone)
{
    auto expect_state_kept = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        const ios_defs::iostate before = is.rdstate();
        EXPECT_EQ(is.peek(), '0');
        EXPECT_EQ(is.rdstate(), before);
    };

    expect_state_kept.operator()<istream>();
    expect_state_kept.operator()<iostream>();
}

// Whatever left the read position where it is -- a read, an ignore of a count,
// an ignore up to a delimiter -- peek reports the character now under it.
TEST(IstreamPeekChar, PeekReportsWhateverTheCursorIsOn)
{
    auto expect_follows_cursor = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        char buf[8] = {};
        is.read(buf, 4);                  // consumed "0123"
        EXPECT_EQ(is.peek(), '4');

        is.ignore();                      // one more
        EXPECT_EQ(is.peek(), '5');

        is.ignore(0);                     // none at all
        EXPECT_EQ(is.peek(), '5');

        is.ignore(6, '8');                // stops after the delimiter
        EXPECT_EQ(is.peek(), '9');
    };

    expect_follows_cursor.operator()<istream>();
    expect_follows_cursor.operator()<iostream>();
}

TEST(IstreamPeekChar, PeekAtTheEndReportsNothingAndSetsEndOfFile)
{
    auto expect_end = []<template <typename, typename> class T>()
    {
        // An empty stream is at the end before anything is read.
        T empty{mem_device{std::string("")}};
        EXPECT_EQ(empty.rdstate(), ios_defs::goodbit);
        EXPECT_FALSE(empty.peek().has_value());
        EXPECT_EQ(empty.rdstate(), ios_defs::eofbit);

        // And so is one that has been read out.
        T drained(mem_device{kDigits});
        drained.ignore(kDigits.size());
        EXPECT_FALSE(drained.peek().has_value());
        EXPECT_TRUE(drained.eof());
    };

    expect_end.operator()<istream>();
    expect_end.operator()<iostream>();
}

// The read position is where the device says it is, so a peek that moved it
// would show up here even though nothing was consumed.
TEST(IstreamPeekChar, PeekDoesNotMoveTheReadPosition)
{
    const std::string path = "test_istream_peek_position.txt";
    file_guard        guard(path, kDigits);

    auto expect_still = [&path]<template <typename, typename> class T, typename TDevice>()
    {
        T is(TDevice{path});
        is.seek(0);

        const auto before = is.tell();
        EXPECT_EQ(is.peek(), '0');
        EXPECT_EQ(is.tell(), before);

        // And from somewhere that is not the start.
        is.seek(10);
        const auto middle = is.tell();
        EXPECT_EQ(is.peek(), 'a');
        EXPECT_EQ(is.tell(), middle);
    };

    expect_still.operator()<istream, ifile_device<char>>();
    expect_still.operator()<iostream, file_device<char>>();
}

// With eofbit in the exception mask the same end-of-input answer is delivered by
// throwing instead of by returning nothing; the bit is set either way.
TEST(IstreamPeekChar, PeekAtTheEndThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        T masked{mem_device{std::string("")}, locale<char>("C")};
        masked.exceptions(ios_defs::eofbit);
        EXPECT_THROW((void)masked.peek(), eof_error);
        EXPECT_TRUE(masked.eof());

        T unmasked{mem_device{std::string("")}, locale<char>("C")};
        EXPECT_FALSE(unmasked.peek().has_value());
        EXPECT_TRUE(unmasked.eof());
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

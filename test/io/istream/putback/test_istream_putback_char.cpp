// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char>::putback(c): pushing a character back in front of the read
 * position.
 *
 * The contract is short and every part of it is observable. A successful
 * putback leaves the stream's state exactly as it found it and makes the next
 * read yield the character that was pushed -- which need not be the one that
 * came out, because it is the caller's character, not the buffer's memory of
 * one. IOv2 adds one clause of its own: putback clears eofbit before touching
 * the buffer, so a stream that has read to the end can be pushed back into
 * having something to read.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so a position and the character read at it check each other rather than
 * resting on an offset into a phrase.
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
    const std::string kDigits = "0123456789abcdef";
}

TEST(IstreamPutbackChar, PutbackMakesTheNextReadYieldIt)
{
    auto expect_pushed_back = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        is.ignore(10);
        EXPECT_EQ(is.peek(), 'a');      // index 10

        // The character pushed back is the caller's, not the one just consumed.
        is.putback('Z');
        EXPECT_EQ(is.peek(), 'Z');

        // And reading resumes from there.
        EXPECT_EQ(is.get(), 'Z');
        EXPECT_EQ(is.get(), 'a');
    };

    expect_pushed_back.operator()<istream>();
    expect_pushed_back.operator()<iostream>();
}

TEST(IstreamPutbackChar, ASuccessfulPutbackLeavesTheStateAlone)
{
    auto expect_state_kept = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});
        is.ignore(4);

        const ios_defs::iostate before = is.rdstate();
        is.putback('x');
        EXPECT_EQ(is.rdstate(), before);
        EXPECT_EQ(is.peek(), 'x');

        // Twice over, so that "unchanged" is not just "was already good".
        const ios_defs::iostate before2 = is.rdstate();
        is.putback('y');
        EXPECT_EQ(is.rdstate(), before2);
        EXPECT_EQ(is.peek(), 'y');
    };

    expect_state_kept.operator()<istream>();
    expect_state_kept.operator()<iostream>();
}

// putback unsets eofbit before reaching the buffer, so a stream that has read
// everything can be given something to read again.
TEST(IstreamPutbackChar, PutbackClearsEndOfFile)
{
    istream is(mem_device{std::string("ab")});

    std::string tok;
    is >> tok;
    EXPECT_EQ(tok, "ab");
    EXPECT_TRUE(is.eof());

    is.putback('c');
    EXPECT_FALSE(is.eof());
    EXPECT_EQ(is.get(), 'c');
}

// A stream already in a failed state is rejected by the input sentry, which
// throws; putback's own catch routes that through handle_exception. With no
// exception mask set nothing escapes to the caller.
TEST(IstreamPutbackChar, PutbackOnAFailedStreamIsReportedRatherThanThrown)
{
    auto expect_reported = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("abc")}, locale<char>("C")};

        int v = 0;
        is >> v;                        // non-numeric input -> strfailbit
        EXPECT_FALSE(static_cast<bool>(is));

        EXPECT_NO_THROW(is.putback('z'));
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_reported.operator()<istream>();
    expect_reported.operator()<iostream>();
}

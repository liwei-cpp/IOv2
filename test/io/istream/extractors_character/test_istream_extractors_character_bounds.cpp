// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The bounds the array extractor stops at, and the two early returns that
 * never reach the extraction loop at all.
 *
 * The helper takes the smaller of the array bound and a non-zero field width,
 * always keeping one place for the terminator. A width of zero means "no width
 * set" rather than "a width of zero", which is why the bound is
 * `width == 0 ? N - 1 : min(width, N) - 1` rather than the shorter formula the
 * documentation used to carry -- substituting zero into a `min` gives either
 * -1 or SIZE_MAX, and the second is exactly the unbounded read the check
 * exists to prevent.
 *
 * Two shapes never get as far as reading anything. A buffer with room for the
 * terminator alone leaves nowhere to put a character, and a width of 1 narrows
 * any buffer to that same shape. Both terminate the buffer, set the failure
 * bit, and -- the part worth pinning -- consume no input, so the token is
 * still there for the next extraction.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace
{
using is_c = IOv2::istream<IOv2::mem_device<char>, char>;

constexpr char k_unwritten = '\x5a';

is_c stream_over(const char* text)
{
    return is_c{IOv2::mem_device<char>{std::string(text)}, IOv2::locale<char>("C")};
}
}

TEST(IstreamExtractCharacterBounds, AZeroWidthMeansTheArrayBoundAloneApplies)
{
    // The default width. The bound is N - 1 characters plus the terminator,
    // and the rest of the token stays in the stream.
    char buf[4];
    std::memset(buf, k_unwritten, sizeof buf);

    is_c is = stream_over("abcdefg");
    is >> buf;

    EXPECT_FALSE(is.str_fail());
    EXPECT_STREQ(buf, "abc");

    char rest[8];
    std::memset(rest, k_unwritten, sizeof rest);
    is >> rest;
    EXPECT_STREQ(rest, "defg") << "the bound stops the read, it does not discard";
}

TEST(IstreamExtractCharacterBounds, TheWidthOnlyEverTightensTheBound)
{
    {
        // width < N: the width wins.
        char buf[8];
        std::memset(buf, k_unwritten, sizeof buf);

        is_c is = stream_over("abcdefg");
        is >> IOv2::setw(4) >> buf;

        EXPECT_STREQ(buf, "abc");
        EXPECT_EQ(is.width(), 0u) << "the width is spent by the extraction that honours it";
    }
    {
        // width > N: the array bound still wins, which is what makes the
        // overload safe regardless of a stale width.
        char buf[4];
        std::memset(buf, k_unwritten, sizeof buf);

        is_c is = stream_over("abcdefg");
        is >> IOv2::setw(1000) >> buf;

        EXPECT_STREQ(buf, "abc");
    }
}

TEST(IstreamExtractCharacterBounds, ABufferWithRoomForTheTerminatorAloneReadsNothing)
{
    char buf[1];
    buf[0] = k_unwritten;

    is_c is = stream_over("abc");
    is >> buf;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(buf[0], '\0') << "the terminator still goes in";

    // Nothing was taken, so a later extraction sees the whole token.
    is.clear();
    char rest[8];
    std::memset(rest, k_unwritten, sizeof rest);
    is >> rest;
    EXPECT_STREQ(rest, "abc");
}

TEST(IstreamExtractCharacterBounds, AWidthOfOneNarrowsAnyBufferToThatSameShape)
{
    char buf[8];
    std::memset(buf, k_unwritten, sizeof buf);

    is_c is = stream_over("abc");
    is >> IOv2::setw(1) >> buf;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(buf[0], '\0');
    EXPECT_EQ(is.width(), 0u);

    is.clear();
    char rest[8];
    std::memset(rest, k_unwritten, sizeof rest);
    is >> rest;
    EXPECT_STREQ(rest, "abc") << "the early return consumes nothing";
}

TEST(IstreamExtractCharacterBounds, TheDelimiterIsLeftInTheStream)
{
    // The loop stops before the whitespace rather than consuming it, so the
    // next extraction starts at the delimiter and skips it itself.
    char first[8];
    char second[8];
    std::memset(first, k_unwritten, sizeof first);
    std::memset(second, k_unwritten, sizeof second);

    is_c is = stream_over("ab cd");
    is >> first >> second;

    EXPECT_STREQ(first, "ab");
    EXPECT_STREQ(second, "cd");
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * What the character extractors leave behind when they do not succeed.
 *
 * Three separate contracts meet here, and each one used to be documented more
 * broadly than it held:
 *
 * The array form promises a terminator on every exit. That promise is about
 * the inside of the extraction helper, not about `is >> buf`: when the sentry
 * reports failure the formatted function returns without attempting any input
 * at all, so the array is never touched. The boundary therefore runs between
 * "the sentry failed" and "the sentry succeeded but no character was
 * extracted" -- only the second writes a terminator. libstdc++ draws it in the
 * same place, so these cases pin conformance rather than a local choice.
 *
 * The basic_string form clears its target up front and fills it through a
 * 128-character staging buffer. A throw part way through therefore used to
 * drop up to 127 characters that had already been consumed off the input,
 * leaving neither an empty string nor the previous value. The extraction loop
 * now appends the staging buffer on the exception path too, so what survives
 * is the complete prefix of what was consumed.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <cstring>
#include <string>

#include <support/throwing_get_device.h>

#include <gtest/gtest.h>

namespace
{
using is_c = IOv2::istream<IOv2::mem_device<char>, char>;
using is_throwing = IOv2::istream<throwing_get_device<char>, char>;

// A byte the extractors never write, so "untouched" is distinguishable from
// "written with something that happens to look empty".
constexpr char k_unwritten = '\x5a';

// Position-bearing input: the character at index i is 'a' + i % 26, so a
// truncated result is checked for content and not merely for length.
std::string patterned(std::size_t n)
{
    std::string s;
    s.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        s.push_back(static_cast<char>('a' + i % 26));
    return s;
}
}

TEST(IstreamExtractCharacterFailureShapes, AFailedSentryLeavesTheArrayUntouched)
{
    // Empty input, all-whitespace input and an already-failed stream are the
    // three ways the sentry reports failure. None of them reaches the
    // extraction helper, so none of them writes the terminator.
    const char* const inputs[] = {"", "   ", "ignored"};

    for (int which = 0; which < 3; ++which)
    {
        char buf[8];
        std::memset(buf, k_unwritten, sizeof buf);

        is_c is{IOv2::mem_device<char>{std::string(inputs[which])}, IOv2::locale<char>("C")};
        if (which == 2)
            is.setstate(IOv2::ios_defs::strfailbit);

        is >> buf;

        EXPECT_TRUE(is.str_fail()) << "case " << which;
        for (std::size_t i = 0; i < sizeof buf; ++i)
            EXPECT_EQ(buf[i], k_unwritten) << "case " << which << " byte " << i;
    }
}

TEST(IstreamExtractCharacterFailureShapes, ASucceedingSentryThatExtractsNothingStillTerminates)
{
    // noskipws puts the leading space in front of the extraction rather than
    // in front of the sentry, so the sentry succeeds and the helper runs. It
    // extracts nothing, sets the failure bit -- and writes the terminator,
    // which is the half of the contract that does hold.
    char buf[8];
    std::memset(buf, k_unwritten, sizeof buf);

    is_c is{IOv2::mem_device<char>{std::string(" ab")}, IOv2::locale<char>("C")};
    is.unsetf(IOv2::ios_defs::skipws);

    is >> buf;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(buf[0], '\0');
}

TEST(IstreamExtractCharacterFailureShapes, AThrowMidTokenKeepsTheWholeConsumedPrefixInTheString)
{
    // The staging buffer holds 128 characters, so the interesting boundary is
    // where a flush has and has not happened: before the fix, boom = 130 kept
    // 128 characters -- truncated to the flush granularity -- instead of 129.
    //
    // The sentry spends the first dget() looking for whitespace to skip, so a
    // throw on call n leaves n - 1 characters in the string. boom = 1 is not
    // this case at all: the sentry itself throws, which the sentry test below
    // covers.
    for (std::size_t boom : {std::size_t{2}, std::size_t{127}, std::size_t{128},
                             std::size_t{129}, std::size_t{130}, std::size_t{255},
                             std::size_t{256}, std::size_t{300}})
    {
        const std::string source = patterned(512);

        // dget() hands out one character per call and throws on the boom-th,
        // so exactly boom - 1 characters reach the string.
        is_throwing is{throwing_get_device<char>{source, boom},
                       IOv2::locale<char>("C")};
        std::string value = "PREVIOUS";

        is >> value;

        const std::size_t consumed = boom - 1;
        ASSERT_EQ(value.size(), consumed) << "boom " << boom;
        EXPECT_EQ(value, source.substr(0, consumed)) << "boom " << boom;
    }
}

TEST(IstreamExtractCharacterFailureShapes, AFailedSentryLeavesTheStringAtItsPreviousValue)
{
    // The clear happens inside the extraction helper, which a failed sentry
    // never reaches -- so here, unlike every other failure path, the previous
    // contents really do survive.
    std::string value = "PREVIOUS";

    is_c is{IOv2::mem_device<char>{std::string("")}, IOv2::locale<char>("C")};
    is >> value;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(value, "PREVIOUS");
}

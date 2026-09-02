// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Two unrelated pieces of ios_base state.
 *
 * boolalpha is the format flag that swaps a bool between digits and words; the
 * words come from the numeric facet, so what it writes is the locale's business
 * and the flag only decides which of the two forms is asked for.
 *
 * The second half is about handle_exception, which has to be idempotent: a
 * failure that passes through two nested handling points must reach the caller
 * as the same exception both times, not as a generic stand-in the second time
 * round.
 */
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <exception>
#include <string>

using namespace IOv2;

// The flag decides the form; the facet decides the words. Both forms are checked
// on one stream, because setting the flag must not be a one-shot.
TEST(IosBaseState, BoolalphaWritesTheWordsTheFacetSupplies)
{
    ostream os(mem_device{""}, locale<char>("C"));
    os.flags(ios_defs::boolalpha);

    os << true << ' ' << false << ' ' << true;
    EXPECT_EQ(os.device().str(), "true false true");
}

// Without the flag a bool is a number, and 0/1 rather than the value's bit
// pattern.
TEST(IosBaseState, WithoutBoolalphaABoolIsWrittenAsADigit)
{
    ostream os(mem_device{""}, locale<char>("C"));

    os << true << ' ' << false;
    EXPECT_EQ(os.device().str(), "1 0");
}

// Handling the same exception twice is observably the same as handling it once. Nested
// handling points make this routine: put() handles its own exception and then rethrows on
// account of the mask, so the same exception reaches the caller's catch as well. What a
// regression would look like is the message being swapped, not a crash -- clear() falls back
// to a generic stand-in ("stream failure bit has been set") whenever the category holds no
// stored exception_ptr, and the first pass consumes the one that was stored.
TEST(IosBaseState, HandlingTheSameExceptionTwiceReportsItTheSameWay)
{
    std::string original;
    std::exception_ptr ex;
    try
    {
        throw stream_error("handle_exception idempotence probe");
    }
    catch (const stream_error& e)
    {
        original = e.what();
        ex = std::current_exception();
    }

    ostream oss(mem_device{""}, locale<char>("C"));
    oss.exceptions(ios_defs::strfailbit);

    std::string first;
    try { oss.handle_exception(ex); }
    catch (const stream_error& e) { first = e.what(); }
    EXPECT_EQ(first, original);
    EXPECT_TRUE(oss.str_fail());

    std::string second;
    try { oss.handle_exception(ex); }
    catch (const stream_error& e) { second = e.what(); }
    EXPECT_EQ(second, original);
    EXPECT_TRUE(oss.str_fail());

    // With the bit out of the mask neither pass throws, and the bit stays set.
    oss.clear();
    oss.exceptions(ios_defs::goodbit);
    ASSERT_TRUE(oss.good());

    EXPECT_NO_THROW(oss.handle_exception(ex));
    EXPECT_NO_THROW(oss.handle_exception(ex));
    EXPECT_TRUE(oss.str_fail());
}

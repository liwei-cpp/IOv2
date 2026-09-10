// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * What the character extraction points require of the iterator they are handed.
 *
 * Both of them are constrained by `std::sentinel_for<TSent, TIter>` plus a check that
 * `TIter::value_type` is the character type. `sentinel_for` pulls in `input_or_output_iterator`,
 * which asks only that `*i` be referenceable and that `++i` advance -- it does not ask for
 * `indirectly_readable`, so nothing in the contract says `*i` gives the same answer twice.
 *
 * The loops used to dereference twice per position, once to test for a delimiter and once to
 * store, which silently tested one value and kept another. These cases pin the fix: one
 * dereference per position, and the character that passes the delimiter test is the character
 * that lands in the target.
 *
 * The calls here are direct rather than through a stream because that is the shape the contract
 * is stated for -- `traits_base.h` lists the iterator form as a recipe callers may write by hand.
 */
#include <IOv2/io/io_base.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <cstddef>
#include <cstring>
#include <iterator>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

using namespace IOv2;

namespace
{
// Satisfies exactly what the extraction points ask for and nothing more: `*i` does not advance,
// but it is not stable either -- the second read of a position answers with the upper-case
// letter, standing in for any iterator whose source can change between two reads.
struct unstable_iter
{
    using value_type      = char;
    using difference_type = std::ptrdiff_t;

    const char* p     = nullptr;
    mutable int reads = 0;

    char operator*() const { return (++reads % 2) ? *p : static_cast<char>(*p - 32); }
    unstable_iter& operator++() { ++p; reads = 0; return *this; }
    void operator++(int) { ++p; reads = 0; }
    bool operator==(const unstable_iter& other) const { return p == other.p; }
};

static_assert(std::input_or_output_iterator<unstable_iter>);
static_assert(std::sentinel_for<unstable_iter, unstable_iter>);
static_assert(std::is_same_v<char, unstable_iter::value_type>);

// Same shape, but stable; it only counts how often it is read.
struct counting_iter
{
    using value_type      = char;
    using difference_type = std::ptrdiff_t;

    const char* p     = nullptr;
    int*        count = nullptr;

    char operator*() const { ++*count; return *p; }
    counting_iter& operator++() { ++p; return *this; }
    void operator++(int) { ++p; }
    bool operator==(const counting_iter& other) const { return p == other.p; }
};

static_assert(std::sentinel_for<counting_iter, counting_iter>);
}

TEST(IstreamExtractCharacterIteratorContract, TheDelimiterTestAndTheStoreSeeTheSameCharacter)
{
    const char* const  source = "abcd ";
    const locale<char> loc("C");

    {
        ios_base<char> io;
        char           buf[16];
        std::memset(buf, 0, sizeof buf);

        istream_extract(unstable_iter{source}, unstable_iter{source + 5}, io, loc, buf,
                        sizeof buf);

        // Reading twice would test 'a' and store 'A'.
        EXPECT_STREQ(buf, "abcd");
    }
    {
        ios_base<char> io;
        std::string    value;

        io_traits<char, std::string>::sread(unstable_iter{source}, unstable_iter{source + 5}, io,
                                            loc, value);

        EXPECT_EQ(value, "abcd");
    }
}

TEST(IstreamExtractCharacterIteratorContract, EachPositionIsDereferencedExactlyOnce)
{
    const char* const  source = "abcd ";
    const locale<char> loc("C");

    // Four characters and the delimiter that stops the loop: five positions, five reads.
    constexpr int k_expected = 5;

    {
        ios_base<char> io;
        char           buf[16];
        int            count = 0;

        istream_extract(counting_iter{source, &count}, counting_iter{source + 5, &count}, io, loc,
                        buf, sizeof buf);

        EXPECT_EQ(count, k_expected);
    }
    {
        ios_base<char> io;
        std::string    value;
        int            count = 0;

        io_traits<char, std::string>::sread(counting_iter{source, &count},
                                            counting_iter{source + 5, &count}, io, loc, value);

        EXPECT_EQ(count, k_expected);
    }
}

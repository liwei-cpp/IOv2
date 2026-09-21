// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The fill ceiling, `ios_defs::max_pad_count`, on every character path.
 *
 * All five insertions in char_and_str.h -- a character, a widened narrow
 * character, a string, a widened narrow string and a std::basic_string -- end
 * in the same `ostream_insert`, and that is where the ceiling is checked. The
 * arithmetic and pointer paths pin their own ceiling elsewhere; this file pins
 * that the character paths sit on exactly the same edge: `max_pad_count` fill
 * characters go through, one more is refused before anything is written, and
 * the width is spent either way.
 *
 * The edge is expressed as a width of `cap + length`, so each case has to know
 * how long its value is -- which is also what makes the widened string path
 * worth its own case: its length is counted after widening, not before.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using namespace IOv2;

namespace
{
    constexpr std::size_t cap = ios_defs::max_pad_count;

    template <typename C>
    using os_t = ostream<mem_device<C>, C>;

    // Writes `value` at the last width the ceiling admits and at the first it
    // refuses; `length` is how many characters `value` occupies once written.
    template <typename C, typename V>
    void expect_edge(const V& value, std::size_t length)
    {
        {
            os_t<C> os{mem_device<C>{}, locale<C>("C")};
            os << setw(static_cast<std::ptrdiff_t>(cap + length)) << value;
            EXPECT_FALSE(os.str_fail());
            EXPECT_EQ(os.device().str().size(), cap + length);
            EXPECT_EQ(os.width(), 0u);
        }
        {
            os_t<C> os{mem_device<C>{}, locale<C>("C")};
            os << setw(static_cast<std::ptrdiff_t>(cap + length + 1)) << value;
            EXPECT_TRUE(os.str_fail());
            EXPECT_TRUE(os.device().str().empty()) << "the check runs before the first character";
            EXPECT_EQ(os.width(), 0u);
        }
    }
}

TEST(OstreamInsertCharacterFillCeiling, ACharacterSitsOnTheEdge)
{
    expect_edge<char>('x', 1);
    expect_edge<wchar_t>(L'x', 1);
}

TEST(OstreamInsertCharacterFillCeiling, AWidenedCharacterSitsOnTheSameEdge)
{
    expect_edge<wchar_t>('x', 1);
}

TEST(OstreamInsertCharacterFillCeiling, AStringSitsOnTheEdge)
{
    expect_edge<char>("ab", 2);
    expect_edge<wchar_t>(L"ab", 2);
}

TEST(OstreamInsertCharacterFillCeiling, AWidenedStringSitsOnTheSameEdge)
{
    expect_edge<wchar_t>("ab", 2);
}

TEST(OstreamInsertCharacterFillCeiling, AStdStringSitsOnTheEdge)
{
    expect_edge<char>(std::string("abc"), 3);
    expect_edge<wchar_t>(std::wstring(L"abc"), 3);
}

// The edge is on the fill count, not the width: a longer value moves the
// admissible width up by exactly its length.
TEST(OstreamInsertCharacterFillCeiling, TheEdgeMovesWithTheValueLength)
{
    const std::string longer(1000, 'q');
    expect_edge<char>(longer, 1000);
    expect_edge<wchar_t>(longer.c_str(), 1000);
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The widening path at its degenerate lengths.
 *
 * Writing a narrow string to a wide stream goes through
 * `io_traits<TChar, char*>`, which widens into a `std::vector<TChar>` sized to
 * the input. An empty input therefore makes a vector of size zero, whose
 * `data()` is allowed to be a null pointer -- and that null is what then
 * reaches `ostream_insert` as the start of a zero-length range.
 *
 * Nothing here is undefined: `nullptr + 0` is a well-formed pointer value, and
 * the copy is a loop over an empty range rather than a `memmove` from null.
 * But the reasoning is subtle enough that it was only ever checked by hand, so
 * an empty narrow string on a wide stream -- with and without a field width --
 * is worth a case of its own. Under a sanitizer build this is also where a
 * regression would surface.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <string>
#include <type_traits>

#include <gtest/gtest.h>

namespace
{
template <typename C>
using os_t = IOv2::ostream<IOv2::mem_device<C>, C>;

// char8_t is UTF-8 by definition, so its collate facet refuses to build over a
// non-UTF-8 locale -- "C" throws where it works for every other character type.
template <typename C>
os_t<C> make_stream()
{
    if constexpr (std::is_same_v<C, char8_t>)
        return os_t<C>{IOv2::mem_device<C>{}, IOv2::locale<C>("C.UTF-8")};
    else
        return os_t<C>{IOv2::mem_device<C>{}, IOv2::locale<C>("C")};
}
}

TEST(OstreamInsertCharacterWideningEdges, AnEmptyNarrowStringWritesNothingOnAWideStream)
{
    auto os = make_stream<wchar_t>();
    os << "";

    EXPECT_FALSE(os.str_fail());
    EXPECT_TRUE(os.device().str().empty());
}

TEST(OstreamInsertCharacterWideningEdges, AnEmptyNarrowStringStillHonoursTheFieldWidth)
{
    // The padding is computed from a length of zero, so the whole field is
    // fill -- and the zero-length copy still runs between the two runs of it.
    auto os = make_stream<wchar_t>();
    os << IOv2::setw(5) << "";

    EXPECT_FALSE(os.str_fail());
    EXPECT_EQ(os.device().str(), std::wstring(5, L' '));
    EXPECT_EQ(os.width(), 0u);
}

TEST(OstreamInsertCharacterWideningEdges, ASingleCharacterIsTheOtherEndOfTheSameRange)
{
    auto os = make_stream<wchar_t>();
    os << "x";

    EXPECT_FALSE(os.str_fail());
    EXPECT_EQ(os.device().str(), std::wstring(L"x"));
}

TEST(OstreamInsertCharacterWideningEdges, TheSamePathHoldsForEveryWideCharacterType)
{
    {
        auto os = make_stream<char8_t>();
        os << "" << "ok";
        EXPECT_FALSE(os.str_fail());
        EXPECT_EQ(os.device().str(), std::u8string(u8"ok"));
    }
    {
        auto os = make_stream<char32_t>();
        os << "" << "ok";
        EXPECT_FALSE(os.str_fail());
        EXPECT_EQ(os.device().str(), std::u32string(U"ok"));
    }
}

TEST(OstreamInsertCharacterWideningEdges, ANarrowStreamTakesTheByteWiseRouteInstead)
{
    // No widening is involved when the stream's character type already
    // matches, so the empty case never builds a vector at all.
    auto os = make_stream<char>();
    os << "" << IOv2::setw(3) << "";

    EXPECT_FALSE(os.str_fail());
    EXPECT_EQ(os.device().str(), "   ");
}

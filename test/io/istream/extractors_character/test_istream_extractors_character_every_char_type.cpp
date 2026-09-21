// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The three character extractors behave identically on every character type.
 *
 * The extractors in char_and_str.h are templates on the stream's character
 * type, so nothing in them is written twice -- but the ctype facet they ask
 * for whitespace is, per type, and so is the locale that has to be able to
 * build it (a char8_t stream needs a UTF-8 locale where the others take "C").
 * The char stream is the one compared against libstdc++ elsewhere; this file
 * pins that wchar_t, char8_t and char32_t streams give the same answer as the
 * char stream in every cell of the same matrix: what state the extraction
 * left, what it wrote (including what it left untouched), and where it left
 * the read position.
 *
 * The inputs are ASCII so that the same text can be spelled in every type by
 * widening it a character at a time; the matrix is inputs x widths x skipws x
 * the three forms, which is what "the read side, all four types" meant when
 * it was owed.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace
{
template <typename C>
using is_t = IOv2::istream<IOv2::mem_device<C>, C>;

template <typename C>
std::basic_string<C> widen(const std::string& s)
{
    std::basic_string<C> out(s.size(), C{});
    for (std::size_t i = 0; i < s.size(); ++i)
        out[i] = static_cast<C>(static_cast<unsigned char>(s[i]));
    return out;
}

template <typename C>
std::string narrow(const C* p, std::size_t n)
{
    std::string out(n, '\0');
    for (std::size_t i = 0; i < n; ++i)
        out[i] = static_cast<char>(p[i]);
    return out;
}

template <typename C>
is_t<C> stream_over(const std::string& text)
{
    if constexpr (std::is_same_v<C, char8_t>)
        return is_t<C>{IOv2::mem_device<C>{widen<C>(text)}, IOv2::locale<C>("C.UTF-8")};
    else
        return is_t<C>{IOv2::mem_device<C>{widen<C>(text)}, IOv2::locale<C>("C")};
}

// A byte the extractors never write, so "untouched" is visible in the value.
constexpr char k_unwritten = '\x5a';

// What one extraction left behind: the state, the target (as narrow text, the
// untouched bytes included), and the read position it stopped at.
using outcome = std::tuple<int, std::string, std::size_t>;

template <typename C>
outcome after(is_t<C>& is)
{
    const int state = static_cast<int>(is.rdstate());
    is.clear();
    const auto pos = is.tell();
    return {state, {}, pos.has_value() ? pos.value() : static_cast<std::size_t>(-1)};
}

template <typename C>
outcome extract_char(const std::string& text, std::ptrdiff_t width, bool skipws)
{
    auto is = stream_over<C>(text);
    if (!skipws) is.unsetf(IOv2::ios_defs::skipws);
    C c = static_cast<C>(k_unwritten);
    is >> IOv2::setw(width) >> c;
    auto o = after(is);
    std::get<1>(o) = narrow(&c, 1);
    return o;
}

template <typename C>
outcome extract_array(const std::string& text, std::ptrdiff_t width, bool skipws)
{
    auto is = stream_over<C>(text);
    if (!skipws) is.unsetf(IOv2::ios_defs::skipws);
    C buf[5];
    for (C& c : buf) c = static_cast<C>(k_unwritten);
    is >> IOv2::setw(width) >> buf;
    auto o = after(is);
    std::get<1>(o) = narrow(buf, 5);
    return o;
}

template <typename C>
outcome extract_string(const std::string& text, std::ptrdiff_t width, bool skipws)
{
    auto is = stream_over<C>(text);
    if (!skipws) is.unsetf(IOv2::ios_defs::skipws);
    std::basic_string<C> s = widen<C>("PREVIOUS");
    is >> IOv2::setw(width) >> s;
    auto o = after(is);
    std::get<1>(o) = narrow(s.data(), s.size());
    return o;
}

const std::vector<std::string> k_inputs = {
    "", "   ", "a", "  abc", "abc def", "abcdefghij", "abc\tdef", "  \n  x", "abcd", "abcde",
};
const std::vector<std::ptrdiff_t> k_widths = {0, 1, 2, 3, 4, 5, 8, 20};

template <typename C>
void expect_same_as_char()
{
    for (const auto& text : k_inputs)
        for (const auto width : k_widths)
            for (const bool skipws : {true, false})
            {
                SCOPED_TRACE(testing::Message() << "sizeof(C)=" << sizeof(C) << " input=\"" << text
                                                << "\" width=" << width << " skipws=" << skipws);
                EXPECT_EQ(extract_char<C>(text, width, skipws), extract_char<char>(text, width, skipws));
                EXPECT_EQ(extract_array<C>(text, width, skipws), extract_array<char>(text, width, skipws));
                EXPECT_EQ(extract_string<C>(text, width, skipws), extract_string<char>(text, width, skipws));
            }
}
}

TEST(IstreamExtractCharacterEveryCharType, AWcharStreamAgreesWithTheCharStreamCellByCell)
{
    expect_same_as_char<wchar_t>();
}

TEST(IstreamExtractCharacterEveryCharType, AChar8StreamAgreesWithTheCharStreamCellByCell)
{
    expect_same_as_char<char8_t>();
}

TEST(IstreamExtractCharacterEveryCharType, AChar32StreamAgreesWithTheCharStreamCellByCell)
{
    expect_same_as_char<char32_t>();
}

// The matrix is only worth something if its cells are not all alike: the char
// baseline has to visit every exit, or agreement would be trivial.
TEST(IstreamExtractCharacterEveryCharType, TheBaselineVisitsEveryExit)
{
    namespace ios_defs = IOv2::ios_defs;

    // Sentry failure on empty input: nothing written, position untouched.
    EXPECT_EQ(extract_array<char>("", 0, true), outcome(ios_defs::strfailbit | ios_defs::eofbit, std::string(5, k_unwritten), 0));
    // Whitespace only with skipws: skipped to the end, then failed.
    EXPECT_EQ(std::get<0>(extract_char<char>("   ", 0, true)), ios_defs::strfailbit | ios_defs::eofbit);
    // A token shorter than the array: terminated, delimiter left in place.
    EXPECT_EQ(extract_array<char>("abc def", 0, true), outcome(ios_defs::goodbit, std::string("abc\0", 4) + k_unwritten, 3));
    // A token longer than the array: cut at N - 1, the rest still there.
    EXPECT_EQ(extract_array<char>("abcdefghij", 0, true), outcome(ios_defs::goodbit, std::string("abcd\0", 5), 4));
    // The width tightens the bound below the array's.
    EXPECT_EQ(extract_array<char>("abcdefghij", 3, true), outcome(ios_defs::goodbit, std::string("ab\0", 3) + std::string(2, k_unwritten), 2));
    // The string form has no bound but the width; running into the end is eofbit, not a failure.
    EXPECT_EQ(extract_string<char>("abcdefghij", 0, true), outcome(ios_defs::eofbit, "abcdefghij", 10));
    EXPECT_EQ(extract_string<char>("abcdefghij", 4, true), outcome(ios_defs::goodbit, "abcd", 4));
    // Without skipws, leading whitespace is where a character extraction stops.
    EXPECT_EQ(extract_char<char>("  abc", 0, false), outcome(ios_defs::goodbit, " ", 1));
    EXPECT_EQ(std::get<0>(extract_string<char>("  abc", 0, false)), ios_defs::strfailbit);
}

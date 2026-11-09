// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Reading through newline_cvt: `\r\n` becomes `\n`, nothing else changes.
 *
 * A lone `\r` and a `\r` that ends the data are kept, so a file that is not
 * pure CRLF still reads back byte for byte apart from its line ends. The cases
 * are read with several request sizes, because a `\r\n` split across two
 * requests -- or across the converter's own 64-character stretches -- is where
 * a held `\r` has to be decided later rather than on the spot.
 */
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/newline_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
    template <typename CharT>
    auto open_for_read(std::basic_string<CharT> src)
    {
        auto c = newline_cvt_creator{}.create(rb_root_cvt{mem_device<CharT>{std::move(src)}});
        c.bos();
        c.main_cont_beg();
        return c;
    }

    // Reads everything in requests of `step` characters.
    template <typename Cvt>
    auto read_all(Cvt& c, std::size_t step)
    {
        std::basic_string<typename Cvt::internal_type> out;
        std::basic_string<typename Cvt::internal_type> buf(step, {});
        while (const std::size_t n = c.get(buf.data(), step))
            out.append(buf.data(), n);
        return out;
    }

    constexpr std::size_t kSteps[] = {1, 2, 3, 64, 65, 200};
}

TEST(NewlineCvtRead, TheCreatorDeducesTheCharacterType)
{
    auto c = newline_cvt_creator{}.create(rb_root_cvt{mem_device<char>{""}});
    static_assert(std::is_same_v<decltype(c), newline_cvt<rb_root_cvt<mem_device<char>>>>);
    static_assert(io_converter<decltype(c)>);
    static_assert(cvt_creator<newline_cvt_creator>);
}

TEST(NewlineCvtRead, OnlyCrLfIsTranslated)
{
    struct Case { const char* in; const char* want; };
    const Case cases[] = {
        {"a\r\nb",            "a\nb"},
        {"a\rb",              "a\rb"},      // a lone \r is kept
        {"a\r",               "a\r"},       // so is a \r that ends the data
        {"\r",                "\r"},
        {"\r\n",              "\n"},
        {"\r\r\n",            "\r\n"},
        {"a\nb",              "a\nb"},      // LF-only stays as it is
        {"a\nb\r\n\r\nc\r",   "a\nb\n\nc\r"},
        {"\x1a\r\n",          "\x1a\n"},    // 0x1A is ordinary data
        {"",                  ""},
    };
    for (const auto& t : cases)
        for (const std::size_t step : kSteps)
        {
            auto c = open_for_read<char>(t.in);
            EXPECT_EQ(read_all(c, step), t.want) << "step " << step;
            EXPECT_TRUE(c.is_eof());
        }
}

// The converter takes at most 64 characters from below at a time; a \r\n split
// by that boundary still reads as one \n.
TEST(NewlineCvtRead, CrLfAcrossTheInternalStretch)
{
    for (const std::size_t at : {62u, 63u, 64u, 127u})
    {
        std::string in(at, 'x');
        in += "\r\n";
        in += std::string(70, 'y');
        in += "\r\nz";
        std::string want(at, 'x');
        want += "\n";
        want += std::string(70, 'y');
        want += "\nz";
        for (const std::size_t step : {1u, 7u, 64u, 65u, 500u})
        {
            auto c = open_for_read<char>(in);
            EXPECT_EQ(read_all(c, step), want) << "at " << at << ", step " << step;
        }
    }
}

// A \r at the end of what has arrived is not delivered until the next character
// decides it; until then it is not at eof either.
TEST(NewlineCvtRead, AHeldCrIsNotEof)
{
    auto c = open_for_read<char>("a\r");
    char ch = 0;
    ASSERT_EQ(c.get(&ch, 1), 1u);
    EXPECT_EQ(ch, 'a');
    EXPECT_FALSE(c.is_eof());
    ASSERT_EQ(c.get(&ch, 1), 1u);
    EXPECT_EQ(ch, '\r');
    EXPECT_TRUE(c.is_eof());
}

// Room for a single character, and a \r followed by something else: only the \r
// goes out, the other character waits for the next call.
TEST(NewlineCvtRead, ACrFollowedByAnotherCharacterWithRoomForOne)
{
    auto c = open_for_read<char>("\rx");
    char ch = 0;
    ASSERT_EQ(c.get(&ch, 1), 1u);
    EXPECT_EQ(ch, '\r');
    ASSERT_EQ(c.get(&ch, 1), 1u);
    EXPECT_EQ(ch, 'x');
    EXPECT_EQ(c.get(&ch, 1), 0u);
}

TEST(NewlineCvtRead, WideCharacters)
{
    auto c = open_for_read<wchar_t>(L"\x674e\r\n\x4f1f\r");
    EXPECT_EQ(read_all(c, 3), L"\x674e\n\x4f1f\r");
}

// Above a UTF-8 code_cvt the translation happens on characters, after decoding.
TEST(NewlineCvtRead, AboveAVariableLengthEncoding)
{
    auto c = newline_cvt_creator{}.create(
        code_cvt_creator<char8_t, char32_t>{}.create(rb_root_cvt{mem_device<char8_t>{u8"李\r\n伟\r"}}));
    c.bos();
    c.main_cont_beg();
    EXPECT_EQ(read_all(c, 2), U"李\n伟\r");
}

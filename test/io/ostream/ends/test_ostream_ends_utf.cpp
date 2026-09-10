// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ends on the two character types the char and wchar_t files do not reach.
 *
 * What ends writes is one code unit of value zero, and the claim worth repeating per type is that
 * it stays one unit next to multi-byte text -- on char8_t a code unit is a byte, so the two are
 * measured alike and a widening bug would show as a length change.
 *
 * A char8_t locale must be a UTF-8 one: locale<char8_t>("C") throws from collate_conf and a
 * default-constructed one follows the environment, so it throws under LC_ALL=C. "C.UTF-8" is
 * named explicitly for that reason.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(OstreamEndsUtf, EndsWritesOneNullCodeUnit)
{
    {
        auto os = ostream(mem_device{std::u8string()}, locale<char8_t>("C.UTF-8"));
        os << ends;
        EXPECT_TRUE(os.good());
        EXPECT_EQ(os.device().str(), std::u8string(1, u8'\0'));
    }
    {
        auto os = ostream(mem_device{std::u32string()}, locale<char32_t>("C"));
        os << ends;
        EXPECT_TRUE(os.good());
        EXPECT_EQ(os.device().str(), std::u32string(1, U'\0'));
    }
}

TEST(OstreamEndsUtf, EachEndsAddsExactlyOneUnitBesideMultiByteText)
{
    {
        auto os = ostream(mem_device{std::u8string()}, locale<char8_t>("C.UTF-8"));
        os << u8"中" << ends << u8"é" << ends;

        const std::u8string want = std::u8string(u8"中") + u8'\0' + std::u8string(u8"é") + u8'\0';
        EXPECT_EQ(os.device().str(), want);
        EXPECT_EQ(os.device().str().size(), 3u + 1u + 2u + 1u);
    }
    {
        auto os = ostream(mem_device{std::u32string()}, locale<char32_t>("C"));
        os << U"中" << ends << U"é" << ends;

        const std::u32string want = std::u32string(U"中") + U'\0' + std::u32string(U"é") + U'\0';
        EXPECT_EQ(os.device().str(), want);
        EXPECT_EQ(os.device().str().size(), 4u);
    }
}

TEST(OstreamEndsUtf, EndsReturnsTheStreamSoItChains)
{
    auto os = ostream(mem_device{std::u8string()}, locale<char8_t>("C.UTF-8"));

    os << ends << ends << ends;

    EXPECT_TRUE(os.good());
    EXPECT_EQ(os.device().str().size(), 3u);
}

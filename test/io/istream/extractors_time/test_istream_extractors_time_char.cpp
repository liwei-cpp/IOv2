// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char> >> std::tm: reading a broken-down time back out of a stream.
 *
 * The extractor's format is the locale's %c, extended with %z and (%Z) on the
 * platforms whose std::tm has somewhere to put them.  So what the input carries
 * depends on the platform, and the point of the case below is that whatever the
 * format asks for reaches the tm rather than being parsed and dropped.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/tm.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <ctime>
#include <string>
#include <string_view>

using namespace IOv2;

TEST(IstreamExtractorsTimeChar, ABrokenDownTimeIsReadWholeFromTheStream)
{
    // Both stream shapes reach the same extractor, so both are asked.
    auto expect_extracted = []<template <typename, typename> class T>()
    {
        // The format asks for an offset and a zone only where this tm can hold them
        // (time_value_fields<std::tm>): glibc both, macOS the offset alone, the UCRT neither.
        std::string in = "Wed Sep  4 13:33:18 2024";
        if constexpr (time_value_fields<std::tm>::has_offset)
            in += " +0800";
        if constexpr (time_value_fields<std::tm>::has_zone)
            in += " (CST)";

        std::tm tp{};
        T f(mem_device{in}, locale<char>("C"));

        f >> tp;
        EXPECT_TRUE(static_cast<bool>(f));
        EXPECT_EQ(tp.tm_year, 2024 - 1900);
        EXPECT_EQ(tp.tm_mon, 9 - 1);
        EXPECT_EQ(tp.tm_mday, 4);
        EXPECT_EQ(tp.tm_hour, 13);
        EXPECT_EQ(tp.tm_min, 33);
        EXPECT_EQ(tp.tm_sec, 18);

        // The fields the format only asks for because this tm can hold them.
        if constexpr (time_value_fields<std::tm>::has_offset)
        {
            EXPECT_EQ(detail::tm_get_gmtoff(tp).count(), 8 * 3600);
        }
        if constexpr (time_value_fields<std::tm>::has_zone)
        {
            ASSERT_NE(detail::tm_get_zone(tp), nullptr);
            EXPECT_EQ(std::string_view(detail::tm_get_zone(tp)), "CST");
        }
    };

    expect_extracted.template operator()<istream>();
    expect_extracted.template operator()<iostream>();
}

TEST(IstreamExtractorsTimeChar, AMissingTimeFacetLeavesTheTmUnchanged)
{
    const auto loc = locale<char>("C").remove<timeio_conf<char>>();

    std::tm value{};
    value.tm_year = -1901;
    value.tm_mon = -1;
    value.tm_mday = 1;

    istream is{mem_device{"Wed Sep  4 13:33:18 2024"}, loc};
    is >> value;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(value.tm_year, -1901);
    EXPECT_EQ(value.tm_mon, -1);
    EXPECT_EQ(value.tm_mday, 1);
}

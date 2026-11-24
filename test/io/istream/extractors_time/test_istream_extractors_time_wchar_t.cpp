// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same contract as test_istream_extractors_time_char.cpp: a std::tm is
 * filled from the locale's %c, extended with %z and (%Z) where the platform's
 * tm has somewhere to put them.  std::tm is a narrow type whatever the stream
 * is made of, so tm_zone is still a const char* here -- which is the one thing
 * this instantiation is for.
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

TEST(IstreamExtractorsTimeWchar, ABrokenDownTimeIsReadWholeFromTheStream)
{
    // Both stream shapes reach the same extractor, so both are asked.
    auto expect_extracted = []<template <typename, typename> class T>()
    {
        // The format asks for an offset and a zone only where this tm can hold them
        // (time_value_fields<std::tm>): glibc both, macOS the offset alone, the UCRT neither.
        std::wstring in = L"Wed Sep  4 13:33:18 2024";
        if constexpr (time_value_fields<std::tm>::has_offset)
            in += L" +0800";
        if constexpr (time_value_fields<std::tm>::has_zone)
            in += L" (CST)";

        std::tm tp{};
        T f(mem_device{in}, locale<wchar_t>("C"));

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

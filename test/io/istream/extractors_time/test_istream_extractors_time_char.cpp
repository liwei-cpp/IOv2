// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char> >> std::tm: reading a broken-down time back out of a stream.
 *
 * The extractor's format is the locale's %c, extended with %z and (%Z) on the
 * platforms whose std::tm has somewhere to put them.  So what the input carries
 * depends on the platform, and the point of the case below is that whatever the
 * format asks for reaches the tm rather than being parsed and dropped.
 */
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/tm.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <ctime>
#include <string_view>

using namespace IOv2;

TEST(IstreamExtractorsTimeChar, ABrokenDownTimeIsReadWholeFromTheStream)
{
    // Both stream shapes reach the same extractor, so both are asked.
    auto expect_extracted = []<template <typename, typename> class T>()
    {
        std::tm tp{};
#ifdef __USE_MISC
        T f(mem_device{"Wed Sep  4 13:33:18 2024 +0800 (CST)"}, locale<char>("C"));
#else
        T f(mem_device{"Wed Sep  4 13:33:18 2024"}, locale<char>("C"));
#endif

        f >> tp;
        EXPECT_TRUE(static_cast<bool>(f));
        EXPECT_EQ(tp.tm_year, 2024 - 1900);
        EXPECT_EQ(tp.tm_mon, 9 - 1);
        EXPECT_EQ(tp.tm_mday, 4);
        EXPECT_EQ(tp.tm_hour, 13);
        EXPECT_EQ(tp.tm_min, 33);
        EXPECT_EQ(tp.tm_sec, 18);

#ifdef __USE_MISC
        // The two fields the format only asks for because this tm can hold them.
        EXPECT_EQ(tp.tm_gmtoff, 8 * 3600);
        ASSERT_NE(tp.tm_zone, nullptr);
        EXPECT_EQ(std::string_view(tp.tm_zone), "CST");
#endif
    };

    expect_extracted.template operator()<istream>();
    expect_extracted.template operator()<iostream>();
}

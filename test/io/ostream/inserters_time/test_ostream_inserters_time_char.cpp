// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Inserting a std::tm into an ostream<char>.
 *
 * The inserter writes the time facet's default format, and IOv2 extends that
 * format so a written tm can be read back whole: where the platform's tm
 * carries tm_gmtoff and tm_zone, the offset and the zone name are appended.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/tm.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <ctime>
#include <string>

using namespace IOv2;

TEST(OstreamInsertTimeChar, ATmIsWrittenInTheDefaultFormatOfTheCLocale)
{
    auto helper = []<template <typename, typename> class T>()
    {
        std::tm tp{};
        tp.tm_year = 2024 - 1900;
        tp.tm_mon  = 9 - 1;
        tp.tm_mday = 4;
        tp.tm_hour = 13;
        tp.tm_min  = 33;
        tp.tm_sec  = 18;

        T f(mem_device{""}, locale<char>("C"));
        f << tp;
        EXPECT_TRUE(static_cast<bool>(f));

        auto [dev, err] = f.detach();

        // A tm with no zone still writes the unknown-zone token, which reads
        // back as an empty tm_zone rather than as nothing at all.
#ifdef __USE_MISC
        EXPECT_EQ(dev.str(), "Wed Sep  4 13:33:18 2024 +0000 (UNKNOWN)");
#else
        EXPECT_EQ(dev.str(), "Wed Sep  4 13:33:18 2024");
#endif
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertTimeChar, AMissingTimeFacetRejectsATm)
{
    const auto loc = locale<char>("C").remove<timeio_conf<char>>();
    std::tm    value{};
    value.tm_year = 2024 - 1900;
    value.tm_mon = 1;
    value.tm_mday = 29;

    ostream os{mem_device{""}, loc};
    os.width(10);
    os << value;

    EXPECT_TRUE(os.str_fail());
    EXPECT_TRUE(os.device().str().empty());
    // The width is one-shot: the failing path has to spend it too.
    EXPECT_EQ(os.width(), 0u);
}

TEST(OstreamInsertTimeChar, TheFieldWidthPadsATmAndIsThenConsumed)
{
    const auto loc = locale<char>("C");
    std::tm    value{};
    value.tm_year = 2024 - 1900;
    value.tm_mon  = 9 - 1;
    value.tm_mday = 4;
    value.tm_hour = 13;
    value.tm_min  = 33;
    value.tm_sec  = 18;

    // The unpadded rendering is locale- and platform-dependent, so measure it.
    ostream probe{mem_device{""}, loc};
    probe << value;
    const std::string plain = probe.detach().first.str();

    auto write = [&](std::size_t w, ios_defs::fmtflags adjust)
    {
        ostream os{mem_device{""}, loc};
        os.fill('.');
        os.setf(adjust, ios_defs::adjustfield);
        os.width(w);
        os << value;
        EXPECT_TRUE(static_cast<bool>(os));
        EXPECT_EQ(os.width(), 0u);
        return os.detach().first.str();
    };

    EXPECT_EQ(write(plain.size() + 4, ios_defs::right), "...." + plain);
    EXPECT_EQ(write(plain.size() + 4, ios_defs::left), plain + "....");
    EXPECT_EQ(write(plain.size(), ios_defs::right), plain);
    EXPECT_EQ(write(1, ios_defs::right), plain);
}

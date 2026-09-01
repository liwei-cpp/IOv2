/**
 * Inserting a std::tm into an ostream<wchar_t>.
 *
 * The inserter writes the time facet's default format, and IOv2 extends that
 * format so a written tm can be read back whole: where the platform's tm
 * carries tm_gmtoff and tm_zone, the offset and the zone name are appended.
 */
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/ostream.h>
#include <io/traits/tm.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <ctime>
#include <string>

using namespace IOv2;

namespace
{
    // Static so that the members this helper does not name -- tm_gmtoff and
    // tm_zone on glibc -- start out zeroed rather than indeterminate.
    std::tm make_tm(int sec, int min, int hour, int mday, int mon, int year,
                    int wday, int yday, int isdst)
    {
        static std::tm tmp;
        tmp.tm_sec   = sec;
        tmp.tm_min   = min;
        tmp.tm_hour  = hour;
        tmp.tm_mday  = mday;
        tmp.tm_mon   = mon;
        tmp.tm_year  = year;
        tmp.tm_wday  = wday;
        tmp.tm_yday  = yday;
        tmp.tm_isdst = isdst;
        return tmp;
    }
}

TEST(OstreamInsertTimeWchar, ATmIsWrittenInTheDefaultFormatOfTheCLocale)
{
    auto helper = []<template <typename, typename> class T>()
    {
        const std::tm tp = make_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 0, 0, 0);

        T f(mem_device{L""}, locale<wchar_t>("C"));
        f << tp;
        EXPECT_TRUE(static_cast<bool>(f));

        auto [dev, err] = f.detach();

        // A tm with no zone still writes the unknown-zone token, which reads
        // back as an empty tm_zone rather than as nothing at all.
#ifdef __USE_MISC
        EXPECT_EQ(dev.str(), L"Wed Sep  4 13:33:18 2024 +0000 (UNKNOWN)");
#else
        EXPECT_EQ(dev.str(), L"Wed Sep  4 13:33:18 2024");
#endif
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

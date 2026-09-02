// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * put_time and get_time.
 *
 * Both hand a std::tm and a format string to the time facet, so every
 * conversion specifier and every name is the locale's; what the manipulators
 * own is only that the format reaches the facet unchanged and that the result
 * is reported through the stream.
 *
 * get_time is the larger half, because parsing has to decide what happens to
 * the fields the format does not mention. IOv2 leaves them as the caller left
 * them rather than taking them from the wall clock, recomputes tm_wday and
 * tm_yday from the resulting date, and normalises out-of-range fields with
 * std::chrono so that no TZ or DST state can perturb the answer.
 */
#include <common/defs.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/traits/tm.h>
#include <locale/locale.h>

#include <support/io_traits_probe.h>

#include <gtest/gtest.h>

#include <ctime>
#include <string>
#include <string_view>

using namespace IOv2;

namespace
{
    // Value-initialisation also clears any platform-specific trailing fields.
    std::tm calendar_time(int year, int month, int day, int hour, int minute, int second,
                          int weekday, int yearday, int daylight)
    {
        std::tm tmp{};
        tmp.tm_year  = year;
        tmp.tm_mon   = month;
        tmp.tm_mday  = day;
        tmp.tm_hour  = hour;
        tmp.tm_min   = minute;
        tmp.tm_sec   = second;
        tmp.tm_wday  = weekday;
        tmp.tm_yday  = yearday;
        tmp.tm_isdst = daylight;
        return tmp;
    }

    bool have_locale(const char* name)
    {
        try { (void)locale<char>(name); }
        catch (const cvt_error&) { return false; }
        return true;
    }

    // 2024-02-29 was a Thursday: a leap day, so a format that reaches the date
    // arithmetic at all has somewhere to go wrong.
    std::tm leap_day() { return calendar_time(124, 1, 29, 12, 0, 0, 4, 59, 0); }
}

// The format reaches the facet unchanged, specifier for specifier.
TEST(IoBaseManipTime, PutTimeWritesWhateverTheFormatAsksFor)
{
    const std::tm when = leap_day();

    {
        ostream oss{mem_device{""}, locale<char>("C")};
        oss << put_time(&when, "%Y-%m-%d");
        auto [dev, err] = oss.detach();
        EXPECT_EQ(dev.str(), "2024-02-29");
    }
    {
        ostream oss{mem_device{""}, locale<char>("C")};
        oss << put_time(&when, "%A %H:%M");
        auto [dev, err] = oss.detach();
        EXPECT_EQ(dev.str(), "Thursday 12:00");
    }
}

// The names are the locale's, not the library's.
TEST(IoBaseManipTime, TheNamesComeFromTheLocale)
{
    if (!have_locale("de_DE.UTF-8"))
        GTEST_SKIP() << "de_DE.UTF-8 is not installed here";

    const std::tm when = leap_day();

    ostream oss{mem_device{""}, locale<char>("de_DE.UTF-8")};
    oss << put_time(&when, "%A, %d.%m.%Y");
    auto [dev, err] = oss.detach();
    EXPECT_EQ(dev.str(), "Donnerstag, 29.02.2024");
}

TEST(IoBaseManipTime, PutTimeBehavesTheSameOnAWideStream)
{
    const std::tm when = leap_day();

    ostream oss{mem_device{L""}, locale<wchar_t>("C")};
    oss << put_time(&when, L"%A %Y-%m-%d");
    auto [dev, err] = oss.detach();
    EXPECT_EQ(dev.str(), L"Thursday 2024-02-29");
}

// The locale's names and the wide path cross here.  A German March is the
// interesting case: "Marz" carries an umlaut, so the name only survives if it
// is widened as a character rather than copied as UTF-8 bytes -- the length is
// what says which happened.
TEST(IoBaseManipTime, AWideStreamWidensTheLocalesNamesRatherThanItsBytes)
{
    if (!have_locale("de_DE.UTF-8"))
        GTEST_SKIP() << "de_DE.UTF-8 is not installed here";

    // 2024-03-31 was a Sunday, and March is the month whose German name is
    // spelled with a character outside ASCII.
    const std::tm when = calendar_time(124, 2, 31, 12, 0, 0, 0, 90, 0);

    ostream oss{mem_device{L""}, locale<wchar_t>("de_DE.UTF-8")};
    oss << put_time(&when, L"%A %B %Y");
    auto [dev, err] = oss.detach();

    EXPECT_EQ(dev.str(), L"Sonntag M\u00e4rz 2024");
    // Four characters, not the five bytes the UTF-8 spelling would occupy.
    EXPECT_EQ(dev.str().size(), 17u);
}

TEST(IoBaseManipTime, GetTimeReadsBackWhatPutTimeWrote)
{
    // Round-trips put_time. Exercises the idiomatic rvalue form `is >> get_time(...)`,
    // which needs the by-value operator>> overload.
    istream iss{mem_device{std::string("1971-04-04 12:34:56")},
                      locale<char>("C")};

    std::tm parsed{};
    iss >> get_time(&parsed, "%Y-%m-%d %H:%M:%S");
    EXPECT_TRUE(static_cast<bool>(iss));
    EXPECT_FALSE(iss.str_fail());
    EXPECT_EQ(parsed.tm_year, 71);
    EXPECT_EQ(parsed.tm_mon, 3);
    EXPECT_EQ(parsed.tm_mday, 4);
    EXPECT_EQ(parsed.tm_hour, 12);
    EXPECT_EQ(parsed.tm_min, 34);
    EXPECT_EQ(parsed.tm_sec, 56);
    // Derived by the context, not present in the input.
    EXPECT_EQ(parsed.tm_wday, 0);
    EXPECT_EQ(parsed.tm_yday, 93);
}

TEST(IoBaseManipTime, ANullTargetOrFormatIsReportedNotDereferenced)
{
    // Null tm / format pointers are reported as a stream failure rather than
    // dereferenced; the target must be left untouched.
    std::tm parsed{};
    parsed.tm_year = 42;

    istream iss{mem_device{std::string("1971-04-04 12:34:56")},
                      locale<char>("C")};
    iss >> get_time(&parsed, static_cast<const char*>(nullptr));
    EXPECT_TRUE(iss.str_fail());
    EXPECT_EQ(parsed.tm_year, 42);

    istream iss2{mem_device{std::string("1971-04-04 12:34:56")},
                       locale<char>("C")};
    iss2 >> get_time(static_cast<std::tm*>(nullptr), "%Y-%m-%d %H:%M:%S");
    EXPECT_TRUE(iss2.str_fail());

    // The null check lives in io_traits<...>::sread, which the extraction operator calls after
    // building its sentry, so the leading whitespace skipws consumes is gone by the time the
    // failure is reported. Nothing else is consumed and nothing is written, so a clear() and a
    // retry still see the whole value.
    for (int null_fmt = 0; null_fmt < 2; ++null_fmt)
    {
        std::tm target{};
        istream iss3{mem_device{std::string("   1971-04-04")},
                           locale<char>("C")};
        EXPECT_EQ(iss3.tell(), 0);

        if (null_fmt != 0)
            iss3 >> get_time(&target, static_cast<const char*>(nullptr));
        else
            iss3 >> get_time(static_cast<std::tm*>(nullptr), "%Y-%m-%d");

        EXPECT_TRUE(iss3.str_fail());
        iss3.clear();
        EXPECT_EQ(iss3.tell(), 3);

        // Retrying from that position still sees the whole input.
        iss3 >> get_time(&target, "%Y-%m-%d");
        EXPECT_FALSE(iss3.str_fail());
        EXPECT_EQ(target.tm_year, 71);
        EXPECT_EQ(target.tm_mon, 3);
        EXPECT_EQ(target.tm_mday, 4);
    }
}

TEST(IoBaseManipTime, GetTimeBehavesTheSameOnAWideStream)
{
    istream iss{mem_device{std::wstring(L"1971-04-04 12:34:56")},
                      locale<wchar_t>("C")};

    std::tm parsed{};
    iss >> get_time(&parsed, L"%Y-%m-%d %H:%M:%S");
    EXPECT_TRUE(static_cast<bool>(iss));
    EXPECT_FALSE(iss.str_fail());
    EXPECT_EQ(parsed.tm_year, 71);
    EXPECT_EQ(parsed.tm_mon, 3);
    EXPECT_EQ(parsed.tm_mday, 4);
    EXPECT_EQ(parsed.tm_hour, 12);
    EXPECT_EQ(parsed.tm_min, 34);
    EXPECT_EQ(parsed.tm_sec, 56);
}

TEST(IoBaseManipTime, FieldsTheFormatDoesNotMentionKeepTheirPreviousValues)
{
    // Fields the format string does not parse keep the values the target already held,
    // instead of being taken from the wall clock.
    const std::tm preset = calendar_time(120, 4, 17, 9, 8, 7, 0, 0, 1);   // 2020-05-17 09:08:07

    // Time only: the date is untouched.
    {
        std::tm parsed = preset;
        istream iss{mem_device{std::string("23:45")}, locale<char>("C")};
        iss >> get_time(&parsed, "%H:%M");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 120);
        EXPECT_EQ(parsed.tm_mon, 4);
        EXPECT_EQ(parsed.tm_mday, 17);
        EXPECT_EQ(parsed.tm_hour, 23);
        EXPECT_EQ(parsed.tm_min, 45);
        EXPECT_EQ(parsed.tm_sec, 7);
    }

    // Date only: the time is untouched.
    {
        std::tm parsed = preset;
        istream iss{mem_device{std::string("1999-12-31")}, locale<char>("C")};
        iss >> get_time(&parsed, "%Y-%m-%d");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 99);
        EXPECT_EQ(parsed.tm_mon, 11);
        EXPECT_EQ(parsed.tm_mday, 31);
        EXPECT_EQ(parsed.tm_hour, 9);
        EXPECT_EQ(parsed.tm_min, 8);
        EXPECT_EQ(parsed.tm_sec, 7);
    }

    // Month and day only: the year is untouched.
    {
        std::tm parsed = preset;
        istream iss{mem_device{std::string("03/04")}, locale<char>("C")};
        iss >> get_time(&parsed, "%m/%d");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 120);
        EXPECT_EQ(parsed.tm_mon, 2);
        EXPECT_EQ(parsed.tm_mday, 4);
    }

    // Two extractions accumulate into one tm: the first one's result is not wiped by the
    // second one's unparsed fields.
    {
        std::tm parsed = preset;
        istream iss{mem_device{std::string("2001-02-03")}, locale<char>("C")};
        iss >> get_time(&parsed, "%Y-%m-%d");
        istream iss2{mem_device{std::string("11:22:33")}, locale<char>("C")};
        iss2 >> get_time(&parsed, "%H:%M:%S");
        EXPECT_FALSE(iss.str_fail() && !iss2.str_fail());
        EXPECT_EQ(parsed.tm_year, 101);
        EXPECT_EQ(parsed.tm_mon, 1);
        EXPECT_EQ(parsed.tm_mday, 3);
        EXPECT_EQ(parsed.tm_hour, 11);
        EXPECT_EQ(parsed.tm_min, 22);
        EXPECT_EQ(parsed.tm_sec, 33);
    }
}

TEST(IoBaseManipTime, WdayAndYdayAreRecomputedFromTheResultingDate)
{
    const std::tm preset = calendar_time(120, 4, 17, 9, 8, 7, 0, 0, 1);   // 2020-05-17 09:08:07

    // tm_wday / tm_yday are always recomputed from the resulting date, and tm_isdst is
    // always -1: no format specifier carries DST information, and keeping the caller's
    // value would be wrong once the date has been rewritten.
    {
        std::tm parsed = preset;
        istream iss{mem_device{std::string("2026-07-28")}, locale<char>("C")};
        iss >> get_time(&parsed, "%Y-%m-%d");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_wday, 2);
        EXPECT_EQ(parsed.tm_yday, 208);
        EXPECT_EQ(parsed.tm_isdst, -1);
    }

    // A two-digit year keeps following the POSIX century rule; the target's century does
    // not leak into it.
    {
        std::tm parsed = preset;
        istream iss{mem_device{std::string("03")}, locale<char>("C")};
        iss >> get_time(&parsed, "%y");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 2003 - 1900);

        std::tm parsed2 = preset;
        parsed2.tm_year = 1875 - 1900;
        istream iss2{mem_device{std::string("03")}, locale<char>("C")};
        iss2 >> get_time(&parsed2, "%y");
        EXPECT_FALSE(iss2.str_fail());
        EXPECT_EQ(parsed2.tm_year, 2003 - 1900);
    }

    // %C alone leaves the year within the century open, and that one is 0 rather than the
    // target's, matching POSIX strptime: century 18 yields 1800. The month and day, which
    // %C says nothing about at all, do still come from the target.
    {
        std::tm parsed = preset;
        istream iss{mem_device{std::string("18")}, locale<char>("C")};
        iss >> get_time(&parsed, "%C");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 1800 - 1900);
        EXPECT_EQ(parsed.tm_mon, 4);
        EXPECT_EQ(parsed.tm_mday, 17);
    }
}

TEST(IoBaseManipTime, OutOfRangeFieldsAreNormalisedWithoutConsultingTheTimezone)
{
    // Out-of-range tm fields are normalized with std::chrono rather than mktime, so no
    // TZ / DST state can perturb them.
    struct { int year, mon, mday, sec; int exp_year, exp_mon, exp_mday, exp_sec; } cases[] = {
        { 120,  13, 17,  7, 2021,  2, 17,  7 },   // month carries into the year
        { 120,  -1, 17,  7, 2019, 12, 17,  7 },   // and borrows from it
        { 120,   4,  0,  7, 2020,  4, 30,  7 },   // mday 0 is the last day of the prior month
        { 120,   4, 32,  7, 2020,  6,  1,  7 },   // and 32 spills into the next one
        { 120,   4, 17, 60, 2020,  5, 17, 59 },   // a leap second is clamped to 59
    };

    for (const auto& c : cases)
    {
        std::tm parsed = calendar_time(c.year, c.mon, c.mday, 9, 8, c.sec, 0, 0, 0);
        istream iss{mem_device{std::string("09")}, locale<char>("C")};
        iss >> get_time(&parsed, "%H");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, c.exp_year - 1900);
        EXPECT_EQ(parsed.tm_mon, c.exp_mon - 1);
        EXPECT_EQ(parsed.tm_mday, c.exp_mday);
        EXPECT_EQ(parsed.tm_sec, c.exp_sec);
    }

    // A year far outside what std::chrono::year can hold is clamped instead of wrapping.
    {
        std::tm parsed = calendar_time(100000, 4, 17, 9, 8, 7, 0, 0, 0);
        istream iss{mem_device{std::string("09")}, locale<char>("C")};
        iss >> get_time(&parsed, "%H");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 32767 - 1900);
    }

    // A day carried over from the target that does not exist in the parsed month becomes
    // that month's last day. Reusing one tm across records must not fail on record N just
    // because record N-1 happened to land on the 31st.
    struct { int year, mon, mday; const char* in; const char* fmt;
             int exp_year, exp_mon, exp_mday; } yields[] = {
        { 120, 0, 31, "02",   "%m", 2020,  2, 29 },   // Jan 31 -> February in a leap year
        { 119, 0, 31, "02",   "%m", 2019,  2, 28 },   // and in a common one
        { 120, 1, 29, "2021", "%Y", 2021,  2, 28 },   // Feb 29 -> a common year
        { 120, 0, 31, "03",   "%m", 2020,  3, 31 },   // a day that fits is untouched
        { 120, 0, 31, "04",   "%m", 2020,  4, 30 },
    };

    for (const auto& c : yields)
    {
        std::tm parsed = calendar_time(c.year, c.mon, c.mday, 9, 8, 7, 0, 0, 0);
        istream iss{mem_device{std::string(c.in)}, locale<char>("C")};
        iss >> get_time(&parsed, c.fmt);
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, c.exp_year - 1900);
        EXPECT_EQ(parsed.tm_mon, c.exp_mon - 1);
        EXPECT_EQ(parsed.tm_mday, c.exp_mday);
    }

    // A day that really was parsed does not give way: February 31 is reported as a failed
    // extraction and the target is left untouched.
    {
        std::tm parsed = calendar_time(120, 1, 15, 9, 8, 7, 0, 0, 0);
        istream iss{mem_device{std::string("31")}, locale<char>("C")};
        iss >> get_time(&parsed, "%d");
        EXPECT_TRUE(iss.str_fail());
        EXPECT_EQ(parsed.tm_mon, 1);
        EXPECT_EQ(parsed.tm_mday, 15);
    }
}

TEST(IoBaseManipTime, ExtractingATmDirectlyGetsTheSameTreatment)
{
    // `is >> tm` goes through parse_context_type<char, std::tm> and gets the same
    // treatment. The C locale's %c is %a %b %e %H:%M:%S %Y, and the stream format appends
    // %z and (%Z) where the platform's tm carries tm_gmtoff and tm_zone, so the input
    // carries an offset and a zone token too.
    {
        std::tm parsed = calendar_time(120, 4, 17, 9, 8, 7, 0, 0, 1);
#ifdef __USE_MISC
        istream iss{mem_device{std::string("Tue Feb  6 07:08:09 2018 +0530 (IST)")},
                          locale<char>("C")};
#else
        istream iss{mem_device{std::string("Tue Feb  6 07:08:09 2018")},
                          locale<char>("C")};
#endif
        iss >> parsed;
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 118);
        EXPECT_EQ(parsed.tm_mon, 1);
        EXPECT_EQ(parsed.tm_mday, 6);
        EXPECT_EQ(parsed.tm_hour, 7);
        EXPECT_EQ(parsed.tm_min, 8);
        EXPECT_EQ(parsed.tm_sec, 9);
        EXPECT_EQ(parsed.tm_wday, 2);
        EXPECT_EQ(parsed.tm_yday, 36);
        // The point of appending them: the offset and the zone reach the tm rather than
        // being dropped. Each restores its own member; neither substitutes for the other.
#ifdef __USE_MISC
        EXPECT_EQ(parsed.tm_gmtoff, 5 * 3600 + 30 * 60);
        EXPECT_NE(parsed.tm_zone, nullptr);
        EXPECT_EQ(std::string_view(parsed.tm_zone), "IST");
#endif
    }

    // Types without a parse context of their own keep extracting as before.
    {
        int n = 7;
        istream iss{mem_device{std::string("42")}, locale<char>("C")};
        iss >> n;
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(n, 42);
    }
}

TEST(IoBaseManipTime, AnOutOfRangeTimeFieldCarriesIntoTheDate)
{
    // An out-of-range time field in the target carries into the date, exactly as the date
    // group already did. Before the fix the time was reduced modulo 24 hours and the day that
    // fell out was dropped, so 00:00:-5 stayed on the same day instead of moving back one --
    // an error of a full 86400 seconds, reported with the stream still good().
    //
    // The format string is a bare literal, so it parses nothing and every field of the result
    // comes from the normalized fallback. All cases start from 2021-01-01.
    struct { int hour, min, sec;
             int exp_year, exp_mon, exp_mday, exp_hour, exp_min, exp_sec; } carry[] = {
        {  0,  0,      -5, 2020, 12, 31, 23, 59, 55 },   // borrows a day
        {  0, -1,       0, 2020, 12, 31, 23, 59,  0 },
        { -1,  0,       0, 2020, 12, 31, 23,  0,  0 },
        { 24,  0,       0, 2021,  1,  2,  0,  0,  0 },   // carries one
        {  0,  0,     125, 2021,  1,  1,  0,  2,  5 },   // seconds beyond 59 are not truncated
        { 48,  0,       0, 2021,  1,  3,  0,  0,  0 },   // more than one day carries too
        {  0,  0,  -86400, 2020, 12, 31,  0,  0,  0 },   // exactly one day back
        {  0,  0,  -86401, 2020, 12, 30, 23, 59, 59 },   // just past it
        { 23, 59,      59, 2021,  1,  1, 23, 59, 59 },   // the in-range boundary is untouched
    };

    for (const auto& c : carry)
    {
        std::tm parsed = calendar_time(121, 0, 1, c.hour, c.min, c.sec, 0, 0, 0);
        istream iss{mem_device{std::string("|")}, locale<char>("C")};
        iss >> get_time(&parsed, "|");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, c.exp_year - 1900);
        EXPECT_EQ(parsed.tm_mon, c.exp_mon - 1);
        EXPECT_EQ(parsed.tm_mday, c.exp_mday);
        EXPECT_EQ(parsed.tm_hour, c.exp_hour);
        EXPECT_EQ(parsed.tm_min, c.exp_min);
        EXPECT_EQ(parsed.tm_sec, c.exp_sec);
    }

    // The time carry and the date group's own normalization share one carry rather than being
    // applied one after the other: mday 0 (the last day of December) plus 24 hours lands back
    // on January 1.
    {
        std::tm parsed = calendar_time(121, 0, 0, 24, 0, 0, 0, 0, 0);
        istream iss{mem_device{std::string("|")}, locale<char>("C")};
        iss >> get_time(&parsed, "|");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 121);
        EXPECT_EQ(parsed.tm_mon, 0);
        EXPECT_EQ(parsed.tm_mday, 1);
        EXPECT_EQ(parsed.tm_hour, 0);
        EXPECT_EQ(parsed.tm_min, 0);
        EXPECT_EQ(parsed.tm_sec, 0);
    }

    // A leap second is still the one truncation in the time group: 60 becomes 59 and carries
    // nothing, so 23:59:60 stays on its own day.
    {
        std::tm parsed = calendar_time(121, 0, 1, 23, 59, 60, 0, 0, 0);
        istream iss{mem_device{std::string("|")}, locale<char>("C")};
        iss >> get_time(&parsed, "|");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 121);
        EXPECT_EQ(parsed.tm_mon, 0);
        EXPECT_EQ(parsed.tm_mday, 1);
        EXPECT_EQ(parsed.tm_hour, 23);
        EXPECT_EQ(parsed.tm_min, 59);
        EXPECT_EQ(parsed.tm_sec, 59);
    }

    // Parsed fields still win over the fallback: the carry only decides what the format string
    // leaves alone. Here the day has already moved to the 2nd when %H overwrites the hour.
    {
        std::tm parsed = calendar_time(121, 0, 1, 24, 0, 0, 0, 0, 0);
        istream iss{mem_device{std::string("07")}, locale<char>("C")};
        iss >> get_time(&parsed, "%H");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 121);
        EXPECT_EQ(parsed.tm_mon, 0);
        EXPECT_EQ(parsed.tm_mday, 2);
        EXPECT_EQ(parsed.tm_hour, 7);
    }
}

TEST(IoBaseManipTime, AWeekdayIsResolvedAgainstTheFallbackDate)
{
    // Weekday and week-number specifiers against a fallback date. A weekday states only a
    // position within some week, so a date can be rebuilt from it only against a reference.
    // That reference used to be January 1 of the deduced year -- "assume week number is 1" --
    // which threw the fallback's month and day away and landed up to a full year off, with the
    // stream still good(). It is now the fallback date itself, so the result stays within six
    // days of what the caller passed in.
    //
    // All cases start from 2020-05-17, itself a Sunday.
    struct { const char* fmt; const char* input;
             int exp_mon, exp_mday, exp_wday; } wd[] = {
        { "%a",        "Sun",       5, 17, 0 },   // already the right weekday: no movement at all
        { "%A",        "Sunday",    5, 17, 0 },
        { "%w",        "0",         5, 17, 0 },
        { "%u",        "7",         5, 17, 0 },
        { "%a",        "Mon",       5, 18, 1 },   // the nearest one forward, not week 1 of the year
        { "%a",        "Wed",       5, 20, 3 },
        { "%a",        "Sat",       5, 23, 6 },   // six days is the worst case
        { "%U %w",     "20 0",      5, 17, 0 },   // a week number pins the week down on its own
        { "%W %w",     "20 0",      5, 24, 0 },   // %W weeks start on Monday, so week 20 ends later
        { "%V %a",     "20 Sun",    5, 17, 0 },   // ISO week 20 of 2020 really is 05-11..05-17
        { "%V %u",     "20 7",      5, 17, 0 },
        { "%d %a",     "09 Wed",    5,  9, 6 },   // an explicit day wins; the weekday is inert
        { "%m %a",     "02 Sun",    2, 17, 1 },   // so does an explicit month
    };

    for (const auto& c : wd)
    {
        std::tm parsed = calendar_time(120, 4, 17, 9, 8, 7, 0, 0, 0);    // 2020-05-17 09:08:07
        istream iss{mem_device{std::string(c.input)}, locale<char>("C")};
        iss >> get_time(&parsed, c.fmt);
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 120);
        EXPECT_EQ(parsed.tm_mon, c.exp_mon - 1);
        EXPECT_EQ(parsed.tm_mday, c.exp_mday);
        EXPECT_EQ(parsed.tm_wday, c.exp_wday);
        EXPECT_EQ(parsed.tm_hour, 9);
        EXPECT_EQ(parsed.tm_min, 8);
        EXPECT_EQ(parsed.tm_sec, 7);
    }

    // %V without %G takes the year from the same deduction every other path uses, so a week
    // number is never silently dropped. Deducing it from a century plus %y has to beat the
    // expanded year %y leaves behind on its own (1985 here), which is why the ISO branch runs
    // after the year has been settled rather than before.
    struct { const char* fmt; const char* input;
             int exp_year, exp_mon, exp_mday; } iso[] = {
        { "%G-%V-%u",   "2021-20-7", 2021, 5, 23 },
        { "%Y %V %u",   "2021 20 7", 2021, 5, 23 },   // %Y stands in for a missing %G
        { "%V %u",      "20 7",      2020, 5, 17 },   // and so does the fallback year
        { "%C%y %V %u", "2085 20 7", 2085, 5, 20 },
    };

    for (const auto& c : iso)
    {
        std::tm parsed = calendar_time(120, 4, 17, 0, 0, 0, 0, 0, 0);
        istream iss{mem_device{std::string(c.input)}, locale<char>("C")};
        iss >> get_time(&parsed, c.fmt);
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, c.exp_year - 1900);
        EXPECT_EQ(parsed.tm_mon, c.exp_mon - 1);
        EXPECT_EQ(parsed.tm_mday, c.exp_mday);
    }

    // An inert weekday must not keep the day out of the clamp: the fallback day is still a
    // fallback, so January 31 plus %m=02 lands on the last day of February rather than
    // reaching year_month_day as a nonexistent February 31.
    {
        std::tm parsed = calendar_time(120, 0, 31, 0, 0, 0, 0, 0, 0);    // 2020-01-31, a leap year
        istream iss{mem_device{std::string("02 Sun")}, locale<char>("C")};
        iss >> get_time(&parsed, "%m %a");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 120);
        EXPECT_EQ(parsed.tm_mon, 1);
        EXPECT_EQ(parsed.tm_mday, 29);
    }
    {
        std::tm parsed = calendar_time(121, 0, 31, 0, 0, 0, 0, 0, 0);    // 2021-01-31, not a leap year
        istream iss{mem_device{std::string("02 Sun")}, locale<char>("C")};
        iss >> get_time(&parsed, "%m %a");
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(parsed.tm_year, 121);
        EXPECT_EQ(parsed.tm_mon, 1);
        EXPECT_EQ(parsed.tm_mday, 28);
    }
}

TEST(IoBaseManipTime, TheFallbacksAreIndependentOfTheCharacterType)
{
    // The fallbacks are independent of the character type.
    std::tm parsed = calendar_time(120, 4, 17, 9, 8, 7, 0, 0, 1);        // 2020-05-17 09:08:07
    istream iss{mem_device{std::wstring(L"23:45")}, locale<wchar_t>("C")};
    iss >> get_time(&parsed, L"%H:%M");
    EXPECT_FALSE(iss.str_fail());
    EXPECT_EQ(parsed.tm_year, 120);
    EXPECT_EQ(parsed.tm_mon, 4);
    EXPECT_EQ(parsed.tm_mday, 17);
    EXPECT_EQ(parsed.tm_hour, 23);
    EXPECT_EQ(parsed.tm_min, 45);
    EXPECT_EQ(parsed.tm_sec, 7);
}

namespace
{
// Direction: put_time inserts only, get_time extracts only. It is expressed by which member
// io_traits provides, so the probes go through io_traits: a failure then points at the io_traits
// specialization itself rather than at the value-category and parse-context handling the operators
// layer on top of it. The stream type drops out for the same reason it could never have carried
// the direction -- an iostream satisfies istream_type and ostream_type alike.
static_assert(  insertable <char, put_time_t<char>> );
static_assert( !extractable<char, put_time_t<char>> );
static_assert(  extractable<char, get_time_t<char>> );
static_assert( !insertable <char, get_time_t<char>> );

// The char_type has to match the manipulator's own: put_time/get_time carry the format string.
static_assert( !insertable <wchar_t, put_time_t<char>> );
static_assert( !extractable<wchar_t, get_time_t<char>> );
static_assert(  insertable <wchar_t, put_time_t<wchar_t>> );
static_assert(  extractable<wchar_t, get_time_t<wchar_t>> );
}

#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/traits/char_and_str.h>
#include <io/traits/arithmetic.h>
#include <io/traits/tm.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/io_traits_probe.h>
#include <support/verify.h>

namespace
{
    std::tm test_tm(int sec, int min, int hour, int mday, int mon, int year, int wday, int yday, int isdst)
    {
        static std::tm tmp;
        tmp.tm_sec = sec;
        tmp.tm_min = min;
        tmp.tm_hour = hour;
        tmp.tm_mday = mday;
        tmp.tm_mon = mon;
        tmp.tm_year = year;
        tmp.tm_wday = wday;
        tmp.tm_yday = yday;
        tmp.tm_isdst = isdst;
        return tmp;
    }
}

void test_io_base_manipulators_put_time_char_1()
{
    dump_info("Test ios_base<char> put_time case 1...");

    IOv2::ostream oss{IOv2::mem_device{""}, IOv2::locale<char>("C")};
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    oss << IOv2::put_time(&time1, "%a %Y");
    auto [dev1, err1] = oss.detach();
    VERIFY(dev1.str() == "Sun 1971");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_time_char_2()
{
    dump_info("Test ios_base<char> put_time case 2...");

    IOv2::ostream oss{IOv2::mem_device{""}, IOv2::locale<char>("de_DE.UTF-8")};
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    oss << IOv2::put_time(&time1, "%A %Y");
    auto [dev2, err2] = oss.detach();
    VERIFY(dev2.str() == "Sonntag 1971");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_time_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> put_time case 1...");

    IOv2::ostream oss{IOv2::mem_device{L""}, IOv2::locale<wchar_t>("C")};
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    oss << IOv2::put_time(&time1, L"%a %Y");
    auto [dev3, err3] = oss.detach();
    VERIFY(dev3.str() == L"Sun 1971");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_time_wchar_t_2()
{
    dump_info("Test ios_base<wchar_t> put_time case 2...");

    IOv2::ostream oss{IOv2::mem_device{L""}, IOv2::locale<wchar_t>("de_DE.UTF-8")};
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    oss << IOv2::put_time(&time1, L"%A %Y");
    auto [dev4, err4] = oss.detach();
    VERIFY(dev4.str() == L"Sonntag 1971");

    dump_info("Done\n");
}
void test_io_base_manipulators_get_time_char_1()
{
    dump_info("Test ios_base<char> get_time case 1...");

    // Round-trips put_time. Exercises the idiomatic rvalue form `is >> get_time(...)`,
    // which needs the by-value operator>> overload.
    IOv2::istream iss{IOv2::mem_device{std::string("1971-04-04 12:34:56")},
                      IOv2::locale<char>("C")};

    std::tm parsed{};
    iss >> IOv2::get_time(&parsed, "%Y-%m-%d %H:%M:%S");
    VERIFY(static_cast<bool>(iss));
    VERIFY(!iss.str_fail());
    VERIFY(parsed.tm_year == 71);
    VERIFY(parsed.tm_mon == 3);
    VERIFY(parsed.tm_mday == 4);
    VERIFY(parsed.tm_hour == 12);
    VERIFY(parsed.tm_min == 34);
    VERIFY(parsed.tm_sec == 56);
    // Derived by the context, not present in the input.
    VERIFY(parsed.tm_wday == 0);
    VERIFY(parsed.tm_yday == 93);

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_char_2()
{
    dump_info("Test ios_base<char> get_time case 2...");

    // Null tm / format pointers are reported as a stream failure rather than
    // dereferenced; the target must be left untouched.
    std::tm parsed{};
    parsed.tm_year = 42;

    IOv2::istream iss{IOv2::mem_device{std::string("1971-04-04 12:34:56")},
                      IOv2::locale<char>("C")};
    iss >> IOv2::get_time(&parsed, static_cast<const char*>(nullptr));
    VERIFY(iss.str_fail());
    VERIFY(parsed.tm_year == 42);

    IOv2::istream iss2{IOv2::mem_device{std::string("1971-04-04 12:34:56")},
                       IOv2::locale<char>("C")};
    iss2 >> IOv2::get_time(static_cast<std::tm*>(nullptr), "%Y-%m-%d %H:%M:%S");
    VERIFY(iss2.str_fail());

    // The null check lives in io_traits<...>::sread, which the extraction operator calls after
    // building its sentry, so the leading whitespace skipws consumes is gone by the time the
    // failure is reported. Nothing else is consumed and nothing is written, so a clear() and a
    // retry still see the whole value.
    for (int null_fmt = 0; null_fmt < 2; ++null_fmt)
    {
        std::tm target{};
        IOv2::istream iss3{IOv2::mem_device{std::string("   1971-04-04")},
                           IOv2::locale<char>("C")};
        VERIFY(iss3.tell() == 0);

        if (null_fmt != 0)
            iss3 >> IOv2::get_time(&target, static_cast<const char*>(nullptr));
        else
            iss3 >> IOv2::get_time(static_cast<std::tm*>(nullptr), "%Y-%m-%d");

        VERIFY(iss3.str_fail());
        iss3.clear();
        VERIFY(iss3.tell() == 3);

        // Retrying from that position still sees the whole input.
        iss3 >> IOv2::get_time(&target, "%Y-%m-%d");
        VERIFY(!iss3.str_fail());
        VERIFY(target.tm_year == 71 && target.tm_mon == 3 && target.tm_mday == 4);
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> get_time case 1...");

    IOv2::istream iss{IOv2::mem_device{std::wstring(L"1971-04-04 12:34:56")},
                      IOv2::locale<wchar_t>("C")};

    std::tm parsed{};
    iss >> IOv2::get_time(&parsed, L"%Y-%m-%d %H:%M:%S");
    VERIFY(static_cast<bool>(iss));
    VERIFY(!iss.str_fail());
    VERIFY(parsed.tm_year == 71);
    VERIFY(parsed.tm_mon == 3);
    VERIFY(parsed.tm_mday == 4);
    VERIFY(parsed.tm_hour == 12);
    VERIFY(parsed.tm_min == 34);
    VERIFY(parsed.tm_sec == 56);

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_char_3()
{
    dump_info("Test ios_base<char> get_time case 3...");

    // Fields the format string does not parse keep the values the target already held,
    // instead of being taken from the wall clock.
    const std::tm preset = test_tm(7, 8, 9, 17, 4, 120, 0, 0, 1);   // 2020-05-17 09:08:07

    // Time only: the date is untouched.
    {
        std::tm parsed = preset;
        IOv2::istream iss{IOv2::mem_device{std::string("23:45")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%H:%M");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 120 && parsed.tm_mon == 4 && parsed.tm_mday == 17);
        VERIFY(parsed.tm_hour == 23 && parsed.tm_min == 45 && parsed.tm_sec == 7);
    }

    // Date only: the time is untouched.
    {
        std::tm parsed = preset;
        IOv2::istream iss{IOv2::mem_device{std::string("1999-12-31")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%Y-%m-%d");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 99 && parsed.tm_mon == 11 && parsed.tm_mday == 31);
        VERIFY(parsed.tm_hour == 9 && parsed.tm_min == 8 && parsed.tm_sec == 7);
    }

    // Month and day only: the year is untouched.
    {
        std::tm parsed = preset;
        IOv2::istream iss{IOv2::mem_device{std::string("03/04")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%m/%d");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 120 && parsed.tm_mon == 2 && parsed.tm_mday == 4);
    }

    // Two extractions accumulate into one tm: the first one's result is not wiped by the
    // second one's unparsed fields.
    {
        std::tm parsed = preset;
        IOv2::istream iss{IOv2::mem_device{std::string("2001-02-03")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%Y-%m-%d");
        IOv2::istream iss2{IOv2::mem_device{std::string("11:22:33")}, IOv2::locale<char>("C")};
        iss2 >> IOv2::get_time(&parsed, "%H:%M:%S");
        VERIFY(!iss.str_fail() && !iss2.str_fail());
        VERIFY(parsed.tm_year == 101 && parsed.tm_mon == 1 && parsed.tm_mday == 3);
        VERIFY(parsed.tm_hour == 11 && parsed.tm_min == 22 && parsed.tm_sec == 33);
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_char_4()
{
    dump_info("Test ios_base<char> get_time case 4...");

    const std::tm preset = test_tm(7, 8, 9, 17, 4, 120, 0, 0, 1);   // 2020-05-17 09:08:07

    // tm_wday / tm_yday are always recomputed from the resulting date, and tm_isdst is
    // always -1: no format specifier carries DST information, and keeping the caller's
    // value would be wrong once the date has been rewritten.
    {
        std::tm parsed = preset;
        IOv2::istream iss{IOv2::mem_device{std::string("2026-07-28")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%Y-%m-%d");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_wday == 2);
        VERIFY(parsed.tm_yday == 208);
        VERIFY(parsed.tm_isdst == -1);
    }

    // A two-digit year keeps following the POSIX century rule; the target's century does
    // not leak into it.
    {
        std::tm parsed = preset;
        IOv2::istream iss{IOv2::mem_device{std::string("03")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%y");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 2003 - 1900);

        std::tm parsed2 = preset;
        parsed2.tm_year = 1875 - 1900;
        IOv2::istream iss2{IOv2::mem_device{std::string("03")}, IOv2::locale<char>("C")};
        iss2 >> IOv2::get_time(&parsed2, "%y");
        VERIFY(!iss2.str_fail());
        VERIFY(parsed2.tm_year == 2003 - 1900);
    }

    // %C alone leaves the year within the century open, and that one is 0 rather than the
    // target's, matching POSIX strptime: century 18 yields 1800. The month and day, which
    // %C says nothing about at all, do still come from the target.
    {
        std::tm parsed = preset;
        IOv2::istream iss{IOv2::mem_device{std::string("18")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%C");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 1800 - 1900);
        VERIFY(parsed.tm_mon == 4 && parsed.tm_mday == 17);
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_char_5()
{
    dump_info("Test ios_base<char> get_time case 5...");

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
        std::tm parsed = test_tm(c.sec, 8, 9, c.mday, c.mon, c.year, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string("09")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%H");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == c.exp_year - 1900);
        VERIFY(parsed.tm_mon == c.exp_mon - 1);
        VERIFY(parsed.tm_mday == c.exp_mday);
        VERIFY(parsed.tm_sec == c.exp_sec);
    }

    // A year far outside what std::chrono::year can hold is clamped instead of wrapping.
    {
        std::tm parsed = test_tm(7, 8, 9, 17, 4, 100000, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string("09")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%H");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 32767 - 1900);
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
        std::tm parsed = test_tm(7, 8, 9, c.mday, c.mon, c.year, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string(c.in)}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, c.fmt);
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == c.exp_year - 1900);
        VERIFY(parsed.tm_mon == c.exp_mon - 1);
        VERIFY(parsed.tm_mday == c.exp_mday);
    }

    // A day that really was parsed does not give way: February 31 is reported as a failed
    // extraction and the target is left untouched.
    {
        std::tm parsed = test_tm(7, 8, 9, 15, 1, 120, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string("31")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%d");
        VERIFY(iss.str_fail());
        VERIFY(parsed.tm_mon == 1 && parsed.tm_mday == 15);
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_char_6()
{
    dump_info("Test ios_base<char> get_time case 6...");

    // `is >> tm` goes through parse_context_type<char, std::tm> and gets the same
    // treatment. The C locale's %c is %m/%d/%y %H:%M:%S.
    {
        std::tm parsed = test_tm(7, 8, 9, 17, 4, 120, 0, 0, 1);
        IOv2::istream iss{IOv2::mem_device{std::string("02/06/18 07:08:09")},
                          IOv2::locale<char>("C")};
        iss >> parsed;
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 118 && parsed.tm_mon == 1 && parsed.tm_mday == 6);
        VERIFY(parsed.tm_hour == 7 && parsed.tm_min == 8 && parsed.tm_sec == 9);
        VERIFY(parsed.tm_wday == 2 && parsed.tm_yday == 36);
    }

    // Types without a parse context of their own keep extracting as before.
    {
        int n = 7;
        IOv2::istream iss{IOv2::mem_device{std::string("42")}, IOv2::locale<char>("C")};
        iss >> n;
        VERIFY(!iss.str_fail());
        VERIFY(n == 42);
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_char_7()
{
    dump_info("Test ios_base<char> get_time case 7...");

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
        std::tm parsed = test_tm(c.sec, c.min, c.hour, 1, 0, 121, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string("|")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "|");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == c.exp_year - 1900);
        VERIFY(parsed.tm_mon == c.exp_mon - 1);
        VERIFY(parsed.tm_mday == c.exp_mday);
        VERIFY(parsed.tm_hour == c.exp_hour);
        VERIFY(parsed.tm_min == c.exp_min);
        VERIFY(parsed.tm_sec == c.exp_sec);
    }

    // The time carry and the date group's own normalization share one carry rather than being
    // applied one after the other: mday 0 (the last day of December) plus 24 hours lands back
    // on January 1.
    {
        std::tm parsed = test_tm(0, 0, 24, 0, 0, 121, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string("|")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "|");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 121 && parsed.tm_mon == 0 && parsed.tm_mday == 1);
        VERIFY(parsed.tm_hour == 0 && parsed.tm_min == 0 && parsed.tm_sec == 0);
    }

    // A leap second is still the one truncation in the time group: 60 becomes 59 and carries
    // nothing, so 23:59:60 stays on its own day.
    {
        std::tm parsed = test_tm(60, 59, 23, 1, 0, 121, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string("|")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "|");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 121 && parsed.tm_mon == 0 && parsed.tm_mday == 1);
        VERIFY(parsed.tm_hour == 23 && parsed.tm_min == 59 && parsed.tm_sec == 59);
    }

    // Parsed fields still win over the fallback: the carry only decides what the format string
    // leaves alone. Here the day has already moved to the 2nd when %H overwrites the hour.
    {
        std::tm parsed = test_tm(0, 0, 24, 1, 0, 121, 0, 0, 0);
        IOv2::istream iss{IOv2::mem_device{std::string("07")}, IOv2::locale<char>("C")};
        iss >> IOv2::get_time(&parsed, "%H");
        VERIFY(!iss.str_fail());
        VERIFY(parsed.tm_year == 121 && parsed.tm_mon == 0 && parsed.tm_mday == 2);
        VERIFY(parsed.tm_hour == 7);
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_get_time_wchar_t_2()
{
    dump_info("Test ios_base<wchar_t> get_time case 2...");

    // The fallbacks are independent of the character type.
    std::tm parsed = test_tm(7, 8, 9, 17, 4, 120, 0, 0, 1);        // 2020-05-17 09:08:07
    IOv2::istream iss{IOv2::mem_device{std::wstring(L"23:45")}, IOv2::locale<wchar_t>("C")};
    iss >> IOv2::get_time(&parsed, L"%H:%M");
    VERIFY(!iss.str_fail());
    VERIFY(parsed.tm_year == 120 && parsed.tm_mon == 4 && parsed.tm_mday == 17);
    VERIFY(parsed.tm_hour == 23 && parsed.tm_min == 45 && parsed.tm_sec == 7);

    dump_info("Done\n");
}

namespace
{
// Direction: put_time inserts only, get_time extracts only. It is expressed by which member
// io_traits provides, so the probes go through io_traits: the operators are unconstrained on the
// value type and reject in the body with a static_assert, which no requires-expression can see.
// The stream type drops out for the same reason it could never have carried the direction -- an
// iostream satisfies istream_type and ostream_type alike.
static_assert(  insertable <char, IOv2::_Put_time<char>> );
static_assert( !extractable<char, IOv2::_Put_time<char>> );
static_assert(  extractable<char, IOv2::_Get_time<char>> );
static_assert( !insertable <char, IOv2::_Get_time<char>> );

// The char_type has to match the manipulator's own: put_time/get_time carry the format string.
static_assert( !insertable <wchar_t, IOv2::_Put_time<char>> );
static_assert( !extractable<wchar_t, IOv2::_Get_time<char>> );
static_assert(  insertable <wchar_t, IOv2::_Put_time<wchar_t>> );
static_assert(  extractable<wchar_t, IOv2::_Get_time<wchar_t>> );
}

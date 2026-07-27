#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/fp_defs/arithmetic.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
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

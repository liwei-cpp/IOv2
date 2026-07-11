#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/fp_defs/arithmetic.h>
#include <io/io_base.h>
#include <io/io_manip.h>
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
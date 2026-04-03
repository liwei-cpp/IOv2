#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/tm.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

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

void test_ostream_inserters_time_char_1()
{
    dump_info("Test ostream<char> operator<< (time) case 1...");

    auto helper = []<template <typename, typename> class T>()
    {
        std::tm tp = test_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 0, 0, 0);
        T f(IOv2::mem_device{""}, IOv2::locale<char>("C"));

        f << tp;
        VERIFY((bool)f);
        const std::string res = f.detach().str();
        VERIFY(res == "09/04/24 13:33:18");
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}
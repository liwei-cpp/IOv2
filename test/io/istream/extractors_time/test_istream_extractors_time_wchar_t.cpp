#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/tm.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_istream_extractors_time_wchar_t_1()
{
    dump_info("Test istream<wchar_t> operator>> (time) case 1...");

    auto helper = []<template <typename, typename> class T>()
    {
        std::tm tp;
        T f(IOv2::mem_device{L"09/04/24 13:33:18"}, IOv2::locale<wchar_t>("C"));

        f >> tp;
        VERIFY((bool)f);
        VERIFY(tp.tm_year == 2024 - 1900);
        VERIFY(tp.tm_mon == 9 - 1);
        VERIFY(tp.tm_mday == 4);
        VERIFY(tp.tm_hour == 13);
        VERIFY(tp.tm_min == 33);
        VERIFY(tp.tm_sec == 18);
    };

    helper.template operator()<IOv2::istream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}
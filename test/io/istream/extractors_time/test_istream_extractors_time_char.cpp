#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <device/mem_device.h>
#include <io/traits/tm.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_istream_extractors_time_char_1()
{
    dump_info("Test istream<char> operator>> (time) case 1...");

    auto helper = []<template <typename, typename> class T>()
    {
        std::tm tp{};
        // The stream format appends %z and (%Z) where the platform's tm carries tm_gmtoff and
        // tm_zone, so the input carries both and both reach the tm instead of being dropped.
#ifdef __USE_MISC
        T f(IOv2::mem_device{"Wed Sep  4 13:33:18 2024 +0800 (CST)"}, IOv2::locale<char>("C"));
#else
        T f(IOv2::mem_device{"Wed Sep  4 13:33:18 2024"}, IOv2::locale<char>("C"));
#endif

        f >> tp;
        VERIFY((bool)f);
        VERIFY(tp.tm_year == 2024 - 1900);
        VERIFY(tp.tm_mon == 9 - 1);
        VERIFY(tp.tm_mday == 4);
        VERIFY(tp.tm_hour == 13);
        VERIFY(tp.tm_min == 33);
        VERIFY(tp.tm_sec == 18);
#ifdef __USE_MISC
        VERIFY(tp.tm_gmtoff == 8 * 3600);
        VERIFY(tp.tm_zone != nullptr && std::string_view(tp.tm_zone) == "CST");
#endif
    };

    helper.template operator()<IOv2::istream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}
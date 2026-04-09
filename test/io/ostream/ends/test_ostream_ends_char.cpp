#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_ostream_ends_char_1()
{
    dump_info("Test ostream<char> with ends case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str01(" santa barbara ");
        auto oss01 = T(IOv2::mem_device{" santa barbara "});
        auto oss02 = T(IOv2::mem_device{""});

        oss01 << IOv2::ends;
        VERIFY(oss01.device().str().size() == str01.size());
        
        oss02 << IOv2::ends;
        oss02.flush();
        VERIFY(oss02.device().str().size() == 1);
        VERIFY(oss02.device().str()[0] == char());
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_ends_char_2()
{
    dump_info("Test ostream<char> with ends case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto osst_01 = T(IOv2::mem_device{""});
        const std::string str_00("herbie_hancock");
        size_t len1 = str_00.size();
        osst_01 << str_00;
        osst_01.flush();
        if (osst_01.device().str().size() != len1) throw std::runtime_error("ostream<char> with ends check fail");

        osst_01 << IOv2::ends;

        const std::string str_01("speak like a child");
        size_t len2 = str_01.size();
        osst_01 << str_01;
        osst_01.flush();
        size_t len3 = osst_01.device().str().size();
        if (len1 >= len3) throw std::runtime_error("ostream<char> with ends check fail");
        if (len3 != len1 + len2 + 1) throw std::runtime_error("ostream<char> with ends check fail");

        osst_01 << IOv2::ends;

        const std::string str_02("+ inventions and dimensions");
        size_t len4 = str_02.size();
        osst_01 << str_02;
        osst_01.flush();
        size_t len5 = osst_01.device().str().size();
        if (len3 >= len5) throw std::runtime_error("ostream<char> with ends check fail");
        if (len5 != len3 + len4 + 1) throw std::runtime_error("ostream<char> with ends check fail");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

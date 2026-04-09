#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_ostream_tell_char_1()
{
    dump_info("Test ostream<char>::tell case 1...");

    auto helper = []<template <typename, typename> class T>()
    {
        file_guard g1("istream_seeks-3.txt");
        T ost1{IOv2::mem_device{""}};
        T ofs1{IOv2::ofile_device<char>{"istream_seeks-3.txt"}};
        
        auto p1 = ost1.tell();
        auto p2 = ofs1.tell();
        VERIFY( p1 == 0 );
        VERIFY( p2 == 0 );
        
        T ost2{IOv2::mem_device{"bob_marley:kaya"}};
        VERIFY( ost2.tell() == 0 );
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_tell_char_2()
{
    dump_info("Test ostream<char>::tell case 2...");

    auto helper = []<template <typename, typename> class T>()
    {
        T ost{IOv2::mem_device{""}};
        auto pos1 = ost.tell();
        VERIFY(pos1 == 0);

        ost << "RZA ";
        pos1 = ost.tell();
        VERIFY( pos1 == 4 );

        ost << "ghost dog: way of the samurai";
        pos1 = ost.tell();
        VERIFY( pos1 == 33 );
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}
#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_istream_putback_wchar_t_1()
{
    dump_info("Test istream<wchar_t>::putback case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str_02(L"soul eyes: john coltrane quartet");
        T is_00(IOv2::mem_device{str_02});
        T is_03(IOv2::mem_device{str_02});
        T is_04(IOv2::mem_device{str_02});

        IOv2::ios_defs::iostate state1, state2;

        // istream& putback(char c)
        is_04.ignore(30);
        is_04.clear();
        state1 = is_04.rdstate();
        is_04.putback(L't');
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == L't' );

        is_04.clear();
        state1 = is_04.rdstate();
        is_04.putback(L'r');
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == L'r' );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

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

void test_istream_ws_wchar_t_1()
{
    dump_info("Test istream<wchar_t> with ws case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str01(L" santa barbara ");

        // template<_CharT, _Traits>
        //  basic_istream<_CharT, _Traits>& ws(basic_istream<_Char, _Traits>& is)
        T iss01{IOv2::mem_device{str01}};
        T iss02{IOv2::mem_device{str01}};

        std::wstring str04;
        std::wstring str05;
        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == L"santa" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == L"santa" );
        VERIFY( str05 == str04 );

        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == L"barbara" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == L"barbara" );
        VERIFY( str05 == str04 );

        VERIFY( (bool)iss01 );
        VERIFY( (bool)iss02 );
        VERIFY( !iss01.eof() );
        VERIFY( !iss02.eof() );

        iss01 >> IOv2::ws;
        VERIFY( (bool)iss01 );
        VERIFY( iss01.eof() );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}


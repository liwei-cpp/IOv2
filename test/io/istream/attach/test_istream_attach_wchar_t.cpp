#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/istream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

// wchar_t counterpart of test_istream_attach_state_char_1.
void test_istream_attach_state_wchar_t_1()
{
    dump_info("Test istream<wchar_t> attach clears state case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T is{IOv2::mem_device{std::wstring(L"ab")}};

        std::wstring tok;
        is >> tok;
        VERIFY( tok == L"ab" );
        VERIFY( is.eof() );

        is.attach(IOv2::mem_device{std::wstring(L"cd")});
        VERIFY( is.rdstate() == IOv2::ios_defs::goodbit );

        std::wstring tok2;
        is >> tok2;
        VERIFY( tok2 == L"cd" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

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

void test_istream_peek_char_1()
{
    dump_info("Test istream<char>::peek case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_02("soul eyes: john coltrane quartet");
        T is_03(IOv2::mem_device{str_02});
        T is_04(IOv2::mem_device{str_02});
        IOv2::ios_defs::iostate state1, state2;

        char carray[60] = "";

        // istream& ignore(streamsize n = 1, int_type delim = traits::eof())
        is_04.read(carray, 9);
        VERIFY( is_04.peek() == ':' );

        state1 = is_04.rdstate();
        is_04.ignore();
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == ' ' );

        state1 = is_04.rdstate();
        is_04.ignore(0);
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == ' ' );

        state1 = is_04.rdstate();
        is_04.ignore(5, ' ');
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == 'j' );

        state1 = is_04.rdstate();
        VERIFY( is_04.peek() == 'j' );
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );

        is_04.ignore(30);
        state1 = is_04.rdstate();
        VERIFY( !is_04.peek().has_value() );
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_peek_char_2()
{
    dump_info("Test istream<char>::peek case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        T stream{IOv2::mem_device{""}};
        VERIFY( stream.rdstate() == IOv2::ios_defs::goodbit );
        auto c = stream.peek();
        VERIFY( !c.has_value() );
        VERIFY( stream.rdstate() == IOv2::ios_defs::eofbit );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_peek_char_3()
{
    dump_info("Test istream<char>::peek case 3...");

    auto helper = []<template<typename, typename> class T>()
    {
        std::string data = 
            "bd2\n"
            "456x\n"
            "9mzuv>?@ABCDEFGHIJKLMNOPQRSTUVWXYZracadabras, i wannaz\n"
            "because because\n"
            "because. . \n"
            "of the wonderful things he does!!\n"
            "ok\n";

        file_guard g("istream_seeks-1.txt", data);

        T if01(IOv2::ifile_device<char>{"istream_seeks-1.txt"});
        if01.seek(0);
        auto pos01 = if01.tell();
        if01.peek();
        auto pos02 = if01.tell();
        VERIFY( pos02 == pos01 );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

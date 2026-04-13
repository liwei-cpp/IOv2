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

void test_istream_tell_char_1()
{
    dump_info("Test istream<char>::tell case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        // in
        T ist1{IOv2::mem_device{""}};
        auto p3 = ist1.tell();

        // N.B. We implement the resolution of DR 453 and
        // istringstream::tell() doesn't fail.
        VERIFY( p3 == 0 );

        std::string data = 
            "bd2\n"
            "456x\n"
            "9mzuv>?@ABCDEFGHIJKLMNOPQRSTUVWXYZracadabras, i wannaz\n"
            "because because\n"
            "because. . \n"
            "of the wonderful things he does!!\n"
            "ok\n";

        file_guard g1("istream_seeks-1.tst", data);
        T ist2{IOv2::mem_device{"bob_marley:kaya"}};
        T ifs2{IOv2::ifile_device<char>{"istream_seeks-1.tst"}};
        p3 = ist2.tell();
        auto p4 = ifs2.tell();
        VERIFY( p3 == p4 );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_tell_char_2()
{
    dump_info("Test istream<char>::tell case 2...");
    auto helper = []<template<typename, typename> class T>()
    {
        std::string num1("555");

        // tell
        {
            T iss(IOv2::mem_device{num1});
            iss.tell();
            int asNum = 0;
            iss >> asNum;
            VERIFY( iss.eof() );
            VERIFY( (bool)iss );
            iss.clear();
            iss.tell();
            VERIFY( (bool)iss );
        }

        // seek
        {
            T iss(IOv2::mem_device{num1});
            iss.tell();
            int asNum = 0;
            iss >> asNum;
            VERIFY( iss.eof() );
            VERIFY( (bool)iss );
            iss.seek(0);
            VERIFY( (bool)iss );
        }

        // seek
        {
            T iss(IOv2::mem_device{num1});
            auto pos1 = iss.tell();
            int asNum = 0;
            iss >> asNum;
            VERIFY( iss.eof() );
            VERIFY( (bool)iss );
            iss.seek(pos1);
            VERIFY( (bool)iss );
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_tell_char_3()
{
    dump_info("Test istream<char>::tell case 3...");
    auto helper = []<template<typename, typename> class T>()
    {
        IOv2::ios_defs::iostate state01, state02;

        const char str_lit01[] = "istream_seeks-1.txt";

        std::string str_lit01_data = 
            "bd2\n"
            "456x\n"
            "9mzuv>?@ABCDEFGHIJKLMNOPQRSTUVWXYZracadabras, i wannaz\n"
            "because because\n"
            "because. . \n"
            "of the wonderful things he does!!\n"
            "ok\n";
        std::string str_lit02_data = "";
        file_guard g1(str_lit01, str_lit01_data);

        T if01{IOv2::ifile_device<char>{str_lit01}};
        VERIFY( if01.good() );

        auto pos01 = if01.tell();
        auto pos02 = if01.tell();
        VERIFY( pos01 == pos02 );

        // cur 
        // NB: see library issues list 136. It's the v-3 interp that seek
        // only sets the input buffer, or else istreams with buffers that
        // have _M_mode == ios_base::out will fail to have consistency
        // between seek and tell.
        state01 = if01.rdstate();
        if01.seek(10 + if01.tell());
        state02 = if01.rdstate();
        pos01 = if01.tell(); 
        VERIFY( pos01 == pos02 + 10 ); 
        VERIFY( state01 == state02 );
        pos02 = if01.tell(); 
        VERIFY( pos02 == pos01 ); 
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_tell_char_4()
{
    dump_info("Test istream<char>::tell case 4...");

    auto helper = []<template<typename, typename> class T>()
    {
        IOv2::ios_defs::iostate state01, state02;

        std::string str_lit01_data = 
            "bd2\n"
            "456x\n"
            "9mzuv>?@ABCDEFGHIJKLMNOPQRSTUVWXYZracadabras, i wannaz\n"
            "because because\n"
            "because. . \n"
            "of the wonderful things he does!!\n"
            "ok\n";
        std::string str_lit02_data = "";

        T if01{IOv2::mem_device{str_lit01_data}};
        T if03{IOv2::mem_device{str_lit02_data}};
        VERIFY( if01.good() );
        VERIFY( if03.good() );

        auto pos01 = if01.tell();
        auto pos02 = if01.tell();
        VERIFY( pos01 == pos02 );

        auto pos05 = if03.tell();
        auto pos06 = if03.tell();
        VERIFY( pos05 == pos06 );

        // cur 
        // NB: see library issues list 136. It's the v-3 interp that seek
        // only sets the input buffer, or else istreams with buffers that
        // have _M_mode == ios_base::out will fail to have consistency
        // between seek and tell.
        state01 = if01.rdstate();
        if01.seek(10 + if01.tell());
        state02 = if01.rdstate();
        pos01 = if01.tell(); 
        VERIFY( pos01 == pos02 + 10 ); 
        VERIFY( state01 == state02 );
        pos02 = if01.tell(); 
        VERIFY( pos02 == pos01 );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

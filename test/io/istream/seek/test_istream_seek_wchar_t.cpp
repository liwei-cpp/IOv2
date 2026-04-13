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

void test_istream_seek_wchar_t_1()
{
    dump_info("Test istream<wchar_t>::seek case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        std::wstring str = L"Duos for Doris";
        T iss(IOv2::mem_device{str});
        auto p0 = iss.tell();
        VERIFY(p0 == 0);

        wchar_t ch[100];
        iss.template get<IOv2::keep_sep, IOv2::no_zt>(ch, str.size());
        VERIFY( iss.rdstate() == IOv2::ios_defs::eofbit );

        iss.seek(0);
        VERIFY( iss.good() );

        iss.seek(0);
        VERIFY( (bool)iss );
        VERIFY( iss.tell() == p0 );

        iss.template get<IOv2::keep_sep, IOv2::no_zt>(ch, str.size());
        VERIFY( iss.rdstate() == IOv2::ios_defs::eofbit );

        iss.seek(p0);
        VERIFY( iss.good() );

        iss.seek(p0);
        VERIFY( (bool)iss );
        VERIFY( iss.tell() == p0 );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_seek_wchar_t_2()
{
    dump_info("Test istream<wchar_t>::seek case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        std::wstring num1(L"555");
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
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_seek_wchar_t_3()
{
    dump_info("Test istream<wchar_t>::seek case 3...");

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

        T if01{IOv2::ifile_device<char>{str_lit01},
               IOv2::code_cvt_creator<char, wchar_t>("C")};
        VERIFY( if01.good() );

        auto pos01 = if01.tell();
        auto pos02 = if01.tell();
        // istream& seek(pos_type)
        // istream& seek(off_type, ios_base::seekdir)

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

        // beg
        state01 = if01.rdstate();
        if01.seek(20);
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

void test_istream_seek_wchar_t_4()
{
    dump_info("Test istream<wchar_t>::seek case 4...");

    auto helper = []<template<typename, typename> class T>()
    {
        IOv2::ios_defs::iostate state01, state02;

        std::wstring str_lit01_data = 
            L"bd2\n"
            L"456x\n"
            L"9mzuv>?@ABCDEFGHIJKLMNOPQRSTUVWXYZracadabras, i wannaz\n"
            L"because because\n"
            L"because. . \n"
            L"of the wonderful things he does!!\n"
            L"ok\n";

        T if01{IOv2::mem_device{str_lit01_data}};
        T if03{IOv2::mem_device{L""}};
        VERIFY( if01.good() );
        VERIFY( if03.good() );

        auto pos01 = if01.tell();
        auto pos02 = if01.tell();
        auto pos05 = if03.tell();
        auto pos06 = if03.tell();
        // istream& seek(pos_type)
        // istream& seek(off_type, ios_base::seekdir)

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

        state01 = if03.rdstate();
        if03.seek(10 + if03.tell());
        state02 = if03.rdstate();
        pos05 = if03.tell(); 
        VERIFY( pos05 == pos06 ); 
        VERIFY( state01 != state02 );
        VERIFY(state02 == IOv2::ios_defs::devfailbit);
        pos06 = if03.tell(); 
        VERIFY( pos05 == pos06 ); 

        // beg
        state01 = if01.rdstate();
        if01.seek(20);
        state02 = if01.rdstate();
        pos01 = if01.tell(); 
        VERIFY( pos01 == pos02 + 10 ); 
        VERIFY( state01 == state02 );
        pos02 = if01.tell();
        VERIFY( pos02 == pos01 ); 

        state01 = if03.rdstate();
        if03.seek(20);
        state02 = if03.rdstate();
        pos05 = if03.tell(); 
        VERIFY( pos05 == pos06);
        VERIFY( state01 == state02 );
        pos06 = if03.tell(); 
        VERIFY( pos05 == pos06 );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

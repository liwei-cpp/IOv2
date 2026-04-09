#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_ostream_inserters_character_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 1...");

    auto helper = []<template <typename, typename> class T>()
    {
        std::wstring str01;
        const int size = 1000;

        // initialize string
        for(int i=0 ; i < size; i++)
        {
            str01 += L'1';
            str01 += L'2';
            str01 += L'3';
            str01 += L'4';
            str01 += L'5';
            str01 += L'6';
            str01 += L'7';
            str01 += L'8';
            str01 += L'9';
            str01 += L'\n';
        }
        T f(IOv2::mem_device{L""});

        f << str01;
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_2()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 2...");

    auto helper = []<template <typename, typename> class T>()
    {
        std::wstring str01 = L"";
        T oss01{IOv2::mem_device{L""}};
        oss01.width(5);
        oss01.fill(L'0');
        oss01.flags(IOv2::ios_defs::left);
        oss01 << str01;
        VERIFY(oss01.detach().str() == L"00000");

        std::wstring str02 = L"1";
        T oss02{IOv2::mem_device{L""}};
        oss02.width(5);
        oss02.fill(L'0');
        oss02.flags(std::ios_base::left);
        oss02 << str02;
        VERIFY(oss02.detach().str() == L"10000");

        std::wstring str03 = L"909909";
        T oss03{IOv2::mem_device{L""}};
        oss03.width(5);
        oss03.fill(L'0');
        oss03.flags(std::ios_base::left);
        oss03 << str03;
        VERIFY(oss03.detach().str() == L"909909");
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_3()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 3...");

    auto helper = []<template <typename, typename> class T>()
    {
        std::wstring str01 = L"";
        T oss01{IOv2::mem_device{L""}};
        oss01.width(5);
        oss01.fill(L'0');
        oss01.flags(IOv2::ios_defs::right);
        oss01 << str01;
        VERIFY(oss01.detach().str() == L"00000");

        std::wstring str02 = L"1";
        T oss02{IOv2::mem_device{L""}};
        oss02.width(5);
        oss02.fill(L'0');
        oss02.flags(std::ios_base::right);
        oss02 << str02;
        VERIFY(oss02.detach().str() == L"00001");

        std::wstring str03 = L"909909";
        T oss03{IOv2::mem_device{L""}};
        oss03.width(5);
        oss03.fill(L'0');
        oss03.flags(std::ios_base::right);
        oss03 << str03;
        VERIFY(oss03.detach().str() == L"909909");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_4()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 4...");

    auto helper = []<template <typename, typename> class T>()
    {
        std::wstring str_01;
        std::wstring str_tmp;
        const int i_max=250;

        T oss_02(IOv2::mem_device{str_01});
        for (int i = 0; i < i_max; ++i) 
            oss_02 << L"Test: " << i << IOv2::endl;
        VERIFY((bool)oss_02);
        VERIFY(oss_02.good());
        str_tmp = oss_02.detach().str();
        VERIFY(str_tmp.size() == 2390);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_5()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 5...");

    auto helper = []<template <typename, typename> class T>()
    {
        wchar_t* pt = 0;
        T oss{IOv2::mem_device{L""}};
        oss << pt;
        VERIFY(!oss);
        oss.flush();
        VERIFY(oss.device().str().size() == 0);
        oss.clear();
        oss << L"";
        VERIFY(oss.good());
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_6()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 6...");

    auto helper = []<template <typename, typename> class T>()
    {
        {
            T oss{IOv2::mem_device{L""}};
            oss.width(-60);
            oss << L'C';
            VERIFY(oss.good());
            VERIFY(oss.detach().str() == L"C");
        }
        {
            T oss{IOv2::mem_device{L""}};
            oss.width(-60);
            oss << L"Consoli";
            VERIFY(oss.good());
            VERIFY(oss.detach().str() == L"Consoli");
        }
        {
            T oss{IOv2::mem_device{L""}};
            oss.width(-60);
            oss << std::wstring(L"Consoli");
            VERIFY(oss.good());
            VERIFY(oss.detach().str() == L"Consoli");
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_7()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 7...");

#define WIDTH 20000000
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T oss_01{IOv2::mem_device{L""}};
            oss_01.width(WIDTH);
            const std::streamsize width = oss_01.width();
            oss_01 << L'a';
            VERIFY(oss_01.good());
            VERIFY(oss_01.detach().str().size() == std::string::size_type(width));
        }
        {
            const std::wstring str_01(50, L'a');
            T oss_01{IOv2::mem_device{L""}};
            oss_01.width(WIDTH);
            const std::streamsize width = oss_01.width();
            oss_01 << str_01.c_str();
            VERIFY(oss_01.good());
            VERIFY(oss_01.detach().str().size() == std::string::size_type(width));
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
#undef WIDTH

    dump_info("Done\n");
}



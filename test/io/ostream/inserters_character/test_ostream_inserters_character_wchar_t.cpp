#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/traits/nullptr.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/io_traits_probe.h>
#include <support/verify.h>

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
        auto [dev01, err01] = oss01.detach();
        VERIFY(dev01.str() == L"00000");

        std::wstring str02 = L"1";
        T oss02{IOv2::mem_device{L""}};
        oss02.width(5);
        oss02.fill(L'0');
        oss02.flags(IOv2::ios_defs::left);
        oss02 << str02;
        auto [dev02, err02] = oss02.detach();
        VERIFY(dev02.str() == L"10000");

        std::wstring str03 = L"909909";
        T oss03{IOv2::mem_device{L""}};
        oss03.width(5);
        oss03.fill(L'0');
        oss03.flags(IOv2::ios_defs::left);
        oss03 << str03;
        auto [dev03, err03] = oss03.detach();
        VERIFY(dev03.str() == L"909909");
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
        auto [dev04, err04] = oss01.detach();
        VERIFY(dev04.str() == L"00000");

        std::wstring str02 = L"1";
        T oss02{IOv2::mem_device{L""}};
        oss02.width(5);
        oss02.fill(L'0');
        oss02.flags(IOv2::ios_defs::right);
        oss02 << str02;
        auto [dev05, err05] = oss02.detach();
        VERIFY(dev05.str() == L"00001");

        std::wstring str03 = L"909909";
        T oss03{IOv2::mem_device{L""}};
        oss03.width(5);
        oss03.fill(L'0');
        oss03.flags(IOv2::ios_defs::right);
        oss03 << str03;
        auto [dev06, err06] = oss03.detach();
        VERIFY(dev06.str() == L"909909");
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
        auto [dev07, err07] = oss_02.detach();
        str_tmp = dev07.str();
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
            oss.width(0);
            oss << L'C';
            VERIFY(oss.good());
            auto [dev08, err08] = oss.detach();
            VERIFY(dev08.str() == L"C");
        }
        {
            T oss{IOv2::mem_device{L""}};
            oss.width(0);
            oss << L"Consoli";
            VERIFY(oss.good());
            auto [dev09, err09] = oss.detach();
            VERIFY(dev09.str() == L"Consoli");
        }
        {
            T oss{IOv2::mem_device{L""}};
            oss.width(0);
            oss << std::wstring(L"Consoli");
            VERIFY(oss.good());
            auto [dev10, err10] = oss.detach();
            VERIFY(dev10.str() == L"Consoli");
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_7()
{
    dump_info("Test ostream<wchar_t> operator<< (character) case 7...");

#define WIDTH 200
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T oss_01{IOv2::mem_device{L""}};
            oss_01.width(WIDTH);
            const size_t width = oss_01.width();
            oss_01 << L'a';
            VERIFY(oss_01.good());
            auto [dev11, err11] = oss_01.detach();
            VERIFY(dev11.str().size() == width);
        }
        {
            const std::wstring str_01(50, L'a');
            T oss_01{IOv2::mem_device{L""}};
            oss_01.width(WIDTH);
            const size_t width = oss_01.width();
            oss_01 << str_01.c_str();
            VERIFY(oss_01.good());
            auto [dev12, err12] = oss_01.detach();
            VERIFY(dev12.str().size() == width);
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
#undef WIDTH

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_8()
{
    dump_info("Test ostream<wchar_t>::write case 8 (null source pointer)...");

    auto helper = []<template<typename, typename> class T>()
    {
        // write() with a null source and a non-zero count is rejected with stream_error
        // -> strfailbit; no mask means no throw.
        T oss{IOv2::mem_device{std::wstring(L"")}};
        bool threw = false;
        try { oss.write(nullptr, 5); }
        catch (...) { threw = true; }
        VERIFY( !threw );
        VERIFY( oss.rdstate() & IOv2::ios_defs::strfailbit );

        // null source with a zero count is a well-defined no-op.
        T oss2{IOv2::mem_device{std::wstring(L"")}};
        oss2.write(nullptr, 0);
        VERIFY( !(oss2.rdstate() & IOv2::ios_defs::strfailbit) );
        auto [dev, err] = oss2.detach();
        VERIFY( dev.str().empty() );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_character_wchar_t_9()
{
    dump_info("Test ostream<wchar_t> operator<< case 9 (character conformance)...");

    // Availability is probed through `insertable` (support/io_traits_probe.h) rather than
    // `requires { oss << v; }`, so a failure points at io_traits itself and not at the
    // value-category and decay handling operator<< layers on top of it.
    static_assert( insertable<wchar_t, char> );
    static_assert( insertable<wchar_t, wchar_t> );
    static_assert( insertable<wchar_t, std::nullptr_t> );
    static_assert( !insertable<wchar_t, char8_t> );
    static_assert( !insertable<wchar_t, char16_t> );
    static_assert( !insertable<wchar_t, char32_t> );

    // The string-pointer overloads mirror the single-character ones exactly.
    static_assert( insertable<wchar_t, const char*> );
    static_assert( insertable<wchar_t, const wchar_t*> );
    static_assert( !insertable<wchar_t, const char8_t*> );
    static_assert( !insertable<wchar_t, const char16_t*> );
    static_assert( !insertable<wchar_t, const char32_t*> );
    // A wide stream has no signed char / unsigned char string overload; as for
    // std::wostream these reach the address path instead.
    static_assert( insertable<wchar_t, const unsigned char*> );
    static_assert( insertable<wchar_t, int*> );

    auto helper = []<template <typename, typename> class T>()
    {
        {
            // On a wide stream signed char / unsigned char are numeric, as they are for
            // std::wostream, where they reach operator<<(int) through integral promotion.
            T oss{IOv2::mem_device{std::wstring(L"")}};
            oss << static_cast<signed char>(65) << L'-' << static_cast<unsigned char>(66);
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"65-66");
        }
        {
            T oss{IOv2::mem_device{std::wstring(L"")}};
            oss << 'A' << L'B';
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"AB");
        }
        {
            T oss{IOv2::mem_device{std::wstring(L"")}};
            oss << nullptr;
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"nullptr");
        }
        {
            // << nullptr is a formatted output function: it pads to width() and then
            // clears it. Skipping the clear would leak the width into the next
            // insertion, so L'|' is what actually pins that half down.
            T oss{IOv2::mem_device{std::wstring(L"")}};
            oss << IOv2::setw(10) << nullptr << L'|';
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"   nullptr|");
        }
        {
            T oss{IOv2::mem_device{std::wstring(L"")}};
            oss << IOv2::setw(10) << IOv2::left << IOv2::setfill(L'*') << nullptr << L'|';
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"nullptr***|");
        }
        {
            // A narrow string is widened into a wide stream, matching the charT-templated
            // operator<<(basic_ostream<charT>&, const char*). It used to print an address.
            T oss{IOv2::mem_device{std::wstring(L"")}};
            const char* p = "yo";
            oss << "hi" << L'/' << p;
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"hi/yo");
        }
        {
            T oss{IOv2::mem_device{std::wstring(L"")}};
            oss << IOv2::setw(5) << "hi" << L'|';
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"   hi|");
        }
        {
            // A volatile key must land in the same specialization as the unqualified one, which
            // on a wide stream means the character types widen and signed / unsigned char stay
            // numeric -- exactly the split pinned by the first two blocks above.
            volatile char          vc  = 'A';
            volatile wchar_t       vwc = L'B';
            volatile signed char   vsc = 65;
            volatile unsigned char vuc = 66;

            T oss{IOv2::mem_device{std::wstring(L"")}};
            oss << vc << vwc << vsc << L'-' << vuc;
            VERIFY(oss.good());
            auto [dev, err] = oss.detach();
            VERIFY(dev.str() == L"AB65-66");
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}



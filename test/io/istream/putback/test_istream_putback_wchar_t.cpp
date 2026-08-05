#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

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

// wchar_t counterpart of test_istream_putback_char_2: putback() on a failed stream is
// rejected by the sentry and routed through handle_exception (-> strfailbit), no throw.
void test_istream_putback_wchar_t_2()
{
    dump_info("Test istream<wchar_t>::putback case 2 (failed stream)...");

    auto helper = []<template<typename, typename> class T>()
    {
        T is{IOv2::mem_device{std::wstring(L"abc")}, IOv2::locale<wchar_t>("C")};

        int v = 0;
        is >> v;                      // non-numeric input -> strfailbit
        VERIFY( !is );

        bool threw = false;
        try { is.putback(L'z'); }
        catch (...) { threw = true; }
        VERIFY( !threw );
        VERIFY( is.rdstate() & IOv2::ios_defs::strfailbit );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

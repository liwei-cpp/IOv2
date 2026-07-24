#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <facet/ctype.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

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

// wchar_t counterpart of test_istream_function_manip_char_1: a std::function manipulator
// in a NON-CONST lvalue must dispatch to operator>>(T&, const std::function<void(T&)>&)
// rather than being shadowed by the (now-constrained) generic value operator>>.
void test_istream_function_manip_wchar_t_1()
{
    dump_info("Test istream<wchar_t> std::function manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::wstring(L"hello world")}};
        using S = decltype(iss);

        int calls = 0;
        std::function<void(S&)> manip = [&calls](S&){ ++calls; };

        iss >> manip;                 // the previously-broken path
        VERIFY( calls == 1 );

        iss >> manip >> manip;        // operator>> returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // sibling function-pointer manipulator form: operator>>(T&, void(*)(T&))
        static int fcalls;
        fcalls = 0;
        iss >> +[](S&){ ++fcalls; };
        VERIFY( fcalls == 1 );

        // the generic value operator>> still extracts real values afterwards
        std::wstring tok;
        iss >> tok;
        VERIFY( tok == L"hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_istream_null_manip_char_1: a null manipulator must be
// rejected by every manipulator overload, leaving strfailbit set (no mask -> no throw).
void test_istream_null_manip_wchar_t_1()
{
    dump_info("Test istream<wchar_t> null manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::wstring(L"hello world")}};
        using S = decltype(iss);

        iss >> static_cast<void(*)(IOv2::ios_base<wchar_t>&)>(nullptr);
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        std::function<void(IOv2::ios_base<wchar_t>&)> empty_base_fn;
        iss >> empty_base_fn;
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        int base_calls = 0;
        std::function<void(IOv2::ios_base<wchar_t>&)> base_fn =
            [&base_calls](IOv2::ios_base<wchar_t>&){ ++base_calls; };
        iss >> base_fn;
        VERIFY( base_calls == 1 );
        VERIFY( !(iss.rdstate() & IOv2::ios_defs::strfailbit) );

        iss >> static_cast<void(*)(S&)>(nullptr);
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        std::function<void(S&)> empty_self_fn;
        iss >> empty_self_fn;
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        std::wstring tok;
        iss >> tok;
        VERIFY( tok == L"hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_istream_ws_no_ctype_char_1: removing the ctype facet makes
// the sentry's whitespace-skip fail with stream_error -> strfailbit (no mask -> no throw).
void test_istream_ws_no_ctype_wchar_t_1()
{
    dump_info("Test istream<wchar_t> sentry with no ctype facet case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const auto loc = IOv2::locale<wchar_t>("C").remove<IOv2::ctype_conf<wchar_t>>();

        T iss{IOv2::mem_device{std::wstring(L"  42")}, loc};
        iss >> IOv2::ws;
        VERIFY( iss.str_fail() );

        T iss2{IOv2::mem_device{std::wstring(L"  42")}, loc};
        int v = 0;
        iss2 >> v;
        VERIFY( iss2.str_fail() );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}


#include <functional>
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
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

void test_istream_ws_char_1()
{
    dump_info("Test istream<char> with ws case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str01(" santa barbara ");

        // template<_CharT, _Traits>
        //  basic_istream<_CharT, _Traits>& ws(basic_istream<_Char, _Traits>& is)
        T iss01{IOv2::mem_device{str01}};
        T iss02{IOv2::mem_device{str01}};

        std::string str04;
        std::string str05;
        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == "santa" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == "santa" );
        VERIFY( str05 == str04 );

        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == "barbara" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == "barbara" );
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

// Regression for the std::function manipulator being shadowed on the extraction side.
// A std::function manipulator held in a NON-CONST lvalue must dispatch to the
// operator>>(T&, const std::function<void(T&)>&) manipulator overload. Before the generic
// value operator>> was constrained (requires is_reader_def<...>), such an lvalue bound to
// operator>>(T&, TValue&) instead and failed to compile ("No parse method provided").
// The invocation counter proves dispatch reached the manipulator (the generic operator
// would never invoke the callable). Also exercises the sibling function-pointer form.
void test_istream_function_manip_char_1()
{
    dump_info("Test istream<char> std::function manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::string("hello world")}};
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
        std::string tok;
        iss >> tok;
        VERIFY( tok == "hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}


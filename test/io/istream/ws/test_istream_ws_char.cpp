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

// A null manipulator (null function pointer / empty std::function) passed to operator>>
// must be rejected by every manipulator overload: the operator throws stream_error, its
// own handler categorizes it into strfailbit, and -- with no exception mask set -- returns
// the stream without throwing. This exercises the error branch of each of the four
// manipulator overloads (both function-pointer and std::function forms, both the
// ios_base<char>& and the stream-typed callables).
void test_istream_null_manip_char_1()
{
    dump_info("Test istream<char> null manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::string("hello world")}};
        using S = decltype(iss);

        // overload: operator>>(T&, void(*)(ios_base<char>&))
        iss >> static_cast<void(*)(IOv2::ios_base<char>&)>(nullptr);
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        // overload: operator>>(T&, const std::function<void(ios_base<char>&)>&)
        std::function<void(IOv2::ios_base<char>&)> empty_base_fn;
        iss >> empty_base_fn;
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        // same overload, non-null: the callable runs against the stream's ios_base
        int base_calls = 0;
        std::function<void(IOv2::ios_base<char>&)> base_fn =
            [&base_calls](IOv2::ios_base<char>&){ ++base_calls; };
        iss >> base_fn;
        VERIFY( base_calls == 1 );
        VERIFY( !(iss.rdstate() & IOv2::ios_defs::strfailbit) );

        // overload: operator>>(T&, void(*)(T&))
        iss >> static_cast<void(*)(S&)>(nullptr);
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        // overload: operator>>(T&, const std::function<void(T&)>&)
        std::function<void(S&)> empty_self_fn;
        iss >> empty_self_fn;
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        // stream is still usable after all the rejected manipulators
        std::string tok;
        iss >> tok;
        VERIFY( tok == "hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// The input sentry classifies leading whitespace through the ctype facet. With that facet
// removed from the locale, the sentry's skip step throws stream_error ("no ctype facet"),
// which is reported as strfailbit; the follow-up validity check inside the sentry then
// re-throws on the now-failed stream. Both the ws manipulator and skipws-enabled
// extraction drive this path. With no exception mask set nothing escapes to the caller.
void test_istream_ws_no_ctype_char_1()
{
    dump_info("Test istream<char> sentry with no ctype facet case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const auto loc = IOv2::locale<char>("C").remove<IOv2::ctype_conf<char>>();

        // ws manipulator: sentry constructed with noskip == false -> needs the ctype facet
        T iss{IOv2::mem_device{std::string("  42")}, loc};
        iss >> IOv2::ws;
        VERIFY( iss.str_fail() );

        // skipws-enabled extraction takes the same sentry skip path
        T iss2{IOv2::mem_device{std::string("  42")}, loc};
        int v = 0;
        iss2 >> v;
        VERIFY( iss2.str_fail() );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}


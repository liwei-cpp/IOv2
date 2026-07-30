#include <functional>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <facet/ctype.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>


void test_ostream_endl_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> with endl case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str01(L" santa barbara ");    
        auto oss01 = T(IOv2::mem_device{str01});
        auto oss02 = T(IOv2::mem_device{L""});

        oss01 << IOv2::endl;
        VERIFY(oss01.device().str().size() == str01.size());

        oss02 << IOv2::endl;
        VERIFY(oss02.device().str().size() == 1);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_ostream_function_manip_char_1: a std::function manipulator
// must dispatch to operator<<(T&, const std::function<void(T&)>&); guards that value
// insertion and both manipulator forms keep working after the generic operator<< was
// constrained (requires is_writer_def<...>).
void test_ostream_function_manip_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> std::function manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::wstring(L"")});
        int calls = 0;
        std::function<void(IOv2::ios_base<wchar_t>&)> manip =
            [&calls](IOv2::ios_base<wchar_t>&){ ++calls; };

        oss << manip;                 // std::function manipulator via operator<<
        VERIFY( calls == 1 );

        oss << manip << manip;        // operator<< returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // sibling function-pointer form: operator<<(T&, void(*)(ios_base<wchar_t>&))
        static int fcalls;
        fcalls = 0;
        oss << +[](IOv2::ios_base<wchar_t>&){ ++fcalls; };
        VERIFY( fcalls == 1 );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_ostream_null_manip_char_1: a null manipulator must be
// rejected by every manipulator overload, leaving strfailbit set (no mask -> no throw).
void test_ostream_null_manip_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> null manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::wstring(L"")});

        oss << static_cast<void(*)(IOv2::ios_base<wchar_t>&)>(nullptr);
        VERIFY( oss.rdstate() & IOv2::ios_defs::strfailbit );
        oss.clear();

        std::function<void(IOv2::ios_base<wchar_t>&)> empty_base_fn;
        oss << empty_base_fn;
        VERIFY( oss.rdstate() & IOv2::ios_defs::strfailbit );
        oss.clear();

        int base_calls = 0;
        std::function<void(IOv2::ios_base<wchar_t>&)> base_fn =
            [&base_calls](IOv2::ios_base<wchar_t>&){ ++base_calls; };
        oss << base_fn;
        VERIFY( base_calls == 1 );
        VERIFY( !(oss.rdstate() & IOv2::ios_defs::strfailbit) );

        oss << L"ok";
        auto [dev, err] = oss.detach();
        VERIFY( dev.str() == L"ok" );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_ostream_manip_direct_call_char_1.
void test_ostream_manip_direct_call_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> manipulator direct-call form case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::wstring(L"")});
        IOv2::endl(oss);
        VERIFY( oss.device().str() == L"\n" );
        VERIFY( oss.good() );

        IOv2::ends(oss);
        VERIFY( oss.device().str().size() == 2 );
        VERIFY( oss.good() );

        IOv2::flush(oss);
        VERIFY( oss.good() );

        auto bad = T(IOv2::mem_device{std::wstring(L"")},
                     IOv2::locale<wchar_t>("C").remove<IOv2::ctype_conf<wchar_t>>());
        IOv2::endl(bad);
        VERIFY( bad.str_fail() );
        VERIFY( !bad.good() );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

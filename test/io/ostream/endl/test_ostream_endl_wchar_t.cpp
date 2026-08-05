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

// wchar_t counterpart of test_ostream_function_manip_char_1: a function-pointer manipulator
// must dispatch to operator<<(T&, void(*)(ios_base<wchar_t>&)) rather than to the generic
// operator<<.
void test_ostream_function_manip_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> function-pointer manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::wstring(L"")});
        static int calls;
        calls = 0;
        void (*manip)(IOv2::ios_base<wchar_t>&) = [](IOv2::ios_base<wchar_t>&){ ++calls; };

        oss << manip;
        VERIFY( calls == 1 );

        oss << manip << manip;        // operator<< returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // a capture-less lambda reaches the same overload once decayed with unary +
        oss << +[](IOv2::ios_base<wchar_t>&){ ++calls; };
        VERIFY( calls == 4 );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_ostream_null_manip_char_1: a null manipulator must be
// rejected, leaving strfailbit set (no mask -> no throw).
void test_ostream_null_manip_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> null manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::wstring(L"")});

        oss << static_cast<void(*)(IOv2::ios_base<wchar_t>&)>(nullptr);
        VERIFY( oss.rdstate() & IOv2::ios_defs::strfailbit );
        oss.clear();

        static int base_calls;
        base_calls = 0;
        oss << +[](IOv2::ios_base<wchar_t>&){ ++base_calls; };
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

// wchar_t counterpart of test_ostream_manip_tag_char_1.
void test_ostream_manip_tag_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> tag manipulators case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::wstring(L"")});
        oss << IOv2::endl;
        VERIFY( oss.device().str() == L"\n" );
        VERIFY( oss.good() );

        oss << IOv2::ends;
        VERIFY( oss.device().str().size() == 2 );
        VERIFY( oss.good() );

        oss << IOv2::flush;
        VERIFY( oss.good() );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

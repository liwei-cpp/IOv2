#include <functional>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
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
        using S = decltype(oss);

        int calls = 0;
        std::function<void(S&)> manip = [&calls](S&){ ++calls; };

        oss << manip;                 // std::function manipulator via operator<<
        VERIFY( calls == 1 );

        oss << manip << manip;        // operator<< returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // sibling function-pointer manipulator form: operator<<(T&, void(*)(T&))
        static int fcalls;
        fcalls = 0;
        oss << +[](S&){ ++fcalls; };
        VERIFY( fcalls == 1 );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

#include <functional>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_ostream_endl_char_1()
{
    dump_info("Test ostream<char> with endl case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str01(" santa barbara ");    
        auto oss01 = T(IOv2::mem_device{" santa barbara "});
        auto oss02 = T(IOv2::mem_device{""});
        
        oss01 << IOv2::endl;
        VERIFY(oss01.device().str().size() == str01.size());

        oss02 << IOv2::endl;
        VERIFY(oss02.device().str().size() == 1);
    };
    
    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// Insertion-side companion to test_istream_function_manip_char_1. operator<< already
// resolved a std::function manipulator correctly (its generic takes const TValue&, so the
// manipulator overload wins on partial ordering), but the generic was likewise constrained
// (requires is_writer_def<...>) for symmetry; this guards that both manipulator forms keep
// dispatching after that change. The invocation counter proves the manipulator overload ran.
void test_ostream_function_manip_char_1()
{
    dump_info("Test ostream<char> std::function manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::string("")});
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

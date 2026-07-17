#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

static_assert(std::is_copy_constructible_v<decltype(IOv2::ostream(IOv2::mem_device<wchar_t>{}))>,
              "IOv2::ostream<mem_device<wchar_t>> must remain copy-constructible");
static_assert(std::is_copy_constructible_v<decltype(IOv2::iostream(IOv2::mem_device<wchar_t>{}))>,
              "IOv2::iostream<mem_device<wchar_t>> must remain copy-constructible");

void test_ostream_flush_wchar_t_1()
{
    dump_info("Test ostream<wchar_t>::flush case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str01(L" santa barbara ");
        T oss01(IOv2::mem_device{str01});
        T oss02(IOv2::mem_device{L""});

        oss01.flush();
        VERIFY(oss01.device().str() == str01);
    
        oss02.flush();
        VERIFY(oss02.device().str().empty());
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_flush_wchar_t_2()
{
    dump_info("Test ostream<wchar_t>::flush case 2 (tie cycle rejected)...");

    auto helper = []<template<typename, typename> class T>()
    {
        T a(IOv2::mem_device{L""});
        T b(IOv2::mem_device{L""});

        // A self-tie is the length-1 cycle and is rejected.
        bool threw = false;
        try { a.tie(&a); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
        VERIFY(a.tie() == nullptr);

        a.tie(&b);

        // Closing the cycle a -> b -> a is rejected at set time; b stays untied.
        threw = false;
        try { b.tie(&a); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
        VERIFY(b.tie() == nullptr);
        VERIFY(a.tie() == &b);

        a.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_flush_wchar_t_3()
{
    dump_info("Test ostream<wchar_t>::flush case 3 (concurrent flush contention)...");

    auto helper = []<template<typename, typename> class T>()
    {
        constexpr size_t thread_num = 8;
        constexpr size_t loop_num = 2000;

        T ostr{IOv2::mem_device{L""}};
        std::vector<std::thread> tr_vec;
        tr_vec.reserve(thread_num);

        for (size_t thread_ID = 0; thread_ID < thread_num; ++thread_ID)
        {
            tr_vec.emplace_back([&ostr]()
            {
                for (size_t i = 0; i < loop_num; ++i)
                    ostr.flush();
            });
        }

        for (auto& tr : tr_vec)
            tr.join();

        VERIFY(ostr.good());
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
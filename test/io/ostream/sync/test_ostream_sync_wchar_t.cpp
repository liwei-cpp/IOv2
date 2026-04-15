#include <cfloat>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_ostream_sync_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> sync-output case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        constexpr size_t thread_num = 10;
        constexpr size_t loop_num = 1024;

        T ostr{IOv2::mem_device{L""}};
        std::vector<std::thread> tr_vec;
        tr_vec.reserve(thread_num);

        for (size_t thread_ID = 0; thread_ID < thread_num; ++thread_ID)
        {
            std::thread tr([&ostr]()
            {
                for (size_t i = 0; i < loop_num; ++i)
                    IOv2::sync(ostr).stream << 123 << L' ' << 456 << L'\n';
            });
            tr_vec.push_back(std::move(tr));
        }

        for (auto& tr : tr_vec)
            tr.join();

        std::wstring ref;
        ref.reserve(loop_num * thread_num * 8);
        for (size_t i = 0; i < loop_num * thread_num; ++i)
            ref += L"123 456\n";

        VERIFY(ostr.detach().str() == ref);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
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
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_istream_sync_char_1()
{
    dump_info("Test istream<char> sync-input case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        constexpr size_t thread_num = 10;
        constexpr size_t loop_num = 1024;

        std::string ref;
        ref.reserve(loop_num * thread_num * 8);
        for (size_t i = 0; i < loop_num * thread_num; ++i)
            ref += "123 456\n";

        T istr{IOv2::mem_device{ref}};
        std::vector<std::thread> tr_vec;
        tr_vec.reserve(thread_num);

        for (size_t thread_ID = 0; thread_ID < thread_num; ++thread_ID)
        {
            std::thread tr([&istr]()
            {
                for (size_t i = 0; i < loop_num; ++i)
                {
                    int v1 = 0;
                    int v2 = 0;
                    IOv2::sync(istr).stream >> v1 >> v2;
                    VERIFY(v1 == 123);
                    VERIFY(v2 == 456);
                }
            });
            tr_vec.push_back(std::move(tr));
        }

        for (auto& tr : tr_vec)
            tr.join();
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
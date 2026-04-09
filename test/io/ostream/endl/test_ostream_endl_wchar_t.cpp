#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>


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
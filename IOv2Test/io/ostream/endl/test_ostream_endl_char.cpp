#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

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
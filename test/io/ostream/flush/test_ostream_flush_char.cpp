#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_ostream_flush_char_1()
{
    dump_info("Test ostream<char>::flush case 1...");
    
    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str01(" santa barbara ");
        T oss01(IOv2::mem_device{str01});
        T oss02(IOv2::mem_device{""});

        oss01.flush();
        VERIFY(oss01.device().str() == str01);
        
        oss02.flush();
        VERIFY(oss02.device().str().empty());
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    
    dump_info("Done\n");
}
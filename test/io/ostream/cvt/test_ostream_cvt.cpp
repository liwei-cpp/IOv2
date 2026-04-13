#include <stdexcept>
#include <string>

#include <cvt/code_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/crypt/hash_cvt.h>
#include <cvt/cvt_pipe_creator.h>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <locale/locale.h>

#include <common/dump_info.h>
#include <common/verify.h>

void test_ostream_cvt_1()
{
    dump_info("Test ostream cvt case 1...");

    auto creator = IOv2::Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                   IOv2::code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    auto helper = [&creator]<template<typename, typename> class T>()
    {
        T os(IOv2::mem_device{""}, creator);
        static_assert(std::is_same_v<typename decltype(os)::char_type, char32_t>);
    
        os << 1024 << U' ' << U"李伟";
    
        auto str = os.detach().str();
        VERIFY(os.good());
        VERIFY(str.size() == 11);
        VERIFY((unsigned char)str[ 0] == (unsigned char)('1' + 'a'));
        VERIFY((unsigned char)str[ 1] == (unsigned char)('0' + 'b'));
        VERIFY((unsigned char)str[ 2] == (unsigned char)('2' + 'c'));
        VERIFY((unsigned char)str[ 3] == (unsigned char)('4' + 'd'));
        VERIFY((unsigned char)str[ 4] == (unsigned char)(' ' + 'e'));
        VERIFY((unsigned char)str[ 5] == (unsigned char)('\xE6' + 'f'));
        VERIFY((unsigned char)str[ 6] == (unsigned char)('\x9D' + 'g'));
        VERIFY((unsigned char)str[ 7] == (unsigned char)('\x8E' + 'a'));
        VERIFY((unsigned char)str[ 8] == (unsigned char)('\xE4' + 'b'));
        VERIFY((unsigned char)str[ 9] == (unsigned char)('\xBC' + 'c'));
        VERIFY((unsigned char)str[10] == (unsigned char)('\x9F' + 'd'));
    };
    
    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_cvt_sync_1()
{
    dump_info("Test ostream cvt (with sync) case 1...");

    auto creator = IOv2::Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                   IOv2::code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    auto helper = [&creator]<template<typename, typename> class T>()
    {
        T os(IOv2::mem_device{""}, creator);
        static_assert(std::is_same_v<typename decltype(os)::char_type, char32_t>);
    
        IOv2::sync(os).stream << 1024 << U' ' << U"李伟";
    
        auto str = IOv2::sync(os).stream.detach().str();
        VERIFY(os.good());
        VERIFY(str.size() == 11);
        VERIFY((unsigned char)str[ 0] == (unsigned char)('1' + 'a'));
        VERIFY((unsigned char)str[ 1] == (unsigned char)('0' + 'b'));
        VERIFY((unsigned char)str[ 2] == (unsigned char)('2' + 'c'));
        VERIFY((unsigned char)str[ 3] == (unsigned char)('4' + 'd'));
        VERIFY((unsigned char)str[ 4] == (unsigned char)(' ' + 'e'));
        VERIFY((unsigned char)str[ 5] == (unsigned char)('\xE6' + 'f'));
        VERIFY((unsigned char)str[ 6] == (unsigned char)('\x9D' + 'g'));
        VERIFY((unsigned char)str[ 7] == (unsigned char)('\x8E' + 'a'));
        VERIFY((unsigned char)str[ 8] == (unsigned char)('\xE4' + 'b'));
        VERIFY((unsigned char)str[ 9] == (unsigned char)('\xBC' + 'c'));
        VERIFY((unsigned char)str[10] == (unsigned char)('\x9F' + 'd'));
    };
    
    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_cvt()
{
    test_ostream_cvt_1();
    test_ostream_cvt_sync_1();
}
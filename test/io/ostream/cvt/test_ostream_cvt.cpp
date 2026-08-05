#include <stdexcept>
#include <string>

#include <cvt/code_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/crypt/hash_cvt.h>
#include <cvt/cvt_pipe_creator.h>
#include <device/mem_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <locale/locale.h>

#include <support/dump_info.h>
#include <support/verify.h>

void test_ostream_cvt_1()
{
    dump_info("Test ostream cvt case 1...");

    auto creator = IOv2::Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                   IOv2::code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    auto helper = [&creator]<template<typename, typename> class T>()
    {
        T os(IOv2::mem_device{""}, creator, IOv2::locale<char32_t>("C"));
        static_assert(std::is_same_v<typename decltype(os)::char_type, char32_t>);
    
        os << 1024 << U' ' << U"李伟";

        auto [dev1, err1] = os.detach();
        auto str = dev1.str();
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
        T os(IOv2::mem_device{""}, creator, IOv2::locale<char32_t>("C"));
        static_assert(std::is_same_v<typename decltype(os)::char_type, char32_t>);
    
        IOv2::sync(os).stream << 1024 << U' ' << U"李伟";

        auto [dev2, err2] = IOv2::sync(os).stream.detach();
        auto str = dev2.str();
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

// Appmode requires a fixed-length, state-independent encoding so the output sentry can
// reposition to the end (rseek(0)) before every insertion. A UTF-8 code_cvt is
// variable-length, so once appmode is set, the sentry's rseek(0) throws cvt_error; the
// sentry rewraps it and operator<< classifies it into cvtfailbit. With no exception mask
// set nothing escapes to the caller. This drives the appmode/cvt_error branch of out_sentry.
void test_ostream_cvt_appmode_1()
{
    dump_info("Test ostream cvt appmode (variable-length encoding) case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto creator = IOv2::code_cvt_creator<char, char32_t>("zh_CN.UTF-8");
        T os(IOv2::mem_device{""}, creator, IOv2::locale<char32_t>("C"));

        os << U"李伟";               // ordinary (non-appmode) insertion succeeds
        os.flush();
        VERIFY( os.good() );

        os << IOv2::appmode;         // request append semantics
        VERIFY( os.good() );

        os << U"X";                  // sentry rseek(0) over UTF-8 -> cvt_error -> cvtfailbit
        VERIFY( !os.good() );
        VERIFY( os.rdstate() & IOv2::ios_defs::cvtfailbit );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_cvt()
{
    test_ostream_cvt_1();
    test_ostream_cvt_sync_1();
    test_ostream_cvt_appmode_1();
}
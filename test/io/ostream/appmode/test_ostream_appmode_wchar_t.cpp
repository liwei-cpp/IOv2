#include <cfloat>
#include <limits>
#include <stdexcept>
#include <string>
#include <cvt/code_cvt.h>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_ostream_appmode_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> with app mode case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        T ostr{IOv2::mem_device{L"abcde"}};
        ostr.put(L'L');
        ostr.flush();
        VERIFY(ostr.device().str() == L"Lbcde");

        ostr << IOv2::appmode;
        ostr.put(L'W');
        ostr.flush();
        VERIFY(ostr.device().str() == L"LbcdeW");

        ostr.seek(0);
        VERIFY(static_cast<bool>(ostr));

        ostr.put(L'X');
        ostr.flush();

        ostr.seek(1);
        VERIFY(static_cast<bool>(ostr));
        ostr << IOv2::noappmode;
        ostr.put(L'Y');
        ostr << IOv2::appmode;
        ostr.flush();
        VERIFY(ostr.device().str() == L"LYcdeWX");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_appmode_wchar_t_sync_1()
{
    dump_info("Test ostream<wchar_t> with app mode (with sync) case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        T ostr{IOv2::mem_device{L"abcde"}};
        IOv2::sync(ostr).stream.put(L'L');
        IOv2::sync(ostr).stream.flush();
        VERIFY(IOv2::sync(ostr).stream.device().str() == L"Lbcde");

        IOv2::sync(ostr).stream << IOv2::appmode;
        IOv2::sync(ostr).stream.put(L'W');
        IOv2::sync(ostr).stream.flush();
        VERIFY(IOv2::sync(ostr).stream.device().str() == L"LbcdeW");

        IOv2::sync(ostr).stream.seek(0);
        VERIFY(static_cast<bool>(IOv2::sync(ostr).stream));

        IOv2::sync(ostr).stream.put(L'X');
        IOv2::sync(ostr).stream.flush();

        IOv2::sync(ostr).stream.seek(1);
        VERIFY(static_cast<bool>(IOv2::sync(ostr).stream));
        IOv2::sync(ostr).stream << IOv2::noappmode;
        IOv2::sync(ostr).stream.put(L'Y');
        IOv2::sync(ostr).stream << IOv2::appmode;
        IOv2::sync(ostr).stream.flush();
        VERIFY(IOv2::sync(ostr).stream.device().str() == L"LYcdeWX");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_appmode_wchar_t_2()
{
    dump_info("Test ostream<wchar_t> with app mode case 2...");
    auto helper = []<template<typename, typename> class T>()
    {
        file_guard g("appmode_test", "abcde");
        T ostr{IOv2::file_device<char>{"appmode_test"},
               IOv2::code_cvt_creator<char, wchar_t>("C")};

        ostr.put('L');
        ostr << IOv2::appmode;
        ostr.put('W');
        ostr.seek(0);
        ostr.put('X');
        ostr.seek(1);
        ostr << IOv2::noappmode;
        ostr.put('Y');
        ostr << IOv2::appmode;
        ostr.flush();
        VERIFY(static_cast<bool>(ostr));
        ostr.detach().close();

        VERIFY(g.contents() == "LYcdeWX");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_appmode_wchar_t_sync_2()
{
    dump_info("Test ostream<wchar_t> with app mode case (with sync) 2...");
    auto helper = []<template<typename, typename> class T>()
    {
        file_guard g("appmode_test", "abcde");
        T ostr{IOv2::file_device<char>{"appmode_test"},
               IOv2::code_cvt_creator<char, wchar_t>("C")};

        IOv2::sync(ostr).stream.put('L');
        IOv2::sync(ostr).stream << IOv2::appmode;
        IOv2::sync(ostr).stream.put('W');
        IOv2::sync(ostr).stream.seek(0);
        IOv2::sync(ostr).stream.put('X');
        IOv2::sync(ostr).stream.seek(1);
        IOv2::sync(ostr).stream << IOv2::noappmode;
        IOv2::sync(ostr).stream.put('Y');
        IOv2::sync(ostr).stream << IOv2::appmode;
        IOv2::sync(ostr).stream.flush();
        VERIFY(static_cast<bool>(IOv2::sync(ostr).stream));
        IOv2::sync(ostr).stream.detach().close();

        VERIFY(g.contents() == "LYcdeWX");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_appmode_wchar_t_3()
{
    dump_info("Test ostream<wchar_t> with app mode case 3...");

    IOv2::iostream str{IOv2::mem_device{L"abcde"}};
    str << IOv2::appmode << L'W';
    str.flush();
    VERIFY(str.device().str() == L"abcdeW");

    wchar_t ch = 0;
    str.get(ch);
    VERIFY(static_cast<bool>(str));
    VERIFY(ch == 0);
    VERIFY(str.eof());
    str.clear();

    str << L" hello";

    VERIFY(str.detach().str() == L"abcdeW hello");

    dump_info("Done\n");
}

void test_ostream_appmode_wchar_t_sync_3()
{
    dump_info("Test ostream<wchar_t> with app mode case (with sync) 3...");

    IOv2::iostream str{IOv2::mem_device{L"abcde"}};
    IOv2::sync(str).stream << IOv2::appmode << L'W';
    IOv2::sync(str).stream.flush();
    VERIFY(IOv2::sync(str).stream.device().str() == L"abcdeW");

    wchar_t ch = 0;
    IOv2::sync(str).stream.get(ch);
    VERIFY(static_cast<bool>(IOv2::sync(str).stream));
    VERIFY(ch == 0);
    VERIFY(IOv2::sync(str).stream.eof());
    IOv2::sync(str).stream.clear();

    IOv2::sync(str).stream << L" hello";

    VERIFY(IOv2::sync(str).stream.detach().str() == L"abcdeW hello");

    dump_info("Done\n");
}
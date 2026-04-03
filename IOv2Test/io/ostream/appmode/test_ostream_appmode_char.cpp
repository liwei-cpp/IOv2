#include <cfloat>
#include <limits>
#include <stdexcept>
#include <string>
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

void test_ostream_appmode_char_1()
{
    dump_info("Test ostream<char> with app mode case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        T ostr{IOv2::mem_device{"abcde"}};
        ostr.put('L');
        ostr.flush();
        VERIFY(ostr.device().str() == "Lbcde");

        ostr << IOv2::appmode;
        ostr.put('W');
        ostr.flush();
        VERIFY(ostr.device().str() == "LbcdeW");

        ostr.seek(0);
        VERIFY(static_cast<bool>(ostr));

        ostr.put('X');
        ostr.flush();

        ostr.seek(1);
        VERIFY(static_cast<bool>(ostr));
        ostr << IOv2::noappmode;
        ostr.put('Y');
        ostr << IOv2::appmode;
        ostr.flush();
        VERIFY(ostr.device().str() == "LYcdeWX");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_appmode_char_sync_1()
{
    dump_info("Test ostream<char> with app mode (with sync) case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        T ostr{IOv2::mem_device{"abcde"}};
        IOv2::sync(ostr).stream.put('L');
        IOv2::sync(ostr).stream.flush();
        VERIFY(IOv2::sync(ostr).stream.device().str() == "Lbcde");

        IOv2::sync(ostr).stream << IOv2::appmode;
        IOv2::sync(ostr).stream.put('W');
        IOv2::sync(ostr).stream.flush();
        VERIFY(IOv2::sync(ostr).stream.device().str() == "LbcdeW");

        IOv2::sync(ostr).stream.seek(0);
        VERIFY(static_cast<bool>(IOv2::sync(ostr).stream));

        IOv2::sync(ostr).stream.put('X');
        IOv2::sync(ostr).stream.flush();

        IOv2::sync(ostr).stream.seek(1);
        VERIFY(static_cast<bool>(IOv2::sync(ostr).stream));
        IOv2::sync(ostr).stream << IOv2::noappmode;
        IOv2::sync(ostr).stream.put('Y');
        IOv2::sync(ostr).stream << IOv2::appmode;
        IOv2::sync(ostr).stream.flush();
        VERIFY(IOv2::sync(ostr).stream.device().str() == "LYcdeWX");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_appmode_char_2()
{
    dump_info("Test ostream<char> with app mode case 2...");
    auto helper = []<template<typename, typename> class T>()
    {
        file_guard g("appmode_test", "abcde");
        T ostr{IOv2::file_device<char>{"appmode_test"}};

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

void test_ostream_appmode_char_sync_2()
{
    dump_info("Test ostream<char> with app mode (with sync) case 2...");
    auto helper = []<template<typename, typename> class T>()
    {
        file_guard g("appmode_test", "abcde");
        T ostr{IOv2::file_device<char>{"appmode_test"}};

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

void test_ostream_appmode_char_3()
{
    dump_info("Test ostream<char> with app mode case 3...");

    IOv2::iostream str{IOv2::mem_device{"abcde"}};
    str << IOv2::appmode << 'W';
    str.flush();
    VERIFY(str.device().str() == "abcdeW");

    char ch = 0;
    str.get(ch);
    VERIFY(static_cast<bool>(str));
    VERIFY(ch == 0);
    VERIFY(str.eof());
    str.clear();

    str << " hello";

    VERIFY(str.detach().str() == "abcdeW hello");

    dump_info("Done\n");
}

void test_ostream_appmode_char_sync_3()
{
    dump_info("Test ostream<char> with app mode (with sync) case 3...");

    IOv2::iostream str{IOv2::mem_device{"abcde"}};
    IOv2::sync(str).stream << IOv2::appmode << 'W';
    IOv2::sync(str).stream.flush();
    VERIFY(IOv2::sync(str).stream.device().str() == "abcdeW");

    char ch = 0;
    IOv2::sync(str).stream.get(ch);
    VERIFY(static_cast<bool>(IOv2::sync(str).stream));
    VERIFY(ch == 0);
    IOv2::sync(str).stream.clear();

    IOv2::sync(str).stream << " hello";

    VERIFY(IOv2::sync(str).stream.detach().str() == "abcdeW hello");

    dump_info("Done\n");
}


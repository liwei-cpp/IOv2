#include <stdexcept>
#include <string>
#include <cvt/code_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

namespace
{
    std::string s_e_lit = []()
    {
        std::string e_lit; e_lit.resize(4102);
        for (int i = 0; i < 4102; i += 7)
        {
            e_lit[i+0] = '\xE6';
            e_lit[i+1] = '\x9D';
            e_lit[i+2] = '\x8E';
            e_lit[i+3] = '\xE4';
            e_lit[i+4] = '\xBC';
            e_lit[i+5] = '\x9F';
            e_lit[i+6] = (i / 7) % 127 + 1;
        }
        return e_lit;
    }();
}

void test_iostream_switch_to_get_char_1()
{
    dump_info("Test iostream<char>::switch_to_get case 1...");

    {
        IOv2::iostream str(IOv2::mem_device{""});
        str.switch_to_get();
        VERIFY(static_cast<bool>(str));
    }

    {
        IOv2::iostream str(IOv2::mem_device{"abcde"});
        str.switch_to_get();
        VERIFY(static_cast<bool>(str));
    }

    dump_info("Done\n");
}

void test_iostream_switch_to_get_char_2()
{
    dump_info("Test iostream<char>::switch_to_get case 2...");

    IOv2::ostream ostr(IOv2::mem_device{""},
                       IOv2::Comp::zlib_cvt_creator<char>{6});
    ostr << s_e_lit;
    VERIFY(static_cast<bool>(ostr));
    std::string compress_res = ostr.detach().str();

    VERIFY(!compress_res.empty());
    VERIFY(compress_res.size() < s_e_lit.size());

    IOv2::iostream istr(IOv2::mem_device{compress_res},
                        IOv2::Comp::zlib_cvt_creator<char>{6});
    istr.switch_to_get();
    VERIFY(static_cast<bool>(istr));

    dump_info("Done\n");
}

void test_iostream_switch_to_get_char_3()
{
    dump_info("Test iostream<char>::switch_to_get case 3...");

    IOv2::iostream str(IOv2::mem_device{""},
                       IOv2::code_cvt_creator<char, wchar_t>("C"));
    str << L"abcde";
    VERIFY(static_cast<bool>(str));

    str.seek(0);
    VERIFY(static_cast<bool>(str));

    str.switch_to_get();
    VERIFY(static_cast<bool>(str));

    dump_info("Done\n");
}

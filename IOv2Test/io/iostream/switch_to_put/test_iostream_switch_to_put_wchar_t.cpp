#include <limits>
#include <stdexcept>
#include <string>
#include <cvt/code_cvt.h>
#include <cvt/cvt_pipe_creator.h>
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
    std::wstring s_e_lit = []()
    {
        std::wstring e_lit; e_lit.resize(1758);
        for (int i = 0; i < 1758; i += 3)
        {
            e_lit[i+0] = L'李';
            e_lit[i+1] = L'伟';
            e_lit[i+2] = (i / 3) % 127 + 1;
        }
        return e_lit;
    }();
}

void test_iostream_switch_to_put_wchar_t_1()
{
    dump_info("Test iostream<wchar_t>::switch_to_put case 1...");

    {
        IOv2::iostream str(IOv2::mem_device{L""});
        str.switch_to_put();
        VERIFY(static_cast<bool>(str));
    }

    {
        IOv2::iostream str(IOv2::mem_device{L"abcde"});
        str.switch_to_put();
        VERIFY(static_cast<bool>(str));
    }

    dump_info("Done\n");
}

void test_iostream_switch_to_put_wchar_t_2()
{
    dump_info("Test iostream<wchar_t>::switch_to_put case 2...");

    IOv2::ostream ostr(IOv2::mem_device{""},
                       IOv2::Comp::zlib_cvt_creator<char>{6} | IOv2::code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
    ostr << s_e_lit;
    VERIFY(static_cast<bool>(ostr));
    auto compress_res = ostr.detach().str();

    VERIFY(!compress_res.empty());
    VERIFY(compress_res.size() < s_e_lit.size() / 3 * 7);

    IOv2::iostream istr(IOv2::mem_device{compress_res},
                        IOv2::Comp::zlib_cvt_creator<char>{6});
    istr.switch_to_put();
    VERIFY(!istr);

    dump_info("Done\n");
}

void test_iostream_switch_to_put_wchar_t_3()
{
    dump_info("Test iostream<wchar_t>::switch_to_put case 3...");

    IOv2::iostream str(IOv2::mem_device{""},
                       IOv2::code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
    str << L"abcde";
    VERIFY(static_cast<bool>(str));

    str.seek(0);
    VERIFY(!str);

    str.clear();
    str.switch_to_get();
    str.seek(0);
    VERIFY(static_cast<bool>(str));

    str.switch_to_put();
    VERIFY(!str);

    str.clear();
    str.switch_to_get();
    str.rseek(0);
    VERIFY(!str);
    str.clear();
    str.switch_to_put();
    VERIFY(!str);

    dump_info("Done\n");
}

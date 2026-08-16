#include <stdexcept>
#include <string>
#include <type_traits>
#include <cvt/code_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <device/mem_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

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
    auto [dev, err] = ostr.detach();
    std::string compress_res = dev.str();

    VERIFY(!compress_res.empty());
    VERIFY(compress_res.size() < s_e_lit.size());

    // A zlib pipeline cannot change direction (support_io_switch is false), so an iostream over
    // one is rejected at the declaration level by base_streambuf's creator constructor
    // (io_concepts.h: cvt_fits_direction). What this case used to do -- build such an iostream
    // and drive switch_to_get() into a run-time cvtfailbit -- is no longer expressible, so the
    // rejection itself is what gets pinned down here.
    static_assert(!std::is_constructible_v<IOv2::iostream<IOv2::mem_device<char>, char>,
                                           IOv2::mem_device<char>,
                                           IOv2::Comp::zlib_cvt_creator<char>>);
    // The single-direction halves stay legal: zlib supports get and put, just not switching.
    static_assert(std::is_constructible_v<IOv2::ostream<IOv2::mem_device<char>, char>,
                                          IOv2::mem_device<char>,
                                          IOv2::Comp::zlib_cvt_creator<char>>);
    static_assert(std::is_constructible_v<IOv2::istream<IOv2::mem_device<char>, char>,
                                          IOv2::mem_device<char>,
                                          IOv2::Comp::zlib_cvt_creator<char>>);

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

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <string>
#include <type_traits>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/cvt_pipe_creator.h>
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/iostream.h>

#include <gtest/gtest.h>

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

TEST(IostreamSwitchToPutWchar, SwitchingOnAFreshStreamIsAlwaysAllowed)
{
    {
        IOv2::iostream str(IOv2::mem_device{L""});
        str.switch_to_put();
        EXPECT_TRUE(static_cast<bool>(str));
    }

    {
        IOv2::iostream str(IOv2::mem_device{L"abcde"});
        str.switch_to_put();
        EXPECT_TRUE(static_cast<bool>(str));
    }
}

TEST(IostreamSwitchToPutWchar, APipelineThatCannotSwitchIsRejectedAtTheDeclaration)
{
    IOv2::ostream ostr(IOv2::mem_device{""},
                       IOv2::Comp::zlib_cvt_creator<char>{6} | IOv2::code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
    ostr << s_e_lit;
    EXPECT_TRUE(static_cast<bool>(ostr));
    auto [dev, err] = ostr.detach();
    auto compress_res = dev.str();

    EXPECT_FALSE(compress_res.empty());
    EXPECT_TRUE(compress_res.size() < s_e_lit.size() / 3 * 7);

    // A zlib pipeline cannot change direction (support_io_switch is false), so an iostream over
    // one is rejected at the declaration level by base_streambuf's creator constructor
    // (io_concepts.h: cvt_fits_direction). What this case used to do -- build such an iostream
    // and drive switch_to_put() into a run-time cvtfailbit -- is no longer expressible, so the
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
}

TEST(IostreamSwitchToPutWchar, SwitchingAfterWritingThroughAConverterKeepsTheStreamGood)
{
    IOv2::iostream str(IOv2::mem_device{""},
                       IOv2::code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
    str << L"abcde";
    EXPECT_TRUE(static_cast<bool>(str));

    str.seek(0);
    EXPECT_FALSE(str);

    str.clear();
    str.switch_to_get();
    str.seek(0);
    EXPECT_TRUE(static_cast<bool>(str));

    str.switch_to_put();
    EXPECT_FALSE(str);

    str.clear();
    str.switch_to_get();
    str.rseek(0);
    EXPECT_FALSE(str);
    str.clear();
    str.switch_to_put();
    EXPECT_FALSE(str);
}

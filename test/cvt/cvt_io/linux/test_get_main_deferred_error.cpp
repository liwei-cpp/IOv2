// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The deferred-error cases of ../test_get_main_deferred_error.cpp that decode through
 * code_cvt with char32_t inside, which needs wchar_t_is_utf32 (glibc only).
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/io/iochannel.h>

#include <support/fail_once_device.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using namespace IOv2;

// Half a character is held when the device fails: it is completed by the bytes after.
TEST(GetMainDeferredError, CodeCvt)
{
    const std::string ext = "ab\xE6\x9D\x8E" "cd";
    code_cvt<no_rb_root_cvt<fail_once_device>, char32_t> obj{no_rb_root_cvt{fail_once_device(ext, 3)}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, std::u32string(U"ab李cd"));
}

// What wcin.read() saw: getn's count, on the exception path, is what it wrote.
TEST(GetMainDeferredError, GetnCountsWhatItWroteBeforeTheError)
{
    ichannel ch(fail_once_device("abcdef", 3), code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));

    std::u32string buf(10, U'#');
    std::size_t got = 99;
    EXPECT_THROW(ch.getn(buf.data(), buf.size(), &got), device_error);
    EXPECT_EQ(got, 3u);
    EXPECT_EQ(buf.substr(0, 3), U"abc");

    EXPECT_EQ(ch.getn(buf.data(), buf.size(), &got), 3u);
    EXPECT_EQ(buf.substr(0, 3), U"def");
}

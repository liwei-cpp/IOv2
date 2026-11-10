// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The raw wchar_t code units zlib_cvt writes before main_cont_beg(), checked byte
 * for byte where wchar_t is two bytes wide, as on Windows. The four-byte
 * counterpart for Linux and macOS is in comp/posix/.
 */
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace IOv2;

namespace
{
    using WcharCvt = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;

    // Every case below is run twice: once on the statically typed zlib_cvt and
    // once on the same converter behind a runtime_cvt, which reaches it through a
    // virtual interface. The two have to agree byte for byte.
    WcharCvt static_cvt(unsigned level = 8)
    {
        return Comp::zlib_cvt_creator<wchar_t>{level}.create(rb_root_cvt{mem_device("")});
    }

    auto runtime_of(WcharCvt&& obj) { return runtime_cvt{std::move(obj)}; }
}

// Data written between bos() and main_cont_beg() reaches the device uncompressed,
// after the header -- and as raw wchar_t code units, two little-endian bytes
// each, which is what makes the width change visible at the byte level.
TEST(ZlibCvtWchar, PutBeforeMainContBegLandsAfterTheHeaderAsRawCodeUnits)
{
    WcharCvt    obj = static_cvt(6);
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);

    wchar_t buf[] = L"uvw";
    obj.put(buf, 3);

    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), std::string("\x78\x9c""u\x00v\x00w\x00", 8));
}

TEST(ZlibCvtWchar, PutBeforeMainContBegLandsAfterTheHeaderThroughARuntimeCvt)
{
    auto        obj = runtime_of(static_cvt(6));
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);

    wchar_t buf[] = L"uvw";
    obj.put(buf, 3);

    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), std::string("\x78\x9c""u\x00v\x00w\x00", 8));
}

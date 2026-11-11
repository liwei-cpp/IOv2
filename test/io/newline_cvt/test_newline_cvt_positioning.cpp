// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * newline_cvt under real streams, where appmode relies on rseek(0) coming before the
 * switch to writing. The converter's own positioning cases are in
 * cvt/newline_cvt/test_newline_cvt_positioning.cpp.
 */
#include <IOv2/cvt/newline_cvt.h>
#include <IOv2/device/file_device.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

using namespace IOv2;

// An iostream over unread content: appmode reaches the end by rseek(0) before
// switching to writing, so the switch finds eof.
TEST(NewlineCvtPositioning, AppmodeOnAnIostreamOverUnreadContent)
{
    iostream ios(mem_device{"a\r\nb"}, newline_cvt_creator{});
    ios << appmode << "x\n";
    ios.flush();
    EXPECT_TRUE(static_cast<bool>(ios));
    EXPECT_EQ(ios.device().str(), "a\r\nbx\r\n");
    EXPECT_EQ(ios.tell(), 5u);
}

// The same over a real file, written through an ostream and read back raw.
TEST(NewlineCvtPositioning, AppmodeOnAFile)
{
    file_guard g("newline_cvt_appmode", "one\r\n");
    {
        ostream os(file_device<char>{"newline_cvt_appmode"}, newline_cvt_creator{});
        os << appmode << "two\n" << "three\n";
        os.flush();
        EXPECT_TRUE(static_cast<bool>(os));
        auto [dev, err] = os.detach();
        dev.close();
    }
    EXPECT_EQ(g.contents(), "one\r\ntwo\r\nthree\r\n");
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The direction bos() settles on holds until main_cont_beg(): the BOS phase reads or
 * writes the stream header, never both. abs_cvt refuses a switch in that phase, whether
 * asked for explicitly or implied by a get/put against the direction, and the refusal
 * leaves the stream and the converter as they were. After main_cont_beg() switching
 * works as before.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/crypt/vigenere_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

using namespace IOv2;

namespace
{
    using Cvt = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
}

TEST(BosNoSwitch, APutIsRefusedWhenTheBosPhaseIsReading)
{
    Cvt obj(rb_root_cvt{mem_device("hello")}, "liwei");
    EXPECT_EQ(obj.bos(), io_status::input);

    EXPECT_THROW(obj.put("XY", 2), cvt_error);
    EXPECT_FALSE(obj.is_tainted());
    EXPECT_EQ(obj.device().str(), "hello");
}

TEST(BosNoSwitch, SwitchToPutIsRefusedInTheBosPhase)
{
    Cvt obj(rb_root_cvt{mem_device("hello")}, "liwei");
    EXPECT_EQ(obj.bos(), io_status::input);

    EXPECT_THROW(obj.switch_to_put(), cvt_error);
    EXPECT_FALSE(obj.is_tainted());
}

TEST(BosNoSwitch, AGetIsRefusedWhenTheBosPhaseIsWriting)
{
    Cvt obj(rb_root_cvt{mem_device("")}, "liwei");
    EXPECT_EQ(obj.bos(), io_status::output);

    char ch = 0;
    EXPECT_THROW(obj.get(&ch, 1), cvt_error);
    EXPECT_FALSE(obj.is_tainted());
}

TEST(BosNoSwitch, SwitchToGetIsRefusedInTheBosPhase)
{
    Cvt obj(rb_root_cvt{mem_device("")}, "liwei");
    EXPECT_EQ(obj.bos(), io_status::output);

    EXPECT_THROW(obj.switch_to_get(), cvt_error);
    EXPECT_FALSE(obj.is_tainted());
}

TEST(BosNoSwitch, ASwitchToTheCurrentDirectionIsANoOp)
{
    Cvt obj(rb_root_cvt{mem_device("hello")}, "liwei");
    EXPECT_EQ(obj.bos(), io_status::input);

    EXPECT_NO_THROW(obj.switch_to_get());
}

TEST(BosNoSwitch, SwitchingWorksAfterMainContBeg)
{
    Cvt obj(rb_root_cvt{mem_device("hello")}, "liwei");
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    obj.put("h", 1);
    char ch = 0;
    EXPECT_EQ(obj.get(&ch, 1), 1u);
    EXPECT_EQ(ch, static_cast<char>('e' - 'i'));
}

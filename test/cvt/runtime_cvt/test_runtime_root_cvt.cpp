// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    using ZCvt = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>>;

    // zlib_cvt reports output at bos() on an empty device and input on a
    // non-empty one, so a stream that has something in it is the only way to
    // drive a zlib-backed runtime_cvt into the input state.
    std::string compressed_stream()
    {
        ZCvt comp{rb_root_cvt{mem_device("")}, 6};
        comp.bos();
        comp.main_cont_beg();
        char data[] = "hi";
        comp.put(data, 2);
        auto [dev, err] = comp.detach();
        return dev.str();
    }
}

TEST(RuntimeCvt, TraitsOverACharMemDevice)
{
    using CheckType = runtime_cvt<mem_device<char>, char>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(RuntimeCvt, TraitsOverAChar32MemDevice)
{
    using CheckType = runtime_cvt<mem_device<char32_t>, char32_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char32_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char32_t>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

// A runtime_cvt holds its kernel behind a pointer, so moving from one leaves it
// null. The null state is not a usable empty converter: every public method has
// to report it as cvt_error rather than dereference the null pointer, and copies
// made from a null instance have to stay null.
TEST(RuntimeCvt, EveryMethodThrowsOnAMovedFromInstance)
{
    using RT = runtime_cvt<mem_device<char>, char>;

    RT src(rb_root_cvt{mem_device("")});
    RT dst = std::move(src); // src.m_ptr is now null

    char         buf[1] = {};
    const char*  cbuf   = "a";
    cvt_behavior beh;
    cvt_status   stat;

    EXPECT_THROW((void)src.device(),             cvt_error);
    EXPECT_THROW(src.attach(mem_device<char>{}), cvt_error);
    EXPECT_THROW(src.adjust(beh),                cvt_error);
    EXPECT_THROW(src.retrieve(stat),             cvt_error);
    EXPECT_THROW((void)src.is_eof(),             cvt_error);
    EXPECT_THROW((void)src.bos(),                cvt_error);
    EXPECT_THROW(src.main_cont_beg(),            cvt_error);
    EXPECT_THROW((void)src.get(buf, 1),          cvt_error);
    EXPECT_THROW(src.put(cbuf, 1),               cvt_error);
    EXPECT_THROW(src.flush(),                    cvt_error);
    EXPECT_THROW((void)src.tell(),               cvt_error);
    EXPECT_THROW(src.seek(0),                    cvt_error);
    EXPECT_THROW(src.rseek(0),                   cvt_error);
    EXPECT_THROW(src.switch_to_get(),            cvt_error);
    EXPECT_THROW(src.switch_to_put(),            cvt_error);
}

TEST(RuntimeCvt, CopyConstructionPropagatesTheNullState)
{
    using RT = runtime_cvt<mem_device<char>, char>;

    RT src(rb_root_cvt{mem_device("")});
    RT dst        = std::move(src);
    RT propagated = src;

    EXPECT_THROW((void)propagated.bos(),   cvt_error);
    EXPECT_THROW((void)propagated.tell(),  cvt_error);
    EXPECT_THROW((void)propagated.flush(), cvt_error);
}

TEST(RuntimeCvt, CopyAssignmentPropagatesTheNullState)
{
    using RT = runtime_cvt<mem_device<char>, char>;

    RT src(rb_root_cvt{mem_device("")});
    RT dst = std::move(src);

    RT target(rb_root_cvt{mem_device("x")}); // valid until the assignment below
    target = src;

    EXPECT_THROW((void)target.bos(),   cvt_error);
    EXPECT_THROW((void)target.tell(),  cvt_error);
    EXPECT_THROW((void)target.flush(), cvt_error);
}

TEST(RuntimeCvt, PositioningAndSwitchingOnASupportingKernel)
{
    runtime_cvt obj(rb_root_cvt{mem_device(std::string("hello world"))});

    EXPECT_EQ(obj.bos(), io_status::input);

    cvt_status stat;
    EXPECT_NO_THROW(obj.retrieve(stat));

    obj.main_cont_beg();
    EXPECT_FALSE(obj.is_eof());

    char buf[5] = {};
    EXPECT_EQ(obj.get(buf, 5), 5);
    EXPECT_GT(obj.tell(), 0);

    obj.seek(0);
    EXPECT_EQ(obj.tell(), 0);

    // rseek(3) counts from the end of the stream, so any position it lands on is
    // past the start; the point here is only that the call reaches the kernel.
    obj.rseek(3);
    EXPECT_GT(obj.tell(), 0);

    // Each switch is exercised twice: the first call really switches, the second
    // takes the early return that a switch to the current direction gets.
    EXPECT_NO_THROW(obj.switch_to_put());
    EXPECT_NO_THROW(obj.switch_to_put());
    EXPECT_NO_THROW(obj.switch_to_get());
    EXPECT_NO_THROW(obj.switch_to_get());

    obj.detach();
}

TEST(RuntimeCvt, PositioningThrowsOnAKernelWithoutIt)
{
    runtime_cvt obj(ZCvt{rb_root_cvt{mem_device("")}, 6});
    EXPECT_THROW((void)obj.tell(), cvt_error);
    EXPECT_THROW(obj.seek(0),      cvt_error);
    EXPECT_THROW(obj.rseek(0),     cvt_error);
}

TEST(RuntimeCvt, SwitchToGetThrowsWhenTheKernelCannotSwitch)
{
    runtime_cvt obj(ZCvt{rb_root_cvt{mem_device("")}, 6});
    EXPECT_EQ(obj.bos(), io_status::output);
    EXPECT_THROW(obj.switch_to_get(), cvt_error);
}

TEST(RuntimeCvt, SwitchToPutThrowsWhenTheKernelCannotSwitch)
{
    runtime_cvt obj(ZCvt{rb_root_cvt{mem_device(compressed_stream())}, 6});
    EXPECT_EQ(obj.bos(), io_status::input);
    EXPECT_THROW(obj.switch_to_put(), cvt_error);
}

TEST(RuntimeCvt, SwitchToGetIsANoopWhenAlreadyReading)
{
    runtime_cvt obj(ZCvt{rb_root_cvt{mem_device(compressed_stream())}, 6});
    EXPECT_EQ(obj.bos(), io_status::input);
    EXPECT_NO_THROW(obj.switch_to_get());
}

TEST(RuntimeCvt, SwitchToPutIsANoopWhenAlreadyWriting)
{
    runtime_cvt obj(ZCvt{rb_root_cvt{mem_device("")}, 6});
    EXPECT_EQ(obj.bos(), io_status::output);
    EXPECT_NO_THROW(obj.switch_to_put());
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/std_device.h>

#include <support/platform.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    namespace sys = iov2_test::sys;

    // Writing "hello", handing the converter over, then writing " world" through
    // the target: the two halves may only reach the descriptor in that order, and
    // exactly once each.
    template <typename T, typename Transfer, typename Finish>
    void expect_a_move_keeps_the_output_stream(T& obj, Transfer transfer, Finish finish)
    {
        oguard<true> g;
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);

        T moved = transfer(obj);
        moved.put(" world", 6);
        finish(moved);

        EXPECT_EQ(g.contents(), "hello world");
    }

    // The read side of the same: five characters through the source, six more
    // through the target, with no character lost or repeated at the handover.
    template <typename T, typename Transfer>
    void expect_a_move_keeps_the_input_stream(T& obj, Transfer transfer)
    {
        iguard      g("hello world");
        std::string str(5, '\0');

        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();
        EXPECT_EQ(obj.get(str.data(), 5), 5u);
        EXPECT_EQ(str, "hello");

        T moved = transfer(obj);
        str.resize(6);
        EXPECT_EQ(moved.get(str.data(), 6), 6u);
        EXPECT_EQ(str, " world");
    }
}

TEST(RootCvtStd, TraitsOverStdin)
{
    using CheckType = no_rb_root_cvt<std_device<sys::stdin_fd>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, std_device<sys::stdin_fd>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    // A standard stream is one-directional and not seekable, so the direction the
    // descriptor was opened for is the only capability the converter can offer.
    static_assert(!cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtStd, TraitsOverStdout)
{
    using CheckType = rb_root_cvt<std_device<sys::stdout_fd>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, std_device<sys::stdout_fd>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(!cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtStd, MoveConstructionKeepsTheOutputStream)
{
    auto obj = rb_root_cvt{std_device<sys::stdout_fd>{}};
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src) { return rb_root_cvt<std_device<sys::stdout_fd>>{std::move(src)}; },
        [](auto& dst) { dst.detach(); });
}

TEST(RootCvtStd, MoveConstructionKeepsTheOutputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<sys::stdout_fd>{}});
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src) { return runtime_cvt{std::move(src)}; },
        [](auto& dst) { dst.detach(); });
}

// Same handover through assignment, and finished with attach() rather than
// detach(): attaching a fresh device has to flush what the old one still held.
TEST(RootCvtStd, MoveAssignmentKeepsTheOutputStream)
{
    auto obj = rb_root_cvt{std_device<sys::stdout_fd>{}};
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src)
        {
            rb_root_cvt<std_device<sys::stdout_fd>> dst{std_device<sys::stdout_fd>{}};
            dst = std::move(src);
            return dst;
        },
        [](auto& dst) { dst.attach(std_device<sys::stdout_fd>{}); });
}

TEST(RootCvtStd, MoveAssignmentKeepsTheOutputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<sys::stdout_fd>{}});
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src)
        {
            runtime_cvt dst{rb_root_cvt{std_device<sys::stdout_fd>{}}};
            dst = std::move(src);
            return dst;
        },
        [](auto& dst) { dst.attach(std_device<sys::stdout_fd>{}); });
}

TEST(RootCvtStd, MoveConstructionKeepsTheInputStream)
{
    auto obj = rb_root_cvt{std_device<sys::stdin_fd>{}};
    expect_a_move_keeps_the_input_stream(
        obj, [](auto& src) { return rb_root_cvt<std_device<sys::stdin_fd>>{std::move(src)}; });
}

TEST(RootCvtStd, MoveConstructionKeepsTheInputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<sys::stdin_fd>{}});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(RootCvtStd, MoveAssignmentKeepsTheInputStream)
{
    auto obj = rb_root_cvt{std_device<sys::stdin_fd>{}};
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        rb_root_cvt<std_device<sys::stdin_fd>> dst{std_device<sys::stdin_fd>{}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtStd, MoveAssignmentKeepsTheInputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<sys::stdin_fd>{}});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{std_device<sys::stdin_fd>{}}};
        dst = std::move(src);
        return dst;
    });
}

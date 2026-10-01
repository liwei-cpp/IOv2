#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/file_device.h>
#include <IOv2/device/mem_device.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <utility>

using namespace IOv2;

namespace
{
    // Each root reads a different way: straight out of the mem_device, out of
    // rb_root_cvt's own buffer, or through abs_cvt's read area (no_rb_root_cvt over
    // a file). The bytes a failed get() had already taken must survive in all three.
    template <typename T>
    void expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::u32string buf(16, U'#');
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
        EXPECT_EQ(buf.substr(0, 2), U"ab");

        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);

        EXPECT_EQ(obj.get(buf.data(), buf.size()), 5u);
        EXPECT_EQ(buf.substr(0, 5), U"cd ef");
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 0u);
    }

    // The lead byte is held across the two fetches of one get(); the byte that
    // proves it wrong is the start of the next character, not part of the error.
    template <typename T>
    void expect_a_held_lead_byte_is_all_an_error_drops(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::u32string buf(16, U'#');
        EXPECT_EQ(obj.get(buf.data(), 2), 1u);
        EXPECT_EQ(buf[0], U'x');

        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);

        EXPECT_EQ(obj.get(buf.data(), buf.size()), 3u);
        EXPECT_EQ(buf.substr(0, 3), U"a b");
    }

    const std::string kBadByte("ab\xff" "cd ef");
    const std::string kHeldLead("x\xe4" "a b");

    using RODev = basic_file_device<true, false, char>;
}

TEST(CodeCvtReadAfterError, BlockReadOverAMemDevice)
{
    code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t> obj{no_rb_root_cvt{mem_device(kBadByte)}, "zh_CN.UTF-8"};
    expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(obj);
}

TEST(CodeCvtReadAfterError, BlockReadOverARootBuffer)
{
    file_guard g("code_cvt_read_after_error", kBadByte);
    code_cvt<rb_root_cvt<RODev>, char32_t> obj{rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(obj);
}

TEST(CodeCvtReadAfterError, BlockReadOverTheReadArea)
{
    file_guard g("code_cvt_read_after_error", kBadByte);
    code_cvt<no_rb_root_cvt<RODev>, char32_t> obj{no_rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(obj);
}

TEST(CodeCvtReadAfterError, TheReadAreaSurvivesAMove)
{
    file_guard g("code_cvt_read_after_error", kBadByte);
    code_cvt<no_rb_root_cvt<RODev>, char32_t> obj{no_rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    std::u32string buf(16, U'#');
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);

    auto moved = std::move(obj);
    EXPECT_THROW(moved.get(buf.data(), buf.size()), cvt_error);
    EXPECT_EQ(moved.get(buf.data(), buf.size()), 5u);
    EXPECT_EQ(buf.substr(0, 5), U"cd ef");
}

TEST(CodeCvtReadAfterError, HeldLeadByteOverAMemDevice)
{
    code_cvt<rb_root_cvt<mem_device<char>>, char32_t> obj{rb_root_cvt{mem_device(kHeldLead)}, "zh_CN.UTF-8"};
    expect_a_held_lead_byte_is_all_an_error_drops(obj);
}

TEST(CodeCvtReadAfterError, HeldLeadByteOverTheReadArea)
{
    file_guard g("code_cvt_read_after_error", kHeldLead);
    code_cvt<no_rb_root_cvt<RODev>, char32_t> obj{no_rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    expect_a_held_lead_byte_is_all_an_error_drops(obj);
}

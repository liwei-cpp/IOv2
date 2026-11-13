// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The partial write of cvt/root_cvt/test_root_cvt_partial_write.cpp, seen from a
 * code_cvt above the root: what the device accepted is neither lost nor written
 * twice.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/root_cvt.h>

#include <support/partial_write_device.h>
#include <support/platform_macros.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

// A layer above writes through the root's own buffer (cvt_writer's root specialization);
// its put_buf makes room the same way once the buffer has a single slot left. The detach
// after the failure sends the rest.
TEST(RootCvtPartialWrite, ALayerAboveLosesNothingTheRootAccepted)
{
#if IOV2_TEST_LINUX_GLIBC
    g_out.clear();
    g_accept.reset();
    g_plain = false;
    code_cvt<Root, char32_t> obj{Root{partial_write_device{}}, "C"};
    obj.bos();
    obj.main_cont_beg();

    const std::u32string earlier(kLen - 1, U'a');
    obj.put(earlier.data(), earlier.size());
    EXPECT_EQ(g_out, "") << "the buffer was to hold all of it";

    g_accept = 7;
    const char32_t one_more = U'b';
    EXPECT_THROW(obj.put(&one_more, 1), dput_error);
    EXPECT_EQ(g_out, std::string(7, 'a'));

    auto [dev, err] = obj.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(g_out, std::string(kLen - 1, 'a')) << "the failed put's character was not accepted";
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * code_cvt_stdio cases that watch __fpending() (glibc's <stdio_ext.h>). The cases
 * that run everywhere are in ../test_code_cvt_stdio_char.cpp.
 */
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/std_device.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <utility>

#include <stdio_ext.h>
#include <unistd.h>

using namespace IOv2;

namespace
{
    // The converter behind wcin / wcout: code_cvt_stdio accepts standard devices only.
    using StdioIn  = code_cvt_stdio<rb_root_cvt<std_device<STDIN_FILENO>>>;
    using StdioOut = code_cvt_stdio<rb_root_cvt<std_device<STDOUT_FILENO>>>;
}

// A move hands the whole pipeline over -- the converter, its root buffer, and the
// device's duty to flush the FILE on destruction -- so the source must die without
// flushing. This is the layer wcout sits on; code_cvt's own move assignment adds
// a close_stream() that the root-level tests do not reach. Full buffering keeps
// the bytes in stdio, so __fpending() says whether the dying source flushed them.
TEST(CodeCvtStdio, AMovedFromConverterFlushesNothingWhenItDies)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    auto moved = [&]
    {
        StdioOut src{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"};
        EXPECT_EQ(src.bos(), io_status::output);
        src.main_cont_beg();
        const wchar_t wen[] = {L'文'};
        src.put(wen, 1);
        src.flush();                                 // 3 bytes into stdio's buffer
        EXPECT_EQ(__fpending(stdout), 3u);
        return StdioOut{std::move(src)};
    }();                                             // src dies here
    EXPECT_EQ(__fpending(stdout), 3u);               // it neither flushed nor wrote

    const wchar_t bang[] = {L'!'};
    moved.put(bang, 1);
    moved.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "文!");
}

TEST(CodeCvtStdio, AMovedFromConverterFlushesNothingWhenItDiesAfterMoveAssignment)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    StdioOut dst{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(dst.bos(), io_status::output);
    dst.main_cont_beg();
    const wchar_t zhong[] = {L'中'};
    dst.put(zhong, 1);                               // 3 bytes in dst's root buffer
    {
        StdioOut src{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"};
        EXPECT_EQ(src.bos(), io_status::output);
        src.main_cont_beg();
        const wchar_t wen[] = {L'文'};
        src.put(wen, 1);
        src.flush();
        EXPECT_EQ(__fpending(stdout), 3u);

        dst = std::move(src);
        EXPECT_EQ(__fpending(stdout), 6u);           // dst's own bytes handed to stdio
    }                                                // src dies here
    EXPECT_EQ(__fpending(stdout), 6u);

    const wchar_t bang[] = {L'!'};
    dst.put(bang, 1);
    dst.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "文中!");
}

TEST(CodeCvtStdio, ASelfMoveChangesNothing)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    StdioOut obj{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    const wchar_t wen[] = {L'文'};
    obj.put(wen, 1);
    obj.flush();

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    obj = std::move(obj);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_EQ(__fpending(stdout), 3u);

    const wchar_t bang[] = {L'!'};
    obj.put(bang, 1);
    obj.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "文!");
}

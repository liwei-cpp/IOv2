// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * root_cvt over a standard stream, where what is observed needs glibc:
 * __fpending() from <stdio_ext.h>, and /dev/full for a write that fails.
 */
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/std_device.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <utility>

#include <fcntl.h>
#include <stdio_ext.h>
#include <unistd.h>

using namespace IOv2;

// The other half of a move: the source must go quiet. Over a fixed fd two duties
// travel with the converter -- its own buffer, and the device's duty to flush
// the FILE on destruction -- and after a move both belong to the target alone.
// A source that still flushed would push bytes out early, and would take the one
// fflush that can fail into a destructor that must swallow the error, leaving
// the real holder with nothing to report.
//
// Full buffering is what makes the second duty observable: the bytes handed to
// stdio stay pending until somebody flushes, so __fpending() says whether the
// dying source did.
TEST(RootCvtStd, AMovedFromOutputRootFlushesNothingWhenItDies)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    auto moved = [&]
    {
        auto src = rb_root_cvt{std_device<STDOUT_FILENO>{}};
        EXPECT_EQ(src.bos(), io_status::output);
        src.main_cont_beg();
        src.put("hello", 5);
        src.flush();                                 // into stdio's buffer, not to the fd
        EXPECT_EQ(__fpending(stdout), 5u);
        return rb_root_cvt<std_device<STDOUT_FILENO>>{std::move(src)};
    }();                                             // src dies here

    EXPECT_EQ(__fpending(stdout), 5u);               // it neither flushed nor wrote

    moved.put(" world", 6);
    moved.detach();                                  // the target still owns both duties
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "hello world");
}

// The same through assignment. The target first hands its own buffer to stdio
// (a dput, not an fflush), then takes over both duties from the source, which
// must die as quietly as a move-constructed one.
TEST(RootCvtStd, AMovedFromOutputRootFlushesNothingWhenItDiesAfterMoveAssignment)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    auto dst = rb_root_cvt{std_device<STDOUT_FILENO>{}};
    EXPECT_EQ(dst.bos(), io_status::output);
    dst.main_cont_beg();
    dst.put("D", 1);                                 // still in dst's own buffer
    {
        auto src = rb_root_cvt{std_device<STDOUT_FILENO>{}};
        EXPECT_EQ(src.bos(), io_status::output);
        src.main_cont_beg();
        src.put("S", 1);
        src.flush();                                 // into stdio's buffer, not to the fd
        EXPECT_EQ(__fpending(stdout), 1u);

        dst = std::move(src);
        EXPECT_EQ(__fpending(stdout), 2u);           // dst's "D" handed to stdio, not flushed
    }                                                // src dies here
    EXPECT_EQ(__fpending(stdout), 2u);               // it neither flushed nor wrote

    dst.put("T", 1);
    dst.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "SDT");
}

// Why the source must stay quiet, at this layer: glibc discards the buffer when
// fflush fails, so only the first flush sees the error. After a move the owner
// has to be the one that reports it, exactly once.
TEST(RootCvtStd, AMovedFromOutputRootLeavesTheFlushFailureToItsOwner)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    const int saved = ::dup(STDOUT_FILENO);
    const int full  = ::open("/dev/full", O_WRONLY);
    ASSERT_NE(full, -1);
    ::dup2(full, STDOUT_FILENO);
    ::close(full);

    auto dst = rb_root_cvt{std_device<STDOUT_FILENO>{}};
    EXPECT_EQ(dst.bos(), io_status::output);
    dst.main_cont_beg();
    {
        auto src = rb_root_cvt{std_device<STDOUT_FILENO>{}};
        EXPECT_EQ(src.bos(), io_status::output);
        src.main_cont_beg();
        src.put("SSSS", 4);
        src.flush();                                 // into stdio's buffer
        dst = std::move(src);
    }                                                // src dies: must not take the fflush
    EXPECT_EQ(__fpending(stdout), 4u);

    EXPECT_THROW(dst.attach(), device_error);        // the owner sees the failure...
    EXPECT_EQ(dst.bos(), io_status::output);
    dst.main_cont_beg();
    EXPECT_NO_THROW(dst.attach());                   // ...and only once

    ::dup2(saved, STDOUT_FILENO);
    ::close(saved);
    std::clearerr(stdout);
}

// A self-move changes nothing: the bytes in the converter's buffer stay there,
// and the duty to flush stays with the object.
TEST(RootCvtStd, ASelfMoveKeepsTheOutputBufferAndTheFlushDuty)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    auto obj = rb_root_cvt{std_device<STDOUT_FILENO>{}};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("AB", 2);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    obj = std::move(obj);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_EQ(__fpending(stdout), 0u);               // "AB" still in the converter's buffer

    obj.put("C", 1);
    obj.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "ABC");
}

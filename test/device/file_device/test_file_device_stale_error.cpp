// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A read-write file_device whose write, or flush, failed used to report the next
 * read that merely reached the end of the file as a read error -- the FILE's error
 * indicator was still set from the write -- and to lose what that read had taken.
 * dget() now clears an indicator left set before reading, so only its own read's
 * error counts; on the normal path it clears nothing and EOF stays sticky.
 *
 * The failing write needs a file size limit, which applies to the whole process, so
 * those cases run in a child (support/test_child.h).
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/file_device.h>

#include <support/file_guard.h>
#include <support/test_child.h>

#include <gtest/gtest.h>

#include <csignal>
#include <fstream>
#include <string>

#include <sys/resource.h>

using namespace IOv2;

namespace
{
    using RWDev = basic_file_device<true, true, char>;
    const char* const kFile = "file_device_stale_error";

    // In the child: a write that the 5-byte limit makes fail (in dput, or in dflush once
    // stdio has buffered it), then a read from the start that reaches the end.
    void read_back_after_a_failed(const std::string& which)
    {
        std::signal(SIGXFSZ, SIG_IGN);
        rlimit old{};
        ::getrlimit(RLIMIT_FSIZE, &old);
        rlimit small = old;
        small.rlim_cur = 5;

        RWDev dev(kFile, file_open_flag::trunc | file_open_flag::binary);
        ::setrlimit(RLIMIT_FSIZE, &small);
        if (which == "dput")
        {
            const std::string big(10000, 'x');           // past stdio's buffer: fwrite writes now
            EXPECT_THROW(dev.dput(big.data(), big.size()), device_error);
        }
        else
        {
            dev.dput("xxxxxxxxxx", 10);                  // buffered by stdio: dput succeeds
            EXPECT_THROW(dev.dflush(), device_error);
        }
        ::setrlimit(RLIMIT_FSIZE, &old);

        dev.dseek(0);
        char buf[100]{};
        std::size_t got = 0;
        EXPECT_NO_THROW(got = dev.dget(buf, sizeof buf)) << "reaching the end was taken for a read error";
        EXPECT_EQ(std::string(buf, got), "xxxxx");
    }
}

TEST(FileDeviceStaleError, AReadAfterAFailedWriteReachesTheEndCleanly)
{
    if (in_test_child())
        return read_back_after_a_failed(test_child_arg());

    for (const char* which : {"dput", "dflush"})
    {
        SCOPED_TRACE(which);
        file_guard g(kFile, "");
        EXPECT_EQ(run_test_child("FileDeviceStaleError.AReadAfterAFailedWriteReachesTheEndCleanly", which), 0)
            << "the checks in the child failed; see its output above";
    }
}

// On the normal path nothing is cleared: once a read reaches the end, what is appended
// to the file later stays out of sight, as stdio's sticky EOF has it.
TEST(FileDeviceStaleError, EofStaysStickyOnTheNormalPath)
{
    file_guard g(kFile, "abc");
    RWDev dev(kFile, file_open_flag::binary);

    char buf[16]{};
    EXPECT_EQ(dev.dget(buf, sizeof buf), 3u);
    EXPECT_EQ(dev.dget(buf, sizeof buf), 0u);

    std::ofstream(kFile, std::ios::binary | std::ios::app) << "def";
    EXPECT_EQ(dev.dget(buf, sizeof buf), 0u);
}

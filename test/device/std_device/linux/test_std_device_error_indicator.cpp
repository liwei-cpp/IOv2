// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A failed write or flush leaves the error indicator of stdout set. stdout is
 * shared with the user's own printf, and code that checks ferror(stdout) before
 * exiting to tell whether all output went out has to see the failure. The device
 * used to clearerr() it -- wiping out even a failure the user's printf had set.
 *
 * Each case runs in a child process (support/test_child.h) with stdout pointed at
 * /dev/full; the child reports through its exit status, since its own output goes
 * to /dev/full too.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/std_device.h>

#include <support/test_child.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include <fcntl.h>
#include <unistd.h>

using namespace IOv2;

namespace
{
    // In the child: stdout to /dev/full, then the case named by the parent. Returns the
    // exit status: 0 when the indicator is set at the end, nonzero otherwise.
    int run_case(const std::string& which)
    {
        const int fd = ::open("/dev/full", O_WRONLY);
        if (fd < 0 || ::dup2(fd, STDOUT_FILENO) < 0)
            return 10;
        ::close(fd);
        std_device<STDOUT_FILENO> dev;

        if (which == "flush")
        {
            dev.dput("abc", 3);                          // fits in stdio's buffer
            try { dev.dflush(); return 11; } catch (const device_error&) {}
        }
        else if (which == "write")
        {
            const std::string big(1 << 20, 'x');         // larger than stdio's buffer
            try { dev.dput(big.data(), big.size()); return 12; } catch (const device_error&) {}
        }
        else if (which == "user_first")
        {
            std::fputs("user", stdout);
            if (std::fflush(stdout) != EOF || !std::ferror(stdout))
                return 13;                               // the user's own failure is set
            dev.dput("abc", 3);
            try { dev.dflush(); return 14; } catch (const device_error&) {}
        }
        return std::ferror(stdout) ? 0 : 1;
    }

    void expect_the_indicator_kept(const char* which)
    {
        EXPECT_EQ(run_test_child("StdDeviceErrorIndicator.KeptAfterAFailure", which), 0)
            << which << ": 1 means ferror(stdout) was cleared; 10-14, the setup went wrong";
    }
}

TEST(StdDeviceErrorIndicator, KeptAfterAFailure)
{
    if (in_test_child())
        ::_exit(run_case(test_child_arg()));

    expect_the_indicator_kept("flush");
    expect_the_indicator_kept("write");
    expect_the_indicator_kept("user_first");
}

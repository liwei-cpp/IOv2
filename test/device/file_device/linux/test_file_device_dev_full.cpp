// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Failures that need the operating system to produce them. /dev/full accepts
 * an open and then fails every write with ENOSPC, which is how the
 * partial-write and flush-failure paths are reached. It exists on Linux only:
 * macOS has no such device, and Windows has no /dev at all (Wine maps the
 * path onto the host's, which is why it once passed there).
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/file_device.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(FileDeviceDevFull, FlushReportsAWriteThatTheSystemRefused)
{
    ofile_device<char> dev("/dev/full");
    dev.dput("some data", 9);
    EXPECT_THROW(dev.dflush(), device_error);
}

TEST(FileDeviceDevFull, CloseReportsTheFailedFlushAndStillReleasesTheFile)
{
    ofile_device<char> dev("/dev/full");
    dev.dput("some data", 9);

    EXPECT_THROW(dev.close(), device_error);
    // close() resets the handle before re-throwing, so the device is closed
    // even though it reported an error.
    EXPECT_FALSE(dev.is_open());
}

TEST(FileDeviceDevFull, PutReportsAPartialWrite)
{
    ofile_device<char> dev("/dev/full");

    // Large enough that it cannot sit in the FILE buffer: the write reaches the
    // system and comes back short.
    const std::string large(1024 * 1024, 'A');
    EXPECT_THROW(dev.dput(large.data(), large.size()), device_error);
}

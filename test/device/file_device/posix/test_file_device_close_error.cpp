// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * close() checks fclose()'s result: a close can fail on its own with nothing
 * left to flush (EIO from close(2) on NFS or a quota, say). The test closes the
 * descriptor behind stdio's back, so fclose()'s own close(2) fails with EBADF.
 *
 * Finding the descriptor relies on POSIX handing out the lowest free one, so a
 * probe open() that is closed again leaves its number for the device's fopen();
 * fstat() confirms the match before anything is closed. Windows is not covered:
 * fclose() on a CRT descriptor that is already closed calls the CRT's invalid
 * parameter handler, which terminates the process rather than returning an error.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/file_device.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace IOv2;

TEST(FileDeviceCloseError, CloseReportsAFailedFcloseAndStillReleasesTheFile)
{
    file_guard g("close_ebadf.txt", std::string("hi"));

    const int fd = ::open("close_ebadf.txt", O_RDONLY);
    ASSERT_NE(fd, -1);
    ASSERT_EQ(::close(fd), 0);

    ifile_device<char> dev("close_ebadf.txt");

    struct stat expected{};
    struct stat actual{};
    ASSERT_EQ(::stat("close_ebadf.txt", &expected), 0);
    if (::fstat(fd, &actual) != 0 || actual.st_dev != expected.st_dev ||
        actual.st_ino != expected.st_ino)
        GTEST_SKIP() << "the device did not get the lowest free descriptor";
    ASSERT_EQ(::close(fd), 0);

    EXPECT_THROW(dev.close(), device_error);
    // close() releases the handle before throwing, so the device is closed
    // even though it reported an error.
    EXPECT_FALSE(dev.is_open());
}

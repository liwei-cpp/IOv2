// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * An insertion that runs after a stream's exit hook -- here from an atexit function
 * registered before IOv2 was initialized, so exit() calls it after the hook -- still
 * reaches the device. The hook switches the stream back to synchronized, so the late
 * bytes go to stdio, which glibc flushes last. An unsynchronized stream used to keep
 * them in its own buffer, which nothing flushed any more: the line was lost.
 *
 * Each case runs in a child process (support/test_child.h): what is checked is what
 * the child's exit() writes, and fd 2 is pointed at a file for the run.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/test_child.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace
{
    const char* const kFile = "io_objects_after_exit_hook";
    bool g_armed = false;

    void write_late() { if (g_armed) IOv2::clog << "late\n"; }

    // Priority 101 runs before the ordinary static initializers, IOv2's among them, so
    // this atexit function runs after their exit hooks.
    __attribute__((constructor(101))) void register_early() { std::atexit(write_late); }
}

TEST(IoObjectsAfterExitHook, ALateInsertionStillReachesTheDevice)
{
    if (in_test_child())
    {
        const int fd = ::open(kFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        ASSERT_GE(fd, 0);
        ASSERT_GE(::dup2(fd, STDERR_FILENO), 0);
        ::close(fd);
        IOv2::clog.sync_with_stdio(test_child_arg() == "sync");
        IOv2::clog << "main\n";
        g_armed = true;
        return;
    }

    for (const char* mode : {"sync", "nosync"})
    {
        SCOPED_TRACE(mode);
        EXPECT_EQ(run_test_child("IoObjectsAfterExitHook.ALateInsertionStillReachesTheDevice", mode), 0);
        std::ifstream in(kFile, std::ios::binary);
        const std::string got{std::istreambuf_iterator<char>(in), {}};
        in.close();
        ::unlink(kFile);
        EXPECT_EQ(got, "main\nlate\n");
    }
}

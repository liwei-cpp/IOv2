// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * An insertion that runs after a stream's exit hook -- here from an atexit function
 * registered before IOv2 was initialized, so exit() calls it after the hook -- still
 * reaches the device. The hook has every later insertion handled as synchronized, so the
 * late bytes go to stdio, which glibc flushes last. An unsynchronized stream used to keep
 * them in its own buffer, which nothing flushed any more: the line was lost.
 *
 * Bytes the hook could not hand over (here: the stream was failed) still go out on a later
 * clear() and sync_with_stdio(true), in either mode. Synchronized, a failed write is what
 * leaves them behind, and sync_with_stdio(true) used to return early on its already true
 * flag without handing them over: they were lost.
 *
 * Each case runs in a child process (support/test_child.h): what is checked is what
 * the child's exit() writes, and fd 2 is pointed at a file for the run.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/test_child.h>

#include <gtest/gtest.h>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

namespace
{
    const char* const kFile = "io_objects_after_exit_hook";
    // What the late atexit function does; 0 = nothing.
    int g_late = 0;

    // An insertion that writes 'A', waits until the exit hook has run, then writes 'B'.
    struct straddler {};
    std::atomic<int> g_straddle{0};
    std::thread* g_straddling = nullptr;
}

namespace IOv2
{
    template <>
    struct io_traits<char, straddler>
    {
        template <typename TIter>
        static TIter swrite(TIter iter, ios_base<char>&, const locale<char>&, straddler)
        {
            *iter++ = 'A';
            g_straddle = 1;
            while (g_straddle != 2)
                std::this_thread::yield();
            *iter++ = 'B';
            return iter;
        }
    };
}

namespace
{

    void write_late()
    {
        switch (g_late)
        {
        case 1:
            IOv2::clog << "late\n";
            break;
        case 2:
            IOv2::clog.sync_with_stdio(false);
            IOv2::clog << "late\n";
            break;
        case 3:
            IOv2::clog.clear();
            IOv2::clog.sync_with_stdio(true);
            break;
        case 4:
        {
            rlimit r{};
            ::getrlimit(RLIMIT_FSIZE, &r);
            r.rlim_cur = r.rlim_max;
            ::setrlimit(RLIMIT_FSIZE, &r);
            IOv2::clog.clear();
            IOv2::clog.sync_with_stdio(true);
            break;
        }
        case 5:
            g_straddle = 2;
            g_straddling->join();
            break;
        default:
            break;
        }
    }

    std::string run_case(const char* test, const char* mode)
    {
        EXPECT_EQ(run_test_child(test, mode), 0);
        std::ifstream in(kFile, std::ios::binary);
        std::string got{std::istreambuf_iterator<char>(in), {}};
        in.close();
        ::unlink(kFile);
        return got;
    }

    void redirect_stderr_to_file()
    {
        const int fd = ::open(kFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        ASSERT_GE(fd, 0);
        ASSERT_GE(::dup2(fd, STDERR_FILENO), 0);
        ::close(fd);
    }

    // Priority 101 runs before the ordinary static initializers, IOv2's among them, so
    // this atexit function runs after their exit hooks.
    __attribute__((constructor(101))) void register_early() { std::atexit(write_late); }
}

TEST(IoObjectsAfterExitHook, ALateInsertionStillReachesTheDevice)
{
    if (in_test_child())
    {
        redirect_stderr_to_file();
        IOv2::clog.sync_with_stdio(test_child_arg() == "sync");
        IOv2::clog << "main\n";
        g_late = 1;
        return;
    }

    for (const char* mode : {"sync", "nosync"})
    {
        SCOPED_TRACE(mode);
        EXPECT_EQ(run_case("IoObjectsAfterExitHook.ALateInsertionStillReachesTheDevice", mode),
                  "main\nlate\n");
    }
}

TEST(IoObjectsAfterExitHook, SwitchingOffAfterTheHookDoesNotLoseLaterInsertions)
{
    if (in_test_child())
    {
        redirect_stderr_to_file();
        IOv2::clog.sync_with_stdio(test_child_arg() == "sync");
        IOv2::clog << "main\n";
        g_late = 2;
        return;
    }

    for (const char* mode : {"sync", "nosync"})
    {
        SCOPED_TRACE(mode);
        EXPECT_EQ(run_case("IoObjectsAfterExitHook.SwitchingOffAfterTheHookDoesNotLoseLaterInsertions", mode),
                  "main\nlate\n");
    }
}

TEST(IoObjectsAfterExitHook, BytesTheHookLeftBehindGoOutOnALaterSwitchBack)
{
    if (in_test_child())
    {
        redirect_stderr_to_file();
        IOv2::clog.sync_with_stdio(false);
        IOv2::clog << "pre\n";
        // The hook's stream-level flush() does nothing on a failed stream.
        IOv2::clog.setstate(IOv2::ios_defs::strfailbit);
        g_late = 3;
        return;
    }

    EXPECT_EQ(run_case("IoObjectsAfterExitHook.BytesTheHookLeftBehindGoOutOnALaterSwitchBack", "1"),
              "pre\n");
}

// Unsynchronized, an insertion holds the lock while the hook runs, so the hook gives up; the
// insertion ends after it and hands everything buffered to stdio.
TEST(IoObjectsAfterExitHook, AnInsertionInProgressWhileTheHookRunsStillReachesTheDevice)
{
    if (in_test_child())
    {
        redirect_stderr_to_file();
        IOv2::clog.sync_with_stdio(false);
        IOv2::clog << "head|";
        g_straddling = new std::thread([] { IOv2::clog << straddler{}; });
        while (g_straddle != 1)
            std::this_thread::yield();
        g_late = 5;
        return;
    }

    EXPECT_EQ(run_case("IoObjectsAfterExitHook.AnInsertionInProgressWhileTheHookRunsStillReachesTheDevice", "1"),
              "head|AB");
}

// Synchronized, a write that stops at a 5-byte file size limit keeps "56789" in the stream's
// buffer and fails the stream; the late atexit function lifts the limit and recovers.
TEST(IoObjectsAfterExitHook, BytesAFailedSynchronizedWriteLeftGoOutOnALaterSyncWithStdio)
{
    if (in_test_child())
    {
        redirect_stderr_to_file();
        std::signal(SIGXFSZ, SIG_IGN);
        rlimit r{};
        ::getrlimit(RLIMIT_FSIZE, &r);
        r.rlim_cur = 5;
        ::setrlimit(RLIMIT_FSIZE, &r);
        IOv2::clog << "0123456789";
        EXPECT_FALSE(IOv2::clog.good());
        g_late = 4;
        return;
    }

    EXPECT_EQ(run_case("IoObjectsAfterExitHook.BytesAFailedSynchronizedWriteLeftGoOutOnALaterSyncWithStdio", "1"),
              "0123456789");
}

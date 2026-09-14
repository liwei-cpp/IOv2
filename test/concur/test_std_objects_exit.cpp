// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * exit() must not wait for another thread's stream lock.
 *
 * The six standard output stream objects register a sing_temp exit hook that
 * flushes whatever they still hold. That hook goes through
 * out_flusher::try_flush(), which takes io_mutex() with try_to_lock: when
 * another thread holds it the hook gives up and those buffered bytes are lost.
 * A blocking flush() there would hang rather than deadlock -- every insertion
 * runs the user's io_traits::swrite inside the lock, so one thread parked in
 * there on a socket, or on a condition variable the exiting thread was supposed
 * to signal, is enough to keep exit() from ever returning. glibc's _IO_cleanup
 * uses _IO_flush_all_lockp(0), which takes no FILE lock, for the same reason.
 *
 * Neither half of that is observable in-process, so this forks a child which
 * holds cout's io_mutex() on a second thread and then calls exit(0); the parent
 * waits on it with a deadline and kills it if it overruns. The control leg --
 * same child, nobody holding the lock -- is what keeps the test honest: a
 * try_flush() that always gave up would fail it.
 *
 * The second case is the everyday shape of the same exit: a detached worker
 * keeps inserting while main() returns. The stream objects are never destroyed,
 * so the worker reads live memory and the process leaves cleanly; under the
 * gcc-tsan preset this is the run that would report a use-after-free if that
 * ever regressed.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/exe_path.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    const char* const kModeEnv = "IOV2_EXIT_HOOK_MODE";
    const char* const kOutFile = "exit_hook_child.out";
    const char* const kPending = "PENDING-AT-EXIT";

    // The child does next to nothing, but it is exec'd, so it pays for a process
    // start-up: long enough not to be flaky on a loaded machine, short enough
    // that a regression fails the suite instead of hanging it.
    constexpr auto kChildDeadline = std::chrono::seconds(15);

    // Runs this same test in a fresh process with stdout on kOutFile. Returns the
    // child's exit status, or -1 if it had to be killed for overrunning.
    int run_exit_hook_child(const char* mode)
    {
        const std::string executable = exe_path();
        std::filesystem::remove(kOutFile);

        const pid_t child = ::fork();
        if (child == -1)
            return -1;

        if (child == 0)
        {
            const int fd = ::open(kOutFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1)
                ::_exit(126);
            ::dup2(fd, STDOUT_FILENO);
            ::close(fd);
            ::setenv(kModeEnv, mode, 1);
            ::execl(executable.c_str(), executable.c_str(),
                    "--gtest_filter=StdObjectsExit.ExitIsNotBlockedByAStreamLock",
                    "--gtest_color=no", static_cast<char*>(nullptr));
            ::_exit(127);
        }

        const auto deadline = std::chrono::steady_clock::now() + kChildDeadline;
        for (;;)
        {
            int status = 0;
            const pid_t done = ::waitpid(child, &status, WNOHANG);
            if (done == child)
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            if (done == -1)
                return -1;
            if (std::chrono::steady_clock::now() >= deadline)
            {
                ::kill(child, SIGKILL);
                ::waitpid(child, &status, 0);
                return -1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // gtest writes its own lines to the same fd, so the marker is searched for
    // rather than compared against the whole file.
    bool child_wrote_pending()
    {
        std::ifstream in(kOutFile);
        std::stringstream buf;
        buf << in.rdbuf();
        return buf.str().find(kPending) != std::string::npos;
    }

    // Static, not a local: the holder / worker thread is detached and outlives
    // every scope in the child, which never returns from the test body.
    std::atomic<bool> g_lock_held{false};
    std::atomic<long> g_inserts{0};
}

TEST(StdObjectsExit, ExitIsNotBlockedByAStreamLock)
{
    const char* const mode = std::getenv(kModeEnv);
    if (mode == nullptr)
    {
        EXPECT_EQ(run_exit_hook_child("hold"), 0)
            << "exit() did not return while another thread held cout's io_mutex()";
        EXPECT_FALSE(child_wrote_pending())
            << "try_flush() took a lock it should have given up on";

        EXPECT_EQ(run_exit_hook_child("free"), 0);
        EXPECT_TRUE(child_wrote_pending())
            << "the exit hook flushed nothing even with the lock free";

        EXPECT_EQ(run_exit_hook_child("worker"), 0)
            << "exit with a detached thread still inserting did not return cleanly";
        return;
    }

    // --- child ---
    // Unsynchronized, so the bytes stay in this stream's own buffer and the exit
    // hook is the only thing that can move them to the fd.
    IOv2::cout.sync_with_stdio(false);
    IOv2::cout << kPending;

    if (std::string_view(mode) == "hold")
    {
        std::thread holder([] {
            std::lock_guard guard(IOv2::cout.io_mutex());
            g_lock_held.store(true);
            for (;;) std::this_thread::sleep_for(std::chrono::hours(1));
        });
        holder.detach();
        while (!g_lock_held.load())
            std::this_thread::yield();
    }
    else if (std::string_view(mode) == "worker")
    {
        std::thread worker([] {
            for (;;) { IOv2::cout << "w\n"; ++g_inserts; }
        });
        worker.detach();
        while (g_inserts.load() < 100)
            std::this_thread::yield();
    }

    std::exit(0);   // runs the six exit hooks
}

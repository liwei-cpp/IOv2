// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The facets' process-wide data must outlive process exit.
 *
 * The eight standard stream objects are never destroyed (see
 * test_std_objects_exit.cpp), so a detached thread that keeps using them while
 * main() returns reads live memory. That promise is only as good as the data
 * the facets behind those streams reach for: the time-zone trie that %Z
 * parsing walks and the text-domain table that messages consults are
 * process-wide too, and if either were an ordinary static with a destructor it
 * would be torn down by exit() while the streams that use it are still alive --
 * a heap-use-after-free in the worker, or in the destructor of any static
 * object that was constructed before the facet data and therefore outlives it.
 *
 * Each case forks a child that drives one of those paths from a detached
 * thread while main() returns and expects it to exit cleanly; under the
 * sanitizer preset a regression is an ASan report and a non-zero exit status.
 * The single-threaded shape (a static object destroyed after the facet data)
 * is not a case here: which static goes first depends on the link order of the
 * suite's translation units, so it could not fail deterministically.
 */
#include <IOv2/facet/messages.h>
#include <IOv2/facet/timeio.h>

#include <support/exe_path.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    const char* const kModeEnv = "IOV2_FACET_STATICS_MODE";
    constexpr auto kChildDeadline = std::chrono::seconds(15);

    void parse_zone()
    {
        IOv2::timeio<char> obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
        const std::string input = "America/Los_Angeles";
        IOv2::time_parse_context<char> ctx;
        (void)obj.get(input.begin(), input.end(), ctx, std::string("%Z"));
    }

    // Runs this same test in a fresh process. Returns the child's exit status,
    // or -1 if it had to be killed for overrunning.
    int run_child(const char* mode)
    {
        const std::string executable = exe_path();

        const pid_t child = ::fork();
        if (child == -1)
            return -1;

        if (child == 0)
        {
            ::setenv(kModeEnv, mode, 1);
            ::execl(executable.c_str(), executable.c_str(),
                    "--gtest_filter=FacetStaticsExit.ProcessWideFacetDataSurvivesExit",
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

    // Static, not a local: the worker is detached and outlives every scope in
    // the child, which never returns from the test body.
    std::atomic<long> g_rounds{0};
}

TEST(FacetStaticsExit, ProcessWideFacetDataSurvivesExit)
{
    const char* const mode = std::getenv(kModeEnv);
    if (mode == nullptr)
    {
        EXPECT_EQ(run_child("zone"), 0)
            << "a detached thread parsing %Z while main() returns did not exit cleanly";
        EXPECT_EQ(run_child("domain"), 0)
            << "a detached thread using the text-domain table while main() returns did not exit cleanly";
        return;
    }

    // --- child ---
    if (std::string_view(mode) == "zone")
    {
        std::thread worker([] {
            for (;;) { parse_zone(); ++g_rounds; }
        });
        worker.detach();
    }
    else
    {
        std::thread worker([] {
            for (unsigned long n = 0;; ++n)
            {
                const std::string domain = "dom" + std::to_string(n % 64);
                IOv2::base_ft<IOv2::messages>::bind_text_domain(domain, "/nonexistent");
                (void)IOv2::base_ft<IOv2::messages>::get_dirname(domain);
                ++g_rounds;
            }
        });
        worker.detach();
    }

    while (g_rounds.load() < 100)
        std::this_thread::yield();
    std::exit(0);
}

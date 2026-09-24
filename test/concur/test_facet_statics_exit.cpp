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
 *
 * The single-threaded shape -- a static object constructed before the facet
 * data, and so destroyed after it, that parses %Z in its destructor -- is the
 * third case. Which static of default priority goes first depends on the link
 * order of the suite's translation units, so the late object is given
 * init_priority(101): it is constructed before every default-priority
 * initializer whatever the link order, and destroyed after all of them.
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

    void use_domain_table()
    {
        IOv2::base_ft<IOv2::messages>::bind_text_domain("late", "/nonexistent");
        (void)IOv2::base_ft<IOv2::messages>::get_dirname("late");
    }

    // Static, not a local: the worker is detached and outlives every scope in
    // the child, which never returns from the test body.
    std::atomic<long> g_rounds{0};

    // Its constructor must not touch IOv2: it runs before any of the library's
    // statics exist. Only the destructor, in the "late_static" child, uses them.
    struct late_user
    {
        late_user() = default;
        late_user(const late_user&) = delete;
        late_user& operator=(const late_user&) = delete;

        ~late_user()
        {
            const char* const mode = std::getenv(kModeEnv);
            if (mode == nullptr || std::string_view(mode) != "late_static")
                return;
            parse_zone();
            use_domain_table();
        }
    };

    __attribute__((init_priority(101))) late_user g_late_user;
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
        EXPECT_EQ(run_child("late_static"), 0)
            << "a static destroyed after the facet data could not parse %Z or use the text-domain table";
        return;
    }

    // --- child ---
    if (std::string_view(mode) == "late_static")
        return;  // g_late_user's destructor does the work once main() returns
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

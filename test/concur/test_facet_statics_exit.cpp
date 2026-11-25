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
 * Each case starts a child that drives one of those paths from a detached
 * thread while main() returns and expects it to exit cleanly; under the
 * sanitizer preset a regression is an ASan report and a non-zero exit status.
 *
 * The single-threaded shape -- a static object constructed before the facet
 * data, and so destroyed after it, that parses %Z in its destructor -- is the
 * third case. Which static of default priority goes first depends on the link
 * order of the suite's translation units, so the late object is given
 * init_priority(101): it is constructed before every default-priority
 * initializer whatever the link order, and destroyed after all of them. That
 * ordering is ELF's -- cl.exe has no init_priority, and on Mach-O it holds within
 * one object file only -- so the third case runs on glibc only.
 */
#include <IOv2/facet/messages.h>
#include <IOv2/facet/timeio.h>

#include <support/platform.h>
#include <support/platform_macros.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace
{
    namespace sys = iov2_test::sys;

    const char* const kModeEnv = "IOV2_FACET_STATICS_MODE";
    constexpr auto kChildDeadline = std::chrono::seconds(120);   // see test_std_objects_exit.cpp

    // Without a time-zone database (macOS) the trie holds only UTC and UNKNOWN.
    void parse_zone()
    {
        IOv2::timeio<char> obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
#if IOV2_HAS_CHRONO_TZDB
        const std::string input = "America/Los_Angeles";
#else
        const std::string input = "UTC";
#endif
        IOv2::time_parse_context<char> ctx;
        (void)obj.get(input.begin(), input.end(), ctx, std::string("%Z"));
    }

    // Runs this same test in a fresh process. Returns the child's exit status,
    // or -1 if it had to be killed for overrunning.
    int run_child(const char* mode)
    {
        return sys::run_self({"--gtest_filter=FacetStaticsExit.ProcessWideFacetDataSurvivesExit",
                              "--gtest_color=no"},
                             kModeEnv, mode, kChildDeadline);
    }

    // Static, not a local: the worker is detached and outlives every scope in
    // the child, which never returns from the test body.
    std::atomic<long> g_rounds{0};

#if IOV2_TEST_LINUX_GLIBC
    void use_domain_table()
    {
        IOv2::base_ft<IOv2::messages>::bind_text_domain("late", "/nonexistent");
        (void)IOv2::base_ft<IOv2::messages>::get_dirname("late");
    }

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
#endif
}

TEST(FacetStaticsExit, ProcessWideFacetDataSurvivesExit)
{
    const std::string mode = sys::get_env(kModeEnv);
    if (mode.empty())
    {
        EXPECT_EQ(run_child("zone"), 0)
            << "a detached thread parsing %Z while main() returns did not exit cleanly";
        EXPECT_EQ(run_child("domain"), 0)
            << "a detached thread using the text-domain table while main() returns did not exit cleanly";
#if IOV2_TEST_LINUX_GLIBC
        EXPECT_EQ(run_child("late_static"), 0)
            << "a static destroyed after the facet data could not parse %Z or use the text-domain table";
#endif
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

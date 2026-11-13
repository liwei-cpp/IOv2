// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The name a clocale_wrapper reports is the C library's, and each library spells
 * and normalizes names its own way, so what is expected differs by platform.
 */
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/defs.h>

#include <support/platform_macros.h>

#include <gtest/gtest.h>

#include <atomic>
#include <clocale>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#if IOV2_TEST_MACOS
#include <stdlib.h>
#endif

using namespace IOv2;

// ---------------------------------------------------------------- glibc

// glibc normalizes an alias in the name it reports: "POSIX" comes back as "C", and so
// does a copy of it.
TEST(ClocaleWrapperGlibc, PosixIsReportedAsC)
{
#if IOV2_TEST_LINUX_GLIBC
    const clocale_wrapper loc("POSIX");
    EXPECT_EQ(loc.name(), "C");
    EXPECT_EQ(clocale_wrapper(loc).name(), "C");
#else
    GTEST_SKIP() << "glibc's normalization of locale names";
#endif
}

// ---------------------------------------------------------------- macOS

namespace
{
#if IOV2_TEST_MACOS
    // LC_ALL outranks LC_CTYPE, so it has to be out of the way too.
    constexpr const char* kLocaleVars[] = {"LC_ALL", "LC_CTYPE"};
#endif

    class ClocaleWrapperMacosEnv : public ::testing::Test
    {
#if IOV2_TEST_MACOS
    protected:
        void SetUp() override
        {
            for (const char* name : kLocaleVars)
            {
                const char* value = std::getenv(name);
                m_saved.push_back(value ? std::optional<std::string>(value) : std::nullopt);
                ::unsetenv(name);
            }
        }

        void TearDown() override
        {
            for (std::size_t i = 0; i < std::size(kLocaleVars); ++i)
            {
                if (m_saved[i]) ::setenv(kLocaleVars[i], m_saved[i]->c_str(), 1);
                else            ::unsetenv(kLocaleVars[i]);
            }
        }

        std::vector<std::optional<std::string>> m_saved;
#endif
    };
}

// macOS reports the name as given: "POSIX" is not normalized to "C", and the name
// still builds the same locale again.
TEST(ClocaleWrapperMacos, PosixIsReportedAsPosix)
{
#if IOV2_TEST_MACOS
    const clocale_wrapper loc("POSIX");
    EXPECT_EQ(loc.name(), "POSIX");
    EXPECT_EQ(clocale_wrapper(loc).name(), "POSIX");
    EXPECT_EQ(clocale_wrapper(loc.name()).name(), "POSIX");
#else
    GTEST_SKIP() << "Apple libc's spelling of locale names";
#endif
}

// UTF-8 is a locale with an LC_CTYPE category and nothing else. "" picks it up under
// LC_CTYPE=UTF-8, and the name reported then is one the constructor's
// newlocale(LC_ALL_MASK) rejects.
TEST_F(ClocaleWrapperMacosEnv, ACtypeOnlyLocaleNameBuildsNoWrapper)
{
#if IOV2_TEST_MACOS
    ::setenv("LC_CTYPE", "UTF-8", 1);
    const clocale_wrapper loc("");
    EXPECT_EQ(loc.name(), "UTF-8");
    EXPECT_THROW((void)clocale_wrapper(loc.name()), io_error);
#else
    GTEST_SKIP() << "Apple libc's LC_CTYPE-only UTF-8 locale";
#endif
}

// ---------------------------------------------------------------- Windows

#if IOV2_TEST_WINDOWS
namespace
{
    // A locale every Windows has, and one whose decimal point is not the C locale's.
    constexpr const char* kGerman = "German_Germany.1252";

    std::string thread_locale() { return std::setlocale(LC_ALL, nullptr); }
    char decimal_point() { return *std::localeconv()->decimal_point; }
    int thread_mode() { return _configthreadlocale(0); }
}
#endif

// The CRT does not know "POSIX", so neither does the wrapper.
TEST(ClocaleWrapperWindows, PosixIsNotAName)
{
#if IOV2_TEST_WINDOWS
    EXPECT_THROW((void)clocale_wrapper("POSIX"), io_error);
#else
    GTEST_SKIP() << "the CRT's locale names";
#endif
}

// The name is the CRT's spelling, and it builds the same locale again.
TEST(ClocaleWrapperWindows, NameIsTheCrtSpelling)
{
#if IOV2_TEST_WINDOWS
    const clocale_wrapper loc(kGerman);
    EXPECT_EQ(loc.name(), kGerman);
    EXPECT_EQ(clocale_wrapper(loc).name(), kGerman);
    EXPECT_EQ(clocale_wrapper(loc.name()).name(), kGerman);
#else
    GTEST_SKIP() << "the CRT's locale names";
#endif
}

// Resolving a name goes through setlocale, in per-thread mode; the thread's locale and
// mode are as they were afterwards.
TEST(ClocaleWrapperWindows, ConstructionLeavesTheThreadLocaleAlone)
{
#if IOV2_TEST_WINDOWS
    const std::string before = thread_locale();
    const int mode = thread_mode();
    {
        const clocale_wrapper env("");
        const clocale_wrapper german(kGerman);
    }
    EXPECT_EQ(thread_locale(), before);
    EXPECT_EQ(thread_mode(), mode);
#else
    GTEST_SKIP() << "the CRT's per-thread locale mode";
#endif
}

// clocale_user switches the thread it runs on and no other: while one thread is inside,
// another sees neither its name nor its decimal point. Afterwards the first thread's
// locale and mode are back.
TEST(ClocaleWrapperWindows, ClocaleUserSwitchesItsOwnThreadOnly)
{
#if IOV2_TEST_WINDOWS
    const clocale_wrapper german(kGerman);
    const std::string global = thread_locale();
    const int mode = thread_mode();

    std::atomic<int> step{0};
    std::string before, inside, after;
    int before_mode = 0, after_mode = 0;
    char inside_dp = 0;
    std::thread t([&] {
        before = thread_locale();
        before_mode = thread_mode();
        {
            clocale_user user(german);
            inside = thread_locale();
            inside_dp = decimal_point();
            step = 1;
            while (step != 2) std::this_thread::yield();
        }
        after = thread_locale();
        after_mode = thread_mode();
    });
    while (step != 1) std::this_thread::yield();
    EXPECT_EQ(thread_locale(), global);
    EXPECT_EQ(decimal_point(), '.');
    EXPECT_EQ(thread_mode(), mode);
    step = 2;
    t.join();

    EXPECT_EQ(inside, kGerman);
    EXPECT_EQ(inside_dp, ',');
    EXPECT_EQ(after, before);
    EXPECT_EQ(after_mode, before_mode);
#else
    GTEST_SKIP() << "the CRT's per-thread locale mode";
#endif
}

// A thread already in per-thread mode with a locale of its own gets both back.
TEST(ClocaleWrapperWindows, ClocaleUserNestsInPerThreadMode)
{
#if IOV2_TEST_WINDOWS
    const clocale_wrapper c("C");
    std::thread t([&] {
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        std::setlocale(LC_ALL, kGerman);
        { clocale_user user(c); EXPECT_EQ(thread_locale(), "C"); }
        EXPECT_EQ(thread_locale(), kGerman);
        EXPECT_EQ(thread_mode(), _ENABLE_PER_THREAD_LOCALE);
    });
    t.join();
#else
    GTEST_SKIP() << "the CRT's per-thread locale mode";
#endif
}

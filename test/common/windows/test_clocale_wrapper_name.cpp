// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/defs.h>

#include <gtest/gtest.h>

#include <atomic>
#include <clocale>
#include <string>
#include <thread>

using namespace IOv2;

namespace
{
    // A locale every Windows has, and one whose decimal point is not the C locale's.
    constexpr const char* kGerman = "German_Germany.1252";

    std::string thread_locale() { return std::setlocale(LC_ALL, nullptr); }
    char decimal_point() { return *std::localeconv()->decimal_point; }
    int thread_mode() { return _configthreadlocale(0); }
}

// The CRT does not know "POSIX", so neither does the wrapper.
TEST(ClocaleWrapperWindows, PosixIsNotAName)
{
    EXPECT_THROW((void)clocale_wrapper("POSIX"), io_error);
}

// The name is the CRT's spelling, and it builds the same locale again.
TEST(ClocaleWrapperWindows, NameIsTheCrtSpelling)
{
    const clocale_wrapper loc(kGerman);
    EXPECT_EQ(loc.name(), kGerman);
    EXPECT_EQ(clocale_wrapper(loc).name(), kGerman);
    EXPECT_EQ(clocale_wrapper(loc.name()).name(), kGerman);
}

// Resolving a name goes through setlocale, in per-thread mode; the thread's locale and
// mode are as they were afterwards.
TEST(ClocaleWrapperWindows, ConstructionLeavesTheThreadLocaleAlone)
{
    const std::string before = thread_locale();
    const int mode = thread_mode();
    {
        const clocale_wrapper env("");
        const clocale_wrapper german(kGerman);
    }
    EXPECT_EQ(thread_locale(), before);
    EXPECT_EQ(thread_mode(), mode);
}

// clocale_user switches the thread it runs on and no other: while one thread is inside,
// another sees neither its name nor its decimal point. Afterwards the first thread's
// locale and mode are back.
TEST(ClocaleWrapperWindows, ClocaleUserSwitchesItsOwnThreadOnly)
{
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
}

// A thread already in per-thread mode with a locale of its own gets both back.
TEST(ClocaleWrapperWindows, ClocaleUserNestsInPerThreadMode)
{
    const clocale_wrapper c("C");
    std::thread t([&] {
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        std::setlocale(LC_ALL, kGerman);
        { clocale_user user(c); EXPECT_EQ(thread_locale(), "C"); }
        EXPECT_EQ(thread_locale(), kGerman);
        EXPECT_EQ(thread_mode(), _ENABLE_PER_THREAD_LOCALE);
    });
    t.join();
}

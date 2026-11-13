// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

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
#include <type_traits>
#include <utility>
#include <vector>

#if IOV2_TEST_MACOS
#include <stdlib.h>
#endif

using namespace IOv2;

TEST(ClocaleWrapper, NothrowTraits)
{
    static_assert(std::is_nothrow_destructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_constructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_assignable_v<clocale_wrapper>);
}

// name() says which locale a wrapper holds but not whether it holds exactly one, so
// what the move and copy paths are checked for here is that they neither throw nor
// double-free. The freelocale() side is what the sanitizer and valgrind jobs are
// watching; NameSurvivesCopyAndMove covers the identity of what was handed over.
TEST(ClocaleWrapper, MoveConstructAndAssign)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(std::move(loc1));

        clocale_wrapper loc3("C");
        loc3 = std::move(loc2);

        // loc1 is moved-from, so this also exercises assignment into an empty
        // target rather than only replacing a live locale.
        loc1 = std::move(loc3);
    });
}

TEST(ClocaleWrapper, CopyConstructAndAssign)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(loc1);

        clocale_wrapper loc3("C");
        loc3 = loc2;

        // Move loc3's locale away, then copy back into the moved-from target.
        clocale_wrapper loc4(std::move(loc3));
        loc3 = loc4;
    });
}

TEST(ClocaleWrapper, SelfAssignment)
{
    clocale_wrapper loc1("C");

    // Through a pointer, otherwise -Wself-assign-overloaded rejects it.
    EXPECT_NO_THROW([&loc1](clocale_wrapper* p) { loc1 = *p; }(&loc1));

    // Outside the macro: a #pragma inside a macro argument is not honoured, and
    // self-move-assignment is noexcept anyway, so there is nothing to wrap.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    loc1 = std::move(loc1);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

// A moved-from wrapper holds a null locale_t, and both copy paths have to take
// the null branch rather than hand it to duplocale().
TEST(ClocaleWrapper, CopyFromMovedFrom)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(std::move(loc1));

        clocale_wrapper loc3(loc1);
        clocale_wrapper loc4("C");
        loc4 = loc1;
    });
}

// The name is taken at its full std::string length. Cut at c_str() instead, a
// leading NUL would read as "" and quietly resolve the environment's locale in
// place of the one the caller named; a NUL further in would silently shorten it.
TEST(ClocaleWrapper, ANameWithAnEmbeddedNulIsRejected)
{
    EXPECT_THROW((void)clocale_wrapper(std::string("\0C", 2)), io_error);
    EXPECT_THROW((void)clocale_wrapper(std::string("C\0junk", 6)), io_error);
    EXPECT_THROW((void)clocale_wrapper(std::string("C\0", 2)), io_error);
}

// name() reports the locale's own resolved LC_CTYPE name, which is what lets it stand
// in for the string the wrapper was built from: "" is a lookup rather than a name and
// comes back concrete, and whatever comes back builds the same locale again. What an
// alias comes back as is the C library's call; the per-platform cases below pin it.
TEST(ClocaleWrapper, NameReportsTheResolvedLocale)
{
    EXPECT_EQ(clocale_wrapper("C").name(), "C");

    const std::string resolved = clocale_wrapper("").name();
    EXPECT_FALSE(resolved.empty());
    EXPECT_EQ(clocale_wrapper(resolved).name(), resolved);
}

// A copy is a locale of its own, and it has to be the same locale; the source of a move
// keeps nothing and can no longer be asked. "C" because it is the one name every
// platform knows; "POSIX" is not one on Windows.
TEST(ClocaleWrapper, NameSurvivesCopyAndMove)
{
    clocale_wrapper loc1("C");
    const std::string name = loc1.name();
    clocale_wrapper loc2(loc1);
    EXPECT_EQ(loc2.name(), name);

    clocale_wrapper loc3(std::move(loc1));
    EXPECT_EQ(loc3.name(), name);
    EXPECT_THROW((void)loc1.name(), io_error);
}

TEST(ClocaleWrapper, ClocaleUserRejectsMovedFrom)
{
    clocale_wrapper loc1("C");
    clocale_wrapper loc2(std::move(loc1));

    EXPECT_THROW((void)clocale_user{loc1}, io_error);
}

TEST(ClocaleWrapper, ClocaleUserAcceptsLiveWrapper)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc3("C");
        clocale_user user(loc3);
    });
}

// ---------------------------------------------------------------------------
// The name a clocale_wrapper reports is the C library's, and each library spells
// and normalizes names its own way, so what is expected differs by platform.
// ---------------------------------------------------------------------------

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

TEST(CommonDefs, DefaultEofErrorMessage)
{
    const eof_error error;
    EXPECT_STREQ(error.what(), "end of file");
}

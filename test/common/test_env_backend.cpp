// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::detail::get_env: a copy of an environment variable's value, or "" when the
 * variable is unset or empty.  The variables are set and removed through the test
 * support's own calls -- the platform's setenv/unsetenv or _putenv_s -- so what the
 * cases check is the library's read alone.
 */
#include <IOv2/common/platform/env_backend.h>

#include <support/platform.h>

#include <gtest/gtest.h>

#include <string>

namespace
{
    constexpr const char* kName = "IOV2_TEST_GET_ENV";

    // The variable is unset when a case starts and when it ends, however it ends.
    struct env_guard
    {
        env_guard() { iov2_test::sys::unset_env(kName); }
        env_guard(const env_guard&) = delete;
        env_guard& operator=(const env_guard&) = delete;
        ~env_guard() { iov2_test::sys::unset_env(kName); }
    };
}

TEST(GetEnv, AnUnsetVariableReadsEmpty)
{
    const env_guard guard;
    EXPECT_EQ(IOv2::detail::get_env(kName), "");
}

TEST(GetEnv, ASetVariableReadsItsValue)
{
    const env_guard guard;
    iov2_test::sys::set_env(kName, "zh_CN.UTF-8:fr_FR");
    EXPECT_EQ(IOv2::detail::get_env(kName), "zh_CN.UTF-8:fr_FR");
}

// Windows asks for the size before the value; one far past any small buffer checks
// that the size it is told is the whole value, and that the null is not kept.
TEST(GetEnv, ALongValueComesBackWhole)
{
    const env_guard guard;
    const std::string value = std::string(4000, 'x') + "end";
    iov2_test::sys::set_env(kName, value);
    EXPECT_EQ(IOv2::detail::get_env(kName), value);
}

// Setting the empty string leaves the variable set but empty on POSIX and removes it
// on Windows; either way it reads the same as unset.
TEST(GetEnv, AnEmptyValueReadsAsUnset)
{
    const env_guard guard;
    iov2_test::sys::set_env(kName, "");
    EXPECT_EQ(IOv2::detail::get_env(kName), "");
}

// What was read is a copy: changing the variable afterwards leaves it alone.
TEST(GetEnv, TheValueIsACopy)
{
    const env_guard guard;
    iov2_test::sys::set_env(kName, "first");
    const std::string read = IOv2::detail::get_env(kName);
    iov2_test::sys::set_env(kName, "second");
    EXPECT_EQ(read, "first");
    EXPECT_EQ(IOv2::detail::get_env(kName), "second");
}

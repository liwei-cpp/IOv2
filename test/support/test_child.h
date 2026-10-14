// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// Runs one test of this executable in a child process of its own, for checks
// that must not touch the test process itself (a file size limit, fd 2
// pointed at a file).
//
// The child is a fresh exec of this executable, not a bare fork that carries
// on in the test body: a forked child that ends with _exit() still holds all
// it inherited, and Valgrind, which follows fork but not exec, reports that as
// leaked -- and its --error-exitcode then fails the child.
//
// Usage, in the test body:
//     if (in_test_child()) { ... checks, reading test_child_arg() ...; return; }
//     EXPECT_EQ(run_test_child("Suite.Name", "arg"), 0);

#include <support/exe_path.h>
#include <support/platform.h>

#include <cstdlib>
#include <string>

namespace
{
    inline const char* const test_child_env = "IOV2_TEST_CHILD";

    inline bool in_test_child() { return std::getenv(test_child_env) != nullptr; }

    // What the parent passed to run_test_child(); empty outside a child.
    inline std::string test_child_arg()
    {
        const char* arg = std::getenv(test_child_env);
        return arg != nullptr ? arg : "";
    }

    // Re-runs this executable on `test` (a full "Suite.Name") with `arg` in the
    // environment. Returns the child's exit status -- 0 when its checks passed --
    // or -1 if it could not be run.
    // `arg` must not be empty: on Windows an empty value removes the variable.
    inline int run_test_child(const std::string& test, const std::string& arg = "1")
    {
        return iov2_test::sys::run_self({"--gtest_filter=" + test, "--gtest_color=no"},
                                        test_child_env, arg);
    }
}

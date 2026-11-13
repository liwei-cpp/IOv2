// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// The platforms the tests know, one macro each, always defined as 0 or 1. A test
// that cannot run everywhere says where it runs with #if on these -- never with
// #ifdef, and never on the compiler's own _WIN32 or __linux__ -- and keeps its
// TEST() on every platform, skipping elsewhere with the reason:
//
//     TEST(Suite, Case)
//     {
//     #if !IOV2_TEST_WINDOWS
//         ...
//     #else
//         GTEST_SKIP() << "the CRT's text mode turns \\n into \\r\\n";
//     #endif
//     }
//
// so every platform reports the same list of tests. -Wundef makes a misspelled
// name, or a source that forgot this header, a build error.
//
// Linux means Linux with glibc: what the tests need there (__fpending, glibc's
// locale names) is glibc's, not the kernel's. Another C library would be a
// platform of its own, with every IOV2_TEST_LINUX_GLIBC to review.

#if defined(__linux__)
#include <features.h>               // defines __GLIBC__, which no compiler predefines
#endif

#if defined(__linux__) && defined(__GLIBC__)
#define IOV2_TEST_LINUX_GLIBC 1
#else
#define IOV2_TEST_LINUX_GLIBC 0
#endif

#if defined(__APPLE__)
#define IOV2_TEST_MACOS 1
#else
#define IOV2_TEST_MACOS 0
#endif

#if defined(_WIN32)
#define IOV2_TEST_WINDOWS 1
#else
#define IOV2_TEST_WINDOWS 0
#endif

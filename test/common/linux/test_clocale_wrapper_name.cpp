// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/clocale_wrapper.h>

#include <gtest/gtest.h>

using namespace IOv2;

// glibc normalizes an alias in the name it reports: "POSIX" comes back as "C", and so
// does a copy of it.
TEST(ClocaleWrapperGlibc, PosixIsReportedAsC)
{
    const clocale_wrapper loc("POSIX");
    EXPECT_EQ(loc.name(), "C");
    EXPECT_EQ(clocale_wrapper(loc).name(), "C");
}

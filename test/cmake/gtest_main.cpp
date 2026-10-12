// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// The main() every test suite links.
//
// One executable per suite is what makes a failure re-runnable on its own, and
// they all need the same entry point: GoogleTest finds the cases by itself, so
// there is nothing per-suite to put here. It lives beside the CMake modules,
// and test/CMakeLists.txt takes it out of the glob that turns "directory
// holding a .cpp" into "suite".

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

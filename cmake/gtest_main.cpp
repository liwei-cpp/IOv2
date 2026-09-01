// The main() every test suite links.
//
// One executable per suite is what makes a failure re-runnable on its own, and
// they all need the same entry point: GoogleTest finds the cases by itself, so
// there is nothing per-suite to put here. It lives under cmake/ rather than
// test/ so the glob that turns "directory holding a .cpp" into "suite" does not
// see it.

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

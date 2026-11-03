// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The object lock behind io_mutex(): a stream's own by default, or an external one given
 * at construction, so that streams over one resource share it (cin and wcin do). A copy or
 * a move never takes the external lock over, and assignment leaves each side with the lock
 * it had -- two independent streams must not end up sharing a lock behind the user's back.
 */
#include <IOv2/io/io_base.h>

#include <gtest/gtest.h>

#include <mutex>
#include <utility>

using namespace IOv2;

namespace
{
    using mutex_type = copyable_mutex<std::recursive_mutex>;
}

TEST(IosBaseLock, ByDefaultEachStreamHasItsOwnLock)
{
    ios_base<char> a;
    ios_base<char> b;
    EXPECT_NE(&a.io_mutex(), &b.io_mutex());
}

TEST(IosBaseLock, AnExternalLockIsTheOneIoMutexReturns)
{
    mutex_type shared;
    ios_base<char> a(&shared);
    ios_state<wchar_t> b(&shared);
    EXPECT_EQ(&a.io_mutex(), &shared);
    EXPECT_EQ(&b.io_mutex(), &shared);
}

TEST(IosBaseLock, ACopyOrAMoveUsesItsOwnLock)
{
    mutex_type shared;
    ios_base<char> a(&shared);

    ios_base<char> copied(a);
    ios_base<char> moved(std::move(a));
    EXPECT_NE(&copied.io_mutex(), &shared);
    EXPECT_NE(&moved.io_mutex(), &shared);
    EXPECT_NE(&copied.io_mutex(), &moved.io_mutex());
    EXPECT_EQ(&a.io_mutex(), &shared);             // NOLINT(bugprone-use-after-move)
}

TEST(IosBaseLock, AssignmentLeavesEachSideWithTheLockItHad)
{
    mutex_type shared;
    ios_base<char> a(&shared);
    ios_base<char> b;
    const mutex_type* const own = &b.io_mutex();

    b = a;
    EXPECT_EQ(&b.io_mutex(), own);
    a = b;
    EXPECT_EQ(&a.io_mutex(), &shared);

    b = std::move(a);
    EXPECT_EQ(&b.io_mutex(), own);
    EXPECT_EQ(&a.io_mutex(), &shared);             // NOLINT(bugprone-use-after-move)
}

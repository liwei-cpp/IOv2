// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/sing_temp.h>

#include <gtest/gtest.h>

#include <type_traits>

using namespace IOv2;

namespace
{
class singleton_probe : public sing_temp<singleton_probe>
{
    friend sing_temp<singleton_probe>;

public:
    static inline int constructions = 0;
    static inline int destructions = 0;

    [[nodiscard]] int value() const noexcept { return 42; }

private:
    singleton_probe() { ++constructions; }
    ~singleton_probe() { ++destructions; }
};

static_assert(!std::is_copy_constructible_v<singleton_probe>);
static_assert(!std::is_move_constructible_v<singleton_probe>);
static_assert(!std::is_copy_constructible_v<singleton_probe::init>);
static_assert(!std::is_move_constructible_v<singleton_probe::init>);
}

TEST(SingTemp, InitOwnsExactlyOneLifecycle)
{
    EXPECT_EQ(singleton_probe::ptr(), nullptr);
    EXPECT_EQ(singleton_probe::constructions, 0);
    EXPECT_EQ(singleton_probe::destructions, 0);

    singleton_probe* observed = nullptr;
    {
        singleton_probe::init lifetime;
        observed = singleton_probe::ptr();

        ASSERT_NE(observed, nullptr);
        EXPECT_EQ(singleton_probe::ptr(), observed);
        EXPECT_EQ(observed->value(), 42);
        EXPECT_EQ(singleton_probe::constructions, 1);
        EXPECT_EQ(singleton_probe::destructions, 0);
    }

    EXPECT_EQ(singleton_probe::ptr(), nullptr);
    EXPECT_EQ(singleton_probe::constructions, 1);
    EXPECT_EQ(singleton_probe::destructions, 1);
}

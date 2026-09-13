// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/sing_temp.h>

#include <gtest/gtest.h>

#include <stdexcept>
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

// Registers an exit hook that only flushes: the shape of the standard stream objects.
// The hook is a captureless lambda, converted to sing_temp::exit_hook.
class flushing_probe : public sing_temp<flushing_probe>
{
    friend sing_temp<flushing_probe>;

public:
    static inline int destructions = 0;
    static inline int flushes = 0;

    void flush() { ++flushes; }

private:
    flushing_probe()
        : sing_temp<flushing_probe>([](flushing_probe* p) noexcept { p->flush(); })
    {}
    ~flushing_probe() { ++destructions; }
};

// The hook's exception-swallowing is the hook's own business, but the exit path must
// survive it: a throwing flush() must neither escape ~init nor destroy the object.
class throwing_probe : public sing_temp<throwing_probe>
{
    friend sing_temp<throwing_probe>;

public:
    static inline int destructions = 0;
    static inline int flushes = 0;

    void flush()
    {
        ++flushes;
        throw std::runtime_error("flush failed");
    }

private:
    throwing_probe()
        : sing_temp<throwing_probe>([](throwing_probe* p) noexcept {
              try { p->flush(); } catch (...) {}
          })
    {}
    ~throwing_probe() { ++destructions; }
};
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

TEST(SingTemp, ExitHookReplacesDestruction)
{
    EXPECT_EQ(flushing_probe::ptr(), nullptr);

    flushing_probe* observed = nullptr;
    {
        flushing_probe::init lifetime;
        observed = flushing_probe::ptr();

        ASSERT_NE(observed, nullptr);
        EXPECT_EQ(flushing_probe::flushes, 0);
        EXPECT_EQ(flushing_probe::destructions, 0);
    }

    // The hook ran once, the object was not destroyed, and it is still reachable
    // through ptr() -- exactly what std::cout guarantees after exit begins.
    EXPECT_EQ(flushing_probe::flushes, 1);
    EXPECT_EQ(flushing_probe::destructions, 0);
    EXPECT_EQ(flushing_probe::ptr(), observed);
    observed->flush();
    EXPECT_EQ(flushing_probe::flushes, 2);
}

TEST(SingTemp, ExitHookSurvivesAThrowingFlush)
{
    {
        throwing_probe::init lifetime;
        ASSERT_NE(throwing_probe::ptr(), nullptr);
    }

    EXPECT_EQ(throwing_probe::flushes, 1);
    EXPECT_EQ(throwing_probe::destructions, 0);
    EXPECT_NE(throwing_probe::ptr(), nullptr);
}

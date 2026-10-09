// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/sing_temp.h>

#include <gtest/gtest.h>

#include <optional>
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
    EXPECT_EQ(singleton_probe::constructions, 0);
    EXPECT_EQ(singleton_probe::destructions, 0);

    {
        singleton_probe::init lifetime;
        singleton_probe& observed = lifetime.get();

        EXPECT_EQ(&lifetime.get(), &observed);
        EXPECT_EQ(observed.value(), 42);
        EXPECT_EQ(singleton_probe::constructions, 1);
        EXPECT_EQ(singleton_probe::destructions, 0);
    }

    EXPECT_EQ(singleton_probe::constructions, 1);
    EXPECT_EQ(singleton_probe::destructions, 1);
}

TEST(SingTemp, ExitHookReplacesDestruction)
{
    flushing_probe* observed = nullptr;
    {
        flushing_probe::init lifetime;
        observed = &lifetime.get();

        EXPECT_EQ(flushing_probe::flushes, 0);
        EXPECT_EQ(flushing_probe::destructions, 0);
    }

    // The hook ran once and the object was not destroyed: it lives in static
    // storage and is still usable through the reference taken earlier -- exactly
    // what std::cout guarantees after exit begins.
    EXPECT_EQ(flushing_probe::flushes, 1);
    EXPECT_EQ(flushing_probe::destructions, 0);
    observed->flush();
    EXPECT_EQ(flushing_probe::flushes, 2);
}

TEST(SingTemp, ExitHookSurvivesAThrowingFlush)
{
    throwing_probe* observed = nullptr;
    {
        throwing_probe::init lifetime;
        observed = &lifetime.get();
    }

    EXPECT_EQ(throwing_probe::flushes, 1);
    EXPECT_EQ(throwing_probe::destructions, 0);
    EXPECT_THROW(observed->flush(), std::runtime_error);   // still alive, still throwing
    EXPECT_EQ(throwing_probe::flushes, 2);
}

namespace
{
// Each test below gets a type of its own: the count and the storage are per type, and
// one process runs every test.
template <int Tag>
class counted_probe : public sing_temp<counted_probe<Tag>>
{
    friend sing_temp<counted_probe<Tag>>;

public:
    static inline int constructions = 0;
    static inline int destructions = 0;
    int state = 0;

private:
    counted_probe() { ++constructions; }
    ~counted_probe() { ++destructions; }
};

template <int Tag>
class hooked_probe : public sing_temp<hooked_probe<Tag>>
{
    friend sing_temp<hooked_probe<Tag>>;

public:
    static inline int constructions = 0;
    static inline int destructions = 0;
    static inline int flushes = 0;
    int state = 0;

private:
    hooked_probe()
        : sing_temp<hooked_probe<Tag>>([](hooked_probe* p) noexcept { p->flush(); })
    {
        ++constructions;
    }
    ~hooked_probe() { ++destructions; }
    void flush() noexcept { ++flushes; }
};
}

// The std::ios_base::Init idiom: an init constructed while another is alive used to
// placement-new the singleton again over the live one (its state reset, what it owned
// leaked) and, on going away, to destroy it under the first init's feet.
TEST(SingTemp, ASecondInitNeitherRebuildsNorWindsUpTheSingleton)
{
    using probe = counted_probe<1>;
    probe::init first;
    first.get().state = 7;
    {
        probe::init second;
        EXPECT_EQ(&second.get(), &first.get());
        EXPECT_EQ(probe::constructions, 1);
        EXPECT_EQ(second.get().state, 7);
    }
    EXPECT_EQ(probe::destructions, 0);
    EXPECT_EQ(first.get().state, 7);
}

// The exit logic belongs to whichever init goes last, not to the one that came first.
TEST(SingTemp, TheLastInitToGoRunsTheExitHook)
{
    using probe = hooked_probe<1>;
    std::optional<probe::init> first(std::in_place);
    std::optional<probe::init> second(std::in_place);
    first.reset();
    EXPECT_EQ(probe::flushes, 0);
    second.reset();
    EXPECT_EQ(probe::flushes, 1);
    EXPECT_EQ(probe::destructions, 0);
}

// The hook left the singleton alive (as the standard streams' does): an init that comes
// after the count fell to zero, during exit say, must not construct it again over itself.
TEST(SingTemp, AnInitAfterTheHookDoesNotRebuildALiveSingleton)
{
    using probe = hooked_probe<2>;
    probe* observed = nullptr;
    {
        probe::init first;
        observed = &first.get();
        observed->state = 7;
    }
    EXPECT_EQ(probe::flushes, 1);
    {
        probe::init late;
        EXPECT_EQ(&late.get(), observed);
        EXPECT_EQ(late.get().state, 7);
    }
    EXPECT_EQ(probe::constructions, 1);
    EXPECT_EQ(probe::destructions, 0);
    EXPECT_EQ(probe::flushes, 2);
}

// Without a hook the last init destroys the singleton, and the next one starts afresh.
TEST(SingTemp, AnInitAfterTheSingletonWasDestroyedConstructsItAgain)
{
    using probe = counted_probe<2>;
    { probe::init first; first.get().state = 7; }
    EXPECT_EQ(probe::destructions, 1);
    {
        probe::init again;
        EXPECT_EQ(probe::constructions, 2);
        EXPECT_EQ(again.get().state, 0);
    }
    EXPECT_EQ(probe::destructions, 2);
}

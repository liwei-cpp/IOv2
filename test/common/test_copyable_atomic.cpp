// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/copyable_atomic.h>

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

// ---------------------------------------------------------------------------
// 1. copyable_atomic 自身的特殊成员特性：可默认构造、可拷贝、可移动，且均为 noexcept。
// ---------------------------------------------------------------------------
static_assert(std::is_default_constructible_v<copyable_atomic<bool>>);
static_assert(std::is_copy_constructible_v<copyable_atomic<bool>>);
static_assert(std::is_copy_assignable_v<copyable_atomic<bool>>);
static_assert(std::is_move_constructible_v<copyable_atomic<bool>>);
static_assert(std::is_move_assignable_v<copyable_atomic<bool>>);
static_assert(std::is_nothrow_copy_constructible_v<copyable_atomic<bool>>);
static_assert(std::is_nothrow_move_constructible_v<copyable_atomic<bool>>);
static_assert(std::is_nothrow_copy_assignable_v<copyable_atomic<bool>>);
static_assert(std::is_nothrow_move_assignable_v<copyable_atomic<bool>>);

static_assert(std::is_nothrow_copy_constructible_v<copyable_atomic<int>>);
static_assert(std::is_nothrow_move_constructible_v<copyable_atomic<int>>);

// ---------------------------------------------------------------------------
// 2. 对拷贝性/移动性的“透明”传播：外层类型的可拷贝/可移动性由**其它成员**决定，
//    而不再被 atomic 成员一票否决。这正是引入 copyable_atomic 的核心目的。
// ---------------------------------------------------------------------------
namespace
{
struct MoveOnlyHolder
{
    std::unique_ptr<int>  p;
    copyable_atomic<bool> f;
};
static_assert(std::is_move_constructible_v<MoveOnlyHolder>);
static_assert(!std::is_copy_constructible_v<MoveOnlyHolder>);

struct CopyableHolder
{
    int                  x;
    copyable_atomic<int> a;
};
static_assert(std::is_copy_constructible_v<CopyableHolder>);
static_assert(std::is_move_constructible_v<CopyableHolder>);

// 对比基线：直接内嵌 std::atomic 会让外层既不可拷贝也不可移动。
struct RawAtomicHolder
{
    int              x;
    std::atomic<int> a;
};
static_assert(!std::is_copy_constructible_v<RawAtomicHolder>);
static_assert(!std::is_move_constructible_v<RawAtomicHolder>);
}

TEST(CopyableAtomic, Traits)
{
    // 所有断言均为编译期 static_assert；能编译通过即已验证。
    SUCCEED();
}

// ---------------------------------------------------------------------------
// 3. 基本原子操作：默认值、值构造、load / store / exchange 语义正确。
// ---------------------------------------------------------------------------
TEST(CopyableAtomic, DefaultConstructionIsValueInitialized)
{
    // 默认构造为值初始化（false / 0）。
    copyable_atomic<bool> b;
    EXPECT_FALSE(b.load());
    copyable_atomic<int> i;
    EXPECT_EQ(i.load(), 0);
}

TEST(CopyableAtomic, StoreLoadExchange)
{
    // 值构造。
    copyable_atomic<int> c{7};
    EXPECT_EQ(c.load(), 7);

    // store / load。
    c.store(5);
    EXPECT_EQ(c.load(), 5);

    // exchange 返回旧值并置入新值。
    EXPECT_EQ(c.exchange(9), 5);
    EXPECT_EQ(c.load(), 9);
}

TEST(CopyableAtomic, ExchangeIsTestAndSet)
{
    // exchange 的“测试并置位”语义（flush guard 用法）：首次由 false→true 返回 false，
    // 再次 exchange(true) 返回 true（已置位）。
    copyable_atomic<bool> flag;
    EXPECT_FALSE(flag.exchange(true));
    EXPECT_TRUE(flag.load());
    EXPECT_TRUE(flag.exchange(true));
    flag.store(false);
    EXPECT_FALSE(flag.load());
}

// ---------------------------------------------------------------------------
// 4. 拷贝/移动**携带当前值**（与 copyable_mutex 的“重置”相反）：副本以一次原子读取
//    搬运源的当前值，且源与副本相互独立。
// ---------------------------------------------------------------------------
TEST(CopyableAtomic, CopyAndMoveConstructionCarryValue)
{
    // 拷贝构造携带值，源不变。
    copyable_atomic<int> src{42};
    copyable_atomic<int> cp = src;
    EXPECT_EQ(cp.load(), 42);
    EXPECT_EQ(src.load(), 42);

    // 副本独立：改副本不影响源。
    cp.store(100);
    EXPECT_EQ(cp.load(), 100);
    EXPECT_EQ(src.load(), 42);

    // 移动构造携带值。
    copyable_atomic<int> mv = std::move(src);
    EXPECT_EQ(mv.load(), 42);
}

TEST(CopyableAtomic, CopyAssignmentCarriesValue)
{
    copyable_atomic<bool> s2{true};
    copyable_atomic<bool> d2;
    d2 = s2;
    EXPECT_TRUE(d2.load());
    EXPECT_TRUE(s2.load());
}

TEST(CopyableAtomic, MoveAssignmentCarriesValue)
{
    copyable_atomic<bool> s3{true};
    copyable_atomic<bool> d3;
    d3 = std::move(s3);
    EXPECT_TRUE(d3.load());
}

// ---------------------------------------------------------------------------
// 5. 含 copyable_atomic 的外层类型可被实际拷贝/移动后仍正常工作，且各持独立的原子量。
// ---------------------------------------------------------------------------
TEST(CopyableAtomic, EnclosingTypeCopy)
{
    // 拷贝：值成员与原子成员的当前值都被复制；两者相互独立。
    CopyableHolder src{42, copyable_atomic<int>{7}};
    CopyableHolder copy = src;
    EXPECT_EQ(copy.x, 42);
    EXPECT_EQ(copy.a.load(), 7);
    copy.a.store(100);
    EXPECT_EQ(copy.a.load(), 100);
    EXPECT_EQ(src.a.load(), 7);   // 源不受影响
}

TEST(CopyableAtomic, EnclosingTypeMove)
{
    // 移动：仅可移动的外层类型也应能移动。
    MoveOnlyHolder mo{std::make_unique<int>(9), copyable_atomic<bool>{true}};
    MoveOnlyHolder moved = std::move(mo);
    ASSERT_NE(moved.p, nullptr);
    EXPECT_EQ(*moved.p, 9);
    EXPECT_TRUE(moved.f.load());
}

// ---------------------------------------------------------------------------
// 6. 并发正确性之一：N 个线程同时 exchange(true)，恰好一个观察到旧值 false——
//    正是 flush() 中“先到者进入、其余跳过”的语义。
// ---------------------------------------------------------------------------
TEST(CopyableAtomic, ExchangeHasExactlyOneWinner)
{
    copyable_atomic<bool> flag;
    std::atomic<int>      winners{0};
    const int             kThreads = 64;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]
        {
            if (!flag.exchange(true))
                winners.fetch_add(1);
        });
    }
    for (auto& th : threads)
        th.join();

    EXPECT_EQ(winners.load(), 1);
    EXPECT_TRUE(flag.load());
}

// ---------------------------------------------------------------------------
// 7. 并发正确性之二：把 exchange/store 当作自旋锁，多线程在临界区累加非原子计数器，
//    最终值必须精确无丢失更新——验证 exchange/store 确实序列化了访问。
// ---------------------------------------------------------------------------
TEST(CopyableAtomic, MutualExclusion)
{
    copyable_atomic<bool> lock;
    long long             counter    = 0;          // 故意用非原子类型
    const int             kThreads   = 8;
    const long long       kPerThread = 20000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]
        {
            for (long long i = 0; i < kPerThread; ++i)
            {
                while (lock.exchange(true)) { /* spin until acquired */ }
                ++counter;
                lock.store(false);
            }
        });
    }
    for (auto& th : threads)
        th.join();

    EXPECT_EQ(counter, static_cast<long long>(kThreads) * kPerThread);
}

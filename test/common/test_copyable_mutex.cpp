// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/copyable_mutex.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace IOv2;

// ---------------------------------------------------------------------------
// 1. copyable_mutex 自身的特殊成员特性：可拷贝、可移动，且 move 为 noexcept
// ---------------------------------------------------------------------------
static_assert(std::is_default_constructible_v<copyable_mutex<>>);
static_assert(std::is_copy_constructible_v<copyable_mutex<>>);
static_assert(std::is_copy_assignable_v<copyable_mutex<>>);
static_assert(std::is_move_constructible_v<copyable_mutex<>>);
static_assert(std::is_move_assignable_v<copyable_mutex<>>);
static_assert(std::is_nothrow_copy_constructible_v<copyable_mutex<>>);
static_assert(std::is_nothrow_move_constructible_v<copyable_mutex<>>);

// 递归形态同样保持“拷贝/移动透明”，其可拷贝/可移动性不受底层 recursive_mutex 影响。
static_assert(std::is_copy_constructible_v<copyable_mutex<std::recursive_mutex>>);
static_assert(std::is_move_constructible_v<copyable_mutex<std::recursive_mutex>>);
static_assert(std::is_nothrow_copy_constructible_v<copyable_mutex<std::recursive_mutex>>);
static_assert(std::is_nothrow_move_constructible_v<copyable_mutex<std::recursive_mutex>>);

// ---------------------------------------------------------------------------
// 2. 对拷贝性/移动性的“透明”传播：外层类型的可拷贝/可移动性由**其它成员**决定，
//    而不再被 mutex 成员一票否决。这正是引入 copyable_mutex 的核心目的。
// ---------------------------------------------------------------------------
namespace
{
    struct MoveOnlyHolder
    {
        std::unique_ptr<int> p;
        copyable_mutex<>     m;
    };

    struct CopyableHolder
    {
        int              x;
        copyable_mutex<> m;
    };

    // 对比基线：直接内嵌 std::mutex 会让外层既不可拷贝也不可移动。
    struct RawMutexHolder
    {
        int        x;
        std::mutex m;
    };
}

static_assert(std::is_move_constructible_v<MoveOnlyHolder>);
static_assert(!std::is_copy_constructible_v<MoveOnlyHolder>);

static_assert(std::is_copy_constructible_v<CopyableHolder>);
static_assert(std::is_move_constructible_v<CopyableHolder>);

static_assert(!std::is_copy_constructible_v<RawMutexHolder>);
static_assert(!std::is_move_constructible_v<RawMutexHolder>);

TEST(CopyableMutex, Traits)
{
    // 所有断言均为编译期 static_assert；能编译通过即已验证。
    SUCCEED();
}

// ---------------------------------------------------------------------------
// 3. BasicLockable / Lockable 行为：lock / unlock / try_lock 语义正确。
//
// “已加锁时 try_lock 失败”这一条必须由另一个线程来观察：对调用线程已持有的
// 非递归 mutex 调用 try_lock 是未定义行为（[thread.mutex.requirements.mutex]），
// 而 copyable_mutex::try_lock() 直接转发给底层 mutex。libstdc++ 自己的
// 30_threads/mutex/try_lock/2.cc 起一个线程也正是为此。
// ---------------------------------------------------------------------------
namespace
{
    bool try_lock_from_another_thread(copyable_mutex<>& m)
    {
        bool acquired = false;
        // 必须在同一线程解锁：由非持有线程 unlock 同样是未定义行为。
        std::thread t([&]
        {
            acquired = m.try_lock();
            if (acquired)
                m.unlock();
        });
        t.join();
        return acquired;
    }
}

TEST(CopyableMutex, Lockable)
{
    copyable_mutex<> m;

    // 空闲时可 try_lock 成功；持有期间另一线程再试则失败（非递归）。
    EXPECT_TRUE(m.try_lock());
    EXPECT_FALSE(try_lock_from_another_thread(m));
    m.unlock();

    // 解锁后可再次获取。
    EXPECT_TRUE(m.try_lock());
    m.unlock();

    // 可直接用于 std::lock_guard。
    {
        std::lock_guard g(m);
        EXPECT_FALSE(try_lock_from_another_thread(m));
    }
    // 离开作用域后已解锁。
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

// ---------------------------------------------------------------------------
// 3b. 递归形态 copyable_mutex<std::recursive_mutex>：同一线程可重入加锁，
//     这是 sentry 在 sync() 已持有 io_mutex() 时再次加锁不自锁死的前提。
// ---------------------------------------------------------------------------
TEST(CopyableMutex, Recursive)
{
    copyable_mutex<std::recursive_mutex> m;

    // 同一线程可重复获取递归锁，须逐次解锁。
    m.lock();
    EXPECT_TRUE(m.try_lock());     // 同线程重入成功（非递归会失败）
    m.lock();
    m.unlock();
    m.unlock();
    m.unlock();

    // 全部释放后再次可用。
    EXPECT_TRUE(m.try_lock());
    m.unlock();

    // 可直接用于 std::lock_guard，且与手动加锁在同线程内可叠加。
    {
        std::lock_guard g(m);
        EXPECT_TRUE(m.try_lock());  // 同线程重入
        m.unlock();
    }
}

// ---------------------------------------------------------------------------
// 4. 拷贝/移动**不搬运锁状态**：副本/移动目标持有一把全新的、独立且未加锁的锁。
// ---------------------------------------------------------------------------
TEST(CopyableMutex, CopyConstructionYieldsFreshMutex)
{
    // 拷贝构造：源已加锁，副本仍应是独立且未加锁的。
    copyable_mutex<> a;
    a.lock();
    copyable_mutex<> b = a;         // copy ctor -> 全新未加锁 mutex
    EXPECT_TRUE(b.try_lock());     // 与 a 相互独立
    b.unlock();
    a.unlock();
}

TEST(CopyableMutex, MoveConstructionYieldsFreshMutex)
{
    // 移动构造：目标应是全新未加锁的锁。
    copyable_mutex<> c;
    copyable_mutex<> d = std::move(c);
    EXPECT_TRUE(d.try_lock());
    d.unlock();
}

TEST(CopyableMutex, CopyAssignmentYieldsFreshMutex)
{
    // 拷贝赋值：不转移锁状态，目标保持可用。
    copyable_mutex<> e, f;
    e.lock();
    f = e;                         // copy assign -> no-op wrt lock state
    EXPECT_TRUE(f.try_lock());
    f.unlock();
    e.unlock();
}

TEST(CopyableMutex, MoveAssignmentYieldsFreshMutex)
{
    // 移动赋值：同样不转移锁状态。
    copyable_mutex<> g, h;
    h = std::move(g);
    EXPECT_TRUE(h.try_lock());
    h.unlock();
}

// ---------------------------------------------------------------------------
// 5. 真实互斥：多线程在锁保护下累加非原子计数器，最终值必须精确无丢失更新。
// ---------------------------------------------------------------------------
TEST(CopyableMutex, MutualExclusion)
{
    copyable_mutex<>   m;
    long long          counter    = 0;          // 故意用非原子类型
    const int          kThreads   = 8;
    const long long    kPerThread = 50000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]
        {
            for (long long i = 0; i < kPerThread; ++i)
            {
                std::lock_guard guard(m);
                ++counter;
            }
        });
    }
    for (auto& th : threads)
        th.join();

    EXPECT_EQ(counter, static_cast<long long>(kThreads) * kPerThread);
}

// ---------------------------------------------------------------------------
// 6. 含 copyable_mutex 的外层类型可被实际拷贝/移动后仍正常工作。
// ---------------------------------------------------------------------------
TEST(CopyableMutex, EnclosingTypeCopy)
{
    CopyableHolder src{42, {}};

    // 拷贝：值成员被复制；锁成员为一把独立的新锁。
    CopyableHolder copy = src;
    EXPECT_EQ(copy.x, 42);
    EXPECT_TRUE(copy.m.try_lock());
    copy.m.unlock();
    // 源的锁与副本的锁相互独立。
    src.m.lock();
    EXPECT_TRUE(copy.m.try_lock());
    copy.m.unlock();
    src.m.unlock();
}

TEST(CopyableMutex, EnclosingTypeMove)
{
    // 移动：仅可移动的外层类型也应能移动。
    MoveOnlyHolder mo{std::make_unique<int>(7), {}};
    MoveOnlyHolder moved = std::move(mo);
    ASSERT_NE(moved.p, nullptr);
    EXPECT_EQ(*moved.p, 7);
    EXPECT_TRUE(moved.m.try_lock());
    moved.m.unlock();
}

// ---------------------------------------------------------------------------
// 7. 共享形态：TMutex 有 lock_shared() 一族时照样转发，可用于 std::shared_lock。
// ---------------------------------------------------------------------------
TEST(CopyableMutex, SharedFlavorForwardsTheSharedLocks)
{
    copyable_mutex<std::shared_mutex> m;
    {
        std::shared_lock a(m);
        EXPECT_TRUE(m.try_lock_shared());   // readers share it
        m.unlock_shared();
        EXPECT_FALSE(m.try_lock());         // a writer has to wait
    }
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

// ---------------------------------------------------------------------------
// 8. 跨 fork()：fork 时另一线程持有的锁，在子进程里仍能加锁。fork() 只把调用它的线程
//    带进子进程，裸 std::mutex 在子进程里会永久阻塞（多线程程序 fork 后 exec 失败、
//    往 cerr 报错即是这一形态）。子进程是裸 fork（检查的正是 fork 本身），不看它的
//    退出码——valgrind 下 _exit 的子进程会因泄漏报告改退出码——只看它是否在时限内
//    经管道回报；卡住的子进程被杀掉，不会拖住整个测试。
// ---------------------------------------------------------------------------
namespace
{
    // Forks; the child runs `in_child` and, if it returns true, writes one byte back.
    // True when that byte arrived within 5 s.
    template <typename F>
    bool child_gets_through(F in_child)
    {
        int fds[2];
        if (::pipe(fds) != 0)
            return false;
        const pid_t pid = ::fork();
        if (pid == 0)
        {
            ::close(fds[0]);
            const char ok = 'k';
            if (in_child())
                (void)!::write(fds[1], &ok, 1);
            ::_exit(0);
        }
        ::close(fds[1]);
        pollfd p{fds[0], POLLIN, 0};
        char c = 0;
        const bool got = pid > 0 && ::poll(&p, 1, 5000) == 1 && ::read(fds[0], &c, 1) == 1;
        if (pid > 0 && !got)
            ::kill(pid, SIGKILL);
        if (pid > 0)
            ::waitpid(pid, nullptr, 0);
        ::close(fds[0]);
        return got;
    }

    // Runs `body` while another thread holds `m` through `lock` / `unlock`.
    template <typename M, typename Lock, typename Unlock, typename F>
    void while_another_thread_holds(M& m, Lock lock, Unlock unlock, F body)
    {
        std::atomic<bool> held{false}, release{false};
        std::thread t([&] {
            lock(m);
            held = true;
            while (!release)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            unlock(m);
        });
        while (!held)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        body();
        release = true;
        t.join();
    }

    template <typename M>
    void check_exclusive_held_across_fork()
    {
        M m;
        while_another_thread_holds(m, [](M& x) { x.lock(); }, [](M& x) { x.unlock(); }, [&] {
            EXPECT_TRUE(child_gets_through([&] {
                m.lock();
                m.unlock();
                if (!m.try_lock())
                    return false;
                m.unlock();
                return true;
            }));
        });
    }
}

TEST(CopyableMutex, ALockAnotherThreadHeldAtForkIsUsableInTheChild)
{
    {
        SCOPED_TRACE("std::mutex");
        check_exclusive_held_across_fork<copyable_mutex<std::mutex>>();
    }
    {
        SCOPED_TRACE("std::recursive_mutex");
        check_exclusive_held_across_fork<copyable_mutex<std::recursive_mutex>>();
    }
    {
        SCOPED_TRACE("std::shared_mutex");
        check_exclusive_held_across_fork<copyable_mutex<std::shared_mutex>>();
    }
}

TEST(CopyableMutex, ASharedLockAnotherThreadHeldAtForkLetsTheChildWrite)
{
    copyable_mutex<std::shared_mutex> m;
    while_another_thread_holds(m, [](auto& x) { x.lock_shared(); }, [](auto& x) { x.unlock_shared(); }, [&] {
        EXPECT_TRUE(child_gets_through([&] {
            std::lock_guard g(m);
            return true;
        }));
    });
}

// The parent's own lock is untouched by the fork: still held by the other thread, free
// once it lets go.
TEST(CopyableMutex, AForkLeavesTheParentsLockAlone)
{
    copyable_mutex<std::mutex> m;
    while_another_thread_holds(m, [](auto& x) { x.lock(); }, [](auto& x) { x.unlock(); }, [&] {
        EXPECT_TRUE(child_gets_through([] { return true; }));
        EXPECT_FALSE(m.try_lock());
    });
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

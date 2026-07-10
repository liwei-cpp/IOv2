#include <common/copyable_mutex.h>
#include <common/verify.h>
#include <common/dump_info.h>

#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

using namespace IOv2;

// ---------------------------------------------------------------------------
// 1. copyable_mutex 自身的特殊成员特性：可拷贝、可移动，且 move 为 noexcept
// ---------------------------------------------------------------------------
static_assert(std::is_default_constructible_v<copyable_mutex>);
static_assert(std::is_copy_constructible_v<copyable_mutex>);
static_assert(std::is_copy_assignable_v<copyable_mutex>);
static_assert(std::is_move_constructible_v<copyable_mutex>);
static_assert(std::is_move_assignable_v<copyable_mutex>);
static_assert(std::is_nothrow_copy_constructible_v<copyable_mutex>);
static_assert(std::is_nothrow_move_constructible_v<copyable_mutex>);

// ---------------------------------------------------------------------------
// 2. 对拷贝性/移动性的“透明”传播：外层类型的可拷贝/可移动性由**其它成员**决定，
//    而不再被 mutex 成员一票否决。这正是引入 copyable_mutex 的核心目的。
// ---------------------------------------------------------------------------
struct MoveOnlyHolder
{
    std::unique_ptr<int> p;
    copyable_mutex       m;
};
static_assert(std::is_move_constructible_v<MoveOnlyHolder>);
static_assert(!std::is_copy_constructible_v<MoveOnlyHolder>);

struct CopyableHolder
{
    int            x;
    copyable_mutex m;
};
static_assert(std::is_copy_constructible_v<CopyableHolder>);
static_assert(std::is_move_constructible_v<CopyableHolder>);

// 对比基线：直接内嵌 std::mutex 会让外层既不可拷贝也不可移动。
struct RawMutexHolder
{
    int        x;
    std::mutex m;
};
static_assert(!std::is_copy_constructible_v<RawMutexHolder>);
static_assert(!std::is_move_constructible_v<RawMutexHolder>);

void test_copyable_mutex_traits()
{
    dump_info("Test copyable_mutex traits...");
    // 所有断言均为编译期 static_assert；能编译通过即已验证。
    VERIFY(true);
    dump_info("Done\n");
}

// ---------------------------------------------------------------------------
// 3. BasicLockable / Lockable 行为：lock / unlock / try_lock 语义正确。
// ---------------------------------------------------------------------------
void test_copyable_mutex_lockable()
{
    dump_info("Test copyable_mutex lockable...");
    copyable_mutex m;

    // 空闲时可 try_lock 成功，随后再次 try_lock 失败（非递归）。
    VERIFY(m.try_lock());
    VERIFY(!m.try_lock());
    m.unlock();

    // 解锁后可再次获取。
    VERIFY(m.try_lock());
    m.unlock();

    // 可直接用于 std::lock_guard。
    {
        std::lock_guard<copyable_mutex> g(m);
        VERIFY(!m.try_lock());
    }
    // 离开作用域后已解锁。
    VERIFY(m.try_lock());
    m.unlock();

    dump_info("Done\n");
}

// ---------------------------------------------------------------------------
// 4. 拷贝/移动**不搬运锁状态**：副本/移动目标持有一把全新的、独立且未加锁的锁。
// ---------------------------------------------------------------------------
void test_copyable_mutex_fresh_on_copy()
{
    dump_info("Test copyable_mutex fresh-on-copy...");

    // 拷贝构造：源已加锁，副本仍应是独立且未加锁的。
    copyable_mutex a;
    a.lock();
    copyable_mutex b = a;          // copy ctor -> 全新未加锁 mutex
    VERIFY(b.try_lock());          // 与 a 相互独立
    b.unlock();
    a.unlock();

    // 移动构造：目标应是全新未加锁的锁。
    copyable_mutex c;
    copyable_mutex d = std::move(c);
    VERIFY(d.try_lock());
    d.unlock();

    // 拷贝赋值：不转移锁状态，目标保持可用。
    copyable_mutex e, f;
    e.lock();
    f = e;                         // copy assign -> no-op wrt lock state
    VERIFY(f.try_lock());
    f.unlock();
    e.unlock();

    // 移动赋值：同样不转移锁状态。
    copyable_mutex g, h;
    h = std::move(g);
    VERIFY(h.try_lock());
    h.unlock();

    dump_info("Done\n");
}

// ---------------------------------------------------------------------------
// 5. 真实互斥：多线程在锁保护下累加非原子计数器，最终值必须精确无丢失更新。
// ---------------------------------------------------------------------------
void test_copyable_mutex_mutual_exclusion()
{
    dump_info("Test copyable_mutex mutual exclusion...");

    copyable_mutex     m;
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
                std::lock_guard<copyable_mutex> guard(m);
                ++counter;
            }
        });
    }
    for (auto& th : threads)
        th.join();

    VERIFY(counter == static_cast<long long>(kThreads) * kPerThread);

    dump_info("Done\n");
}

// ---------------------------------------------------------------------------
// 6. 含 copyable_mutex 的外层类型可被实际拷贝/移动后仍正常工作。
// ---------------------------------------------------------------------------
void test_copyable_mutex_enclosing_type()
{
    dump_info("Test copyable_mutex enclosing type...");

    CopyableHolder src{42, {}};

    // 拷贝：值成员被复制；锁成员为一把独立的新锁。
    CopyableHolder copy = src;
    VERIFY(copy.x == 42);
    VERIFY(copy.m.try_lock());
    copy.m.unlock();
    // 源的锁与副本的锁相互独立。
    src.m.lock();
    VERIFY(copy.m.try_lock());
    copy.m.unlock();
    src.m.unlock();

    // 移动：仅可移动的外层类型也应能移动。
    MoveOnlyHolder mo{std::make_unique<int>(7), {}};
    MoveOnlyHolder moved = std::move(mo);
    VERIFY(moved.p && *moved.p == 7);
    VERIFY(moved.m.try_lock());
    moved.m.unlock();

    dump_info("Done\n");
}

void test_copyable_mutex()
{
    test_copyable_mutex_traits();
    test_copyable_mutex_lockable();
    test_copyable_mutex_fresh_on_copy();
    test_copyable_mutex_mutual_exclusion();
    test_copyable_mutex_enclosing_type();
}

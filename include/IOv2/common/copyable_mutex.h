// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file copyable_mutex.h
 * @lang{ZH}
 * 对拷贝/移动“透明”的互斥量包装：使含有它的类型仍可按成员自动合成拷贝/移动构造。
 * @endif
 * @lang{EN}
 * A copy/move-"transparent" mutex wrapper: lets an enclosing type keep its
 * implicitly-defined copy/move constructors instead of having them deleted by the mutex.
 * @endif
 */

#pragma once

#include <IOv2/common/iov2_export.h>

#include <atomic>
#include <memory>
#include <mutex>

#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif

namespace IOv2::detail
{
/**
 * @lang{ZH}
 * `copyable_mutex` 的进程级 fork 记录：`generation` 在每次 `fork()` 的子进程里加一，
 * `refresh` 串行化子进程里对过期互斥量的重建。
 * @endif
 *
 * @lang{EN}
 * The process-wide fork record of `copyable_mutex`: `generation` goes up by one in the
 * child of every `fork()`, and `refresh` serializes rebuilding stale mutexes in the child.
 * @endif
 */
struct fork_state
{
    std::atomic<unsigned> generation{0};
    std::mutex            refresh;
};

#if defined(IOV2_SHARED)
IOV2_API fork_state& copyable_mutex_fork_state() noexcept;   // defined in iov2_objects.cpp
IOV2_API void        copyable_mutex_watch_fork() noexcept;   // defined in iov2_objects.cpp
#else
inline fork_state& copyable_mutex_fork_state() noexcept
{
    static constinit fork_state s;
    return s;
}
#endif

/**
 * @lang{ZH}
 * `pthread_atfork` 的子进程回调：此时子进程里只有调用 `fork()` 的那个线程。只加代数、
 * 重建 `refresh`；各互斥量在下一次加锁时自行重建。
 * @endif
 *
 * @lang{EN}
 * The child handler for `pthread_atfork`, run while the child has only the thread that
 * called `fork()`. It only bumps the generation and rebuilds `refresh`; each mutex rebuilds
 * itself on its next lock.
 * @endif
 */
inline void copyable_mutex_on_fork_child() noexcept
{
    auto& s = copyable_mutex_fork_state();
    s.generation.fetch_add(1, std::memory_order_relaxed);
    std::construct_at(&s.refresh);
}

#if !defined(IOV2_SHARED)
/**
 * @lang{ZH}
 * 登记 `copyable_mutex_on_fork_child`，每个进程一次。没有 `fork()` 的平台上什么也不做。
 * @endif
 *
 * @lang{EN}
 * Registers `copyable_mutex_on_fork_child`, once per process. Does nothing on platforms
 * without `fork()`.
 * @endif
 */
inline void copyable_mutex_watch_fork() noexcept
{
#if defined(__unix__) || defined(__APPLE__)
    [[maybe_unused]] static const int registered =
        ::pthread_atfork(nullptr, nullptr, &copyable_mutex_on_fork_child);
#endif
}
#endif
}

namespace IOv2
{
/**
 * @lang{ZH}
 * 对拷贝与移动“透明”的互斥量。
 *
 * `std::mutex` 既不可拷贝也不可移动，任何把它作为成员的类型都会因此丢失编译器
 * 合成的拷贝/移动构造函数。本包装把这一层挡掉：它自身可拷贝、可移动，但**不搬运
 * 锁状态**——拷贝或移动只是持有一把全新的、未加锁的底层互斥量。
 *
 * 于是含有它的流类型，其可拷贝/可移动性重新由**其余成员**（如底层 iochannel、
 * cvt、device）决定，无需手写任何拷贝/移动逻辑。
 *
 * 语义上这也是正确的：互斥量保护的是“对象自身的临界区”，而非需要被复制的值；
 * 一个副本是独立的对象，理应拥有自己独立的锁。
 *
 * 底层互斥量的**形态**由模板参数 `TMutex` 决定：默认 `std::mutex`（普通形态）；
 * 传 `std::recursive_mutex` 即得到可重入形态——例如 sentry 在已被 `sync` 持有的
 * `io_mutex()` 上再次加锁时必须用递归形态，否则同一线程自锁死锁。只要 `TMutex`
 * 满足 `BasicLockable`/`Lockable`，本包装即照样满足，可直接用于 `std::lock_guard`、
 * `std::unique_lock`、`std::scoped_lock` 等。`TMutex` 提供 `lock_shared()` 一族时（如
 * `std::shared_mutex`）也照样转发，可用于 `std::shared_lock`。
 *
 * **跨 `fork()` 可用**（POSIX）：`fork()` 只把调用它的线程带进子进程，别的线程持有的锁在
 * 子进程里永远不会释放——裸用 `std::mutex` 时，子进程一加锁就永久阻塞，例如多线程程序
 * fork 后 `exec` 失败、往 `cerr` 报错。本类在子进程里第一次加锁时把这样的锁原地重建成未加锁
 * 状态，此后照常使用。平时的代价是每次加锁多两次读；不 fork 的进程里重建从不发生，没有
 * `fork()` 的平台上不登记任何东西。重建只救回锁，不修复它保护的数据：持锁线程若正改到一半，
 * 子进程看到的就是半截状态（glibc 在 fork 时重置 `FILE` 的锁，取舍相同）。
 *
 * @warning 拷贝/移动时**不会**转移锁的持有状态。请勿在锁被持有期间拷贝或移动
 *          外层对象——那本身就是对该对象的数据竞争。
 *
 * @warning **不支持持有本锁期间由同一线程调用 `fork()`**（例如在 `IOv2::sync` 圈住的一段里，
 *          或在用户的 `io_traits::swrite` 里 fork）：子进程里这个线程随后对本锁的解锁是未定义
 *          行为——与 `std::mutex` 跨 fork 相同（glibc 上子进程已不是持有者，再加锁会永久阻塞）。
 *          本类在这里只保证再加锁不阻塞；glibc + libstdc++ 上多出的解锁被忽略，libc++ 开着断言
 *          时可能 abort。
 * @endif
 *
 * @lang{EN}
 * A mutex that is "transparent" to copy and move.
 *
 * `std::mutex` is neither copyable nor movable, so any type holding one as a member
 * loses its compiler-generated copy/move constructors. This wrapper absorbs that:
 * it is itself copyable and movable, but it does **not** carry lock state -- a copy
 * or move simply holds a fresh, unlocked underlying mutex.
 *
 * As a result, an enclosing stream type's copyability/movability is decided again by
 * its **other** members (e.g. the underlying iochannel / cvt / device), with no
 * hand-written copy/move logic required.
 *
 * This is also semantically correct: the mutex protects "the object's own critical
 * section", not a value that needs copying; a copy is a distinct object and should
 * own a distinct lock.
 *
 * The **flavor** of the underlying mutex is selected by the template parameter
 * `TMutex`: it defaults to `std::mutex` (the plain flavor); passing
 * `std::recursive_mutex` yields a reentrant flavor -- required, for instance, when a
 * sentry re-locks an `io_mutex()` that a `sync` already holds, since a non-recursive
 * mutex would self-deadlock the same thread. As long as `TMutex` models
 * `BasicLockable`/`Lockable`, so does this wrapper, so it drops directly into
 * `std::lock_guard`, `std::unique_lock`, `std::scoped_lock`, etc. When `TMutex` has the
 * `lock_shared()` family (`std::shared_mutex`, say), those are forwarded too, so it works
 * with `std::shared_lock`.
 *
 * **Usable across `fork()`** (POSIX): `fork()` takes only the calling thread into the child,
 * so a lock another thread held is never released there -- with a bare `std::mutex` the
 * child blocks forever on its first lock, say when a multithreaded program forks, `exec`
 * fails, and the child reports it on `cerr`. This class rebuilds such a lock, unlocked, in
 * place on the child's first lock, and it is used as usual from then on. The everyday cost
 * is two extra loads per lock; in a process that never forks no rebuild happens, and on
 * platforms without `fork()` nothing is registered. The rebuild saves the lock only, not
 * the data it guards: if the holder was halfway through a change, the child sees it half
 * done (glibc resets the `FILE` locks at fork, with the same trade-off).
 *
 * @warning Copy/move does **not** transfer lock ownership. Do not copy or move the
 *          enclosing object while the lock is held -- that is itself a data race on
 *          the object.
 *
 * @warning **Calling `fork()` from a thread that holds this lock is not supported** (inside
 *          a stretch held by `IOv2::sync`, say, or in a user's `io_traits::swrite`): that
 *          thread's later unlocks of this lock in the child are undefined behavior -- the same
 *          as `std::mutex` across fork (on glibc the child is no longer the holder, and a
 *          relock blocks forever). This class only guarantees that relocking does not block;
 *          on glibc + libstdc++ the extra unlocks are ignored, and libc++ with assertions on
 *          may abort.
 * @endif
 */
template <typename TMutex = std::mutex>
class copyable_mutex
{
public:
    copyable_mutex() noexcept
        : m_generation(detail::copyable_mutex_fork_state().generation.load(std::memory_order_relaxed))
    {
        detail::copyable_mutex_watch_fork();
    }

    // Copy/move construct a fresh, unlocked mutex; no lock state is carried over.
    copyable_mutex(const copyable_mutex&) noexcept : copyable_mutex() {}
    copyable_mutex& operator=(const copyable_mutex&) noexcept { return *this; }
    copyable_mutex(copyable_mutex&&) noexcept : copyable_mutex() {}
    copyable_mutex& operator=(copyable_mutex&&) noexcept { return *this; }

    ~copyable_mutex() = default;

    void lock()     { refresh_after_fork(); m_mutex.lock(); }
    void unlock()   { m_mutex.unlock(); }
    bool try_lock() { refresh_after_fork(); return m_mutex.try_lock(); }

    void lock_shared() requires requires (TMutex& m) { m.lock_shared(); }
    {
        refresh_after_fork();
        m_mutex.lock_shared();
    }
    void unlock_shared() requires requires (TMutex& m) { m.unlock_shared(); }
    {
        m_mutex.unlock_shared();
    }
    bool try_lock_shared() requires requires (TMutex& m) { m.try_lock_shared(); }
    {
        refresh_after_fork();
        return m_mutex.try_lock_shared();
    }

private:
    // In the child of a fork() the mutex may be held by a thread that did not come along;
    // the first lock there rebuilds it unlocked. Never taken in a process that has not forked.
    void refresh_after_fork()
    {
        const unsigned gen = detail::copyable_mutex_fork_state().generation.load(std::memory_order_relaxed);
        if (m_generation.load(std::memory_order_acquire) != gen) [[unlikely]]
            rebuild(gen);
    }

    void rebuild(unsigned gen)
    {
        std::lock_guard guard(detail::copyable_mutex_fork_state().refresh);
        if (m_generation.load(std::memory_order_relaxed) != gen)
        {
            std::construct_at(&m_mutex);
            m_generation.store(gen, std::memory_order_release);
        }
    }

    TMutex                m_mutex;
    std::atomic<unsigned> m_generation;
};
}

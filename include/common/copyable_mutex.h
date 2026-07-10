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

#include <mutex>

namespace IOv2
{
/**
 * @lang{ZH}
 * 对拷贝与移动“透明”的互斥量。
 *
 * `std::mutex` 既不可拷贝也不可移动，任何把它作为成员的类型都会因此丢失编译器
 * 合成的拷贝/移动构造函数。本包装把这一层挡掉：它自身可拷贝、可移动，但**不搬运
 * 锁状态**——拷贝或移动只是持有一把全新的、未加锁的 `std::mutex`。
 *
 * 于是含有它的流类型，其可拷贝/可移动性重新由**其余成员**（如底层 streambuf、
 * cvt、device）决定，无需手写任何拷贝/移动逻辑。
 *
 * 语义上这也是正确的：互斥量保护的是“对象自身的临界区”，而非需要被复制的值；
 * 一个副本是独立的对象，理应拥有自己独立的锁。
 *
 * 满足 `BasicLockable` 与 `Lockable`，可直接用于 `std::lock_guard`、`std::scoped_lock` 等。
 *
 * @warning 拷贝/移动时**不会**转移锁的持有状态。请勿在锁被持有期间拷贝或移动
 *          外层对象——那本身就是对该对象的数据竞争。
 * @endif
 *
 * @lang{EN}
 * A mutex that is "transparent" to copy and move.
 *
 * `std::mutex` is neither copyable nor movable, so any type holding one as a member
 * loses its compiler-generated copy/move constructors. This wrapper absorbs that:
 * it is itself copyable and movable, but it does **not** carry lock state -- a copy
 * or move simply holds a fresh, unlocked `std::mutex`.
 *
 * As a result, an enclosing stream type's copyability/movability is decided again by
 * its **other** members (e.g. the underlying streambuf / cvt / device), with no
 * hand-written copy/move logic required.
 *
 * This is also semantically correct: the mutex protects "the object's own critical
 * section", not a value that needs copying; a copy is a distinct object and should
 * own a distinct lock.
 *
 * Models `BasicLockable` and `Lockable`, so it drops directly into `std::lock_guard`,
 * `std::scoped_lock`, etc.
 *
 * @warning Copy/move does **not** transfer lock ownership. Do not copy or move the
 *          enclosing object while the lock is held -- that is itself a data race on
 *          the object.
 * @endif
 */
class copyable_mutex
{
public:
    copyable_mutex() noexcept = default;

    // Copy/move construct a fresh, unlocked mutex; no lock state is carried over.
    copyable_mutex(const copyable_mutex&) noexcept {}
    copyable_mutex& operator=(const copyable_mutex&) noexcept { return *this; }
    copyable_mutex(copyable_mutex&&) noexcept {}
    copyable_mutex& operator=(copyable_mutex&&) noexcept { return *this; }

    ~copyable_mutex() = default;

    void lock()     { m_mutex.lock(); }
    void unlock()   { m_mutex.unlock(); }
    bool try_lock() { return m_mutex.try_lock(); }

private:
    std::mutex m_mutex;
};
}

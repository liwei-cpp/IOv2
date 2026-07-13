/**
 * @file copyable_atomic.h
 * @lang{ZH}
 * 对拷贝/移动“透明”的原子量包装：使含有它的类型仍可按成员自动合成拷贝/移动构造。
 * @endif
 * @lang{EN}
 * A copy/move-"transparent" atomic wrapper: lets an enclosing type keep its
 * implicitly-defined copy/move constructors instead of having them deleted by the atomic.
 * @endif
 */

#pragma once

#include <atomic>

namespace IOv2
{
/**
 * @lang{ZH}
 * 对拷贝与移动“透明”的原子量。
 *
 * `std::atomic<T>` 既不可拷贝也不可移动，任何把它作为成员的类型都会因此丢失编译器
 * 合成的拷贝/移动构造函数。本包装把这一层挡掉：它自身可拷贝、可移动，拷贝/移动时以一次
 * 原子读取搬运**当前值**（而非搬运任何“进行中”的同步状态）。
 *
 * 于是含有它的类型，其可拷贝/可移动性重新由**其余成员**决定，无需手写任何拷贝/移动逻辑。
 *
 * 只暴露 `load`/`store`/`exchange` 三个最常用的原子操作，均为 `noexcept` 且无锁（对通常的
 * 无锁类型而言），因此可安全地在析构等不应抛出的场景中调用。
 *
 * @warning 拷贝/移动只保证对**本原子量自身**无数据竞争；若在拷贝的同时其它线程正在修改
 *          外层对象的其它非原子成员，那仍是对该对象的数据竞争。请勿在对象被并发使用期间
 *          拷贝或移动它。
 * @endif
 *
 * @lang{EN}
 * An atomic that is "transparent" to copy and move.
 *
 * `std::atomic<T>` is neither copyable nor movable, so any type holding one as a member
 * loses its compiler-generated copy/move constructors. This wrapper absorbs that: it is
 * itself copyable and movable, and a copy/move carries over the **current value** via a
 * single atomic read (rather than any "in-progress" synchronization state).
 *
 * As a result, an enclosing type's copyability/movability is decided again by its **other**
 * members, with no hand-written copy/move logic required.
 *
 * It exposes only the three most common atomic operations `load`/`store`/`exchange`, all
 * `noexcept` and lock-free (for the usual lock-free types), so they may be called safely
 * from contexts that must not throw, such as destructors.
 *
 * @warning Copy/move is data-race-free only with respect to **this atomic itself**; if
 *          another thread is concurrently mutating other, non-atomic members of the
 *          enclosing object while it is copied, that remains a data race on the object. Do
 *          not copy or move an object while it is concurrently in use.
 * @endif
 */
template <typename T>
class copyable_atomic
{
public:
    copyable_atomic() noexcept = default;
    copyable_atomic(T value) noexcept : m_value(value) {}

    copyable_atomic(const copyable_atomic& other) noexcept
        : m_value(other.m_value.load()) {}
    copyable_atomic& operator=(const copyable_atomic& other) noexcept
    {
        m_value.store(other.m_value.load());
        return *this;
    }
    copyable_atomic(copyable_atomic&& other) noexcept
        : m_value(other.m_value.load()) {}
    copyable_atomic& operator=(copyable_atomic&& other) noexcept
    {
        m_value.store(other.m_value.load());
        return *this;
    }

    ~copyable_atomic() = default;

    T    load() const noexcept  { return m_value.load(); }
    void store(T value) noexcept { m_value.store(value); }
    T    exchange(T value) noexcept { return m_value.exchange(value); }

private:
    std::atomic<T> m_value{};
};
}

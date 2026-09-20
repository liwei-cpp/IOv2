// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file iochannel_iterator.h
 * @lang{ZH}
 * 定义了在通道（iochannel）之上的迭代器适配器：
 * - `ichannel_iterator`：单遍输入迭代器，从通道逐字符读取。
 * - `ochannel_iterator`：输出迭代器，向通道逐字符写入。
 *
 * 二者对应标准库的 `std::istreambuf_iterator` / `std::ostreambuf_iterator`，使得算法与
 * 范围可以直接作用于本库的通道。
 * @endif
 *
 * @lang{EN}
 * Defines the iterator adapters over channels (iochannel):
 * - `ichannel_iterator`: a single-pass input iterator that reads a channel
 *   character by character.
 * - `ochannel_iterator`: an output iterator that writes a channel character by
 *   character.
 *
 * They correspond to the standard `std::istreambuf_iterator` / `std::ostreambuf_iterator`,
 * letting algorithms and ranges operate directly on this library's channels.
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/common/iochannel_defs.h>
#include <IOv2/io/iochannel.h>

#include <cstddef>
#include <iterator>
#include <optional>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 输入通道迭代器（单遍输入迭代器）。
 *
 * @warning 独占访问契约：在一个 ichannel_iterator 处于活动（尚未走到末尾、仍可能被解引用
 *          或自增）期间，**不得**通过任何其它途径操作其绑定的 iochannel。这包括：另建一个
 *          作用于同一 iochannel 的迭代器、直接调用 `getc()`/`bumpc()`/`putbackc()`、以及
 *          ——当底层为双向 iochannel 时——通过 `putc()`/`putn()` 写入。迭代器自身携带前瞻
 *          缓存，任何交错的流操作都会破坏其不变式，导致读取跳过/重复/返回陈旧数据等未指定
 *          行为。此要求与标准 `std::istreambuf_iterator` 一致。
 * @tparam TChannel 通道类型，须为 `iochannel` 或 `ichannel`（即可读）。
 * @endif
 *
 * @lang{EN}
 * @brief Input channel iterator (single-pass input iterator).
 *
 * @warning Exclusive-access contract: while an ichannel_iterator is active (not yet at end,
 *          still potentially dereferenced or incremented), the iochannel it is bound to MUST NOT
 *          be operated on through any other path. This includes: constructing another iterator
 *          over the same iochannel, calling `getc()`/`bumpc()`/`putbackc()` directly, and --
 *          when the underlying iochannel is bidirectional -- writing through
 *          `putc()`/`putn()`. The iterator carries look-ahead state; any interleaved stream
 *          operation breaks its invariant and yields unspecified behavior such as skipped,
 *          repeated, or stale characters. This matches the requirement of the standard
 *          `std::istreambuf_iterator`.
 * @tparam TChannel The channel type; must be an `iochannel` or `ichannel`
 *         (i.e. readable).
 * @endif
 */
template <typename TChannel>
    requires (is_iochannel<TChannel> || is_ichannel<TChannel>)
class ichannel_iterator
{
public:
    using value_type = typename TChannel::char_type;        ///< @lang{ZH} 迭代的字符类型。 @endif @lang{EN} The character type iterated over. @endif
    using difference_type = std::ptrdiff_t;                 ///< @lang{ZH} 迭代器差值类型。 @endif @lang{EN} The iterator difference type. @endif

    /**
     * @lang{ZH}
     * @brief `operator->` 的返回代理，持有一份字符值。
     *
     * 因单遍输入迭代器无法返回指向持久对象的指针，故以代理对象承载解引用结果。
     * @endif
     *
     * @lang{EN}
     * @brief Return proxy for `operator->`, holding a copy of the character value.
     *
     * Since a single-pass input iterator cannot return a pointer to a persistent object,
     * the dereference result is carried by a proxy object.
     * @endif
     */
    struct proxy
    {
        value_type m_value;     ///< @lang{ZH} 持有的字符值。 @endif @lang{EN} The held character value. @endif
        const value_type* operator->() const { return &m_value; }
        value_type operator*() const { return m_value; }
    };

public:
    /**
     * @lang{ZH}
     * @brief 默认构造，得到一个末尾（end）迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Default constructor, producing an end iterator.
     * @endif
     */
    constexpr ichannel_iterator()
        : m_channel(nullptr) {}

    /**
     * @lang{ZH}
     * @brief 由 `std::default_sentinel_t` 构造末尾迭代器。
     *
     * 使本迭代器可与 `std::default_sentinel` 比较，便于用作范围的哨兵终点。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs an end iterator from `std::default_sentinel_t`.
     *
     * Lets this iterator be compared against `std::default_sentinel`, convenient for use
     * as a range's sentinel end.
     * @endif
     */
    constexpr ichannel_iterator(std::default_sentinel_t) noexcept
      : ichannel_iterator() {}

    /**
     * @lang{ZH}
     * @brief 绑定到一个通道，构造一个可读的起始迭代器；可选地附加一个“已观察到输入
     *        结束”的报告位。
     *
     * 若提供了 `saw_eof`，迭代器在**任何**观察到输入结束的时刻（`operator*` 取不到字符、
     * 或判等时 `is_eof()` 为真）把它置为 true。所有副本共享同一个报告位，因此该信息既能
     * 穿过按值传参（如 `io_traits::sread`），也能在读取过程抛出异常时存活——它位于调用方的
     * 栈帧上，不随迭代器副本销毁。
     *
     * @param p_channel 要绑定的通道。
     * @param saw_eof 可选的报告位；`nullptr` 表示不上报。
     * @warning 迭代器持有 `saw_eof` 的地址。**不得**让迭代器逃出 `saw_eof` 的生存期。
     * @endif
     *
     * @lang{EN}
     * @brief Binds to a channel, constructing a readable begin iterator; optionally
     *        attaches an "observed end of input" report flag.
     *
     * When `saw_eof` is supplied, the iterator sets it to true at **any** point where it
     * observes end of input (`operator*` cannot fetch a character, or `is_eof()` is true
     * during comparison). All copies share the same flag, so the information survives both
     * by-value passing (e.g. `io_traits::sread`) and an exception thrown mid-read -- it lives
     * in the caller's frame, not in any iterator copy.
     *
     * @param p_channel The channel to bind to.
     * @param saw_eof Optional report flag; `nullptr` means do not report.
     * @warning The iterator holds the address of `saw_eof`. Do **not** let the iterator
     *          outlive `saw_eof`.
     * @endif
     */
    ichannel_iterator(TChannel& p_channel, bool* saw_eof = nullptr)
        : m_channel(&p_channel), m_saw_eof(saw_eof) {}

public:
    /**
     * @lang{ZH}
     * @brief 解引用，返回当前字符（不前进）。
     *
     * 优先返回迭代器自身缓存的字符（`m_c`）；若无缓存则经 `getc()` 预读当前字符。
     * 若已到达末尾，则将迭代器标记为末尾并抛出 eof_error。
     * @return 当前字符。
     * @throw eof_error 当已到达末尾、无字符可返回时。
     * @endif
     *
     * @lang{EN}
     * @brief Dereferences, returning the current character (without advancing).
     *
     * Prefers the iterator's own cached character (`m_c`); if there is no cache it peeks
     * the current character via `getc()`. If the end has been reached, the iterator is
     * marked as an end iterator and eof_error is thrown.
     * @return The current character.
     * @throw eof_error When the end has been reached and no character can be returned.
     * @endif
     */
    value_type operator*() const
    {
        auto res = m_c;
        if (m_channel && (!res.has_value()))
        {
            res = m_channel->getc();
            if (!res.has_value())
            {
                m_channel = nullptr;
                if (m_saw_eof) *m_saw_eof = true;
            }
        }
        if (!res.has_value()) throw eof_error{};
        return res.value();
    }

    /**
     * @lang{ZH}
     * @brief 通过代理访问当前字符的成员。
     * @return 承载当前字符的 proxy 对象。
     * @throw eof_error 当已到达末尾时（经由 operator*()）。
     * @endif
     *
     * @lang{EN}
     * @brief Accesses members of the current character through a proxy.
     * @return A proxy object carrying the current character.
     * @throw eof_error When the end has been reached (via operator*()).
     * @endif
     */
    proxy operator->() const
    {
        return proxy{ operator*() };
    }

    /**
     * @lang{ZH}
     * @brief 前置自增：前进到下一个字符。
     *
     * 若当前没有缓存字符，则通过 `bumpc()` 消费底层流的当前字符；随后清空缓存。
     * @return 自增后的自身引用。
     * @endif
     *
     * @lang{EN}
     * @brief Pre-increment: advances to the next character.
     *
     * If there is no cached character, consumes the underlying stream's current character
     * via `bumpc()`; then clears the cache.
     * @return A reference to *this after incrementing.
     * @endif
     */
    ichannel_iterator& operator++()
    {
        if (m_channel && !m_c.has_value())
            m_channel->bumpc();
        m_c = std::optional<value_type>{};
        return *this;
    }

    /**
     * @lang{ZH}
     * @brief 后置自增：前进到下一个字符，返回自增前的迭代器。
     *
     * 返回的旧迭代器缓存了自增前的当前字符，因而其仍可被安全解引用一次。
     * @return 自增前的迭代器副本。
     * @endif
     *
     * @lang{EN}
     * @brief Post-increment: advances to the next character, returning the pre-increment
     * iterator.
     *
     * The returned old iterator caches the current character from before the increment, so
     * it can still be safely dereferenced once.
     * @return A copy of the iterator before incrementing.
     * @endif
     */
    ichannel_iterator operator++(int)
    {
        ichannel_iterator old = *this;
        if (m_channel && !m_c.has_value())
            old.m_c = m_channel->bumpc();
        m_c = std::optional<value_type>{};
        return old;
    }

    /**
     * @lang{ZH}
     * @brief 将字符压回底层通道。
     *
     * 若迭代器自身缓存了一个字符（`m_c`），会先把该缓存字符压回，再压回 `ch`，从而
     * 保持字符顺序。
     * @param ch 要压回的字符。
     * @throw cvt_error 当迭代器未绑定通道（末尾迭代器）时。
     * @endif
     *
     * @lang{EN}
     * @brief Pushes a character back into the underlying channel.
     *
     * If the iterator holds a cached character (`m_c`), that cached character is pushed
     * back first and then `ch`, preserving character order.
     * @param ch The character to push back.
     * @throw cvt_error When the iterator is not bound to a channel (an end iterator).
     * @endif
     */
    void putbackc(value_type ch)
    {
        if (!m_channel)
            throw cvt_error("put back fails");

        if (m_c.has_value())
        {
            m_channel->putbackc(m_c.value());
            m_c = std::optional<value_type>{};
        }
        m_channel->putbackc(ch);
    }

    /**
     * @lang{ZH}
     * @brief 比较两个迭代器是否相等。
     *
     * 当且仅当两者的“是否处于末尾”状态相同时视为相等。因此任意两个末尾迭代器相等，
     * 任意两个非末尾迭代器相等；这也使得起始迭代器可与末尾（哨兵）迭代器比较以检测末尾。
     * @param val1 左操作数。
     * @param val2 右操作数。
     * @return 两者末尾状态相同则为 true。
     * @endif
     *
     * @lang{EN}
     * @brief Compares two iterators for equality.
     *
     * Two iterators are equal iff their "at end" status is the same. Thus any two end
     * iterators are equal, and any two non-end iterators are equal; this also lets a begin
     * iterator be compared against an end (sentinel) iterator to detect the end.
     * @param val1 The left operand.
     * @param val2 The right operand.
     * @return true if both have the same end status.
     * @endif
     */
    friend bool operator==(const ichannel_iterator& val1, const ichannel_iterator& val2)
    {
        return at_end(val1) == at_end(val2);
    }

private:
    /**
     * @lang{ZH}
     * @brief 判断迭代器是否已到达末尾。
     *
     * 若有缓存字符则未到末尾；若未绑定通道则已到末尾；否则查询底层流的 is_eof()。
     * @param it 待判断的迭代器。
     * @return 已到达末尾则为 true。
     * @endif
     *
     * @lang{EN}
     * @brief Determines whether the iterator has reached the end.
     *
     * If it has a cached character it is not at the end; if it is not bound to a stream
     * buffer it is at the end; otherwise it queries the underlying stream's is_eof().
     * @param it The iterator to test.
     * @return true if the end has been reached.
     * @endif
     */
    static bool at_end(const ichannel_iterator& it)
    {
        if (it.m_c.has_value()) return false;
        if (it.m_channel == nullptr) return true;
        if (it.m_channel->is_eof())
        {
            if (it.m_saw_eof) *it.m_saw_eof = true;
            return true;
        }
        return false;
    }

private:
    mutable TChannel* m_channel;        ///< @lang{ZH} 绑定的通道；nullptr 表示末尾迭代器。 @endif @lang{EN} The bound channel; nullptr denotes an end iterator. @endif
    std::optional<value_type> m_c;      ///< @lang{ZH} 前瞻缓存的字符（若有）。 @endif @lang{EN} The look-ahead cached character (if any). @endif
    bool* m_saw_eof = nullptr;          ///< @lang{ZH} 可选的"已观察到输入结束"报告位；由所有副本共享，位于调用方栈帧。 @endif @lang{EN} Optional "observed end of input" report flag; shared by all copies, living in the caller's frame. @endif
};

/**
 * @lang{ZH}
 * @brief 输出通道迭代器（输出迭代器）。
 *
 * @note 与 ichannel_iterator 不同，本迭代器**不持有任何缓存或前瞻状态**，`operator=(c)`
 *       仅将字符转发给 `iochannel::putc()`。因此交错的其它流操作不会破坏本迭代器自身的
 *       不变式；但它们仍会按调用发生的先后顺序改变底层流的写入/读取语义。
 * @tparam TChannel 通道类型，须为 `iochannel` 或 `ochannel`（即可写）。
 * @endif
 *
 * @lang{EN}
 * @brief Output channel iterator (output iterator).
 *
 * @note Unlike ichannel_iterator, this iterator holds NO cache or look-ahead state;
 *       `operator=(c)` merely forwards the character to `iochannel::putc()`. Interleaved stream
 *       operations therefore cannot break this iterator's own invariant, but they still alter the
 *       underlying stream's write/read semantics in call order.
 * @tparam TChannel The channel type; must be an `iochannel` or `ochannel`
 *         (i.e. writable).
 * @endif
 */
template <typename TChannel>
    requires (is_iochannel<TChannel> || is_ochannel<TChannel>)
class ochannel_iterator
{
public:
    using value_type = typename TChannel::char_type;        ///< @lang{ZH} 写入的字符类型。 @endif @lang{EN} The character type written. @endif
    using difference_type = std::ptrdiff_t;                 ///< @lang{ZH} 迭代器差值类型。 @endif @lang{EN} The iterator difference type. @endif

public:
    /**
     * @lang{ZH}
     * @brief 绑定到一个通道，构造输出迭代器。
     * @param p_channel 要写入的通道。
     * @endif
     *
     * @lang{EN}
     * @brief Binds to a channel, constructing an output iterator.
     * @param p_channel The channel to write to.
     * @endif
     */
    ochannel_iterator(TChannel& p_channel)
        : m_channel(&p_channel) {}

public:
    /**
     * @lang{ZH} @brief 解引用（空操作）：返回自身，以配合 `*it = c` 的输出迭代器惯用法。 @endif
     * @lang{EN} @brief Dereference (no-op): returns *this to support the `*it = c` output-iterator idiom. @endif
     */
    ochannel_iterator& operator*()
    { return *this; }

    /**
     * @lang{ZH} @brief 后置自增（空操作）：返回自身。 @endif
     * @lang{EN} @brief Post-increment (no-op): returns *this. @endif
     */
    ochannel_iterator& operator++(int)
    { return *this; }

    /**
     * @lang{ZH} @brief 前置自增（空操作）：返回自身。 @endif
     * @lang{EN} @brief Pre-increment (no-op): returns *this. @endif
     */
    ochannel_iterator& operator++()
    { return *this; }

    /**
     * @lang{ZH}
     * @brief 写入一个字符：转发给底层通道的 putc()。
     * @param c 要写入的字符。
     * @return 自身引用。
     * @endif
     *
     * @lang{EN}
     * @brief Writes a character: forwards to the underlying channel's putc().
     * @param c The character to write.
     * @return A reference to *this.
     * @endif
     */
    ochannel_iterator& operator=(value_type c)
    {
        m_channel->putc(c);
        return *this;
    }

private:
    TChannel* m_channel;        ///< @lang{ZH} 绑定的通道。 @endif @lang{EN} The bound channel. @endif
};
}

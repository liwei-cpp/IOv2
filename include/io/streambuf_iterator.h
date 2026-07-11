/**
 * @file streambuf_iterator.h
 * @lang{ZH}
 * 定义了在流缓冲区（streambuf）之上的迭代器适配器：
 * - `istreambuf_iterator`：单遍输入迭代器，从流缓冲区逐字符读取。
 * - `ostreambuf_iterator`：输出迭代器，向流缓冲区逐字符写入。
 *
 * 二者对应标准库的 `std::istreambuf_iterator` / `std::ostreambuf_iterator`，使得算法与
 * 范围可以直接作用于本库的流缓冲区。
 * @endif
 *
 * @lang{EN}
 * Defines the iterator adapters over stream buffers (streambuf):
 * - `istreambuf_iterator`: a single-pass input iterator that reads a stream buffer
 *   character by character.
 * - `ostreambuf_iterator`: an output iterator that writes a stream buffer character by
 *   character.
 *
 * They correspond to the standard `std::istreambuf_iterator` / `std::ostreambuf_iterator`,
 * letting algorithms and ranges operate directly on this library's stream buffers.
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <common/streambuf_defs.h>
#include <io/streambuf.h>

#include <cstddef>
#include <iterator>
#include <optional>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 输入流缓冲区迭代器（单遍输入迭代器）。
 *
 * 输入流缓冲区迭代器（单遍输入迭代器）。
 *
 * @warning 独占访问契约：在一个 istreambuf_iterator 处于活动（尚未走到末尾、仍可能
 * 被解引用或自增）期间，**不得**通过任何其它途径操作其绑定的 streambuf。这包括：另建
 * 一个作用于同一 streambuf 的迭代器、直接调用 sgetc()/sbumpc()/sputbackc()、以及——当
 * 底层为双向 streambuf 时——通过 sputc()/sputn() 写入。迭代器自身携带前瞻缓存
 * （m_c 以及底层读缓冲区中被 sgetc() 预读的字符），任何交错的流操作都会破坏其不变式，
 * 导致读取跳过/重复/返回陈旧数据等未指定行为。此要求与标准 std::istreambuf_iterator
 * 一致。
 * @note 上述契约的一个推论：在迭代器正确使用（流始终处于输入方向）时，
 * operator== 内部对 is_eof() 的调用不会引发方向切换，因而没有副作用；反之若违反契约在
 * 写入途中比较迭代器，才可能触发 is_eof() → switch_to_get() 的方向翻转，这属于契约被
 * 破坏后的后果，而非本类的保证。
 * @tparam TStreamBuf 流缓冲区类型，须为 `streambuf` 或 `istreambuf`（即可读）。
 * @endif
 *
 * @lang{EN}
 * @brief Input stream-buffer iterator (single-pass input iterator).
 *
 * Input stream-buffer iterator (single-pass input iterator).
 *
 * @warning Exclusive-access contract: while an istreambuf_iterator is active (not yet at
 * end, still potentially dereferenced or incremented), the streambuf it is bound to
 * MUST NOT be operated on through any other path. This includes: constructing another
 * iterator over the same streambuf, calling sgetc()/sbumpc()/sputbackc() directly, and —
 * when the underlying streambuf is bidirectional — writing through sputc()/sputn(). The
 * iterator carries look-ahead state (m_c plus the character pre-read into the streambuf's
 * read buffer by sgetc()); any interleaved stream operation breaks its invariant and
 * yields unspecified behavior such as skipped, repeated, or stale characters. This
 * matches the requirement of the standard std::istreambuf_iterator.
 * @note A corollary: under correct use (the stream is always in the input direction),
 * the is_eof() call inside operator== performs no direction switch and is therefore
 * side-effect free; only when the contract is violated by comparing iterators mid-write
 * can is_eof() trigger a switch_to_get() direction flip — a consequence of the broken
 * contract, not a guarantee of this class.
 * @tparam TStreamBuf The stream-buffer type; must be a `streambuf` or `istreambuf`
 * (i.e. readable).
 * @endif
 */
template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_istreambuf<TStreamBuf>)
class istreambuf_iterator
{
public:
    using value_type = typename TStreamBuf::char_type;      ///< @lang{ZH} 迭代的字符类型。 @endif @lang{EN} The character type iterated over. @endif
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
    constexpr istreambuf_iterator()
        : m_streambuf(nullptr) {}

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
    constexpr istreambuf_iterator(std::default_sentinel_t) noexcept
      : istreambuf_iterator() {}

    /**
     * @lang{ZH}
     * @brief 绑定到一个流缓冲区，构造一个可读的起始迭代器。
     * @param p_streambuf 要绑定的流缓冲区。
     * @endif
     *
     * @lang{EN}
     * @brief Binds to a stream buffer, constructing a readable begin iterator.
     * @param p_streambuf The stream buffer to bind to.
     * @endif
     */
    istreambuf_iterator(TStreamBuf& p_streambuf)
        : m_streambuf(&p_streambuf) {}

public:
    /**
     * @lang{ZH}
     * @brief 解引用，返回当前字符（不前进）。
     *
     * 优先返回迭代器自身缓存的字符（`m_c`）；若无缓存则经 `sgetc()` 预读当前字符。
     * 若已到达末尾，则将迭代器标记为末尾并抛出 eof_error。
     * @return 当前字符。
     * @throw eof_error 当已到达末尾、无字符可返回时。
     * @endif
     *
     * @lang{EN}
     * @brief Dereferences, returning the current character (without advancing).
     *
     * Prefers the iterator's own cached character (`m_c`); if there is no cache it peeks
     * the current character via `sgetc()`. If the end has been reached, the iterator is
     * marked as an end iterator and eof_error is thrown.
     * @return The current character.
     * @throw eof_error When the end has been reached and no character can be returned.
     * @endif
     */
    value_type operator*() const
    {
        auto res = m_c;
        if (m_streambuf && (!res.has_value()))
        {
            res = m_streambuf->sgetc();
            if (!res.has_value()) m_streambuf = nullptr;
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
     * 若当前没有缓存字符，则通过 `sbumpc()` 消费底层流的当前字符；随后清空缓存。
     * @return 自增后的自身引用。
     * @endif
     *
     * @lang{EN}
     * @brief Pre-increment: advances to the next character.
     *
     * If there is no cached character, consumes the underlying stream's current character
     * via `sbumpc()`; then clears the cache.
     * @return A reference to *this after incrementing.
     * @endif
     */
    istreambuf_iterator& operator++()
    {
        if (m_streambuf && !m_c.has_value())
            m_streambuf->sbumpc();
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
    istreambuf_iterator operator++(int)
    {
        istreambuf_iterator old = *this;
        if (m_streambuf && !m_c.has_value())
            old.m_c = m_streambuf->sbumpc();
        m_c = std::optional<value_type>{};
        return old;
    }

    /**
     * @lang{ZH}
     * @brief 将字符压回底层流缓冲区。
     *
     * 若迭代器自身缓存了一个字符（`m_c`），会先把该缓存字符压回，再压回 `ch`，从而
     * 保持字符顺序。
     * @param ch 要压回的字符。
     * @throw cvt_error 当迭代器未绑定流缓冲区（末尾迭代器）时。
     * @endif
     *
     * @lang{EN}
     * @brief Pushes a character back into the underlying stream buffer.
     *
     * If the iterator holds a cached character (`m_c`), that cached character is pushed
     * back first and then `ch`, preserving character order.
     * @param ch The character to push back.
     * @throw cvt_error When the iterator is not bound to a stream buffer (an end iterator).
     * @endif
     */
    void sputbackc(value_type ch)
    {
        if (!m_streambuf)
            throw cvt_error("put back fails");

        if (m_c.has_value())
        {
            m_streambuf->sputbackc(m_c.value());
            m_c = std::optional<value_type>{};
        }
        m_streambuf->sputbackc(ch);
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
    friend bool operator==(const istreambuf_iterator& val1, const istreambuf_iterator& val2)
    {
        return at_end(val1) == at_end(val2);
    }

private:
    /**
     * @lang{ZH}
     * @brief 判断迭代器是否已到达末尾。
     *
     * 若有缓存字符则未到末尾；若未绑定流缓冲区则已到末尾；否则查询底层流的 is_eof()。
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
    static bool at_end(const istreambuf_iterator& it)
    {
        if (it.m_c.has_value()) return false;
        if (it.m_streambuf == nullptr) return true;
        return it.m_streambuf->is_eof();
    }

private:
    mutable TStreamBuf* m_streambuf;    ///< @lang{ZH} 绑定的流缓冲区；nullptr 表示末尾迭代器。 @endif @lang{EN} The bound stream buffer; nullptr denotes an end iterator. @endif
    std::optional<value_type> m_c;      ///< @lang{ZH} 前瞻缓存的字符（若有）。 @endif @lang{EN} The look-ahead cached character (if any). @endif
};

/**
 * @lang{ZH}
 * @brief 输出流缓冲区迭代器（输出迭代器）。
 *
 * 输出流缓冲区迭代器（输出迭代器）。
 *
 * @note 与 istreambuf_iterator 不同，本迭代器**不持有任何缓存或前瞻状态**，
 * operator=(c) 仅将字符转发给 streambuf::sputc()。因此交错的其它流操作不会破坏本迭代器
 * 自身的不变式；但它们仍会按调用发生的先后顺序改变底层流的写入/读取语义（例如穿插的
 * sputc()、或对双向流的读取导致的方向切换），故正确用法下仍应确保写入序列按预期进行。
 * @endif
 *
 * @lang{EN}
 * @brief Output stream-buffer iterator (output iterator).
 *
 * Output stream-buffer iterator (output iterator).
 *
 * @note Unlike istreambuf_iterator, this iterator holds NO cache or look-ahead state;
 * operator=(c) merely forwards the character to streambuf::sputc(). Interleaved stream
 * operations therefore cannot break this iterator's own invariant, but they still alter
 * the underlying stream's write/read semantics in call order (e.g. an interspersed
 * sputc(), or a read on a bidirectional stream causing a direction switch), so under
 * correct use the intended write sequence should still be preserved.
 * @tparam TStreamBuf 流缓冲区类型，须为 `streambuf` 或 `ostreambuf`（即可写）。
 * @endif
 */
template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_ostreambuf<TStreamBuf>)
class ostreambuf_iterator
{
public:
    using value_type = typename TStreamBuf::char_type;      ///< @lang{ZH} 写入的字符类型。 @endif @lang{EN} The character type written. @endif
    using difference_type = std::ptrdiff_t;                 ///< @lang{ZH} 迭代器差值类型。 @endif @lang{EN} The iterator difference type. @endif

public:
    /**
     * @lang{ZH}
     * @brief 绑定到一个流缓冲区，构造输出迭代器。
     * @param p_streambuf 要写入的流缓冲区。
     * @endif
     *
     * @lang{EN}
     * @brief Binds to a stream buffer, constructing an output iterator.
     * @param p_streambuf The stream buffer to write to.
     * @endif
     */
    ostreambuf_iterator(TStreamBuf& p_streambuf)
        : m_streambuf(&p_streambuf) {}

public:
    /**
     * @lang{ZH} @brief 解引用（空操作）：返回自身，以配合 `*it = c` 的输出迭代器惯用法。 @endif
     * @lang{EN} @brief Dereference (no-op): returns *this to support the `*it = c` output-iterator idiom. @endif
     */
    ostreambuf_iterator& operator*()
    { return *this; }

    /**
     * @lang{ZH} @brief 后置自增（空操作）：返回自身。 @endif
     * @lang{EN} @brief Post-increment (no-op): returns *this. @endif
     */
    ostreambuf_iterator& operator++(int)
    { return *this; }

    /**
     * @lang{ZH} @brief 前置自增（空操作）：返回自身。 @endif
     * @lang{EN} @brief Pre-increment (no-op): returns *this. @endif
     */
    ostreambuf_iterator& operator++()
    { return *this; }

    /**
     * @lang{ZH}
     * @brief 写入一个字符：转发给底层流缓冲区的 sputc()。
     * @param c 要写入的字符。
     * @return 自身引用。
     * @endif
     *
     * @lang{EN}
     * @brief Writes a character: forwards to the underlying stream buffer's sputc().
     * @param c The character to write.
     * @return A reference to *this.
     * @endif
     */
    ostreambuf_iterator& operator=(value_type c)
    {
        m_streambuf->sputc(c);
        return *this;
    }

private:
    TStreamBuf* m_streambuf;    ///< @lang{ZH} 绑定的流缓冲区。 @endif @lang{EN} The bound stream buffer. @endif
};
}

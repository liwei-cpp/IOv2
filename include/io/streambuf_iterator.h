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
 * @endif
 *
 * @lang{EN}
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
 * @endif
 */
template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_istreambuf<TStreamBuf>)
class istreambuf_iterator
{
public:
    using value_type = typename TStreamBuf::char_type;
    using difference_type = std::ptrdiff_t;

    struct proxy
    {
        value_type m_value;
        const value_type* operator->() const { return &m_value; }
        value_type operator*() const { return m_value; }
    };

public:
    constexpr istreambuf_iterator()
        : m_streambuf(nullptr) {}

    constexpr istreambuf_iterator(std::default_sentinel_t) noexcept
      : istreambuf_iterator() {}

    istreambuf_iterator(TStreamBuf& p_streambuf)
        : m_streambuf(&p_streambuf) {}

public:
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

    proxy operator->() const
    {
        return proxy{ operator*() };
    }

    istreambuf_iterator& operator++()
    {
        if (m_streambuf && !m_c.has_value())
            m_streambuf->sbumpc();
        m_c = std::optional<value_type>{};
        return *this;
    }

    istreambuf_iterator operator++(int)
    {
        istreambuf_iterator old = *this;
        if (m_streambuf && !m_c.has_value())
            old.m_c = m_streambuf->sbumpc();
        m_c = std::optional<value_type>{};
        return old;
    }

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

    friend bool operator==(const istreambuf_iterator& val1, const istreambuf_iterator& val2)
    {
        return at_end(val1) == at_end(val2);
    }

private:
    static bool at_end(const istreambuf_iterator& it)
    {
        if (it.m_c.has_value()) return false;
        if (it.m_streambuf == nullptr) return true;
        return it.m_streambuf->is_eof();
    }

private:
    mutable TStreamBuf* m_streambuf;
    std::optional<value_type> m_c;
};

/**
 * @lang{ZH}
 * 输出流缓冲区迭代器（输出迭代器）。
 *
 * @note 与 istreambuf_iterator 不同，本迭代器**不持有任何缓存或前瞻状态**，
 * operator=(c) 仅将字符转发给 streambuf::sputc()。因此交错的其它流操作不会破坏本迭代器
 * 自身的不变式；但它们仍会按调用发生的先后顺序改变底层流的写入/读取语义（例如穿插的
 * sputc()、或对双向流的读取导致的方向切换），故正确用法下仍应确保写入序列按预期进行。
 * @endif
 *
 * @lang{EN}
 * Output stream-buffer iterator (output iterator).
 *
 * @note Unlike istreambuf_iterator, this iterator holds NO cache or look-ahead state;
 * operator=(c) merely forwards the character to streambuf::sputc(). Interleaved stream
 * operations therefore cannot break this iterator's own invariant, but they still alter
 * the underlying stream's write/read semantics in call order (e.g. an interspersed
 * sputc(), or a read on a bidirectional stream causing a direction switch), so under
 * correct use the intended write sequence should still be preserved.
 * @endif
 */
template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_ostreambuf<TStreamBuf>)
class ostreambuf_iterator
{
public:
    using value_type = typename TStreamBuf::char_type;
    using difference_type = std::ptrdiff_t;

public:
    ostreambuf_iterator(TStreamBuf& p_streambuf)
        : m_streambuf(&p_streambuf) {}

public:
    ostreambuf_iterator& operator*()
    { return *this; }

    ostreambuf_iterator& operator++(int)
    { return *this; }

    ostreambuf_iterator& operator++()
    { return *this; }

    ostreambuf_iterator& operator=(value_type c)
    {
        m_streambuf->sputc(c);
        return *this;
    }

private:
    TStreamBuf* m_streambuf;
};
}

#pragma once
#include <common/streambuf_defs.h>
#include <cvt/runtime_cvt.h>
#include <io/io_concepts.h>

#include <cstddef>
#include <deque>
#include <exception>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace IOv2
{
template <io_device TDevice, typename TChar, bool IsIn, bool IsOut>
    requires (IsIn || IsOut)
class base_streambuf
{
public:
    using device_type = TDevice;
    using char_type = TChar;

public:
    explicit base_streambuf(TDevice dev) requires (IsOut)
        : m_cvt(no_rb_root_cvt{std::move(dev)})
    {
        init_cvt();
    }

    template <cvt_creator TCreator>
    base_streambuf(TDevice dev, const TCreator& creator) requires (IsOut)
        : m_cvt(creator.create(no_rb_root_cvt{std::move(dev)}))
    {
        init_cvt();
    }

    explicit base_streambuf(TDevice dev, bool has_in_buf = true) requires (IsIn && !IsOut)
        : m_cvt(has_in_buf ? runtime_cvt<TDevice, TChar>(rb_root_cvt{std::move(dev)})
                           : runtime_cvt<TDevice, TChar>(no_rb_root_cvt{std::move(dev)}))
    {
        init_cvt();
    }

    template <cvt_creator TCreator>
    base_streambuf(TDevice dev, const TCreator& creator, bool has_in_buf = true) requires (IsIn && !IsOut)
        : m_cvt(has_in_buf ? runtime_cvt<TDevice, TChar>(creator.create(rb_root_cvt{std::move(dev)}))
                           : runtime_cvt<TDevice, TChar>(creator.create(no_rb_root_cvt{std::move(dev)})))
    {
        init_cvt();
    }

    base_streambuf(const base_streambuf& val) = default;
    base_streambuf(base_streambuf&&) = default;
    base_streambuf& operator=(const base_streambuf&) = default;
    base_streambuf& operator=(base_streambuf&&) = default;

public:
    /// get
    std::optional<char_type> sgetc() requires (IsIn)
    {
        if constexpr (IsOut)
            switch_to_get();

        if (!m_read_buf.empty()) return m_read_buf.front();

        char_type c;
        if (m_cvt.get(&c, 1) == 0) return std::optional<char_type>{};
        m_read_buf.push_front(c);
        return c;
    }

    std::optional<char_type> sbumpc() requires (IsIn)
    {
        if constexpr (IsOut)
            switch_to_get();

        if (!m_read_buf.empty())
        {
            char_type c = m_read_buf.front();
            m_read_buf.pop_front();
            return c;
        }

        char_type c;
        if (m_cvt.get(&c, 1) == 0) return std::optional<char_type>{};
        return c;
    }

    std::optional<char_type> snextc() requires (IsIn)
    {
        if (!sbumpc().has_value())
            return std::optional<char_type>{};
        return sgetc();
    }

    size_t sgetn(char_type* s, size_t n) requires (IsIn)
    {
        if ((s == nullptr) || (n == 0)) return 0;

        if constexpr (IsOut)
            switch_to_get();

        size_t res = 0;
        while (!m_read_buf.empty())
        {
            *s++ = m_read_buf.front();
            m_read_buf.pop_front();
            if (++res == n) break;
        }

        while (res < n)
        {
            size_t c = m_cvt.get(s, n - res);
            if (c == 0) break;
            res += c;
            s += c;
        }
        return res;
    }

    bool is_eof() requires (IsIn)
    {
        if constexpr (IsOut)
            switch_to_get();
        return (this->m_read_buf.empty()) && (m_cvt.is_eof());
    }

    /**
     * @lang{ZH}
     * 将字符压回读缓冲区，使其成为下一次读取操作返回的字符。
     * 回退按调用次数计数，不校验压回的字符是否与原始读取内容一致——调用方可以用任意
     * 字符替换原始内容（例如 `sgetc()` 读到 'b' 之后压回 '?'，之后仍按回退了 1 个字符计数）。
     * 压回次数没有上限，包括超过实际已读取字符数的情形；此时 tell() 报告的逻辑位置会在
     * 流起点处饱和（钳位为 0），不会产生 size_t 下溢的错误巨大值，见 tell()。
     * @endif
     *
     * @lang{EN}
     * Pushes a character back into the read buffer so it becomes the next character
     * returned by a subsequent read. Put-back is counted by call count only; the
     * pushed-back character is not required to match what was actually read, so the
     * caller may substitute arbitrary content (e.g. push back '?' after `sgetc()`
     * returned 'b' — this still counts as one character rewound). There is no upper
     * bound on how many times this may be called, including beyond the number of
     * characters actually read; in that case the logical position reported by tell()
     * saturates at the stream origin (clamped to 0) instead of wrapping around as a
     * huge size_t value — see tell().
     * @endif
     */
    void sputbackc(char_type ch) requires (IsIn)
    {
        if constexpr (IsOut)
            switch_to_get();
        m_read_buf.push_front(ch);
    }

    /// put
    void sputc(char_type ch) requires (IsOut)
    {
        if constexpr (IsIn)
            switch_to_put();
        m_cvt.put(&ch, 1);
    }

    void sputn(const char_type* s, size_t n) requires (IsOut)
    {
        if ((s == nullptr) || (n == 0)) return;

        if constexpr (IsIn)
            switch_to_put();
        m_cvt.put(s, n);
    }

    void flush() requires (IsOut)
    {
        if constexpr (IsIn)
            switch_to_put();
        m_cvt.flush();
    }

    /// positioning
    /**
     * @lang{ZH}
     * 返回当前逻辑读取位置。
     * 若通过 sputbackc() 压回的字符数超过了实际从底层转换器读取的字符数（见 sputbackc()
     * 的按次数计数、不校验内容的语义），本函数返回的位置在流起点处饱和为 0，而不是让
     * 减法在 size_t 上下溢产生一个巨大的错误值。
     * @endif
     *
     * @lang{EN}
     * Returns the current logical read position.
     * If more characters have been pushed back via sputbackc() than have actually
     * been read from the underlying converter (see sputbackc()'s count-based,
     * content-agnostic semantics), the position returned here saturates at the
     * stream origin (0) rather than letting the subtraction wrap around to a huge
     * size_t value.
     * @endif
     */
    size_t tell() const
    {
        if constexpr (IsIn)
        {
            const size_t res1 = m_cvt.tell();
            const size_t res2 = m_read_buf.size();
            if (res1 < res2) return 0;
            return res1 - res2;
        }
        else
            return m_cvt.tell();
    }

    void seek(size_t pos)
    {
        m_cvt.seek(pos);
        if constexpr (IsIn)
            m_read_buf.clear();
    }

    void rseek(size_t pos)
    {
        m_cvt.rseek(pos);
        if constexpr (IsIn)
            m_read_buf.clear();
    }

    /// io switch
    void switch_to_get() requires (IsIn && IsOut)
    {
        m_cvt.switch_to_get();
    }

    /**
     * @lang{ZH}
     * 切换到输出模式。
     * 若读缓冲区（m_read_buf）中还留有已经被 sgetc()/sputbackc() 取出但尚未被
     * sbumpc()/sgetn() 消费的字符，则必须先把底层转换器的物理位置回退到这些字符对应
     * 的逻辑位置，才能保证之后再切回输入模式时仍能读到它们——这一步依赖底层转换器
     * 支持定位（cvt_cpt::support_positioning）。
     * 换言之：一个仅满足 cvt_cpt::support_io_switch（支持读、写、切换方向）而不支持
     * 定位的转换器，只要读缓冲区非空就无法调用本函数；若读缓冲区为空（即从未调用过
     * sgetc()/sputbackc()，只用过 sbumpc()/sgetn()），则切换方向不需要定位支持。
     * 关于过度回退：若 sputbackc() 压回的字符数超过实际已读字符数，回退目标位置
     * （即 tell()）会在起点处钳位为 0（见 tell()/sputbackc()），因此切换后写入将从
     * 位置 0 开始。这与逻辑读游标此刻正位于 0 是一致的；又因为 put-back 并不把字符
     * 真正写回底层，切换到输出后从该位置写入会相应改变逻辑数据流——这是既定语义，
     * 非错误。
     * @endif
     *
     * @lang{EN}
     * Switches to output mode.
     * If the read buffer (m_read_buf) still holds characters that were fetched by
     * sgetc()/sputbackc() but not yet consumed by sbumpc()/sgetn(), the underlying
     * converter's physical position must first be rewound to the logical position
     * those characters represent, so that a later switch back to input mode can
     * still read them — this step requires the underlying converter to support
     * positioning (cvt_cpt::support_positioning).
     * In other words: a converter that only satisfies cvt_cpt::support_io_switch
     * (get + put + direction switching) but not positioning cannot call this
     * function while the read buffer is non-empty; if the read buffer is empty
     * (i.e. only sbumpc()/sgetn() have been used, never sgetc()/sputbackc()),
     * switching direction does not require positioning support.
     * On over-pushback: if sputbackc() has pushed back more characters than were
     * actually read, the rewind target (i.e. tell()) saturates at the stream origin
     * 0 (see tell()/sputbackc()), so writing after the switch begins at position 0.
     * This is consistent with the logical read cursor being at 0 at that moment;
     * and because put-back is never written through to the underlying stream,
     * writing from that position after switching to output changes the logical data
     * stream accordingly — this is by-design behavior, not a bug.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH} 读缓冲区非空，且回退这些字符所需的底层定位操作失败（例如底层转换器
     * 不支持定位）时抛出。 @endif
     * @lang{EN} Thrown when the read buffer is non-empty and the underlying
     * positioning operation needed to rewind those characters fails (e.g. the
     * underlying converter does not support positioning). @endif
     */
    void switch_to_put() requires (IsIn && IsOut)
    {
        if (!m_read_buf.empty())
        {
            try
            {
                const size_t pos = tell();
                m_cvt.seek(pos);
            }
            catch (const cvt_error& e)
            {
                throw cvt_error("base_streambuf::switch_to_put fails: cannot reposition "
                                 + std::to_string(m_read_buf.size())
                                 + " buffered/put-back character(s) before switching to output mode: "
                                 + e.what());
            }
            m_read_buf.clear();
        }
        m_cvt.switch_to_put();
    }

    /// others
    device_type& device()
    {
        return m_cvt.device();
    }

    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        if constexpr (IsIn)
        {
            if (!m_read_buf.empty())
            {
                try
                {
                    // On over-pushback (more put-back than actually read), tell()
                    // saturates at 0, so the device is handed back positioned at the
                    // stream origin. That is consistent with the logical read cursor
                    // being at 0; see tell()/sputbackc() and switch_to_put().
                    const size_t pos = tell();
                    m_cvt.seek(pos);
                }
                catch (...)
                {
                    // Swallowed on purpose: a device that doesn't support positioning
                    // (e.g. a pipe/tty-backed stdin) can never honor this reposition,
                    // and that is an inherent property of the device, not an
                    // actionable failure - the caller could not have repositioned it
                    // either after getting it back. Reporting this error would make
                    // routine detach()/attach() cycles on such devices start throwing
                    // (e.g. sync_with_stdio() right after a formatted read has left
                    // one buffered lookahead character), even though nothing is
                    // actually broken; losing that lookahead character is the
                    // accepted, unavoidable cost for non-positionable devices.
                }
                m_read_buf.clear();
            }
        }
        return m_cvt.detach();
    }

    void attach(device_type&& dev = device_type{})
    {
        if constexpr (IsIn)
            m_read_buf.clear();
        m_cvt.attach(std::move(dev));
        init_cvt();
    }

    void adjust(const cvt_behavior& acc)
    {
        return m_cvt.adjust(acc);
    }

    void retrieve(cvt_status& acc) const
    {
        return m_cvt.retrieve(acc);
    }

private:
    void init_cvt()
    {
        auto res = m_cvt.bos();
        m_cvt.main_cont_beg();
        if constexpr (IsIn && !IsOut)
        {
            if (res != io_status::input)
                m_cvt.switch_to_get();
        }
        else if constexpr (!IsIn && IsOut)
        {
            if (res != io_status::output)
                m_cvt.switch_to_put();
        }
    }

private:
    runtime_cvt<TDevice, TChar> m_cvt;

    [[no_unique_address]]
    std::conditional_t<IsIn, std::deque<char_type>, std::monostate> m_read_buf;
};

template <io_device TDevice, typename TChar>
struct streambuf : public base_streambuf<TDevice, TChar, true, true>
{
    using base_streambuf<TDevice, TChar, true, true>::base_streambuf;
};

template <io_device TDevice, typename TChar>
struct istreambuf : public base_streambuf<TDevice, TChar, true, false>
{
    using base_streambuf<TDevice, TChar, true, false>::base_streambuf;
};

template <io_device TDevice, typename TChar>
struct ostreambuf : public base_streambuf<TDevice, TChar, false, true>
{
    using base_streambuf<TDevice, TChar, false, true>::base_streambuf;
};

template <io_device TDevice>
streambuf(TDevice) -> streambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
streambuf(TDevice, const TCreator&) -> streambuf<TDevice, ext_to_int<TDevice, TCreator>>;

template <io_device TDevice>
istreambuf(TDevice) -> istreambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
istreambuf(TDevice, const TCreator&) -> istreambuf<TDevice, ext_to_int<TDevice, TCreator>>;

template <io_device TDevice>
ostreambuf(TDevice) -> ostreambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
ostreambuf(TDevice, const TCreator&) -> ostreambuf<TDevice, ext_to_int<TDevice, TCreator>>;
}
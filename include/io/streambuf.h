/**
 * @file streambuf.h
 * @lang{ZH}
 * 定义了流缓冲区（stream buffer）体系，是设备、转换器与上层流之间的桥梁。
 *
 * 核心是类模板 `base_streambuf`，它在一条转换器管线（`runtime_cvt`）之上再叠加一个
 * 读缓冲区，向上层提供面向字符的读、写、回退、定位以及读/写方向切换等操作。基于它，
 * 本文件还提供三个便捷别名类：
 * - `streambuf`：同时支持输入与输出。
 * - `istreambuf`：仅支持输入。
 * - `ostreambuf`：仅支持输出。
 *
 * 文件末尾提供了一组类模板实参推导指引（CTAD），使得可以直接由「设备」或「设备 + 转换器
 * 工厂」推导出对应的字符类型（后者借助 io/io_concepts.h 中的 `ext_to_int`）。
 * @endif
 *
 * @lang{EN}
 * Defines the stream-buffer hierarchy, the bridge between devices, converters, and the
 * higher-level streams.
 *
 * At its core is the class template `base_streambuf`, which layers a read buffer on top
 * of a converter pipeline (`runtime_cvt`) and offers the upper layers character-oriented
 * read, write, put-back, positioning, and read/write direction-switching operations.
 * Building on it, this file also provides three convenience alias classes:
 * - `streambuf`: supports both input and output.
 * - `istreambuf`: input only.
 * - `ostreambuf`: output only.
 *
 * At the end of the file, a set of class-template argument deduction guides (CTAD) lets
 * the character type be deduced directly from a "device" or a "device + converter
 * creator" pair (the latter via `ext_to_int` from io/io_concepts.h).
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/device_concepts.h>
#include <io/io_concepts.h>

#include <cassert>
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
/**
 * @lang{ZH}
 * @brief 流缓冲区的通用实现基类。
 *
 * `base_streambuf` 在一条转换器管线（`runtime_cvt<TDevice, TChar>`）之上封装出面向字符
 * 的缓冲接口。它按需持有一个读缓冲区 `m_read_buf`（`std::deque`，仅在 `IsIn` 为真时存在），
 * 用于保存被 `sgetc()` 预读（peek）以及被 `sputbackc()` 压回的字符。
 *
 * 是否具备输入/输出能力由模板参数 `IsIn`/`IsOut` 在编译期决定，相应的成员函数通过
 * `requires` 约束仅在对应能力开启时可用。当二者同时为真时，对象是双向的，输入与输出
 * 操作会按需自动调用 `switch_to_get()`/`switch_to_put()` 切换底层转换器的方向。
 *
 * @note 关于读缓冲区与逻辑位置的语义细节（尤其是过度回退时位置在起点处饱和为 0），
 *       见 sputbackc()、tell() 与 switch_to_put() 的说明。
 *
 * @tparam TDevice 底层设备类型，须满足 `io_device`。
 * @tparam TChar 字符类型（转换器管线暴露的内部数据类型）。
 * @tparam IsIn 是否支持输入操作。
 * @tparam IsOut 是否支持输出操作。
 * @endif
 *
 * @lang{EN}
 * @brief Common implementation base class for stream buffers.
 *
 * `base_streambuf` wraps a converter pipeline (`runtime_cvt<TDevice, TChar>`) into a
 * character-oriented buffered interface. It optionally holds a read buffer `m_read_buf`
 * (a `std::deque`, present only when `IsIn` is true) that stores characters peeked by
 * `sgetc()` and pushed back by `sputbackc()`.
 *
 * Whether input/output is available is decided at compile time by the template
 * parameters `IsIn`/`IsOut`; the corresponding member functions are constrained via
 * `requires` to be available only when the matching capability is enabled. When both are
 * true the object is bidirectional, and input/output operations automatically call
 * `switch_to_get()`/`switch_to_put()` to switch the underlying converter's direction as
 * needed.
 *
 * @note For the detailed semantics of the read buffer and the logical position (in
 *       particular, the position saturating at 0 on over-pushback), see sputbackc(),
 *       tell(), and switch_to_put().
 *
 * @tparam TDevice The underlying device type; must satisfy `io_device`.
 * @tparam TChar The character type (the internal data type exposed by the converter
 *         pipeline).
 * @tparam IsIn Whether input operations are supported.
 * @tparam IsOut Whether output operations are supported.
 * @endif
 */
template <io_device TDevice, typename TChar, bool IsIn, bool IsOut>
    requires (IsIn || IsOut)
class base_streambuf
{
public:
    using device_type = TDevice;
    using char_type = TChar;

public:
    /**
     * @lang{ZH}
     * @brief 从设备构造一个仅输出的流缓冲区（无转换，直连根转换器）。
     * @param dev 底层设备（按值取走所有权）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs an output-only stream buffer from a device (no conversion, wired
     * directly to the root converter).
     * @param dev The underlying device (ownership taken by value).
     * @endif
     */
    explicit base_streambuf(TDevice dev) requires (IsOut)
        : m_cvt(no_rb_root_cvt{std::move(dev)})
    {
        init_cvt();
    }

    /**
     * @lang{ZH}
     * @brief 用转换器工厂在设备之上构建转换管线，构造一个输出流缓冲区。
     * @tparam TCreator 转换器工厂类型，须满足 `cvt_creator`。
     * @param dev 底层设备（按值取走所有权）。
     * @param creator 用于在根转换器之上创建转换管线的工厂。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs an output stream buffer, building a converter pipeline on top of
     * the device via a converter creator.
     * @tparam TCreator The converter creator type; must satisfy `cvt_creator`.
     * @param dev The underlying device (ownership taken by value).
     * @param creator The creator used to build the converter pipeline over the root
     * converter.
     * @endif
     */
    template <cvt_creator TCreator>
    base_streambuf(TDevice dev, const TCreator& creator) requires (IsOut)
        : m_cvt(creator.create(no_rb_root_cvt{std::move(dev)}))
    {
        init_cvt();
    }

    /**
     * @lang{ZH}
     * @brief 从设备构造一个仅输入的流缓冲区（无转换）。
     * @param dev 底层设备（按值取走所有权）。
     * @param has_in_buf 是否为根转换器启用回读缓冲（`rb_root_cvt`）；为 false 时使用
     *        不带回读缓冲的根转换器（`no_rb_root_cvt`）。默认 true。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs an input-only stream buffer from a device (no conversion).
     * @param dev The underlying device (ownership taken by value).
     * @param has_in_buf Whether to enable the read-back buffer on the root converter
     *        (`rb_root_cvt`); when false, the root converter without a read-back buffer
     *        (`no_rb_root_cvt`) is used. Defaults to true.
     * @endif
     */
    explicit base_streambuf(TDevice dev, bool has_in_buf = true) requires (IsIn && !IsOut)
        : m_cvt(has_in_buf ? runtime_cvt<TDevice, TChar>(rb_root_cvt{std::move(dev)})
                           : runtime_cvt<TDevice, TChar>(no_rb_root_cvt{std::move(dev)}))
    {
        init_cvt();
    }

    /**
     * @lang{ZH}
     * @brief 用转换器工厂在设备之上构建转换管线，构造一个输入流缓冲区。
     * @tparam TCreator 转换器工厂类型，须满足 `cvt_creator`。
     * @param dev 底层设备（按值取走所有权）。
     * @param creator 用于在根转换器之上创建转换管线的工厂。
     * @param has_in_buf 是否为根转换器启用回读缓冲，语义同上。默认 true。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs an input stream buffer, building a converter pipeline on top of
     * the device via a converter creator.
     * @tparam TCreator The converter creator type; must satisfy `cvt_creator`.
     * @param dev The underlying device (ownership taken by value).
     * @param creator The creator used to build the converter pipeline over the root
     * converter.
     * @param has_in_buf Whether to enable the read-back buffer on the root converter,
     *        with the same meaning as above. Defaults to true.
     * @endif
     */
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
    /**
     * @lang{ZH}
     * @name 读取操作（仅 IsIn）
     * @{
     * @endif
     * @lang{EN}
     * @name Read operations (IsIn only)
     * @{
     * @endif
     */

    /**
     * @lang{ZH}
     * @brief 预读（peek）当前字符但不消费它。
     *
     * 若读缓冲区非空，返回其队首字符；否则从底层转换器读取一个字符，压入读缓冲区队首后
     * 返回。因此该字符**下一次**仍会被 sgetc()/sbumpc() 读到。
     * @return 当前字符；若已到达末尾则为空的 optional。
     * @note 在双向模式下会先切换到输入方向。
     * @endif
     *
     * @lang{EN}
     * @brief Peeks the current character without consuming it.
     *
     * If the read buffer is non-empty, returns its front character; otherwise reads one
     * character from the underlying converter, pushes it to the front of the read buffer,
     * and returns it. The character therefore remains available to the **next**
     * sgetc()/sbumpc().
     * @return The current character; an empty optional if the end has been reached.
     * @note In bidirectional mode it first switches to the input direction.
     * @endif
     */
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

    /**
     * @lang{ZH}
     * @brief 读取并消费当前字符。
     *
     * 若读缓冲区非空，弹出并返回其队首字符；否则直接从底层转换器读取一个字符返回。
     * @return 被读取的字符；若已到达末尾则为空的 optional。
     * @note 在双向模式下会先切换到输入方向。
     * @endif
     *
     * @lang{EN}
     * @brief Reads and consumes the current character.
     *
     * If the read buffer is non-empty, pops and returns its front character; otherwise
     * reads one character directly from the underlying converter and returns it.
     * @return The character read; an empty optional if the end has been reached.
     * @note In bidirectional mode it first switches to the input direction.
     * @endif
     */
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

    /**
     * @lang{ZH}
     * @brief 先消费当前字符，再预读下一个字符。
     *
     * 等价于先调用 sbumpc() 前进一个字符，再调用 sgetc() 预读。
     * @return 下一个字符；若前进或预读遇到末尾则为空的 optional。
     * @endif
     *
     * @lang{EN}
     * @brief Consumes the current character, then peeks the next one.
     *
     * Equivalent to calling sbumpc() to advance by one character, then sgetc() to peek.
     * @return The next character; an empty optional if advancing or peeking hits the end.
     * @endif
     */
    std::optional<char_type> snextc() requires (IsIn)
    {
        if (!sbumpc().has_value())
            return std::optional<char_type>{};
        return sgetc();
    }

    /**
     * @lang{ZH}
     * @brief 批量读取最多 `n` 个字符到缓冲区 `s`。
     *
     * 先耗尽读缓冲区中已有的字符，再从底层转换器继续读取，直到取满 `n` 个或到达末尾。
     * @param s 目标缓冲区。
     * @param n 最多读取的字符数。
     * @param got 可选出参。非空时，始终反映已读入 `s` 的字符数——包括**抛出异常时**：本函数
     *        抛出后，`*got` 仍是已消费并写入 `s` 的准确数量，调用方据此可完整交付这批数据，
     *        不会丢失。返回值只在正常返回时可用，故需要异常路径下的计数时必须使用本参数。
     * @return 实际读取的字符数；`s` 为 nullptr 或 `n` 为 0 时返回 0。
     * @note 在双向模式下会先切换到输入方向。
     * @endif
     *
     * @lang{EN}
     * @brief Reads up to `n` characters in bulk into the buffer `s`.
     *
     * First drains the characters already in the read buffer, then continues reading from
     * the underlying converter until `n` characters are obtained or the end is reached.
     * @param s The destination buffer.
     * @param n The maximum number of characters to read.
     * @param got Optional out-parameter. When non-null it always reflects the number of
     *        characters already read into `s` -- including **when an exception is thrown**:
     *        after this function throws, `*got` is still the exact number consumed and
     *        written into `s`, so the caller can deliver that data instead of losing it.
     *        The return value is only available on a normal return, so this parameter is
     *        required whenever the count is needed on the exception path.
     * @return The number of characters actually read; 0 if `s` is nullptr or `n` is 0.
     * @note In bidirectional mode it first switches to the input direction.
     * @endif
     */
    size_t sgetn(char_type* s, size_t n, size_t* got = nullptr) requires (IsIn)
    {
        if (got) *got = 0;
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
        if (got) *got = res;

        while (res < n)
        {
            size_t c = m_cvt.get(s, n - res);
            assert(c <= n - res);
            if (c == 0) break;
            res += c;
            s += c;
            if (got) *got = res;
        }
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 查询是否已到达输入末尾。
     *
     * 仅当读缓冲区为空**且**底层转换器也报告 EOF 时才返回 true——只要读缓冲区中还残留
     * 字符（例如 peek 或 put-back 的内容），就不算末尾。
     * @return 已到达末尾则为 true。
     * @note 在双向模式下会先切换到输入方向。
     * @endif
     *
     * @lang{EN}
     * @brief Queries whether the end of input has been reached.
     *
     * Returns true only when the read buffer is empty **and** the underlying converter
     * also reports EOF — as long as characters remain in the read buffer (e.g. peeked or
     * put-back content), the end is not considered reached.
     * @return true if the end has been reached.
     * @note In bidirectional mode it first switches to the input direction.
     * @endif
     */
    bool is_eof() requires (IsIn)
    {
        if constexpr (IsOut)
            switch_to_get();
        return (this->m_read_buf.empty()) && (m_cvt.is_eof());
    }

    /**
     * @lang{ZH}
     * @brief 将字符压回读缓冲区，使其成为下一次读取操作返回的字符。
     *
     * 将字符压回读缓冲区，使其成为下一次读取操作返回的字符。
     * 回退按调用次数计数，不校验压回的字符是否与原始读取内容一致——调用方可以用任意
     * 字符替换原始内容（例如 `sgetc()` 读到 'b' 之后压回 '?'，之后仍按回退了 1 个字符计数）。
     * 压回次数没有上限，包括超过实际已读取字符数的情形；此时 tell() 报告的逻辑位置会在
     * 流起点处饱和（钳位为 0），不会产生 size_t 下溢的错误巨大值，见 tell()。
     * @endif
     *
     * @lang{EN}
     * @brief Pushes a character back into the read buffer so it becomes the next
     * character returned by a subsequent read.
     *
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
    /**
     * @lang{ZH} @} @endif
     * @lang{EN} @} @endif
     */

    /**
     * @lang{ZH}
     * @name 写入操作（仅 IsOut）
     * @{
     * @endif
     * @lang{EN}
     * @name Write operations (IsOut only)
     * @{
     * @endif
     */

    /**
     * @lang{ZH}
     * @brief 写入单个字符。
     * @param ch 要写入的字符。
     * @note 在双向模式下会先切换到输出方向。
     * @endif
     *
     * @lang{EN}
     * @brief Writes a single character.
     * @param ch The character to write.
     * @note In bidirectional mode it first switches to the output direction.
     * @endif
     */
    void sputc(char_type ch) requires (IsOut)
    {
        if constexpr (IsIn)
            switch_to_put();
        m_cvt.put(&ch, 1);
    }

    /**
     * @lang{ZH}
     * @brief 批量写入 `n` 个字符。
     * @param s 源缓冲区。
     * @param n 要写入的字符数。
     * @note `s` 为 nullptr 或 `n` 为 0 时不做任何事。在双向模式下会先切换到输出方向。
     * @endif
     *
     * @lang{EN}
     * @brief Writes `n` characters in bulk.
     * @param s The source buffer.
     * @param n The number of characters to write.
     * @note Does nothing if `s` is nullptr or `n` is 0. In bidirectional mode it first
     * switches to the output direction.
     * @endif
     */
    void sputn(const char_type* s, size_t n) requires (IsOut)
    {
        if ((s == nullptr) || (n == 0)) return;

        if constexpr (IsIn)
            switch_to_put();
        m_cvt.put(s, n);
    }

    /**
     * @lang{ZH}
     * @brief 刷新输出，将缓冲数据推送到底层设备。
     * @note 在双向模式下会先切换到输出方向。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes output, pushing buffered data to the underlying device.
     * @note In bidirectional mode it first switches to the output direction.
     * @endif
     */
    void flush() requires (IsOut)
    {
        if constexpr (IsIn)
            switch_to_put();
        m_cvt.flush();
    }
    /**
     * @lang{ZH} @} @endif
     * @lang{EN} @} @endif
     */

    /**
     * @lang{ZH}
     * @name 定位操作
     * @{
     * @endif
     * @lang{EN}
     * @name Positioning operations
     * @{
     * @endif
     */
    /**
     * @lang{ZH}
     * @brief 返回当前逻辑读取位置。
     *
     * 返回当前逻辑读取位置。
     * 若通过 sputbackc() 压回的字符数超过了实际从底层转换器读取的字符数（见 sputbackc()
     * 的按次数计数、不校验内容的语义），本函数返回的位置在流起点处饱和为 0，而不是让
     * 减法在 size_t 上下溢产生一个巨大的错误值。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the current logical read position.
     *
     * Returns the current logical read position.
     * If more characters have been pushed back via sputbackc() than have actually
     * been read from the underlying converter (see sputbackc()'s count-based,
     * content-agnostic semantics), the position returned here saturates at the
     * stream origin (0) rather than letting the subtraction wrap around to a huge
     * size_t value.
     * @endif
     */
    [[nodiscard]] size_t tell() const
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

    /**
     * @lang{ZH}
     * @brief 定位到绝对位置 `pos`。
     * @param pos 目标绝对位置。
     * @note 若支持输入，定位会清空读缓冲区（丢弃已 peek/put-back 的字符）。
     * @endif
     *
     * @lang{EN}
     * @brief Seeks to the absolute position `pos`.
     * @param pos The target absolute position.
     * @note If input is supported, seeking clears the read buffer (discarding any
     * peeked/put-back characters).
     * @endif
     */
    void seek(size_t pos)
    {
        m_cvt.seek(pos);
        if constexpr (IsIn)
            m_read_buf.clear();
    }

    /**
     * @lang{ZH}
     * @brief 相对定位（相对当前位置偏移 `pos`）。
     * @param pos 相对偏移量。
     * @note 若支持输入，定位会清空读缓冲区（丢弃已 peek/put-back 的字符）。
     * @endif
     *
     * @lang{EN}
     * @brief Relative seek (offset `pos` from the current position).
     * @param pos The relative offset.
     * @note If input is supported, seeking clears the read buffer (discarding any
     * peeked/put-back characters).
     * @endif
     */
    void rseek(size_t pos)
    {
        m_cvt.rseek(pos);
        if constexpr (IsIn)
            m_read_buf.clear();
    }
    /**
     * @lang{ZH} @} @endif
     * @lang{EN} @} @endif
     */

    /**
     * @lang{ZH}
     * @name 读/写方向切换（仅 IsIn && IsOut）
     * @{
     * @endif
     * @lang{EN}
     * @name Read/write direction switching (IsIn && IsOut only)
     * @{
     * @endif
     */
    /**
     * @lang{ZH}
     * @brief 切换到输入模式。
     * @note 直接委托给底层转换器的 switch_to_get()。
     * @endif
     *
     * @lang{EN}
     * @brief Switches to input mode.
     * @note Delegates directly to the underlying converter's switch_to_get().
     * @endif
     */
    void switch_to_get() requires (IsIn && IsOut)
    {
        m_cvt.switch_to_get();
    }

    /**
     * @lang{ZH}
     * @brief 切换到输出模式。
     *
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
     * @brief Switches to output mode.
     *
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
    /**
     * @lang{ZH} @} @endif
     * @lang{EN} @} @endif
     */

    /**
     * @lang{ZH}
     * @name 其他（设备/转换器接口透传）
     * @{
     * @endif
     * @lang{EN}
     * @name Others (device/converter interface pass-through)
     * @{
     * @endif
     */
    /**
     * @lang{ZH}
     * @brief 返回对底层设备的引用。
     * @return 底层设备的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns a reference to the underlying device.
     * @return A reference to the underlying device.
     * @endif
     */
    device_type& device()
    {
        return m_cvt.device();
    }

    /**
     * @lang{ZH}
     * @brief 分离并取回底层设备。
     *
     * 在支持输入且读缓冲区非空时，先尝试把底层转换器定位回逻辑读位置（tell()），以便交还
     * 的设备停在正确位置，随后清空读缓冲区。该定位失败会被**有意吞掉**：不支持定位的设备
     * （如管道/终端的 stdin）本就无法满足此重定位，这是设备固有属性而非可处理的错误——
     * 报告它会让此类设备上例行的 detach()/attach() 循环开始抛异常；对这类设备而言，丢失
     * 预读字符是可接受且不可避免的代价。
     * @return 一个 pair：取回的设备，以及转换器在分离过程中捕获的异常指针（可能为空）。
     * @note 本函数为 noexcept。
     * @endif
     *
     * @lang{EN}
     * @brief Detaches and retrieves the underlying device.
     *
     * When input is supported and the read buffer is non-empty, it first tries to
     * reposition the underlying converter back to the logical read position (tell()), so
     * that the returned device stops at the correct place, then clears the read buffer.
     * A failure of that reposition is **swallowed on purpose**: a device that does not
     * support positioning (e.g. a pipe/tty-backed stdin) inherently cannot honor this
     * reposition — an intrinsic property of the device, not an actionable error.
     * Reporting it would make routine detach()/attach() cycles on such devices start
     * throwing; for those devices, losing the lookahead character is the accepted and
     * unavoidable cost.
     * @return A pair: the retrieved device, and the exception pointer captured by the
     * converter during detach (possibly null).
     * @note This function is noexcept.
     * @endif
     */
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
                catch (...) // NOLINT(bugprone-empty-catch)
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

    /**
     * @lang{ZH}
     * @brief 附加一个新设备并重新初始化转换器。
     * @param dev 要附加的设备；默认为一个默认构造的设备。
     * @note 若支持输入，会先清空读缓冲区。
     * @endif
     *
     * @lang{EN}
     * @brief Attaches a new device and re-initializes the converter.
     * @param dev The device to attach; defaults to a default-constructed device.
     * @note If input is supported, the read buffer is cleared first.
     * @endif
     */
    void attach(device_type&& dev = device_type{})
    {
        if constexpr (IsIn)
            m_read_buf.clear();
        m_cvt.attach(std::move(dev));
        init_cvt();
    }

    /**
     * @lang{ZH}
     * @brief 向底层转换器应用一个行为策略。
     * @param acc 行为策略对象。
     * @endif
     *
     * @lang{EN}
     * @brief Applies a behavior policy to the underlying converter.
     * @param acc The behavior policy object.
     * @endif
     */
    void adjust(const cvt_behavior& acc)
    {
        m_cvt.adjust(acc);
    }

    /**
     * @lang{ZH}
     * @brief 从底层转换器提取内部状态。
     * @param acc 用于接收状态的对象。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts internal state from the underlying converter.
     * @param acc The object that receives the state.
     * @endif
     */
    void retrieve(cvt_status& acc) const
    {
        m_cvt.retrieve(acc);
    }
    /**
     * @lang{ZH} @} @endif
     * @lang{EN} @} @endif
     */

private:
    /**
     * @lang{ZH}
     * @brief 初始化转换器：建立初始 I/O 状态并进入主内容阶段。
     *
     * 调用底层转换器的 bos()（建立初始状态并返回初始方向）与 main_cont_beg()。对于
     * 单向的流缓冲区，若初始方向与所需方向不一致，则相应切换到输入或输出方向。
     * @endif
     *
     * @lang{EN}
     * @brief Initializes the converter: establishes the initial I/O state and enters the
     * main content phase.
     *
     * Calls the underlying converter's bos() (which establishes the initial state and
     * returns the initial direction) and main_cont_beg(). For a unidirectional stream
     * buffer, if the initial direction does not match the required one, it switches to
     * the input or output direction accordingly.
     * @endif
     */
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
    runtime_cvt<TDevice, TChar> m_cvt;      ///< @lang{ZH} 底层转换器管线。 @endif @lang{EN} The underlying converter pipeline. @endif

    /**
     * @lang{ZH}
     * @brief 读缓冲区，保存已 peek/put-back 的字符（仅 IsIn 时存在）。
     *
     * 队首即为下一次读取将返回的字符。非输入模式下退化为 `std::monostate`，并借助
     * `[[no_unique_address]]` 不占用对象空间。
     * @endif
     *
     * @lang{EN}
     * @brief Read buffer holding peeked/put-back characters (present only when IsIn).
     *
     * The front element is the character the next read will return. In non-input mode it
     * degenerates to `std::monostate` and, thanks to `[[no_unique_address]]`, takes no
     * object space.
     * @endif
     */
    [[no_unique_address]]
    std::conditional_t<IsIn, std::deque<char_type>, std::monostate> m_read_buf;
};

/**
 * @lang{ZH}
 * @brief 双向流缓冲区：同时支持输入与输出。
 *
 * `base_streambuf<TDevice, TChar, true, true>` 的便捷别名，并继承其全部构造函数。
 * @tparam TDevice 底层设备类型。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Bidirectional stream buffer: supports both input and output.
 *
 * A convenience alias for `base_streambuf<TDevice, TChar, true, true>` that inherits all
 * of its constructors.
 * @tparam TDevice The underlying device type.
 * @tparam TChar The character type.
 * @endif
 */
template <io_device TDevice, typename TChar>
struct streambuf : public base_streambuf<TDevice, TChar, true, true>
{
    using base_streambuf<TDevice, TChar, true, true>::base_streambuf;
};

/**
 * @lang{ZH}
 * @brief 只读流缓冲区：仅支持输入。
 *
 * `base_streambuf<TDevice, TChar, true, false>` 的便捷别名，并继承其全部构造函数。
 * @tparam TDevice 底层设备类型。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Input-only stream buffer: supports input only.
 *
 * A convenience alias for `base_streambuf<TDevice, TChar, true, false>` that inherits all
 * of its constructors.
 * @tparam TDevice The underlying device type.
 * @tparam TChar The character type.
 * @endif
 */
template <io_device TDevice, typename TChar>
struct istreambuf : public base_streambuf<TDevice, TChar, true, false>
{
    using base_streambuf<TDevice, TChar, true, false>::base_streambuf;
};

/**
 * @lang{ZH}
 * @brief 只写流缓冲区：仅支持输出。
 *
 * `base_streambuf<TDevice, TChar, false, true>` 的便捷别名，并继承其全部构造函数。
 * @tparam TDevice 底层设备类型。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Output-only stream buffer: supports output only.
 *
 * A convenience alias for `base_streambuf<TDevice, TChar, false, true>` that inherits all
 * of its constructors.
 * @tparam TDevice The underlying device type.
 * @tparam TChar The character type.
 * @endif
 */
template <io_device TDevice, typename TChar>
struct ostreambuf : public base_streambuf<TDevice, TChar, false, true>
{
    using base_streambuf<TDevice, TChar, false, true>::base_streambuf;
};

/**
 * @lang{ZH}
 * @name 类模板实参推导指引（CTAD）
 * @brief 由设备（及可选的转换器工厂）推导 streambuf/istreambuf/ostreambuf 的字符类型。
 *
 * 无转换器工厂时，字符类型直接取设备的 `char_type`；提供转换器工厂时，则通过
 * `ext_to_int`（见 io/io_concepts.h）从转换管线推导出内部字符类型。
 * @{
 * @endif
 *
 * @lang{EN}
 * @name Class-template argument deduction guides (CTAD)
 * @brief Deduce the character type of streambuf/istreambuf/ostreambuf from a device (and
 * an optional converter creator).
 *
 * Without a converter creator, the character type is taken directly from the device's
 * `char_type`; with a converter creator, the internal character type is deduced from the
 * converter pipeline via `ext_to_int` (see io/io_concepts.h).
 * @{
 * @endif
 */
template <io_device TDevice>
streambuf(TDevice) -> streambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
streambuf(TDevice, const TCreator&) -> streambuf<TDevice, ext_to_int<no_rb_root_cvt<TDevice>, TCreator>>;

template <io_device TDevice>
istreambuf(TDevice) -> istreambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
istreambuf(TDevice, const TCreator&) -> istreambuf<TDevice, ext_to_int<rb_root_cvt<TDevice>, TCreator>>;

template <io_device TDevice>
istreambuf(TDevice, bool) -> istreambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
istreambuf(TDevice, const TCreator&, bool) -> istreambuf<TDevice, ext_to_int<rb_root_cvt<TDevice>, TCreator>>;

template <io_device TDevice>
ostreambuf(TDevice) -> ostreambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
ostreambuf(TDevice, const TCreator&) -> ostreambuf<TDevice, ext_to_int<no_rb_root_cvt<TDevice>, TCreator>>;
/**
 * @lang{ZH} @} @endif
 * @lang{EN} @} @endif
 */
}

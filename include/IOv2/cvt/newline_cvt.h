// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file newline_cvt.h
 * @lang{ZH}
 * 换行转换器（newline_cvt）及其工厂类的定义文件。
 * 下层使用 `\r\n` 作行尾、上层使用 `\n`，用于模拟 Windows 文本模式的读写行为。
 * 转换器与平台无关，可以叠加在任何字符类型为整数类型的转换器之上。
 * @endif
 *
 * @lang{EN}
 * Definition of the newline converter (newline_cvt) and its factory class.
 * The layer below ends lines with `\r\n`, the layer above with `\n`; this emulates how
 * Windows text mode reads and writes. The converter is platform-independent and can be
 * stacked on any converter whose character type is an integral type.
 * @endif
 */
#pragma once
#include <IOv2/cvt/abs_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * 换行转换器：读时把 `\r\n` 换成 `\n`，写时把 `\n` 换成 `\r\n`。
 *
 * 转换规则：
 * - 读：`\r\n` → `\n`；单独的 `\r`、下层数据末尾的 `\r` 原样保留；其余字符原样。
 * - 写：`\n` → `\r\n`；其余字符原样（数据中的 `\r\n` 写成 `\r\r\n`，读回仍是 `\r\n`）。
 *
 * 位置按本层（上层）的逻辑字符计数，`\r\n` 算一个字符：在空流中写入 `"a\nb"` 后
 * `tell()` 返回 3。要知道一段数据含多少个逻辑字符只能实际读过它，因此定位只支持
 * 读状态下的 `seek(0)` 与 `rseek(0)`（后者读到末尾并计数）；写入只能发生在流的末尾。
 *
 * @tparam KernelType 下层转换器类型，须满足 `io_converter`，其 `internal_type` 须为整数类型。
 *
 * @note 线程安全性：本类**不是**线程安全的。
 * @endif
 *
 * @lang{EN}
 * Newline converter: reading turns `\r\n` into `\n`, writing turns `\n` into `\r\n`.
 *
 * Translation rules:
 * - Read: `\r\n` → `\n`; a lone `\r`, and a `\r` that ends the data below, are kept;
 *   everything else as is.
 * - Write: `\n` → `\r\n`; everything else as is (`\r\n` in the data is written as
 *   `\r\r\n` and reads back as `\r\n`).
 *
 * Positions count logical characters of this (the upper) layer, a `\r\n` counting as one:
 * having written `"a\nb"` to an empty stream, `tell()` returns 3. How many logical
 * characters a stretch holds can only be known by reading it, so positioning supports only
 * `seek(0)` and `rseek(0)` while reading (the latter reads to the end, counting); writes
 * happen only at the end of the stream.
 *
 * @tparam KernelType The converter below, which must satisfy `io_converter` and whose
 *                    `internal_type` must be an integral type.
 *
 * @note Thread safety: this class is **not** thread-safe.
 * @endif
 */
template <io_converter KernelType>
    requires std::is_integral_v<typename KernelType::internal_type>
class newline_cvt : public abs_cvt<KernelType, typename KernelType::internal_type, true, true>
{
    using BT = abs_cvt<KernelType, typename KernelType::internal_type, true, true>;
    friend BT;  // for put_main, get_main, and private hooks

public:
    using device_type = typename KernelType::device_type;     ///< 底层设备类型 / Underlying device type.
    using internal_type = typename KernelType::internal_type; ///< 本层字符类型 / Character type at this layer.
    using external_type = internal_type;                       ///< 与本层相同 / Same as this layer's.

    /**
     * @lang{ZH} 以下层转换器构造。 @endif
     * @lang{EN} Construct on top of the converter below. @endif
     */
    explicit newline_cvt(KernelType kernel)
        : BT(std::move(kernel))
    {}

    /**
     * @lang{ZH}
     * 拷贝操作仅当 `KernelType` 可拷贝时可用。`abs_cvt` 的拷贝构造不带约束，若不在此加约束，
     * `std::copy_constructible` 对不可拷贝的下层也会报告为真（如 `runtime_cvt` 的 `clone()`
     * 据此决定是否拷贝），直到实例化时才出错。
     * @endif
     *
     * @lang{EN}
     * Copying is available only when `KernelType` is copyable. `abs_cvt`'s copy constructor
     * is unconstrained; without the constraint here, `std::copy_constructible` would report
     * true over a non-copyable layer below (`runtime_cvt`'s `clone()` decides on it, say),
     * failing only on instantiation.
     * @endif
     */
    newline_cvt(const newline_cvt&) requires (std::copy_constructible<KernelType>) = default;
    newline_cvt& operator=(const newline_cvt&) requires (std::copy_constructible<KernelType>) = default;
    newline_cvt(newline_cvt&&) noexcept = default;             ///< 移动构造 / Move constructor.
    newline_cvt& operator=(newline_cvt&&) noexcept = default;  ///< 移动赋值 / Move assignment.
    ~newline_cvt() = default;                                  ///< 析构 / Destructor.

private:
    /**
     * @lang{ZH}
     * 写状态下当前位置是否处于流的末尾。只在写状态下有意义，读状态下不读也不改。
     * - `unknown`：尚未判断。`main_cont_beg()`、`detach()`/`attach()` 之后处于此状态；
     *   写状态下第一次需要时（`put`、`rseek(0)`）才判断，结果缓存。
     * - `yes`：处于末尾。读 → 写切换（要求 `is_eof()`）后即为此值；此后一直写，写入只往后
     *   追加、写状态下又不能定位，因此保持不变。
     * - `no`：不在末尾（只写的下层以不截断方式打开非空流）。此时拒绝写入与 `rseek(0)`。
     * @endif
     *
     * @lang{EN}
     * Whether the current position is the end of the stream, while writing. Meaningful only
     * while writing; reading neither reads nor changes it.
     * - `unknown`: not decided yet. The state after `main_cont_beg()` and `detach()`/
     *   `attach()`; decided the first time writing needs it (`put`, `rseek(0)`), then cached.
     * - `yes`: at the end. Set by a read → write switch (which requires `is_eof()`); it
     *   stays so while writing goes on, since writes only append and writing allows no
     *   repositioning.
     * - `no`: not at the end (a write-only layer below opened on a non-empty stream without
     *   truncation). Writes and `rseek(0)` are refused then.
     * @endif
     */
    enum class at_end : std::uint8_t { unknown, yes, no };

    static constexpr internal_type s_cr = static_cast<internal_type>('\r');
    static constexpr internal_type s_lf = static_cast<internal_type>('\n');

    /// 每次向下层取数的最大长度 / Most characters taken from the layer below at a time.
    static constexpr std::size_t s_buf_len = 64;

    /// `rseek(0)` 计数时每次向下层取数的长度 / Characters taken from below at a time when `rseek(0)` counts.
    static constexpr std::size_t s_count_buf_len = 1024;

    /**
     * @lang{ZH}
     * `abs_cvt::detach()` / `attach()` 的钩子（也在启动失败路径上调用）：清零逻辑位置，
     * 末尾状态回到 `unknown`。幂等。
     * @endif
     *
     * @lang{EN}
     * Hook for `abs_cvt::detach()` / `attach()` (also called on failed start-up): clears
     * the logical position and returns the end state to `unknown`. Idempotent.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        m_accu_len = 0;
        m_at_end = at_end::unknown;
        return nullptr;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::main_cont_beg()` 的钩子：主内容从逻辑位置 0 开始，末尾状态回到 `unknown`。
     * 与 `detach_impl()` 重复，但被移动过的对象可以不经 `attach()` 直接重新开始，
     * 而默认的移动操作不复位源对象的这两个成员。
     * @endif
     *
     * @lang{EN}
     * Hook for `abs_cvt::main_cont_beg()`: the main content starts at logical position 0,
     * and the end state returns to `unknown`. This repeats `detach_impl()`, but a moved-from
     * object can start again without `attach()`, and the defaulted move operations do not
     * reset these two members of the source.
     * @endif
     */
    void main_cont_beg_impl()
    {
        m_accu_len = 0;
        m_at_end = at_end::unknown;
    }

    /// `translate()` 的结果 / Result of `translate()`.
    struct scan_result
    {
        std::size_t consumed; ///< 消耗的下层字符数 / Characters from below consumed.
        std::size_t produced; ///< 产出的本层字符数 / Characters of this layer produced.
        bool        held_cr;  ///< 停在段尾的 `\r` 上（未消耗）/ Stopped at a `\r` ending the stretch (not consumed).
    };

    /**
     * @lang{ZH}
     * 读取方向的转换规则，读与计数共用，保证两者对字符的计法完全一致。
     *
     * 处理下层的一段 `[ptr, ptr + len)`，最多产出 `room` 个本层字符：`\r\n` → `\n`，
     * 后面跟着别的字符的 `\r` 原样产出，其余字符原样产出。段尾的 `\r` 看不到下一个字符，
     * 不消耗，由调用方决定如何处理（`held_cr` 为 true）。
     *
     * @tparam Store 为 `true` 时把产出写入 `out`；为 `false` 时只计数，`out` 不被访问。
     * @endif
     *
     * @lang{EN}
     * The translation rule of the read direction, shared by reading and counting so that the
     * two count characters exactly alike.
     *
     * Process a stretch `[ptr, ptr + len)` from below, producing at most `room` characters of
     * this layer: `\r\n` → `\n`, a `\r` followed by anything else is produced as is, and so
     * is every other character. A `\r` that ends the stretch cannot see the next character;
     * it is not consumed, and the caller decides what to do with it (`held_cr` is true).
     *
     * @tparam Store When `true`, the output is written to `out`; when `false`, it is only
     *               counted and `out` is not accessed.
     * @endif
     */
    template <bool Store>
    static scan_result translate(const internal_type* ptr, std::size_t len,
                                 internal_type* out, std::size_t room) noexcept
    {
        std::size_t i = 0;
        std::size_t produced = 0;
        while (i < len && produced < room)
        {
            internal_type c = ptr[i];
            if (c == s_cr)
            {
                if (i + 1 == len)
                    return {i, produced, true};
                if (ptr[i + 1] == s_lf)
                {
                    c = s_lf;
                    ++i;
                }
            }
            if constexpr (Store)
                out[produced] = c;
            ++produced;
            ++i;
        }
        return {i, produced, false};
    }

    /**
     * @lang{ZH}
     * 从下层读取字符并把 `\r\n` 换成 `\n`（由 `abs_cvt::get` 调用）。
     *
     * 取到的一段以 `\r` 结尾时，看不到下一个字符就无法判断，于是不消费这个 `\r`，把它留在
     * 读取区：已交出字符时先返回，下一次 `get` 连同新数据重新判断；一个字符都没交出时再向
     * 下层取数（交互式输入在这里会等下一个字符）。下层再也取不到数据时，这个 `\r` 原样交出。
     * 留在读取区的 `\r` 未交出，不计入逻辑位置；`abs_cvt` 的 `kernel_is_eof()` 也因读取区
     * 非空而返回 false。
     *
     * 数据本身不会引起错误，`second` 只用来带回下层抛出的异常。
     *
     * @return `first` 为交出的字符数，`second` 为下层抛出的异常（无则为 `nullptr`）。
     * @endif
     *
     * @lang{EN}
     * Read characters from the layer below, turning `\r\n` into `\n` (called by
     * `abs_cvt::get`).
     *
     * When a stretch taken ends in `\r`, nothing can be decided without the next character,
     * so that `\r` is left unconsumed in the read area: with characters delivered, this call
     * returns them and the next `get` decides together with new data; with none delivered,
     * it takes more from below (interactive input waits for the next character here). Once
     * the layer below has nothing more, the `\r` is delivered as is. A `\r` left in the read
     * area has not been delivered and does not count towards the logical position; it also
     * keeps `abs_cvt`'s `kernel_is_eof()` false, the read area not being empty.
     *
     * The data itself causes no errors; `second` only carries exceptions from the layer
     * below.
     *
     * @return `first` is the number of characters delivered, `second` the exception thrown
     *         by the layer below (`nullptr` if none).
     * @endif
     */
    std::pair<std::size_t, std::exception_ptr>
    get_main(internal_type* to, std::size_t to_max) noexcept
        requires (cvt_cpt::support_get<KernelType>)
    {
        std::size_t total = 0;
        try
        {
            if (to_max == 0) return {0, nullptr};
            auto reader = BT::kernel_reader();
            reader.reset(s_buf_len);

            bool held_cr = false;
            while (total < to_max)
            {
                // A held \r needs at least one character behind it to be decided.
                std::size_t want = std::min(to_max - total, s_buf_len);
                if (held_cr) want = std::max<std::size_t>(want, 2);

                auto [ptr, len] = reader.get_buf(want);
                if (len == 0)
                    return {total, nullptr};
                if (held_cr && len == 1)
                {
                    // Nothing more below: the \r ends the data and is kept.
                    to[total++] = s_cr;
                    ++m_accu_len;
                    return {total, nullptr};
                }

                const auto res = translate<true>(ptr, len, to + total, to_max - total);
                held_cr = res.held_cr;
                total += res.produced;
                m_accu_len += res.produced;
                if (res.consumed != len)
                    reader.rollback(len - res.consumed);
                if (held_cr && total != 0)
                    return {total, nullptr};
            }
            return {total, nullptr};
        }
        catch (...)
        {
            return {total, std::current_exception()};
        }
    }

    /**
     * @lang{ZH}
     * 把字符写往下层，`\n` 换成 `\r\n`（由 `abs_cvt::put` 调用）。
     *
     * 写入只能发生在流的末尾：末尾状态为 `unknown` 时先判断一次（见 `resolve_at_end()`），
     * 为 `no` 时拒绝写入。拒绝发生在任何字符写出之前，但 `abs_cvt::put` 仍会因异常而污染
     * 转换器；走到这一步的只有只写的流，污染不妨碍别的操作。
     *
     * @throws cvt_error 当前位置不在流的末尾。
     * @endif
     *
     * @lang{EN}
     * Write characters to the layer below, turning `\n` into `\r\n` (called by
     * `abs_cvt::put`).
     *
     * Writes happen only at the end of the stream: an `unknown` end state is decided first
     * (see `resolve_at_end()`), and `no` refuses the write. The refusal comes before any
     * character is written, yet `abs_cvt::put` still taints the converter on the exception;
     * only write-only streams get here, where the taint stands in the way of nothing else.
     *
     * @throws cvt_error The current position is not the end of the stream.
     * @endif
     */
    void put_main(const internal_type* to, std::size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        resolve_at_end();
        if (m_at_end == at_end::no)
            throw cvt_error("newline_cvt::put fail: not at the end of the stream");
        if (to_size == 0) return;

        auto writer = BT::kernel_writer();
        writer.reset(s_buf_len * 2);
        std::size_t done = 0;
        while (done != to_size)
        {
            // Each character takes at most two slots below.
            const std::size_t count = std::min(s_buf_len, to_size - done);
            internal_type* ptr = writer.put_buf(count * 2);
            std::size_t used = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                const internal_type c = to[done + i];
                if (c == s_lf)
                    ptr[used++] = s_cr;
                ptr[used++] = c;
            }
            if (used != count * 2)
                writer.rollback(count * 2 - used);
            done += count;
        }
        // commit() is the responsibility of abs_cvt::put.
        m_accu_len += to_size;
    }

    /**
     * @lang{ZH}
     * 切换至写状态的派生层前置检查。从读状态切换时要求已到达流的末尾（`is_eof()`：
     * 读取区里没有暂存的 `\r`，下层也到了末尾），之后末尾状态即为 `yes`。
     * 调用之前 `abs_cvt::switch_to_put` 已把读取区还给下层。
     *
     * 写 → 读不需要检查，因此没有 `switch_to_get_impl()`：写入只发生在末尾，切换实际上也在
     * 末尾。
     *
     * @throws cvt_error 从读状态切换时未到达流的末尾。
     * @endif
     *
     * @lang{EN}
     * Derived-layer precondition for switching to writing. Switching from reading requires
     * the end of the stream (`is_eof()`: no `\r` held in the read area, and the layer below
     * at its end); the end state is `yes` from then on. By the time it runs,
     * `abs_cvt::switch_to_put` has handed the read area back to the layer below.
     *
     * Write → read needs no check, hence no `switch_to_get_impl()`: writes happen only at
     * the end, so the switch in effect happens there too.
     *
     * @throws cvt_error Switching from reading before the end of the stream.
     * @endif
     */
    void switch_to_put_impl()
        requires (cvt_cpt::support_io_switch<KernelType>)
    {
        if (BT::m_io_status == io_status::input)
        {
            if (!this->is_eof())
                throw cvt_error("newline_cvt::switch_to_put fail: not at the end of the stream");
            m_at_end = at_end::yes;
        }
    }

    /**
     * @lang{ZH}
     * 写状态下末尾状态为 `unknown` 时判断一次并缓存，按下层的能力依次选择：
     * - 下层能读：`kernel_is_eof()`；
     * - 下层只写但能定位：记下 `kernel_tell()`，`kernel_rseek(0)` 后再取一次，两者相同即在
     *   末尾；不同则 `kernel_seek()` 移回原处。两次取值来自同一层，比较不涉及换算；
     * - 下层不能定位（如管道）：写入本来只能追加，视为在末尾。
     *
     * 只在写状态下、本层没有暂存待写数据时调用（`abs_cvt::put` 在返回前已提交）。
     * @endif
     *
     * @lang{EN}
     * While writing, decide an `unknown` end state once and cache it, by what the layer
     * below can do:
     * - it can read: `kernel_is_eof()`;
     * - it is write-only but positionable: take `kernel_tell()`, `kernel_rseek(0)`, take it
     *   again; equal means at the end, otherwise `kernel_seek()` back. Both values come from
     *   the same layer, so the comparison involves no conversion;
     * - it is not positionable (a pipe, say): writes can only append anyway, so it counts as
     *   the end.
     *
     * Called only while writing, with nothing staged at this layer (`abs_cvt::put` commits
     * before it returns).
     * @endif
     *
     * @lang{ZH} 下层在判断途中抛出时，其位置可能已不在原处，转换器被污染。 @endif
     * @lang{EN} If the layer below throws midway, its position may no longer be where it
     *           was, and the converter is tainted. @endif
     */
    void resolve_at_end()
    {
        if (m_at_end != at_end::unknown) return;

        try
        {
            if constexpr (cvt_cpt::support_get<KernelType>)
                m_at_end = BT::kernel_is_eof() ? at_end::yes : at_end::no;
            else if constexpr (cvt_cpt::support_positioning<KernelType>)
            {
                const std::size_t saved = BT::kernel_tell();
                BT::kernel_rseek(0);
                if (BT::kernel_tell() == saved)
                    m_at_end = at_end::yes;
                else
                {
                    BT::kernel_seek(saved);
                    m_at_end = at_end::no;
                }
            }
            else
                m_at_end = at_end::yes;
        }
        catch (...)
        {
            BT::set_tainted();
            throw;
        }
    }

    /**
     * @lang{ZH}
     * 返回逻辑位置：已交出（读）或已写入（写）的本层字符数，`\r\n` 算一个字符。
     * 暂存在读取区的 `\r` 尚未交出，不计入。
     * @endif
     *
     * @lang{EN}
     * Return the logical position: the characters of this layer delivered (reading) or
     * written (writing), a `\r\n` counting as one. A `\r` held in the read area has not been
     * delivered and does not count.
     * @endif
     */
    [[nodiscard]] std::size_t tell_impl() const
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        return m_accu_len;
    }

    /**
     * @lang{ZH}
     * 定位到逻辑位置 `pos`。只支持读状态下的 `seek(0)`：下层 `seek(0)`，逻辑位置清零；
     * 读取区（含暂存的 `\r`）由 `kernel_seek()` 丢弃。
     * `pos` 等于当前位置时 `abs_cvt::seek` 已直接返回，不会走到这里——写状态下空流的
     * `seek(0)` 即属此类。
     *
     * @throws cvt_error 处于写状态，或 `pos != 0`。
     * @endif
     *
     * @lang{EN}
     * Reposition to logical position `pos`. Only `seek(0)` while reading is supported: the
     * layer below does `seek(0)` and the logical position is cleared; `kernel_seek()` drops
     * the read area, a held `\r` included.
     * When `pos` equals the current position `abs_cvt::seek` has already returned and this
     * is not reached -- `seek(0)` on an empty stream while writing is such a case.
     *
     * @throws cvt_error Writing, or `pos != 0`.
     * @endif
     */
    void seek_impl(std::size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        if (BT::m_io_status == io_status::output)
            throw cvt_error("newline_cvt::seek fail: cannot seek while writing");
        if (pos != 0)
            throw cvt_error("newline_cvt::seek fail: only seek(0) is supported");

        BT::kernel_seek(0);
        m_accu_len = 0;
    }

    /**
     * @lang{ZH}
     * 定位到距末尾 `pos` 个字符处。只支持 `rseek(0)`，即移到末尾，并保证之后 `tell()` 精确。
     *
     * 写状态：写入只发生在末尾，所以通常已经在末尾，什么都不用做。先确认这一点（见
     * `resolve_at_end()`）：
     * - 在末尾：直接返回，位置不变。
     * - 不在末尾：下层固然能移到末尾，但那时 `tell()` 应是已有内容的逻辑字符数，要数出其中的
     *   `\r\n` 就得把这段内容读一遍。就现有的下层而言，只有只写的下层以不截断方式打开非空流时
     *   才会这样（能读的下层只在末尾时 `bos()` 才判定为写状态），而只写的下层读不了。
     *   `tell()` 无法精确，于是拒绝。判断时移动过的下层位置已恢复，流的状态不变。
     *
     * 读状态：读到末尾，边读边数（与 `get_main()` 共用 `translate()`，计法一致），不存放
     * 读到的字符，也不需要下层的 `rseek`。段尾的 `\r` 留在读取区，与下一段一起判断；
     * 下层取不到数据时它计为一个字符。下层在途中抛出时已不知道跳过了多少字符，转换器被污染。
     * 读到末尾后对下层当前位置做一次 `kernel_seek()`：重定位应丢弃先前 `get` 暂存待报的错误，
     * 而读取不会丢弃它；目标即当前位置，位置不变。
     *
     * @throws cvt_error `pos != 0`，或写状态下不在末尾。
     * @endif
     *
     * @lang{EN}
     * Reposition to `pos` characters before the end. Only `rseek(0)` is supported: going to
     * the end, with `tell()` exact afterwards.
     *
     * Writing: writes happen only at the end, so this is normally the end already and
     * nothing needs doing. That is confirmed first (see `resolve_at_end()`):
     * - At the end: return at once, the position unchanged.
     * - Not at the end: the layer below could go to its end, but `tell()` would then have to
     *   be the logical length of the existing content, and counting the `\r\n`s in it takes
     *   reading that content. Among the existing layers this happens only with a write-only
     *   layer below opened on a non-empty stream without truncation (a layer that can read
     *   has `bos()` settle on writing only at the end), and a write-only layer cannot read.
     *   `tell()` cannot be exact, so this is refused. The position the check moved below
     *   has been restored; the stream is unchanged.
     *
     * Reading: read to the end, counting on the way (through `translate()`, shared with
     * `get_main()`, so the count agrees), storing nothing read and needing no `rseek` from
     * below. A `\r` ending a stretch stays in the read area to be decided with the next one;
     * when the layer below has nothing more, it counts as one character. If the layer below
     * throws midway, how many characters were skipped is no longer known, and the converter
     * is tainted. Having reached the end, it does one `kernel_seek()` to where the layer below
     * stands: a repositioning should drop an error a previous `get` held back, and reading
     * does not; the target being the current position, the position does not change.
     *
     * @throws cvt_error `pos != 0`, or not at the end while writing.
     * @endif
     */
    void rseek_impl(std::size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        if (pos != 0)
            throw cvt_error("newline_cvt::rseek fail: only rseek(0) is supported");

        if (BT::m_io_status == io_status::output)
        {
            // Writes only happen at the end, so normally there is nothing to do; confirm it.
            resolve_at_end();
            // Not at the end: reaching it would need the logical length of what lies
            // between, which takes reading it -- impossible for the only layer that gets
            // here (write-only). tell() could not be exact, so refuse.
            if (m_at_end == at_end::no)
                throw cvt_error("newline_cvt::rseek fail: not at the end of the stream");
            return;
        }

        // Reading (a write-only layer below never is): count the characters up to the end,
        // storing none of them.
        if constexpr (cvt_cpt::support_get<KernelType>)
        {
            try
            {
                auto reader = BT::kernel_reader();
                reader.reset(s_count_buf_len);
                bool held_cr = false;
                while (true)
                {
                    auto [ptr, len] = reader.get_buf(s_count_buf_len);
                    if (len == 0)
                        break;
                    if (held_cr && len == 1)
                    {
                        ++m_accu_len;   // the \r ends the data
                        break;
                    }
                    const auto res = translate<false>(ptr, len, nullptr, len);
                    m_accu_len += res.produced;
                    held_cr = res.held_cr;
                    if (res.consumed != len)
                        reader.rollback(len - res.consumed);
                }
            }
            catch (...)
            {
                BT::set_tainted();
                throw;
            }

            // A repositioning drops an error a previous get held back; reading did not, so
            // seek where the layer below already stands. On failure nothing has changed.
            BT::kernel_seek(BT::kernel_tell());
        }
    }

private:
    std::size_t m_accu_len = 0;             ///< 逻辑位置（本层字符数）/ Logical position (characters of this layer).
    at_end      m_at_end = at_end::unknown; ///< 写状态下是否处于末尾 / Whether at the end, while writing.
};

/**
 * @lang{ZH}
 * `newline_cvt` 的工厂类。无状态；字符类型由 `create()` 的实参推导。满足 `cvt_creator` 概念。
 * @endif
 *
 * @lang{EN}
 * Factory class for `newline_cvt`. Stateless; the character type is deduced from the
 * argument of `create()`. Satisfies the `cvt_creator` concept.
 * @endif
 */
class newline_cvt_creator
{
public:
    using category = CvtCreatorCategory;

    /**
     * @lang{ZH} 为下层转换器创建一个 `newline_cvt`。 @endif
     * @lang{EN} Create a `newline_cvt` on top of the converter below. @endif
     */
    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return newline_cvt<std::remove_cvref_t<TKernel>>{std::forward<TKernel>(kernel)};
    }
};
}

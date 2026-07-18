/**
 * @file io_base.h
 * @lang{ZH}
 * 定义了 I/O 流体系的基础设施，包括：
 * - `ios_defs`：格式化标志（`fmtflags`）与流状态位（`iostate`）的定义。
 * - `io_state_and_exp`：管理流状态位以及与之关联的异常（异常掩码 / 异常指针）。
 * - `ios_base`：所有流的格式化状态基类，持有格式标志、精度、宽度、填充字符、
 *   pword 存储以及本地化变更回调。
 * - 一组用于修改流格式的操纵符（manipulator），如 `hex`、`left`、`fixed` 等。
 * - `sync`：对流的 I/O 互斥量进行 RAII 加锁的辅助类。
 * @endif
 *
 * @lang{EN}
 * Defines the infrastructure for the I/O stream hierarchy, including:
 * - `ios_defs`: definitions of the formatting flags (`fmtflags`) and stream state
 *   bits (`iostate`).
 * - `io_state_and_exp`: manages the stream state bits together with their associated
 *   exceptions (exception mask / exception pointers).
 * - `ios_base`: the base class holding the formatting state of every stream: format
 *   flags, precision, width, fill character, pword storage, and locale-change callbacks.
 * - A set of manipulators that modify a stream's formatting, such as `hex`, `left`,
 *   `fixed`, etc.
 * - `sync`: an RAII helper that locks a stream's I/O mutex.
 * @endif
 */
#pragma once
#include <common/copyable_atomic.h>
#include <common/copyable_mutex.h>
#include <common/defs.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <forward_list>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 存放流格式化标志与流状态位定义的命名空间。
 *
 * 类比于标准库的 `std::ios_base` 内嵌类型，本命名空间集中定义了控制格式化行为的
 * `fmtflags` 位标志，以及描述流健康状况的 `iostate` 状态位。
 * @endif
 *
 * @lang{EN}
 * @brief Namespace holding the definitions of the stream formatting flags and stream
 * state bits.
 *
 * Analogous to the nested types of the standard `std::ios_base`, this namespace
 * gathers the `fmtflags` bit flags that control formatting behaviour together with
 * the `iostate` bits that describe the health of a stream.
 * @endif
 */
namespace ios_defs
{
    /**
     * @lang{ZH} 格式化标志的位掩码类型（16 位）。 @endif
     * @lang{EN} Bitmask type for the formatting flags (16 bits). @endif
     */
    using fmtflags = std::uint16_t;
    /**
     * @lang{ZH} 流状态位的位掩码类型（8 位）。 @endif
     * @lang{EN} Bitmask type for the stream state bits (8 bits). @endif
     */
    using iostate  = std::uint8_t;

    constexpr static fmtflags boolalpha   = 1L << 0;    ///< @lang{ZH} 以文本形式（`true`/`false`）而非数字形式表示布尔值。 @endif @lang{EN} Represent bool values as text (`true`/`false`) instead of numbers. @endif
    constexpr static fmtflags dec         = 1L << 1;    ///< @lang{ZH} 整数以十进制表示（`basefield` 之一）。 @endif @lang{EN} Integers in decimal (one of `basefield`). @endif
    constexpr static fmtflags fixed       = 1L << 2;    ///< @lang{ZH} 浮点数以定点记法表示（`floatfield` 之一）。 @endif @lang{EN} Floating-point in fixed notation (one of `floatfield`). @endif
    constexpr static fmtflags hex         = 1L << 3;    ///< @lang{ZH} 整数以十六进制表示（`basefield` 之一）。 @endif @lang{EN} Integers in hexadecimal (one of `basefield`). @endif
    constexpr static fmtflags internal    = 1L << 4;    ///< @lang{ZH} 在符号/进制前缀与数值之间填充（`adjustfield` 之一）。 @endif @lang{EN} Pad between the sign/base prefix and the value (one of `adjustfield`). @endif
    constexpr static fmtflags left        = 1L << 5;    ///< @lang{ZH} 左对齐，在右侧填充（`adjustfield` 之一）。 @endif @lang{EN} Left-justify, padding on the right (one of `adjustfield`). @endif
    constexpr static fmtflags oct         = 1L << 6;    ///< @lang{ZH} 整数以八进制表示（`basefield` 之一）。 @endif @lang{EN} Integers in octal (one of `basefield`). @endif
    constexpr static fmtflags right       = 1L << 7;    ///< @lang{ZH} 右对齐，在左侧填充（`adjustfield` 之一）。 @endif @lang{EN} Right-justify, padding on the left (one of `adjustfield`). @endif
    constexpr static fmtflags scientific  = 1L << 8;    ///< @lang{ZH} 浮点数以科学记法表示（`floatfield` 之一）。 @endif @lang{EN} Floating-point in scientific notation (one of `floatfield`). @endif
    constexpr static fmtflags showbase    = 1L << 9;    ///< @lang{ZH} 输出整数时显示进制前缀（如 `0x`、`0`）。 @endif @lang{EN} Show the numeric base prefix (e.g. `0x`, `0`) on integer output. @endif
    constexpr static fmtflags showpoint   = 1L << 10;   ///< @lang{ZH} 浮点数输出总是显示小数点。 @endif @lang{EN} Always show the decimal point on floating-point output. @endif
    constexpr static fmtflags showpos     = 1L << 11;   ///< @lang{ZH} 对非负数显示正号 `+`。 @endif @lang{EN} Show a leading `+` on non-negative numbers. @endif
    constexpr static fmtflags skipws      = 1L << 12;   ///< @lang{ZH} 输入时跳过前导空白。 @endif @lang{EN} Skip leading whitespace on input. @endif
    constexpr static fmtflags unitbuf     = 1L << 13;   ///< @lang{ZH} 每次输出操作后刷新缓冲区。 @endif @lang{EN} Flush the buffer after each output operation. @endif
    constexpr static fmtflags uppercase   = 1L << 14;   ///< @lang{ZH} 在数值输出中使用大写字母（如 `0X`、`1E5`）。 @endif @lang{EN} Use uppercase letters in numeric output (e.g. `0X`, `1E5`). @endif
    constexpr static fmtflags appmode     = 1L << 15;   ///< @lang{ZH} 追加模式标志（本库扩展）。 @endif @lang{EN} Append-mode flag (a library extension). @endif
    constexpr static fmtflags adjustfield = left | right | internal;    ///< @lang{ZH} 对齐字段掩码：`left`、`right`、`internal`。 @endif @lang{EN} Adjustment field mask: `left`, `right`, `internal`. @endif
    constexpr static fmtflags basefield   = dec | oct | hex;            ///< @lang{ZH} 整数进制字段掩码：`dec`、`oct`、`hex`。 @endif @lang{EN} Integer base field mask: `dec`, `oct`, `hex`. @endif
    constexpr static fmtflags floatfield  = scientific | fixed;         ///< @lang{ZH} 浮点记法字段掩码：`scientific`、`fixed`。 @endif @lang{EN} Floating-point notation field mask: `scientific`, `fixed`. @endif

    constexpr static iostate goodbit        = 0;        ///< @lang{ZH} 无错误状态（所有状态位清零）。 @endif @lang{EN} No-error state (all state bits clear). @endif
    constexpr static iostate eofbit         = 1L << 0;  ///< @lang{ZH} 已到达输入序列末尾。 @endif @lang{EN} End of the input sequence has been reached. @endif
    constexpr static iostate devfailbit     = 1L << 1;  ///< @lang{ZH} 底层设备操作失败。 @endif @lang{EN} An underlying device operation failed. @endif
    constexpr static iostate cvtfailbit     = 1L << 2;  ///< @lang{ZH} 字符编码转换失败。 @endif @lang{EN} A character-encoding conversion failed. @endif
    constexpr static iostate strfailbit     = 1L << 3;  ///< @lang{ZH} 流层面的格式化/解析失败。 @endif @lang{EN} A stream-level formatting/parsing failure. @endif
    constexpr static iostate otherfailbit   = 1L << 4;  ///< @lang{ZH} 其他（未归类）失败。 @endif @lang{EN} Some other (uncategorized) failure. @endif
};

/**
 * @lang{ZH}
 * @brief 管理流状态位及其关联异常的组件。
 *
 * 本类同时维护两部分信息：
 * - **状态位**（`m_stream_state`）：当前流的健康状况，由 `ios_defs::iostate` 位组成。
 * - **异常掩码**（`m_exception`）：指定哪些状态位一旦被置位就应当抛出异常。
 *
 * 与标准 `std::ios` 不同，本类为每个失败类别额外保存了一个 `std::exception_ptr`
 * （设备/转换/流/其他），因此当某个失败位对应的异常被触发时，可以**重新抛出最初
 * 捕获的原始异常**，而不是抛出一个信息量更少的通用异常。当对应异常指针为空时，则回退
 * 到抛出一个与该失败类别匹配的默认异常。
 * @endif
 *
 * @lang{EN}
 * @brief Component that manages the stream state bits and their associated exceptions.
 *
 * This class maintains two pieces of information at once:
 * - The **state bits** (`m_stream_state`): the current health of the stream, made of
 *   `ios_defs::iostate` bits.
 * - The **exception mask** (`m_exception`): which state bits, once set, should cause an
 *   exception to be thrown.
 *
 * Unlike the standard `std::ios`, this class additionally stores a `std::exception_ptr`
 * per failure category (device / conversion / stream / other), so that when the
 * exception for a failure bit fires it can **rethrow the original exception that was
 * first captured** rather than a less informative generic one. When the corresponding
 * exception pointer is empty it falls back to throwing a default exception matching that
 * failure category.
 * @endif
 */
struct io_state_and_exp
{
    /**
     * @lang{ZH}
     * @brief 返回当前的流状态位。
     * @return 当前 `iostate` 位的按位或。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the current stream state bits.
     * @return The bitwise-or of the current `iostate` bits.
     * @endif
     */
    [[nodiscard]] ios_defs::iostate rdstate() const { return m_stream_state.load(); }

    /**
     * @lang{ZH}
     * @brief 将流状态位设置为 `s`，并按需触发异常。
     *
     * 先把状态位整体替换为 `s`。对于被清除（不再置位）的失败位，其保存的
     * `std::exception_ptr` 会被一并释放。随后，若 `s` 中仍被置位的失败位同时也在异常
     * 掩码 `exceptions()` 中，则触发异常：优先按 设备 → 转换 → 流 → 其他 → EOF 的顺序，
     * 重新抛出对应类别先前保存的原始异常；若该指针为空，则抛出与该类别匹配的默认异常。
     *
     * @param s 新的流状态位，默认为 `goodbit`（即清除所有状态）。
     * @throw device_error 当 `devfailbit` 被置位且在异常掩码中，且无保存的原始异常时。
     * @throw cvt_error 当 `cvtfailbit` 被置位且在异常掩码中，且无保存的原始异常时。
     * @throw stream_error 当 `strfailbit`/`otherfailbit` 被置位且在异常掩码中，
     *        且无保存的原始异常时。
     * @throw eof_error 当 `eofbit` 被置位且在异常掩码中时。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the stream state bits to `s`, triggering exceptions as required.
     *
     * The state bits are first replaced wholesale by `s`. For any failure bit that is
     * cleared (no longer set), its stored `std::exception_ptr` is released as well.
     * Then, for any failure bit still set in `s` that is also present in the exception
     * mask `exceptions()`, an exception is triggered: in the order
     * device → conversion → stream → other → EOF, the original exception previously
     * captured for that category is rethrown; if that pointer is empty, a default
     * exception matching the category is thrown instead.
     *
     * @param s The new stream state bits; defaults to `goodbit` (i.e. clears all state).
     * @throw device_error When `devfailbit` is set and in the exception mask, with no
     *        stored original exception.
     * @throw cvt_error When `cvtfailbit` is set and in the exception mask, with no stored
     *        original exception.
     * @throw stream_error When `strfailbit`/`otherfailbit` is set and in the exception
     *        mask, with no stored original exception.
     * @throw eof_error When `eofbit` is set and in the exception mask.
     * @endif
     */
    void clear(ios_defs::iostate s = ios_defs::goodbit)
    {
        std::lock_guard guard(m_state_mutex);
        m_stream_state.store(s);
        if ((s & ios_defs::devfailbit) == ios_defs::goodbit) m_exp_dev_fail = std::exception_ptr{};
        if ((s & ios_defs::cvtfailbit) == ios_defs::goodbit) m_exp_cvt_fail = std::exception_ptr{};
        if ((s & ios_defs::strfailbit) == ios_defs::goodbit) m_exp_str_fail = std::exception_ptr{};
        if ((s & ios_defs::otherfailbit) == ios_defs::goodbit) m_exp_other_fail = std::exception_ptr{};

        ios_defs::iostate state_in_exp = m_exception & s;
        if (state_in_exp & ios_defs::devfailbit)
        {
            if (m_exp_dev_fail)
                std::rethrow_exception(std::exchange(m_exp_dev_fail, nullptr));
            else
                throw device_error("device failure bit has been set");
        }
        else if (state_in_exp & ios_defs::cvtfailbit)
        {
            if (m_exp_cvt_fail)
                std::rethrow_exception(std::exchange(m_exp_cvt_fail, nullptr));
            else
                throw cvt_error("converter failure bit has been set");
        }
        else if (state_in_exp & ios_defs::strfailbit)
        {
            if (m_exp_str_fail)
                std::rethrow_exception(std::exchange(m_exp_str_fail, nullptr));
            else
                throw stream_error("stream failure bit has been set");
        }
        else if (state_in_exp & ios_defs::otherfailbit)
        {
            if (m_exp_other_fail)
                std::rethrow_exception(std::exchange(m_exp_other_fail, nullptr));
            else
                throw stream_error("other failure bit has been set");
        }
        else if (state_in_exp & ios_defs::eofbit)
        {
            throw eof_error{};
        }
    }

    /**
     * @lang{ZH}
     * @brief 在现有状态位的基础上附加 `s`（按位或），并可能触发异常。
     * @param s 要附加置位的状态位。
     * @note 等价于 `clear(rdstate() | s)`，因此同样受异常掩码影响，见 clear()。
     * @endif
     *
     * @lang{EN}
     * @brief Adds `s` on top of the existing state bits (bitwise-or), possibly
     * triggering exceptions.
     * @param s The state bits to additionally set.
     * @note Equivalent to `clear(rdstate() | s)`, so it is likewise subject to the
     * exception mask; see clear().
     * @endif
     */
    void setstate(ios_defs::iostate s)
    {
        std::lock_guard guard(m_state_mutex);
        clear(rdstate() | s);
    }

    void unset_state(ios_defs::iostate s)
    {
        std::lock_guard guard(m_state_mutex);
        clear(rdstate() & ~s);
    }
    /**
     * @lang{ZH} @brief 是否无任何错误（状态位全为 0）。 @endif
     * @lang{EN} @brief Whether there is no error at all (state bits all zero). @endif
     */
    [[nodiscard]] bool good() const { return rdstate() == 0; }
    /**
     * @lang{ZH} @brief 是否置位了设备失败位。 @endif
     * @lang{EN} @brief Whether the device-failure bit is set. @endif
     */
    [[nodiscard]] bool dev_fail() const { return rdstate() & ios_defs::devfailbit; }
    /**
     * @lang{ZH} @brief 是否置位了转换失败位。 @endif
     * @lang{EN} @brief Whether the conversion-failure bit is set. @endif
     */
    [[nodiscard]] bool cvt_fail() const { return rdstate() & ios_defs::cvtfailbit; }
    /**
     * @lang{ZH} @brief 是否置位了流失败位。 @endif
     * @lang{EN} @brief Whether the stream-failure bit is set. @endif
     */
    [[nodiscard]] bool str_fail() const { return rdstate() & ios_defs::strfailbit; }
    /**
     * @lang{ZH} @brief 是否置位了其他失败位。 @endif
     * @lang{EN} @brief Whether the other-failure bit is set. @endif
     */
    [[nodiscard]] bool other_fail() const { return rdstate() & ios_defs::otherfailbit; }
    /**
     * @lang{ZH} @brief 是否已到达文件/输入末尾（`eofbit` 置位）。 @endif
     * @lang{EN} @brief Whether end-of-file/input has been reached (`eofbit` set). @endif
     */
    [[nodiscard]] bool eof() const { return rdstate() & ios_defs::eofbit; }

    /**
     * @lang{ZH}
     * @brief 布尔转换：当流仍可用时为 `true`。
     *
     * 仅当状态为 `goodbit`，或仅置位了 `eofbit`（无任何失败位）时返回 `true`。
     * 换言之，只要出现任何失败位即返回 `false`；单纯的 EOF 不视为不可用。
     * @endif
     *
     * @lang{EN}
     * @brief Boolean conversion: `true` while the stream is still usable.
     *
     * Returns `true` only when the state is `goodbit`, or when only `eofbit` is set
     * (without any failure bit). In other words, any failure bit makes it `false`;
     * plain EOF alone is not treated as unusable.
     * @endif
     */
    explicit operator bool() const
    {
        const ios_defs::iostate s = rdstate();
        return (s == 0) || (s == ios_defs::eofbit);
    }

    /**
     * @lang{ZH} @brief 返回当前的异常掩码。 @endif
     * @lang{EN} @brief Returns the current exception mask. @endif
     */
    [[nodiscard]] ios_defs::iostate exceptions() const
    {
        std::lock_guard guard(m_state_mutex);
        return m_exception;
    }
    /**
     * @lang{ZH}
     * @brief 设置异常掩码，指定哪些状态位应触发异常。
     *
     * 设置后会立即以当前状态位调用 `clear(m_stream_state)`，因此若当前已置位的状态位
     * 落入新掩码，会**立刻**触发相应异常。
     * @param e 新的异常掩码。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the exception mask that specifies which state bits should throw.
     *
     * After setting, `clear(m_stream_state)` is immediately called with the current
     * state bits, so if an already-set state bit falls within the new mask the
     * corresponding exception fires **immediately**.
     * @param e The new exception mask.
     * @endif
     */
    void exceptions(ios_defs::iostate e)
    {
        std::lock_guard guard(m_state_mutex);
        m_exception = e;
        clear(m_stream_state.load());
    }

    /**
     * @lang{ZH}
     * @brief 捕获并归类一个异常，将其转换为对应的失败状态位。
     *
     * 重新抛出 `ex` 并按类型归类：`device_error`→`devfailbit`、`cvt_error`→`cvtfailbit`、
     * `stream_error`→`strfailbit`、`eof_error`→`eofbit`、其余→`otherfailbit`。对于设备/
     * 转换/流/其他类别，若该类别尚无保存的异常指针，则记录当前异常，以便日后经 clear()/
     * setstate() 重新抛出原始异常。随后通过 `setstate()` 置位相应状态位——**这本身可能再次
     * 抛出异常**（若该位在异常掩码中）。
     *
     * @param ex 要处理的异常指针；若为空指针则不做任何事。
     * @note EOF 类别不保存异常指针（EOF 无需携带原始异常信息）。
     * @warning **不支持对阻塞在本库 I/O 中的线程调用 `pthread_cancel`。** glibc 以抛出特殊异常
     *          （`__cxxabiv1::__forced_unwind`）的方式实现线程取消，该异常会落入本函数最后的
     *          `catch(...)` 并被归类为 `otherfailbit`；若 `otherfailbit` 不在异常掩码中就不会被
     *          重新抛出，取消因而被吞掉，导致 `FATAL: exception not rethrown` 并 abort。线程取消
     *          不属于 C++ 标准，也没有可移植的手段在 `catch(...)` 中将其识别出来。若需中断阻塞中
     *          的 I/O，请改为关闭底层设备（使阻塞调用带错误返回），或使用超时 / 非阻塞 I/O。
     * @endif
     *
     * @lang{EN}
     * @brief Captures and categorizes an exception, turning it into the matching
     * failure state bit.
     *
     * Rethrows `ex` and categorizes it by type: `device_error`→`devfailbit`,
     * `cvt_error`→`cvtfailbit`, `stream_error`→`strfailbit`, `eof_error`→`eofbit`,
     * everything else→`otherfailbit`. For the device/conversion/stream/other categories,
     * if that category has no stored exception pointer yet, the current exception is
     * recorded so the original can later be rethrown via clear()/setstate(). It then
     * sets the matching state bit through `setstate()` — which **may itself throw again**
     * (if that bit is in the exception mask).
     *
     * @param ex The exception pointer to handle; does nothing if it is null.
     * @note The EOF category stores no exception pointer (EOF carries no original
     * exception information).
     * @warning **Calling `pthread_cancel` on a thread blocked inside this library is not
     *          supported.** glibc implements thread cancellation by throwing a special
     *          exception (`__cxxabiv1::__forced_unwind`), which lands in this function's
     *          final `catch(...)` and is categorized as `otherfailbit`; unless
     *          `otherfailbit` is in the exception mask it is not rethrown, so the
     *          cancellation is swallowed and the process aborts with
     *          `FATAL: exception not rethrown`. Thread cancellation is not part of the C++
     *          standard, and there is no portable way to recognize it inside a `catch(...)`.
     *          To interrupt blocked I/O, close the underlying device instead (so the
     *          blocking call returns with an error), or use timeouts / non-blocking I/O.
     * @endif
     */
    void handle_exception(const std::exception_ptr& ex, bool at_eof = false)
    {
        if (!ex) return;
        std::lock_guard guard(m_state_mutex);
        const ios_defs::iostate eof = at_eof ? ios_defs::eofbit : ios_defs::goodbit;
        try
        {
            std::rethrow_exception(ex);
        }
        catch (device_error&)
        {
            if (!m_exp_dev_fail)
                m_exp_dev_fail = std::current_exception();
            setstate(ios_defs::devfailbit | eof);
        }
        catch (cvt_error&)
        {
            if (!m_exp_cvt_fail)
                m_exp_cvt_fail = std::current_exception();
            setstate(ios_defs::cvtfailbit | eof);
        }
        catch (stream_error&)
        {
            if (!m_exp_str_fail)
                m_exp_str_fail = std::current_exception();
            setstate(ios_defs::strfailbit | eof);
        }
        catch (eof_error&)
        {
            setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            if (!m_exp_other_fail)
                m_exp_other_fail = std::current_exception();
            setstate(ios_defs::otherfailbit | eof);
        }
    }

private:
    mutable copyable_mutex<std::recursive_mutex> m_state_mutex;  ///< @lang{ZH} 串行化以下全部成员的**写**：状态位与其 `exception_ptr` 构成一个多字不变式，须整体加锁。递归形态因内部存在同对象嵌套（如 `setstate()` → `clear()`）。 @endif @lang{EN} Serializes all **writes** to the members below: the state bits and their `exception_ptr`s form one multi-word invariant that must be updated as a whole. Recursive because this component nests on itself (e.g. `setstate()` → `clear()`). @endif

    ios_defs::iostate  m_exception = ios_defs::goodbit;      ///< @lang{ZH} 异常掩码：哪些状态位应触发异常。 @endif @lang{EN} Exception mask: which state bits should throw. @endif
    copyable_atomic<ios_defs::iostate> m_stream_state{ios_defs::goodbit};   ///< @lang{ZH} 当前流状态位。原子量，使 `rdstate()` 及 `good()`/`eof()`/`operator bool` 等热路径无锁读取；其**写**仍与其余成员一同在 `m_state_mutex` 下完成，故不变式不受影响。 @endif @lang{EN} Current stream state bits. Atomic so that `rdstate()` -- and the hot `good()`/`eof()`/`operator bool` built on it -- reads lock-free; **writes** still happen under `m_state_mutex` together with the other members, so the invariant is unaffected. @endif
    std::exception_ptr m_exp_dev_fail = std::exception_ptr{};    ///< @lang{ZH} 设备失败类别保存的原始异常。 @endif @lang{EN} Original exception saved for the device-failure category. @endif
    std::exception_ptr m_exp_cvt_fail = std::exception_ptr{};    ///< @lang{ZH} 转换失败类别保存的原始异常。 @endif @lang{EN} Original exception saved for the conversion-failure category. @endif
    std::exception_ptr m_exp_str_fail = std::exception_ptr{};    ///< @lang{ZH} 流失败类别保存的原始异常。 @endif @lang{EN} Original exception saved for the stream-failure category. @endif
    std::exception_ptr m_exp_other_fail = std::exception_ptr{};  ///< @lang{ZH} 其他失败类别保存的原始异常。 @endif @lang{EN} Original exception saved for the other-failure category. @endif
};

template <typename TChar> class locale;

template <typename TChar> class ios_base;

/**
 * @lang{ZH}
 * @brief `ios_base` 的 `void` 特化，承载所有字符类型共享的全局状态。
 *
 * 该特化不面向用户使用，仅作为进程内全局计数器 `s_top` 的宿主——`xalloc()` 借助它发放
 * 全局唯一的存储索引。将其独立于 `TChar` 之外，可保证不同字符类型实例化的 `ios_base`
 * 共用同一个索引空间。
 * @endif
 *
 * @lang{EN}
 * @brief The `void` specialization of `ios_base`, carrying global state shared by all
 * character types.
 *
 * This specialization is not for user consumption; it merely hosts the process-wide
 * global counter `s_top`, which `xalloc()` uses to hand out globally unique storage
 * indices. Keeping it independent of `TChar` ensures that `ios_base` instantiated for
 * different character types share a single index space.
 * @endif
 */
template <>
class ios_base<void>
{
    template <typename>
    friend class ios_base;

    inline static std::atomic<size_t> s_top = 0;
};

/**
 * @lang{ZH}
 * @brief 所有流的格式化状态基类。
 *
 * `ios_base` 持有一个流的全部格式化状态：格式标志（`fmtflags`）、浮点精度、字段宽度、
 * 填充字符，以及可扩展的 per-stream 用户存储（pword）和本地化变更回调。它对应标准库中
 * `std::ios_base` 与 `std::basic_ios` 中与格式化相关的部分，但按本库的设计做了取舍
 * （例如可拷贝、精度/宽度以 8 位存储等，详见各成员说明）。
 *
 * @note 本类不涉及流状态位（good/eof/fail），那些由 `io_state_and_exp` 负责。
 *
 * @tparam TChar 字符类型（如 `char`、`wchar_t` 等）。用于填充字符与本地化回调的签名。
 * @endif
 *
 * @lang{EN}
 * @brief Base class holding the formatting state of every stream.
 *
 * `ios_base` holds all of a stream's formatting state: the format flags (`fmtflags`),
 * floating-point precision, field width, fill character, plus extensible per-stream
 * user storage (pword) and locale-change callbacks. It corresponds to the
 * formatting-related parts of `std::ios_base` and `std::basic_ios`, but with design
 * choices specific to this library (e.g. it is copyable, precision/width are stored in
 * 8 bits — see the individual members for details).
 *
 * @note This class does not deal with the stream state bits (good/eof/fail); those are
 * handled by `io_state_and_exp`.
 *
 * @tparam TChar The character type (e.g. `char`, `wchar_t`). Used by the fill character
 * and the locale-callback signature.
 * @endif
 */
template <typename TChar>
class ios_base
{
public:
    /**
     * @lang{ZH}
     * @brief 本地化变更回调的类型。
     *
     * 当本地化（locale）发生变更时，`access_callbacks()` 会以新 locale 及该回调对应 id
     * 上已存的 pword 数据（可能为空）调用此函数，回调返回的新数据将回写为该 id 的 pword
     * （返回空指针表示删除该条目）。
     * @endif
     *
     * @lang{EN}
     * @brief Type of a locale-change callback.
     *
     * When the locale changes, `access_callbacks()` invokes this callable with the new
     * locale and the pword data currently stored under the callback's id (possibly
     * null); the new data returned by the callback is written back as the pword for that
     * id (returning a null pointer removes the entry).
     * @endif
     */
    using event_callback = std::function<std::shared_ptr<void>(const locale<TChar>&, std::shared_ptr<void>)>;

public:
    /**
     * @lang{ZH} @brief 默认构造，使用默认的格式化状态。 @endif
     * @lang{EN} @brief Default constructor, with the default formatting state. @endif
     */
    ios_base() = default;

    /**
     * @lang{ZH}
     * @brief 拷贝/移动构造与赋值。
     *
     * 拷贝/移动均被有意支持（与不可拷贝的 std::ios_base 不同，此为本库的既定设计）。
     * @note 拷贝为浅共享语义：m_pwords 与 m_callbacks 本身按值复制（两个对象各自持有
     * 独立的容器），但 m_pwords 中存放的是 std::shared_ptr<void>，因此拷贝后两个 ios_base
     * 实例的 pword 条目**共享同一批底层对象**——通过任一实例经 set_pword() 替换某 id 的条目
     * 只影响自身的容器，但只要该条目未被替换，两侧解引用得到的仍是同一 pword 对象，经其中
     * 一侧修改该对象内容对另一侧可见。若需要独立的 pword，请在拷贝后自行深拷贝相应对象。
     * @endif
     *
     * @lang{EN}
     * @brief Copy/move construction and assignment.
     *
     * Copy and move are intentionally supported (unlike the non-copyable std::ios_base;
     * this is a deliberate design choice of this library).
     * @note Copy has shallow-sharing semantics: m_pwords and m_callbacks are copied by
     * value (each instance owns an independent container), but m_pwords stores
     * std::shared_ptr<void>, so after a copy the two ios_base instances' pword entries
     * **share the same underlying objects**. Replacing an id's entry via set_pword() on
     * one instance affects only that instance's own container, but as long as the entry
     * is not replaced, dereferencing it on either side yields the same pword object, and
     * mutations to that object made through one side are visible to the other. If
     * independent pwords are required, deep-copy the relevant objects after copying.
     * @endif
     */
    ios_base(const ios_base&) = default;
    ios_base(ios_base&&) = default;
    ios_base& operator=(const ios_base&) = default;
    ios_base& operator=(ios_base&&) = default;

public:
    /**
     * @lang{ZH} @brief 返回当前的格式化标志。 @endif
     * @lang{EN} @brief Returns the current formatting flags. @endif
     */
    ios_defs::fmtflags flags() const { return m_flags; }
    /**
     * @lang{ZH}
     * @brief 将格式化标志整体替换为 `fmtfl`。
     * @param fmtfl 新的格式化标志。
     * @return 替换前的旧标志。
     * @endif
     *
     * @lang{EN}
     * @brief Replaces the formatting flags wholesale with `fmtfl`.
     * @param fmtfl The new formatting flags.
     * @return The old flags before replacement.
     * @endif
     */
    ios_defs::fmtflags flags(ios_defs::fmtflags fmtfl)
    {
      ios_defs::fmtflags old = m_flags;
      m_flags = fmtfl;
      return old;
    }

    /**
     * @lang{ZH}
     * @brief 置位 `fmtfl` 中的标志（按位或）。
     * @param fmtfl 要附加置位的标志。
     * @return 修改前的旧标志。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the flags in `fmtfl` (bitwise-or).
     * @param fmtfl The flags to additionally set.
     * @return The old flags before modification.
     * @endif
     */
    ios_defs::fmtflags setf(ios_defs::fmtflags fmtfl)
    {
      ios_defs::fmtflags old = m_flags;
      m_flags |= fmtfl;
      return old;
    }

    /**
     * @lang{ZH}
     * @brief 在掩码 `msk` 限定的位域内，将标志设置为 `fmtfl`。
     *
     * 先清除 `msk` 覆盖的所有位，再置入 `fmtfl & msk`。常用于设置 `basefield`、
     * `adjustfield`、`floatfield` 等互斥字段。
     * @param fmtfl 要设置的标志（仅 `msk` 内的位生效）。
     * @param msk 限定生效范围的掩码。
     * @return 修改前的旧标志。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the flags to `fmtfl` within the bit field limited by the mask `msk`.
     *
     * First clears every bit covered by `msk`, then sets `fmtfl & msk`. Commonly used to
     * set mutually exclusive fields such as `basefield`, `adjustfield`, `floatfield`.
     * @param fmtfl The flags to set (only the bits within `msk` take effect).
     * @param msk The mask limiting the affected range.
     * @return The old flags before modification.
     * @endif
     */
    ios_defs::fmtflags setf(ios_defs::fmtflags fmtfl, ios_defs::fmtflags msk)
    {
      ios_defs::fmtflags old = m_flags;
      m_flags &= ~msk;
      m_flags |= (fmtfl & msk);
      return old;
    }

    /**
     * @lang{ZH}
     * @brief 清除 `msk` 中的标志。
     * @param msk 要清除的标志掩码。
     * @endif
     *
     * @lang{EN}
     * @brief Clears the flags in `msk`.
     * @param msk The mask of flags to clear.
     * @endif
     */
    void unsetf(ios_defs::fmtflags msk) { m_flags &= ~msk; }

    /**
     * @lang{ZH}
     * @brief 获取/设置浮点精度。
     *
     * 无参重载返回当前精度；带参重载将精度设置为 @p prec 并返回旧值。
     * @note 精度以 std::uint8_t 存储，取值范围被有意限制在 0..255。这与标准
     * std::ios_base 使用 std::streamsize 不同：本库不支持大于 255 的精度，
     * 更大的值无法通过本接口表达（参数类型即为 std::uint8_t）。此为有意设计。
     * @endif
     *
     * @lang{EN}
     * @brief Get/set the floating-point precision.
     *
     * The argument-less overload returns the current precision; the overload taking an
     * argument sets the precision to @p prec and returns the old value.
     * @note The precision is stored as a std::uint8_t and is intentionally limited
     * to the range 0..255. Unlike the standard std::ios_base, which uses
     * std::streamsize, this library does not support precision values greater than
     * 255; larger values cannot be expressed through this interface (the parameter
     * type is std::uint8_t itself). This is by design.
     * @endif
     */
    std::uint8_t precision() const { return m_precision; }
    std::uint8_t precision(std::uint8_t prec)
    {
        std::uint8_t old = m_precision;
        m_precision = prec;
        return old;
    }

    /**
     * @lang{ZH}
     * @brief 获取/设置字段宽度。
     *
     * 无参重载返回当前宽度；带参重载将宽度设置为 @p wide 并返回旧值。
     * @note 宽度以 std::uint8_t 存储，取值范围被有意限制在 0..255。这与标准
     * std::ios_base 使用 std::streamsize 不同：本库不支持大于 255 的字段宽度，
     * 更大的值无法通过本接口表达（参数类型即为 std::uint8_t）。此为有意设计。
     * @endif
     *
     * @lang{EN}
     * @brief Get/set the field width.
     *
     * The argument-less overload returns the current width; the overload taking an
     * argument sets the width to @p wide and returns the old value.
     * @note The width is stored as a std::uint8_t and is intentionally limited to
     * the range 0..255. Unlike the standard std::ios_base, which uses
     * std::streamsize, this library does not support field widths greater than 255;
     * larger values cannot be expressed through this interface (the parameter type
     * is std::uint8_t itself). This is by design.
     * @endif
     */
    std::uint8_t width() const { return m_width; }
    std::uint8_t width(std::uint8_t wide)
    {
        std::uint8_t old = m_width;
        m_width = wide;
        return old;
    }

    /**
     * @lang{ZH}
     * @brief 获取/设置填充字符。
     *
     * 无参重载返回当前填充字符；带参重载将其设置为 @p ch 并返回旧值。
     * 填充字符用于在字段宽度大于内容时补齐输出。
     * @endif
     *
     * @lang{EN}
     * @brief Get/set the fill character.
     *
     * The argument-less overload returns the current fill character; the overload taking
     * an argument sets it to @p ch and returns the old value. The fill character is used
     * to pad output when the field width exceeds the content.
     * @endif
     */
    TChar fill() const noexcept { return m_fill; }
    TChar fill(TChar ch)
    {
        TChar old = m_fill;
        m_fill = ch;
        return old;
    }

public:
    /**
     * @lang{ZH}
     * @brief 分配一个进程内唯一的存储索引。
     *
     * 返回的索引可作为 set_pword()/get_pword() 的 id，以及 register_callback() 中回调的
     * 关联 id，用于在流上存取用户自定义数据。
     * @note numeric_limits<size_t>::max() 被保留作为“已耗尽”哨兵：索引空间在发放到
     * max()-1 后即视为耗尽，再次调用抛出 stream_error，而不是让计数器回绕并复用已发放
     * 的索引。采用 CAS 循环实现，保证并发调用下索引唯一且不越过耗尽点。
     * @endif
     *
     * @lang{EN}
     * @brief Allocates a process-wide unique storage index.
     *
     * The returned index can be used as the id for set_pword()/get_pword() and as the
     * association id for callbacks in register_callback(), to store and retrieve
     * user-defined data on a stream.
     * @note numeric_limits<size_t>::max() is reserved as an "exhausted" sentinel: once
     * indices up to max()-1 have been handed out the space is considered exhausted and
     * a further call throws stream_error, instead of letting the counter wrap around
     * and reuse an already-issued index. Implemented with a CAS loop so that concurrent
     * calls stay unique and never step past the exhaustion point.
     * @endif
     *
     * @throws stream_error
     * @lang{ZH} 索引空间耗尽时抛出。 @endif
     * @lang{EN} Thrown when the index space is exhausted. @endif
     */
    static size_t xalloc()
    {
        size_t cur = ios_base<void>::s_top.load(std::memory_order_relaxed);
        do
        {
            if (cur == std::numeric_limits<size_t>::max())
                throw stream_error("ios_base::xalloc fails: storage index space exhausted");
        } while (!ios_base<void>::s_top.compare_exchange_weak(
                     cur, cur + 1, std::memory_order_relaxed));
        return cur;
    }

    /**
     * @lang{ZH}
     * @brief 设置 id 对应的 pword（per-stream 用户数据）条目。
     *
     * 若 @p pword 非空，则设置/替换 @p id 处的条目；若为空，则删除该条目。
     * @param id 存储索引，一般由 xalloc() 获得。
     * @param pword 要存入的数据（`shared_ptr<void>`），为空表示删除。
     * @return 该 id 处替换或删除前的旧数据；若原本不存在则为 nullptr。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the pword (per-stream user data) entry for an id.
     *
     * If @p pword is non-null, the entry at @p id is set/replaced; if null, the entry is
     * removed.
     * @param id The storage index, typically obtained from xalloc().
     * @param pword The data to store (`shared_ptr<void>`); null means remove.
     * @return The old data at that id before replacement/removal; nullptr if none
     * existed.
     * @endif
     */
    std::shared_ptr<void> set_pword(size_t id, std::shared_ptr<void> pword)
    {
        if (auto it = m_pwords.find(id); it == m_pwords.end())
        {
            if (pword) m_pwords.emplace(id, std::move(pword));
            return nullptr;
        }
        else
        {
            auto res = std::move(it->second);
            if (pword) it->second = std::move(pword);
            else m_pwords.erase(it);
            return res;
        }
    }

    /**
     * @lang{ZH}
     * @brief 获取 id 对应的 pword 条目。
     * @param id 存储索引。
     * @return 该 id 处存储的数据；若不存在则为 nullptr。
     * @endif
     *
     * @lang{EN}
     * @brief Gets the pword entry for an id.
     * @param id The storage index.
     * @return The data stored at that id; nullptr if none exists.
     * @endif
     */
    std::shared_ptr<void> get_pword(size_t id) const
    {
        auto it = m_pwords.find(id);
        if (it != m_pwords.end()) return it->second;
        return nullptr;
    }

    /**
     * @lang{ZH}
     * @brief 注册一个本地化变更回调。
     *
     * 回调被前插到回调列表中，因此**后注册者先被调用**。每个回调关联一个 @p id，
     * 该 id 用于在 access_callbacks() 中定位其对应的 pword 数据。
     * @param fn 回调函数。
     * @param id 与该回调关联的存储索引。
     * @endif
     *
     * @lang{EN}
     * @brief Registers a locale-change callback.
     *
     * The callback is prepended to the callback list, so **the most recently registered
     * is invoked first**. Each callback is associated with an @p id used by
     * access_callbacks() to locate its corresponding pword data.
     * @param fn The callback function.
     * @param id The storage index associated with this callback.
     * @endif
     */
    void register_callback(event_callback fn, size_t id)
    {
        m_callbacks.push_front({std::move(fn), id});
    }

protected:
    /**
     * @lang{ZH}
     * @brief 依次调用所有已注册的本地化变更回调。
     *
     * 对每个回调，以新 locale 及其 id 上现存的 pword 数据（可能为空）调用之，并将返回的
     * 新数据回写为该 id 的 pword（返回空则删除该条目）。回调**可能重入**地通过 set_pword()
     * 修改 m_pwords，因此本函数在回调返回后会**重新定位**迭代器，以避免因容器重哈希/删除
     * 而导致的迭代器失效。
     *
     * @param new_loc 新的本地化对象。
     * @note 若某个回调抛出异常，会被暂存并继续执行其余回调；全部执行完毕后，重新抛出**第一个**
     *       捕获到的异常（后续异常被吞掉）。
     * @endif
     *
     * @lang{EN}
     * @brief Invokes every registered locale-change callback in turn.
     *
     * For each callback, it is invoked with the new locale and the pword data currently
     * stored under its id (possibly null), and the returned new data is written back as
     * that id's pword (returning null removes the entry). A callback **may reentrantly**
     * mutate m_pwords via set_pword(), so this function **re-locates** the iterator after
     * the callback returns, to avoid iterator invalidation caused by container
     * rehashing/erasure.
     *
     * @param new_loc The new locale object.
     * @note If a callback throws, the exception is held and the remaining callbacks still
     *       run; once all have run, the **first** captured exception is rethrown (later
     *       exceptions are swallowed).
     * @endif
     */
    void access_callbacks(const locale<TChar>& new_loc)
    {
        std::exception_ptr throw_exception = nullptr;

        for (const auto& [cb, id] : m_callbacks)
        {
            try
            {
                auto pre = m_pwords.find(id);
                std::shared_ptr<void> old_data =
                    (pre != m_pwords.end()) ? pre->second : nullptr;

                std::shared_ptr<void> new_data = cb(new_loc, old_data);

                // Re-locate after the callback: cb may reentrantly mutate
                // m_pwords (via set_pword), which can rehash/erase and would
                // invalidate an iterator held across the call.
                auto it = m_pwords.find(id);
                if (new_data)
                {
                    if (it != m_pwords.end()) it->second = std::move(new_data);
                    else m_pwords.insert({id, std::move(new_data)});
                }
                else if (it != m_pwords.end())
                {
                    m_pwords.erase(it);
                }
            }
            catch(...)
            {
                if (!throw_exception)
                    throw_exception = std::current_exception();
            }
        }

        if (throw_exception)
            std::rethrow_exception(throw_exception);
    }

protected:
    ios_defs::fmtflags m_flags     = ios_defs::skipws | ios_defs::dec;   ///< @lang{ZH} 格式化标志；默认置位 `skipws | dec`。 @endif @lang{EN} Formatting flags; defaults to `skipws | dec`. @endif
    std::uint8_t       m_precision = 6;     ///< @lang{ZH} 浮点精度，默认 6。 @endif @lang{EN} Floating-point precision, default 6. @endif
    std::uint8_t       m_width     = 0;     ///< @lang{ZH} 字段宽度，默认 0（不填充）。 @endif @lang{EN} Field width, default 0 (no padding). @endif
    /**
     * @lang{ZH}
     * @brief 默认填充字符。
     *
     * 此处直接用 C 风格转换 (TChar)' ' 得到，而**不**经由 locale 的 widen()
     * 之类的接口——这是有意为之：ios_base 不与 locale 建立直接依赖。
     * @note 这要求 TChar 可由 char 字面量 ' ' 构造/转换。对 char/wchar_t/char8_t/char16_t/
     * char32_t 均成立；若以不满足该要求的 TChar 实例化，则在此处编译期失败（而非静默产生
     * 非空格的填充字符）。此为既定约束，非缺陷。
     * @endif
     *
     * @lang{EN}
     * @brief Default fill character.
     *
     * Obtained directly via the C-style cast (TChar)' ', and
     * deliberately NOT through a locale facility such as widen(): ios_base intentionally
     * keeps no direct dependency on locale.
     * @note This requires TChar to be constructible/convertible from the char literal ' '.
     * That holds for char/wchar_t/char8_t/char16_t/char32_t; instantiating with a TChar
     * that does not meet this requirement fails to compile here (rather than silently
     * yielding a non-space fill character). This is a deliberate constraint, not a defect.
     * @endif
     */
    TChar              m_fill      = (TChar)' ';

    std::unordered_map<size_t, std::shared_ptr<void>> m_pwords;              ///< @lang{ZH} 按 id 索引的 per-stream 用户数据存储。 @endif @lang{EN} Per-stream user-data storage indexed by id. @endif
    std::forward_list<std::pair<event_callback, size_t>> m_callbacks;        ///< @lang{ZH} 已注册的本地化变更回调及其关联 id（前插，后注册者先调用）。 @endif @lang{EN} Registered locale-change callbacks with their associated ids (prepended; last registered runs first). @endif
};

/**
 * @lang{ZH}
 * @defgroup ios_manipulators 格式化操纵符
 * @brief 直接作用于 ios_base 的无参操纵符，用于置位或清除相应的格式化标志。
 *
 * 这些函数与标准库中同名的流操纵符对应，通过 setf()/unsetf() 修改 ios_base 的格式标志。
 * 其中作用于互斥字段（`adjustfield`/`basefield`/`floatfield`）的操纵符会先清除该字段
 * 再置位目标位。所有参数 @p base 均为要修改的 ios_base 对象。
 * @{
 * @endif
 *
 * @lang{EN}
 * @defgroup ios_manipulators Formatting manipulators
 * @brief Argument-less manipulators acting directly on ios_base to set or clear the
 * corresponding formatting flags.
 *
 * These functions correspond to the identically named stream manipulators of the
 * standard library and modify the format flags of ios_base via setf()/unsetf(). Those
 * acting on a mutually exclusive field (`adjustfield`/`basefield`/`floatfield`) first
 * clear the field and then set the target bit. In every case @p base is the ios_base
 * object to modify.
 * @{
 * @endif
 */

/** @lang{ZH} @brief 置位 `boolalpha`：布尔值以文本形式表示。 @endif @lang{EN} @brief Set `boolalpha`: represent bool values as text. @endif */
template <typename TChar>
inline void boolalpha(ios_base<TChar>& base)
{
    base.setf(ios_defs::boolalpha);
}

/** @lang{ZH} @brief 清除 `boolalpha`：布尔值以数字形式表示。 @endif @lang{EN} @brief Clear `boolalpha`: represent bool values as numbers. @endif */
template <typename TChar>
inline void noboolalpha(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::boolalpha);
}

/** @lang{ZH} @brief 置位 `showbase`：显示整数的进制前缀。 @endif @lang{EN} @brief Set `showbase`: show the numeric base prefix. @endif */
template <typename TChar>
inline void showbase(ios_base<TChar>& base)
{
    base.setf(ios_defs::showbase);
}

/** @lang{ZH} @brief 清除 `showbase`：不显示进制前缀。 @endif @lang{EN} @brief Clear `showbase`: do not show the base prefix. @endif */
template <typename TChar>
inline void noshowbase(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::showbase);
}

/** @lang{ZH} @brief 置位 `showpoint`：浮点数总是显示小数点。 @endif @lang{EN} @brief Set `showpoint`: always show the decimal point. @endif */
template <typename TChar>
inline void showpoint(ios_base<TChar>& base)
{
    base.setf(ios_defs::showpoint);
}

/** @lang{ZH} @brief 清除 `showpoint`：不强制显示小数点。 @endif @lang{EN} @brief Clear `showpoint`: do not force the decimal point. @endif */
template <typename TChar>
inline void noshowpoint(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::showpoint);
}

/** @lang{ZH} @brief 置位 `showpos`：对非负数显示正号。 @endif @lang{EN} @brief Set `showpos`: show a leading `+` on non-negative numbers. @endif */
template <typename TChar>
inline void showpos(ios_base<TChar>& base)
{
    base.setf(ios_defs::showpos);
}

/** @lang{ZH} @brief 清除 `showpos`：不显示正号。 @endif @lang{EN} @brief Clear `showpos`: do not show the `+` sign. @endif */
template <typename TChar>
inline void noshowpos(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::showpos);
}

/** @lang{ZH} @brief 置位 `skipws`：输入时跳过前导空白。 @endif @lang{EN} @brief Set `skipws`: skip leading whitespace on input. @endif */
template <typename TChar>
inline void skipws(ios_base<TChar>& base)
{
    base.setf(ios_defs::skipws);
}

/** @lang{ZH} @brief 清除 `skipws`：输入时不跳过前导空白。 @endif @lang{EN} @brief Clear `skipws`: do not skip leading whitespace on input. @endif */
template <typename TChar>
inline void noskipws(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::skipws);
}

/** @lang{ZH} @brief 置位 `uppercase`：数值输出使用大写字母。 @endif @lang{EN} @brief Set `uppercase`: use uppercase letters in numeric output. @endif */
template <typename TChar>
inline void uppercase(ios_base<TChar>& base)
{
    base.setf(ios_defs::uppercase);
}

/** @lang{ZH} @brief 清除 `uppercase`：数值输出使用小写字母。 @endif @lang{EN} @brief Clear `uppercase`: use lowercase letters in numeric output. @endif */
template <typename TChar>
inline void nouppercase(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::uppercase);
}

/**
 * @lang{ZH}
 * @brief 置位 `appmode`：启用追加模式（本库扩展）。
 *
 * 置位后，每次输出操作在其 sentry 构造期间都会把流重新定位到末尾。该定位经由转换器
 * 完成（`rseek`），因此需要转换器能由“距末尾的字节偏移”反推出对应的内部字符位置。
 *
 * @warning **仅支持定长且状态无关的编码。** 对 UTF-8 等变长编码或状态相关编码启用
 *          `appmode` 后，首次输出即会在 sentry 构造中抛出 `cvt_error`，流被置
 *          `cvtfailbit`，此后所有输出操作都会在 sentry 的有效性检查处失败——即该流
 *          **一个字节也写不出去**。此限制继承自 `code_cvt::rseek`：变长/状态相关编码
 *          下无法重建内部字符位置，而该位置是重定位的必要组成部分。
 * @note 本操纵符只置标志位，不做上述能力检查（`ios_base` 与转换器解耦，此处看不到
 *       流的转换器）。检查发生在首次输出时。
 * @endif
 *
 * @lang{EN}
 * @brief Set `appmode`: enable append mode (a library extension).
 *
 * Once set, every output operation repositions the stream to the end while its sentry is
 * being constructed. That repositioning goes through the converter (`rseek`), which
 * therefore must be able to recover the corresponding internal character position from a
 * byte offset relative to the end.
 *
 * @warning **Only fixed-length, state-independent encodings are supported.** With
 *          `appmode` enabled on a variable-length encoding such as UTF-8, or on a
 *          state-dependent one, the very first output throws `cvt_error` during sentry
 *          construction and sets `cvtfailbit`; every later output then fails at the
 *          sentry's validity check -- i.e. **not a single byte can be written** to that
 *          stream. The restriction is inherited from `code_cvt::rseek`: for
 *          variable-length / state-dependent encodings the internal character position
 *          cannot be reconstructed, and that position is an essential part of
 *          repositioning.
 * @note This manipulator only sets the flag; it does not perform the capability check
 *       above (`ios_base` is decoupled from the converter and cannot see the stream's
 *       converter here). The check happens on the first output.
 * @endif
 */
template <typename TChar>
inline void appmode(ios_base<TChar>& base)
{
    base.setf(ios_defs::appmode);
}

/** @lang{ZH} @brief 清除 `appmode`：禁用追加模式。 @endif @lang{EN} @brief Clear `appmode`: disable append mode. @endif */
template <typename TChar>
inline void noappmode(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::appmode);
}

/** @lang{ZH} @brief 置位 `unitbuf`：每次输出后刷新缓冲区。 @endif @lang{EN} @brief Set `unitbuf`: flush the buffer after each output. @endif */
template <typename TChar>
inline void unitbuf(ios_base<TChar>& base)
{
    base.setf(ios_defs::unitbuf);
}

/** @lang{ZH} @brief 清除 `unitbuf`：不在每次输出后刷新。 @endif @lang{EN} @brief Clear `unitbuf`: do not flush after each output. @endif */
template <typename TChar>
inline void nounitbuf(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::unitbuf);
}

/** @lang{ZH} @brief 在 `adjustfield` 内设置 `internal`：在符号/前缀与数值间填充。 @endif @lang{EN} @brief Set `internal` within `adjustfield`: pad between sign/prefix and value. @endif */
template <typename TChar>
inline void internal(ios_base<TChar>& base)
{
    base.setf(ios_defs::internal, ios_defs::adjustfield);
}

/** @lang{ZH} @brief 在 `adjustfield` 内设置 `left`：左对齐。 @endif @lang{EN} @brief Set `left` within `adjustfield`: left-justify. @endif */
template <typename TChar>
inline void left(ios_base<TChar>& base)
{
    base.setf(ios_defs::left, ios_defs::adjustfield);
}

/** @lang{ZH} @brief 在 `adjustfield` 内设置 `right`：右对齐。 @endif @lang{EN} @brief Set `right` within `adjustfield`: right-justify. @endif */
template <typename TChar>
inline void right(ios_base<TChar>& base)
{
    base.setf(ios_defs::right, ios_defs::adjustfield);
}

/** @lang{ZH} @brief 在 `basefield` 内设置 `dec`：整数以十进制表示。 @endif @lang{EN} @brief Set `dec` within `basefield`: integers in decimal. @endif */
template <typename TChar>
inline void dec(ios_base<TChar>& base)
{
    base.setf(ios_defs::dec, ios_defs::basefield);
}

/** @lang{ZH} @brief 在 `basefield` 内设置 `hex`：整数以十六进制表示。 @endif @lang{EN} @brief Set `hex` within `basefield`: integers in hexadecimal. @endif */
template <typename TChar>
inline void hex(ios_base<TChar>& base)
{
    base.setf(ios_defs::hex, ios_defs::basefield);
}

/** @lang{ZH} @brief 在 `basefield` 内设置 `oct`：整数以八进制表示。 @endif @lang{EN} @brief Set `oct` within `basefield`: integers in octal. @endif */
template <typename TChar>
inline void oct(ios_base<TChar>& base)
{
    base.setf(ios_defs::oct, ios_defs::basefield);
}

/** @lang{ZH} @brief 在 `floatfield` 内设置 `fixed`：浮点数以定点记法表示。 @endif @lang{EN} @brief Set `fixed` within `floatfield`: floating-point in fixed notation. @endif */
template <typename TChar>
inline void fixed(ios_base<TChar>& base)
{
    base.setf(ios_defs::fixed, ios_defs::floatfield);
}

/** @lang{ZH} @brief 在 `floatfield` 内设置 `scientific`：浮点数以科学记法表示。 @endif @lang{EN} @brief Set `scientific` within `floatfield`: floating-point in scientific notation. @endif */
template <typename TChar>
inline void scientific(ios_base<TChar>& base)
{
    base.setf(ios_defs::scientific, ios_defs::floatfield);
}

/** @lang{ZH} @brief 在 `floatfield` 内同时设置 `fixed | scientific`：浮点数以十六进制记法表示。 @endif @lang{EN} @brief Set both `fixed | scientific` within `floatfield`: floating-point in hexadecimal notation. @endif */
template <typename TChar>
inline void hexfloat(ios_base<TChar>& base)
{
    base.setf(ios_defs::fixed | ios_defs::scientific, ios_defs::floatfield);
}

/** @lang{ZH} @brief 清除 `floatfield`：恢复默认浮点记法。 @endif @lang{EN} @brief Clear `floatfield`: restore the default floating-point notation. @endif */
template <typename TChar>
inline void defaultfloat(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::floatfield);
}
/**
 * @lang{ZH} @} @endif
 * @lang{EN} @} @endif
 */

/**
 * @lang{ZH}
 * @brief 对流的 I/O 互斥量进行 RAII 加锁的辅助类。
 *
 * 构造时锁定 `stream.io_mutex()`，析构时解锁，从而在其生命周期内保证对该流的独占访问。
 * 常用于把对同一流的多次 I/O 操作合并为一个原子的临界区。本类不可拷贝。
 *
 * @tparam TStream 流类型，需提供 `io_mutex()` 接口（返回可 lock()/unlock() 的互斥量）。
 * @endif
 *
 * @lang{EN}
 * @brief RAII helper that locks a stream's I/O mutex.
 *
 * Locks `stream.io_mutex()` on construction and unlocks it on destruction, guaranteeing
 * exclusive access to the stream for its lifetime. Commonly used to group several I/O
 * operations on the same stream into one atomic critical section. This class is
 * non-copyable.
 *
 * @tparam TStream The stream type, which must provide an `io_mutex()` interface
 * (returning a mutex that can be lock()/unlock()'d).
 * @endif
 */
template <typename TStream>
struct sync
{
    /**
     * @lang{ZH}
     * @brief 构造并锁定流的 I/O 互斥量。
     * @param str 要加锁的流。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs and locks the stream's I/O mutex.
     * @param str The stream to lock.
     * @endif
     */
    sync(TStream& str)
        : stream(str)
    {
        stream.io_mutex().lock();
    }

    /**
     * @lang{ZH} @brief 析构并解锁流的 I/O 互斥量。 @endif
     * @lang{EN} @brief Destroys and unlocks the stream's I/O mutex. @endif
     */
    ~sync()
    {
        stream.io_mutex().unlock();
    }

    sync(const sync&) = delete;
    sync& operator=(const sync&) = delete;

    TStream& stream;   ///< @lang{ZH} 被加锁的流的引用。 @endif @lang{EN} Reference to the locked stream. @endif
};
}

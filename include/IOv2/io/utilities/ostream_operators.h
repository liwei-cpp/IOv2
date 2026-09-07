// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file ostream_operators.h
 * @lang{ZH}
 * 定义了输出流的格式化与非格式化插入设施。
 * 包含输出哨兵 `out_sentry`、承载多态 `try_flush()` 的 CRTP 基类 `out_flusher`、
 * 输出流概念 `ostream_type`、承载各类插入操作（`put`/`write`）的 `ostream_operators`
 * 混入基类，以及插入运算符 `operator<<`：一条泛型的（把类型分派到扩展点
 * `io_traits<TChar, TValue>`），外加一条给 `ios_base` 函数指针操纵符的。
 * @endif
 *
 * @lang{EN}
 * Defines the formatted and unformatted insertion facilities for output streams.
 * Includes the output sentry `out_sentry`, the CRTP base `out_flusher` that carries the
 * polymorphic `try_flush()`, the output-stream concept `ostream_type`, the `ostream_operators`
 * mix-in base that carries the various insertion operations (`put`/`write`), and the
 * insertion `operator<<`: one generic (dispatching the type to the `io_traits<TChar, TValue>`
 * extension point) plus one for function-pointer `ios_base` manipulators.
 * @endif
 */
#pragma once

#include <IOv2/common/defs.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/streambuf_iterator.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 输出操作的 RAII 哨兵：在每次插入操作的入口统一完成前置准备与末尾自动刷新。
 *
 * 哨兵负责校验流的有效性、刷新关联（tie）流，并在需要时切换读写方向、
 * （追加模式下）定位到末尾；析构时对 unitbuf / 与 stdio 同步的流执行自动刷新。
 * 哨兵不可拷贝、不可移动。
 * @tparam TStream 关联的输出流类型。
 * @tparam involve_input 若为 `true`，表示该流同时支持输入，构造时会将底层缓冲区
 *                       切换到写入模式（`switch_to_put`）。
 * @tparam is_std 若为 `true`，表示这是标准流：读取其 `m_sync_with_stdio` 以决定析构刷新，
 *                且不执行追加模式的末尾定位。
 * @endif
 *
 * @lang{EN}
 * @brief RAII sentry for output operations: performs the shared setup at the entry of every
 *        insertion operation and an automatic flush at its end.
 *
 * The sentry validates the stream, flushes the tied stream, and
 * optionally switches direction and (in append mode) repositions to the end; on destruction it
 * auto-flushes a unitbuf / stdio-synced stream. The sentry is neither copyable nor movable.
 * @tparam TStream The associated output stream type.
 * @tparam involve_input If `true`, the stream also supports input, and construction switches
 *                       the underlying buffer to put mode (`switch_to_put`).
 * @tparam is_std If `true`, this is a standard stream: its `m_sync_with_stdio` is read to
 *                decide the destruction flush, and no append-mode end repositioning is done.
 * @endif
 */
template <typename TStream, bool involve_input, bool is_std = false>
struct out_sentry
{
    /**
     * @lang{ZH}
     * @brief 构造输出哨兵：校验流、刷新关联流，并按需切换读写方向 / 定位到末尾。
     *
     * @warning **调用方必须已经持有 `os.io_mutex()`，必须有自己的 `catch`，且锁要持到那个 `catch`
     *          之后。** 哨兵自己不加锁：它在 `try` 块末尾就析构了，而 `catch` 里的
     *          `handle_exception` 需要在锁内更新流状态，才能让成功路径与失败路径对同一把
     *          `io_mutex()` 的可见性保持一致；析构中的 unitbuf/stdio 刷新同样依赖那把仍被持有的锁。
     *          把异常留给外层的 `catch`（例如运算符那层）**不够**：栈展开会先析构本地的锁守卫，置位
     *          就落到解锁之后了。锁因此必须是调用方的局部变量，而不是哨兵的成员。
     *
     * 关联流的刷新走 `abs_flusher::try_flush()`，取不到对方的锁就跳过，绝不阻塞。本线程因此可以
     * 安全地在持有本流锁的状态下发起它：tie 这条用户看不见的加锁边永远不会成为等待边，死锁只可能
     * 由用户自己能定序的锁构成。
     * @param os 要操作的输出流。
     * @param is_unit_buf 是否为 unitbuf 流（决定析构时是否自动刷新并对设备 `dflush`）。
     * @param is_app_mode 是否为追加模式；非标准流在追加模式下会在构造时定位到末尾。
     * @throw stream_error 若流无效。
     * @throw cvt_error 若追加模式下无法定位到末尾（追加模式要求定长、与状态无关的编码）。
     * @endif
     * @lang{EN}
     * @brief Constructs the output sentry: validates the stream, flushes the tied stream, and
     * switches direction / repositions to end as needed.
     *
     * @warning **The caller must already hold `os.io_mutex()`, must have a `catch` of its own, and
     *          must keep holding the lock past that `catch`.** The sentry does not lock: it is
     *          destroyed at the end of the enclosing `try`, while `handle_exception` in the
     *          `catch` needs the lock to update the stream state, so that the success and failure
     *          paths stay consistent with respect to the same `io_mutex()`; the unitbuf/stdio
     *          flush in the destructor relies on that still-held lock as well. Leaving the
     *          exception to an outer `catch` -- the operator's, say -- is **not** enough:
     *          unwinding destroys the local lock guard first, so the state bits land after the
     *          the sentry. The lock therefore has to be a local of the caller rather than a member of
     *          the sentry.
     *
     * The tied stream is flushed through `abs_flusher::try_flush()`, which skips the flush rather
     * than wait when the target's lock cannot be taken. This thread can therefore start it safely
     * while holding its own stream's lock: the tie edge -- the one lock edge the user cannot see
     * -- never becomes a waiting edge, so any deadlock can only be built from locks the user is
     * able to order.
     * @param os The output stream to operate on.
     * @param is_unit_buf Whether this is a unitbuf stream (governs the auto-flush and device
     *                    `dflush` on destruction).
     * @param is_app_mode Whether this is append mode; a non-standard stream repositions to the
     *                    end during construction in append mode.
     * @throw stream_error If the stream is invalid.
     * @throw cvt_error If append mode cannot reposition to the end (append mode requires a
     *                  fixed-length, state-independent encoding).
     * @endif
     */
    out_sentry(TStream& os, bool is_unit_buf, bool is_app_mode)
        : m_os(os)
        , m_is_unit_buf(is_unit_buf)
    {
        if (!static_cast<bool>(m_os))
            throw stream_error("ostream_sentry create fail: Invalid ostream");

        if (auto* tied = m_os.tie())
            tied->try_flush();

        if constexpr (is_std)
            m_sync_with_stdio = os.m_sync_with_stdio.load();

        if constexpr (involve_input)
            os.m_streambuf.switch_to_put();

        if constexpr (!is_std)
        {
            if (is_app_mode)
            {
                try
                {
                    m_os.m_streambuf.rseek(0);
                }
                catch (const cvt_error& e)
                {
                    throw cvt_error(std::string("ostream_sentry create fail: appmode cannot "
                                                "reposition to the end; appmode requires a "
                                                "fixed-length, state-independent encoding: ")
                                    + e.what());
                }
            }
        }

        if (!static_cast<bool>(m_os))
            throw stream_error("ostream_sentry create fail: Invalid ostream");
    }

    /**
     * @lang{ZH}
     * @brief 析构时对 unitbuf / 与 stdio 同步的流执行自动刷新，并按异常掩码决定是否上报失败。
     *
     * 对设置了 unitbuf 或与 stdio 同步的流，析构会把缓冲区 `flush()` 出去（unitbuf 时再对设备
     * `dflush()`）。刷新失败经 `handle_exception` 上报：按类别置 `devfailbit`/`cvtfailbit`/
     * `strfailbit`，并在该位处于异常掩码时抛出。无失败位入掩码时（默认）只置位不抛。
     *
     * 若析构发生在栈展开期间，则仍尝试刷新但**只置位、绝不抛出**，以免触发 `std::terminate`；
     * 该判定由 `handle_exception` 自身完成。为使正常退出路径的通知得以传播，本析构声明为
     * `noexcept(false)`。
     * @endif
     *
     * @lang{EN}
     * @brief On destruction, auto-flushes a unitbuf / stdio-synced stream and decides whether
     * to report a flush failure according to the exception mask.
     *
     * For a stream with unitbuf set or synced with stdio, destruction flushes the buffer via
     * `flush()` (and, for unitbuf, `dflush()`es the device). A flush failure is reported through
     * `handle_exception`: it sets `devfailbit`/`cvtfailbit`/`strfailbit` by category and throws
     * when that bit is in the exception mask. When no such bit is in the mask (the default) the
     * bit is only set and nothing is thrown.
     *
     * If destruction happens during stack unwinding it still attempts the flush but **only sets
     * bits and never throws**, so as not to trigger `std::terminate`; that test lives in
     * `handle_exception` itself. To let the normal-path notification propagate, this destructor
     * is declared `noexcept(false)`.
     * @endif
     */
    ~out_sentry() noexcept(false)
    {
        try
        {
            if (m_os)
            {
                if (m_is_unit_buf || m_sync_with_stdio)
                    m_os.m_streambuf.flush();

                if (m_is_unit_buf)
                    m_os.m_streambuf.device().dflush();
            }
        }
        catch (...)
        {
            m_os.handle_exception(std::current_exception());
        }
    }

    out_sentry(const out_sentry&) = delete;
    out_sentry& operator=(const out_sentry&) = delete;
    out_sentry(out_sentry&&) = delete;
    out_sentry& operator=(out_sentry&&) = delete;

private:
    TStream&    m_os;
    bool        m_is_unit_buf;
    bool        m_sync_with_stdio = false;
};

/**
 * @lang{ZH}
 * @brief 类型特征的主模板：默认判定任意类型都不是 `out_sentry`。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Primary template of the type trait: by default any type is not an `out_sentry`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename>
struct is_out_sentry_impl
{
    constexpr static bool value = false;
};

/**
 * @lang{ZH}
 * @brief `is_out_sentry_impl` 的偏特化：对 `out_sentry` 实例判定为真。
 * @endif
 *
 * @lang{EN}
 * @brief Partial specialization of `is_out_sentry_impl`: true for instances of `out_sentry`.
 * @endif
 */
template <typename TStream, bool involve_input, bool is_std>
struct is_out_sentry_impl<out_sentry<TStream, involve_input, is_std>>
{
    constexpr static bool value = true;
};

/**
 * @lang{ZH}
 * @brief 判定某类型是否为 `out_sentry` 实例的概念。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept that checks whether a type is an instance of `out_sentry`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename T>
concept is_out_sentry = is_out_sentry_impl<T>::value;

/**
 * @lang{ZH}
 * @brief 承载多态 `try_flush()` 接口的抽象基类。
 *
 * 提供纯虚的 `try_flush()`，使得可通过基类指针以类型无关的方式刷新任意 tie 目标；输出流的
 * 实现由 CRTP 派生类 `out_flusher<T>` 给出。
 * @endif
 *
 * @lang{EN}
 * @brief Abstract base carrying the polymorphic `try_flush()` interface.
 *
 * Provides a pure virtual `try_flush()` so that any tie target can be flushed type-erased
 * through a base pointer; for output streams the implementation is given by the CRTP-derived
 * `out_flusher<T>`.
 * @endif
 */
class abs_flusher
{
public:
    virtual ~abs_flusher() = default;

protected:
    abs_flusher()                              = default;
    abs_flusher(const abs_flusher&)            = default;
    abs_flusher& operator=(const abs_flusher&) = default;
    abs_flusher(abs_flusher&&)                 = default;
    abs_flusher& operator=(abs_flusher&&)      = default;

public:
    /**
     * @lang{ZH}
     * @brief 尽力刷新本流的纯虚接口；由 `out_flusher<T>` 实现。
     * @warning **实现必须非阻塞。** 哨兵在持有自己流的锁时调用本函数，而这条加锁边对用户不可见，
     *          他无从把它纳入自己的锁序；实现若改用阻塞的 `lock()`，AB-BA 死锁立即回归。取锁
     *          失败时必须直接返回，不得等待，也不留任何补偿——tie 因此是尽力而为，不是保证。
     * @warning **实现必须自行吞掉刷新过程中的一切异常，故声明为 `noexcept`。** 本函数的调用点
     *          位于哨兵构造函数中，而哨兵构造在**发起方**流的 `try` 块内（见
     *          `ostream_operators.h` 的插入运算符）；异常一旦逸出，就会被那里的 `catch` 交给
     *          **发起方**的 `handle_exception`，于是本流（tie 目标）的失败被记成发起方的失败：
     *          发起方会拿到一个描述**别人的** streambuf 的 `cvt_error`，并按异常类型置上自己
     *          从未发生过的 `cvtfailbit`。`noexcept` 把"失败只记在 tie 目标身上"这条归属规则
     *          变成编译期约束，而不再依赖每个调用点自觉包一层 `catch (...)`。
     * @endif
     *
     * @lang{EN}
     * @brief Pure virtual interface that flushes this stream on a best-effort basis; implemented
     *        by `out_flusher<T>`.
     * @warning **An implementation must never block.** The sentry calls this while holding its
     *          own stream's lock, and that lock edge is invisible to the user, who therefore
     *          cannot fold it into a lock order of their own; an implementation that takes a
     *          blocking `lock()` brings the AB-BA deadlock straight back. On failure to acquire
     *          the lock it must return at once, without waiting and without leaving anything
     *          pending -- which is why a tie is best-effort rather than a guarantee.
     * @warning **An implementation must swallow every exception raised by the flush, hence the
     *          `noexcept`.** This is called from a sentry constructor, and the sentry is itself
     *          constructed inside the *initiating* stream's `try` block (see the insertion
     *          operators in `ostream_operators.h`). An escaping exception would be caught there
     *          and handed to the *initiator's* `handle_exception`, recording this stream's (the
     *          tie target's) failure against the initiator: it would receive a `cvt_error`
     *          describing *someone else's* streambuf and, dispatched on exception type, have a
     *          `cvtfailbit` set that its own pipeline never earned. The `noexcept` turns
     *          "a failure is recorded only on the tie target" into a compile-time constraint
     *          instead of a convention every call site must remember to honor.
     * @endif
     */
    virtual void try_flush() noexcept = 0;
};

/**
 * @lang{ZH}
 * @brief 承载多态 `try_flush()` 的 CRTP 基类：对具体流类型 `T` 的向下转型集中于此。
 *
 * 虚函数无法使用 deducing-this，只能 `static_cast<T&>(*this)` 取回具体流类型；本模板专为承载
 * 该 `T` 而设，使 `ostream_operators` 不必再携带 CRTP 自身参数。每个输出流同时派生
 * `out_flusher<自身>` 与 `ostream_operators<TChar>`。
 * @tparam T 具体的输出流类型。
 * @endif
 * @lang{EN}
 * @brief CRTP base carrying the polymorphic `try_flush()`: the down-cast to the concrete stream
 * type `T` is localized here.
 *
 * A virtual cannot use deducing-this, so it must `static_cast<T&>(*this)` to recover the concrete
 * stream type; this template exists solely to carry that `T`, letting `ostream_operators` drop
 * its CRTP self-parameter. Every output stream derives from both `out_flusher<Self>` and
 * `ostream_operators<TChar>`.
 * @tparam T The concrete output stream type.
 * @endif
 */
template <typename T>
struct out_flusher : public abs_flusher
{
    /**
     * @lang{ZH}
     * @brief 尽力刷新本流：取到锁就 `flush()`，取不到就放弃。
     *
     * 锁一律以 `std::try_to_lock` 获取，至多尝试三次、其间让出时间片。放弃时不做任何补偿，
     * 该次 tie 刷新就此跳过。调大尝试次数不会更安全：真正的 AB-BA 场景下对方线程正阻塞在本
     * 线程持有的锁上，所有尝试注定失败。
     *
     * 刷新失败时，异常在**本流**（tie 目标）上经 `handle_exception<true>()` 落地：置对应的失败
     * 位、把原始异常存进对应的 `m_exp_*_fail` 槽位，然后返回。发起方一位不动——理由见
     * `abs_flusher::try_flush()` 的第二条 `@warning`。这里必须用忽略掩码的那个版本：掩码版会走到
     * `clear()` 的重抛分支，那里的 `exchange` 会把刚存进去的异常清空，结果恰恰对那些显式把该位
     * 放进 `exceptions()` 的使用者一个异常对象都不留。
     *
     * @note 因此 **tie 目标的异常掩码在这条路径上不生效**，这是有意的：唯一在场的调用栈属于
     *       一个与本流无关的流，异常送不出去。失败通过本流的状态位与 `m_exp_*_fail` 上报。
     *       这一点与标准库不同——libstdc++ 与 libc++ 都会让 tie 目标的掩码打断**发起方**的操作
     *       （两者一个在插入侧、一个在提取侧，方向正好相反）。
     * @endif
     *
 * @lang{EN}
 * @brief Flushes this stream on a best-effort basis: `flush()`es if the lock can be taken,
 *        gives up otherwise.
 *
 * The lock is always taken with `std::try_to_lock`, for at most three attempts with a yield in
 * between. Giving up leaves nothing pending; that tie flush is simply skipped. Raising the
 * attempt count does not buy safety: in a genuine AB-BA the other thread is blocked on a lock
 * this thread holds, so every attempt is doomed.
 *
 * When the flush fails, the exception lands on *this* stream (the tie target) through
 * `handle_exception<true>()`: the matching failure bit is set, the original exception is stored
 * in the matching `m_exp_*_fail` slot, and the function returns. The initiator is left untouched
 * -- see the second `@warning` on `abs_flusher::try_flush()` for why. The mask-ignoring form is
 * required here: the mask-honoring one would reach `clear()`'s rethrow branch, which empties the
 * slot it just filled, leaving no exception object at all for exactly those users who put the bit
 * in `exceptions()`.
 *
 * @note Consequently **the tie target's exception mask does not apply on this path**, by
 *       design: the only stack available belongs to an unrelated stream, so an exception has
 *       nowhere to be delivered. The failure is reported through this stream's state bits and
 *       `m_exp_*_fail`. This differs from the standard library, where the tie target's mask
 *       aborts the *initiator's* operation -- libstdc++ and libc++ both do this, on opposite
 *       sides (insertion and extraction respectively).
 * @endif
     */
    void try_flush() noexcept override
    {
        constexpr int attempts = 3;

        T& obj = static_cast<T&>(*this);
        for (int i = 0; i < attempts; ++i)
        {
            std::unique_lock lk(obj.io_mutex(), std::try_to_lock);
            if (lk)
            {
                try
                {
                    obj.flush();
                }
                catch (...)
                {
                    // Record on the tie target, never on the initiator, and never rethrow.
                    // <true> keeps the stored exception alive; see the note above.
                    obj.template handle_exception<true>(std::current_exception());
                }
                return;
            }
            if (i + 1 < attempts)
                std::this_thread::yield();
        }
    }
};

template <typename TChar>
struct ostream_operators;

/**
 * @lang{ZH}
 * @brief 输出流类型的概念。
 *
 * 一个类型要成为输出流，必须提供 `out_sentry_type`、`out_iter_type` 与 `char_type` 类型、
 * 可返回其 locale，且其 `out_sentry_type` 满足 `is_out_sentry`；同时它必须派生自
 * `ios_state<char_type>` 与 `ostream_operators<char_type>`。
 * @note `ios_state` 与 `ios_base` 两条是必需的，不只是描述性的：本概念约束下的代码会直接调用
 *       `handle_exception()` 与 `operator bool`；而带有两个 `ios_base` 子对象的类型在这里就被
 *       拒掉，不必拖到后面某个 `io_mutex()` 调用才报二义。
 * @note `out_iter_type` 必须是 `o_iter()` 的返回类型（`o_iter()` 的返回类型就写成它，因此两者
 *       不会漂移）。之所以要把这个**类型**公开出来，是因为 `o_iter()` 本身是私有的，而命名概念
 *       无法调用它；公开类型不等于公开对象——`ostreambuf_iterator` 只能由 `TStreamBuf&` 构造。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept for an output stream type.
 *
 * To qualify as an output stream, a type must expose the `out_sentry_type`, `out_iter_type` and
 * `char_type` types, be able to return its locale, and have an `out_sentry_type` that satisfies
 * `is_out_sentry`; it must also derive from `ios_state<char_type>` and
 * `ostream_operators<char_type>`.
 * @note The `ios_state` and `ios_base` clauses are requirements, not just descriptions: code
 *       constrained by this concept calls `handle_exception()` and `operator bool` directly, and
 *       a type carrying two `ios_base` subobjects is rejected here rather than at some later
 *       ambiguous `io_mutex()` call.
 * @note `out_iter_type` must be the return type of `o_iter()` -- which is spelled as exactly
 *       that type, so the two cannot drift apart. The type is public because `o_iter()` itself
 *       is private and a named concept cannot call it; exposing the type is not exposing the
 *       object, since an `ostreambuf_iterator` can only be built from a `TStreamBuf&`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename T>
concept ostream_type =
    requires (T a)
    {
        typename T::out_sentry_type;
        typename T::out_iter_type;
        typename T::char_type;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_out_sentry<typename T::out_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>> &&
    std::derived_from<T, ios_state<typename T::char_type>> &&
    std::derived_from<T, ostream_operators<typename T::char_type>>;

namespace detail
{
/**
 * @lang{ZH}
 * @brief `T` 能否用 `io_traits` 的**流形式** `swrite(stream, value)` 插入 `TValue`。
 * @endif
 *
 * @lang{EN}
 * @brief Whether a `TValue` can be inserted into `T` through the **stream form** of
 *        `io_traits`, `swrite(stream, value)`.
 * @endif
 */
template <typename T, typename TKey, typename TArg = TKey>
concept insertable_with_stream = ostream_type<T> &&
    requires(T& obj, const TArg& value) { io_traits<typename T::char_type, TKey>::swrite(obj, value); };

/**
 * @lang{ZH}
 * @brief `T` 能否用 `io_traits` 的**迭代器形式** `swrite(iter, io, loc, value)` 插入 `TValue`。
 * @note 迭代器以 `out_iter_type&` 也就是**左值**参与探测，与运算符里 `auto iter = obj.o_iter();`
 *       之后传 `iter` 的形式一致：探测与调用必须是同一个表达式，否则两者会给出不同答案。
 * @endif
 *
 * @lang{EN}
 * @brief Whether a `TValue` can be inserted into `T` through the **iterator form** of
 *        `io_traits`, `swrite(iter, io, loc, value)`.
 * @note The iterator is probed as an `out_iter_type&`, that is as an **lvalue**, matching the
 *       operator's `auto iter = obj.o_iter();` followed by passing `iter`: the probe and the
 *       call have to be the same expression, or the two can disagree.
 * @endif
 */
template <typename T, typename TKey, typename TArg = TKey>
concept insertable_with_iter = ostream_type<T> &&
    requires(T& obj, typename T::out_iter_type& iter, const TArg& value)
    { io_traits<typename T::char_type, TKey>::swrite(iter, obj, obj.locale(), value); };

/**
 * @lang{ZH}
 * @brief `os << value` 是否成立：两种形式任一可用即可，每种形式内 `TValue` 与其衰退型都试。
 * @note 本概念就是 `operator<<` 的约束，运算符体内的分派也复用它的两个组成部分，因此
 *       "能不能插入"只有一份真相：`requires { os << x; }` 与运算符实际选中的通道永远一致。
 *       衰退那两档不能省——数组名要衰退成指针、函数名要衰退成函数指针，`os << "hello"`
 *       靠的就是它们。
 * @endif
 *
 * @lang{EN}
 * @brief Whether `os << value` is valid: either form will do, and within each form both
 *        `TValue` and its decayed type are tried.
 * @note This concept *is* the constraint on `operator<<`, and the dispatch inside the operator
 *       reuses its two components, so "can this be inserted" has a single source of truth:
 *       `requires { os << x; }` and the channel the operator actually picks can never disagree.
 *       The two decay rungs are not optional -- they are what lets an array name decay to a
 *       pointer and a function name to a function pointer, which is how `os << "hello"` works.
 * @endif
 */
template <typename T, typename TValue>
concept insertable =
    ostream_type<T> &&
    (insertable_with_stream<T, TValue> ||
     insertable_with_stream<T, std::decay_t<const TValue&>, TValue> ||
     insertable_with_iter<T, TValue> ||
     insertable_with_iter<T, std::decay_t<const TValue&>, TValue>);
}

/**
 * @lang{ZH}
 * @brief 为输出流提供各类插入操作的混入（mix-in）基类。
 *
 * 本模板集中承载 `put`、`write` 等成员，供具体输出流类型派生使用；这些成员通过
 * deducing-this（`this TSelf& self`）以派生类的具体类型执行操作。每个操作先取本流的
 * `io_mutex()`，再在锁内构造输出哨兵以完成前置准备，并将异常统一交由流的 `handle_exception`
 * 处理，从而按异常掩码更新流状态。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Mix-in base that provides the various insertion operations for output streams.
 *
 * This template centralizes members such as `put` and `write` for concrete output stream
 * types to derive from; these members use deducing-this (`this TSelf& self`) to operate on
 * the concrete derived type. Each operation takes the stream's `io_mutex()` and then constructs
 * an output sentry under it to handle the setup, and routes exceptions through the stream's
 * `handle_exception`, which updates the stream state according to the exception mask.
 * @tparam TChar The character type.
 * @endif
 */
template <typename TChar>
struct ostream_operators
{
    /**
     * @lang{ZH}
     * @brief 向流写入单个字符。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param c 要写入的字符。
     * @param force_flush 为 `true` 时，无论 `unitbuf` 标志是否置位，本次写入结束后都刷新
     *        缓冲区并对设备 `dflush()`；为 `false`（默认）时只遵循 `unitbuf`。
     * @return 流自身的引用。
     * @note 本参数存在的原因：「这一次要不要刷新」从来都是一个**局部**信息。若没有它，像 `endl`
     *       这样需要强制刷新的操纵符只能绕道去临时置位再复位全局的 `unitbuf` 标志，而那个
     *       读-改-写既非原子也非异常安全。
     * @endif
     *
     * @lang{EN}
     * @brief Writes a single character to the stream.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param c The character to write.
     * @param force_flush When `true`, the buffer is flushed and the device `dflush()`ed after
     *        this write regardless of the `unitbuf` flag; when `false` (the default), only
     *        `unitbuf` governs that.
     * @return A reference to the stream itself.
     * @note Why this parameter exists: "should this particular call flush" has always been
     *       **local** information. Without it, a manipulator that needs a forced flush -- `endl`
     *       being the one -- would have to temporarily set and clear the global `unitbuf` flag,
     *       a read-modify-write that is neither atomic nor exception-safe.
     * @endif
     */
    template<typename TSelf>
    TSelf& put(this TSelf& self, TChar c, bool force_flush = false)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, force_flush || bool(self.flags() & ios_defs::unitbuf),
                             bool(self.flags() & ios_defs::appmode));
            self.m_streambuf.sputc(c);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return self;
    }

    /**
     * @lang{ZH}
     * @brief 向流写入 `n` 个字符。
     *
     * 这是非格式化插入。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param s 源缓冲区。当 `n != 0` 时不得为空指针。
     * @param n 要写入的字符数。
     * @return 流自身的引用。
     * @throw stream_error 若 `s` 为空指针而 `n != 0`。
     * @endif
     *
     * @lang{EN}
     * @brief Writes `n` characters to the stream.
     *
     * This is an unformatted insertion.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param s The source buffer. Must not be a null pointer when `n != 0`.
     * @param n The number of characters to write.
     * @return A reference to the stream itself.
     * @throw stream_error If `s` is a null pointer while `n != 0`.
     * @endif
     */
    template<typename TSelf>
    TSelf& write(this TSelf& self, const TChar* s, std::size_t n)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, bool(self.flags() & ios_defs::unitbuf), bool(self.flags() & ios_defs::appmode));
            if (s == nullptr && n != 0)
                throw stream_error("ostream write fail: null character sequence");
            self.m_streambuf.sputn(s, n);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return self;
    }

    /**
     * @lang{ZH}
     * @brief 刷新本流：把缓冲区写出到底层设备。
     *
     * 刷新不经哨兵，而是直接以 `std::lock_guard` 持有本流的 `io_mutex()`（该锁为递归锁）
     * 直至刷新结束。由此：
     *   - **并发刷新被串行化**：多个线程同时 `flush()` 同一流时逐个进入，每个都真正完成一次
     *     刷新（不是"先到者刷、其余跳过"的弱语义）；底层缓冲区不会被并发操作。
     *   - **写与刷互斥**：`put`/`write`/`operator<<` 在其哨兵生命周期内持有同一把
     *     `io_mutex()`，故写与刷不会并发。
     *
     * 本函数**不刷新 tie 流**：与标准 `std::basic_ostream::flush` 一致，刷新只作用于本流，
     * 关联流的刷新仅由输出操作的哨兵在其入口触发。因刷新不再沿 tie 链传播，也就不存在递归回
     * 到本流的可能，无需任何"正在刷新"自旋/跳过标志。
     *
     * 输出前的读写模式切换由 `streambuf::flush()` 自行完成（其内部会 `switch_to_put()`），
     * 故此处无需哨兵代劳。
     * @warning **本函数在失败态下拒绝写出，但生命周期收尾的冲刷不受状态位约束。**
     *          流的 `operator bool` 为假时（任一失败位；单独的 `eofbit` 不算），本函数在触碰
     *          缓冲区之前就抛 `stream_error`，经 `handle_exception` 转成 `strfailbit`，
     *          那批待刷字节一个也没写出去。但流的**析构**、`detach()` 与**赋值**都不看状态位：
     *          它们的冲刷发生在下一层（`~root_cvt` / `root_cvt::detach()` /
     *          `root_cvt::operator=`），而 `root_cvt` 拿不到 `ios_state`，于是同一批字节照样
     *          `dput` 给设备。因此「`flush()` 被拒」**不意味着**这批字节不会到达设备；
     *          真要丢弃，只能 `detach()` 之后弃用取回的设备，或者先 `clear()` 再 `flush()`。
     * @note 上一条与标准库同构，不是本库的特例：`std::ofstream` 在失败态下 `flush()` 同样
     *       什么都不写，而 `~basic_ofstream()` 照样冲刷 put 区，分层的原因相同。本库没有
     *       `close()`，最接近的 `detach()` 会把清理阶段的首个异常以 `exception_ptr` 交还调用方。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes this stream: writes the buffer out to the underlying device.
     *
     * Flushing does not go through the sentry; instead a `std::lock_guard` holds this stream's
     * `io_mutex()` (a recursive mutex) for the whole flush. Therefore:
     *   - **Concurrent flushes are serialized**: when several threads `flush()` the same
     *     stream, they enter one at a time and each actually completes a flush (not the weak
     *     "first caller flushes, the rest skip" semantics); the underlying buffer is never
     *     operated on concurrently.
     *   - **Write and flush are mutually exclusive**: `put`/`write`/`operator<<` hold the same
     *     `io_mutex()` for their sentry's lifetime, so a write never races a flush.
     *
     * This function **does not flush tied streams**: like the standard
     * `std::basic_ostream::flush`, it acts on this stream alone; tied streams are flushed only
     * by the sentry at the entry of an output operation. Since a flush no longer propagates
     * down the tie chain, it can never recurse back into this stream, so no "already flushing"
     * spin/skip flag is needed.
     *
     * Switching the buffer from get to put mode is done by `streambuf::flush()` itself (it
     * calls `switch_to_put()` internally), so no sentry is needed for that either.
     * @warning **This function refuses to write on a failed stream, but the flushes that wind
     *          the stream down are not bound by the state bits.** When the stream's
     *          `operator bool` is false (any failure bit; `eofbit` alone does not count), this
     *          function throws `stream_error` before it touches the buffer, `handle_exception`
     *          turns that into `strfailbit`, and not one of the pending bytes is written. But
     *          the stream's **destructor**, `detach()` and **assignment** do not look at the
     *          state bits: their flush happens one layer down (`~root_cvt`,
     *          `root_cvt::detach()`, `root_cvt::operator=`), and `root_cvt` has no access to
     *          `ios_state`, so those same bytes are still `dput` to the device. A refused
     *          `flush()` therefore **does not** mean the bytes will never reach the device. To
     *          really discard them, `detach()` and drop the device handed back, or `clear()`
     *          first and then `flush()`.
 * @note The above mirrors the standard library rather than being peculiar to this library:
 *       `std::ofstream::flush()` on a failed stream writes nothing either, while
 *       `~basic_ofstream()` still flushes the put area, for the same layering reason. This
 *       library has no `close()`; the nearest thing, `detach()`, hands the first exception
 *       raised during cleanup back to the caller as an `exception_ptr`.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @endif
     */
    template <typename TSelf>
    void flush(this TSelf& self)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            if (!static_cast<bool>(self))
                throw stream_error("ostream flush fail: Invalid ostream");

            if constexpr (dev_cpt::support_put<typename TSelf::device_type>)
            {
                self.m_streambuf.flush();
                self.m_streambuf.device().dflush();
            }
            else
                throw stream_error("ostream flush fail: device does not support output");
        }
        catch (...)
        {
            self.handle_exception(std::current_exception());
        }
    }

    /**
     * @lang{ZH}
     * @brief 取绑定到本流缓冲区的输出迭代器。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 绑定到本流缓冲区的 `ostreambuf_iterator`。
     * @note 返回类型写成 `TSelf::out_iter_type` 而不是 `auto`，使探测那一侧
     *       （`detail::insertable_with_iter`）看到的别名与实现不可能漂移。
     * @endif
     *
     * @lang{EN}
     * @brief Gets an output iterator bound to this stream's buffer.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return An `ostreambuf_iterator` bound to this stream's buffer.
     * @note The return type is spelled `TSelf::out_iter_type` rather than `auto`, so the alias the
     *       probing side (`detail::insertable_with_iter`) sees and the implementation cannot drift
     *       apart.
     * @endif
     */
private:
    template <typename TSelf>
    typename TSelf::out_iter_type o_iter(this TSelf& self)
    {
        return ostreambuf_iterator(self.m_streambuf);
    }

    /**
     * @lang{ZH}
     * @brief 声明格式化插入运算符为友元，使其可访问私有的 `o_iter`。
     * @endif
     *
     * @lang{EN}
     * @brief Befriends the formatted insertion operator so it can access the private `o_iter`.
     * @endif
     */
    template <ostream_type U, typename TValue>
        requires detail::insertable<U, TValue>
    friend U& operator<<(U& obj, const TValue& value);
};

/**
 * @lang{ZH}
 * @brief 插入运算符：把一个 `TValue` 类型的值或一个操纵符写入流。
 *
 * 本运算符是插入侧**唯一**的入口。它在 `if constexpr` 里挑出可用的通道：流形式
 * `swrite(stream, value)`（操纵符）与迭代器形式 `swrite(iter, io, loc, value)`（格式化输出）；
 * 每种形式内先试 `TValue`，不成再试 `std::decay_t<const TValue&>`（数组实参经此退化成指针，函数名退化
 * 成函数指针）。
 *
 * @note 同一个 `io_traits` 特化**只能提供其中一种形式**的 `swrite`：两种都提供会撞上本函数体
 *       开头的 `static_assert`。形式内部则保证不衰退的 `TValue` 优先于
 *       `std::decay_t<const TValue&>`。
 * @note 本运算符由 `detail::insertable` 约束，而**函数体内的分派复用同一组概念**，因此
 *       `requires { os << x; }` 与运算符实际选中的通道永远一致。该判据只对当前 TU 有效，
 *       拿它分支前请先看 `traits_base.h` 上关于可探测性的 `@warning`。加约束的代价是类型不
 *       支持时诊断退化为通用的 "no match for `operator<<`"——`static_assert` 要可达就得不加
 *       约束，不加约束就无法探测。
 * @note 概念里直接写 `io_traits<TChar, TValue>::swrite(...)` 是安全的：未特化时它是不完整
 *       类型，在 requires 表达式里属于可 SFINAE 的替换失败，结果为 `false` 而非硬错误。
 * @note 加锁位置分两种：迭代器形式由本运算符取 `io_mutex()`，并保证 `handle_exception()`
 *       也在锁内；流形式一律不加锁，由操纵符自己决定——各操纵符所需的锁作用域并不相同。
 *       相应地，流形式外面的 `catch` 是**最外层**的异常出口，而不是唯一的：自己取了
 *       `io_mutex()` 的操纵符必须在锁内自己 `catch` 并调 `handle_exception`，否则栈展开会先
 *       析构它的锁守卫，置位就落到解锁之后；理由见 `traits_base.h` 上的 `@warning`。掩码命中时
 *       从操纵符自己的 `handle_exception` 再抛出来的异常在这里被处理两次，但 `handle_exception`
 *       幂等，无害。
 * @tparam T 输出流类型。
 * @tparam TValue 源值类型。
 * @param obj 输出流。
 * @param value 要写入的值或操纵符。
 * @return 流自身的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion operator: writes a value of type `TValue`, or a manipulator, to the stream.
 *
 * This is the **only** entry point on the insertion side. An `if constexpr` chain picks whichever
 * channel is usable: the stream form `swrite(stream, value)` (manipulators) or the iterator form
 * `swrite(iter, io, loc, value)` (formatted output). Within each form `TValue` is tried first and
 * `std::decay_t<const TValue&>` second -- that is where array arguments decay to pointers and function
 * names to function pointers.
 *
 * @note One `io_traits` specialization may provide **only one of the two forms** of `swrite`:
 *       providing both hits the `static_assert` at the top of this function body. Within a form,
 *       the undecayed `TValue` is guaranteed to win over `std::decay_t<const TValue&>`.
 * @note This operator is constrained by `detail::insertable`, and the dispatch **inside the body
 *       reuses the same concepts**, so `requires { os << x; }` and the channel the operator
 *       actually picks can never disagree. That test holds for the current TU only; see the
 *       `@warning` on detectability in `traits_base.h` before branching on it. The price of the
 *       constraint is that an unsupported type gets the generic "no match for `operator<<`"
 *       diagnostic, since a reachable `static_assert` would require an unconstrained operator and
 *       an unconstrained operator cannot be probed.
 * @note Naming `io_traits<TChar, TValue>::swrite(...)` directly in the concepts is safe: where it
 *       is not specialized it is an incomplete type, which inside a requires-expression is a
 *       SFINAE-able substitution failure yielding `false` rather than a hard error.
 * @note Locking splits two ways: for the iterator form this operator takes `io_mutex()` and
 *       keeps `handle_exception()` inside it; the stream form is never locked here and decides
 *       for itself, since the lock scope each manipulator needs differs. Correspondingly, the
 *       `catch` around the stream form is the **outermost** exception exit, not the only one: a
 *       manipulator that takes `io_mutex()` itself must `catch` and call `handle_exception`
 *       inside its own lock, or unwinding destroys its lock guard first and the state write lands
 *       after the unlock; see the `@warning` in `traits_base.h`. An exception rethrown from a
 *       manipulator's own `handle_exception` on a mask hit is handled twice here, harmlessly,
 *       since `handle_exception` is idempotent.
 * @tparam T The output stream type.
 * @tparam TValue The source value type.
 * @param obj The output stream.
 * @param value The value or manipulator to write.
 * @return A reference to the stream itself.
 * @endif
 */
template <ostream_type T, typename TValue>
    requires detail::insertable<T, TValue>
T& operator<<(T& obj, const TValue& value)
{
    using TChar  = typename T::char_type;
    using TDecay = std::decay_t<const TValue&>;

    constexpr bool stream_v = detail::insertable_with_stream<T, TValue>;
    constexpr bool iter_v   = detail::insertable_with_iter<T, TValue>;
    constexpr bool stream_d = detail::insertable_with_stream<T, TDecay, TValue>;
    constexpr bool iter_d   = detail::insertable_with_iter<T, TDecay, TValue>;

    static_assert(!(stream_v && iter_v) && !(stream_d && iter_d),
        "IOv2: this io_traits specialization provides both forms of swrite() -- the stream form "
        "swrite(stream, value) and the iterator form swrite(iter, io, loc, value). Provide "
        "exactly one: the iterator form for formatted output, the stream form for a manipulator. "
        "See io/traits/traits_base.h.");

    if constexpr (stream_v || stream_d)
    {
        try
        {
            io_traits<TChar, std::conditional_t<stream_v, TValue, TDecay>>::swrite(obj, value);
        }
        catch (...)
        {
            obj.handle_exception(std::current_exception());
        }
    }
    else if constexpr (iter_v || iter_d)
    {
        using sentry_type = typename T::out_sentry_type;
        std::lock_guard guard(obj.io_mutex());
        try
        {
            sentry_type cerb(obj, bool(obj.flags() & ios_defs::unitbuf),
                                  bool(obj.flags() & ios_defs::appmode));

            auto iter = obj.o_iter();
            io_traits<TChar, std::conditional_t<iter_v, TValue, TDecay>>::swrite(
                iter, obj, obj.locale(), value);
        }
        catch (...)
        {
            obj.handle_exception(std::current_exception());
        }
    }
    else
        static_assert(dependent_false_v<TValue>,
            "IOv2 internal: detail::insertable admitted this type but the dispatch chain has no "
            "branch for it. The concept and the chain must stay in step -- see "
            "IOv2/io/utilities/ostream_operators.h.");

    return obj;
}

/**
 * @lang{ZH}
 * @brief 插入运算符：应用一个作用于 `ios_base<char_type>&` 的函数指针操纵符。
 *
 * 本条是插入侧唯一**不走 `io_traits` 扩展点**的重载，它的形参类型必须是**非推导语境**：
 * `boolalpha` / `hex` / `defaultfloat` 这些操纵符是函数模板，`os << IOv2::boolalpha` 给出的
 * 是一个模板名而不是某个具体函数，只有形参类型事先确定，编译器才能反推出模板实参、取到函数
 * 地址。用户自己写的操纵符函数模板同样依赖这一条。
 *
 * 只取 `ios_base<char_type>&` 的操纵符碰不到 streambuf 与设备，做不了 I/O，因此**无所谓方向**：
 * 提取侧有形状相同的重载，`os << pf` 与 `is >> pf` 等价。两条都不加锁、不建哨兵——操纵符
 * 改的只是格式化状态，而那些访问器自己就是线程安全的。
 * @note 只有函数指针这一种形状：`std::function`、`std::move_only_function`、仿函数、裸 lambda
 *       都走不到这里，泛型运算符的约束也不满足，于是没有可行重载、编译不过。无捕获的 lambda
 *       写 `+lambda` 可退化成函数指针；带状态的操纵符请走 `io_traits` 的流形式——它拿到的是
 *       真正的流，比 `ios_base&` 能做的更多。
 * @param obj 输出流。
 * @param pf 操纵符；为空指针时置 `strfailbit`。
 * @return 流自身的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion operator: applies a function-pointer manipulator acting on
 *        `ios_base<char_type>&`.
 *
 * This is the only insertion-side overload that does **not** go through the `io_traits` extension
 * point, and its parameter type has to be a **non-deduced context**: manipulators such as
 * `boolalpha`, `hex` and `defaultfloat` are function templates, so `os << IOv2::boolalpha` names a
 * template rather than one function, and only a parameter type fixed in advance lets the compiler
 * work backwards to the template arguments and take the function's address. User-written
 * manipulator function templates depend on this too.
 *
 * A manipulator taking only `ios_base<char_type>&` cannot reach the streambuf or the device and
 * so cannot do I/O; it therefore has **no direction**. The extraction side carries an overload of
 * the same shape and `os << pf` is equivalent to `is >> pf`. Neither overload locks nor builds a
 * sentry -- a manipulator only changes formatting state, and those accessors are thread-safe
 * themselves.
 * @note A function pointer is the only shape accepted: `std::function`,
 *       `std::move_only_function`, a functor and a bare lambda all miss this overload and do not
 *       satisfy the generic one either, so no overload is viable and they do not compile. A
 *       capture-less lambda can be written `+lambda` to decay to a function pointer; a manipulator
 *       that carries state belongs in the stream form of `io_traits`, which gets the real stream
 *       and can do more than `ios_base&` allows.
 * @param obj The output stream.
 * @param pf The manipulator; a null pointer sets `strfailbit`.
 * @return A reference to the stream itself.
 * @endif
 */
template <ostream_type T>
T& operator<<(T& obj, void (*pf)(ios_base<typename T::char_type>&))
{
    apply_ios_manip<typename T::char_type>(obj, pf);
    return obj;
}

}

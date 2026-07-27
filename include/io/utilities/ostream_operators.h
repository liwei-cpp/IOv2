/**
 * @file ostream_operators.h
 * @lang{ZH}
 * 定义了输出流的格式化与非格式化插入设施。
 * 包含输出哨兵 `out_sentry`、承载多态 `flush()` 的 CRTP 基类 `out_flusher`、
 * 输出流概念 `ostream_type`、承载各类插入操作（`put`/`write`）的 `ostream_operators`
 * 混入基类，以及插入运算符 `operator<<`（含操纵符重载）。
 * @endif
 *
 * @lang{EN}
 * Defines the formatted and unformatted insertion facilities for output streams.
 * Includes the output sentry `out_sentry`, the CRTP base `out_flusher` that carries the
 * polymorphic `flush()`, the output-stream concept `ostream_type`, the `ostream_operators`
 * mix-in base that carries the various insertion operations (`put`/`write`), and the
 * insertion `operator<<` (including manipulator overloads).
 * @endif
 */
#pragma once

#include <common/defs.h>
#include <common/metafunctions.h>
#include <device/device_concepts.h>
#include <io/fp_defs/base_fp.h>
#include <io/io_base.h>
#include <io/streambuf_iterator.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 输出操作的 RAII 哨兵：在每次插入操作的入口统一完成前置准备与末尾自动刷新。
 *
 * 哨兵负责校验流的有效性、刷新关联（tie）流、获取流锁，并在需要时切换读写方向、
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
 * The sentry validates the stream, flushes the tied stream, acquires the stream lock, and
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
     * @brief 哨兵所用锁的类型，即针对流 `io_mutex()` 返回的互斥量的 `std::unique_lock`。
     * @endif
     *
     * @lang{EN}
     * @brief The lock type used by the sentry: a `std::unique_lock` over the mutex
     *        returned by the stream's `io_mutex()`.
     * @endif
     */
    using lock_type =
        std::unique_lock<std::remove_reference_t<decltype(std::declval<TStream&>().io_mutex())>>;

public:
    /**
     * @lang{ZH}
     * @brief 构造输出哨兵：校验流、（在加锁前）刷新关联流、加锁，并按需切换读写方向 / 定位到末尾。
     *
     * 锁不由哨兵自身持有，而是从调用方借入（`lock`，须处于 `defer_lock` 状态）：哨兵在构造中
     * 对其加锁，但其生命周期由调用方的局部变量掌握。因此当哨兵在 `try` 块末尾析构后，锁仍被
     * 调用方持有，`catch` 中的 `handle_exception` 得以在持锁状态下更新流状态，使成功路径与失败
     * 路径对同一把 `io_mutex()` 的可见性保持一致。析构中的 unitbuf/stdio 刷新同样在这把仍被持有
     * 的锁下进行（同一递归锁、同一线程），故哨兵析构先于调用方的锁析构时刷新依旧安全。
     *
     * 关联流的刷新在加锁之前完成，保证任一时刻本线程至多持有一把流锁，维持不死锁保证。
     * @param os 要操作的输出流。
     * @param is_unit_buf 是否为 unitbuf 流（决定析构时是否自动刷新并对设备 `dflush`）。
     * @param is_app_mode 是否为追加模式；非标准流在追加模式下会在构造时定位到末尾。
     * @param lock 从调用方借入的锁，必须处于 `defer_lock` 状态；哨兵在构造时对其加锁。
     * @throw stream_error 若流无效。
     * @throw cvt_error 若追加模式下无法定位到末尾（追加模式要求定长、与状态无关的编码）。
     * @endif
     * @lang{EN}
     * @brief Constructs the output sentry: validates the stream, flushes the tied stream
     * (before locking), acquires the lock, and switches direction / repositions to end as
     * needed.
     *
     * The lock is not owned by the sentry but borrowed from the caller (`lock`, which must be
     * in `defer_lock` state): the sentry locks it during construction, yet its lifetime is
     * owned by the caller's local variable. Thus, after the sentry is destroyed at the end of
     * the enclosing `try`, the lock is still held by the caller, so `handle_exception` in the
     * `catch` can update the stream state while holding the lock, keeping the success and
     * failure paths consistent with respect to the same `io_mutex()`. The unitbuf/stdio flush
     * in the destructor also runs under that still-held lock (same recursive mutex, same
     * thread), so flushing remains safe even though the sentry is destroyed before the caller's
     * lock.
     *
     * The tied stream is flushed before locking, so at most one stream lock is held by this
     * thread at any time, preserving the no-deadlock guarantee.
     * @param os The output stream to operate on.
     * @param is_unit_buf Whether this is a unitbuf stream (governs the auto-flush and device
     *                    `dflush` on destruction).
     * @param is_app_mode Whether this is append mode; a non-standard stream repositions to the
     *                    end during construction in append mode.
     * @param lock The lock borrowed from the caller; must be in `defer_lock` state. The sentry
     *             locks it during construction.
     * @throw stream_error If the stream is invalid.
     * @throw cvt_error If append mode cannot reposition to the end (append mode requires a
     *                  fixed-length, state-independent encoding).
     * @endif
     */
    out_sentry(TStream& os, bool is_unit_buf, bool is_app_mode, lock_type& lock)
        : m_os(os)
        , m_lock(lock)
        , m_is_unit_buf(is_unit_buf)
    {
        if (!static_cast<bool>(m_os))
            throw stream_error("ostream_sentry create fail: Invalid ostream");

        if (auto* tied = m_os.tie())
        {
            try { tied->flush(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }

        m_lock.lock();

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
     * `dflush()`）。若析构时当前线程没有任何异常正在展开（以 `std::uncaught_exceptions() == 0`
     * 判定）——则刷新失败会经 `handle_exception` 上报：按类别置 `devfailbit`/`cvtfailbit`/
     * `strfailbit`，并在该位处于异常掩码时抛出；该异常会被发起本次输出的操作自身的 try/catch
     * 接住，并按掩码传播给调用者。无失败位入掩码时（默认）只置位不抛，因此正常输出路径不产生
     * 异常开销。
     *
     * 反之，只要析构时已有任何异常正在展开（无论其是否早于本哨兵构造即已在飞），则仍尝试刷新但
     * **吞掉任何异常、绝不抛出**，以免在栈展开期间抛异常导致 `std::terminate`；此分支保持与旧
     * 实现一致的“尽力刷新并吞掉”行为。
     *
     * 为使正常退出分支的通知得以传播，本析构声明为 `noexcept(false)`。
     * @endif
     *
     * @lang{EN}
     * @brief On destruction, auto-flushes a unitbuf / stdio-synced stream and decides whether
     * to report a flush failure according to the exception mask.
     *
     * For a stream with unitbuf set or synced with stdio, destruction flushes the buffer via
     * `flush()` (and, for unitbuf, `dflush()`es the device). If no exception is currently
     * propagating on this thread when the sentry is destroyed — determined by
     * `std::uncaught_exceptions() == 0` — a flush failure is reported through
     * `handle_exception`: it sets `devfailbit`/`cvtfailbit`/`strfailbit` by category and
     * throws when that bit is in the exception mask; the thrown exception is caught by the
     * originating output operation's own try/catch and propagated to the caller per the mask.
     * When no such fail bit is in the mask (the default) the bit is only set and nothing is
     * thrown, so the normal output path incurs no exception overhead.
     *
     * If instead any exception is already unwinding when the sentry is destroyed — including
     * one that was already in flight before this sentry was constructed — it still attempts
     * the flush but **swallows any exception and never throws**, so as not to throw during
     * stack unwinding and trigger `std::terminate`; this branch preserves the prior
     * "best-effort flush and swallow" behavior.
     *
     * To let the normal-exit notification propagate, this destructor is declared
     * `noexcept(false)`.
     * @endif
     */
    ~out_sentry() noexcept(false)
    {
        if (std::uncaught_exceptions() != 0)
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
                try { m_os.template handle_exception<true>(std::current_exception()); }
                catch (...) {} // NOLINT(bugprone-empty-catch)
            }
            return;
        }

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
    lock_type&  m_lock;
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
 * @brief 承载多态 `flush()` 接口的抽象基类。
 *
 * 提供纯虚的 `flush()`，使得可通过基类指针以类型无关的方式刷新任意输出流；具体实现由
 * CRTP 派生类 `out_flusher<T>` 给出。
 * @endif
 *
 * @lang{EN}
 * @brief Abstract base carrying the polymorphic `flush()` interface.
 *
 * Provides a pure virtual `flush()` so that any output stream can be flushed type-erased
 * through a base pointer; the concrete implementation is given by the CRTP-derived
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
     * @brief 刷新本流的纯虚接口；由 `out_flusher<T>` 实现。
     * @endif
     *
     * @lang{EN}
     * @brief Pure virtual interface to flush this stream; implemented by `out_flusher<T>`.
     * @endif
     */
    virtual void flush() = 0;
};

/**
 * @lang{ZH}
 * @brief 承载多态 `flush()` 的 CRTP 基类：对具体流类型 `T` 的向下转型集中于此。
 *
 * `flush()` 覆盖 `abs_flusher::flush`，而虚函数无法使用 deducing-this，只能
 * `static_cast<T&>(*this)` 取回具体流类型。单独引入本模板承载该 `T`，从而让
 * `ostream_operators` 不必再携带 CRTP 自身参数（与 `istream_operators<TChar>` 对称）。
 * 每个输出流同时派生 `out_flusher<自身>` 与 `ostream_operators<TChar>`。
 * @tparam T 具体的输出流类型。
 * @endif
 * @lang{EN}
 * @brief CRTP base carrying the polymorphic `flush()`: the down-cast to the concrete stream
 * type `T` is localized here.
 *
 * `flush()` overrides `abs_flusher::flush`; a virtual cannot use deducing-this, so it must
 * `static_cast<T&>(*this)` to recover the concrete stream type. This template exists solely
 * to carry that `T`, letting `ostream_operators` drop its CRTP self-parameter (making it
 * symmetric with `istream_operators<TChar>`). Every output stream derives from both
 * `out_flusher<Self>` and `ostream_operators<TChar>`.
 * @tparam T The concrete output stream type.
 * @endif
 */
template <typename T>
struct out_flusher : public abs_flusher
{
    /**
     * @lang{ZH}
     * @brief 刷新本流：把缓冲区写出到底层设备。
     *
     * 刷新不经 sentry，而是直接以 `std::lock_guard` 持有本流的 `io_mutex()`（该锁为递归锁）
     * 直至刷新结束。由此：
     *   - **并发刷新被串行化**：多个线程同时 `flush()` 同一流时逐个进入，每个都真正完成一次
     *     刷新（不是“先到者刷、其余跳过”的弱语义）；底层缓冲区不会被并发操作。
     *   - **写与刷互斥**：`put`/`write`/`operator<<` 在其 sentry 生命周期内持有同一把
     *     `io_mutex()`，故写与刷不会并发。
     *
     * 本函数**不刷新 tie 流**：与标准 `std::basic_ostream::flush` 一致，刷新只作用于本流，
     * 关联流的刷新仅由输出操作的 sentry 在其入口触发。因刷新不再沿 tie 链传播，也就不存在
     * 递归回到本流的可能，无需任何“正在刷新”自旋/跳过标志。
     *
     * 输出前的读写模式切换由 `streambuf::flush()` 自行完成（其内部会 `switch_to_put()`），
     * 故此处无需 sentry 代劳。
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
     * @endif
     */
    void flush() override
    {
        T& obj = static_cast<T&>(*this);
        std::lock_guard guard(obj.io_mutex());
        if (!static_cast<bool>(obj)) return;
        try
        {
            if constexpr (dev_cpt::support_put<typename T::device_type>)
            {
                obj.m_streambuf.flush();
                obj.m_streambuf.device().dflush();
            }
            else
                throw stream_error("out_flusher::flush fail: device does not support output");
        }
        catch(...)
        {
            obj.handle_exception(std::current_exception());
        }
    }
};

template <typename TChar>
struct ostream_operators;

/**
 * @lang{ZH}
 * @brief 输出流类型的概念。
 *
 * 一个类型要成为输出流，必须提供 `out_sentry_type` 与 `char_type` 类型、可返回其
 * locale，且其 `out_sentry_type` 满足 `is_out_sentry`；同时它必须同时派生自
 * `ios_base<char_type>` 与 `ostream_operators<char_type>`。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept for an output stream type.
 *
 * To qualify as an output stream, a type must expose the `out_sentry_type` and `char_type`
 * types, be able to return its locale, and have an `out_sentry_type` that satisfies
 * `is_out_sentry`; it must also derive from both `ios_base<char_type>` and
 * `ostream_operators<char_type>`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename T>
concept ostream_type =
    requires (T a)
    {
        typename T::out_sentry_type;
        typename T::char_type;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_out_sentry<typename T::out_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>> &&
    std::derived_from<T, ostream_operators<typename T::char_type>>;

/**
 * @lang{ZH}
 * @brief 为输出流提供各类插入操作的混入（mix-in）基类。
 *
 * 本模板集中承载 `put`、`write` 等成员，供具体输出流类型派生使用；这些成员通过
 * deducing-this（`this TSelf& self`）以派生类的具体类型执行操作。每个操作都在其内部
 * 构造输出哨兵以完成加锁与前置准备，并将异常统一交由流的 `handle_exception` 处理，
 * 从而按异常掩码更新流状态。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Mix-in base that provides the various insertion operations for output streams.
 *
 * This template centralizes members such as `put` and `write` for concrete output stream
 * types to derive from; these members use deducing-this (`this TSelf& self`) to operate on
 * the concrete derived type. Each operation constructs an output sentry internally to handle
 * locking and setup, and routes exceptions through the stream's `handle_exception`, which
 * updates the stream state according to the exception mask.
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
     * @note 本参数存在的原因：`out_sentry` 本就把"是否 unitbuf"作为构造参数接收，所以
     *       "这一次要不要刷新"从来都是一个**局部**信息。若没有它，像 `endl` 这样需要强制
     *       刷新的操纵符只能绕道去临时置位再复位全局的 `unitbuf` 标志，而那个读-改-写会
     *       跨越本函数、既非原子也非异常安全：并发下两个线程会各自读到对方的中间态、其中
     *       一个静默地不刷新，异常路径上则会把 `unitbuf` 永久遗留在流上。把它作为参数传入
     *       就从根上消除了这两种失败，也不再污染其它线程看到的格式标志。
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
     * @note Why this parameter exists: `out_sentry` already takes "is this unitbuf" as a
     *       constructor argument, so "should this particular call flush" has always been
     *       **local** information. Without it, a manipulator that needs a forced flush -- `endl`
     *       being the one -- has to detour through temporarily setting and clearing the global
     *       `unitbuf` flag, and that read-modify-write straddles this function while being
     *       neither atomic nor exception-safe: concurrently, two threads each observe the
     *       other's intermediate state and one of them silently skips its flush, while on an
     *       exception path `unitbuf` is left set on the stream for good. Passing it as an
     *       argument removes both failures at the root, and stops perturbing the format flags
     *       other threads observe.
     * @endif
     */
    template<typename TSelf>
    TSelf& put(this TSelf& self, TChar c, bool force_flush = false)
    {
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, force_flush || bool(self.flags() & ios_defs::unitbuf),
                             bool(self.flags() & ios_defs::appmode), lk);
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
    TSelf& write(this TSelf& self, const TChar* s, size_t n)
    {
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, bool(self.flags() & ios_defs::unitbuf), bool(self.flags() & ios_defs::appmode), lk);
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
     * @brief 取绑定到本流缓冲区的输出迭代器。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 绑定到本流缓冲区的 `ostreambuf_iterator`。
     * @endif
     *
     * @lang{EN}
     * @brief Gets an output iterator bound to this stream's buffer.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return An `ostreambuf_iterator` bound to this stream's buffer.
     * @endif
     */
private:
    template <typename TSelf>
    auto o_iter(this TSelf& self)
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
        requires (is_writer_def<typename U::char_type, TValue>
               || is_writer_def<typename U::char_type, std::decay_t<TValue>>)
    friend U& operator<<(U& obj, const TValue& value);
};

/**
 * @lang{ZH}
 * @brief 插入操纵符：应用一个作用于 `ios_base<char_type>&` 的函数指针操纵符。
 * @tparam T 输出流类型。
 * @param obj 输出流。
 * @param pf 操纵符函数指针。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion manipulator: applies a function-pointer manipulator taking
 *        `ios_base<char_type>&`.
 * @tparam T The output stream type.
 * @param obj The output stream.
 * @param pf The manipulator function pointer.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is null.
 * @endif
 */
template <ostream_type T>
T& operator << (T& obj, void(*pf)(ios_base<typename T::char_type>&))
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 插入操纵符：应用一个作用于 `ios_base<char_type>&` 的 `std::function` 操纵符。
 * @tparam T 输出流类型。
 * @param obj 输出流。
 * @param pf 操纵符可调用对象。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion manipulator: applies a `std::function` manipulator taking
 *        `ios_base<char_type>&`.
 * @tparam T The output stream type.
 * @param obj The output stream.
 * @param pf The manipulator callable.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is empty.
 * @endif
 */
template <ostream_type T>
T& operator << (T& obj, const std::function<void(ios_base<typename T::char_type>&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 插入操纵符：应用一个作用于具体流类型 `T&` 的函数指针操纵符。
 * @tparam T 输出流类型。
 * @param obj 输出流。
 * @param pf 操纵符函数指针。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion manipulator: applies a function-pointer manipulator taking the concrete
 *        stream type `T&`.
 * @tparam T The output stream type.
 * @param obj The output stream.
 * @param pf The manipulator function pointer.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is null.
 * @endif
 */
template <ostream_type T>
T& operator << (T& obj, void(*pf)(T&))
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 插入操纵符：应用一个作用于具体流类型 `T&` 的 `std::function` 操纵符。
 * @tparam T 输出流类型。
 * @param obj 输出流。
 * @param pf 操纵符可调用对象。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion manipulator: applies a `std::function` manipulator taking the concrete
 *        stream type `T&`.
 * @tparam T The output stream type.
 * @param obj The output stream.
 * @param pf The manipulator callable.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is empty.
 * @endif
 */
template <ostream_type T>
T& operator << (T& obj, const std::function<void(T&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 插入操纵符：应用一个作用于 `T` 某个基类 `U&` 的函数指针操纵符。
 * @tparam T 输出流类型。
 * @tparam U `T` 的基类类型。
 * @param obj 输出流。
 * @param pf 操纵符函数指针。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion manipulator: applies a function-pointer manipulator taking a base class
 *        `U&` of `T`.
 * @tparam T The output stream type.
 * @tparam U A base class type of `T`.
 * @param obj The output stream.
 * @param pf The manipulator function pointer.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is null.
 * @endif
 */
template <ostream_type T, typename U>
    requires std::derived_from<T, U>
T& operator << (T& obj, void(*pf)(U&))
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 插入操纵符：应用一个作用于 `T` 某个基类 `U&` 的 `std::function` 操纵符。
 * @tparam T 输出流类型。
 * @tparam U `T` 的基类类型。
 * @param obj 输出流。
 * @param pf 操纵符可调用对象。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Insertion manipulator: applies a `std::function` manipulator taking a base class
 *        `U&` of `T`.
 * @tparam T The output stream type.
 * @tparam U A base class type of `T`.
 * @param obj The output stream.
 * @param pf The manipulator callable.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is empty.
 * @endif
 */
template <ostream_type T, typename U>
    requires std::derived_from<T, U>
T& operator << (T& obj, const std::function<void(U&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 格式化插入运算符：按当前 locale 将一个 `TValue` 类型的值写入流。
 *
 * 借助为该值类型（或其退化类型 `std::decay_t<TValue>`）注册的 `writer` 完成格式化输出；
 * 优先匹配原类型，其次匹配退化类型。
 * @tparam T 输出流类型。
 * @tparam TValue 源值类型；必须存在对应的 `writer` 定义。
 * @param obj 输出流。
 * @param value 要写入的值。
 * @return 流自身的引用。
 * @note 异常统一交由 `handle_exception` 处理，并按异常掩码更新流状态。
 * @endif
 *
 * @lang{EN}
 * @brief Formatted insertion operator: writes a value of type `TValue` to the stream under
 *        the current locale.
 *
 * Formatting is delegated to the `writer` registered for the value type (or its decayed type
 * `std::decay_t<TValue>`); the original type is matched first, then the decayed type.
 * @tparam T The output stream type.
 * @tparam TValue The source value type; a corresponding `writer` definition must exist.
 * @param obj The output stream.
 * @param value The value to write.
 * @return A reference to the stream itself.
 * @note Exceptions are routed through `handle_exception`, which updates the stream state
 *       according to the exception mask.
 * @endif
 */
template <ostream_type T, typename TValue>
    requires (is_writer_def<typename T::char_type, TValue>
           || is_writer_def<typename T::char_type, std::decay_t<TValue>>)
T& operator<<(T& obj, const TValue& value)
{
    using TDecay = std::decay_t<TValue>;
    using TChar = typename T::char_type;
    std::unique_lock lk(obj.io_mutex(), std::defer_lock);
    try
    {
        using sentry_type = typename T::out_sentry_type;
        sentry_type cerb(obj, bool(obj.flags() & ios_defs::unitbuf), bool(obj.flags() & ios_defs::appmode), lk);

        if constexpr (is_writer_def<TChar, TValue>)
            writer<TChar, TValue>::swrite(obj.o_iter(), obj, obj.locale(), value);
        else if constexpr (is_writer_def<TChar, TDecay>)
            writer<TChar, TDecay>::swrite(obj.o_iter(), obj, obj.locale(), value);
        else
            static_assert(dependent_false_v<TValue>, "No format method provided");
    }
    catch(...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}
}

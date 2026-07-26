/**
 * @file stream_common_operators.h
 * @lang{ZH}
 * 定义了输入流与输出流共用的公共操作。
 * 包含保护整张 tie 图的进程级全局锁 `tie_graph_mutex()`，以及承载定位（`tell`/`seek`/
 * `rseek`）、底层设备管理（`device`/`detach`/`attach`）、转换行为调整（`adjust`/`retrieve`）、
 * locale 存取、`io_mutex` 访问与流绑定（`tie`）等操作的 `stream_common_operators` 混入基类。
 * @endif
 *
 * @lang{EN}
 * Defines the common operations shared by input and output streams.
 * Includes the process-wide lock `tie_graph_mutex()` that guards the entire tie graph, and the
 * `stream_common_operators` mix-in base carrying positioning (`tell`/`seek`/`rseek`), underlying
 * device management (`device`/`detach`/`attach`), conversion-behavior adjustment
 * (`adjust`/`retrieve`), locale access, `io_mutex` access, and stream tying (`tie`).
 * @endif
 */
#pragma once

#include <common/copyable_atomic.h>
#include <common/defs.h>
#include <common/iov2_export.h>
#include <cvt/cvt_concepts.h>
#include <io/io_base.h>
#include <io/utilities/ostream_operators.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <mutex>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 返回保护整张 tie 图的进程级全局锁。
 *
 * `tie()` setter 会以本锁把“沿目标链检测是否成环”与“提交新的 tie 指针”合成一个原子
 * 步骤。若无此锁，两个线程并发 `A.tie(B)` 与 `B.tie(A)` 可能各自读到对方尚未指回的旧
 * 状态、双双通过检测，从而形成环（详见 `tie()`）。有了本锁，所有 setter 串行化，任一
 * 次检测期间图都被冻结，故并发也无法成环。
 *
 * 本锁是**普通**（非递归）互斥量即可：检测遍历调用的是 tie() 的 getter（一次原子读，
 * 不取本锁），持锁期间不会重入 setter。它与各流的 `io_mutex()` 互不嵌套——setter 只取
 * 本锁、sentry 只取 `io_mutex()`——故不引入新的加锁顺序约束。
 *
 * 采用按需构造的函数内静态量（Meyers 单例）：`tie()` 在静态初始化期间即被调用
 * （如 `__cerr` 构造时 `tie(&cout)`），懒构造保证“首次使用前必已构造”，天然无静态
 * 初始化顺序问题。为在共享库（DSO）模式下仍是**全进程唯一**一份，本函数在 `IOV2_SHARED`
 * 下只声明、定义集中于 `iov2_objects.cpp` 并经 `IOV2_API` 导出；header-only 模式下则为
 * inline 定义。
 * @return 保护整张 tie 图的进程级全局互斥量的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Returns the process-wide lock that guards the entire tie graph.
 *
 * The `tie()` setter uses this lock to fuse "walk the target chain to detect a cycle"
 * and "commit the new tie pointer" into one atomic step. Without it, two threads doing
 * `A.tie(B)` and `B.tie(A)` concurrently could each read the other's not-yet-updated
 * state, both pass the check, and form a cycle (see `tie()`). With it, all setters are
 * serialized and the graph is frozen for the duration of any walk, so no cycle can form
 * even under concurrency.
 *
 * A **plain** (non-recursive) mutex suffices: the detection walk calls tie()'s getter (a
 * single atomic load that does not take this lock), so a setter never re-enters while
 * holding it. This lock never nests with a stream's `io_mutex()` -- the setter takes only
 * this lock, a sentry takes only `io_mutex()` -- so it adds no new lock-ordering
 * constraint.
 *
 * It is a lazily-constructed function-local static (Meyers singleton): `tie()` runs during
 * static initialization (e.g. `__cerr`'s ctor does `tie(&cout)`), and lazy construction
 * guarantees "constructed before first use", so there is no static-init-order problem. To
 * stay a single process-wide instance under shared-library (DSO) mode, this function is
 * only declared under `IOV2_SHARED` with its one definition living in `iov2_objects.cpp`
 * and exported via `IOV2_API`; in header-only mode it is defined inline here.
 * @return A reference to the process-wide mutex that guards the entire tie graph.
 * @endif
 */
#if defined(IOV2_SHARED)
IOV2_API std::mutex& tie_graph_mutex();     // defined in iov2_objects.cpp
#else
inline std::mutex& tie_graph_mutex()
{
    static std::mutex m;
    return m;
}
#endif

/**
 * @lang{ZH}
 * @brief 为输入流与输出流提供公共操作的混入（mix-in）基类。
 *
 * 本结构集中承载定位、底层设备管理、转换行为调整、locale 存取、`io_mutex` 访问与流绑定
 * 等成员，供具体流类型派生使用；这些成员通过 deducing-this（`this TSelf& self`）以派生类
 * 的具体类型执行操作。除显式标注为**不做线程同步**者（`device`/`detach`/`attach`）外，
 * 其余操作均持有本流的 `io_mutex()`，并将异常统一交由 `handle_exception` 处理。
 * 本结构还持有本流所绑定（tie）的目标指针。
 * @endif
 *
 * @lang{EN}
 * @brief Mix-in base that provides operations common to input and output streams.
 *
 * This struct centralizes members for positioning, underlying device management,
 * conversion-behavior adjustment, locale access, `io_mutex` access, and stream tying, for
 * concrete stream types to derive from; these members use deducing-this (`this TSelf& self`)
 * to operate on the concrete derived type. Except for those explicitly documented as **not
 * synchronized** (`device`/`detach`/`attach`), all operations hold the stream's `io_mutex()`
 * and route exceptions through `handle_exception`. This struct also holds the pointer to the
 * stream this one is tied to.
 * @endif
 */
struct stream_common_operators
{
    /**
     * @lang{ZH}
     * @brief 返回底层流的当前位置。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 当前位置；若流处于失败状态或发生异常，则返回 `static_cast<size_t>(-1)`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the current position of the underlying stream.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The current position; `static_cast<size_t>(-1)` if the stream is in a failed
     *         state or an exception occurs.
     * @endif
     */
    template <typename TSelf>
    size_t tell(this TSelf& self)
    {
        std::lock_guard guard(self.io_mutex());
        if (!static_cast<bool>(self))
            return static_cast<size_t>(-1);
        try
        {
            return self.m_streambuf.tell();
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return static_cast<size_t>(-1);
    }

    /**
     * @lang{ZH}
     * @brief 从头（起始处）定位到绝对位置 `pos`。
     *
     * 定位前会清除 `eofbit`。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param pos 相对起始处的目标位置。
     * @return 流自身的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Seeks to the absolute position `pos` measured from the beginning.
     *
     * `eofbit` is cleared before seeking.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param pos The target position relative to the beginning.
     * @return A reference to the stream itself.
     * @endif
     */
    template <typename TSelf>
    TSelf& seek(this TSelf& self, size_t pos)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            self.unset_state(ios_defs::eofbit);
            self.m_streambuf.seek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 从尾（末尾处）定位到位置 `pos`。
     *
     * 定位前会清除 `eofbit`。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param pos 相对末尾处的目标位置。
     * @return 流自身的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Seeks to the position `pos` measured from the end.
     *
     * `eofbit` is cleared before seeking.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param pos The target position relative to the end.
     * @return A reference to the stream itself.
     * @endif
     */
    template <typename TSelf>
    TSelf& rseek(this TSelf& self, size_t pos)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            self.unset_state(ios_defs::eofbit);
            self.m_streambuf.rseek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 返回底层设备的引用。
     *
     * @warning 本操作**不做线程同步**，不获取 `io_mutex()`。返回的是绑定到底层设备的引用，
     *          **不要把它保存到临界区之外**再使用：在并发变更（如 `attach`/`detach`）下，
     *          离开锁的保护后读取该引用即为数据竞争与未定义行为。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 底层设备的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns a reference to the underlying device.
     *
     * @warning This operation is **not synchronized** and takes no `io_mutex()`. It returns a
     *          reference bound to the underlying device; **do not keep it past the critical
     *          section**: under concurrent mutation (e.g. `attach`/`detach`), reading the
     *          reference outside the lock is a data race and undefined behavior.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return A reference to the underlying device.
     * @endif
     */
    template <typename TSelf>
    auto& device(this TSelf& self)
    {
        return self.m_streambuf.device();
    }

    /**
     * @lang{ZH}
     * @brief 分离并取回底层设备（连同分离期间捕获的错误）。
     *
     * @warning 本操作**不做线程同步**：与本流的其它操作（`tell`/`seek`/格式化 I/O 等均持有
     *          `io_mutex()`）不同，`detach()` 不获取任何锁。它是一个类似构造/析构的生命周期
     *          操作——分离底层设备本就意味着流不再处于可用于读写的稳定状态。调用方必须保证在
     *          `detach()` 执行期间没有任何其它线程对本流进行操作（读、写，或再次 attach/detach），
     *          否则行为未定义；正如不应在对象构造/析构过程中使用该对象一样。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 取回的设备，以及一个 `exception_ptr`（分离时若发生 flush 等错误则非空）。
     * @endif
     *
     * @lang{EN}
     * @brief Detaches and retrieves the underlying device (along with any error captured
     *        during detach).
     *
     * @warning This operation is **not synchronized**: unlike the stream's other operations
     *          (`tell`/`seek`/formatted I/O, which all hold `io_mutex()`), `detach()` takes no
     *          lock. It is a lifecycle operation akin to construction/destruction -- detaching
     *          the underlying device inherently means the stream is no longer in a stable state
     *          usable for I/O. The caller must ensure that no other thread operates on this
     *          stream (reading, writing, or another attach/detach) while `detach()` runs;
     *          otherwise the behavior is undefined, just as one must not use an object while it
     *          is being constructed or destroyed.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The retrieved device and an `exception_ptr` (non-null if e.g. a flush error
     *         occurred during detach).
     * @endif
     */
    template <typename TSelf>
    auto detach(this TSelf& self) noexcept
    {
        return self.m_streambuf.detach();
    }

    /**
     * @lang{ZH}
     * @brief 安装（替换）底层设备。
     *
     * @warning 与 `detach()` 相同，本操作**不做线程同步**、不获取 `io_mutex()`。它是类似构造的
     *          生命周期操作：替换底层设备期间，任何并发读写本身都是不稳定且无意义的。调用方必须
     *          保证在 `attach()` 执行期间没有任何其它线程对本流进行操作，否则行为未定义。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param dev 要安装的设备；默认为默认构造的设备。
     * @endif
     *
     * @lang{EN}
     * @brief Installs (replaces) the underlying device.
     *
     * @warning Like `detach()`, this operation is **not synchronized** and takes no
     *          `io_mutex()`. It is a construction-like lifecycle operation: any concurrent
     *          read/write while the underlying device is being replaced is itself unstable and
     *          meaningless. The caller must ensure that no other thread operates on this stream
     *          while `attach()` runs; otherwise the behavior is undefined.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param dev The device to install; defaults to a default-constructed device.
     * @endif
     */
    template <typename TSelf>
    void attach(this TSelf& self, typename TSelf::device_type&& dev = typename TSelf::device_type{})
    {
        self.m_streambuf.attach(std::move(dev));
        self.unset_state(ios_defs::eofbit);
    }

    /**
     * @lang{ZH}
     * @brief 调整底层编码转换的行为。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param acc 要应用的转换行为设置。
     * @endif
     *
     * @lang{EN}
     * @brief Adjusts the behavior of the underlying encoding conversion.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param acc The conversion-behavior settings to apply.
     * @endif
     */
    template <typename TSelf>
    void adjust(this TSelf& self, const cvt_behavior& acc)
    {
        std::lock_guard guard(self.io_mutex());
        return self.m_streambuf.adjust(acc);
    }

    /**
     * @lang{ZH}
     * @brief 取回底层编码转换的当前状态。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param acc 用于接收转换状态的输出参数。
     * @endif
     *
     * @lang{EN}
     * @brief Retrieves the current state of the underlying encoding conversion.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param acc Output parameter receiving the conversion status.
     * @endif
     */
    template <typename TSelf>
    void retrieve(this const TSelf& self, cvt_status& acc)
    {
        std::lock_guard guard(self.io_mutex());
        return self.m_streambuf.retrieve(acc);
    }

    /**
     * @lang{ZH}
     * @brief 返回本流当前使用的 locale（getter）。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 绑定到本流 `m_locale` 的常量引用。
     * @warning 返回的是引用，**不要把它保存到临界区之外**再使用：在并发调用 `locale(loc)`
     *          setter 时，离开锁的保护后读取该引用即为数据竞争与未定义行为。详见 locale setter。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale currently used by this stream (getter).
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return A const reference bound to this stream's `m_locale`.
     * @warning This returns a reference; **do not keep it past the critical section**: while a
     *          `locale(loc)` setter runs concurrently, reading the reference outside the lock is
     *          a data race and undefined behavior. See the locale setter for details.
     * @endif
     */
    template <typename TSelf>
    const auto& locale(this const TSelf& self)
    {
        return self.m_locale;
    }

    /**
     * @lang{ZH}
     * @brief 设置本流使用的 locale，并返回先前的 locale。
     *
     * 新 locale 生效后会立即以其调用 `access_callbacks`，使已注册的 facet 相关回调据以
     * 刷新缓存。
     *
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @tparam TChar locale 的字符类型。
     * @param loc 要设置的新 locale（按值接收，内部移动入 `m_locale`）。
     * @return 先前的 locale（已被移出并通过返回值转交调用方）。
     *
     * @note 回调失败时的行为：新 locale 的安装（`m_locale` 的 move-assign）在调用回调**之前**
     *       就已完成，且 `access_callbacks` 会先把所有回调**全部执行完毕**、再重抛其中第一个
     *       捕获的异常（见 `access_callbacks`）。因此回调抛出并不意味着本次设置被中断或只完成了
     *       一半：新 locale 已生效，全部回调也都已执行，异常只是“其中至少有一个失败了”的事后
     *       报告。本 setter **不做任何回滚**——回滚会让已按新 locale 重建完毕的 pword 缓存与被
     *       还原的旧 locale 相互矛盾，且为修复它而重跑一遍回调同样可能失败。
     * @note 该异常随后交由 `handle_exception` 归类：置上相应失败位（`stream_error`→`strfailbit`、
     *       `cvt_error`→`cvtfailbit` 等），并**仅当该位处于异常掩码中时**才向调用方传播。
     *       故默认（掩码为空）情况下本函数不抛出，正常返回旧 locale，仅留下一个失败位供检查；
     *       若该位在掩码中则本函数抛出，此时旧 locale 随返回值一同丢失，无法取回。
     * @note 本 setter 自身持有本流的 `io_mutex()`。由于 `operator>>`/`operator<<`/`get`/`put`
     *       等格式化 I/O 在其 sentry 生命周期内持有同一把锁，本次 move-assign `m_locale` 绝不会
     *       落在某次格式化操作的中途——格式化过程内部持有的 `locale()` 引用因此始终有效。
     * @warning 仍属调用方责任的是：`locale()` getter 返回的是绑定到 `m_locale` 的引用，**不要
     *          把它保存到临界区之外**再使用；一旦离开锁的保护，另一线程的 `locale(loc)` 会把它
     *          指向的对象移走，读取即为数据竞争与未定义行为。若需跨多次操作稳定持有，请以
     *          `IOv2::sync` 把这些操作圈进同一个临界区。
     * @note 此约束并非 locale 专有：凡返回内部状态引用的 getter（如 `device()`）在并发变更下都
     *       有同样要求。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the locale used by this stream and returns the previous one.
     *
     * Once the new locale takes effect, `access_callbacks` is invoked with it immediately so
     * that registered facet-related callbacks refresh their caches accordingly.
     *
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @tparam TChar The character type of the locale.
     * @param loc The new locale to install (taken by value, moved into `m_locale`).
     * @return The previous locale (moved out and handed back to the caller).
     *
     * @note Behavior when a callback fails: the new locale is installed (the move-assignment
     *       of `m_locale`) *before* the callbacks are invoked, and `access_callbacks` runs
     *       *every* callback to completion before rethrowing the first exception it captured
     *       (see `access_callbacks`). A throwing callback therefore does not mean the set was
     *       interrupted or half-applied: the new locale is in effect and all callbacks have
     *       run; the exception is merely an after-the-fact report that at least one of them
     *       failed. This setter performs **no rollback** -- restoring the old locale would
     *       contradict the pword caches already rebuilt for the new one, and re-running the
     *       callbacks to repair that could fail just the same.
     * @note The exception is then categorized by `handle_exception`: it sets the matching
     *       failure bit (`stream_error`→`strfailbit`, `cvt_error`→`cvtfailbit`, etc.) and
     *       propagates to the caller **only if that bit is in the exception mask**. So by
     *       default (empty mask) this function does not throw: it returns the previous locale
     *       normally and merely leaves a failure bit to be inspected. If the bit is in the
     *       mask this function throws, and the previous locale is lost along with the return
     *       value -- it cannot be recovered.
     * @note This setter itself holds the stream's `io_mutex()`. Since formatted I/O
     *       (`operator>>`/`operator<<`/`get`/`put`) holds that same lock for its sentry's
     *       lifetime, this move-assignment of `m_locale` can never land in the middle of a
     *       formatting operation, so the `locale()` reference that formatting holds internally
     *       stays valid throughout.
     * @warning What remains the caller's responsibility: the `locale()` getter returns a
     *          reference bound to `m_locale` — **do not keep it past the critical section**.
     *          Outside the lock, another thread's `locale(loc)` can move the referenced object
     *          out from under you, and reading it is a data race and undefined behavior. To
     *          hold it stably across several operations, group them into one critical section
     *          with `IOv2::sync`.
     * @note This is not specific to locale: every getter returning a reference to internal
     *       state (e.g. `device()`) carries the same requirement under concurrent mutation.
     * @endif
     */
    template <typename TSelf, typename TChar>
    auto locale(this TSelf& self, IOv2::locale<TChar> loc)
    {
        std::lock_guard guard(self.io_mutex());
        auto res = std::move(self.m_locale);
        self.m_locale = std::move(loc);
        try
        {
            self.access_callbacks(self.m_locale);
        }
        catch (...)
        {
            self.handle_exception(std::current_exception());
        }
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 返回本流用于同步 I/O 的互斥量。
     *
     * 该锁为递归锁，格式化 I/O 与各定位/存取操作在其临界区内持有它；可配合 `IOv2::sync`
     * 将多次操作圈进同一临界区。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 本流 `m_io_mutex` 的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the mutex this stream uses to synchronize I/O.
     *
     * The lock is recursive; formatted I/O and the positioning/access operations hold it within
     * their critical sections. It can be combined with `IOv2::sync` to group several operations
     * into one critical section.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return A reference to this stream's `m_io_mutex`.
     * @endif
     */
    template <typename TSelf>
    auto& io_mutex(this TSelf& self)
    {
        return self.m_io_mutex;
    }

    /**
     * @lang{ZH}
     * @brief 设置本流所绑定（tie）的输出流，并返回先前绑定的流。
     *
     * 绑定后，本流在每次 I/O 前都会（经 sentry）调用 `tie()->flush()`，以保证例如提示符
     * 先于其对应的输入被刷出。
     *
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param str 要绑定的输出流；传 `nullptr` 解除绑定。
     * @return 先前绑定的输出流（可能为 `nullptr`）。
     *
     * @warning 生命周期由调用方负责：`str` 仅以裸指针保存，本类不做任何生命周期管理，
     *          也不会在析构时自动解绑。最简单且始终安全的规则是：让 `str` 所指的流存活于
     *          所有绑定到它的流之后（这也是标准用法，如全局 `cout` 活得比 `cin` 久）。否则
     *          任何 I/O 都会经由 `tie()->flush()` 解引用悬空指针，导致未定义行为。此契约与
     *          标准库 `std::basic_ios::tie` 一致。
     * @warning **并发下的加强约束**：单独调用 `tie(nullptr)` 并**不足以**使随后销毁 `str`
     *          变得安全。sentry 会在获取本流 `io_mutex()` **之前**就（无锁）原子读出 tie 指针
     *          并 `flush()` 该目标（见 in_sentry/out_sentry 构造函数），因此另一线程在途的
     *          I/O 可能在你的 `tie(nullptr)` 写入变得可见之前，就已读到旧指针并正准备对目标
     *          `flush()`。故若要在生命周期中途改绑或销毁某个 tie 目标 P，调用方必须：先保证
     *          没有任何线程正在、也不会即将对任何 `tie() == P` 的流发起 I/O；再对这些流调用
     *          `tie(nullptr)`（或确保其已析构）；最后才销毁 P。这一点无法用锁代劳——目标的
     *          生命周期不受本流 `io_mutex()` 保护，且没有任何锁能保护一个正被其自身析构函数
     *          销毁的对象。
     * @note 设置前会沿目标流 `str` 的 tie 链向前遍历：若该链会回到本流（即形成环，自绑定是
     *       长度为 1 的环），则抛出 `stream_error` 并**保持本流原绑定不变**，从而在设置时杜绝
     *       环。这满足了 `std::basic_ios::tie` “不得成环”的前置条件，而非像标准那样把成环留作
     *       未定义行为。仅当本流本身可作为 tie 目标（即派生自 `abs_flusher`）时才会遍历；纯输入
     *       流不可能被 tie，也就不可能出现在环中。
     * @note 上述“检测 + 提交”由进程级全局锁 `tie_graph_mutex()` 合成一个原子步骤，因此即便
     *       两个线程并发 `tie()`（如 `A.tie(B)` 与 `B.tie(A)`）也无法成环：所有 setter 串行化，
     *       任一次检测遍历期间整张 tie 图都被冻结，绝不会出现“各自读到对方旧状态、双双通过检测”
     *       的窗口。该锁为**普通**互斥量即可——遍历调用的是 tie() 的 getter（一次原子读、不取该
     *       锁），持锁期间不会重入 setter；它也不与各流的 `io_mutex()` 嵌套，故不引入新的加锁
     *       顺序约束。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the output stream this stream is tied to and returns the previous one.
     *
     * Once tied, before every I/O operation this stream calls `tie()->flush()` (via the
     * sentry), so that e.g. a prompt is flushed before its matching input is read.
     *
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param str The stream to tie to; pass `nullptr` to untie.
     * @return The previously tied stream (may be `nullptr`).
     *
     * @warning Lifetime is the caller's responsibility: `str` is stored as a raw pointer;
     *          this class performs no lifetime management and does not auto-untie on
     *          destruction. The simplest always-safe rule is to let `str` outlive every
     *          stream tied to it (this is also the standard usage, e.g. a global `cout`
     *          outlives `cin`); otherwise any I/O dereferences a dangling pointer through
     *          `tie()->flush()` — undefined behavior. This matches the
     *          `std::basic_ios::tie` contract.
     * @warning **Concurrency addendum:** calling `tie(nullptr)` alone is **not** sufficient
     *          to make a subsequent destruction of `str` safe. The sentry reads the tie
     *          pointer (a lock-free atomic load) and `flush()`es the target *before* it
     *          acquires this stream's `io_mutex()` (see the in_sentry/out_sentry
     *          constructors), so another thread's in-flight I/O may have already latched the
     *          old pointer -- and be about to `flush()` the target -- before your
     *          `tie(nullptr)` store becomes visible. Therefore, to retarget or destroy a tie
     *          target P during its lifetime, the caller must: first ensure no thread is
     *          performing, or about to perform, I/O on any stream whose `tie() == P`; then
     *          `tie(nullptr)` those streams (or ensure they are already destroyed); and only
     *          then destroy P. No lock can do this for you -- the target's lifetime is not
     *          guarded by this stream's `io_mutex()`, and no lock can protect an object while
     *          it is being torn down by its own destructor.
     * @note Before setting, the tie chain reachable from the target `str` is walked
     *       forward: if that chain leads back to this stream (i.e. it would form a cycle;
     *       a self-tie is the length-1 cycle), a `stream_error` is thrown and **this
     *       stream's existing tie is left unchanged**, so cycles are rejected at set time.
     *       This satisfies the no-cycle precondition of `std::basic_ios::tie` rather than
     *       leaving a cycle as undefined behavior as the standard does. The walk runs only
     *       when this stream can itself be a tie target (i.e. derives from `abs_flusher`);
     *       a pure input stream can never be tied to, so it can never appear in a cycle.
     * @note This "detect + commit" is fused into one atomic step by the process-wide lock
     *       `tie_graph_mutex()`, so no cycle can form even under concurrent `tie()` (e.g.
     *       `A.tie(B)` and `B.tie(A)`): all setters are serialized and the whole tie graph
     *       is frozen for the duration of any detection walk, so the "each reads the other's
     *       stale state and both pass the check" window never exists. A **plain** mutex
     *       suffices -- the walk calls tie()'s getter (a single atomic load that does not
     *       take this lock), so a setter never re-enters while holding it -- and it never
     *       nests with a stream's `io_mutex()`, so it adds no new lock-ordering constraint.
     * @endif
     */
    template <typename TSelf>
    abs_flusher* tie(this TSelf& self, abs_flusher* str)
    {
        std::lock_guard graph_lock(tie_graph_mutex());
        auto res = self.m_tie_stream.load();
        if constexpr (std::derived_from<TSelf, abs_flusher>)
        {
            abs_flusher* ptr = str;
            auto check = static_cast<abs_flusher*>(&self);
            while (ptr != nullptr)
            {
                if (ptr == check)
                    throw stream_error("stream tie fail: the requested tie would form a cycle in the tie chain");
                auto* node = dynamic_cast<stream_common_operators*>(ptr);
                if (!node)
                    break;
                ptr = node->tie();
            }
        }
        self.m_tie_stream.store(str);
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 返回本流当前所绑定（tie）的输出流（getter）。
     *
     * 通过一次无锁的原子读取完成，不获取任何锁。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 当前绑定的输出流；未绑定时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the output stream this stream is currently tied to (getter).
     *
     * Performed by a single lock-free atomic load; takes no lock.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The currently tied output stream; `nullptr` if none.
     * @endif
     */
    template <typename TSelf>
    abs_flusher* tie(this const TSelf& self)
    {
        return self.m_tie_stream.load();
    }

private:
    /**
     * @lang{ZH}
     * @brief 本流所绑定（tie）目标的原子指针；`nullptr` 表示未绑定。
     * @endif
     *
     * @lang{EN}
     * @brief Atomic pointer to this stream's tie target; `nullptr` means untied.
     * @endif
     */
    copyable_atomic<abs_flusher*> m_tie_stream{nullptr};
};
}

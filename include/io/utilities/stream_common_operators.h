#pragma once

#include "common/defs.h"
#include <common/copyable_atomic.h>
#include <common/copyable_mutex.h>
#include <common/iov2_export.h>
#include <cvt/cvt_concepts.h>
#include <io/io_base.h>
#include <io/utilities/istream_operators.h>
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

struct stream_common_operators
{
    template <typename TSelf>
    size_t tell(this TSelf& self)
    {
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

    template <typename TSelf>
    TSelf& seek(this TSelf& self, size_t pos)
    {
        try
        {
            self.clear(self.rdstate() & ~ios_defs::eofbit);
            self.m_streambuf.seek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    TSelf& rseek(this TSelf& self, size_t pos)
    {
        try
        {
            self.clear(self.rdstate() & ~ios_defs::eofbit);
            self.m_streambuf.rseek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    auto& device(this TSelf& self)
    {
        return self.m_streambuf.device();
    }

    template <typename TSelf>
    auto detach(this TSelf& self) noexcept
    {
        return self.m_streambuf.detach();
    }

    template <typename TSelf>
    void attach(this TSelf& self, typename TSelf::device_type&& dev = typename TSelf::device_type{})
    {
        self.m_streambuf.attach(std::move(dev));
    }

    template <typename TSelf>
    void adjust(this TSelf& self, const cvt_behavior& acc)
    {
        return self.m_streambuf.adjust(acc);
    }

    template <typename TSelf>
    void retrieve(this const TSelf& self, cvt_status& acc)
    {
        return self.m_streambuf.retrieve(acc);
    }

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
     * @param loc 要设置的新 locale（按值接收，内部移动入 `m_locale`）。
     * @return 先前的 locale（已被移出并通过返回值转交调用方）。
     *
     * @warning 本 setter 是写操作，须由调用方外部同步（经 `io_mutex()`）。同步范围**不仅**是
     *          `locale(loc)` 这一次调用，而必须覆盖**整个** `operator>>`/`operator<<`/`get`/
     *          `put` 等格式化 I/O 期间：`locale()` getter 返回的是绑定到 `m_locale` 的引用，
     *          而格式化过程会在内部持有该引用直到操作结束。若在此期间另一路径对本流
     *          `locale(loc)`（move-assign `m_locale`）或以其它方式变更本流状态，正在进行的操作
     *          将读到被移走/被替换的对象，属数据竞争与未定义行为。
     * @note 此约束与 `flush()`/`write()`/`tie()` 的 `io_mutex()` 契约一致，并非 locale 专有：
     *       凡返回内部状态引用的 getter（如 `device()`）在并发变更下都有同样要求。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the locale used by this stream and returns the previous one.
     *
     * Once the new locale takes effect, `access_callbacks` is invoked with it immediately so
     * that registered facet-related callbacks refresh their caches accordingly.
     *
     * @param loc The new locale to install (taken by value, moved into `m_locale`).
     * @return The previous locale (moved out and handed back to the caller).
     *
     * @warning This setter is a write and must be serialized by the caller via `io_mutex()`.
     *          The synchronization scope is **not** merely the `locale(loc)` call itself but the
     *          **entire** duration of a formatted I/O operation (`operator>>`/`operator<<`/
     *          `get`/`put`): the `locale()` getter returns a reference bound to `m_locale`, and
     *          formatting holds that reference internally until the operation completes. If
     *          another path re-imbues this stream (move-assigning `m_locale`) — or otherwise
     *          mutates its state — while such an operation is in flight, the in-flight operation
     *          reads a moved-from/replaced object: a data race and undefined behavior.
     * @note This matches the `io_mutex()` contract of `flush()`/`write()`/`tie()` and is not
     *       specific to locale: every getter returning a reference to internal state (e.g.
     *       `device()`) carries the same requirement under concurrent mutation.
     * @endif
     */
    template <typename TSelf, typename TChar>
    auto locale(this TSelf& self, IOv2::locale<TChar> loc)
    {
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
     * @param str 要绑定的输出流；传 `nullptr` 解除绑定。
     * @return 先前绑定的输出流（可能为 `nullptr`）。
     *
     * @warning 生命周期由调用方负责：`str` 仅以裸指针保存，本类不做任何生命周期管理，
     *          也不会在析构时自动解绑。必须保证 `str` 所指的流在本流之后析构，或在 `str`
     *          被销毁前调用 `tie(nullptr)` 解绑；否则后续任何 I/O 都会经由 `tie()->flush()`
     *          解引用悬空指针，导致未定义行为。此契约与标准库 `std::basic_ios::tie` 一致。
     * @note 设置前会沿目标流 `str` 的 tie 链向前遍历：若该链会回到本流（即形成环，自绑定是
     *       长度为 1 的环），则抛出 `stream_error` 并**保持本流原绑定不变**，从而在设置时杜绝
     *       环。这满足了 `std::basic_ios::tie` “不得成环”的前置条件，而非像标准那样把成环留作
     *       未定义行为。仅当本流本身可作为 tie 目标（即派生自 `abs_ostream`）时才会遍历；纯输入
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
     * @param str The stream to tie to; pass `nullptr` to untie.
     * @return The previously tied stream (may be `nullptr`).
     *
     * @warning Lifetime is the caller's responsibility: `str` is stored as a raw pointer;
     *          this class performs no lifetime management and does not auto-untie on
     *          destruction. The caller must ensure `str` outlives this stream, or call
     *          `tie(nullptr)` before `str` is destroyed; otherwise any subsequent I/O
     *          dereferences a dangling pointer through `tie()->flush()` — undefined
     *          behavior. This matches the `std::basic_ios::tie` contract.
     * @note Before setting, the tie chain reachable from the target `str` is walked
     *       forward: if that chain leads back to this stream (i.e. it would form a cycle;
     *       a self-tie is the length-1 cycle), a `stream_error` is thrown and **this
     *       stream's existing tie is left unchanged**, so cycles are rejected at set time.
     *       This satisfies the no-cycle precondition of `std::basic_ios::tie` rather than
     *       leaving a cycle as undefined behavior as the standard does. The walk runs only
     *       when this stream can itself be a tie target (i.e. derives from `abs_ostream`);
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
    abs_ostream* tie(this TSelf& self, abs_ostream* str)
    {
        std::lock_guard graph_lock(tie_graph_mutex());
        auto res = self.m_tie_stream.load();
        if constexpr (std::derived_from<TSelf, abs_ostream>)
        {
            abs_ostream* ptr = str;
            auto check = static_cast<abs_ostream*>(&self);
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

    template <typename TSelf>
    abs_ostream* tie(this const TSelf& self)
    {
        return self.m_tie_stream.load();
    }

private:
    copyable_atomic<abs_ostream*> m_tie_stream{nullptr};
};
}

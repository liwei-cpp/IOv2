#pragma once

#include <common/copyable_mutex.h>
#include <cvt/cvt_concepts.h>
#include <io/io_base.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <utility>

namespace IOv2
{
template <typename TDevice, typename TChar>
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
    TDevice& device(this TSelf& self)
    {
        return self.m_streambuf.device();
    }

    template <typename TSelf>
    std::pair<TDevice, std::exception_ptr> detach(this TSelf& self) noexcept
    {
        return self.m_streambuf.detach();
    }

    template <typename TSelf>
    void attach(this TSelf& self, TDevice&& dev = TDevice{})
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
     * @note 允许被 tie 的流之间构成环：`flush()` 带有“正在刷新”保护，当一次刷新（无论是
     *       同一线程沿 tie 环重入，还是不同线程经各自的 tie 并发触发）进入某个正在刷新中的
     *       流时会直接返回，因此不会退化为 `std::basic_ios::tie` “不得成环”前置条件所暗示的
     *       无限递归。自绑定（tie 到自身）会在设置时被规范化为 `nullptr`。
     * @note 并发下的刷新为“先到者刷新，其余并发调用直接返回”：**跳过的线程返回时并不保证被
     *       tie 的流已刷新完成，只保证有某个线程正在刷新它**——详见 `flush()` 的说明。若某
     *       线程依赖“它本身没有写入、但要读取的目标流内容一定已经落盘”，需自行同步。
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
     * @note Cycles among tied streams are permitted: `flush()` carries an "already
     *       flushing" guard, so a flush that reaches a stream already being flushed —
     *       whether re-entered by the same thread along a tie cycle or triggered
     *       concurrently by another thread through its own tie — returns immediately,
     *       instead of recursing forever as the no-cycle precondition of
     *       `std::basic_ios::tie` would otherwise imply. A self-tie is additionally
     *       normalized to `nullptr` at set time.
     * @note Under concurrency the flush is "first caller flushes, other concurrent callers
     *       return immediately": **a skipping thread does not, on return, guarantee that
     *       the tied stream has finished flushing — only that some thread is flushing it**
     *       (see `flush()`). A thread relying on content it did not write itself being
     *       durably flushed before it reads must synchronize on its own.
     * @endif
     */
    template <typename TSelf>
    abs_ostream* tie(this TSelf& self, abs_ostream* str)
    {
        auto res = self.m_tie_stream;
        if constexpr (std::derived_from<TSelf, abs_ostream>)
        {
            if (str == static_cast<abs_ostream*>(&self))
                str = nullptr;
        }
        self.m_tie_stream = str;
        return res;
    }

    template <typename TSelf>
    abs_ostream* tie(this TSelf& self)
    {
        return self.m_tie_stream;
    }

    template <typename TSelf>
    const IOv2::locale<TChar>& locale(this const TSelf& self)
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
    template <typename TSelf>
    IOv2::locale<TChar> locale(this TSelf& self, IOv2::locale<TChar> loc)
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
    copyable_mutex& io_mutex(this TSelf& self)
    {
        return self.m_io_mutex;
    }
};
}

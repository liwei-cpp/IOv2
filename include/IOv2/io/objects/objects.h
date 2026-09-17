// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file objects.h
 * @lang{ZH}
 * 本库标准流的入口：带来八个全局流对象——`cin` / `cout` / `cerr` / `clog` 与四个宽字符对应物
 * `wcin` / `wcout` / `wcerr` / `wclog`（定义在 `in_impl.h` 与 `out_impl.h`）——以及一次切换全部
 * 八个流的自由函数 `sync_with_stdio()`。调用时机见 `stdin_api::sync_with_stdio`。
 *
 * 本头文件还包含 `IOv2/io/istream.h` 与 `IOv2/io/ostream.h`，因此四个不带参数的操纵符 `ws` /
 * `endl` / `ends` / `flush` 随之可用。这是入口有意做的 re-export：两个实现头只带流对象本身。
 *
 * @note **本头文件只带来流对象，不带来「往流里写一个数或一个字符串」的能力。** 与
 *       `IOv2/io/ostream.h` 一样，它不包含 `IOv2/io/traits/arithmetic.h` 与
 *       `IOv2/io/traits/char_and_str.h`，因此只 `#include <IOv2/io/objects/objects.h>` 时
 *       `cout << 42`、`cout << "abc"` 都编译不过，而 `cout << endl` 可以。这是按值类型选配的
 *       一贯做法（见 `IOv2/io/traits/traits_base.h`）：要读写哪类值就包哪个 traits 头，本文件
 *       不替你决定。
 * @note 八个流对象是进程级单例，但「一份」的范围随构建模式而变：`IOV2_SHARED` 下它们是
 *       `libiov2.so` 里唯一的一份，头文件模式下是本程序内的 `inline` 变量。`IOV2_SHARED`
 *       必须在同一次链接的所有翻译单元里口径一致，见 `IOv2/common/iov2_export.h` 的 `@warning`。
 * @note **哪些静态对象的构造 / 析构里可以用这八个流**：口径与 `std::cout`
 *       （[iostream.objects.overview]/3）相同——**同一个翻译单元里、定义在本头文件之后**的
 *       静态对象可以。它们的析构也一样安全：八个流退出时不析构（见 `out_impl.h` 的 `@note`）。
 *       其它两种形态取决于实现的初始化顺序，标准只保证 [basic.start.dynamic]/3.3 的
 *       indeterminately sequenced：不包含本头文件的翻译单元里的静态对象（实测取决于 `.o` 的
 *       链接顺序），以及**定义在本头文件包含之前**的静态对象（gcc 15 上是空引用，clang 21 上
 *       正常——两家都合规）。这两种形态请改用函数内静态量（首次使用时才构造）。
 * @endif
 *
 * @lang{EN}
 * The entry point for this library's standard streams: brings in the eight global stream
 * objects -- `cin` / `cout` / `cerr` / `clog` and the four wide-character counterparts
 * `wcin` / `wcout` / `wcerr` / `wclog` (defined in `in_impl.h` and `out_impl.h`) -- along
 * with the free function `sync_with_stdio()` that switches all eight at once. See
 * `stdin_api::sync_with_stdio` for when to call it.
 *
 * This header also includes `IOv2/io/istream.h` and `IOv2/io/ostream.h`, so the four
 * parameterless manipulators `ws` / `endl` / `ends` / `flush` come with it. That re-export is
 * deliberate and belongs to the entry point: the two implementation headers bring in the
 * stream objects alone.
 *
 * @note **This header brings in the stream objects, not the ability to write a number or a
 *       string to one.** Like `IOv2/io/ostream.h`, it does not include
 *       `IOv2/io/traits/arithmetic.h` or `IOv2/io/traits/char_and_str.h`, so with
 *       `#include <IOv2/io/objects/objects.h>` alone `cout << 42` and `cout << "abc"` do not
 *       compile while `cout << endl` does. That is the library's usual
 *       opt-in-per-value-type arrangement (see `IOv2/io/traits/traits_base.h`): include the
 *       traits header for the kind of value you mean to read or write; this file does not
 *       choose for you.
 * @note The eight stream objects are process-wide singletons, but what "one" spans depends on
 *       the build mode: under `IOV2_SHARED` they are the single copy inside `libiov2.so`,
 *       and in header-only mode they are `inline` variables of the program itself.
 *       `IOV2_SHARED` must be defined consistently across every translation unit of a single
 *       link; see the `@warning` in `IOv2/common/iov2_export.h`.
 * @note **Which static objects may use the eight streams from their constructors and
 *       destructors**: the same scope `std::cout` gives ([iostream.objects.overview]/3) --
 *       a static object **defined after this header is included, in the same translation
 *       unit**. Its destructor is as safe, since the eight are not destroyed at exit (see
 *       the `@note` in `out_impl.h`). The other two shapes depend on the implementation's
 *       initialization order, the standard guaranteeing only [basic.start.dynamic]/3.3,
 *       indeterminately sequenced: a static object in a translation unit that does not
 *       include this header (measured to depend on the link order of the `.o` files), and
 *       one **defined before this header is included** (a null reference on gcc 15, fine on
 *       clang 21 -- both conforming). Use a function-local static, constructed on first use,
 *       for those two.
 * @endif
 */
#pragma once

#include <IOv2/common/defs.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>

#include <exception>
#include <string>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief `sync_with_stdio()` 的失败报告：哪些标准流没能切换，以及各自的原因。
 *
 * 自由函数 `sync_with_stdio()` 对八个流逐个操作，它们彼此独立、没有先后关系，因此任何
 * 「只保留第一个异常」的做法都是在做任意选择。本类型把八个结果一并带出来：每个流一个
 * 具名成员，非空即该流失败、其中是它抛出的那个异常，空即该流已按要求切换。
 * `what()` 把失败的流名与各自的原因依次拼出，只 `log(e.what())` 的调用方也能看到全貌。
 *
 * 哪个流失败也记在**该流自己的状态位**上，那是权威来源；本异常只是当调用方武装了某个流的
 * `exceptions()` 掩码时，用来大声通知的通道。
 * @endif
 *
 * @lang{EN}
 * @brief How `sync_with_stdio()` reports failure: which standard streams did not switch, and why.
 *
 * The free `sync_with_stdio()` operates on the eight streams one by one. They are independent
 * of each other and have no order among them, so keeping "the first" exception would be an
 * arbitrary choice. This type carries all eight results instead: one named member per stream,
 * non-null meaning that stream failed and holding the exception it threw, null meaning it
 * switched as asked. `what()` spells out the failing streams and their reasons in turn, so a
 * caller that only does `log(e.what())` still sees the whole picture.
 *
 * Which stream failed is also recorded in **that stream's own state bits**, which are the
 * authoritative source; this exception is only the channel that speaks up when the caller has
 * armed a stream's `exceptions()` mask.
 * @endif
 */
class sync_error : public io_error
{
public:
    /**
     * @lang{ZH} 每个标准流一个成员：非空为该流失败时抛出的异常，空为该流切换成功。 @endif
     * @lang{EN} One member per standard stream: non-null is the exception that stream threw,
     * null means it switched successfully. @endif
     */
    struct failures
    {
        std::exception_ptr cout_err;
        std::exception_ptr cerr_err;
        std::exception_ptr clog_err;
        std::exception_ptr wcout_err;
        std::exception_ptr wcerr_err;
        std::exception_ptr wclog_err;
        std::exception_ptr cin_err;
        std::exception_ptr wcin_err;

        /// @lang{ZH} 是否有任何一个流失败。 @endif @lang{EN} Whether any stream failed. @endif
        [[nodiscard]] bool any() const noexcept
        {
            return cout_err || cerr_err || clog_err || wcout_err
                || wcerr_err || wclog_err || cin_err || wcin_err;
        }
    };

    explicit sync_error(failures failed)
        : io_error(describe(failed))
        , m_failed(std::move(failed))
    {}

    /**
     * @lang{ZH} @brief 八个流各自的结果。 @return 对 `failures` 的常引用。 @endif
     * @lang{EN} @brief The outcome for each of the eight streams. @return A const reference
     * to the `failures`. @endif
     */
    [[nodiscard]] const failures& failed() const noexcept { return m_failed; }

private:
    // Each reason is recovered by rethrowing the pointer and catching it -- which leaves the
    // pointer intact, so `failed()` still hands out every exception afterwards.
    static std::string describe(const failures& f)
    {
        std::string msg = "IOv2::sync_with_stdio failed for";
        bool first = true;

        const auto add = [&](const char* name, const std::exception_ptr& err)
        {
            if (!err) return;
            msg += first ? " " : "; ";
            first = false;
            msg += name;
            msg += ": ";
            try { std::rethrow_exception(err); }
            catch (const std::exception& e) { msg += e.what(); }
            catch (...) { msg += "unknown exception"; }
        };

        add("cout", f.cout_err);   add("cerr", f.cerr_err);   add("clog", f.clog_err);
        add("wcout", f.wcout_err); add("wcerr", f.wcerr_err); add("wclog", f.wclog_err);
        add("cin", f.cin_err);     add("wcin", f.wcin_err);
        return msg;
    }

    failures m_failed;
};

/**
 * @lang{ZH}
 * @brief 对全部八个标准流对象调用 `sync_with_stdio(sync)`。
 *
 * **八个流一律都会被尝试**：某个流失败不会妨碍其余的切换。每个流的失败先记在它自己的状态位上
 * （输出流是切回同步时那次冲刷失败，输入流是重建 streambuf 失败），若该流的 `exceptions()`
 * 掩码含该位，它会抛出——本函数把这些异常收齐，最后抛一个 `sync_error`。默认掩码
 * （`goodbit`）下本函数不抛任何异常，八个流全部切换完毕，失败只体现在状态位上。
 *
 * 应在任何 `stdin` 读取之前调用：输入流会换掉整个 streambuf，已缓冲但未消费的输入随之
 * 丢弃（见 `stdin_api::sync_with_stdio`）。输出侧没有这个限制，随时可切，与并发的插入操作
 * 安全竞争；切回同步时它会取该流的锁冲刷一次，因此可能等待正在进行的插入结束
 * （见 `stdout_api::sync_with_stdio`）。
 *
 * @param sync `true` 为同步（默认），`false` 为各流自行缓冲。
 * @throws sync_error 至少一个流失败，且该流的 `exceptions()` 掩码含相应的位。异常里带着八个流
 *         各自的结果（见 `sync_error::failed()`），`what()` 列出失败的流名与原因。
 * @endif
 *
 * @lang{EN}
 * @brief Calls `sync_with_stdio(sync)` on all eight standard stream objects.
 *
 * **All eight streams are attempted**: one that fails cannot keep the others from switching.
 * Each failure is first recorded in that stream's own state bits (on an output stream, the
 * flush performed when switching back to synchronized; on an input stream, rebuilding the
 * streambuf), and if that stream's `exceptions()` mask includes the bit it throws -- this
 * function collects those exceptions and finally throws one `sync_error`. Under the default
 * mask (`goodbit`) it throws nothing: all eight switch and the failures show up only as
 * state bits.
 *
 * Call it before any `stdin` read: the input streams replace their whole streambuf, which
 * discards input that was buffered but not yet consumed (see `stdin_api::sync_with_stdio`).
 * The output side carries no such restriction and is switchable at any time, safe against
 * concurrent insertions; switching back to synchronized takes that stream's lock to flush
 * once, so it may wait for an insertion already under way (see
 * `stdout_api::sync_with_stdio`).
 *
 * @param sync `true` for synchronized (the default), `false` for per-stream buffering.
 * @throws sync_error At least one stream failed and its `exceptions()` mask included the
 *         matching bit. The exception carries the outcome of all eight streams (see
 *         `sync_error::failed()`), and `what()` lists the failing streams and their reasons.
 * @endif
 */
inline void sync_with_stdio(bool sync = true)
{
    sync_error::failures failed;

    // Every stream is attempted, so one that throws cannot keep the others from
    // switching. Collecting costs nothing that can fail: assigning an exception_ptr
    // is noexcept, and the table is a local.
    const auto one = [sync](auto& stream, std::exception_ptr& slot) noexcept
    {
        try { stream.sync_with_stdio(sync); }
        catch (...) { slot = std::current_exception(); }
    };

    one(cout, failed.cout_err);
    one(cerr, failed.cerr_err);
    one(clog, failed.clog_err);

    one(wcout, failed.wcout_err);
    one(wcerr, failed.wcerr_err);
    one(wclog, failed.wclog_err);

    one(cin, failed.cin_err);
    one(wcin, failed.wcin_err);

    if (failed.any()) throw sync_error(std::move(failed));
}
}

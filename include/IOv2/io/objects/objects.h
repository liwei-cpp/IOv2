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

#include <IOv2/io/istream.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 对全部八个标准流对象调用 `sync_with_stdio(sync)`。
 *
 * 各流的 `sync_with_stdio` 都不会失败（输出流翻标志，切回同步时的那次冲刷失败只记状态位；
 * 输入流重建 streambuf，失败即 `std::abort()`），因此本函数要么把八个流全部切换，要么进程
 * 终止，不存在部分完成的状态。
 *
 * 应在任何 `stdin` 读取之前调用：输入流会换掉整个 streambuf，已缓冲但未消费的输入随之
 * 丢弃（见 `stdin_api::sync_with_stdio`）。输出侧没有这个限制，随时可切，与并发的插入操作
 * 安全竞争；切回同步时它会取该流的锁冲刷一次，因此可能等待正在进行的插入结束
 * （见 `stdout_api::sync_with_stdio`）。
 *
 * @param sync `true` 为同步（默认），`false` 为各流自行缓冲。
 * @endif
 *
 * @lang{EN}
 * @brief Calls `sync_with_stdio(sync)` on all eight standard stream objects.
 *
 * No stream's `sync_with_stdio` can fail (the output streams flip a flag, and a failed
 * flush when switching back to synchronized is only recorded as a state bit; the input
 * streams rebuild their streambuf and `std::abort()` on failure), so this function either
 * switches all eight or the process ends -- there is no partially completed state.
 *
 * Call it before any `stdin` read: the input streams replace their whole streambuf, which
 * discards input that was buffered but not yet consumed (see `stdin_api::sync_with_stdio`).
 * The output side carries no such restriction and is switchable at any time, safe against
 * concurrent insertions; switching back to synchronized takes that stream's lock to flush
 * once, so it may wait for an insertion already under way (see
 * `stdout_api::sync_with_stdio`).
 *
 * @param sync `true` for synchronized (the default), `false` for per-stream buffering.
 * @endif
 */
inline void sync_with_stdio(bool sync = true) noexcept
{
    cout.sync_with_stdio(sync);
    cerr.sync_with_stdio(sync);
    clog.sync_with_stdio(sync);

    wcout.sync_with_stdio(sync);
    wcerr.sync_with_stdio(sync);
    wclog.sync_with_stdio(sync);

    cin.sync_with_stdio(sync);
    wcin.sync_with_stdio(sync);
}
}

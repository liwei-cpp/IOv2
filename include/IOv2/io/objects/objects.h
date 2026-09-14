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
 * 各流的 `sync_with_stdio` 都不会失败（输出流是原子交换；输入流重建 streambuf，失败即
 * `std::abort()`），因此本函数要么把八个流全部切换，要么进程终止，不存在部分完成的状态。
 *
 * 应在任何 `stdin` 读取之前调用：输入流会换掉整个 streambuf，已缓冲但未消费的输入随之
 * 丢弃（见 `stdin_api::sync_with_stdio`）。输出侧没有这个限制——那只是一次原子交换，随时
 * 可切，与并发的插入操作安全竞争。
 *
 * @param sync `true` 为同步（默认），`false` 为各流自行缓冲。
 * @endif
 *
 * @lang{EN}
 * @brief Calls `sync_with_stdio(sync)` on all eight standard stream objects.
 *
 * No stream's `sync_with_stdio` can fail (the output streams do an atomic exchange;
 * the input streams rebuild their streambuf and `std::abort()` on failure), so this
 * function either switches all eight or the process ends -- there is no partially
 * completed state.
 *
 * Call it before any `stdin` read: the input streams replace their whole streambuf, which
 * discards input that was buffered but not yet consumed (see `stdin_api::sync_with_stdio`).
 * The output side carries no such restriction -- there it is a single atomic exchange,
 * switchable at any time and safe against concurrent insertions.
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

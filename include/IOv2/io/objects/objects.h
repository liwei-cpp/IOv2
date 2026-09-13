// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <IOv2/device/std_device.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>

#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/out_impl.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 对全部八个标准流对象调用 `sync_with_stdio(sync)`。
 *
 * 各流的 `sync_with_stdio` 都不会失败（输出流是原子交换；输入流重建 streambuf，失败即
 * `std::abort()`），因此本函数要么把八个流全部切换，要么进程终止，不存在部分完成的状态。
 * 应在任何标准流 I/O 之前调用，理由见 `stdin_api::sync_with_stdio`。
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
 * completed state. Call it before any standard stream I/O; see
 * `stdin_api::sync_with_stdio` for why.
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

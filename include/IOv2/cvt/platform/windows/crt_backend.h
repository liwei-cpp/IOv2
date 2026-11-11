// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file crt_backend.h
 * @lang{ZH}
 * 转换器所用 C 运行库函数的 Windows（UCRT）后端。
 *
 * UCRT 把 `wcrtomb` 标为不安全（C4996），在 `/WX` 下无法编译，故改用 `wcrtomb_s`。
 * @endif
 * @lang{EN}
 * Windows (UCRT) backend of the C runtime functions the converters use.
 *
 * The UCRT marks `wcrtomb` unsafe (C4996), which `/WX` turns into an error, so
 * `wcrtomb_s` is used instead.
 * @endif
 */

#pragma once

#include <cstddef>
#include <cwchar>
#include <span>

namespace IOv2::detail
{
/**
 * @lang{ZH}
 * 与 `std::wcrtomb(to.data(), ch, &state)` 同义，经 `wcrtomb_s` 实现。`to.size()` 须不小于
 * 当前 locale 的 `MB_CUR_MAX`。
 *
 * @return 写入的字节数；`ch` 在当前 locale 下无法编码时为 `static_cast<std::size_t>(-1)`。
 *
 * @note `wcrtomb_s` 拒绝一个字符时，有时返回非零错误码，有时返回 0 却把字节数置为 -1
 *       （UTF-8 locale 下的代理即如此，见 E0 探针），两种情况都报为 -1。
 * @endif
 * @lang{EN}
 * The same as `std::wcrtomb(to.data(), ch, &state)`, through `wcrtomb_s`. `to.size()` must
 * be at least the current locale's `MB_CUR_MAX`.
 *
 * @return The number of bytes written, or `static_cast<std::size_t>(-1)` if `ch` cannot be
 *         encoded in the current locale.
 *
 * @note When `wcrtomb_s` rejects a character it sometimes returns a nonzero error code and
 *       sometimes returns 0 with the byte count set to -1 (a surrogate under a UTF-8 locale
 *       does the latter); both are reported as -1.
 * @endif
 */
inline std::size_t crt_wcrtomb(std::span<char> to, wchar_t ch, std::mbstate_t& state)
{
    std::size_t count = 0;
    if (wcrtomb_s(&count, to.data(), to.size(), ch, &state) != 0)
        return static_cast<std::size_t>(-1);
    return count;
}
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file crt_backend.h
 * @lang{ZH}
 * 转换器所用 C 运行库函数的 POSIX 后端：直接调用标准函数。
 * @endif
 * @lang{EN}
 * POSIX backend of the C runtime functions the converters use: the standard functions,
 * called directly.
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
 * 即 `std::wcrtomb(to.data(), ch, &state)`。`to.size()` 供 Windows 后端用；调用方须保证它
 * 不小于当前 locale 的 `MB_CUR_MAX`。
 *
 * @return 写入的字节数；`ch` 在当前 locale 下无法编码时为 `static_cast<std::size_t>(-1)`。
 * @endif
 * @lang{EN}
 * `std::wcrtomb(to.data(), ch, &state)`. `to.size()` is for the Windows backend; the caller
 * guarantees it is at least the current locale's `MB_CUR_MAX`.
 *
 * @return The number of bytes written, or `static_cast<std::size_t>(-1)` if `ch` cannot be
 *         encoded in the current locale.
 * @endif
 */
inline std::size_t crt_wcrtomb(std::span<char> to, wchar_t ch, std::mbstate_t& state)
{
    return std::wcrtomb(to.data(), ch, &state);
}
}

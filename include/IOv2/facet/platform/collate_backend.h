// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file collate_backend.h
 * @lang{ZH}
 * `collate` facet 所用字符串比较与排序键函数的平台后端选择入口。各后端在 `IOv2::detail`
 * 中提供同名的 `collate_strcoll`、`collate_wcscoll`、`collate_strxfrm`、`collate_wcsxfrm` 与
 * `collate_is_utf8`。比较函数返回 -1、0 或 1，变换函数的语义同 `strxfrm`（返回值不含结尾
 * 空字符，不小于 `n` 时 `dest` 内容不确定）；二者失败时都返回 `std::nullopt`。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the string-comparison and collation-key functions the
 * `collate` facet uses. Each backend provides, in `IOv2::detail`, the same names:
 * `collate_strcoll`, `collate_wcscoll`, `collate_strxfrm`, `collate_wcsxfrm` and
 * `collate_is_utf8`. The comparisons return -1, 0 or 1; the transforms behave as `strxfrm`
 * (the result excludes the terminating null, and `dest` is indeterminate when it is not less
 * than `n`). Both return `std::nullopt` on failure.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/facet/platform/windows/collate_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/facet/platform/posix/collate_backend.h>
#else
#error "IOv2 facet: unsupported platform"
#endif

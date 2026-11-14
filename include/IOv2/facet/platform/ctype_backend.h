// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file ctype_backend.h
 * @lang{ZH}
 * `ctype` facet 所用 C 库字符分类与大小写函数的平台后端选择入口。各后端在 `IOv2::detail`
 * 中提供同名的：字节版 `ctype_isupper` … `ctype_ispunct`、`ctype_toupper`、`ctype_tolower`；
 * 宽字符版 `ctype_wclass`、`ctype_wctype`、`ctype_iswctype`、`ctype_towupper`、`ctype_towlower`。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the C library character-classification and case
 * functions the `ctype` facet uses. Each backend provides, in `IOv2::detail`, the same names:
 * for bytes `ctype_isupper` ... `ctype_ispunct`, `ctype_toupper` and `ctype_tolower`; for wide
 * characters `ctype_wclass`, `ctype_wctype`, `ctype_iswctype`, `ctype_towupper` and
 * `ctype_towlower`.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/facet/platform/windows/ctype_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/facet/platform/posix/ctype_backend.h>
#else
#error "IOv2 facet: unsupported platform"
#endif

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file crt_backend.h
 * @lang{ZH}
 * 转换器所用 C 运行库函数的平台后端选择入口。各后端在 `IOv2::detail` 中提供同名的
 * `crt_wcrtomb`。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the C runtime functions the converters use. Each
 * backend provides, in `IOv2::detail`, the same names: `crt_wcrtomb`.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/cvt/platform/windows/crt_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/cvt/platform/posix/crt_backend.h>
#else
#error "IOv2 cvt: unsupported platform"
#endif

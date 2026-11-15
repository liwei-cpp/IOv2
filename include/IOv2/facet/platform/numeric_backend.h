// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file numeric_backend.h
 * @lang{ZH}
 * `numeric` facet 读取 locale 数值标点与布尔名称的平台后端选择入口。各后端在 `IOv2::detail`
 * 中提供同名的 `numeric_raw_punct` 与 `numeric_read_punct`。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the backends the `numeric` facet reads a locale's numeric
 * punctuation and boolean names through. Each backend provides, in `IOv2::detail`, the same
 * names: `numeric_raw_punct` and `numeric_read_punct`.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/facet/platform/windows/numeric_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/facet/platform/posix/numeric_backend.h>
#else
#error "IOv2 facet: unsupported platform"
#endif

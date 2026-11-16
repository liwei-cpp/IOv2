// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file monetary_backend.h
 * @lang{ZH}
 * `monetary` facet 读取 locale 货币格式数据的平台后端选择入口。各后端在 `IOv2::detail`
 * 中提供同名的 `monetary_raw_flags`、`monetary_raw_punct` 与 `monetary_read_punct`。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the backends the `monetary` facet reads a locale's
 * monetary formatting data through. Each backend provides, in `IOv2::detail`, the same
 * names: `monetary_raw_flags`, `monetary_raw_punct` and `monetary_read_punct`.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/facet/platform/windows/monetary_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/facet/platform/posix/monetary_backend.h>
#else
#error "IOv2 facet: unsupported platform"
#endif

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file timeio_backend.h
 * @lang{ZH}
 * `timeio` facet 读取 locale 日期时间数据的平台后端选择入口。各后端在 `IOv2::detail` 中提供
 * 同名的 `timeio_raw_era`、`timeio_raw_data`、`timeio_read_eras`、`timeio_read_alt_digits` 与
 * `timeio_read_data`。纪元与替代数字是每个平台都要回答的问题，平台没有这项数据时返回空。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the backends the `timeio` facet reads a locale's date and
 * time data through. Each backend provides, in `IOv2::detail`, the same names:
 * `timeio_raw_era`, `timeio_raw_data`, `timeio_read_eras`, `timeio_read_alt_digits` and
 * `timeio_read_data`. Eras and alternative digits are a question every platform answers, with
 * an empty result where it has no such data.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#error "IOv2 facet: timeio is not ported to Windows yet"
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/facet/platform/posix/timeio_backend.h>
#else
#error "IOv2 facet: unsupported platform"
#endif

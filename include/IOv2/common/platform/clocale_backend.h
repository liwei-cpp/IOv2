// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file clocale_backend.h
 * @lang{ZH}
 * `clocale_wrapper` 平台后端的选择入口。各后端在 `IOv2::detail` 中提供同名的
 * `clocale_handle`、`clocale_create`、`clocale_duplicate`、`clocale_release`、
 * `clocale_ctype_name`，以及切换线程 locale 用的 `clocale_saved`、`clocale_use`、`clocale_restore`。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the `clocale_wrapper` backends. Each backend provides,
 * in `IOv2::detail`, the same names: `clocale_handle`, `clocale_create`, `clocale_duplicate`,
 * `clocale_release`, `clocale_ctype_name`, and for switching the thread's locale
 * `clocale_saved`, `clocale_use` and `clocale_restore`.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/common/platform/windows/clocale_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/common/platform/posix/clocale_backend.h>
#else
#error "IOv2 common: unsupported platform"
#endif

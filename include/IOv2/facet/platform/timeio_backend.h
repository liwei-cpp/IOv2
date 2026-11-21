// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file timeio_backend.h
 * @lang{ZH}
 * `timeio` facet 读取 locale 日期时间数据的平台后端选择入口。各后端在 `IOv2::detail` 中提供
 * 同名的 `timeio_raw_era`、`timeio_raw_wide`、`timeio_raw_data`、`timeio_read_eras`、
 * `timeio_read_alt_digits` 与 `timeio_read_data`。纪元与替代数字是每个平台都要回答的问题，平台
 * 没有这项数据时返回空；宽字符形式（`timeio_raw_data::wide`）只有原生数据是宽字符串的平台才给。
 * `timeio_read_data(loc)` 在线程已经以 `clocale_user` 切换到 `loc` 时调用：POSIX 经线程 locale 读
 * `nl_langinfo`，Windows 用 `loc` 里 `LC_TIME` 的 BCP-47 名字与代码页读 NLS。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the backends the `timeio` facet reads a locale's date and
 * time data through. Each backend provides, in `IOv2::detail`, the same names:
 * `timeio_raw_era`, `timeio_raw_wide`, `timeio_raw_data`, `timeio_read_eras`,
 * `timeio_read_alt_digits` and `timeio_read_data`. Eras and alternative digits are a question
 * every platform answers, with an empty result where it has no such data; the wide form
 * (`timeio_raw_data::wide`) comes only from a platform whose own data is wide.
 * `timeio_read_data(loc)` is called with the thread switched to `loc` by a `clocale_user`:
 * POSIX reads `nl_langinfo` through the thread's locale, Windows reads NLS by the BCP-47 name
 * and the code page of `loc`'s `LC_TIME`.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/facet/platform/windows/timeio_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/facet/platform/posix/timeio_backend.h>
#else
#error "IOv2 facet: unsupported platform"
#endif

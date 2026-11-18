// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file messages_backend.h
 * @lang{ZH}
 * `messages` facet 平台后端的选择入口。各后端在 `IOv2::detail` 中提供同名的
 * `messages_default_dirname` 与 `messages_system_languages`。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the `messages` facet's backends. Each backend provides,
 * in `IOv2::detail`, the same names: `messages_default_dirname` and
 * `messages_system_languages`.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/facet/platform/windows/messages_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/facet/platform/posix/messages_backend.h>
#else
#error "IOv2 facet: unsupported platform"
#endif

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file std_device_backend.h
 * @lang{ZH}
 * `std_device` 平台后端的选择入口。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for the `std_device` backends.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/device/platform/windows/std_device_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/device/platform/posix/std_device_backend.h>
#else
#error "IOv2 device: unsupported platform"
#endif

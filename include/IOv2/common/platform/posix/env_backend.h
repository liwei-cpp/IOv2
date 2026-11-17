// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file env_backend.h
 * @lang{ZH}
 * 读环境变量的 POSIX 后端：`std::getenv`。
 * @endif
 * @lang{EN}
 * POSIX backend for reading environment variables: `std::getenv`.
 * @endif
 */

#pragma once

#include <cstdlib>
#include <string>

namespace IOv2::detail
{
// The value of the environment variable name, copied out at once; empty if it is unset
// or empty.
inline std::string get_env(const char* name)
{
    const char* p = std::getenv(name);
    return p ? std::string(p) : std::string();
}
}

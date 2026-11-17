// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file env_backend.h
 * @lang{ZH}
 * 读环境变量的 Windows（UCRT）后端：`getenv_s`。UCRT 把 `getenv` 标为不安全（C4996），在
 * `/WX` 下无法编译。`getenv_s` 先问所需长度、再取值，不像 `_dupenv_s` 那样要调用方 `free`。
 * @endif
 * @lang{EN}
 * Windows (UCRT) backend for reading environment variables: `getenv_s`. The UCRT marks
 * `getenv` unsafe (C4996), which `/WX` turns into an error. `getenv_s` is asked for the
 * size first and the value second, and unlike `_dupenv_s` leaves nothing for the caller to
 * `free`.
 * @endif
 */

#pragma once

#include <cstddef>
#include <cstdlib>
#include <string>

namespace IOv2::detail
{
// The value of the environment variable name, copied out at once; empty if it is unset.
// A value that grows between the two calls no longer fits and reads as unset.
inline std::string get_env(const char* name)
{
    std::size_t len = 0;   // the size getenv_s wants, the terminating null included
    if (getenv_s(&len, nullptr, 0, name) != 0 || len == 0)
        return {};
    std::string res(len, '\0');
    if (getenv_s(&len, res.data(), res.size(), name) != 0 || len == 0)
        return {};
    res.resize(len - 1);
    return res;
}
}

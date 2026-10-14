// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file file_device_backend.h
 * @lang{ZH}
 * `file_device` 的 POSIX 后端：路径字节原样交给 `fopen`。
 * @endif
 * @lang{EN}
 * POSIX backend of `file_device`: the path's bytes go to `fopen` unchanged.
 * @endif
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace IOv2::detail
{
// Opens `p` with an fopen-style `mode`; nullptr with errno set on failure.
inline FILE* file_open(const std::filesystem::path& p, const char* mode)
{
    return std::fopen(p.c_str(), mode); // NOLINT(cppcoreguidelines-owning-memory)
}

inline int file_seek(FILE* stream, std::int64_t offset, int origin)
{
    return fseeko(stream, offset, origin);
}

inline std::int64_t file_tell(FILE* stream)
{
    return ftello(stream);
}

// The path's own bytes: no conversion, so nothing to fail.
inline std::string path_for_message(const std::filesystem::path& p)
{
    return p.native();
}
}

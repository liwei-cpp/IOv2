// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>

// Throws rather than asserting, so that a caller decides how to report it. This
// header used to say "deliberately free of support/verify.h", which pulled in
// objects.h and made every including TU emit code for its eight inline stream
// singletons; that header is gone now, but the reason to stay dependency-free
// here has not changed.
inline std::string exe_path()
{
    char dest[PATH_MAX];
    memset(dest, 0, sizeof(dest)); // readlink does not null terminate!
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1)
        throw std::runtime_error("exe_path: readlink(/proc/self/exe) failed");
    return dest;
}

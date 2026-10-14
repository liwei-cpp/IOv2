// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <support/platform.h>

#include <filesystem>

// Throws rather than asserting, so that a caller decides how to report it. This
// header used to say "deliberately free of support/verify.h", which pulled in
// objects.h and made every including TU emit code for its eight inline stream
// singletons; that header is gone now, but the reason to stay dependency-free
// here has not changed.
inline std::filesystem::path exe_path()
{
    return iov2_test::sys::exe_path();
}

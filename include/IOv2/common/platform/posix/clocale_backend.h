// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file clocale_backend.h
 * @lang{ZH}
 * `clocale_wrapper` 的 POSIX 后端：`newlocale`/`duplocale`/`freelocale`/`uselocale`。
 * 取 `LC_CTYPE` 名字没有 POSIX 接口，glibc 上用扩展 `nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), …)`，
 * macOS 上用 `querylocale(LC_CTYPE_MASK, …)`（`<xlocale.h>`），其他 libc 不受支持。
 * @endif
 * @lang{EN}
 * POSIX backend of `clocale_wrapper`: `newlocale`/`duplocale`/`freelocale`/`uselocale`.
 * POSIX has no call that names a locale's `LC_CTYPE`, so the name comes from the glibc
 * extension `nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), …)` on glibc and from
 * `querylocale(LC_CTYPE_MASK, …)` (`<xlocale.h>`) on macOS; other C libraries are not
 * supported.
 * @endif
 */

#pragma once

// glibc defines __GLIBC__ in <features.h>, which <clocale> includes.
#include <clocale>

#if defined(__GLIBC__)
#include <langinfo.h>
#elif defined(__APPLE__)
#include <xlocale.h>
#else
#error "IOv2 common: clocale_wrapper supports glibc and macOS only among POSIX C libraries"
#endif

namespace IOv2::detail
{
using clocale_handle = locale_t;

// A locale with all categories from name; null if the C library rejects it.
inline clocale_handle clocale_create(const char* name) noexcept
{
    return newlocale(LC_ALL_MASK, name, nullptr);
}

// An independent copy of loc; null on failure.
inline clocale_handle clocale_duplicate(clocale_handle loc) noexcept
{
    return duplocale(loc);
}

inline void clocale_release(clocale_handle loc) noexcept
{
    freelocale(loc);
}

// The name of loc's LC_CTYPE category. glibc reports "POSIX" as "C"; macOS keeps it.
inline const char* clocale_ctype_name(clocale_handle loc) noexcept
{
#if defined(__GLIBC__)
    return nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), loc);
#else
    return querylocale(LC_CTYPE_MASK, loc);
#endif
}

// What clocale_use hands back for clocale_restore: the thread's previous locale.
using clocale_saved = locale_t;

// Makes loc the calling thread's locale.
inline clocale_saved clocale_use(clocale_handle loc) noexcept
{
    return uselocale(loc);
}

// Puts back the thread locale clocale_use replaced.
inline void clocale_restore(clocale_saved old) noexcept
{
    uselocale(old);
}
}

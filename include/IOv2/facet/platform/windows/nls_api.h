// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file nls_api.h
 * @lang{ZH}
 * facet 的 Windows 后端所用的 NLS 函数与常量，按 `<windows.h>` 的写法声明但不包含它（理由见
 * `device/platform/windows/win32_api.h`）。
 * @endif
 * @lang{EN}
 * The NLS functions and constants the Windows facet backends use, declared as `<windows.h>`
 * does but without including it (the reasons are in `device/platform/windows/win32_api.h`).
 * @endif
 */

#pragma once

// The version-info parameter of LCMapStringEx and CompareStringEx needs the struct's tag at
// global scope so these declarations and <windows.h>'s name the same type.
struct _nlsversioninfo;   // NOLINT(bugprone-reserved-identifier): <winnls.h>'s own tag

namespace IOv2::detail
{
#if defined(_WIN64)
using win_lparam = __int64;   // LPARAM
#else
using win_lparam = long;
#endif

extern "C"
{
__declspec(dllimport) int __stdcall LCMapStringEx(const wchar_t* lpLocaleName, unsigned long dwMapFlags,
                                                  const wchar_t* lpSrcStr, int cchSrc,
                                                  wchar_t* lpDestStr, int cchDest,
                                                  ::_nlsversioninfo* lpVersionInformation,
                                                  void* lpReserved, win_lparam sortHandle);
__declspec(dllimport) int __stdcall CompareStringEx(const wchar_t* lpLocaleName, unsigned long dwCmpFlags,
                                                    const wchar_t* lpString1, int cchCount1,
                                                    const wchar_t* lpString2, int cchCount2,
                                                    ::_nlsversioninfo* lpVersionInformation,
                                                    void* lpReserved, win_lparam lParam);
__declspec(dllimport) int __stdcall GetStringTypeW(unsigned long dwInfoType, const wchar_t* lpSrcStr,
                                                   int cchSrc, unsigned short* lpCharType);
__declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned int CodePage, unsigned long dwFlags,
                                                        const char* lpMultiByteStr, int cbMultiByte,
                                                        wchar_t* lpWideCharStr, int cchWideChar);
}

inline constexpr unsigned long win_ct_ctype1 = 0x0001;                    // CT_CTYPE1
inline constexpr unsigned long win_lcmap_lowercase = 0x00000100;          // LCMAP_LOWERCASE
inline constexpr unsigned long win_lcmap_uppercase = 0x00000200;          // LCMAP_UPPERCASE
inline constexpr unsigned long win_lcmap_sortkey = 0x00000400;            // LCMAP_SORTKEY
inline constexpr unsigned long win_lcmap_linguistic_casing = 0x01000000;  // LCMAP_LINGUISTIC_CASING
inline constexpr unsigned long win_mb_err_invalid_chars = 0x00000008;     // MB_ERR_INVALID_CHARS
inline constexpr int win_cstr_less_than = 1;                              // CSTR_LESS_THAN
inline constexpr int win_cstr_equal = 2;                                  // CSTR_EQUAL
}

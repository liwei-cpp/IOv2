// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file win32_api.h
 * @lang{ZH}
 * Windows 后端用到的少数几个 Win32 函数的声明，代替 `<windows.h>`。
 *
 * `<windows.h>` 有 include guard：我们以什么宏（`WIN32_LEAN_AND_MEAN`、`NOMINMAX`）引入它，
 * 用户此后的每一次 `#include <windows.h>` 就都是什么样子。这里的签名与 `<windows.h>` 逐一
 * 相同，两者可以共存。
 * @endif
 * @lang{EN}
 * Declarations of the few Win32 functions the Windows backends use, in place of `<windows.h>`.
 *
 * `<windows.h>` has an include guard, so whatever macros we included it under
 * (`WIN32_LEAN_AND_MEAN`, `NOMINMAX`) would bind every later include in the user's code.
 * The signatures here match `<windows.h>` exactly, so the two coexist.
 * @endif
 */

#pragma once

namespace IOv2::detail
{
extern "C"
{
__declspec(dllimport) unsigned long __stdcall GetFileType(void* hFile);
__declspec(dllimport) int __stdcall PeekNamedPipe(void* hNamedPipe, void* lpBuffer,
                                                  unsigned long nBufferSize,
                                                  unsigned long* lpBytesRead,
                                                  unsigned long* lpTotalBytesAvail,
                                                  unsigned long* lpBytesLeftThisMessage);
__declspec(dllimport) unsigned long __stdcall GetLastError();
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags,
                                                        const wchar_t* lpWideCharStr, int cchWideChar,
                                                        char* lpMultiByteStr, int cbMultiByte,
                                                        const char* lpDefaultChar,
                                                        int* lpUsedDefaultChar);
}

inline constexpr unsigned long win_file_type_disk = 0x0001;    // FILE_TYPE_DISK
inline constexpr unsigned long win_file_type_pipe = 0x0003;    // FILE_TYPE_PIPE
inline constexpr unsigned long win_error_broken_pipe = 109;    // ERROR_BROKEN_PIPE
inline constexpr unsigned int win_cp_acp = 0;                  // CP_ACP
}

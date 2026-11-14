// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file ctype_backend.h
 * @lang{ZH}
 * `ctype` facet 所用字符分类函数的 POSIX 后端：POSIX.1-2008 的 `wctype_l`/`iswctype_l`。
 * macOS 上它们声明在 `<xlocale/_wctype.h>` 中，而 `<xlocale.h>` 只在 `<wctype.h>` 先于它
 * 被包含时才引入该头文件，故此处显式包含。
 * @endif
 * @lang{EN}
 * POSIX backend of the character-classification functions the `ctype` facet uses: the
 * POSIX.1-2008 `wctype_l`/`iswctype_l`. On macOS they are declared in `<xlocale/_wctype.h>`,
 * which `<xlocale.h>` pulls in only when `<wctype.h>` was included before it, so it is
 * included here explicitly.
 * @endif
 */

#pragma once

#include <IOv2/common/platform/clocale_backend.h>

#include <cctype>
#include <cstdlib>
#include <cwctype>

#if defined(__APPLE__)
#include <xlocale.h>
#include <xlocale/_ctype.h>
#include <xlocale/_stdlib.h>
#include <xlocale/_wctype.h>
#endif

namespace IOv2::detail
{
/**
 * @lang{ZH}
 * 字节 `c` 在 `loc` 中是否为一个独立的字符，可交给 `is*_l`/`to*_l`。
 *
 * glibc 上恒为 `true`：多字节 locale 中它自己就不对高字节分类。macOS 则把字节当作同值的
 * Unicode 码位来分类，不看编码：UTF-8 locale 下 0x80–0x9F 成了 `cntrl`、0xE9 成了字母，而
 * 它们在 UTF-8 中只是多字节序列的一部分。故 macOS 上多字节 locale 的高字节一律不属于
 * 任何类别、大小写不变，与 glibc 相同。
 * @endif
 * @lang{EN}
 * Whether byte `c` is a character of its own in `loc`, fit for `is*_l`/`to*_l`.
 *
 * Always `true` on glibc, which does not classify high bytes of a multibyte locale in
 * the first place. macOS classifies a byte as the Unicode code point of the same value,
 * whatever the encoding: under UTF-8 it makes 0x80-0x9F `cntrl` and 0xE9 a letter,
 * although there they are only parts of multibyte sequences. So on macOS a high byte of
 * a multibyte locale belongs to no class and keeps its case, as on glibc.
 * @endif
 */
#if defined(__APPLE__)
inline bool ctype_byte_is_char(int c, clocale_handle loc) noexcept
{
    return c < 0x80 || MB_CUR_MAX_L(loc) == 1;
}
#else
constexpr bool ctype_byte_is_char(int /*c*/, clocale_handle /*loc*/) noexcept
{
    return true;
}
#endif

// Byte classification in loc; c is an unsigned char value.
inline bool ctype_isupper(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && isupper_l(c, loc) != 0; }
inline bool ctype_islower(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && islower_l(c, loc) != 0; }
inline bool ctype_isalpha(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && isalpha_l(c, loc) != 0; }
inline bool ctype_isdigit(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && isdigit_l(c, loc) != 0; }
inline bool ctype_isxdigit(int c, clocale_handle loc) noexcept { return ctype_byte_is_char(c, loc) && isxdigit_l(c, loc) != 0; }
inline bool ctype_isspace(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && isspace_l(c, loc) != 0; }
inline bool ctype_isprint(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && isprint_l(c, loc) != 0; }
inline bool ctype_iscntrl(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && iscntrl_l(c, loc) != 0; }
inline bool ctype_ispunct(int c, clocale_handle loc) noexcept  { return ctype_byte_is_char(c, loc) && ispunct_l(c, loc) != 0; }

// Byte case mapping in loc. macOS maps a byte as a code point too and can answer with
// one above 0xFF (0xFF -> U+0178), which is no byte; c is then kept.
inline int ctype_toupper(int c, clocale_handle loc) noexcept
{
    if (!ctype_byte_is_char(c, loc)) return c;
    const int res = toupper_l(c, loc);
    return (res >= 0 && res <= 0xFF) ? res : c;
}

inline int ctype_tolower(int c, clocale_handle loc) noexcept
{
    if (!ctype_byte_is_char(c, loc)) return c;
    const int res = tolower_l(c, loc);
    return (res >= 0 && res <= 0xFF) ? res : c;
}

// What ctype_wctype returns: a wctype_t, valid only together with the locale it came from.
using ctype_wclass = wctype_t;

// The class named name ("upper", "alpha", ...) in loc; 0 if loc has no such class.
inline ctype_wclass ctype_wctype(const char* name, clocale_handle loc) noexcept
{
    return wctype_l(name, loc);
}

// Whether c belongs to cls, which ctype_wctype obtained from loc.
inline bool ctype_iswctype(wint_t c, ctype_wclass cls, clocale_handle loc) noexcept
{
    return iswctype_l(c, cls, loc) != 0;
}

// Wide case mapping in loc.
inline wint_t ctype_towupper(wint_t c, clocale_handle loc) noexcept { return towupper_l(c, loc); }
inline wint_t ctype_towlower(wint_t c, clocale_handle loc) noexcept { return towlower_l(c, loc); }
}

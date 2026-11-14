// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file ctype_backend.h
 * @lang{ZH}
 * `ctype` facet 所用字符分类函数的 Windows（UCRT）后端。
 *
 * Windows 上宽字符分类与 locale 无关，故本后端不使用 locale 参数：
 * - UCRT 没有 `wctype_l`；`wctype` 是一张固定的名字 → 位掩码表（`_UPPER`、`_ALPHA` 等，
 *   与 Win32 的 `C1_*` 相同）。
 * - UCRT 的 `_iswctype_l` 忽略 locale 参数，直接转发给 `iswctype`。
 * - 根源在 Win32：`GetStringTypeW` 没有 locale 参数，按 Unicode 属性分类。MSVC STL 的
 *   `ctype<wchar_t>`（`_Getwctype` 丢弃 `_Ctypevec`）、libc++ 与 mingw 上的 libstdc++ 都
 *   因此与 locale 无关。
 *
 * 本后端对每个字符都直接问 `GetStringTypeW`，不用 `iswctype`：后者对小于 256 的字符查 UCRT
 * 自带的固定表，该表注释说与 `GetStringTypeW` 一致，实际只有 U+0020 带 `_BLANK`，于是
 * U+00A0（NBSP）既是空白又不可打印，与 1252 下字节 0xA0 的答案、MSVC STL 及 glibc 都不同。
 *
 * 由此，`"C"` locale 下 `L'é'` 在 Windows 上也属于 `alpha`/`lower`，与 glibc 不同。`wchar_t`
 * 是 UTF-16，单独的代理不属于任何类别，BMP 以外的字符无法分类。
 * @endif
 * @lang{EN}
 * Windows (UCRT) backend of the character-classification functions the `ctype` facet uses.
 *
 * Wide-character classification on Windows does not depend on the locale, so this backend
 * leaves the locale argument unused:
 * - The UCRT has no `wctype_l`; `wctype` is a fixed table from names to bit masks (`_UPPER`,
 *   `_ALPHA`, ..., the same bits as Win32's `C1_*`).
 * - The UCRT's `_iswctype_l` ignores its locale argument and forwards to `iswctype`.
 * - The root is Win32: `GetStringTypeW` takes no locale and classifies by Unicode
 *   properties. That is why the MSVC STL's `ctype<wchar_t>` (`_Getwctype` discards its
 *   `_Ctypevec`), libc++ and libstdc++ on mingw are all locale-independent too.
 *
 * This backend asks `GetStringTypeW` about every character rather than going through
 * `iswctype`, which looks a character below 256 up in the UCRT's own fixed table. That
 * table's comment says it agrees with `GetStringTypeW`, yet only U+0020 has `_BLANK` in
 * it, so U+00A0 (NBSP) is a space that does not print, unlike byte 0xA0 under 1252, the
 * MSVC STL and glibc.
 *
 * So under the `"C"` locale `L'é'` is `alpha`/`lower` on Windows, unlike glibc. `wchar_t` is
 * UTF-16: a lone surrogate belongs to no class, and characters outside the BMP cannot be
 * classified.
 * @endif
 */

#pragma once

#include <IOv2/common/platform/clocale_backend.h>
#include <IOv2/facet/platform/windows/nls_api.h>

#include <cctype>
#include <cwctype>

namespace IOv2::detail
{
// Byte classification and case mapping in loc; c is an unsigned char value. Unlike the
// wide functions, these do read the locale: its code page's table. Some are macros in
// the UCRT headers, hence no :: qualification.
inline bool ctype_isupper(int c, clocale_handle loc) noexcept  { return _isupper_l(c, loc->loc) != 0; }
inline bool ctype_islower(int c, clocale_handle loc) noexcept  { return _islower_l(c, loc->loc) != 0; }
inline bool ctype_isalpha(int c, clocale_handle loc) noexcept  { return _isalpha_l(c, loc->loc) != 0; }
inline bool ctype_isdigit(int c, clocale_handle loc) noexcept  { return _isdigit_l(c, loc->loc) != 0; }
inline bool ctype_isxdigit(int c, clocale_handle loc) noexcept { return _isxdigit_l(c, loc->loc) != 0; }
inline bool ctype_isspace(int c, clocale_handle loc) noexcept  { return _isspace_l(c, loc->loc) != 0; }
// Not _isprint_l: it is any of _BLANK|_PUNCT|_ALPHA|_DIGIT, and a named locale's table,
// built by GetStringTypeA, gives tab C1_BLANK, so tab would print there though not in "C".
// C makes printing and control characters disjoint (C11 7.4/3); a blank prints only if
// it is not a control character.
inline bool ctype_isprint(int c, clocale_handle loc) noexcept
{
    return _isctype_l(c, _PUNCT | _ALPHA | _DIGIT, loc->loc) != 0
        || (_isctype_l(c, _BLANK, loc->loc) != 0 && _iscntrl_l(c, loc->loc) == 0);
}
inline bool ctype_iscntrl(int c, clocale_handle loc) noexcept  { return _iscntrl_l(c, loc->loc) != 0; }
inline bool ctype_ispunct(int c, clocale_handle loc) noexcept  { return _ispunct_l(c, loc->loc) != 0; }
inline int  ctype_toupper(int c, clocale_handle loc) noexcept  { return _toupper_l(c, loc->loc); }
inline int  ctype_tolower(int c, clocale_handle loc) noexcept  { return _tolower_l(c, loc->loc); }

// What ctype_wctype returns: a C1_* bit mask, the same in every locale.
using ctype_wclass = wctype_t;

// The class named name ("upper", "alpha", ...); 0 if there is no such class.
inline ctype_wclass ctype_wctype(const char* name, clocale_handle /*loc*/) noexcept
{
    return std::wctype(name);
}

/**
 * @lang{ZH}
 * `c` 是否属于 `cls`（`ctype_wctype` 给出的掩码，有其中任一位即属于）。`c` 的位取自
 * `GetStringTypeW(CT_CTYPE1, …)`，理由见文件说明。
 *
 * `print` 另行判定：`wctype("print")` 是 `_BLANK|_PUNCT|_ALPHA|_DIGIT`，而 `GetStringTypeW`
 * 给 tab 也加了 `C1_BLANK`。C 规定打印字符与控制字符不相交（C11 7.4/3），故空白只在不是
 * 控制字符时才算可打印，与窄字符的 `ctype_isprint` 相同。
 * @endif
 * @lang{EN}
 * Whether `c` belongs to `cls`, a mask from `ctype_wctype` (`c` belongs if it has any of
 * its bits). The bits of `c` come from `GetStringTypeW(CT_CTYPE1, ...)`; the file comment
 * says why.
 *
 * `print` is decided apart: `wctype("print")` is `_BLANK|_PUNCT|_ALPHA|_DIGIT`, and
 * `GetStringTypeW` gives tab `C1_BLANK` as well. C makes printing and control characters
 * disjoint (C11 7.4/3), so a blank prints only if it is not a control character, as in the
 * byte-wise `ctype_isprint`.
 * @endif
 */
// The signature every backend shares; only here are wint_t and wctype_t both unsigned short.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
inline bool ctype_iswctype(wint_t c, ctype_wclass cls, clocale_handle /*loc*/) noexcept
{
    const auto wc = static_cast<wchar_t>(c);
    unsigned short type = 0;
    if (GetStringTypeW(win_ct_ctype1, &wc, 1, &type) == 0)
        return false;

    constexpr ctype_wclass print_class = _BLANK | _PUNCT | _ALPHA | _DIGIT;
    if (cls == print_class)
        return (type & (_PUNCT | _ALPHA | _DIGIT)) != 0
            || ((type & _BLANK) != 0 && (type & _CONTROL) == 0);
    return (type & cls) != 0;
}

/**
 * @lang{ZH}
 * 按 `loc` 的 `LC_CTYPE` 对单个 UTF-16 代码单元做大小写映射，`flags` 为 `LCMAP_UPPERCASE` 或
 * `LCMAP_LOWERCASE`。
 *
 * 不用 UCRT 的 `_towupper_l`/`_towlower_l`：它们对小于 256 的字符查的是按 locale 代码页字节
 * 建的表，即拿码位当代码页字节用，在代码页 locale 下给出错误结果（探针实测：1252 下
 * U+00FF → U+009F，1251 下 U+00FF → U+00DF，1253 下 U+00DF → U+00BA）。`LCMapStringEx` 按
 * Unicode 映射，在所有 locale 下都正确；加 `LCMAP_LINGUISTIC_CASING` 与 glibc 一致
 * （U+00B5 → U+039C、U+0131 → `I`、U+01C5 → U+01C4，土耳其语 `i` → U+0130）。
 *
 * `"C"` locale 没有 BCP-47 名字，与 UCRT 及 glibc 的 `"C"` 一样只映射 ASCII。映射失败或结果
 * 不是单个代码单元时返回 `c`。
 * @endif
 * @lang{EN}
 * Case-maps one UTF-16 code unit by `loc`'s `LC_CTYPE`; `flags` is `LCMAP_UPPERCASE` or
 * `LCMAP_LOWERCASE`.
 *
 * Not the UCRT's `_towupper_l`/`_towlower_l`: below 256 they look the character up in a
 * table built from the locale's code-page bytes, using the code point as a code-page byte,
 * which is wrong under a code-page locale (measured by the probe: U+00FF -> U+009F under
 * 1252, U+00FF -> U+00DF under 1251, U+00DF -> U+00BA under 1253). `LCMapStringEx` maps by
 * Unicode and is right in every locale; `LCMAP_LINGUISTIC_CASING` makes it agree with glibc
 * (U+00B5 -> U+039C, U+0131 -> `I`, U+01C5 -> U+01C4, Turkish `i` -> U+0130).
 *
 * The `"C"` locale has no BCP-47 name and, like the UCRT's and glibc's `"C"`, maps ASCII
 * only. Returns `c` if the mapping fails or does not give a single code unit.
 * @endif
 */
inline wint_t ctype_map_case(wint_t c, unsigned long flags, clocale_handle loc) noexcept
{
    if (loc->ctype_bcp47.empty())
    {
        if (flags == win_lcmap_uppercase && c >= L'a' && c <= L'z') return static_cast<wint_t>(c - L'a' + L'A');
        if (flags == win_lcmap_lowercase && c >= L'A' && c <= L'Z') return static_cast<wint_t>(c - L'A' + L'a');
        return c;
    }
    const auto src = static_cast<wchar_t>(c);
    wchar_t dst = 0;
    if (LCMapStringEx(loc->ctype_bcp47.c_str(), flags | win_lcmap_linguistic_casing,
                      &src, 1, &dst, 1, nullptr, nullptr, 0) != 1)
        return c;
    return dst;
}

// Wide case mapping in loc. Unlike classification this reads the locale; see ctype_map_case.
inline wint_t ctype_towupper(wint_t c, clocale_handle loc) noexcept
{
    return ctype_map_case(c, win_lcmap_uppercase, loc);
}

inline wint_t ctype_towlower(wint_t c, clocale_handle loc) noexcept
{
    return ctype_map_case(c, win_lcmap_lowercase, loc);
}
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file collate_backend.h
 * @lang{ZH}
 * `collate` facet 的 Windows 后端：直接调用 `CompareStringEx` 与 `LCMapStringEx(LCMAP_SORTKEY)`，
 * 以 locale 的 `LC_COLLATE` BCP-47 名字定排序规则。
 *
 * 不用 UCRT 的 `_strcoll_l`/`_strxfrm_l`：前者给 `CompareStringEx` 加了 `SORT_STRINGSORT`，
 * 后者调 `LCMapStringEx` 时却不加，于是排序键的顺序与比较结果不一致（真机实测 de-DE 下
 * `"co-op"` 比 `"coat"` 小，其排序键却比 `"coat"` 的大）。MSVC STL 的 `collate` 自己调 NLS，
 * 也是同样的不一致。本后端两边都不加标志：标点在第一层被忽略，在上例上与 glibc 一致。
 *
 * 窄字符串先按 `LC_COLLATE` 的代码页用 `MultiByteToWideChar` 转成 UTF-16，带
 * `MB_ERR_INVALID_CHARS`：该代码页下的非法字节序列算失败，与 UCRT 的 `_strcoll_l` 相同。
 * `LCMapStringEx` 给出的排序键是字节串：窄字符的键就是这些字节，宽字符的键每个 `wchar_t`
 * 放一个字节，与 UCRT 的 `wcsxfrm` 相同。
 *
 * `"C"` locale 没有 BCP-47 名字，与 UCRT 一样按代码单元比较，排序键就是输入本身。宽字符
 * 是 UTF-16 代码单元，故 `"C"` 下 BMP 以外的字符排在 U+E000–U+FFFF 之前。
 * @endif
 * @lang{EN}
 * Windows backend of the `collate` facet: `CompareStringEx` and
 * `LCMapStringEx(LCMAP_SORTKEY)`, called directly, with the collation rules picked by the
 * locale's `LC_COLLATE` BCP-47 name.
 *
 * Not the UCRT's `_strcoll_l`/`_strxfrm_l`: the former passes `SORT_STRINGSORT` to
 * `CompareStringEx`, the latter calls `LCMapStringEx` without it, so the order of the keys
 * disagrees with the comparison (measured on real Windows under de-DE: `"co-op"` is less than
 * `"coat"`, its key greater than `"coat"`'s). The MSVC STL's `collate`, which calls NLS itself,
 * disagrees the same way. This backend passes no flag to either: punctuation is ignored at
 * the first level, which agrees with glibc on that example.
 *
 * A narrow string goes to UTF-16 first, by `MultiByteToWideChar` in the code page of
 * `LC_COLLATE`, with `MB_ERR_INVALID_CHARS`: a byte sequence invalid in that code page is a
 * failure, as it is for the UCRT's `_strcoll_l`. The key `LCMapStringEx` produces is a string
 * of bytes: a narrow key is those bytes, a wide key one byte per `wchar_t`, as in the UCRT's
 * `wcsxfrm`.
 *
 * The `"C"` locale has no BCP-47 name and, as in the UCRT, compares by code unit, its key
 * being the input itself. Wide characters are UTF-16 code units, so under `"C"` a character
 * outside the BMP sorts before U+E000-U+FFFF.
 * @endif
 */

#pragma once

#include <IOv2/common/platform/clocale_backend.h>
#include <IOv2/facet/platform/windows/nls_api.h>

#include <cstddef>
#include <cstring>
#include <cwchar>
#include <optional>
#include <string>

namespace IOv2::detail
{
inline constexpr unsigned int win_cp_utf8 = 65001;   // CP_UTF8

// src, in loc's LC_COLLATE code page, as UTF-16; nullopt if it is invalid there.
inline std::optional<std::wstring> collate_widen(const char* src, clocale_handle loc)
{
    const int len = MultiByteToWideChar(loc->collate_cp, win_mb_err_invalid_chars, src, -1, nullptr, 0);
    if (len <= 0)
        return std::nullopt;
    std::wstring res(static_cast<std::size_t>(len - 1), L'\0');
    // len counts the NUL, which lands on res's own terminator.
    if (MultiByteToWideChar(loc->collate_cp, win_mb_err_invalid_chars, src, -1, res.data(), len) != len)
        return std::nullopt;
    return res;
}

inline std::optional<int> collate_compare_utf16(const wchar_t* s1, const wchar_t* s2, clocale_handle loc) noexcept
{
    const int res = CompareStringEx(loc->collate_bcp47.c_str(), 0, s1, -1, s2, -1, nullptr, nullptr, 0);
    if (res == 0)
        return std::nullopt;
    return res - win_cstr_equal;
}

// The sort key of s, without the NUL LCMapStringEx ends it with; nullopt on failure.
inline std::optional<std::string> collate_sort_key(const wchar_t* s, clocale_handle loc)
{
    const wchar_t* name = loc->collate_bcp47.c_str();
    const int len = LCMapStringEx(name, win_lcmap_sortkey, s, -1, nullptr, 0, nullptr, nullptr, 0);
    if (len <= 0)
        return std::nullopt;
    std::string res(static_cast<std::size_t>(len), '\0');
    // Under LCMAP_SORTKEY the destination is a buffer of len bytes, whatever its type.
    if (LCMapStringEx(name, win_lcmap_sortkey, s, -1,
                      reinterpret_cast<wchar_t*>(res.data()), len,    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                      nullptr, nullptr, 0) != len)
        return std::nullopt;
    res.pop_back();
    return res;
}

inline std::optional<int> collate_strcoll(const char* s1, const char* s2, clocale_handle loc)
{
    if (loc->collate_bcp47.empty())
    {
        const int res = std::strcmp(s1, s2);
        return (res > 0) - (res < 0);
    }
    const auto w1 = collate_widen(s1, loc);
    const auto w2 = collate_widen(s2, loc);
    if (!w1 || !w2)
        return std::nullopt;
    return collate_compare_utf16(w1->c_str(), w2->c_str(), loc);
}

inline std::optional<int> collate_wcscoll(const wchar_t* s1, const wchar_t* s2, clocale_handle loc) noexcept
{
    if (loc->collate_bcp47.empty())
    {
        const int res = std::wcscmp(s1, s2);
        return (res > 0) - (res < 0);
    }
    return collate_compare_utf16(s1, s2, loc);
}

inline std::optional<std::size_t> collate_strxfrm(char* dest, const char* src, std::size_t n, clocale_handle loc)
{
    if (loc->collate_bcp47.empty())
    {
        const std::size_t len = std::strlen(src);
        if (len < n)
            std::memcpy(dest, src, len + 1);
        return len;
    }
    const auto wide = collate_widen(src, loc);
    if (!wide)
        return std::nullopt;
    const auto key = collate_sort_key(wide->c_str(), loc);
    if (!key)
        return std::nullopt;
    if (key->size() < n)
        std::memcpy(dest, key->c_str(), key->size() + 1);
    return key->size();
}

inline std::optional<std::size_t> collate_wcsxfrm(wchar_t* dest, const wchar_t* src, std::size_t n, clocale_handle loc)
{
    if (loc->collate_bcp47.empty())
    {
        const std::size_t len = std::wcslen(src);
        if (len < n)
            std::wmemcpy(dest, src, len + 1);
        return len;
    }
    const auto key = collate_sort_key(src, loc);
    if (!key)
        return std::nullopt;
    if (key->size() < n)
    {
        for (std::size_t i = 0; i < key->size(); ++i)
            dest[i] = static_cast<wchar_t>(static_cast<unsigned char>((*key)[i]));
        dest[key->size()] = L'\0';
    }
    return key->size();
}

// Whether loc's LC_COLLATE code page, the one narrow strings are read in, is UTF-8.
inline bool collate_is_utf8(clocale_handle loc) noexcept
{
    return loc->collate_cp == win_cp_utf8;
}
}

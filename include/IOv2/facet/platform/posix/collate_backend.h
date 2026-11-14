// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file collate_backend.h
 * @lang{ZH}
 * `collate` facet 的 POSIX 后端：POSIX.1-2008 的 `strcoll_l`、`wcscoll_l`、`strxfrm_l`、
 * `wcsxfrm_l`，不切换线程 locale。macOS 上它们与 `nl_langinfo_l` 声明在 `<xlocale/_*.h>` 中，
 * 故此处显式包含。
 *
 * POSIX 没有为这些函数保留表示失败的返回值，只规定失败时可以设置 `errno`。glibc 从不报告
 * 失败；macOS 遇到当前编码下的非法字节序列时设置 `EILSEQ`，返回值却照常。故本后端调用前
 * 清零 `errno`、调用后检查，再恢复调用者的 `errno`，与 libstdc++ 的 `collate::do_transform`
 * 同理。
 * @endif
 * @lang{EN}
 * POSIX backend of the `collate` facet: the POSIX.1-2008 `strcoll_l`, `wcscoll_l`, `strxfrm_l`
 * and `wcsxfrm_l`, which leave the thread's locale alone. macOS declares them and
 * `nl_langinfo_l` in `<xlocale/_*.h>`, included here explicitly.
 *
 * POSIX reserves no return value of these functions for failure; it only lets them set
 * `errno`. glibc never reports a failure; macOS sets `EILSEQ` on a byte sequence invalid in
 * the encoding, yet returns an ordinary value. So this backend clears `errno` before a call,
 * checks it after, and puts the caller's `errno` back, as libstdc++'s `collate::do_transform`
 * does.
 * @endif
 */

#pragma once

#include <IOv2/common/platform/clocale_backend.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <langinfo.h>
#include <optional>
#include <utility>

#if defined(__APPLE__)
#include <xlocale.h>
#include <xlocale/_langinfo.h>
#include <xlocale/_string.h>
#include <xlocale/_wchar.h>
#endif

namespace IOv2::detail
{
// Runs call with errno cleared; nullopt if it set errno. The caller's errno is kept.
template <typename Call>
std::optional<decltype(std::declval<Call&>()())> collate_errno_call(Call call) noexcept
{
    const int saved = errno;
    errno = 0;
    const auto res = call();
    const bool failed = (errno != 0);
    errno = saved;
    if (failed)
        return std::nullopt;
    return res;
}

inline std::optional<int> collate_strcoll(const char* s1, const char* s2, clocale_handle loc) noexcept
{
    const auto res = collate_errno_call([&] { return strcoll_l(s1, s2, loc); });
    if (!res) return std::nullopt;
    return (*res > 0) - (*res < 0);
}

inline std::optional<int> collate_wcscoll(const wchar_t* s1, const wchar_t* s2, clocale_handle loc) noexcept
{
    const auto res = collate_errno_call([&] { return wcscoll_l(s1, s2, loc); });
    if (!res) return std::nullopt;
    return (*res > 0) - (*res < 0);
}

inline std::optional<std::size_t> collate_strxfrm(char* dest, const char* src, std::size_t n,
                                                  clocale_handle loc) noexcept
{
    return collate_errno_call([&] { return strxfrm_l(dest, src, n, loc); });
}

inline std::optional<std::size_t> collate_wcsxfrm(wchar_t* dest, const wchar_t* src, std::size_t n,
                                                  clocale_handle loc) noexcept
{
    return collate_errno_call([&] { return wcsxfrm_l(dest, src, n, loc); });
}

// Whether loc's codeset is UTF-8. glibc names it "UTF-8" in every UTF-8 locale, C.UTF-8
// included, and so does macOS.
inline bool collate_is_utf8(clocale_handle loc) noexcept
{
    return std::strcmp(nl_langinfo_l(CODESET, loc), "UTF-8") == 0;
}
}

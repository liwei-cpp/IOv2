// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file clocale_backend.h
 * @lang{ZH}
 * `clocale_wrapper` 的 Windows（UCRT）后端。
 *
 * UCRT 的 `_locale_t` 既不能复制、也报不出名字，也没有 `uselocale`。于是本后端在创建时把
 * 名字解析成具体名字并保存下来：复制即按保存的名字再 `_create_locale` 一次；临时切换线程
 * locale 则让本线程进入 `_ENABLE_PER_THREAD_LOCALE` 模式后 `setlocale`，此时 `setlocale` 只改
 * 本线程，全局 locale 与其他线程不受影响。解析名字时也在这一模式下进行，用完即恢复。
 *
 * 解析时还一并保存 `LC_CTYPE` 的 BCP-47 名字（如 `en-US`，取自 `___lc_locale_name_func`），
 * 供 facet 后端调用 `LCMapStringEx` 等以名字定 locale 的 NLS 函数；`"C"` locale 没有此名字。
 * @endif
 * @lang{EN}
 * Windows (UCRT) backend of `clocale_wrapper`.
 *
 * A UCRT `_locale_t` can neither be copied nor report its name, and there is no
 * `uselocale`. So this backend resolves the name when the locale is created and keeps it:
 * a copy is another `_create_locale` from the kept name, and switching the thread's
 * locale puts the thread in `_ENABLE_PER_THREAD_LOCALE` mode and calls `setlocale`, which
 * in that mode changes this thread alone, leaving the global locale and other threads
 * untouched. Resolving the name happens in the same mode and is undone at once.
 *
 * Resolving also keeps the BCP-47 name of `LC_CTYPE` (`en-US`, say, from
 * `___lc_locale_name_func`), for facet backends calling `LCMapStringEx` and the other NLS
 * functions that take a locale by name; the `"C"` locale has no such name.
 * @endif
 */

#pragma once

#include <IOv2/common/defs.h>

#include <clocale>
#include <memory>
#include <string>

namespace IOv2::detail
{
struct windows_clocale
{
    _locale_t    loc = nullptr;
    std::string  all_name;     // setlocale(LC_ALL) after switching to it: rebuilds the locale
    std::string  ctype_name;   // setlocale(LC_CTYPE) likewise: what name() reports
    std::wstring ctype_bcp47;  // LC_CTYPE's BCP-47 name ("en-US") for the NLS APIs; empty for "C"
};

using clocale_handle = windows_clocale*;

/**
 * @lang{ZH}
 * 切换线程 locale 前的状态：线程的 locale 模式与 locale 名字。本线程原先就在 per-thread
 * 模式下（嵌套）时，恢复的也是原先的模式与 locale。
 * @endif
 * @lang{EN}
 * The state before switching the thread's locale: the thread's locale mode and locale
 * name. When the thread was already in per-thread mode (nesting), what comes back is that
 * mode and that locale.
 * @endif
 */
struct clocale_saved
{
    int         mode = _DISABLE_PER_THREAD_LOCALE;
    std::string name;
};

// Puts this thread in per-thread locale mode, where setlocale changes this thread alone,
// and returns what clocale_restore needs to undo it.
inline clocale_saved clocale_enter_thread_mode()
{
    clocale_saved res;
    res.mode = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    try
    {
        res.name = std::setlocale(LC_ALL, nullptr);
    }
    catch (...)
    {
        _configthreadlocale(res.mode);
        throw;
    }
    return res;
}

// Puts back the thread locale and mode clocale_use (or clocale_enter_thread_mode) replaced.
inline void clocale_restore(const clocale_saved& old) noexcept
{
    std::setlocale(LC_ALL, old.name.c_str());
    _configthreadlocale(old.mode);
}

// A locale with all categories from name; null if the CRT rejects it. Throws only
// std::bad_alloc.
inline clocale_handle clocale_create(const char* name)
{
    auto res = std::make_unique<windows_clocale>();
    const clocale_saved old = clocale_enter_thread_mode();
    const char* all = nullptr;
    try
    {
        all = std::setlocale(LC_ALL, name);
        if (all)
        {
            res->all_name   = all;   // copied before the next setlocale reuses the buffer
            res->ctype_name = std::setlocale(LC_CTYPE, nullptr);
            if (const wchar_t* bcp47 = ___lc_locale_name_func()[LC_CTYPE])
                res->ctype_bcp47 = bcp47;
        }
    }
    catch (...)
    {
        clocale_restore(old);
        throw;
    }
    clocale_restore(old);
    if (!all)
        return nullptr;

    res->loc = _create_locale(LC_ALL, res->all_name.c_str());
    return res->loc ? res.release() : nullptr;
}

// An independent copy of loc; null on failure. Throws only std::bad_alloc.
inline clocale_handle clocale_duplicate(clocale_handle loc)
{
    auto res = std::make_unique<windows_clocale>(
        windows_clocale{.all_name = loc->all_name, .ctype_name = loc->ctype_name,
                        .ctype_bcp47 = loc->ctype_bcp47});
    res->loc = _create_locale(LC_ALL, res->all_name.c_str());
    return res->loc ? res.release() : nullptr;
}

inline void clocale_release(clocale_handle loc) noexcept
{
    // Owned since clocale_create / clocale_duplicate released it from a unique_ptr.
    const std::unique_ptr<windows_clocale> owned(loc);
    _free_locale(owned->loc);
}

// The name of loc's LC_CTYPE category, as setlocale reports it. "POSIX" is not a name the
// CRT knows, so no locale has it.
inline const char* clocale_ctype_name(clocale_handle loc) noexcept
{
    return loc->ctype_name.c_str();
}

// Makes loc the calling thread's locale. Throws io_error, with nothing changed, if
// setlocale refuses the switch.
inline clocale_saved clocale_use(clocale_handle loc)
{
    clocale_saved old = clocale_enter_thread_mode();
    if (!std::setlocale(LC_ALL, loc->all_name.c_str()))
    {
        clocale_restore(old);
        throw io_error("clocale_user: setlocale rejected " + loc->all_name);
    }
    return old;
}
}

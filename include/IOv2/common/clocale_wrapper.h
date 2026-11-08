// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file clocale_wrapper.h
 * @lang{ZH}
 * C 库 locale 对象的 RAII 包装类。
 * @endif
 * @lang{EN}
 * RAII wrapper for C library locale objects.
 * @endif
 */

#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/common/platform/clocale_backend.h>

#include <clocale>
#include <string>

namespace IOv2
{
/**
 * @lang{ZH}
 * C 库 locale 对象的 RAII 包装类。
 *
 * @note 支持 glibc、macOS 与 Windows（UCRT），平台相关的部分在 `common/platform/` 下的后端
 *       里：POSIX 上包装 `locale_t`，Windows 上包装 `_locale_t` 及其解析后的名字。
 * @note 此类不是线程安全的。
 * @endif
 *
 * @lang{EN}
 * RAII wrapper for C library locale objects.
 *
 * @note Supports glibc, macOS and Windows (UCRT); the platform-specific part lives in the
 *       backends under `common/platform/`: a `locale_t` on POSIX, a `_locale_t` together
 *       with its resolved names on Windows.
 * @note This class is NOT thread-safe.
 * @endif
 */
struct clocale_wrapper
{
    friend struct clocale_user;
    template <typename CharT> friend class ctype_conf;

    /**
     * @lang{ZH}
     * @brief 以 `newlocale(LC_ALL_MASK, name)` 构造。
     *
     * `name` 按 `std::string` 的全长解释：含内嵌 NUL 的名字在此拒绝，而不是在 `c_str()` 处
     * 静默截断——否则首字节为 NUL 的名字会变成 `""`、按环境解析，调用方显式给的名字被丢掉
     * 却没有任何报告。这是唯一一处本类自己做的判定；名字是否合法仍由 C 库说了算。
     *
     * @param name locale 名；`""` 按 POSIX 规则查环境。
     * @throws io_error `name` 含内嵌 NUL，或 `newlocale` 拒绝该名字。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs from `newlocale(LC_ALL_MASK, name)`.
     *
     * `name` is taken at its full `std::string` length: a name with an embedded NUL is
     * rejected here rather than silently cut at `c_str()` -- otherwise a name whose first
     * byte is NUL would turn into `""`, be resolved from the environment, and the name the
     * caller explicitly gave would be dropped without a word. This is the only check the
     * class makes itself; whether a name is valid is still the C library's call.
     *
     * @param name The locale name; `""` looks at the environment per POSIX.
     * @throws io_error `name` contains an embedded NUL, or `newlocale` rejects it.
     * @endif
     */
    explicit clocale_wrapper(const std::string& name)
        : c_locale(name.find('\0') == std::string::npos ? detail::clocale_create(name.c_str()) : nullptr)
    {
        if (name.find('\0') != std::string::npos)
            throw io_error("clocale_wrapper: locale name contains an embedded NUL");

        if (!c_locale)
            throw io_error("Cannot construct a C locale: " + name);
    }

    ~clocale_wrapper() noexcept
    {
        if (c_locale)
            detail::clocale_release(c_locale);
    }

    clocale_wrapper(const clocale_wrapper& val)
        : c_locale(val.c_locale ? detail::clocale_duplicate(val.c_locale) : nullptr)
    {
        if (val.c_locale && !c_locale)
            throw io_error("clocale_wrapper: copying the locale failed during copy construction");
    }

    clocale_wrapper(clocale_wrapper&& val) noexcept
        : c_locale(val.c_locale)
    {
        val.c_locale = nullptr;
    }

    clocale_wrapper& operator=(clocale_wrapper&& val) noexcept
    {
        if (this != &val)
        {
            if (c_locale)
                detail::clocale_release(c_locale);
            c_locale = val.c_locale;
            val.c_locale = nullptr;
        }
        return *this;
    }

    clocale_wrapper& operator=(const clocale_wrapper& val)
    {
        if (this != &val)
        {
            detail::clocale_handle tmp = val.c_locale ? detail::clocale_duplicate(val.c_locale) : nullptr;
            if (val.c_locale && !tmp)
                throw io_error("clocale_wrapper: copying the locale failed during assignment");

            if (c_locale)
                detail::clocale_release(c_locale);
            c_locale = tmp;
        }
        return *this;
    }

    /**
     * @lang{ZH}
     * 返回本 locale 的 `LC_CTYPE` 名字。
     *
     * 这是 locale 自己报出的**已解析**名字，不是构造时的实参：以 `""` 构造时，返回的是当时
     * 查环境得到的具体名字（如 `"C.UTF-8"`）；别名按平台规范化（glibc 把 `"POSIX"` 报成
     * `"C"`）。返回值可再交给 `newlocale` 得到同一个 `LC_CTYPE`。
     *
     * @note 名字由 `common/platform/` 下的后端取得：glibc 上用扩展
     *       `nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), …)`，macOS 上用 `querylocale`，Windows 上
     *       是构造时 `setlocale(LC_CTYPE, nullptr)` 报出并保存的名字。POSIX 没有等价接口，musl
     *       等其他 libc 不受支持。
     * @note macOS 与 glibc 有两处不同：别名不规范化，`"POSIX"` 仍报 `"POSIX"`；`LC_CTYPE` 可以
     *       来自只有这一个类别的 locale，如 `LC_CTYPE=UTF-8` 时以 `""` 构造得到的 `"UTF-8"`——
     *       它能交给 `newlocale(LC_CTYPE_MASK, …)`，但本类构造用的 `newlocale(LC_ALL_MASK, …)`
     *       拒绝它。
     * @note Windows 上名字是 CRT 的写法，如 `"English_United States.1252"`；CRT 不认 `"POSIX"`，
     *       以它构造会抛异常。
     * @return 已解析的 locale 名字。
     * @throws io_error 若本对象处于 moved-from 状态。
     * @endif
     *
     * @lang{EN}
     * Return this locale's `LC_CTYPE` name.
     *
     * This is the **resolved** name the locale reports for itself, not the argument it was
     * built from: for a locale constructed from `""` it is the concrete name the environment
     * resolved to at that time (e.g. `"C.UTF-8"`), and aliases are normalized the way the
     * platform normalizes them (glibc reports `"POSIX"` as `"C"`). The result can be handed
     * back to `newlocale` to obtain the same `LC_CTYPE`.
     *
     * @note The name comes from the backend under `common/platform/`: the glibc extension
     *       `nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), …)` on glibc, `querylocale` on macOS,
     *       and on Windows the name `setlocale(LC_CTYPE, nullptr)` reported at construction,
     *       kept since. POSIX has no equivalent, so other C libraries (musl, say) are not
     *       supported.
     * @note macOS differs from glibc in two ways: aliases are not normalized, so `"POSIX"`
     *       comes back as `"POSIX"`; and `LC_CTYPE` may come from a locale that has that
     *       category alone, such as the `"UTF-8"` a wrapper built from `""` gets under
     *       `LC_CTYPE=UTF-8` -- `newlocale(LC_CTYPE_MASK, …)` takes it, but the
     *       `newlocale(LC_ALL_MASK, …)` this class constructs with rejects it.
     * @note On Windows the name is spelled the CRT's way, e.g. `"English_United States.1252"`;
     *       the CRT does not know `"POSIX"`, so constructing from it throws.
     * @return The resolved locale name.
     * @throws io_error If this object is in a moved-from state.
     * @endif
     */
    [[nodiscard]] std::string name() const
    {
        if (!c_locale)
            throw io_error("clocale_wrapper::name: wrapper is in moved-from state");

        return detail::clocale_ctype_name(c_locale);
    }

private:
    detail::clocale_handle c_locale;
};

/**
 * @lang{ZH}
 * 用于临时切换当前线程的 C locale 的 RAII 守卫。
 *
 * @note 此类不是线程安全的，因为：
 *       - 对象必须在同一个线程中构造和析构
 *       - 禁用复制和移动操作以防止跨线程使用
 *       - uselocale() 仅影响调用线程的语言环境
 *
 * @par 示例
 * @code
 *     clocale_wrapper zh_locale("zh_CN.UTF-8");
 *     {
 *         clocale_user guard(zh_locale);  // 切换到中文语言环境
 *         // ... 执行语言环境相关的操作 ...
 *     }  // 自动恢复之前的语言环境
 * @endcode
 * @endif
 *
 * @lang{EN}
 * RAII guard for temporarily switching the current thread's C locale.
 *
 * @note This class is NOT thread-safe in the sense that:
 *       - The object must be constructed and destructed in the SAME thread
 *       - Copy and move operations are disabled to prevent cross-thread usage
 *       - uselocale() only affects the calling thread's locale
 *
 * @par Example
 * @code
 *     clocale_wrapper zh_locale("zh_CN.UTF-8");
 *     {
 *         clocale_user guard(zh_locale);  // Switch to Chinese locale
 *         // ... locale-sensitive operations ...
 *     }  // Automatically restore previous locale
 * @endcode
 * @endif
 */
struct clocale_user
{
    /**
     * @lang{ZH}
     * 构造一个语言环境守卫，切换到指定的语言环境。
     * @param wrapper 要切换到的语言环境
     * @throws io_error 如果 wrapper 处于 moved-from 状态；Windows 上 `setlocale` 拒绝切换时也抛
     * @note 捕获当前线程的语言环境以便稍后恢复。POSIX 上用 `uselocale`；Windows 上让本线程进入
     *       per-thread locale 模式后 `setlocale`，只影响本线程，析构时连同模式一起恢复。
     * @endif
     *
     * @lang{EN}
     * Construct a locale guard that switches to the specified locale.
     * @param wrapper The locale to switch to
     * @throws io_error If wrapper is in moved-from state; on Windows also if `setlocale`
     *         refuses the switch
     * @note Captures the current thread's locale for later restoration. On POSIX this is
     *       `uselocale`; on Windows the thread enters per-thread locale mode and calls
     *       `setlocale`, which affects this thread alone, and the destructor restores the
     *       mode along with the locale.
     * @endif
     */
    explicit clocale_user(const clocale_wrapper& wrapper)
    {
        if (!wrapper.c_locale)
            throw io_error("clocale_user: wrapper is in moved-from state");
        old = detail::clocale_use(wrapper.c_locale);
    }

    // Non-copyable and non-movable to prevent cross-thread usage
    clocale_user(const clocale_user&) = delete;
    clocale_user& operator=(const clocale_user&) = delete;
    clocale_user(clocale_user&&) = delete;
    clocale_user& operator=(clocale_user&&) = delete;

    /**
     * @lang{ZH}
     * 恢复之前的语言环境。
     * @note 必须由构造此对象的同一个线程调用
     * @endif
     *
     * @lang{EN}
     * Restore the previous locale.
     * @note Must be called from the same thread that constructed this object
     * @endif
     */
    ~clocale_user() noexcept
    {
        detail::clocale_restore(old);
    }
private:
    detail::clocale_saved old{};
};
}

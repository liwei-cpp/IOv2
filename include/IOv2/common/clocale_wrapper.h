// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file clocale_wrapper.h
 * @lang{ZH}
 * POSIX locale_t 的 RAII 包装类。
 * @endif
 * @lang{EN}
 * RAII wrapper for POSIX locale_t objects.
 * @endif
 */

#pragma once
#include <IOv2/common/defs.h>

#include <clocale>
#include <stdexcept>
#include <string>

#include <langinfo.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * POSIX locale_t 对象的 RAII 包装类。
 *
 * @note 此类需要 POSIX locale 扩展（在 Linux、macOS 上可用）。
 *       在 Windows 上不可用。
 * @note 此类不是线程安全的。
 * @endif
 *
 * @lang{EN}
 * RAII wrapper for POSIX locale_t objects.
 *
 * @note This class requires POSIX locale extensions (available on Linux, macOS).
 *       Not available on Windows.
 * @note This class is NOT thread-safe.
 * @endif
 */
struct clocale_wrapper
{
    friend struct clocale_user;
    template <typename CharT> friend class ctype_conf;

    explicit clocale_wrapper(const char* name)
        : c_locale(name ? newlocale(LC_ALL_MASK, name, nullptr) : nullptr)
    {
        if (!name)
            throw cvt_error("clocale_wrapper: name cannot be null");

        if (!c_locale)
            throw cvt_error(std::string("Cannot construct a C locale: ") + name);
    }

    ~clocale_wrapper() noexcept
    {
        if (c_locale)
            freelocale(c_locale);
    }

    clocale_wrapper(const clocale_wrapper& val)
        : c_locale(val.c_locale ? duplocale(val.c_locale) : nullptr)
    {
        if (val.c_locale && !c_locale)
            throw cvt_error("clocale_wrapper: duplocale failed during copy construction");
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
                freelocale(c_locale);
            c_locale = val.c_locale;
            val.c_locale = nullptr;
        }
        return *this;
    }

    clocale_wrapper& operator=(const clocale_wrapper& val)
    {
        if (this != &val)
        {
            locale_t tmp = val.c_locale ? duplocale(val.c_locale) : nullptr;
            if (val.c_locale && !tmp)
                throw cvt_error("clocale_wrapper: duplocale failed during assignment");

            if (c_locale)
                freelocale(c_locale);
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
     * @note 实现依赖 glibc 扩展 `nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), …)`（`<langinfo.h>`）；
     *       POSIX 没有等价接口，musl 等 libc 需要另想办法（例如自己保存解析后的名字）。
     * @return 已解析的 locale 名字。
     * @throws cvt_error 若本对象处于 moved-from 状态。
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
     * @note Relies on the glibc extension `nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), …)`
     *       (`<langinfo.h>`); POSIX has no equivalent, so another libc (musl, say) would need
     *       a different route, such as keeping the resolved name itself.
     * @return The resolved locale name.
     * @throws cvt_error If this object is in a moved-from state.
     * @endif
     */
    [[nodiscard]] std::string name() const
    {
        if (!c_locale)
            throw cvt_error("clocale_wrapper::name: wrapper is in moved-from state");

        return nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), c_locale);
    }

private:
    locale_t c_locale;
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
     * @throws cvt_error 如果 wrapper 处于 moved-from 状态
     * @note 捕获当前线程的语言环境以便稍后恢复
     * @endif
     *
     * @lang{EN}
     * Construct a locale guard that switches to the specified locale.
     * @param wrapper The locale to switch to
     * @throws cvt_error If wrapper is in moved-from state
     * @note Captures the current thread's locale for later restoration
     * @endif
     */
    explicit clocale_user(const clocale_wrapper& wrapper)
    {
        if (!wrapper.c_locale)
            throw cvt_error("clocale_user: wrapper is in moved-from state");
        old = uselocale(wrapper.c_locale);
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
        uselocale(old);
    }
private:
    locale_t old = nullptr;
};
}

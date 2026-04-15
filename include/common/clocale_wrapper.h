#pragma once
#include <locale.h>
#include <stdexcept>

#include <common/defs.h>

namespace IOv2
{
struct clocale_wrapper
{
    friend struct clocale_user;
    template <typename CharT> friend class ctype_conf;

    clocale_wrapper(const char* name)
        : c_locale(newlocale(LC_ALL_MASK, name, 0))
    {
        if (!c_locale)
        {
            throw cvt_error(std::string("Cannot construct a C locale: ") + name);
        }
    }

    ~clocale_wrapper() noexcept
    {
        if (c_locale)
            freelocale(c_locale);
    }
    
    clocale_wrapper(const clocale_wrapper& val)
        : c_locale(duplocale(val.c_locale))
    {
        if (!c_locale)
        {
            throw cvt_error("clocale_wrapper: duplocale failed during copy construction");
        }
    }

    clocale_wrapper(clocale_wrapper&& val) noexcept
        : c_locale(val.c_locale)
    {
        val.c_locale = nullptr;
    }

    clocale_wrapper& operator = (clocale_wrapper&& val) noexcept
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

    clocale_wrapper& operator= (const clocale_wrapper& val)
    {
        if (this != &val)
        {
            locale_t tmp = duplocale(val.c_locale);
            if (!tmp)
            {
                throw cvt_error("clocale_wrapper: duplocale failed during assignment");
            }
            if (c_locale)
                freelocale(c_locale);
            c_locale = tmp;
        }
        return *this;
    }

private:
    locale_t c_locale;
};

/// RAII guard for temporarily switching the current thread's C locale.
///
/// @note This class is NOT thread-safe in the sense that:
///       - The object must be constructed and destructed in the SAME thread
///       - Copy and move operations are disabled to prevent cross-thread usage
///       - uselocale() only affects the calling thread's locale
///
/// @example
///     clocale_wrapper zh_locale("zh_CN.UTF-8");
///     {
///         clocale_user guard(zh_locale);  // Switch to Chinese locale
///         // ... locale-sensitive operations ...
///     }  // Automatically restore previous locale
///
struct clocale_user
{
    /// Construct a locale guard that switches to the specified locale.
    /// @param wrapper The locale to switch to
    /// @note Captures the current thread's locale for later restoration
    explicit clocale_user(const clocale_wrapper& wrapper) noexcept
        : old(uselocale(wrapper.c_locale))
    { }

    // Non-copyable and non-movable to prevent cross-thread usage
    clocale_user(const clocale_user&) = delete;
    clocale_user& operator= (const clocale_user&) = delete;
    clocale_user(clocale_user&&) = delete;
    clocale_user& operator= (clocale_user&&) = delete;

    /// Restore the previous locale.
    /// @note Must be called from the same thread that constructed this object
    ~clocale_user() noexcept
    {
        uselocale(old);
    }
private:
    locale_t old;
};
}
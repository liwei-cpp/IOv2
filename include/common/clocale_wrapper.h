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

struct clocale_user
{
    clocale_user(const clocale_wrapper& wrapper)
        : old(uselocale(wrapper.c_locale))
    { }
    
    clocale_user(const clocale_user&) = delete;
    clocale_user& operator= (const clocale_user&) = delete;
    
    ~clocale_user()
    {
        uselocale(old);
    }
private:
    locale_t old;
};
}
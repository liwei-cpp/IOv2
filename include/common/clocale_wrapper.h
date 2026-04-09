#pragma once
#include <locale.h>
#include <stdexcept>

#include <common/defs.h>

namespace IOv2
{
struct clocale_wrapper
{
    clocale_wrapper(const char* name)
        : c_locale(newlocale(LC_ALL_MASK, name, 0))
    {
        if (!c_locale)
        {
            throw cvt_error(std::string("Cannot construct a C locale: ") + name);
        }
    }

    ~clocale_wrapper()
    {
        freelocale(c_locale);
    }
    
    clocale_wrapper(const clocale_wrapper& val)
        : c_locale(duplocale(val.c_locale)) {}

    clocale_wrapper& operator= (const clocale_wrapper& val)
    {
        freelocale(c_locale);
        c_locale = duplocale(val.c_locale);
        return *this;
    }

    locale_t c_locale;
};

struct clocale_user
{
    clocale_user(locale_t c_locale)
        : old(uselocale(c_locale))
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
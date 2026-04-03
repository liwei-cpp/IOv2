#pragma once

#include <stdexcept>

#include <common/dump_info.h>

inline void VERIFY(bool val)
{
    if (!val) throw std::runtime_error("check fail");
}

template <typename T>
inline void FAIL_SEEK(T& obj, size_t pos)
{
    try
    {
        if constexpr (IOv2::io_device<T>)
            obj.dseek(pos);
        else
            obj.seek(pos);

        dump_info("fail-seek check fail");
        std::abort();
    }
    catch(...) {}
}

template <typename T>
inline void FAIL_RSEEK(T& obj, size_t pos)
{
    try
    {
        if constexpr (IOv2::io_device<T>)
            obj.drseek(pos);
        else
            obj.rseek(pos);

        dump_info("fail-seek check fail");
        std::abort();
    }
    catch(...) {}
}
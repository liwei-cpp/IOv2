#pragma once

#include <stdexcept>
#include <string>
#include <iostream>

#include <common/dump_info.h>

template <typename T>
inline void verify_impl(const T& val, const char* expr, const char* file, int line)
{
    if (!static_cast<bool>(val))
    {
        std::string msg = "check fail: (";
        msg += expr;
        msg += ") at ";
        msg += file;
        msg += ":";
        msg += std::to_string(line);
        dump_info(msg + "\n");
        throw std::runtime_error("check fail");
    }
}

#define VERIFY(...) verify_impl((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

template <typename T>
inline void FAIL_SEEK(T& obj, size_t pos)
{
    try
    {
        if constexpr (IOv2::io_device<T>)
            obj.dseek(pos);
        else
            obj.seek(pos);

        dump_info("fail-seek check fail\n");
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

        dump_info("fail-seek check fail\n");
        std::abort();
    }
    catch(...) {}
}

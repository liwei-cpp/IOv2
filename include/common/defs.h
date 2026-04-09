#pragma once
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace IOv2
{
    constexpr static size_t INF_LEN = std::numeric_limits<size_t>::max();

    struct device_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct cvt_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct eof_error : std::exception
    {
        using std::exception::exception;
    };
    
    struct stream_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;;
    };
}
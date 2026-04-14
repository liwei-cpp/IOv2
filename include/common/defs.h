#pragma once
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace IOv2
{
    // glibc's AVX2-optimized string functions (e.g., wcsxfrm, wcscoll) use SIMD instructions
    // that read 32 bytes at a time, even when processing smaller strings. This can cause
    // Valgrind to report "Invalid read of size 32" when the allocated buffer is smaller than
    // 32 bytes. To avoid these false positives, we add padding to ensure buffers passed to
    // these functions are at least 32 bytes larger than the actual data.
    constexpr static size_t SIMD_PADDING_BYTES = 32;

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
        using std::runtime_error::runtime_error;
    };
}
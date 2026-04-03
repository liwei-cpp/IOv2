#pragma once
#include <io/objects/objects.h>

namespace
{
    inline void dump_info(const std::string& info)
    {
        IOv2::cout << info << IOv2::flush;
    }
}
#pragma once

#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>

// Deliberately free of support/verify.h: that header reaches objects.h through
// dump_info.h, and every TU that sees objects.h has to emit code for its eight
// inline stream singletons. A failure here throws what VERIFY threw, so the
// aggregate main()s that still catch it report it exactly as before.
inline std::string exe_path()
{
    char dest[PATH_MAX];
    memset(dest, 0, sizeof(dest)); // readlink does not null terminate!
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1)
        throw std::runtime_error("exe_path: readlink(/proc/self/exe) failed");
    return dest;
}

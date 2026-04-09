#include <string>
#include <cstring>
#include <common/verify.h>

inline std::string exe_path()
{
    char dest[PATH_MAX];
    memset(dest,0,sizeof(dest)); // readlink does not null terminate!
    VERIFY(readlink("/proc/self/exe", dest, PATH_MAX) != -1);
    return dest;
}
#pragma once

#include <device/std_device.h>
#include <io/istream.h>
#include <io/ostream.h>

#include <io/objects/in_impl.h>
#include <io/objects/out_impl.h>

namespace IOv2
{
inline void sync_with_stdio(bool sync = true)
{
    cout.sync_with_stdio(sync);
    cerr.sync_with_stdio(sync);
    clog.sync_with_stdio(sync);

    wcout.sync_with_stdio(sync);
    wcerr.sync_with_stdio(sync);
    wclog.sync_with_stdio(sync);

    cin.sync_with_stdio(sync);
    wcin.sync_with_stdio(sync);
}
}
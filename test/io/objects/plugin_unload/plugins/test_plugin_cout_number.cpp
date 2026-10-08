// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// Loaded and unloaded by test_plugin_unload.cpp. It is the first module in the
// process to write a number to cout, so the numeric facet cout's locale caches
// is the one built here.
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/arithmetic.h>

extern "C" __attribute__((visibility("default"))) void iov2_test_plugin_write()
{
    IOv2::cout << 42;
}

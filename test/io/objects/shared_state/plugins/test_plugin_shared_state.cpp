// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// Loaded by test_shared_state.cpp and built with hidden visibility, so that none
// of its inline variables merge with the host's: whatever process-wide state it
// reaches has to come from libiov2 itself.
#include <IOv2/facet/messages.h>
#include <IOv2/facet/timeio.h>
#include <IOv2/io/io_base.h>

#include <cstddef>

extern "C" __attribute__((visibility("default")))
void iov2_test_plugin_bind_text_domain(const char* domain, const char* dirname)
{
    IOv2::base_ft<IOv2::messages>::bind_text_domain(domain, dirname);
}

extern "C" __attribute__((visibility("default")))
std::size_t iov2_test_plugin_xalloc()
{
    return IOv2::ios_base<char>::xalloc();
}

extern "C" __attribute__((visibility("default")))
const void* iov2_test_plugin_timezone_tree()
{
    return &IOv2::base_ft<IOv2::timeio>::s_timezone_tree;
}

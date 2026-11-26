// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// Loaded by test_shared_state.cpp and built with its variables hidden, so that none
// of its inline variables merge with the host's: whatever process-wide state it
// reaches has to come from libiov2 itself.
#include <IOv2/facet/messages.h>
#include <IOv2/facet/numeric.h>
#include <IOv2/facet/timeio.h>
#include <IOv2/io/io_base.h>

#include <support/plugin.h>

#include <cstddef>

extern "C" IOV2_TEST_PLUGIN_EXPORT
void iov2_test_plugin_bind_text_domain(const char* domain, const char* dirname)
{
    IOv2::base_ft<IOv2::messages>::bind_text_domain(domain, dirname);
}

extern "C" IOV2_TEST_PLUGIN_EXPORT
std::size_t iov2_test_plugin_xalloc()
{
    return IOv2::ios_base<char>::xalloc();
}

// Whether this module's keys for numeric<char>, as a facet and as a facet conf, equal the
// host's ones it is handed.
extern "C" IOV2_TEST_PLUGIN_EXPORT
bool iov2_test_plugin_same_numeric_keys(const void* facet_key, const void* conf_key)
{
    return *static_cast<const IOv2::facet_id_t*>(facet_key) == IOv2::type_id_v<IOv2::numeric<char>>()
        && *static_cast<const IOv2::facet_id_t*>(conf_key) == IOv2::numeric_conf<char>::id();
}

extern "C" IOV2_TEST_PLUGIN_EXPORT
const void* iov2_test_plugin_timezone_tree()
{
    return &IOv2::base_ft<IOv2::timeio>::s_timezone_tree;
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * State that has to be one per process, reached from a plugin that hides its
 * variables (-fvisibility-ms-compat; its types stay visible, see iov2_export.h).
 * Such a module merges none of its inline variables with the host's -- on Windows no
 * module ever does -- so the text-domain table, the xalloc() counter and the
 * time-zone trie it reaches are its own unless libiov2 exports them. They used to be:
 * a domain the plugin bound did not exist for the host, the plugin's first xalloc()
 * handed out index 0 again, and the plugin built a trie of its own at load time.
 *
 * Only the installed-shared consumer builds the plugin; elsewhere the tests skip.
 */
#include <IOv2/facet/messages.h>
#include <IOv2/facet/numeric.h>
#include <IOv2/facet/timeio.h>
#include <IOv2/io/io_base.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>

#if defined(IOV2_TEST_SHARED_STATE_PLUGIN)
#include <support/plugin.h>

#include <string>

namespace
{
    namespace plugin = iov2_test::plugin;

    // Loaded once and never closed: shared mode pins it anyway.
    plugin::handle shared_state_plugin()
    {
        static const plugin::handle h = [] {
            std::string error;
            const plugin::handle opened = plugin::open(IOV2_TEST_SHARED_STATE_PLUGIN, error);
            if (opened == nullptr) ADD_FAILURE() << error;
            return opened;
        }();
        return h;
    }

    template <typename Fn>
    Fn* plugin_function(const char* name)
    {
        if (shared_state_plugin() == nullptr) return nullptr;
        Fn* fn = plugin::function<Fn>(shared_state_plugin(), name);
        if (fn == nullptr) ADD_FAILURE() << "the plugin exports no " << name;
        return fn;
    }
}
#endif

TEST(SharedState, ATextDomainBoundInAPluginIsSeenByTheHost)
{
#if !defined(IOV2_TEST_SHARED_STATE_PLUGIN)
    GTEST_SKIP() << "needs the installed-shared consumer";
#else
    auto* bind = plugin_function<void(const char*, const char*)>("iov2_test_plugin_bind_text_domain");
    ASSERT_NE(bind, nullptr);
    bind("iov2_shared_state", "/iov2/shared/state");
    EXPECT_EQ(IOv2::base_ft<IOv2::messages>::get_dirname("iov2_shared_state"),
              std::filesystem::path("/iov2/shared/state"));
#endif
}

TEST(SharedState, XallocNeverRepeatsAnIndexAcrossModules)
{
#if !defined(IOV2_TEST_SHARED_STATE_PLUGIN)
    GTEST_SKIP() << "needs the installed-shared consumer";
#else
    auto* plugin_xalloc = plugin_function<std::size_t()>("iov2_test_plugin_xalloc");
    ASSERT_NE(plugin_xalloc, nullptr);
    // One counter hands out increasing indices. A plugin with a counter of its own
    // starts again from 0, below anything the host already has.
    const std::size_t host = IOv2::ios_base<char>::xalloc();
    const std::size_t in_plugin = plugin_xalloc();
    const std::size_t host_again = IOv2::ios_base<char>::xalloc();
    EXPECT_LT(host, in_plugin);
    EXPECT_LT(in_plugin, host_again);
#endif
}

// Facets are keyed by type_info. libc++ compares a hidden type's type_info by address
// alone, so on macOS a module that hid its types had keys no other module matched.
TEST(SharedState, TheFacetKeysOfOneTypeAgreeAcrossModules)
{
#if !defined(IOV2_TEST_SHARED_STATE_PLUGIN)
    GTEST_SKIP() << "needs the installed-shared consumer";
#else
    auto* same = plugin_function<bool(const void*, const void*)>("iov2_test_plugin_same_numeric_keys");
    ASSERT_NE(same, nullptr);
    const IOv2::facet_id_t facet_key = IOv2::type_id_v<IOv2::numeric<char>>();
    const IOv2::facet_id_t conf_key = IOv2::numeric_conf<char>::id();
    EXPECT_TRUE(same(&facet_key, &conf_key));
#endif
}

TEST(SharedState, TheTimeZoneTreeIsBuiltOnce)
{
#if !defined(IOV2_TEST_SHARED_STATE_PLUGIN)
    GTEST_SKIP() << "needs the installed-shared consumer";
#else
    auto* tree = plugin_function<const void*()>("iov2_test_plugin_timezone_tree");
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(tree(), static_cast<const void*>(&IOv2::base_ft<IOv2::timeio>::s_timezone_tree));
#endif
}

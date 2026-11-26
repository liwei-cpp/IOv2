// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * State that has to be one per process, reached from a plugin built with hidden
 * visibility. Such a module merges none of its inline variables with the host's --
 * on Windows no module ever does -- so the text-domain table, the xalloc() counter
 * and the time-zone trie it reaches are its own unless libiov2 exports them. They
 * used to be: a domain the plugin bound did not exist for the host, the plugin's
 * first xalloc() handed out index 0 again, and the plugin built a trie of its own
 * at load time.
 *
 * Only the installed-shared consumer builds the plugin; elsewhere the tests skip.
 */
#include <IOv2/facet/messages.h>
#include <IOv2/facet/timeio.h>
#include <IOv2/io/io_base.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>

#if defined(IOV2_TEST_SHARED_STATE_PLUGIN)
#include <dlfcn.h>

namespace
{
    // Loaded once and never closed: shared mode pins it anyway.
    void* plugin()
    {
        static void* const handle = [] {
            void* h = ::dlopen(IOV2_TEST_SHARED_STATE_PLUGIN, RTLD_NOW);
            if (h == nullptr) ADD_FAILURE() << ::dlerror();
            return h;
        }();
        return handle;
    }

    template <typename Fn>
    Fn* plugin_function(const char* name)
    {
        if (plugin() == nullptr) return nullptr;
        void* sym = ::dlsym(plugin(), name);
        if (sym == nullptr) ADD_FAILURE() << name << ": " << ::dlerror();
        return reinterpret_cast<Fn*>(sym);
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

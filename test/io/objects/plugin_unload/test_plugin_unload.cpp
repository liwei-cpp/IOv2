// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A plugin in shared-library mode writes `cout << 42` and is dlclose'd; the host
 * then writes `cout << 1`. The numeric facet cout's locale cached was built in the
 * plugin -- its vtable, its type_info key and its control block all live in the
 * plugin's image -- so unloading the plugin left the cache pointing into unmapped
 * memory, and the host's write crashed. Every module that includes IOv2 in shared
 * mode now pins itself at load time: the plugin stays resident and the cache stays
 * valid.
 *
 * The plugin is built with -fno-gnu-unique under g++: otherwise its inline
 * variables are STB_GNU_UNIQUE, glibc never unloads it, and the test proves nothing.
 * Only the installed-shared consumer builds the plugin; elsewhere the test skips.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/test_child.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include <dlfcn.h>

TEST(PluginUnload, AFacetCachedByAnUnloadedPluginStaysUsable)
{
#if !defined(IOV2_TEST_UNLOAD_PLUGIN)
    GTEST_SKIP() << "needs the installed-shared consumer";
#else
    if (in_test_child())
    {
        if (std::freopen("plugin_unload.out", "w", stdout) == nullptr) std::_Exit(1);

        void* plugin = ::dlopen(IOV2_TEST_UNLOAD_PLUGIN, RTLD_NOW);
        if (plugin == nullptr) { std::fprintf(stderr, "%s\n", ::dlerror()); std::_Exit(2); }
        auto write = reinterpret_cast<void (*)()>(::dlsym(plugin, "iov2_test_plugin_write"));
        if (write == nullptr) std::_Exit(3);
        write();
        ::dlclose(plugin);

        if (::dlopen(IOV2_TEST_UNLOAD_PLUGIN, RTLD_NOW | RTLD_NOLOAD) == nullptr) std::_Exit(4);

        IOv2::cout << 1;
        IOv2::cout.flush();
        if (!IOv2::cout || std::fflush(stdout) != 0) std::_Exit(5);
        std::_Exit(0);
    }

    ASSERT_EQ(run_test_child("PluginUnload.AFacetCachedByAnUnloadedPluginStaysUsable"), 0);
    std::ifstream out("plugin_unload.out");
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(out), {}), "421");
#endif
}

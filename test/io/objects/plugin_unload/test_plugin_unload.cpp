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
#include <sys/wait.h>
#include <unistd.h>

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

/**
 * The main program pins itself too, and glibc's dladdr names it by argv[0], which
 * the caller of exec chose. The pin used to dlopen that name: with argv[0] set to
 * "/dev/stdin" it opened stdin and read the input away before main ran (and with a
 * FIFO it blocked there). The child here is run that way, with "hello" on a pipe,
 * and must still read all of it.
 */
TEST(PluginUnload, TheMainProgramsPinLeavesArgv0Alone)
{
#if !defined(IOV2_SHARED)
    GTEST_SKIP() << "the pin exists only in shared-library mode";
#else
    if (in_test_child())
    {
        std::string input;
        char buf[64];
        for (ssize_t n; (n = ::read(STDIN_FILENO, buf, sizeof buf)) > 0; )
            input.append(buf, static_cast<std::size_t>(n));
        std::_Exit(input == "hello" ? 0 : 1);
    }

    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const std::string executable = exe_path();
    const pid_t child = ::fork();
    ASSERT_NE(child, -1);
    if (child == 0)
    {
        ::dup2(fds[0], STDIN_FILENO);
        ::close(fds[0]);
        ::close(fds[1]);
        ::setenv(test_child_env, "1", 1);
        ::execl(executable.c_str(), "/dev/stdin",
                "--gtest_filter=PluginUnload.TheMainProgramsPinLeavesArgv0Alone",
                "--gtest_color=no", static_cast<char*>(nullptr));
        ::_exit(127);
    }
    ::close(fds[0]);
    EXPECT_EQ(::write(fds[1], "hello", 5), 5);
    ::close(fds[1]);

    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
#endif
}

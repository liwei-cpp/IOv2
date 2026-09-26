// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// A stateful LC_CTYPE for tests, and a child process to run checks under it.
//
// glibc ships no stateful locale, so one is built: a hand-written charmap that
// names the ISO-2022-JP codeset (glibc's gconv module does the actual encoding,
// so ASCII entries are all the charmap needs), compiled by localedef into the
// suite's working directory. A locale outside the system paths can only be
// reached through LOCPATH, which changes how every newlocale in the process
// resolves names, and glibc leaks a little memory on each lookup made through
// it. So the checks run in a child process that has LOCPATH set, no locale
// variables, and LeakSanitizer turned off.
//
// Usage, in the test body:
//     if (!in_stateful_child()) { EXPECT_EQ(run_stateful_child("Suite.Name"), 0); return; }
//     ... checks under stateful_locale_name ...

#include <support/exe_path.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    inline const char* const stateful_child_env = "IOV2_STATEFUL_ENCODING_CHILD";
    inline const char* const stateful_locale_name = "xx_XX.ISO-2022-JP";

    inline bool in_stateful_child() { return std::getenv(stateful_child_env) != nullptr; }

    inline std::filesystem::path stateful_resource(const char* leaf)
    {
        std::filesystem::path p = exe_path();
        p = p.remove_filename() / ".." / "IOv2TestResources" / "iso2022jp" / leaf;
        return std::filesystem::canonical(p);
    }

    // localedef exits 1 with -c when it only warned (the source leaves six
    // categories undefined), so success is read from the output, not the status.
    inline bool build_stateful_locale(const std::filesystem::path& locpath)
    {
        const std::filesystem::path out = locpath / stateful_locale_name;
        std::filesystem::remove_all(out);
        std::filesystem::create_directories(locpath);
        const std::string charmap = stateful_resource("ISO-2022-JP.cm").string();
        const std::string source = stateful_resource("mini.src").string();

        const pid_t child = ::fork();
        if (child == -1)
            return false;
        if (child == 0)
        {
            const int null_fd = ::open("/dev/null", O_WRONLY);
            ::dup2(null_fd, STDOUT_FILENO);
            ::dup2(null_fd, STDERR_FILENO);
            ::setenv("LC_ALL", "C", 1);
            ::execlp("localedef", "localedef", "-c", "-f", charmap.c_str(),
                     "-i", source.c_str(), out.c_str(), static_cast<char*>(nullptr));
            ::_exit(127);
        }

        int status = 0;
        if (::waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) > 1)
            return false;
        return std::filesystem::exists(out / "LC_CTYPE");
    }

    // Builds the locale, then re-runs this executable on `test` (a full
    // "Suite.Name") under it. Returns the child's exit status, or -1.
    inline int run_stateful_child(const std::string& test)
    {
        const std::filesystem::path locpath = std::filesystem::current_path() / "stateful-locales";
        if (!build_stateful_locale(locpath))
            return -1;

        const std::string filter = "--gtest_filter=" + test;
        const std::string executable = exe_path();
        const pid_t child = ::fork();
        if (child == -1)
            return -1;

        if (child == 0)
        {
            for (const char* name : {"LC_ALL", "LC_CTYPE", "LC_COLLATE", "LC_MONETARY",
                                     "LC_NUMERIC", "LC_TIME", "LC_MESSAGES", "LANG"})
                ::unsetenv(name);
            ::setenv("LOCPATH", locpath.c_str(), 1);
            std::string asan = "detect_leaks=0";
            if (const char* old = std::getenv("ASAN_OPTIONS"); old != nullptr && *old != '\0')
                asan = std::string(old) + ":" + asan;
            ::setenv("ASAN_OPTIONS", asan.c_str(), 1);
            ::setenv(stateful_child_env, "1", 1);
            ::execl(executable.c_str(), executable.c_str(),
                    filter.c_str(), "--gtest_color=no", static_cast<char*>(nullptr));
            ::_exit(127);
        }

        int status = 0;
        if (::waitpid(child, &status, 0) != child || !WIFEXITED(status))
            return -1;
        return WEXITSTATUS(status);
    }
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A write to stderr that stops partway -- here at a 5-byte file size limit -- and
 * the recovery the out_impl.h header recommends (clear(), then flush()) leave
 * `0123456789` on fd 2 exactly once. The accepted prefix used to go out a second
 * time: `012340123456789`, with the stream good afterwards.
 *
 * Each case runs in a child process: the file size limit applies to the whole
 * process, and fd 2 is pointed at a file for the run.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <csignal>
#include <fstream>
#include <iterator>
#include <string>

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    const char* const kFile = "io_objects_partial_write";

    // Writes `text` through `s` under a 5-byte limit, then lifts the limit and recovers.
    // Returns 0 when the write failed and the recovery left the stream good.
    template <typename S, typename Text>
    int write_through_a_limit(S& s, const Text& text)
    {
        std::signal(SIGXFSZ, SIG_IGN);
        rlimit old{};
        ::getrlimit(RLIMIT_FSIZE, &old);
        rlimit small = old;
        small.rlim_cur = 5;

        ::setrlimit(RLIMIT_FSIZE, &small);
        s << text;
        s.flush();
        const bool failed = !s.good();
        ::setrlimit(RLIMIT_FSIZE, &old);

        s.clear();
        s.flush();
        if (!failed)
            return 1;
        return s.good() ? 0 : 2;
    }

    // Runs `body` in a child whose fd 2 is a fresh file, and returns what the file holds.
    template <typename F>
    std::string stderr_of_child(F body, int& exit_code)
    {
        const pid_t child = ::fork();
        if (child == 0)
        {
            const int fd = ::open(kFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0 || ::dup2(fd, STDERR_FILENO) < 0)
                ::_exit(100);
            ::close(fd);
            ::_exit(body());
        }
        int status = 0;
        exit_code = -1;
        if (child > 0 && ::waitpid(child, &status, 0) == child && WIFEXITED(status))
            exit_code = WEXITSTATUS(status);

        std::ifstream in(kFile, std::ios::binary);
        std::string got{std::istreambuf_iterator<char>(in), {}};
        in.close();
        ::unlink(kFile);
        return got;
    }
}

TEST(IoObjectsPartialWrite, ClogResumesExactlyOnce)
{
    for (const bool sync : {true, false})
    {
        SCOPED_TRACE(sync);
        int code = -1;
        const std::string got = stderr_of_child([sync] {
            IOv2::clog.sync_with_stdio(sync);
            return write_through_a_limit(IOv2::clog, "0123456789");
        }, code);
        EXPECT_EQ(code, 0) << "1: the write did not fail, 2: the recovery did not leave the stream good";
        EXPECT_EQ(got, "0123456789");
    }
}

TEST(IoObjectsPartialWrite, CerrResumesExactlyOnce)
{
    for (const bool sync : {true, false})
    {
        SCOPED_TRACE(sync);
        int code = -1;
        const std::string got = stderr_of_child([sync] {
            IOv2::cerr.sync_with_stdio(sync);
            return write_through_a_limit(IOv2::cerr, "0123456789");
        }, code);
        EXPECT_EQ(code, 0) << "1: the write did not fail, 2: the recovery did not leave the stream good";
        EXPECT_EQ(got, "0123456789");
    }
}

TEST(IoObjectsPartialWrite, WcerrResumesExactlyOnce)
{
    for (const bool sync : {true, false})
    {
        SCOPED_TRACE(sync);
        int code = -1;
        const std::string got = stderr_of_child([sync] {
            IOv2::wcerr.sync_with_stdio(sync);
            return write_through_a_limit(IOv2::wcerr, L"0123456789");
        }, code);
        EXPECT_EQ(code, 0) << "1: the write did not fail, 2: the recovery did not leave the stream good";
        EXPECT_EQ(got, "0123456789");
    }
}

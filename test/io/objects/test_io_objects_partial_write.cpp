// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A write to stderr that stops partway -- here at a 5-byte file size limit -- and
 * the recovery the out_impl.h header recommends (clear(), then flush()) leave
 * `0123456789` on fd 2 exactly once. The accepted prefix used to go out a second
 * time: `012340123456789`, with the stream good afterwards.
 *
 * Each case runs in a child process (support/test_child.h): the file size limit
 * applies to the whole process, and fd 2 is pointed at a file for the run.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/test_child.h>

#include <gtest/gtest.h>

#include <csignal>
#include <fstream>
#include <iterator>
#include <string>

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

namespace
{
    const char* const kFile = "io_objects_partial_write";

    // In the child: points fd 2 at kFile, then writes `text` through `s` (in the mode the
    // parent asked for) under a 5-byte limit, lifts the limit and recovers.
    template <typename S, typename Text>
    void write_through_a_limit(S& s, const Text& text)
    {
        const int fd = ::open(kFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        ASSERT_GE(fd, 0);
        ASSERT_GE(::dup2(fd, STDERR_FILENO), 0);
        ::close(fd);
        s.sync_with_stdio(test_child_arg() == "sync");

        std::signal(SIGXFSZ, SIG_IGN);
        rlimit old{};
        ::getrlimit(RLIMIT_FSIZE, &old);
        rlimit small = old;
        small.rlim_cur = 5;

        ::setrlimit(RLIMIT_FSIZE, &small);
        s << text;
        s.flush();
        EXPECT_FALSE(s.good()) << "the write did not fail";
        ::setrlimit(RLIMIT_FSIZE, &old);

        s.clear();
        s.flush();
        EXPECT_TRUE(s.good()) << "the recovery did not leave the stream good";
    }

    // In the parent: runs `test` in a child once per mode and checks what fd 2 got.
    void expect_exactly_once(const char* test)
    {
        for (const char* mode : {"sync", "nosync"})
        {
            SCOPED_TRACE(mode);
            EXPECT_EQ(run_test_child(test, mode), 0) << "the checks in the child failed; see its output above";
            std::ifstream in(kFile, std::ios::binary);
            const std::string got{std::istreambuf_iterator<char>(in), {}};
            in.close();
            ::unlink(kFile);
            EXPECT_EQ(got, "0123456789");
        }
    }
}

TEST(IoObjectsPartialWrite, ClogResumesExactlyOnce)
{
    if (in_test_child())
        return write_through_a_limit(IOv2::clog, "0123456789");
    expect_exactly_once("IoObjectsPartialWrite.ClogResumesExactlyOnce");
}

TEST(IoObjectsPartialWrite, CerrResumesExactlyOnce)
{
    if (in_test_child())
        return write_through_a_limit(IOv2::cerr, "0123456789");
    expect_exactly_once("IoObjectsPartialWrite.CerrResumesExactlyOnce");
}

TEST(IoObjectsPartialWrite, WcerrResumesExactlyOnce)
{
    if (in_test_child())
        return write_through_a_limit(IOv2::wcerr, L"0123456789");
    expect_exactly_once("IoObjectsPartialWrite.WcerrResumesExactlyOnce");
}

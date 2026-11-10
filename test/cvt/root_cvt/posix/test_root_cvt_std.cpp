// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * root_cvt over a standard stream that passes every byte through unchanged. On
 * Windows the CRT opens the standard streams in text mode, where \n is written
 * as \r\n and Ctrl+Z ends the input (see device/std_device/windows).
 */
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/std_device.h>

#include <support/platform.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>

using namespace IOv2;

namespace
{
    namespace sys = iov2_test::sys;

    constexpr std::size_t kSize = 4102;

    // 586 repetitions of the UTF-8 for U'李' U'伟' plus one byte cycling 1..127.
    // The point of the sample here is only its length and that 4102 is not a
    // multiple of any chunk size below, so the loops cross the root converter's
    // own buffer boundary at every offset.
    std::string sample()
    {
        std::string out;
        out.resize(kSize);
        for (std::size_t i = 0; i < kSize; i += 7)
        {
            out[i + 0] = '\xE6';
            out[i + 1] = '\x9D';
            out[i + 2] = '\x8E';
            out[i + 3] = '\xE4';
            out[i + 4] = '\xBC';
            out[i + 5] = '\x9F';
            out[i + 6] = (i / 7) % 127 + 1;
        }
        return out;
    }

    constexpr std::size_t kGetChunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    constexpr std::size_t kPutChunks[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

    template <typename T>
    void expect_chunked_get_reads_the_sample(T& obj, const std::string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::string out_buf(kSize, '\0');
        std::size_t total = 0;
        char*       cur   = out_buf.data();
        int         id    = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kSize - total, kGetChunks[id++]);
            auto        s = obj.get(cur, n);
            id %= std::size(kGetChunks);
            cur   += s;
            total += s;
            if (s == 0) break;
        }

        EXPECT_EQ(total, kSize);
        EXPECT_EQ(cur, out_buf.data() + kSize);
        EXPECT_EQ(out_buf, expected);
    }

    template <typename T>
    void put_sample_in_chunks(T& obj, std::string& plain)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        char* cur = plain.data();
        int   id  = 0;
        while (cur < plain.data() + kSize)
        {
            std::size_t n = std::min<std::size_t>(kPutChunks[id++], plain.data() + kSize - cur);
            obj.put(cur, n);
            id %= std::size(kPutChunks);
            cur += n;
        }
        EXPECT_EQ(cur, plain.data() + kSize);
        obj.flush();
    }
}

TEST(RootCvtStd, ChunkedGetReadsTheWholeSample)
{
    const std::string expected = sample();
    iguard            g(expected);
    auto              obj = rb_root_cvt{std_device<sys::stdin_fd>{}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtStd, ChunkedGetReadsTheWholeSampleThroughARuntimeCvt)
{
    const std::string expected = sample();
    iguard            g(expected);
    runtime_cvt       obj{rb_root_cvt{std_device<sys::stdin_fd>{}}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

// The same read without a read-back buffer: every character has to come straight
// off the descriptor, so nothing may be re-read or dropped between chunks.
TEST(RootCvtStd, ChunkedGetReadsTheWholeSampleWithoutAReadBuffer)
{
    const std::string expected = sample();
    iguard            g(expected);
    auto              obj = no_rb_root_cvt{std_device<sys::stdin_fd>{}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtStd, ChunkedGetReadsTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    const std::string expected = sample();
    iguard            g(expected);
    runtime_cvt       obj{no_rb_root_cvt{std_device<sys::stdin_fd>{}}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtStd, ChunkedPutReachesStdout)
{
    std::string  plain = sample();
    oguard<true> g;
    {
        auto obj = no_rb_root_cvt{std_device<sys::stdout_fd>{}};
        put_sample_in_chunks(obj, plain);
    }
    EXPECT_EQ(g.contents(), plain);
}

// stderr is unbuffered, so the same writes have to land without waiting for the
// converter to be destroyed.
TEST(RootCvtStd, ChunkedPutReachesStderr)
{
    std::string   plain = sample();
    oguard<false> g;
    auto          obj = no_rb_root_cvt{std_device<sys::stderr_fd>{}};
    put_sample_in_chunks(obj, plain);
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtStd, ChunkedPutReachesStdoutThroughARuntimeCvt)
{
    std::string  plain = sample();
    oguard<true> g;
    {
        runtime_cvt obj(no_rb_root_cvt{std_device<sys::stdout_fd>{}});
        put_sample_in_chunks(obj, plain);
    }
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtStd, ChunkedPutReachesStderrThroughARuntimeCvt)
{
    std::string   plain = sample();
    oguard<false> g;
    runtime_cvt   obj(no_rb_root_cvt{std_device<sys::stderr_fd>{}});
    put_sample_in_chunks(obj, plain);
    EXPECT_EQ(g.contents(), plain);
}

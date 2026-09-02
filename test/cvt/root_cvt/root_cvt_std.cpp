// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/std_device.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

#include <unistd.h>

using namespace IOv2;

namespace
{
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

    // Writing "hello", handing the converter over, then writing " world" through
    // the target: the two halves may only reach the descriptor in that order, and
    // exactly once each.
    template <typename T, typename Transfer, typename Finish>
    void expect_a_move_keeps_the_output_stream(T& obj, Transfer transfer, Finish finish)
    {
        oguard<true> g;
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);

        T moved = transfer(obj);
        moved.put(" world", 6);
        finish(moved);

        EXPECT_EQ(g.contents(), "hello world");
    }

    // The read side of the same: five characters through the source, six more
    // through the target, with no character lost or repeated at the handover.
    template <typename T, typename Transfer>
    void expect_a_move_keeps_the_input_stream(T& obj, Transfer transfer)
    {
        iguard      g("hello world");
        std::string str(5, '\0');

        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();
        EXPECT_EQ(obj.get(str.data(), 5), 5u);
        EXPECT_EQ(str, "hello");

        T moved = transfer(obj);
        str.resize(6);
        EXPECT_EQ(moved.get(str.data(), 6), 6u);
        EXPECT_EQ(str, " world");
    }

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

TEST(RootCvtStd, TraitsOverStdin)
{
    using CheckType = no_rb_root_cvt<std_device<STDIN_FILENO>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, std_device<STDIN_FILENO>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    // A standard stream is one-directional and not seekable, so the direction the
    // descriptor was opened for is the only capability the converter can offer.
    static_assert(!cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtStd, TraitsOverStdout)
{
    using CheckType = rb_root_cvt<std_device<STDOUT_FILENO>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, std_device<STDOUT_FILENO>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(!cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtStd, MoveConstructionKeepsTheOutputStream)
{
    auto obj = rb_root_cvt{std_device<STDOUT_FILENO>{}};
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src) { return rb_root_cvt<std_device<STDOUT_FILENO>>{std::move(src)}; },
        [](auto& dst) { dst.detach(); });
}

TEST(RootCvtStd, MoveConstructionKeepsTheOutputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<STDOUT_FILENO>{}});
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src) { return runtime_cvt{std::move(src)}; },
        [](auto& dst) { dst.detach(); });
}

// Same handover through assignment, and finished with attach() rather than
// detach(): attaching a fresh device has to flush what the old one still held.
TEST(RootCvtStd, MoveAssignmentKeepsTheOutputStream)
{
    auto obj = rb_root_cvt{std_device<STDOUT_FILENO>{}};
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src)
        {
            rb_root_cvt<std_device<STDOUT_FILENO>> dst{std_device<STDOUT_FILENO>{}};
            dst = std::move(src);
            return dst;
        },
        [](auto& dst) { dst.attach(std_device<STDOUT_FILENO>{}); });
}

TEST(RootCvtStd, MoveAssignmentKeepsTheOutputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<STDOUT_FILENO>{}});
    expect_a_move_keeps_the_output_stream(
        obj,
        [](auto& src)
        {
            runtime_cvt dst{rb_root_cvt{std_device<STDOUT_FILENO>{}}};
            dst = std::move(src);
            return dst;
        },
        [](auto& dst) { dst.attach(std_device<STDOUT_FILENO>{}); });
}

TEST(RootCvtStd, MoveConstructionKeepsTheInputStream)
{
    auto obj = rb_root_cvt{std_device<STDIN_FILENO>{}};
    expect_a_move_keeps_the_input_stream(
        obj, [](auto& src) { return rb_root_cvt<std_device<STDIN_FILENO>>{std::move(src)}; });
}

TEST(RootCvtStd, MoveConstructionKeepsTheInputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<STDIN_FILENO>{}});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(RootCvtStd, MoveAssignmentKeepsTheInputStream)
{
    auto obj = rb_root_cvt{std_device<STDIN_FILENO>{}};
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        rb_root_cvt<std_device<STDIN_FILENO>> dst{std_device<STDIN_FILENO>{}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtStd, MoveAssignmentKeepsTheInputStreamThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{std_device<STDIN_FILENO>{}});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{std_device<STDIN_FILENO>{}}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtStd, ChunkedGetReadsTheWholeSample)
{
    const std::string expected = sample();
    iguard            g(expected);
    auto              obj = rb_root_cvt{std_device<STDIN_FILENO>{}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtStd, ChunkedGetReadsTheWholeSampleThroughARuntimeCvt)
{
    const std::string expected = sample();
    iguard            g(expected);
    runtime_cvt       obj{rb_root_cvt{std_device<STDIN_FILENO>{}}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

// The same read without a read-back buffer: every character has to come straight
// off the descriptor, so nothing may be re-read or dropped between chunks.
TEST(RootCvtStd, ChunkedGetReadsTheWholeSampleWithoutAReadBuffer)
{
    const std::string expected = sample();
    iguard            g(expected);
    auto              obj = no_rb_root_cvt{std_device<STDIN_FILENO>{}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtStd, ChunkedGetReadsTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    const std::string expected = sample();
    iguard            g(expected);
    runtime_cvt       obj{no_rb_root_cvt{std_device<STDIN_FILENO>{}}};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtStd, ChunkedPutReachesStdout)
{
    std::string  plain = sample();
    oguard<true> g;
    {
        auto obj = no_rb_root_cvt{std_device<STDOUT_FILENO>{}};
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
    auto          obj = no_rb_root_cvt{std_device<STDERR_FILENO>{}};
    put_sample_in_chunks(obj, plain);
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtStd, ChunkedPutReachesStdoutThroughARuntimeCvt)
{
    std::string  plain = sample();
    oguard<true> g;
    {
        runtime_cvt obj(no_rb_root_cvt{std_device<STDOUT_FILENO>{}});
        put_sample_in_chunks(obj, plain);
    }
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtStd, ChunkedPutReachesStderrThroughARuntimeCvt)
{
    std::string   plain = sample();
    oguard<false> g;
    runtime_cvt   obj(no_rb_root_cvt{std_device<STDERR_FILENO>{}});
    put_sample_in_chunks(obj, plain);
    EXPECT_EQ(g.contents(), plain);
}

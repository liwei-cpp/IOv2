// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/file_device.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    using RWDev  = basic_file_device<true, true, char>;
    using RODev  = basic_file_device<true, false, char>;
    using WODev  = basic_file_device<false, true, char>;

    // Minimal write-only device that can be told to throw on the next dput().
    // Used to exercise the catch(...) blocks in move-assignment and destructor.
    struct throw_write_device
    {
        using char_type = char;
        bool should_throw = false;

        void dput(const char_type*, std::size_t)
        {
            if (should_throw) throw device_error("forced throw");
        }
        void dflush() {}
    };

    constexpr std::size_t kSize = 4102;

    // 586 repetitions of the UTF-8 for U'李' U'伟' plus one byte cycling 1..127.
    // What matters here is the length: 4102 is not a multiple of any chunk size
    // below, so the loops cross the root converter's buffer boundary at every
    // offset.
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

    // A converter handed over mid-stream has to finish the write through the
    // target. The source is left with nothing, so anything that still reaches the
    // file after the handover came from the target.
    template <typename T, typename Transfer>
    void expect_a_move_keeps_the_output_stream(T& obj, Transfer transfer)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        T moved = transfer(obj);
        moved.put(" world", 6);
    }

    // The read side: five characters through the source, six more through the
    // target, with the position carried across and nothing re-read.
    template <typename T, typename Transfer>
    void expect_a_move_keeps_the_input_stream(T& obj, Transfer transfer)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::string str(5, '\0');
        EXPECT_EQ(obj.get(str.data(), 5), 5u);
        EXPECT_EQ(str, "hello");
        EXPECT_EQ(obj.tell(), 5u);

        T moved = transfer(obj);
        EXPECT_EQ(moved.tell(), 5u);
        str.resize(6);
        EXPECT_EQ(moved.get(str.data(), 6), 6u);
        EXPECT_EQ(str, " world");
        EXPECT_EQ(moved.tell(), 11u);
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
    void put_sample_in_chunks(T& obj, const std::string& plain)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        const char* cur = plain.data();
        int         id  = 0;
        while (cur < plain.data() + kSize)
        {
            std::size_t n = std::min<std::size_t>(kPutChunks[id++], plain.data() + kSize - cur);
            obj.put(cur, n);
            id %= std::size(kPutChunks);
            cur += n;
        }
        EXPECT_EQ(cur, plain.data() + kSize);
    }

    // seek() takes an absolute position, rseek() counts back from the end of the
    // five-character file, so rseek(3) lands on index 2.
    template <typename T>
    void expect_seek_and_rseek_land_where_asked(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        obj.seek(3);
        EXPECT_EQ(obj.tell(), 3u);

        char ch = 0;
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, '4');

        obj.rseek(3);
        EXPECT_EQ(obj.tell(), 2u);
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, '3');
    }

    // A second main_cont_beg() moves the origin: what was read before it becomes a
    // prologue, tell() restarts at 0, and an rseek that would land before the
    // origin is refused without moving the position.
    template <typename T>
    void expect_positions_are_relative_to_the_main_content(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        char c = 0;
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, '1');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, '2');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, '3');

        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        obj.seek(3);
        EXPECT_EQ(obj.tell(), 3u);

        char ch = 0;
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, 'd');

        obj.rseek(3);
        EXPECT_EQ(obj.tell(), 4u);
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, 'e');

        EXPECT_ANY_THROW(obj.rseek(60));
        EXPECT_EQ(obj.tell(), 5u);

        EXPECT_ANY_THROW(obj.rseek(9));
        EXPECT_EQ(obj.tell(), 5u);
    }
}

TEST(RootCvtFile, TraitsOverAReadWriteFile)
{
    using CheckType = no_rb_root_cvt<basic_file_device<true, true, char>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, basic_file_device<true, true, char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtFile, TraitsOverAReadWriteChar8File)
{
    using CheckType = rb_root_cvt<basic_file_device<true, true, char8_t>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, basic_file_device<true, true, char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

// A one-directional file is still seekable, so positioning survives while the
// direction the file was not opened for -- and with it the ability to turn
// around -- does not.
TEST(RootCvtFile, TraitsOverAReadOnlyFile)
{
    using CheckType = no_rb_root_cvt<basic_file_device<true, false, char>>;
    static_assert(!cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtFile, TraitsOverAWriteOnlyFile)
{
    using CheckType = rb_root_cvt<basic_file_device<false, true, char>>;
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(!cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

// A read-write device opened on an existing file and rewound to the end appends,
// so "hello" survives and " world" follows it.
TEST(RootCvtFile, MoveConstructionKeepsTheOutputStreamOnAReadWriteFile)
{
    file_guard g("test_file", "hello");
    RWDev      dev("test_file");
    dev.drseek(0);
    auto obj = rb_root_cvt{std::move(dev)};
    expect_a_move_keeps_the_output_stream(obj, [](auto& src)
    { return rb_root_cvt<RWDev>{std::move(src)}; });
    EXPECT_EQ(g.contents(), "hello world");
}

// A write-only device truncates on open, so the same sequence leaves the file
// holding only what was written after the handover.
TEST(RootCvtFile, MoveConstructionKeepsTheOutputStreamOnAWriteOnlyFile)
{
    file_guard g("test_file", "hello");
    WODev      dev("test_file");
    dev.drseek(0);
    auto obj = rb_root_cvt{std::move(dev)};
    expect_a_move_keeps_the_output_stream(obj, [](auto& src)
    { return rb_root_cvt<WODev>{std::move(src)}; });
    EXPECT_EQ(g.contents(), " world");
}

TEST(RootCvtFile, MoveConstructionKeepsTheOutputStreamOnAReadWriteFileThroughARuntimeCvt)
{
    file_guard g("test_file", "hello");
    RWDev      dev("test_file");
    dev.drseek(0);
    runtime_cvt obj(rb_root_cvt{std::move(dev)});
    expect_a_move_keeps_the_output_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
    EXPECT_EQ(g.contents(), "hello world");
}

TEST(RootCvtFile, MoveConstructionKeepsTheOutputStreamOnAWriteOnlyFileThroughARuntimeCvt)
{
    file_guard g("test_file", "hello");
    WODev      dev("test_file");
    dev.drseek(0);
    runtime_cvt obj(rb_root_cvt{std::move(dev)});
    expect_a_move_keeps_the_output_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
    EXPECT_EQ(g.contents(), " world");
}

TEST(RootCvtFile, MoveAssignmentKeepsTheOutputStreamOnAReadWriteFile)
{
    file_guard g("test_file", "hello");
    RWDev      dev("test_file");
    dev.drseek(0);
    auto obj = rb_root_cvt{std::move(dev)};
    expect_a_move_keeps_the_output_stream(obj, [](auto& src)
    {
        rb_root_cvt<RWDev> dst{RWDev{}};
        dst = std::move(src);
        return dst;
    });
    EXPECT_EQ(g.contents(), "hello world");
}

TEST(RootCvtFile, MoveAssignmentKeepsTheOutputStreamOnAWriteOnlyFile)
{
    file_guard g("test_file", "hello");
    WODev      dev("test_file");
    dev.drseek(0);
    auto obj = rb_root_cvt{std::move(dev)};
    expect_a_move_keeps_the_output_stream(obj, [](auto& src)
    {
        rb_root_cvt<WODev> dst{WODev{}};
        dst = std::move(src);
        return dst;
    });
    EXPECT_EQ(g.contents(), " world");
}

TEST(RootCvtFile, MoveAssignmentKeepsTheOutputStreamOnAReadWriteFileThroughARuntimeCvt)
{
    file_guard g("test_file", "hello");
    RWDev      dev("test_file");
    dev.drseek(0);
    runtime_cvt obj(rb_root_cvt{std::move(dev)});
    expect_a_move_keeps_the_output_stream(obj, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{RWDev{}}};
        dst = std::move(src);
        return dst;
    });
    EXPECT_EQ(g.contents(), "hello world");
}

TEST(RootCvtFile, MoveAssignmentKeepsTheOutputStreamOnAWriteOnlyFileThroughARuntimeCvt)
{
    file_guard g("test_file", "hello");
    WODev      dev("test_file");
    dev.drseek(0);
    runtime_cvt obj(rb_root_cvt{std::move(dev)});
    expect_a_move_keeps_the_output_stream(obj, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{WODev{}}};
        dst = std::move(src);
        return dst;
    });
    EXPECT_EQ(g.contents(), " world");
}

TEST(RootCvtFile, MoveConstructionKeepsTheInputStreamOnAReadWriteFile)
{
    file_guard g("test_file", "hello world");
    auto       obj = rb_root_cvt{RWDev("test_file")};
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    { return rb_root_cvt<RWDev>{std::move(src)}; });
}

TEST(RootCvtFile, MoveConstructionKeepsTheInputStreamOnAReadOnlyFile)
{
    file_guard g("test_file", "hello world");
    auto       obj = rb_root_cvt{RODev("test_file")};
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    { return rb_root_cvt<RODev>{std::move(src)}; });
}

TEST(RootCvtFile, MoveConstructionKeepsTheInputStreamOnAReadWriteFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "hello world");
    runtime_cvt obj(rb_root_cvt{RWDev("test_file")});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(RootCvtFile, MoveConstructionKeepsTheInputStreamOnAReadOnlyFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "hello world");
    runtime_cvt obj(rb_root_cvt{RODev("test_file")});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(RootCvtFile, MoveAssignmentKeepsTheInputStreamOnAReadWriteFile)
{
    file_guard g("test_file", "hello world");
    auto       obj = rb_root_cvt{RWDev("test_file")};
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        rb_root_cvt<RWDev> dst{RWDev{}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtFile, MoveAssignmentKeepsTheInputStreamOnAReadOnlyFile)
{
    file_guard g("test_file", "hello world");
    auto       obj = rb_root_cvt{RODev("test_file")};
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        rb_root_cvt<RODev> dst{RODev{}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtFile, MoveAssignmentKeepsTheInputStreamOnAReadWriteFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "hello world");
    runtime_cvt obj(rb_root_cvt{RWDev("test_file")});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{RWDev{}}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtFile, MoveAssignmentKeepsTheInputStreamOnAReadOnlyFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "hello world");
    runtime_cvt obj(rb_root_cvt{RODev("test_file")});
    expect_a_move_keeps_the_input_stream(obj, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{RODev{}}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleOnAReadWriteFile)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    auto              obj = rb_root_cvt{RWDev("test_file")};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleOnAReadOnlyFile)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    auto              obj = rb_root_cvt{RODev("test_file")};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleOnAReadWriteFileThroughARuntimeCvt)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    runtime_cvt       obj(rb_root_cvt{RWDev("test_file")});
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleOnAReadOnlyFileThroughARuntimeCvt)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    runtime_cvt       obj(rb_root_cvt{RODev("test_file")});
    expect_chunked_get_reads_the_sample(obj, expected);
}

// The same read without a read-back buffer: every character comes straight off
// the descriptor, so nothing may be re-read or dropped between chunks.
TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleWithoutAReadBuffer)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    auto              obj = no_rb_root_cvt{RWDev("test_file")};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleWithoutAReadBufferOnAReadOnlyFile)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    auto              obj = no_rb_root_cvt{RODev("test_file")};
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    runtime_cvt       obj(no_rb_root_cvt{RWDev("test_file")});
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtFile, ChunkedGetReadsTheWholeSampleWithoutAReadBufferOnAReadOnlyFileThroughARuntimeCvt)
{
    const std::string expected = sample();
    file_guard        g("test_file", expected);
    runtime_cvt       obj(no_rb_root_cvt{RODev("test_file")});
    expect_chunked_get_reads_the_sample(obj, expected);
}

TEST(RootCvtFile, ChunkedPutWritesTheWholeSampleOnAReadWriteFile)
{
    const std::string plain = sample();
    file_guard        g("test_file", "");
    auto              obj = rb_root_cvt{RWDev("test_file", file_open_flag::trunc)};
    put_sample_in_chunks(obj, plain);
    obj.detach();
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtFile, ChunkedPutWritesTheWholeSampleOnAWriteOnlyFile)
{
    const std::string plain = sample();
    file_guard        g("test_file");
    auto              obj = rb_root_cvt{WODev("test_file")};
    put_sample_in_chunks(obj, plain);
    obj.detach();
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtFile, ChunkedPutWritesTheWholeSampleOnAReadWriteFileThroughARuntimeCvt)
{
    const std::string plain = sample();
    file_guard        g("test_file", "");
    runtime_cvt       obj(rb_root_cvt{RWDev("test_file", file_open_flag::trunc)});
    put_sample_in_chunks(obj, plain);
    obj.detach();
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtFile, ChunkedPutWritesTheWholeSampleOnAWriteOnlyFileThroughARuntimeCvt)
{
    const std::string plain = sample();
    file_guard        g("test_file");
    runtime_cvt       obj(rb_root_cvt{WODev("test_file")});
    put_sample_in_chunks(obj, plain);
    obj.detach();
    EXPECT_EQ(g.contents(), plain);
}

TEST(RootCvtFile, SeekAndRseekLandWhereAskedOnAReadWriteFile)
{
    file_guard g("test_file", "12345");
    auto       obj = rb_root_cvt{RWDev("test_file")};
    expect_seek_and_rseek_land_where_asked(obj);
}

TEST(RootCvtFile, SeekAndRseekLandWhereAskedOnAReadOnlyFile)
{
    file_guard g("test_file", "12345");
    auto       obj = rb_root_cvt{RODev("test_file")};
    expect_seek_and_rseek_land_where_asked(obj);
}

TEST(RootCvtFile, SeekAndRseekLandWhereAskedOnAReadWriteFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "12345");
    runtime_cvt obj(rb_root_cvt{RWDev("test_file")});
    expect_seek_and_rseek_land_where_asked(obj);
}

TEST(RootCvtFile, SeekAndRseekLandWhereAskedOnAReadOnlyFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "12345");
    runtime_cvt obj(rb_root_cvt{RODev("test_file")});
    expect_seek_and_rseek_land_where_asked(obj);
}

TEST(RootCvtFile, PositionsAreRelativeToTheMainContentOnAReadWriteFile)
{
    file_guard g("test_file", "123abcdefg");
    auto       obj = rb_root_cvt{RWDev("test_file")};
    expect_positions_are_relative_to_the_main_content(obj);
}

TEST(RootCvtFile, PositionsAreRelativeToTheMainContentOnAReadOnlyFile)
{
    file_guard g("test_file", "123abcdefg");
    auto       obj = rb_root_cvt{RODev("test_file")};
    expect_positions_are_relative_to_the_main_content(obj);
}

TEST(RootCvtFile, PositionsAreRelativeToTheMainContentOnAReadWriteFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "123abcdefg");
    runtime_cvt obj(rb_root_cvt{RWDev("test_file")});
    expect_positions_are_relative_to_the_main_content(obj);
}

TEST(RootCvtFile, PositionsAreRelativeToTheMainContentOnAReadOnlyFileThroughARuntimeCvt)
{
    file_guard  g("test_file", "123abcdefg");
    runtime_cvt obj(rb_root_cvt{RODev("test_file")});
    expect_positions_are_relative_to_the_main_content(obj);
}

namespace
{
    // attach() ends the session on the old file and starts one on the new: the
    // first file has to be complete on disk before the second is touched, and the
    // new session gets its own prologue and its own position origin.
    template <typename T, typename G>
    void expect_attach_switches_files(T& obj, G& g1, G& g2)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        obj.put("hello", 5);
        obj.main_cont_beg();
        obj.put(" world", 6);
        EXPECT_EQ(obj.tell(), 6u);

        obj.attach(WODev("test_file2"));
        EXPECT_EQ(g1.contents(), "hello world");

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        obj.put("liwei", 5);
        obj.main_cont_beg();
        obj.put(" cpp", 4);
        EXPECT_EQ(obj.tell(), 4u);
        obj.flush();
        obj.device().dflush();

        EXPECT_EQ(g2.contents(), "liwei cpp");
    }

    // Three sessions on one converter, with the first file detached, parked, and
    // attached again: the writes have to resume on the file they belong to.
    template <typename T>
    void expect_devices_can_be_swapped_in_and_out(T& obj)
    {
        using device_type = typename T::device_type;

        file_guard  g1("test_file1", "");
        device_type f1("test_file1", file_open_flag::trunc);
        obj.attach(std::move(f1));
        obj.bos();
        obj.main_cont_beg();
        obj.put("abc", 3);

        auto [detach1_dev, detach1_err] = obj.detach();
        f1 = std::move(detach1_dev);

        file_guard g2("test_file2", "");
        obj.attach(device_type("test_file2", file_open_flag::trunc));
        obj.bos();
        obj.main_cont_beg();
        obj.put("123", 3);

        auto [detach2_dev, detach2_err] = obj.detach();
        device_type f2 = std::move(detach2_dev);
        obj.attach(std::move(f1));
        obj.bos();
        obj.main_cont_beg();
        obj.put("def", 3);

        auto [detach3_dev, detach3_err] = obj.detach();
        f1 = std::move(detach3_dev);
        f1.close();
        f2.close();
        EXPECT_EQ(g1.contents(), "abcdef");
        EXPECT_EQ(g2.contents(), "123");
    }
}

TEST(RootCvtFile, AttachSwitchesFiles)
{
    file_guard g1("test_file1", "");
    file_guard g2("test_file2", "");
    auto       obj = rb_root_cvt{WODev("test_file1")};
    expect_attach_switches_files(obj, g1, g2);
}

TEST(RootCvtFile, AttachSwitchesFilesThroughARuntimeCvt)
{
    file_guard  g1("test_file1", "");
    file_guard  g2("test_file2", "");
    runtime_cvt obj(rb_root_cvt{WODev("test_file1")});
    expect_attach_switches_files(obj, g1, g2);
}

TEST(RootCvtFile, DevicesCanBeSwappedInAndOutOnWriteOnlyFiles)
{
    auto obj = rb_root_cvt{WODev{}};
    expect_devices_can_be_swapped_in_and_out(obj);
}

TEST(RootCvtFile, DevicesCanBeSwappedInAndOutOnWriteOnlyFilesThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{WODev{}});
    expect_devices_can_be_swapped_in_and_out(obj);
}

TEST(RootCvtFile, DevicesCanBeSwappedInAndOutOnReadWriteFiles)
{
    auto obj = rb_root_cvt{RWDev{}};
    expect_devices_can_be_swapped_in_and_out(obj);
}

TEST(RootCvtFile, DevicesCanBeSwappedInAndOutOnReadWriteFilesThroughARuntimeCvt)
{
    runtime_cvt obj(rb_root_cvt{RWDev{}});
    expect_devices_can_be_swapped_in_and_out(obj);
}

namespace
{
    // The converter reads ahead: after one character the descriptor is already at
    // the end of the file. What it hands back on detach() has to be the stream
    // position the caller saw, not the read-ahead position.
    template <typename T>
    void expect_detach_rewinds_the_read_ahead(T& obj)
    {
        obj.bos();
        obj.main_cont_beg();

        char ch = 0;
        obj.get(&ch, 1);
        EXPECT_EQ(ch, '1');
        EXPECT_EQ(obj.device().dtell(), 8u);

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.dtell(), 1u);
    }

    // The write side of the same: the bytes are still in the converter's buffer,
    // so the descriptor has not moved until detach() flushes them.
    template <typename T>
    void expect_detach_flushes_the_write_buffer(T& obj)
    {
        obj.bos();
        obj.main_cont_beg();

        obj.put("123", 3);
        EXPECT_EQ(obj.device().dtell(), 0u);

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.dtell(), 3u);
    }
}

TEST(RootCvtFile, DetachRewindsTheReadAhead)
{
    file_guard g("test_file1", "12345678");
    auto       obj = rb_root_cvt{ifile_device<char>("test_file1")};
    expect_detach_rewinds_the_read_ahead(obj);
}

TEST(RootCvtFile, DetachRewindsTheReadAheadThroughARuntimeCvt)
{
    file_guard  g("test_file1", "12345678");
    runtime_cvt obj(rb_root_cvt{ifile_device<char>("test_file1")});
    expect_detach_rewinds_the_read_ahead(obj);
}

TEST(RootCvtFile, DetachFlushesTheWriteBuffer)
{
    file_guard g("test_file1", "");
    auto       obj = rb_root_cvt{ofile_device<char>("test_file1")};
    expect_detach_flushes_the_write_buffer(obj);
}

TEST(RootCvtFile, DetachFlushesTheWriteBufferThroughARuntimeCvt)
{
    file_guard  g("test_file1", "");
    runtime_cvt obj(rb_root_cvt{ofile_device<char>("test_file1")});
    expect_detach_flushes_the_write_buffer(obj);
}

// Attaching a new file afterwards must not reach back into the one already
// handed out.
TEST(RootCvtFile, AttachAfterDetachLeavesTheOldReadDeviceAlone)
{
    file_guard g("test_file1", "12345678");
    auto       obj = rb_root_cvt{ifile_device<char>("test_file1")};
    obj.bos();
    obj.main_cont_beg();

    char ch = 0;
    obj.get(&ch, 1);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.device().dtell(), 8u);

    file_guard g2("test_file2", "abcde");
    auto [dev, err] = obj.detach();
    obj.attach(ifile_device<char>("test_file2"));
    EXPECT_EQ(dev.dtell(), 1u);
}

TEST(RootCvtFile, AttachAfterDetachLeavesTheOldReadDeviceAloneThroughARuntimeCvt)
{
    file_guard  g("test_file1", "12345678");
    runtime_cvt obj(rb_root_cvt{ifile_device<char>("test_file1")});
    obj.bos();
    obj.main_cont_beg();

    char ch = 0;
    obj.get(&ch, 1);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.device().dtell(), 8u);

    file_guard g2("test_file2", "abcde");
    auto [dev, err] = obj.detach();
    obj.attach(ifile_device<char>("test_file2"));
    EXPECT_EQ(dev.dtell(), 1u);
}

TEST(RootCvtFile, AttachAfterDetachLeavesTheOldWriteDeviceAlone)
{
    file_guard g("test_file1", "");
    auto       obj = rb_root_cvt{ofile_device<char>("test_file1")};
    obj.bos();
    obj.main_cont_beg();

    obj.put("123", 3);
    EXPECT_EQ(obj.device().dtell(), 0u);

    file_guard g2("test_file2", "");
    auto [dev, err] = obj.detach();
    obj.attach(ofile_device<char>("test_file2"));
    EXPECT_EQ(dev.dtell(), 3u);
}

TEST(RootCvtFile, AttachAfterDetachLeavesTheOldWriteDeviceAloneThroughARuntimeCvt)
{
    file_guard  g("test_file1", "");
    runtime_cvt obj(rb_root_cvt{ofile_device<char>("test_file1")});
    obj.bos();
    obj.main_cont_beg();

    obj.put("123", 3);
    EXPECT_EQ(obj.device().dtell(), 0u);

    file_guard g2("test_file2", "");
    auto [dev, err] = obj.detach();
    obj.attach(ofile_device<char>("test_file2"));
    EXPECT_EQ(dev.dtell(), 3u);
}

// is_eof() has to answer from whichever of the converter's own buffers is live,
// and only fall through to the device when neither can answer.
TEST(RootCvtFile, IsEofIsFalseWhileTheReadBufferStillHasData)
{
    file_guard g("test_file1", "hello world");
    auto       obj = rb_root_cvt{RWDev("test_file1")};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char ch = 0;
    obj.get(&ch, 1); // the whole file is now buffered, one character consumed
    EXPECT_FALSE(obj.is_eof());
}

TEST(RootCvtFile, IsEofAsksTheDeviceWhenThereIsNoReadBuffer)
{
    file_guard g("test_file2", "hi");
    auto       obj = no_rb_root_cvt{RWDev("test_file2")};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char buf[2];
    obj.get(buf, 2); // read straight from the device, which is now at the end
    EXPECT_TRUE(obj.is_eof());
}

// In output mode the pending bytes have to reach the file before the question can
// be answered, or the device would report a position the caller never wrote to.
TEST(RootCvtFile, IsEofFlushesAPendingWriteFirst)
{
    file_guard g("test_file3", "");
    auto       obj = rb_root_cvt{RWDev("test_file3", file_open_flag::trunc)};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    obj.put("hello", 5); // still in the write buffer
    EXPECT_TRUE(obj.is_eof());
}

TEST(RootCvtFile, IsEofSkipsTheFlushWhenNothingIsPending)
{
    file_guard g("test_file4", "");
    auto       obj = rb_root_cvt{RWDev("test_file4", file_open_flag::trunc)};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    EXPECT_TRUE(obj.is_eof());
}

// Before bos() the converter has no direction, so there is no buffer to consult
// and no meaningful answer to give.
TEST(RootCvtFile, IsEofIsRejectedBeforeBos)
{
    file_guard g("test_file5", "hello");
    auto       obj = rb_root_cvt{RWDev("test_file5")};
    EXPECT_THROW((void)obj.is_eof(), cvt_error);
}

// get() and put() settle the direction themselves when bos() has not been called,
// which is the default branch of the two switch helpers.
TEST(RootCvtFile, AGetBeforeBosSwitchesToInput)
{
    file_guard g("test_file", "hello");
    auto       obj = rb_root_cvt{RWDev("test_file")};

    char ch = 0;
    EXPECT_EQ(obj.get(&ch, 1), 1u);
    EXPECT_EQ(ch, 'h');
}

TEST(RootCvtFile, APutBeforeBosSwitchesToOutput)
{
    file_guard g("test_file", "");
    auto       obj = rb_root_cvt{RWDev("test_file", file_open_flag::trunc)};

    obj.put("X", 1);
    obj.detach();
    EXPECT_EQ(g.contents(), "X");
}

// Turning from output to input has to flush first: the characters just written
// are part of the file the read is about to see, and the descriptor is left at
// the end of them.
TEST(RootCvtFile, SwitchingToInputFlushesTheWriteBuffer)
{
    file_guard g("test_file", "");
    auto       obj = rb_root_cvt{RWDev("test_file", file_open_flag::trunc)};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    obj.put("ABC", 3);

    char ch = 0;
    EXPECT_EQ(obj.get(&ch, 1), 0u); // the descriptor is at the end of "ABC"
    obj.detach();
    EXPECT_EQ(g.contents(), "ABC");
}

// Turning from input to output with nothing left in the read buffer needs no
// repositioning: the descriptor is already where the caller thinks it is.
TEST(RootCvtFile, SwitchingToOutputNeedsNoSeekWhenNothingWasReadAhead)
{
    file_guard g("test_file", "hello");
    auto       obj = no_rb_root_cvt{RWDev("test_file")};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char buf[5];
    obj.get(buf, 5); // read straight from the device, nothing buffered
    obj.put("!", 1);
    obj.detach();
    EXPECT_EQ(g.contents(), "hello!");
}

// With characters still in the read buffer the descriptor has run ahead of the
// caller, so the switch has to seek back to the position the caller is at --
// otherwise the write would land after the read-ahead.
TEST(RootCvtFile, SwitchingToOutputSeeksBackOverTheReadAhead)
{
    file_guard g("test_file", "hello world");
    auto       obj = rb_root_cvt{RWDev("test_file")};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char ch = 0;
    obj.get(&ch, 1); // buffers all 11 characters, consumes one
    EXPECT_EQ(ch, 'h');
    EXPECT_EQ(obj.tell(), 1u);

    obj.put("X", 1);
    obj.detach();
    EXPECT_EQ(g.contents(), "hXllo world");
}

// tell() has no answer before the direction is settled.
TEST(RootCvtFile, TellIsRejectedBeforeBos)
{
    file_guard g("test_file", "hello");
    auto       obj = rb_root_cvt{RWDev("test_file")};
    EXPECT_THROW((void)obj.tell(), cvt_error);
}

// On a one-directional device the switch helper for the other direction is not
// even compiled, so the neutral state has to be caught by the status check.
TEST(RootCvtFile, GetBeforeBosIsRejectedOnAReadOnlyFile)
{
    file_guard g("test_file", "hello");
    auto       obj = rb_root_cvt{ifile_device<char>("test_file")};

    char ch = 0;
    EXPECT_THROW((void)obj.get(&ch, 1), cvt_error);
}

TEST(RootCvtFile, PutBeforeBosIsRejectedOnAWriteOnlyFile)
{
    file_guard g("test_file", "");
    auto       obj = rb_root_cvt{ofile_device<char>("test_file")};
    EXPECT_THROW(obj.put("hi", 2), cvt_error);
}

namespace
{
    // seek() adds the position to the length of the prologue before handing it to
    // the device. With a one-character prologue, SIZE_MAX would wrap, so the
    // overflow has to be caught before the addition rather than after.
    template <typename T>
    void expect_seek_rejects_an_overflowing_position(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        char ch = 0;
        obj.get(&ch, 1);     // buffers five characters, consumes one
        obj.main_cont_beg(); // so the prologue is one character long

        EXPECT_THROW(obj.seek(std::numeric_limits<std::size_t>::max()), cvt_error);
    }
}

TEST(RootCvtFile, SeekRejectsAnOverflowingPosition)
{
    file_guard g("test_file", "hello");
    auto       obj = rb_root_cvt{RWDev("test_file")};
    expect_seek_rejects_an_overflowing_position(obj);
}

TEST(RootCvtFile, SeekRejectsAnOverflowingPositionThroughARuntimeCvt)
{
    file_guard  g("test_file", "hello");
    runtime_cvt obj(rb_root_cvt{RWDev("test_file")});
    expect_seek_rejects_an_overflowing_position(obj);
}

// adjust() and retrieve() have nothing to report for a plain file, but they are
// part of the converter interface and have to be callable.
TEST(RootCvtFile, AdjustIsANoop)
{
    file_guard g("test_file", "hello");
    auto       obj = rb_root_cvt{ifile_device<char>("test_file")};
    obj.bos();
    obj.main_cont_beg();

    cvt_behavior beh;
    EXPECT_NO_THROW(obj.adjust(beh));
}

TEST(RootCvtFile, RetrieveIsANoop)
{
    file_guard g("test_file", "hello");
    auto       obj = rb_root_cvt{ifile_device<char>("test_file")};
    obj.bos();
    obj.main_cont_beg();

    cvt_status stat;
    EXPECT_NO_THROW(obj.retrieve(stat));
}

// A write that exactly fills the buffer must not be treated as an overflow, and
// one that arrives on top of a partly-filled buffer takes the bypass that writes
// straight through -- both have to end up with the same bytes in the same order.
TEST(RootCvtFile, AWriteThatExactlyFillsTheBufferIsWrittenWhole)
{
    constexpr std::size_t kBuf = rb_root_cvt<ofile_device<char>>::s_buffer_length;

    file_guard  g("test_file", "");
    std::string data(kBuf, 'x');
    {
        auto obj = rb_root_cvt{ofile_device<char>("test_file")};
        obj.bos();
        obj.main_cont_beg();
        obj.put(data.data(), kBuf);
    }
    EXPECT_EQ(g.contents(), data);
}

TEST(RootCvtFile, ALargeWriteOnTopOfAPartialBufferKeepsTheOrder)
{
    constexpr std::size_t kBuf = rb_root_cvt<ofile_device<char>>::s_buffer_length;

    file_guard  g("test_file", "");
    std::string small(100, 'a');
    std::string large(kBuf, 'b');
    {
        auto obj = rb_root_cvt{ofile_device<char>("test_file")};
        obj.bos();
        obj.main_cont_beg();
        obj.put(small.data(), small.size());
        obj.put(large.data(), large.size());
    }
    EXPECT_EQ(g.contents(), small + large);
}

// Repositioning in output mode has to flush first, or the buffered bytes would
// later be written at the new position instead of the one they were put at.
TEST(RootCvtFile, SeekInOutputModeFlushesFirst)
{
    file_guard g("test_file", "");
    auto       obj = rb_root_cvt{ofile_device<char>("test_file")};
    obj.bos();
    obj.main_cont_beg();
    obj.put("hello", 5);
    obj.seek(0);
    EXPECT_EQ(g.contents(), "hello");
}

TEST(RootCvtFile, RseekInOutputModeFlushesFirst)
{
    file_guard g("test_file", "");
    auto       obj = rb_root_cvt{ofile_device<char>("test_file")};
    obj.bos();
    obj.main_cont_beg();
    obj.put("hello world", 11);
    obj.rseek(6);
    EXPECT_EQ(obj.tell(), 5u);
}

// rseek() counts back from the end of the main content, which is the file size
// minus the prologue. Swapping in a file shorter than the prologue makes that
// subtraction impossible, and it has to be refused rather than wrapped.
TEST(RootCvtFile, RseekIsRejectedWhenTheFileIsShorterThanThePrologue)
{
    file_guard g1("test_file1", "12345678");
    file_guard g2("test_file2", "ab");

    auto obj = rb_root_cvt{ifile_device<char>("test_file1")};
    obj.bos();

    char buf[3];
    obj.get(buf, 3);     // buffers all eight characters, returns three
    obj.main_cont_beg(); // so the prologue is three characters long

    obj.device() = ifile_device<char>("test_file2"); // only two characters long
    EXPECT_THROW(obj.rseek(0), cvt_error);
}

// Switching to the direction the converter is already in changes nothing, and
// must not disturb the buffers on the way.
TEST(RootCvtFile, SwitchToGetWhileAlreadyReadingIsANoop)
{
    file_guard g("test_file", "hello");
    auto       obj = rb_root_cvt{file_device<char>("test_file")};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    obj.switch_to_get();
    char buf[5] = {};
    EXPECT_EQ(obj.get(buf, 5), 5u);
    EXPECT_EQ(std::string(buf, 5), "hello");
}

TEST(RootCvtFile, SwitchToPutWhileAlreadyWritingIsANoop)
{
    file_guard g("test_file", "");
    auto       obj = rb_root_cvt{file_device<char>("test_file", file_open_flag::trunc)};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    obj.switch_to_put();
    obj.put("world", 5);
    obj.detach();
    EXPECT_EQ(g.contents(), "world");
}

// Move-assignment and destruction both flush what the target still holds. Neither
// may let an exception from that flush escape: assignment would leave the object
// half-assigned, and the destructor would terminate.
TEST(RootCvtFile, MoveAssignmentSwallowsAFailingFlush)
{
    auto obj = rb_root_cvt{throw_write_device{}};
    obj.bos();
    obj.main_cont_beg();
    obj.put("hello", 5);
    obj.device().should_throw = true;

    auto obj2 = rb_root_cvt{throw_write_device{}};
    obj2.bos();
    obj2.main_cont_beg();
    EXPECT_NO_THROW(obj = std::move(obj2));
}

TEST(RootCvtFile, DestructionSwallowsAFailingFlush)
{
    auto obj = rb_root_cvt{throw_write_device{}};
    obj.bos();
    obj.main_cont_beg();
    obj.put("hello", 5);
    obj.device().should_throw = true;
    // obj is destroyed at the end of the test; the throw from flush() has to be
    // swallowed there rather than reaching std::terminate.
}

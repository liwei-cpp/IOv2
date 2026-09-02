// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

namespace
{
    using RbCvt   = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    using NoRbCvt = code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t>;

    constexpr std::size_t kExtSize = 4102;          // bytes on the device
    constexpr std::size_t kIntSize = 4102 / 7 * 3;  // char32_t they decode to

    // The external form: 586 repetitions of the UTF-8 for U'李' (3 bytes) and
    // U'伟' (3 bytes) plus one ASCII byte cycling 1..127. Seven external bytes per
    // three internal characters is what makes the two sizes above differ, and it
    // is why every chunk size below lands mid-character sooner or later.
    std::string external_sample()
    {
        std::string out;
        out.resize(kExtSize);
        for (std::size_t i = 0; i < kExtSize; i += 7)
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

    // The internal form of the same sample.
    std::u32string internal_sample()
    {
        std::u32string out;
        out.reserve(kIntSize);
        for (std::size_t i = 0; i < kIntSize; i += 3)
        {
            out.push_back(U'李');
            out.push_back(U'伟');
            out.push_back((i / 3) % 127 + 1);
        }
        return out;
    }

    constexpr std::size_t kChunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

    // Little-endian char32_t bytes for the given ASCII characters, which is what a
    // 4-byte-per-character locale writes to the device.
    std::string as_char32_bytes(const char* ascii)
    {
        std::string out;
        for (const char* p = ascii; *p; ++p)
        {
            out += *p;
            out += '\x00';
            out += '\x00';
            out += '\x00';
        }
        return out;
    }

    void expect_decodes_to_the_sample(const std::vector<char32_t>& out_buf)
    {
        auto it = out_buf.begin();
        for (std::size_t i = 0; i < out_buf.size(); i += 3)
        {
            EXPECT_EQ(*it++, U'李');
            EXPECT_EQ(*it++, U'伟');
            EXPECT_EQ(*it++, static_cast<char32_t>((i / 3) % 127 + 1));
        }
    }

    // Reads the whole sample in rotating chunks. `fork` decides what happens to the
    // converter between chunks: it is handed the live converter and returns the one
    // the next chunk goes through, and `restore` puts that one back. Between them
    // they take a full copy or a full move every step, so the decoder state and the
    // position have to survive both directions of the round trip.
    template <typename T, typename Fork, typename Restore>
    void expect_chunked_get_survives(T& obj, Fork fork, Restore restore)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::vector<char32_t> out_buf(kExtSize);
        std::size_t           total = 0;
        char32_t*             cur   = out_buf.data();
        int                   id    = 0;
        while (true)
        {
            T           stepped = fork(obj);
            std::size_t n       = std::min<std::size_t>(kExtSize - total, kChunks[id++]);
            auto        s       = stepped.get(cur, n);
            id %= std::size(kChunks);
            cur   += s;
            total += s;
            EXPECT_EQ(stepped.tell(), total);
            if (s == 0) break;
            restore(obj, stepped);
        }

        ASSERT_EQ(cur - out_buf.data(), static_cast<std::ptrdiff_t>(kIntSize));
        out_buf.resize(kIntSize);
        expect_decodes_to_the_sample(out_buf);
    }

    // The write mirror of the above.
    template <typename T, typename Fork, typename Restore>
    void expect_chunked_put_survives(T& obj, Fork fork, Restore restore)
    {
        const std::u32string i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total = 0;
        const char32_t* cur   = i_lit.data();
        int             id    = 0;
        while (total < kIntSize)
        {
            T           stepped = fork(obj);
            std::size_t n       = std::min<std::size_t>(kIntSize - total, kChunks[id++]);
            stepped.put(cur, n);
            id %= std::size(kChunks);
            cur   += n;
            total += n;
            EXPECT_EQ(stepped.tell(), total);
            restore(obj, stepped);
        }

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), external_sample());
    }

    // The plain chunked read, with no copying or moving in between.
    template <typename T>
    void expect_chunked_get_reads_the_sample(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::vector<char32_t> out_buf(kExtSize);
        std::size_t           total = 0;
        char32_t*             cur   = out_buf.data();
        int                   id    = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kExtSize - total, kChunks[id++]);
            auto        s = obj.get(cur, n);
            id %= std::size(kChunks);
            cur   += s;
            total += s;
            EXPECT_EQ(obj.tell(), total);
            if (s == 0) break;
        }

        ASSERT_EQ(cur - out_buf.data(), static_cast<std::ptrdiff_t>(kIntSize));
        out_buf.resize(kIntSize);
        expect_decodes_to_the_sample(out_buf);
    }

    template <typename T>
    void expect_chunked_put_writes_the_sample(T& obj)
    {
        const std::u32string i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total = 0;
        const char32_t* cur   = i_lit.data();
        int             id    = 0;
        while (total < kIntSize)
        {
            std::size_t n = std::min<std::size_t>(kIntSize - total, kChunks[id++]);
            obj.put(cur, n);
            id %= std::size(kChunks);
            cur   += n;
            total += n;
            EXPECT_EQ(obj.tell(), total);
        }

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), external_sample());
    }

    template <typename T>
    void expect_whole_put_writes_the_sample(T& obj)
    {
        const std::u32string i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), external_sample());
    }
}

TEST(CodeCvtMemChar32, TraitsOverAMemDeviceOfChar)
{
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    // The device holds bytes while the caller sees code points, which is the whole
    // point of this converter -- and the two positions it has to keep in step.
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(CodeCvtMemChar32, TraitsWithoutAReadBuffer)
{
    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
}

TEST(CodeCvtMemChar32, TraitsWithWcharAsTheInternalType)
{
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
}

// The decoder carries state between chunks -- a multi-byte sequence can be split
// across two get() calls -- so a converter that is copied or moved between every
// chunk has to carry that state with it.
TEST(CodeCvtMemChar32, ChunkedGetSurvivesACopyBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar32, ChunkedGetSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"}};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar32, ChunkedGetSurvivesAMoveBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar32, ChunkedGetSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"}};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar32, ChunkedPutSurvivesACopyBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar32, ChunkedPutSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar32, ChunkedPutSurvivesAMoveBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"};
    expect_chunked_put_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar32, ChunkedPutSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    expect_chunked_put_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

namespace
{
    // main_cont_beg() draws the line between the prologue and the main content:
    // tell() counts internal characters from there, while the device's own cursor
    // keeps counting bytes from the start of the stream.
    template <typename T>
    void expect_an_empty_prologue_leaves_both_positions_at_zero(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_EQ(obj.device().dtell(), 0u);
    }

    // Three ASCII characters read in a locale that stores four bytes each: the
    // prologue is twelve bytes on the device but tell() still restarts at zero.
    template <typename T>
    void expect_a_read_prologue_of_three_ascii_characters(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        char32_t c = 0;
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, U'1');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, U'2');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, U'3');

        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_EQ(obj.device().dtell(), 12u);
    }
}

TEST(CodeCvtMemChar32, AnEmptyPrologueLeavesBothPositionsAtZero)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
}

TEST(CodeCvtMemChar32, AnEmptyPrologueLeavesBothPositionsAtZeroThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
}

TEST(CodeCvtMemChar32, AReadPrologueLeavesTheDeviceAheadOfTell)
{
    NoRbCvt obj(no_rb_root_cvt{mem_device(as_char32_bytes("12345"))}, "zh_CN.UTF-8");
    expect_a_read_prologue_of_three_ascii_characters(obj);
}

TEST(CodeCvtMemChar32, AReadPrologueLeavesTheDeviceAheadOfTellThroughARuntimeCvt)
{
    runtime_cvt obj{NoRbCvt{no_rb_root_cvt{mem_device(as_char32_bytes("12345"))}, "zh_CN.UTF-8"}};
    expect_a_read_prologue_of_three_ascii_characters(obj);
}

namespace
{
    // The same prologue, but with two of the three characters outside ASCII: the
    // byte count is the same because this locale is fixed-width, and the decoder
    // has to hand back the code points rather than the bytes.
    template <typename T>
    void expect_a_read_prologue_of_mixed_width_characters(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        char32_t c = 0;
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, U'李');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, U'd');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, U'伟');

        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.dtell(), 12u);
    }

    // A prologue written rather than read: three characters go out before
    // main_cont_beg(), and they have to be on the device by the time it returns.
    template <typename T>
    void expect_a_write_prologue(T& obj, const char32_t* text, const std::string& expected_bytes)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.put(text, 3);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);

        const auto& dev = obj.device();
        EXPECT_EQ(dev.dtell(), 12u);
        EXPECT_EQ(dev.str(), expected_bytes);
    }

    // Little-endian char32_t for U'李' U'd' U'伟'.
    std::string li_d_wei_bytes()
    {
        std::string info;
        info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
        info += 'd';    info += '\x00'; info += '\x00'; info += '\x00';
        info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
        return info;
    }
}

TEST(CodeCvtMemChar32, AReadPrologueOfMixedWidthCharacters)
{
    std::string info = li_d_wei_bytes() + as_char32_bytes("cpp");
    RbCvt       obj(rb_root_cvt{mem_device(info)}, "zh_CN.UTF-8");
    expect_a_read_prologue_of_mixed_width_characters(obj);
}

TEST(CodeCvtMemChar32, AReadPrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
    std::string info = li_d_wei_bytes() + as_char32_bytes("cpp");
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(info)}, "zh_CN.UTF-8"}};
    expect_a_read_prologue_of_mixed_width_characters(obj);
}

TEST(CodeCvtMemChar32, AWritePrologueOfAsciiCharacters)
{
    RbCvt          obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, text, as_char32_bytes("123"));
}

TEST(CodeCvtMemChar32, AWritePrologueOfAsciiCharactersThroughARuntimeCvt)
{
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, text, as_char32_bytes("123"));
}

TEST(CodeCvtMemChar32, AWritePrologueOfMixedWidthCharacters)
{
    RbCvt          obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_bytes());
}

TEST(CodeCvtMemChar32, AWritePrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_bytes());
}

TEST(CodeCvtMemChar32, ChunkedGetDecodesTheWholeSample)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar32, ChunkedGetDecodesTheWholeSampleThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"}};
    expect_chunked_get_reads_the_sample(obj);
}

// The same read with no read-back buffer under the converter: a split multi-byte
// sequence cannot be re-read from the kernel, so the decoder has to hold the
// partial state itself.
TEST(CodeCvtMemChar32, ChunkedGetDecodesTheWholeSampleWithoutAReadBuffer)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar32, ChunkedGetDecodesTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    runtime_cvt obj{NoRbCvt{no_rb_root_cvt{mem_device(external_sample())}, "zh_CN.UTF-8"}};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar32, ChunkedPutEncodesTheWholeSample)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_chunked_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar32, ChunkedPutEncodesTheWholeSampleThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    expect_chunked_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar32, ChunkedPutEncodesTheWholeSampleWithoutAReadBuffer)
{
    NoRbCvt obj(no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_chunked_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar32, ChunkedPutEncodesTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    runtime_cvt obj{NoRbCvt{no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    expect_chunked_put_writes_the_sample(obj);
}

// One put() of everything must produce the same bytes as the chunked one.
TEST(CodeCvtMemChar32, AWholePutEncodesTheSameBytesAsAChunkedOne)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_whole_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar32, AWholePutEncodesTheSameBytesAsAChunkedOneThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    expect_whole_put_writes_the_sample(obj);
}

// The same single put() without a read buffer under the converter: the encoder
// has nowhere to stage the bytes but the device itself.
TEST(CodeCvtMemChar32, AWholePutEncodesTheSameBytesAsAChunkedOneWithoutAReadBuffer)
{
    NoRbCvt obj(no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_whole_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar32, AWholePutEncodesTheSameBytesAsAChunkedOneWithoutAReadBufferThroughARuntimeCvt)
{
    runtime_cvt obj{NoRbCvt{no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    expect_whole_put_writes_the_sample(obj);
}

namespace
{
    // flush() has to push the encoder's pending bytes all the way to the device,
    // and must not move the position: tell() still counts what the caller wrote.
    template <typename T>
    void expect_flush_completes_the_encoding(T& obj)
    {
        const std::u32string i_lit    = internal_sample();
        const std::string    expected = external_sample();

        const auto& dev = obj.device();
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
        EXPECT_EQ(dev.str().size(), expected.size());
        EXPECT_EQ(obj.tell(), i_lit.size());
        EXPECT_EQ(dev.str(), expected);
    }

    // Flushing after every chunk has to move bytes to the device each time, and
    // still add up to exactly the same stream at the end.
    template <typename T>
    void expect_a_flush_after_every_chunk_reaches_the_device(T& obj)
    {
        const std::u32string i_lit = internal_sample();

        const auto& dev = obj.device();
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total = 0;
        const char32_t* cur   = i_lit.data();
        int             id    = 0;
        while (total < kIntSize)
        {
            std::size_t n       = std::min<std::size_t>(kIntSize - total, kChunks[id++]);
            std::size_t ori_len = dev.str().size();
            obj.put(cur, n);
            id %= std::size(kChunks);
            cur += n;

            EXPECT_EQ(obj.tell(), total + n);
            obj.flush();
            total += n;
            EXPECT_NE(dev.str().size(), ori_len);
            EXPECT_EQ(obj.tell(), total);
        }
        obj.flush();
        EXPECT_EQ(dev.str(), external_sample());
    }
}

TEST(CodeCvtMemChar32, FlushCompletesTheEncoding)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar32, FlushCompletesTheEncodingThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar32, FlushCompletesTheEncodingWithoutAReadBuffer)
{
    NoRbCvt obj(no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar32, FlushCompletesTheEncodingWithoutAReadBufferThroughARuntimeCvt)
{
    runtime_cvt obj(NoRbCvt{no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar32, AFlushAfterEveryChunkReachesTheDevice)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_a_flush_after_every_chunk_reaches_the_device(obj);
}

TEST(CodeCvtMemChar32, AFlushAfterEveryChunkReachesTheDeviceThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    expect_a_flush_after_every_chunk_reaches_the_device(obj);
}

namespace
{
    // In a variable-width locale the converter cannot turn a character index into a
    // byte offset without decoding everything in front of it, so it refuses every
    // seek but the one that is already true.
    template <typename T>
    void expect_seek_is_refused_in_a_variable_width_locale(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_ANY_THROW(obj.seek(100));
        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_ANY_THROW(obj.seek(1));
        EXPECT_EQ(obj.tell(), 0u);
        obj.seek(0);
        EXPECT_EQ(obj.tell(), 0u);

        std::u32string str(6, U'\0');
        EXPECT_EQ(obj.get(str.data(), 6), 6u);
        EXPECT_EQ(str, U"123456");
    }

    // In a single-byte locale the character index is the byte offset, so seeking
    // inside the stream works -- past the end still does not.
    template <typename T>
    void expect_seek_works_in_a_single_byte_locale(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_ANY_THROW(obj.seek(100));
        EXPECT_EQ(obj.tell(), 0u);
        obj.seek(1);
        EXPECT_EQ(obj.tell(), 1u);

        std::u32string str(6, U'\0');
        EXPECT_EQ(obj.get(str.data(), 6), 5u);
        str.resize(5);
        EXPECT_EQ(str, U"23456");
    }

    // The same on the write side: a variable-width locale refuses every seek, so
    // the writes stay where they were and simply append.
    template <typename T>
    void expect_seek_is_refused_while_writing_variable_width(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        char32_t ch = U'李';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 1u);
        ch = U'x';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 2u);
        ch = U'伟';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 3u);

        EXPECT_ANY_THROW(obj.seek(100));
        EXPECT_EQ(obj.tell(), 3u);
        EXPECT_ANY_THROW(obj.seek(1));
        EXPECT_EQ(obj.tell(), 3u);
        EXPECT_ANY_THROW(obj.seek(0));
        EXPECT_EQ(obj.tell(), 3u);

        const char32_t c[] = U"xy";
        obj.put(c, 2);

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), "李x伟xy");
    }

    // In a single-byte locale a write seek does land, and what follows overwrites
    // from there.
    template <typename T>
    void expect_seek_works_while_writing_single_byte(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        char32_t ch = U'a';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 1u);
        ch = U'b';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 2u);
        ch = U'c';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 3u);

        EXPECT_ANY_THROW(obj.seek(100));
        EXPECT_EQ(obj.tell(), 3u);
        obj.seek(1);
        EXPECT_EQ(obj.tell(), 1u);

        const char32_t c[] = U"xy";
        obj.put(c, 2);

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), "axy");
    }
}

TEST(CodeCvtMemChar32, SeekIsRefusedWhileReadingInAVariableWidthLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8");
    expect_seek_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, SeekIsRefusedWhileReadingInAVariableWidthLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8"});
    expect_seek_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, SeekWorksWhileReadingInASingleByteLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("123456")}, "C");
    expect_seek_works_in_a_single_byte_locale(obj);
}

TEST(CodeCvtMemChar32, SeekWorksWhileReadingInASingleByteLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("123456")}, "C"});
    expect_seek_works_in_a_single_byte_locale(obj);
}

TEST(CodeCvtMemChar32, SeekIsRefusedWhileWritingInAVariableWidthLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_seek_is_refused_while_writing_variable_width(obj);
}

TEST(CodeCvtMemChar32, SeekIsRefusedWhileWritingInAVariableWidthLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    expect_seek_is_refused_while_writing_variable_width(obj);
}

TEST(CodeCvtMemChar32, SeekWorksWhileWritingInASingleByteLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "C");
    expect_seek_works_while_writing_single_byte(obj);
}

TEST(CodeCvtMemChar32, SeekWorksWhileWritingInASingleByteLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("")}, "C"});
    expect_seek_works_while_writing_single_byte(obj);
}

namespace
{
    // rseek() counts back from the end, which needs the same index-to-offset map
    // seek() needs, so it is refused in a variable-width locale for the same
    // reason -- including rseek(0), which would still have to find the end.
    template <typename T>
    void expect_rseek_is_refused_in_a_variable_width_locale(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_ANY_THROW(obj.rseek(100));
        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_ANY_THROW(obj.rseek(1));
        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_ANY_THROW(obj.rseek(0));
        EXPECT_EQ(obj.tell(), 0u);

        std::u32string str(6, U'\0');
        EXPECT_EQ(obj.get(str.data(), 6), 6u);
        EXPECT_EQ(obj.device().str(), "123456");
    }

    template <typename T>
    void expect_rseek_works_in_a_single_byte_locale(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_ANY_THROW(obj.rseek(100));
        EXPECT_EQ(obj.tell(), 0u);
        obj.rseek(4);
        EXPECT_EQ(obj.tell(), 2u);

        std::u32string str(6, U'\0');
        EXPECT_EQ(obj.get(str.data(), 6), 4u);
        str.resize(4);
        EXPECT_EQ(str, U"3456");
    }

    template <typename T>
    void expect_rseek_is_refused_while_writing_variable_width(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        char32_t ch = U'李';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 1u);
        ch = U'x';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 2u);
        ch = U'伟';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 3u);

        EXPECT_ANY_THROW(obj.rseek(100));
        EXPECT_EQ(obj.tell(), 3u);
        EXPECT_ANY_THROW(obj.rseek(1));
        EXPECT_EQ(obj.tell(), 3u);

        const char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        EXPECT_EQ(obj.device().str(), "李x伟xy");
    }

    template <typename T>
    void expect_rseek_works_while_writing_single_byte(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);
        char32_t ch = U'a';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 1u);
        ch = U'b';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 2u);
        ch = U'c';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 3u);

        EXPECT_ANY_THROW(obj.rseek(100));
        EXPECT_EQ(obj.tell(), 3u);
        obj.rseek(1);
        EXPECT_EQ(obj.tell(), 2u);

        const char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        EXPECT_EQ(obj.device().str(), "abxy");
    }
}

TEST(CodeCvtMemChar32, RseekIsRefusedWhileReadingInAVariableWidthLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8");
    expect_rseek_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, RseekIsRefusedWhileReadingInAVariableWidthLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8"});
    expect_rseek_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, RseekWorksWhileReadingInASingleByteLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("123456")}, "C");
    expect_rseek_works_in_a_single_byte_locale(obj);
}

TEST(CodeCvtMemChar32, RseekWorksWhileReadingInASingleByteLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("123456")}, "C"});
    expect_rseek_works_in_a_single_byte_locale(obj);
}

TEST(CodeCvtMemChar32, RseekIsRefusedWhileWritingInAVariableWidthLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    expect_rseek_is_refused_while_writing_variable_width(obj);
}

TEST(CodeCvtMemChar32, RseekIsRefusedWhileWritingInAVariableWidthLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    expect_rseek_is_refused_while_writing_variable_width(obj);
}

TEST(CodeCvtMemChar32, RseekWorksWhileWritingInASingleByteLocale)
{
    RbCvt obj(rb_root_cvt{mem_device("")}, "C");
    expect_rseek_works_while_writing_single_byte(obj);
}

TEST(CodeCvtMemChar32, RseekWorksWhileWritingInASingleByteLocaleThroughARuntimeCvt)
{
    runtime_cvt obj(RbCvt{rb_root_cvt{mem_device("")}, "C"});
    expect_rseek_works_while_writing_single_byte(obj);
}

namespace
{
    // Writing, seeking back inside what was written, reading it, then writing on:
    // in a single-byte locale all three directions share one position, and the
    // stream that comes out has to reflect them in the order they happened.
    template <typename T>
    void expect_read_and_write_share_one_position(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);

        const char32_t bos_str[] = U"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        const char32_t content1[] = U"12345";
        obj.put(content1, 5);
        obj.seek(0);

        std::u32string get_content(3, U'\0');
        EXPECT_EQ(obj.get(get_content.data(), 3), 3u);
        EXPECT_EQ(get_content, U"123");
        EXPECT_EQ(obj.tell(), 3u);

        const char32_t content2[] = U"78";
        obj.put(content2, 2);
        EXPECT_EQ(obj.tell(), 5u);
        obj.flush();

        EXPECT_EQ(obj.device().str(), as_char32_bytes("abcdefgh") + "12378");
    }

    // The mirror: a stream opened for reading, turned around mid-way, and read
    // again -- the prologue is the eight characters read before main_cont_beg().
    template <typename T>
    void expect_a_read_stream_can_be_written_through(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        std::u32string bos_str(8, U'\0');
        EXPECT_EQ(obj.get(bos_str.data(), 8), 8u);
        EXPECT_EQ(bos_str, U"abcdefgh");
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        const char32_t content1[] = U"67";
        obj.put(content1, 2);
        EXPECT_EQ(obj.tell(), 2u);

        std::u32string content_str(2, U'\0');
        EXPECT_EQ(obj.get(content_str.data(), 2), 2u);
        EXPECT_EQ(content_str, U"34");
        EXPECT_EQ(obj.tell(), 4u);

        obj.seek(0);
        EXPECT_EQ(obj.tell(), 0u);
        const char32_t content2[] = U"QWER";
        obj.put(content2, 4);
        EXPECT_EQ(obj.tell(), 4u);
        obj.flush();

        content_str.resize(4);
        EXPECT_EQ(obj.get(content_str.data(), 4), 1u);
        EXPECT_EQ(content_str[0], U'5');

        EXPECT_EQ(obj.device().str(), as_char32_bytes("abcdefgh") + "QWER5");
    }
}

TEST(CodeCvtMemChar32, ReadAndWriteShareOnePosition)
{
    NoRbCvt obj(no_rb_root_cvt{mem_device("")}, "C");
    expect_read_and_write_share_one_position(obj);
}

TEST(CodeCvtMemChar32, ReadAndWriteShareOnePositionThroughARuntimeCvt)
{
    runtime_cvt obj(NoRbCvt{no_rb_root_cvt{mem_device("")}, "C"});
    expect_read_and_write_share_one_position(obj);
}

TEST(CodeCvtMemChar32, AReadStreamCanBeWrittenThrough)
{
    std::string info = as_char32_bytes("abcdefgh") + "12345";
    NoRbCvt     obj(no_rb_root_cvt{mem_device(info)}, "C");
    expect_a_read_stream_can_be_written_through(obj);
}

TEST(CodeCvtMemChar32, AReadStreamCanBeWrittenThroughThroughARuntimeCvt)
{
    std::string info = as_char32_bytes("abcdefgh") + "12345";
    runtime_cvt obj(NoRbCvt{no_rb_root_cvt{mem_device(info)}, "C"});
    expect_a_read_stream_can_be_written_through(obj);
}

namespace
{
    // In a variable-width locale the same sequence cannot seek, so a get() after a
    // write returns nothing and the position never moves back. Once the stream has
    // been turned round to reading explicitly and seeked to a position that is
    // known (0), turning it back to writing is refused: the decoder would have to
    // know where the read left off in bytes.
    template <typename T>
    void expect_turning_around_is_refused_in_a_variable_width_locale(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);

        const char32_t bos_str[] = U"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        const char32_t content1[] = U"12345";
        obj.put(content1, 5);
        EXPECT_ANY_THROW(obj.seek(0));

        std::u32string get_content(3, U'\0');
        EXPECT_EQ(obj.get(get_content.data(), 3), 0u);
        EXPECT_EQ(obj.tell(), 5u);

        const char32_t content2[] = U"78";
        obj.put(content2, 2);
        EXPECT_EQ(obj.tell(), 7u);
        obj.flush();

        obj.switch_to_get();
        obj.seek(0);
        get_content.resize(3);
        EXPECT_EQ(obj.get(get_content.data(), 3), 3u);
        EXPECT_EQ(get_content, U"123");

        EXPECT_ANY_THROW(obj.switch_to_put());

        EXPECT_EQ(obj.device().str(), as_char32_bytes("abcdefgh") + "1234578");
    }

    // A stream being read in a variable-width locale cannot be written to at all:
    // the byte offset of the current character is not known.
    template <typename T>
    void expect_writing_is_refused_while_reading_variable_width(T& obj, const std::string& info)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        std::u32string bos_str(8, U'\0');
        EXPECT_EQ(obj.get(bos_str.data(), 8), 8u);
        EXPECT_EQ(bos_str, U"abcdefgh");
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        std::u32string content_str(2, U'\0');
        EXPECT_EQ(obj.get(content_str.data(), 2), 2u);
        EXPECT_EQ(content_str, U"12");

        const char32_t content1[] = U"67";
        EXPECT_ANY_THROW(obj.put(content1, 2));
        EXPECT_EQ(obj.tell(), 2u);

        obj.seek(0);
        EXPECT_EQ(obj.tell(), 0u);

        content_str.resize(10);
        EXPECT_EQ(obj.get(content_str.data(), 10), 5u);
        EXPECT_EQ(content_str.substr(0, 5), U"12345");

        EXPECT_EQ(obj.device().str(), info);
    }
}

TEST(CodeCvtMemChar32, TurningAroundIsRefusedInAVariableWidthLocale)
{
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj = creator.create(no_rb_root_cvt{mem_device("")});
    expect_turning_around_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, TurningAroundIsRefusedInAVariableWidthLocaleWithAReadBuffer)
{
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    expect_turning_around_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, TurningAroundIsRefusedInAVariableWidthLocaleThroughARuntimeCvt)
{
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    runtime_cvt obj(creator.create(no_rb_root_cvt{mem_device("")}));
    expect_turning_around_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, TurningAroundIsRefusedInAVariableWidthLocaleWithAReadBufferThroughARuntimeCvt)
{
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    runtime_cvt obj(creator.create(rb_root_cvt{mem_device("")}));
    expect_turning_around_is_refused_in_a_variable_width_locale(obj);
}

TEST(CodeCvtMemChar32, WritingIsRefusedWhileReadingInAVariableWidthLocale)
{
    const std::string info = as_char32_bytes("abcdefgh") + "12345";
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj = creator.create(no_rb_root_cvt{mem_device(info)});
    expect_writing_is_refused_while_reading_variable_width(obj, info);
}

TEST(CodeCvtMemChar32, WritingIsRefusedWhileReadingInAVariableWidthLocaleWithAReadBuffer)
{
    const std::string info = as_char32_bytes("abcdefgh") + "12345";
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj = creator.create(rb_root_cvt{mem_device(info)});
    expect_writing_is_refused_while_reading_variable_width(obj, info);
}

TEST(CodeCvtMemChar32, WritingIsRefusedWhileReadingInAVariableWidthLocaleThroughARuntimeCvt)
{
    const std::string info = as_char32_bytes("abcdefgh") + "12345";
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    runtime_cvt obj(creator.create(no_rb_root_cvt{mem_device(info)}));
    expect_writing_is_refused_while_reading_variable_width(obj, info);
}

TEST(CodeCvtMemChar32, WritingIsRefusedWhileReadingInAVariableWidthLocaleWithAReadBufferThroughARuntimeCvt)
{
    const std::string info = as_char32_bytes("abcdefgh") + "12345";
    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    runtime_cvt obj(creator.create(rb_root_cvt{mem_device(info)}));
    expect_writing_is_refused_while_reading_variable_width(obj, info);
}

// The "C" locale encodes one byte per character, so a code point outside ASCII
// has no representation at all and wcrtomb refuses it.
TEST(CodeCvtMemChar32, ACodePointTheLocaleCannotEncodeIsRejected)
{
    {
        RbCvt obj{rb_root_cvt{mem_device("")}, "C"};
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        const char32_t ch = U'李';
        EXPECT_THROW(obj.put(&ch, 1), cvt_error);
    }
    {
        RbCvt obj{rb_root_cvt{mem_device("")}, "C"};
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        const char32_t ch = U'伟';
        EXPECT_THROW(obj.put(&ch, 1), cvt_error);
    }
}

// A failed encode must leave the device byte-for-byte as it was. put_buf reserves
// epc() bytes straight inside mem_device (the cvt_writer specialization for
// mem_device has no staging buffer and commit() is a no-op), so without the
// put_buf_guard in put_main those reserved-but-never-written bytes stay in the
// device as filler.
namespace
{
    template <typename T>
    void expect_a_failed_encode_leaves_the_device_empty(T& obj, char32_t bad)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        EXPECT_THROW(obj.put(&bad, 1), cvt_error);
        EXPECT_TRUE(obj.device().str().empty());
    }
}

TEST(CodeCvtMemChar32, AFailedEncodeLeavesTheDeviceEmptyInTheCLocale)
{
    RbCvt obj{rb_root_cvt{mem_device("")}, "C"};
    expect_a_failed_encode_leaves_the_device_empty(obj, U'李');
}

TEST(CodeCvtMemChar32, AFailedEncodeLeavesTheDeviceEmptyInTheCLocaleWithoutAReadBuffer)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device("")}, "C"};
    expect_a_failed_encode_leaves_the_device_empty(obj, U'李');
}

// In a UTF-8 locale epc() is MB_CUR_MAX, and a lone surrogate is rejected by
// wcrtomb -- so the reserved slot that has to be given back is the widest one
// this kernel ever takes.
TEST(CodeCvtMemChar32, AFailedEncodeLeavesTheDeviceEmptyOnTheWidestReservation)
{
    RbCvt obj{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"};
    expect_a_failed_encode_leaves_the_device_empty(obj, static_cast<char32_t>(0xD800U));
}

TEST(CodeCvtMemChar32, AFailedEncodeLeavesTheDeviceEmptyOnTheWidestReservationWithoutAReadBuffer)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"};
    expect_a_failed_encode_leaves_the_device_empty(obj, static_cast<char32_t>(0xD800U));
}

// With valid output in front, the device must end exactly at the last valid byte.
TEST(CodeCvtMemChar32, AFailedEncodeLeavesTheEarlierOutputUntouched)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const char32_t ok[] = {U'a', U'b'};
    obj.put(ok, 2);

    const char32_t bad = static_cast<char32_t>(0xD800U);
    EXPECT_THROW(obj.put(&bad, 1), cvt_error);
    EXPECT_EQ(obj.device().str(), "ab");
}

// A '\0' byte decodes to U'\0', which mbrtowc reports by returning 0 rather than
// the byte count -- a path of its own that must still advance by one character.
TEST(CodeCvtMemChar32, ANullByteDecodesToANullCharacter)
{
    std::string buf;
    buf += 'a';
    buf += '\0';
    buf += 'b';

    RbCvt    obj{rb_root_cvt{mem_device(buf)}, "C"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char32_t out[3] = {1, 1, 1};
    EXPECT_EQ(obj.get(out, 3), 3u);
    EXPECT_EQ(out[0], U'a');
    EXPECT_EQ(out[1], U'\0');
    EXPECT_EQ(out[2], U'b');
}

TEST(CodeCvtMemChar32, ANullByteDecodesToANullCharacterThroughARuntimeCvt)
{
    std::string buf;
    buf += 'a';
    buf += '\0';
    buf += 'b';

    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(buf)}, "C"}};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char32_t out[3] = {1, 1, 1};
    EXPECT_EQ(obj.get(out, 3), 3u);
    EXPECT_EQ(out[0], U'a');
    EXPECT_EQ(out[1], U'\0');
    EXPECT_EQ(out[2], U'b');
}

// In the "C" locale every byte above 0x7F is invalid, and mbrtowc says so with
// -1. The converter has to report that rather than substitute anything.
TEST(CodeCvtMemChar32, AnInvalidExternalByteIsRejected)
{
    for (char bad : {'\xff', '\xfe'})
    {
        std::string bytes(1, bad);
        RbCvt       obj{rb_root_cvt{mem_device(bytes)}, "C"};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        char32_t buf[4];
        EXPECT_THROW((void)obj.get(buf, 4), cvt_error);
    }
}

namespace
{
    struct throw_on_put
    {
        using char_type = char;
        bool should_throw = false;

        void dput(const char_type*, std::size_t)
        {
            if (should_throw) throw device_error("forced put error");
        }
        void dflush() {}
    };
}

// flush() only has something to do while writing; in any other state it returns
// without touching the kernel.
TEST(CodeCvtMemChar32, FlushIsANoopWhileNotWriting)
{
    RbCvt obj{rb_root_cvt{mem_device(std::string("hello"))}, "C"};
    EXPECT_NO_THROW(obj.flush()); // before bos(): no direction yet

    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();
    EXPECT_NO_THROW(obj.flush()); // reading: nothing pending to write
}

// A flush that fails leaves the stream in an unknown state, so the converter is
// tainted and every later operation refuses rather than guesses.
TEST(CodeCvtMemChar32, AFailedFlushTaintsTheConverter)
{
    code_cvt<rb_root_cvt<throw_on_put>, char32_t> obj{rb_root_cvt{throw_on_put{}}, "C"};

    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const char32_t ch = U'A';
    obj.put(&ch, 1);

    obj.device().should_throw = true;
    EXPECT_THROW(obj.flush(), device_error);
    EXPECT_THROW(obj.flush(), cvt_error);
}

// The taint can also be set directly by a derived converter that knows it has
// lost track of the stream; from then on the object behaves the same way.
TEST(CodeCvtMemChar32, SetTaintedMakesEveryLaterOperationRefuse)
{
    struct taintable : RbCvt
    {
        using RbCvt::RbCvt;
        void expose_set_tainted() { set_tainted(); }
    };

    taintable obj{rb_root_cvt{mem_device("")}, "C"};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    obj.expose_set_tainted();
    EXPECT_THROW(obj.flush(), cvt_error);
}

// The encode/decode helpers take a range and must reject one that is inverted --
// a caller error that would otherwise read or write outside the buffer.
TEST(CodeCvtMemChar32, TheEncodeHelperRejectsAnInvertedRange)
{
    codecvt_kernel<char, char32_t> kernel("C");

    char  buf[4];
    char* to     = buf + 2;
    char* to_end = buf;
    EXPECT_THROW((void)kernel.out_helper(U'A', to, to_end), cvt_error);
}

// A buffer with no room left is not an error: the helper says so by returning
// false, and the caller makes room and comes back.
TEST(CodeCvtMemChar32, TheEncodeHelperReportsAFullBuffer)
{
    codecvt_kernel<char, char32_t> kernel("C");

    char  buf[1];
    char* to     = buf;
    char* to_end = buf;
    EXPECT_FALSE(kernel.out_helper(U'A', to, to_end));
}

TEST(CodeCvtMemChar32, TheDecodeHelperRejectsAnInvertedRange)
{
    codecvt_kernel<char, char32_t> kernel("C");

    const char  input[]  = "hello";
    const char* from     = input + 3;
    const char* from_end = input;
    char32_t    out[4];
    char32_t*   to     = out;
    char32_t*   to_end = out + 4;
    EXPECT_THROW(kernel.in_helper(from, from_end, to, to_end), cvt_error);
}

// 0xE6 opens a three-byte UTF-8 sequence, so a stream that ends there leaves the
// decoder mid-character. Turning the converter round to writing at that point
// would strand those bytes, so it is refused.
TEST(CodeCvtMemChar32, SwitchingToWritingIsRefusedMidCharacter)
{
    std::string partial;
    partial += '\xE6';

    RbCvt obj{rb_root_cvt{mem_device(partial)}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char32_t buf[4];
    obj.get(buf, 4); // consumes 0xE6; mbrtowc reports the sequence as incomplete

    EXPECT_THROW(obj.switch_to_put(), cvt_error);
}

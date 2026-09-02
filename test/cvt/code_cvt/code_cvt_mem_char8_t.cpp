// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/cvt_facilities.h>
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

// The counterpart of code_cvt_mem_char_char32_t.cpp: there the external side is
// char and the encoding comes from a named locale, here it is char8_t and the
// library's own UTF-8 kernel does the work, so no locale name is given at all.
namespace
{
    using RbCvt   = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    using NoRbCvt = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, char32_t>;

    constexpr std::size_t kExtSize = 4102;          // char8_t on the device
    constexpr std::size_t kIntSize = 4102 / 7 * 3;  // char32_t they decode to

    // 586 repetitions of the UTF-8 for U'李' (3 bytes) and U'伟' (3 bytes) plus one
    // ASCII byte cycling 1..127. Seven external units per three internal
    // characters is what makes the two sizes above differ, and it is why every
    // chunk size below lands mid-character sooner or later.
    std::u8string external_sample()
    {
        std::u8string out;
        out.resize(kExtSize);
        for (std::size_t i = 0; i < kExtSize; i += 7)
        {
            out[i + 0] = u8'\xE6';
            out[i + 1] = u8'\x9D';
            out[i + 2] = u8'\x8E';
            out[i + 3] = u8'\xE4';
            out[i + 4] = u8'\xBC';
            out[i + 5] = u8'\x9F';
            out[i + 6] = (i / 7) % 127 + 1;
        }
        return out;
    }

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

    // Little-endian char32_t code units written as UTF-8 bytes -- what these tests
    // use as a fixed, seekable prologue: every character is four units wide, so a
    // character index and a unit offset differ by a constant.
    std::u8string as_char32_units(const char* ascii)
    {
        std::u8string out;
        for (const char* p = ascii; *p; ++p)
        {
            out += static_cast<char8_t>(*p);
            out += u8'\x00';
            out += u8'\x00';
            out += u8'\x00';
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

        EXPECT_EQ(obj.device().str(), external_sample());
    }

    template <typename T>
    void expect_whole_put_writes_the_sample(T& obj)
    {
        const std::u32string i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());

        EXPECT_EQ(obj.device().str(), external_sample());
    }
}

TEST(CodeCvtMemChar8, TraitsOverAMemDeviceOfChar8)
{
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(CodeCvtMemChar8, TraitsWithoutAReadBuffer)
{
    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, char32_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);
}

// The decoder carries state between chunks -- a multi-byte sequence can be split
// across two get() calls -- so a converter that is copied or moved between every
// chunk has to carry that state with it.
TEST(CodeCvtMemChar8, ChunkedGetSurvivesACopyBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8, ChunkedGetSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8, ChunkedGetSurvivesAMoveBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar8, ChunkedGetSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar8, ChunkedPutSurvivesACopyBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8, ChunkedPutSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(std::u8string{})}}};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8, ChunkedPutSurvivesAMoveBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_chunked_put_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar8, ChunkedPutSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(std::u8string{})}}};
    expect_chunked_put_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

namespace
{
    template <typename T>
    void expect_an_empty_prologue_leaves_both_positions_at_zero(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);
        EXPECT_EQ(obj.device().dtell(), 0u);
    }

    // Three characters read out of a prologue stored four units each: the device
    // is twelve units in but tell() restarts at zero.
    template <typename T>
    void expect_a_read_prologue_of_three_characters(T& obj, char32_t c1, char32_t c2, char32_t c3)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        char32_t c = 0;
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, c1);
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, c2);
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, c3);

        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.dtell(), 12u);
    }

    // A prologue written rather than read: the three characters have to be on the
    // device by the time main_cont_beg() returns.
    template <typename T>
    void expect_a_write_prologue(T& obj, const char32_t* text, const std::u8string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.put(text, 3);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);

        const auto& dev = obj.device();
        EXPECT_EQ(dev.dtell(), 12u);
        EXPECT_EQ(dev.str(), expected);
    }

    // Little-endian char32_t units for U'李' U'd' U'伟'.
    std::u8string li_d_wei_units()
    {
        std::u8string info;
        info += u8'\x4e'; info += u8'\x67'; info += u8'\x00'; info += u8'\x00';
        info += u8'd';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'\x1f'; info += u8'\x4f'; info += u8'\x00'; info += u8'\x00';
        return info;
    }
}

TEST(CodeCvtMemChar8, AnEmptyPrologueLeavesBothPositionsAtZero)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
}

TEST(CodeCvtMemChar8, AnEmptyPrologueLeavesBothPositionsAtZeroThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
}

TEST(CodeCvtMemChar8, AReadPrologueOfAsciiCharacters)
{
    RbCvt obj(rb_root_cvt{mem_device(as_char32_units("12345"))});
    expect_a_read_prologue_of_three_characters(obj, U'1', U'2', U'3');
}

TEST(CodeCvtMemChar8, AReadPrologueOfAsciiCharactersThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(as_char32_units("12345"))}}};
    expect_a_read_prologue_of_three_characters(obj, U'1', U'2', U'3');
}

TEST(CodeCvtMemChar8, AReadPrologueOfMixedWidthCharacters)
{
    RbCvt obj(rb_root_cvt{mem_device(li_d_wei_units() + as_char32_units("cpp"))});
    expect_a_read_prologue_of_three_characters(obj, U'李', U'd', U'伟');
}

TEST(CodeCvtMemChar8, AReadPrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(li_d_wei_units() + as_char32_units("cpp"))}}};
    expect_a_read_prologue_of_three_characters(obj, U'李', U'd', U'伟');
}

TEST(CodeCvtMemChar8, AWritePrologueOfAsciiCharacters)
{
    RbCvt          obj(rb_root_cvt{mem_device(u8"")});
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, text, as_char32_units("123"));
}

TEST(CodeCvtMemChar8, AWritePrologueOfAsciiCharactersThroughARuntimeCvt)
{
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, text, as_char32_units("123"));
}

TEST(CodeCvtMemChar8, AWritePrologueOfMixedWidthCharacters)
{
    RbCvt          obj(rb_root_cvt{mem_device(u8"")});
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_units());
}

TEST(CodeCvtMemChar8, AWritePrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_units());
}

TEST(CodeCvtMemChar8, ChunkedGetDecodesTheWholeSample)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar8, ChunkedGetDecodesTheWholeSampleThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_reads_the_sample(obj);
}

// The same read with no read-back buffer under the converter: a split multi-byte
// sequence cannot be re-read from the kernel, so the decoder has to hold the
// partial state itself.
TEST(CodeCvtMemChar8, ChunkedGetDecodesTheWholeSampleWithoutAReadBuffer)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar8, ChunkedGetDecodesTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    runtime_cvt obj{NoRbCvt{no_rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar8, ChunkedPutEncodesTheWholeSample)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_chunked_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar8, ChunkedPutEncodesTheWholeSampleThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_chunked_put_writes_the_sample(obj);
}

// One put() of everything must produce the same units as the chunked one.
TEST(CodeCvtMemChar8, AWholePutEncodesTheSameUnitsAsAChunkedOne)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_whole_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar8, AWholePutEncodesTheSameUnitsAsAChunkedOneThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_whole_put_writes_the_sample(obj);
}

namespace
{
    // flush() has to push the encoder's pending units all the way to the device,
    // and must not move the position: tell() still counts what the caller wrote.
    template <typename T>
    void expect_flush_completes_the_encoding(T& obj)
    {
        const std::u32string i_lit    = internal_sample();
        const std::u8string  expected = external_sample();

        const auto& dev = obj.device();
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
        EXPECT_EQ(dev.str().size(), expected.size());
        EXPECT_EQ(obj.tell(), i_lit.size());
        EXPECT_EQ(dev.str(), expected);
    }

    // Flushing after every chunk has to move units to the device each time, and
    // still add up to exactly the same stream at the end.
    template <typename T>
    void expect_a_flush_after_every_chunk_reaches_the_device(T& obj)
    {
        const std::u32string i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total = 0;
        const char32_t* cur   = i_lit.data();
        int             id    = 0;
        while (total < kIntSize)
        {
            std::size_t n       = std::min<std::size_t>(kIntSize - total, kChunks[id++]);
            std::size_t ori_len = obj.device().str().size();
            obj.put(cur, n);
            id %= std::size(kChunks);
            cur += n;

            EXPECT_EQ(obj.tell(), total + n);
            obj.flush();
            total += n;
            EXPECT_NE(obj.device().str().size(), ori_len);
            EXPECT_EQ(obj.tell(), total);
        }
        obj.flush();
        EXPECT_EQ(obj.device().str(), external_sample());
    }
}

TEST(CodeCvtMemChar8, FlushCompletesTheEncoding)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar8, FlushCompletesTheEncodingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar8, AFlushAfterEveryChunkReachesTheDevice)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_a_flush_after_every_chunk_reaches_the_device(obj);
}

TEST(CodeCvtMemChar8, AFlushAfterEveryChunkReachesTheDeviceThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_a_flush_after_every_chunk_reaches_the_device(obj);
}

namespace
{
    // UTF-8 is variable width, so the converter cannot turn a character index into
    // a unit offset without decoding everything in front of it: every seek but the
    // one that is already true is refused, in both directions and both modes.
    template <typename T>
    void expect_seek_is_refused_while_reading(T& obj)
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

    template <typename T>
    void expect_rseek_is_refused_while_reading(T& obj)
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
        EXPECT_EQ(obj.device().str(), std::u8string(u8"123456"));
    }

    // `reposition` is seek or rseek: either way it is refused, the position stays
    // put, and the writes simply append.
    template <typename T, typename Reposition>
    void expect_repositioning_is_refused_while_writing(T& obj, Reposition reposition)
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

        EXPECT_ANY_THROW(reposition(obj, 100));
        EXPECT_EQ(obj.tell(), 3u);
        EXPECT_ANY_THROW(reposition(obj, 1));
        EXPECT_EQ(obj.tell(), 3u);
        EXPECT_ANY_THROW(reposition(obj, 0));
        EXPECT_EQ(obj.tell(), 3u);

        const char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        EXPECT_EQ(obj.device().str(), std::u8string(u8"李x伟xy"));
    }
}

TEST(CodeCvtMemChar8, SeekIsRefusedWhileReading)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"123456")});
    expect_seek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8, SeekIsRefusedWhileReadingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"123456")}}};
    expect_seek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8, SeekIsRefusedWhileWriting)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.seek(n); });
}

TEST(CodeCvtMemChar8, SeekIsRefusedWhileWritingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.seek(n); });
}

TEST(CodeCvtMemChar8, RseekIsRefusedWhileReading)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"123456")});
    expect_rseek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8, RseekIsRefusedWhileReadingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"123456")}}};
    expect_rseek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8, RseekIsRefusedWhileWriting)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.rseek(n); });
}

TEST(CodeCvtMemChar8, RseekIsRefusedWhileWritingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.rseek(n); });
}

namespace
{
    // A four-unit-wide prologue makes positions addressable again, but only inside
    // the main content, and only after the stream has been turned round explicitly.
    // Turning it back to writing afterwards is refused: the decoder would have to
    // know where the read left off in units.
    template <typename T>
    void expect_turning_around_is_refused(T& obj)
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

        EXPECT_EQ(obj.device().str(), as_char32_units("abcdefgh") + u8"1234578");
    }

    // A stream being read cannot be written to at all: the unit offset of the
    // current character is not known.
    template <typename T>
    void expect_writing_is_refused_while_reading(T& obj, const std::u8string& info)
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

TEST(CodeCvtMemChar8, TurningAroundIsRefused)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_turning_around_is_refused(obj);
}

TEST(CodeCvtMemChar8, TurningAroundIsRefusedThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_turning_around_is_refused(obj);
}

TEST(CodeCvtMemChar8, WritingIsRefusedWhileReading)
{
    const std::u8string              info = as_char32_units("abcdefgh") + u8"12345";
    code_cvt_creator<char8_t, char32_t> creator;
    auto obj = creator.create(no_rb_root_cvt{mem_device(info)});
    expect_writing_is_refused_while_reading(obj, info);
}

TEST(CodeCvtMemChar8, WritingIsRefusedWhileReadingThroughARuntimeCvt)
{
    const std::u8string              info = as_char32_units("abcdefgh") + u8"12345";
    code_cvt_creator<char8_t, char32_t> creator;
    runtime_cvt obj(creator.create(no_rb_root_cvt{mem_device(info)}));
    expect_writing_is_refused_while_reading(obj, info);
}

// UTF-8 has no shift state: a decoder never needs to remember anything that is
// not in the bytes themselves, which is what lets a copy taken mid-stream resume
// exactly where the original was.
TEST(CodeCvtMemChar8, TheUtf8KernelIsNotStateDependent)
{
    codecvt_kernel<char8_t, char32_t> k;
    EXPECT_FALSE(k.is_state_dep());
}

namespace
{
    // é U+00E9 = C3 A9, ñ U+00F1 = C3 B1, µ U+00B5 = C2 B5, then a one-byte 'A':
    // the two-byte encoding paths, with a single-byte character behind them so the
    // decoder has to come back out of the two-byte branch cleanly.
    std::u8string two_byte_sample()
    {
        std::u8string out;
        out += char8_t(0xC3); out += char8_t(0xA9);
        out += char8_t(0xC3); out += char8_t(0xB1);
        out += char8_t(0xC2); out += char8_t(0xB5);
        out += char8_t(0x41);
        return out;
    }

    // 😀 U+1F600 = F0 9F 98 80, 💩 U+1F4A9 = F0 9F 92 A9, then 'B': the four-byte
    // paths, which are the only ones that reach outside the basic plane.
    std::u8string four_byte_sample()
    {
        std::u8string out;
        out += char8_t(0xF0); out += char8_t(0x9F);
        out += char8_t(0x98); out += char8_t(0x80);
        out += char8_t(0xF0); out += char8_t(0x9F);
        out += char8_t(0x92); out += char8_t(0xA9);
        out += char8_t(0x42);
        return out;
    }

    template <typename T>
    void expect_decodes(T& obj, const std::u32string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();
        std::u32string out(expected.size(), 0);
        EXPECT_EQ(obj.get(out.data(), expected.size()), expected.size());
        EXPECT_EQ(out, expected);
    }

    template <typename T>
    void expect_encodes(T& obj, const std::u32string& source, const std::u8string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put(source.data(), source.size());
        obj.flush();
        EXPECT_EQ(obj.device().str(), expected);
    }
}

TEST(CodeCvtMemChar8, TwoByteSequencesDecode)
{
    RbCvt obj{rb_root_cvt{mem_device(two_byte_sample())}};
    expect_decodes(obj, std::u32string{U'é', U'ñ', U'µ', U'A'});
}

TEST(CodeCvtMemChar8, TwoByteSequencesDecodeThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(two_byte_sample())}}};
    expect_decodes(obj, std::u32string{U'é', U'ñ', U'µ', U'A'});
}

TEST(CodeCvtMemChar8, TwoByteSequencesEncode)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_encodes(obj, std::u32string{U'é', U'ñ', U'µ', U'A'}, two_byte_sample());
}

TEST(CodeCvtMemChar8, TwoByteSequencesEncodeThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(std::u8string{})}}};
    expect_encodes(obj, std::u32string{U'é', U'ñ', U'µ', U'A'}, two_byte_sample());
}

TEST(CodeCvtMemChar8, FourByteSequencesDecode)
{
    RbCvt obj{rb_root_cvt{mem_device(four_byte_sample())}};
    expect_decodes(obj, std::u32string{U'\U0001F600', U'\U0001F4A9', U'B'});
}

TEST(CodeCvtMemChar8, FourByteSequencesDecodeThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(four_byte_sample())}}};
    expect_decodes(obj, std::u32string{U'\U0001F600', U'\U0001F4A9', U'B'});
}

TEST(CodeCvtMemChar8, FourByteSequencesEncode)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_encodes(obj, std::u32string{U'\U0001F600', U'\U0001F4A9', U'B'}, four_byte_sample());
}

TEST(CodeCvtMemChar8, FourByteSequencesEncodeThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(std::u8string{})}}};
    expect_encodes(obj, std::u32string{U'\U0001F600', U'\U0001F4A9', U'B'}, four_byte_sample());
}

// UTF-8 has no encoding for the surrogate range or for anything above U+10FFFF,
// so the encoder refuses those rather than emitting something a decoder would
// then reject.
TEST(CodeCvtMemChar8, TheEncoderRejectsCodePointsUtf8CannotRepresent)
{
    for (char32_t bad : {static_cast<char32_t>(0xD800U),
                         static_cast<char32_t>(0xDFFFU),
                         static_cast<char32_t>(0x110000U)})
    {
        SCOPED_TRACE(static_cast<unsigned long>(bad));
        RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        EXPECT_THROW(obj.put(&bad, 1), cvt_error);
    }
}

// A rejected code point must not leave behind the four units put_buf reserved for
// it: the cvt_writer specialization for mem_device reserves straight inside the
// device and commit() is a no-op, so without the put_buf_guard in put_main they
// would stay there as filler.
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

TEST(CodeCvtMemChar8, AFailedEncodeOfASurrogateLeavesTheDeviceEmpty)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_a_failed_encode_leaves_the_device_empty(obj, static_cast<char32_t>(0xD800U));
}

TEST(CodeCvtMemChar8, AFailedEncodeOfASurrogateLeavesTheDeviceEmptyWithoutAReadBuffer)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device(std::u8string{})}};
    expect_a_failed_encode_leaves_the_device_empty(obj, static_cast<char32_t>(0xD800U));
}

TEST(CodeCvtMemChar8, AFailedEncodeOfAnOutOfRangeCodePointLeavesTheDeviceEmpty)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_a_failed_encode_leaves_the_device_empty(obj, static_cast<char32_t>(0x110000U));
}

TEST(CodeCvtMemChar8, AFailedEncodeOfAnOutOfRangeCodePointLeavesTheDeviceEmptyWithoutAReadBuffer)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device(std::u8string{})}};
    expect_a_failed_encode_leaves_the_device_empty(obj, static_cast<char32_t>(0x110000U));
}

// With a multi-byte code point in front, the device must end exactly at its last
// unit, with no remnant of the rejected one.
TEST(CodeCvtMemChar8, AFailedEncodeLeavesTheEarlierOutputUntouched)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device(std::u8string{})}};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const char32_t ok[] = {U'a', U'李'};
    obj.put(ok, 2);

    const char32_t bad = static_cast<char32_t>(0xD800U);
    EXPECT_THROW(obj.put(&bad, 1), cvt_error);
    EXPECT_EQ(obj.device().str(), std::u8string{u8"a李"});
}

// Every way a UTF-8 sequence can be malformed, one per case: a continuation byte
// where a start byte belongs, a continuation that is not one, an overlong form, a
// surrogate spelled out in three bytes, a code point above U+10FFFF, and a start
// byte no UTF-8 sequence ever begins with.
TEST(CodeCvtMemChar8, EveryMalformedSequenceIsRejected)
{
    const std::u8string bad_inputs[] = {
        {char8_t(0x80)},                                              // continuation as start
        {char8_t(0xC3), char8_t(0x20)},                               // bad 2-byte continuation
        {char8_t(0xC0), char8_t(0x80)},                               // overlong null
        {char8_t(0xE6), char8_t(0x20), char8_t(0x8E)},                // bad 3-byte continuation
        {char8_t(0xE6), char8_t(0x9D), char8_t(0x20)},                // bad 3-byte continuation
        {char8_t(0xED), char8_t(0xA0), char8_t(0x80)},                // U+D800 as 3 bytes
        {char8_t(0xF0), char8_t(0x20), char8_t(0x80), char8_t(0x80)}, // bad 4-byte continuation
        {char8_t(0xF4), char8_t(0x90), char8_t(0x80), char8_t(0x80)}, // above U+10FFFF
        {char8_t(0xF0), char8_t(0x80), char8_t(0x80), char8_t(0x80)}, // overlong 4-byte null
        {char8_t(0xFF)},                                              // never a start byte
    };

    for (std::size_t i = 0; i < std::size(bad_inputs); ++i)
    {
        SCOPED_TRACE(i);
        RbCvt obj{rb_root_cvt{mem_device(bad_inputs[i])}};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        char32_t buf[4];
        EXPECT_THROW((void)obj.get(buf, 4), cvt_error);
    }
}

// A stream that ends part-way through a sequence is a different failure from a
// malformed one, and the decoder has to reach it from both the two-byte and the
// four-byte branch.
TEST(CodeCvtMemChar8, AStreamEndingMidSequenceIsRejected)
{
    const std::u8string partials[] = {
        {char8_t(0xC3)},                                // first of two
        {char8_t(0xF0), char8_t(0x9F), char8_t(0x98)},  // three of four
    };

    for (std::size_t i = 0; i < std::size(partials); ++i)
    {
        SCOPED_TRACE(i);
        RbCvt obj{rb_root_cvt{mem_device(partials[i])}};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        char32_t buf[4];
        EXPECT_THROW((void)obj.get(buf, 1), cvt_error);
    }
}

// switch_to_put on a converter that is already writing changes nothing; after an
// attach() the converter is back to neutral, and the same call is what settles
// the direction, standing in for bos().
TEST(CodeCvtMemChar8, SwitchToPutIsANoopWhileWritingAndSettlesTheDirectionAfterAttach)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};

    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    char32_t ch = U'X';
    obj.put(&ch, 1);

    obj.switch_to_put();
    EXPECT_EQ(obj.tell(), 1u);

    auto [old_dev, old_err] = obj.detach();
    obj.attach(mem_device(std::u8string{}));
    EXPECT_GE(old_dev.str().size(), 1u);

    obj.switch_to_put();
    obj.main_cont_beg();
    obj.put(&ch, 1);
    obj.flush();
    EXPECT_EQ(obj.device().str().size(), 1u); // 'X' is one UTF-8 unit
}

TEST(CodeCvtMemChar8, SwitchToGetIsANoopWhileReadingAndSettlesTheDirectionAfterAttach)
{
    const std::u8string content = {char8_t(0xC3), char8_t(0xA9)}; // é

    RbCvt obj{rb_root_cvt{mem_device(content)}};

    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    obj.switch_to_get();

    char32_t ch = 0;
    EXPECT_EQ(obj.get(&ch, 1), 1u);
    EXPECT_EQ(ch, U'é');

    obj.attach(mem_device(content));

    obj.switch_to_get();
    obj.main_cont_beg();
    ch = 0;
    EXPECT_EQ(obj.get(&ch, 1), 1u);
    EXPECT_EQ(ch, U'é');
}

// The encode/decode helpers take a range and must reject one that is inverted --
// a caller error that would otherwise read or write outside the buffer.
TEST(CodeCvtMemChar8, TheEncodeHelperRejectsAnInvertedRange)
{
    codecvt_kernel<char8_t, char32_t> kernel;

    char8_t  buf[4];
    char8_t* to     = buf + 2;
    char8_t* to_end = buf;
    EXPECT_THROW((void)kernel.out_helper(U'A', to, to_end), cvt_error);
}

// The helper reserves room for the widest sequence it might produce, so a buffer
// with fewer than four units left is reported as full even for a one-unit
// character.
TEST(CodeCvtMemChar8, TheEncodeHelperReportsABufferTooSmallForTheWidestSequence)
{
    codecvt_kernel<char8_t, char32_t> kernel;

    char8_t  buf[2];
    char8_t* to     = buf;
    char8_t* to_end = buf + 2;
    EXPECT_FALSE(kernel.out_helper(U'A', to, to_end));
}

TEST(CodeCvtMemChar8, TheDecodeHelperRejectsAnInvertedRange)
{
    codecvt_kernel<char8_t, char32_t> kernel;

    const char8_t  input[]  = u8"hello";
    const char8_t* from     = input + 3;
    const char8_t* from_end = input;
    char32_t       out[4];
    char32_t*      to     = out;
    char32_t*      to_end = out + 4;
    EXPECT_THROW(kernel.in_helper(from, from_end, to, to_end), cvt_error);
}

// The standalone conversion helpers the rest of the library uses for names and
// messages, rather than for streams.
TEST(CvtFacilities, Utf8ToUtf32)
{
    EXPECT_EQ(detail::to_u32string(u8"hello"), U"hello");

    auto r = detail::to_u32string(u8"李伟");
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0], U'李');
    EXPECT_EQ(r[1], U'伟');

    EXPECT_TRUE(detail::to_u32string(u8"").empty());
}

TEST(CvtFacilities, Utf32ToUtf8)
{
    EXPECT_EQ(detail::to_u8string(U"hello"), std::u8string(u8"hello"));
    EXPECT_EQ(detail::to_u8string(std::u32string{U'李', U'伟'}), std::u8string(u8"李伟"));
    EXPECT_TRUE(detail::to_u8string(std::u32string{}).empty());
}

TEST(CvtFacilities, ASingleCodePointToUtf8)
{
    EXPECT_EQ(detail::to_u8string(U'A'), std::u8string(u8"A"));
    EXPECT_EQ(detail::to_u8string(U'李'), std::u8string(u8"李"));
}

TEST(CvtFacilities, NarrowToWideThroughANamedLocale)
{
    EXPECT_EQ(detail::to_wstring("hello", "C"), L"hello");
    EXPECT_TRUE(detail::to_wstring("", "C").empty());
}

TEST(CvtFacilities, NarrowToUtf32ThroughANamedLocale)
{
    EXPECT_EQ(detail::to_u32string("hello", "C"), U"hello");
    EXPECT_TRUE(detail::to_u32string("", "C").empty());
}

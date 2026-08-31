#include <common/defs.h>
#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

// The same cases as code_cvt_mem_char8_t.cpp with wchar_t as the internal type
// instead of char32_t. On this platform the two are the same width, so what this
// file adds is that the UTF-8 kernel is selected from the internal type rather
// than hard-wired to char32_t: everything below has to behave identically.
namespace
{
    using RbCvt   = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    using NoRbCvt = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, wchar_t>;

    constexpr std::size_t kExtSize = 4102;          // char8_t on the device
    constexpr std::size_t kIntSize = 4102 / 7 * 3;  // wchar_t they decode to

    // 586 repetitions of the UTF-8 for L'李' (3 bytes) and L'伟' (3 bytes) plus one
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

    std::wstring internal_sample()
    {
        std::wstring out;
        out.reserve(kIntSize);
        for (std::size_t i = 0; i < kIntSize; i += 3)
        {
            out.push_back(L'李');
            out.push_back(L'伟');
            out.push_back((i / 3) % 127 + 1);
        }
        return out;
    }

    constexpr std::size_t kChunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

    // Little-endian wchar_t code units written as UTF-8 bytes -- what these tests
    // use as a fixed, seekable prologue: every character is four units wide, so a
    // character index and a unit offset differ by a constant.
    std::u8string as_wide_units(const char* ascii)
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

    void expect_decodes_to_the_sample(const std::vector<wchar_t>& out_buf)
    {
        auto it = out_buf.begin();
        for (std::size_t i = 0; i < out_buf.size(); i += 3)
        {
            EXPECT_EQ(*it++, L'李');
            EXPECT_EQ(*it++, L'伟');
            EXPECT_EQ(*it++, static_cast<wchar_t>((i / 3) % 127 + 1));
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

        std::vector<wchar_t> out_buf(kExtSize);
        std::size_t           total = 0;
        wchar_t*             cur   = out_buf.data();
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
        const std::wstring i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total = 0;
        const wchar_t* cur   = i_lit.data();
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

        std::vector<wchar_t> out_buf(kExtSize);
        std::size_t           total = 0;
        wchar_t*             cur   = out_buf.data();
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
        const std::wstring i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total = 0;
        const wchar_t* cur   = i_lit.data();
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
        const std::wstring i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());

        EXPECT_EQ(obj.device().str(), external_sample());
    }
}

TEST(CodeCvtMemChar8Wchar, TraitsOverAMemDeviceOfChar8)
{
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(CodeCvtMemChar8Wchar, TraitsWithoutAReadBuffer)
{
    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);
}

// The decoder carries state between chunks -- a multi-byte sequence can be split
// across two get() calls -- so a converter that is copied or moved between every
// chunk has to carry that state with it.
TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesACopyBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesAMoveBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesACopyBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(std::u8string{})}}};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesAMoveBetweenEveryChunk)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_chunked_put_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
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
    void expect_a_read_prologue_of_three_characters(T& obj, wchar_t c1, wchar_t c2, wchar_t c3)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        wchar_t c = 0;
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
    void expect_a_write_prologue(T& obj, const wchar_t* text, const std::u8string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.put(text, 3);
        obj.main_cont_beg();

        EXPECT_EQ(obj.tell(), 0u);

        const auto& dev = obj.device();
        EXPECT_EQ(dev.dtell(), 12u);
        EXPECT_EQ(dev.str(), expected);
    }

    // Little-endian wchar_t units for L'李' L'd' L'伟'.
    std::u8string li_d_wei_units()
    {
        std::u8string info;
        info += u8'\x4e'; info += u8'\x67'; info += u8'\x00'; info += u8'\x00';
        info += u8'd';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'\x1f'; info += u8'\x4f'; info += u8'\x00'; info += u8'\x00';
        return info;
    }
}

TEST(CodeCvtMemChar8Wchar, AnEmptyPrologueLeavesBothPositionsAtZero)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
}

TEST(CodeCvtMemChar8Wchar, AnEmptyPrologueLeavesBothPositionsAtZeroThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfAsciiCharacters)
{
    RbCvt obj(rb_root_cvt{mem_device(as_wide_units("12345"))});
    expect_a_read_prologue_of_three_characters(obj, L'1', L'2', L'3');
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfAsciiCharactersThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(as_wide_units("12345"))}}};
    expect_a_read_prologue_of_three_characters(obj, L'1', L'2', L'3');
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfMixedWidthCharacters)
{
    RbCvt obj(rb_root_cvt{mem_device(li_d_wei_units() + as_wide_units("cpp"))});
    expect_a_read_prologue_of_three_characters(obj, L'李', L'd', L'伟');
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(li_d_wei_units() + as_wide_units("cpp"))}}};
    expect_a_read_prologue_of_three_characters(obj, L'李', L'd', L'伟');
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfAsciiCharacters)
{
    RbCvt          obj(rb_root_cvt{mem_device(u8"")});
    const wchar_t text[] = L"123";
    expect_a_write_prologue(obj, text, as_wide_units("123"));
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfAsciiCharactersThroughARuntimeCvt)
{
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    const wchar_t text[] = L"123";
    expect_a_write_prologue(obj, text, as_wide_units("123"));
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfMixedWidthCharacters)
{
    RbCvt          obj(rb_root_cvt{mem_device(u8"")});
    const wchar_t text[] = L"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_units());
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    const wchar_t text[] = L"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_units());
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSample)
{
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSampleThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_reads_the_sample(obj);
}

// The same read with no read-back buffer under the converter: a split multi-byte
// sequence cannot be re-read from the kernel, so the decoder has to hold the
// partial state itself.
TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSampleWithoutAReadBuffer)
{
    NoRbCvt obj{no_rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    runtime_cvt obj{NoRbCvt{no_rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_reads_the_sample(obj);
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutEncodesTheWholeSample)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_chunked_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutEncodesTheWholeSampleThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_chunked_put_writes_the_sample(obj);
}

// One put() of everything must produce the same units as the chunked one.
TEST(CodeCvtMemChar8Wchar, AWholePutEncodesTheSameUnitsAsAChunkedOne)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_whole_put_writes_the_sample(obj);
}

TEST(CodeCvtMemChar8Wchar, AWholePutEncodesTheSameUnitsAsAChunkedOneThroughARuntimeCvt)
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
        const std::wstring i_lit    = internal_sample();
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
        const std::wstring i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total = 0;
        const wchar_t* cur   = i_lit.data();
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

TEST(CodeCvtMemChar8Wchar, FlushCompletesTheEncoding)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar8Wchar, FlushCompletesTheEncodingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_flush_completes_the_encoding(obj);
}

TEST(CodeCvtMemChar8Wchar, AFlushAfterEveryChunkReachesTheDevice)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_a_flush_after_every_chunk_reaches_the_device(obj);
}

TEST(CodeCvtMemChar8Wchar, AFlushAfterEveryChunkReachesTheDeviceThroughARuntimeCvt)
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

        std::wstring str(6, L'\0');
        EXPECT_EQ(obj.get(str.data(), 6), 6u);
        EXPECT_EQ(str, L"123456");
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

        std::wstring str(6, L'\0');
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
        wchar_t ch = L'李';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 1u);
        ch = L'x';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 2u);
        ch = L'伟';
        obj.put(&ch, 1);
        EXPECT_EQ(obj.tell(), 3u);

        EXPECT_ANY_THROW(reposition(obj, 100));
        EXPECT_EQ(obj.tell(), 3u);
        EXPECT_ANY_THROW(reposition(obj, 1));
        EXPECT_EQ(obj.tell(), 3u);
        EXPECT_ANY_THROW(reposition(obj, 0));
        EXPECT_EQ(obj.tell(), 3u);

        const wchar_t c[] = L"xy";
        obj.put(c, 2);
        obj.flush();

        EXPECT_EQ(obj.device().str(), std::u8string(u8"李x伟xy"));
    }
}

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileReading)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"123456")});
    expect_seek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileReadingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"123456")}}};
    expect_seek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileWriting)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.seek(n); });
}

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileWritingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.seek(n); });
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileReading)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"123456")});
    expect_rseek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileReadingThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"123456")}}};
    expect_rseek_is_refused_while_reading(obj);
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileWriting)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.rseek(n); });
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileWritingThroughARuntimeCvt)
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

        const wchar_t bos_str[] = L"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        const wchar_t content1[] = L"12345";
        obj.put(content1, 5);
        EXPECT_ANY_THROW(obj.seek(0));

        std::wstring get_content(3, L'\0');
        EXPECT_EQ(obj.get(get_content.data(), 3), 0u);
        EXPECT_EQ(obj.tell(), 5u);

        const wchar_t content2[] = L"78";
        obj.put(content2, 2);
        EXPECT_EQ(obj.tell(), 7u);
        obj.flush();

        obj.switch_to_get();
        obj.seek(0);
        get_content.resize(3);
        EXPECT_EQ(obj.get(get_content.data(), 3), 3u);
        EXPECT_EQ(get_content, L"123");

        EXPECT_ANY_THROW(obj.switch_to_put());

        EXPECT_EQ(obj.device().str(), as_wide_units("abcdefgh") + u8"1234578");
    }

    // A stream being read cannot be written to at all: the unit offset of the
    // current character is not known.
    template <typename T>
    void expect_writing_is_refused_while_reading(T& obj, const std::u8string& info)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        std::wstring bos_str(8, L'\0');
        EXPECT_EQ(obj.get(bos_str.data(), 8), 8u);
        EXPECT_EQ(bos_str, L"abcdefgh");
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        std::wstring content_str(2, L'\0');
        EXPECT_EQ(obj.get(content_str.data(), 2), 2u);
        EXPECT_EQ(content_str, L"12");

        const wchar_t content1[] = L"67";
        EXPECT_ANY_THROW(obj.put(content1, 2));
        EXPECT_EQ(obj.tell(), 2u);

        obj.seek(0);
        EXPECT_EQ(obj.tell(), 0u);

        content_str.resize(10);
        EXPECT_EQ(obj.get(content_str.data(), 10), 5u);
        EXPECT_EQ(content_str.substr(0, 5), L"12345");

        EXPECT_EQ(obj.device().str(), info);
    }
}

TEST(CodeCvtMemChar8Wchar, TurningAroundIsRefused)
{
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_turning_around_is_refused(obj);
}

TEST(CodeCvtMemChar8Wchar, TurningAroundIsRefusedThroughARuntimeCvt)
{
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_turning_around_is_refused(obj);
}

TEST(CodeCvtMemChar8Wchar, WritingIsRefusedWhileReading)
{
    const std::u8string              info = as_wide_units("abcdefgh") + u8"12345";
    code_cvt_creator<char8_t, wchar_t> creator;
    auto obj = creator.create(no_rb_root_cvt{mem_device(info)});
    expect_writing_is_refused_while_reading(obj, info);
}

TEST(CodeCvtMemChar8Wchar, WritingIsRefusedWhileReadingThroughARuntimeCvt)
{
    const std::u8string              info = as_wide_units("abcdefgh") + u8"12345";
    code_cvt_creator<char8_t, wchar_t> creator;
    runtime_cvt obj(creator.create(no_rb_root_cvt{mem_device(info)}));
    expect_writing_is_refused_while_reading(obj, info);
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/mem_device.h>

#include <support/platform_macros.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

// The same cases as test_code_cvt_mem_char8_t.cpp with wchar_t as the internal type
// instead of char32_t. On glibc the two are the same width, so what this
// file adds is that the UTF-8 kernel is selected from the internal type rather
// than hard-wired to char32_t: everything below has to behave identically.
#if IOV2_TEST_LINUX_GLIBC
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
#endif

TEST(CodeCvtMemChar8Wchar, TraitsOverAMemDeviceOfChar8)
{
#if IOV2_TEST_LINUX_GLIBC
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, TraitsWithoutAReadBuffer)
{
#if IOV2_TEST_LINUX_GLIBC
    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

// The decoder carries state between chunks -- a multi-byte sequence can be split
// across two get() calls -- so a converter that is copied or moved between every
// chunk has to carry that state with it.
TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesACopyBetweenEveryChunk)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesAMoveBetweenEveryChunk)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesACopyBetweenEveryChunk)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesACopyBetweenEveryChunkThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(std::u8string{})}}};
    expect_chunked_put_survives(obj, [](auto& src) { return src; },
                                 [](auto& dst, auto& src) { dst = src; });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesAMoveBetweenEveryChunk)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    expect_chunked_put_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(std::u8string{})}}};
    expect_chunked_put_survives(obj, [](auto& src) { return std::move(src); },
                                 [](auto& dst, auto& src) { dst = std::move(src); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

#if IOV2_TEST_LINUX_GLIBC
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
#endif

TEST(CodeCvtMemChar8Wchar, AnEmptyPrologueLeavesBothPositionsAtZero)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AnEmptyPrologueLeavesBothPositionsAtZeroThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_an_empty_prologue_leaves_both_positions_at_zero(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfAsciiCharacters)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(as_wide_units("12345"))});
    expect_a_read_prologue_of_three_characters(obj, L'1', L'2', L'3');
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfAsciiCharactersThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(as_wide_units("12345"))}}};
    expect_a_read_prologue_of_three_characters(obj, L'1', L'2', L'3');
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfMixedWidthCharacters)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(li_d_wei_units() + as_wide_units("cpp"))});
    expect_a_read_prologue_of_three_characters(obj, L'李', L'd', L'伟');
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AReadPrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(li_d_wei_units() + as_wide_units("cpp"))}}};
    expect_a_read_prologue_of_three_characters(obj, L'李', L'd', L'伟');
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfAsciiCharacters)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt          obj(rb_root_cvt{mem_device(u8"")});
    const wchar_t text[] = L"123";
    expect_a_write_prologue(obj, text, as_wide_units("123"));
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfAsciiCharactersThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    const wchar_t text[] = L"123";
    expect_a_write_prologue(obj, text, as_wide_units("123"));
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfMixedWidthCharacters)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt          obj(rb_root_cvt{mem_device(u8"")});
    const wchar_t text[] = L"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_units());
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AWritePrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt    obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    const wchar_t text[] = L"李d伟";
    expect_a_write_prologue(obj, text, li_d_wei_units());
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSample)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj{rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_reads_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSampleThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_reads_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

// The same read with no read-back buffer under the converter: a split multi-byte
// sequence cannot be re-read from the kernel, so the decoder has to hold the
// partial state itself.
TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSampleWithoutAReadBuffer)
{
#if IOV2_TEST_LINUX_GLIBC
    NoRbCvt obj{no_rb_root_cvt{mem_device(external_sample())}};
    expect_chunked_get_reads_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedGetDecodesTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{NoRbCvt{no_rb_root_cvt{mem_device(external_sample())}}};
    expect_chunked_get_reads_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutEncodesTheWholeSample)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_chunked_put_writes_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, ChunkedPutEncodesTheWholeSampleThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_chunked_put_writes_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

// One put() of everything must produce the same units as the chunked one.
TEST(CodeCvtMemChar8Wchar, AWholePutEncodesTheSameUnitsAsAChunkedOne)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_whole_put_writes_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AWholePutEncodesTheSameUnitsAsAChunkedOneThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_whole_put_writes_the_sample(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

#if IOV2_TEST_LINUX_GLIBC
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
#endif

TEST(CodeCvtMemChar8Wchar, FlushCompletesTheEncoding)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_flush_completes_the_encoding(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, FlushCompletesTheEncodingThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_flush_completes_the_encoding(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AFlushAfterEveryChunkReachesTheDevice)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_a_flush_after_every_chunk_reaches_the_device(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, AFlushAfterEveryChunkReachesTheDeviceThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_a_flush_after_every_chunk_reaches_the_device(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

#if IOV2_TEST_LINUX_GLIBC
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
#endif

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileReading)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"123456")});
    expect_seek_is_refused_while_reading(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileReadingThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"123456")}}};
    expect_seek_is_refused_while_reading(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileWriting)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.seek(n); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, SeekIsRefusedWhileWritingThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.seek(n); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileReading)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"123456")});
    expect_rseek_is_refused_while_reading(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileReadingThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"123456")}}};
    expect_rseek_is_refused_while_reading(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileWriting)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.rseek(n); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, RseekIsRefusedWhileWritingThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_repositioning_is_refused_while_writing(
        obj, [](auto& o, std::size_t n) { o.rseek(n); });
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

#if IOV2_TEST_LINUX_GLIBC
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
#endif

TEST(CodeCvtMemChar8Wchar, TurningAroundIsRefused)
{
#if IOV2_TEST_LINUX_GLIBC
    RbCvt obj(rb_root_cvt{mem_device(u8"")});
    expect_turning_around_is_refused(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, TurningAroundIsRefusedThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    runtime_cvt obj{RbCvt{rb_root_cvt{mem_device(u8"")}}};
    expect_turning_around_is_refused(obj);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, WritingIsRefusedWhileReading)
{
#if IOV2_TEST_LINUX_GLIBC
    const std::u8string              info = as_wide_units("abcdefgh") + u8"12345";
    code_cvt_creator<char8_t, wchar_t> creator;
    auto obj = creator.create(no_rb_root_cvt{mem_device(info)});
    expect_writing_is_refused_while_reading(obj, info);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

TEST(CodeCvtMemChar8Wchar, WritingIsRefusedWhileReadingThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    const std::u8string              info = as_wide_units("abcdefgh") + u8"12345";
    code_cvt_creator<char8_t, wchar_t> creator;
    runtime_cvt obj(creator.create(no_rb_root_cvt{mem_device(info)}));
    expect_writing_is_refused_while_reading(obj, info);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-32 (glibc)";
#endif
}

// UTF-8 <-> 16-bit wchar_t: a character outside the BMP is a surrogate pair inside,
// and the pair can be cut by any call boundary.
#if IOV2_TEST_WINDOWS
namespace
{
    using RbCvt   = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    using NoRbCvt = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, wchar_t>;

    std::u8string bytes(std::initializer_list<unsigned> list)
    {
        std::u8string out;
        for (const unsigned b : list)
            out += static_cast<char8_t>(b);
        return out;
    }

    std::wstring units(std::initializer_list<unsigned> list)
    {
        std::wstring out;
        for (const unsigned u : list)
            out += static_cast<wchar_t>(u);
        return out;
    }

    // a, U+1F600, U+674E, U+10348, z: two pairs around a BMP character.
    const std::u8string kBytes = bytes({0x61, 0xF0, 0x9F, 0x98, 0x80, 0xE6, 0x9D, 0x8E,
                                        0xF0, 0x90, 0x8D, 0x88, 0x7A});
    const std::wstring  kUnits = units({0x61, 0xD83D, 0xDE00, 0x674E, 0xD800, 0xDF48, 0x7A});

    template <typename T>
    std::wstring get_in_chunks(T& obj, std::size_t chunk)
    {
        std::wstring got;
        std::wstring buf(chunk, L'\0');
        for (;;)
        {
            const std::size_t n = obj.get(buf.data(), chunk);
            if (n == 0)
                break;
            got.append(buf.data(), n);
        }
        return got;
    }

    template <typename TRoot>
    void every_chunk_size_decodes_the_same_units()
    {
        for (std::size_t chunk = 1; chunk <= kUnits.size() + 1; ++chunk)
        {
            code_cvt<TRoot, wchar_t> obj{TRoot{mem_device(kBytes)}};
            ASSERT_EQ(obj.bos(), io_status::input);
            obj.main_cont_beg();
            EXPECT_EQ(get_in_chunks(obj, chunk), kUnits) << "chunk " << chunk;
            EXPECT_TRUE(obj.is_eof());
            EXPECT_EQ(obj.tell(), kUnits.size());
        }
    }

    template <typename TRoot>
    void every_chunk_size_encodes_the_same_bytes()
    {
        for (std::size_t chunk = 1; chunk <= kUnits.size() + 1; ++chunk)
        {
            code_cvt<TRoot, wchar_t> obj{TRoot{mem_device(std::u8string{})}};
            ASSERT_EQ(obj.bos(), io_status::output);
            obj.main_cont_beg();
            for (std::size_t i = 0; i < kUnits.size(); i += chunk)
                obj.put(kUnits.data() + i, std::min(chunk, kUnits.size() - i));
            EXPECT_EQ(obj.tell(), kUnits.size());
            auto [dev, err] = obj.detach();
            EXPECT_FALSE(err);
            EXPECT_EQ(dev.str(), kBytes) << "chunk " << chunk;
        }
    }
}
#endif

TEST(CodeCvtMemChar8Wchar16, Traits)
{
#if IOV2_TEST_WINDOWS
    static_assert(wchar_t_is_utf16);
    static_assert(io_converter<RbCvt>);
    static_assert(io_converter<NoRbCvt>);
    static_assert(std::is_same_v<RbCvt::internal_type, wchar_t>);
    static_assert(std::is_same_v<RbCvt::external_type, char8_t>);

    code_cvt_access acc;
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    obj.retrieve(acc);
    EXPECT_EQ(acc.code, "(builtin)UTF-8");
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeDecodesTheSameUnits)
{
#if IOV2_TEST_WINDOWS
    every_chunk_size_decodes_the_same_units<rb_root_cvt<mem_device<char8_t>>>();
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeDecodesTheSameUnitsWithoutAReadBuffer)
{
#if IOV2_TEST_WINDOWS
    every_chunk_size_decodes_the_same_units<no_rb_root_cvt<mem_device<char8_t>>>();
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeEncodesTheSameBytes)
{
#if IOV2_TEST_WINDOWS
    every_chunk_size_encodes_the_same_bytes<rb_root_cvt<mem_device<char8_t>>>();
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeEncodesTheSameBytesWithoutAReadBuffer)
{
#if IOV2_TEST_WINDOWS
    every_chunk_size_encodes_the_same_bytes<no_rb_root_cvt<mem_device<char8_t>>>();
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// The input ends in a pair read one unit at a time: the low surrogate is held in the
// converter after the bytes are gone, so the end is not reached until it is handed over.
TEST(CodeCvtMemChar8Wchar16, AHeldLowSurrogateComesOutAfterTheLastByte)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(bytes({0x61, 0xF0, 0x9F, 0x98, 0x80}))}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t c = 0;
    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, L'a');
    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, static_cast<wchar_t>(0xD83D));
    EXPECT_FALSE(obj.is_eof());
    EXPECT_EQ(obj.tell(), 2u);

    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, static_cast<wchar_t>(0xDE00));
    EXPECT_TRUE(obj.is_eof());
    EXPECT_EQ(obj.tell(), 3u);
    EXPECT_EQ(obj.get(&c, 1), 0u);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// seek(0) with a low surrogate held drops it: reading starts over from the first unit.
TEST(CodeCvtMemChar8Wchar16, SeekToTheStartDropsAHeldLowSurrogate)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(kBytes)}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t buf[2]{};
    ASSERT_EQ(obj.get(buf, 2), 2u);
    EXPECT_EQ(buf[1], static_cast<wchar_t>(0xD83D)) << "the read did not stop inside the pair";
    EXPECT_FALSE(obj.is_eof());

    obj.seek(0);
    EXPECT_EQ(obj.tell(), 0u);
    EXPECT_EQ(get_in_chunks(obj, 3), kUnits);
    EXPECT_EQ(obj.tell(), kUnits.size());
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// Half a pair handed over: turning round to writing is refused, the converter unchanged.
TEST(CodeCvtMemChar8Wchar16, TurningRoundIsRefusedInsideAPair)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(bytes({0xF0, 0x9F, 0x98, 0x80}))}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t c = 0;
    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_THROW(obj.switch_to_put(), cvt_error);
    EXPECT_FALSE(obj.is_tainted());

    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, static_cast<wchar_t>(0xDE00));
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// A put that ends in a high surrogate writes nothing for it yet; the low surrogate of
// the next put completes the character.
TEST(CodeCvtMemChar8Wchar16, AHighSurrogateEndingAPutWaitsForTheNext)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    ASSERT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const wchar_t high = static_cast<wchar_t>(0xD83D);
    const wchar_t low  = static_cast<wchar_t>(0xDE00);
    obj.put(&high, 1);
    obj.flush();
    EXPECT_TRUE(obj.device().str().empty());
    EXPECT_THROW(obj.switch_to_get(), cvt_error) << "half a pair let the direction turn";

    obj.put(&low, 1);
    auto [dev, err] = obj.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(dev.str(), bytes({0xF0, 0x9F, 0x98, 0x80}));
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// Closing the stream with the low surrogate never come is an incomplete character.
TEST(CodeCvtMemChar8Wchar16, DetachWithAHighSurrogateHeldReportsIt)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    ASSERT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const std::wstring text = units({0x61, 0xD83D});
    obj.put(text.data(), text.size());
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(err);
    EXPECT_EQ(dev.str(), u8"a");
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

TEST(CodeCvtMemChar8Wchar16, ALoneLowSurrogateCannotBeEncoded)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    ASSERT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const std::wstring text = units({0x61, 0xDE00});
    EXPECT_THROW(obj.put(text.data(), text.size()), cvt_error);
    EXPECT_TRUE(obj.is_tainted());
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

TEST(CodeCvtMemChar8Wchar16, AHighSurrogateNotFollowedByALowOneCannotBeEncoded)
{
#if IOV2_TEST_WINDOWS
    {
        RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
        ASSERT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        const std::wstring text = units({0xD83D, 0x61});
        EXPECT_THROW(obj.put(text.data(), text.size()), cvt_error);
        EXPECT_TRUE(obj.is_tainted());
    }
    {
        // The same with the high surrogate held from the previous put.
        RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
        ASSERT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        const wchar_t high = static_cast<wchar_t>(0xD83D);
        obj.put(&high, 1);
        const wchar_t a = L'a';
        EXPECT_THROW(obj.put(&a, 1), cvt_error);
        EXPECT_TRUE(obj.is_tainted());
    }
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// The UTF-8 side is as strict as with char32_t: an encoded surrogate is invalid.
TEST(CodeCvtMemChar8Wchar16, AnEncodedSurrogateIsAnInvalidSequence)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(bytes({0xED, 0xA0, 0x80}))}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t buf[4]{};
    EXPECT_THROW(obj.get(buf, 4), cvt_error);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// Input cut off inside a four-byte character: nothing comes out, and the read fails.
TEST(CodeCvtMemChar8Wchar16, InputCutOffInsideAPairFailsTheRead)
{
#if IOV2_TEST_WINDOWS
    RbCvt obj{rb_root_cvt{mem_device(bytes({0x61, 0xF0, 0x9F, 0x98}))}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t buf[4]{};
    ASSERT_EQ(obj.get(buf, 4), 1u);
    EXPECT_EQ(buf[0], L'a');
    EXPECT_THROW(obj.get(buf, 4), cvt_error);
#else
    GTEST_SKIP() << "takes wchar_t to be UTF-16 (Windows)";
#endif
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * code_cvt over the standard streams with char32_t inside, which needs
 * wchar_t_is_utf32 (glibc only), and the code_cvt_stdio case that needs a glibc
 * locale name (zh_CN.GBK). The code_cvt_stdio cases over wchar_t are in
 * test_code_cvt_stdio_char.cpp.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/cvt/stdin_root_cvt.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/device/std_device.h>

#include <support/platform_macros.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if IOV2_TEST_LINUX_GLIBC
#include <unistd.h>
#endif

using namespace IOv2;

#if IOV2_TEST_LINUX_GLIBC
namespace
{
    using StdinCvt  = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
    using StdoutCvt = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
    using StderrCvt = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;

    // The converter behind wcin / wcout: code_cvt_stdio accepts standard devices only.
    using StdioIn  = code_cvt_stdio<rb_root_cvt<std_device<STDIN_FILENO>>>;
    using StdioOut = code_cvt_stdio<rb_root_cvt<std_device<STDOUT_FILENO>>>;

    constexpr std::size_t kExtSize = 4102;          // bytes on the descriptor
    constexpr std::size_t kIntSize = 4102 / 7 * 3;  // char32_t they decode to

    // 586 repetitions of the UTF-8 for U'李' (3 bytes) and U'伟' (3 bytes) plus one
    // ASCII byte cycling 1..127. Seven external bytes per three internal
    // characters is what makes the two sizes above differ, and it is why every
    // chunk size below lands mid-character sooner or later.
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
    // four-byte-per-character locale writes to the descriptor.
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

    // A standard stream cannot be rewound, so the whole file drives the converter
    // forward only. `move_between_chunks` hands the converter over between every
    // chunk: the decoder state has to travel with it, and there is no way to
    // recover if it does not.
    template <typename T>
    void expect_chunked_get_reads_the_sample(T& obj, bool move_between_chunks)
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
            std::size_t s = 0;
            if (move_between_chunks)
            {
                T moved(std::move(obj));
                s = moved.get(cur, n);
                if (s != 0) obj = std::move(moved);
            }
            else
            {
                s = obj.get(cur, n);
            }
            id %= std::size(kChunks);
            cur   += s;
            total += s;
            if (s == 0) break;
        }

        ASSERT_EQ(cur - out_buf.data(), static_cast<std::ptrdiff_t>(kIntSize));
        out_buf.resize(kIntSize);
        expect_decodes_to_the_sample(out_buf);
    }

    template <typename T>
    void put_sample_in_chunks(T& obj, bool move_between_chunks)
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
            if (move_between_chunks)
            {
                T moved(std::move(obj));
                moved.put(cur, n);
                obj = std::move(moved);
            }
            else
            {
                obj.put(cur, n);
            }
            id %= std::size(kChunks);
            cur   += n;
            total += n;
        }
        obj.flush();
    }

    template <typename T>
    void put_sample_whole(T& obj)
    {
        const std::u32string i_lit = internal_sample();

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
    }
}
#endif

TEST(CodeCvtStdio, TraitsOverStdin)
{
#if IOV2_TEST_LINUX_GLIBC
    using CheckType = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, std_device<STDIN_FILENO>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    // The descriptor decides the direction and cannot be repositioned, so the
    // converter over it loses everything but the one direction it was opened for.
    static_assert(!cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, TraitsOverStdout)
{
#if IOV2_TEST_LINUX_GLIBC
    using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, std_device<STDOUT_FILENO>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(!cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedGetSurvivesAMoveBetweenEveryChunk)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard    g(external_sample());
    StdinCvt  obj{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"};
    expect_chunked_get_reads_the_sample(obj, true);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedGetSurvivesAMoveBetweenEveryChunkThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard      g(external_sample());
    runtime_cvt obj{StdinCvt{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"}};
    expect_chunked_get_reads_the_sample(obj, true);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutSurvivesAMoveBetweenEveryChunkOnStdout)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    StdoutCvt    obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
    put_sample_in_chunks(obj, true);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutSurvivesAMoveBetweenEveryChunkOnStderr)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    StderrCvt     obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
    put_sample_in_chunks(obj, true);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutSurvivesAMoveBetweenEveryChunkOnStdoutThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    runtime_cvt  obj{StdoutCvt{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"}};
    put_sample_in_chunks(obj, true);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutSurvivesAMoveBetweenEveryChunkOnStderrThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    runtime_cvt   obj{StderrCvt{rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8"}};
    put_sample_in_chunks(obj, true);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

// bos() on a descriptor with something waiting has to report input, and
// main_cont_beg() has to complete without a position to record.
TEST(CodeCvtStdio, BosReportsInputOnANonEmptyStdin)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard   g("12345");
    StdinCvt obj(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
    EXPECT_EQ(obj.bos(), io_status::input);
    EXPECT_NO_THROW(obj.main_cont_beg());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, BosReportsInputOnANonEmptyStdinThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard      g("12345");
    runtime_cvt obj{StdinCvt{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"}};
    EXPECT_EQ(obj.bos(), io_status::input);
    EXPECT_NO_THROW(obj.main_cont_beg());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

#if IOV2_TEST_LINUX_GLIBC
namespace
{
    // Three characters read before main_cont_beg() are a prologue. A standard
    // stream has no positions, so the only thing to check is that the decoder
    // hands back the right code points and that the boundary itself is accepted.
    template <typename T>
    void expect_a_read_prologue(T& obj, char32_t c1, char32_t c2, char32_t c3)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        char32_t c = 0;
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, c1);
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, c2);
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, c3);

        EXPECT_NO_THROW(obj.main_cont_beg());
    }

    // Little-endian char32_t bytes for U'李' U'd' U'伟'.
    std::string li_d_wei_bytes()
    {
        std::string info;
        info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
        info += 'd';    info += '\x00'; info += '\x00'; info += '\x00';
        info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
        return info;
    }
}
#endif

TEST(CodeCvtStdio, AReadPrologueOfAsciiCharacters)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard   g(as_char32_bytes("123"));
    StdinCvt obj(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
    expect_a_read_prologue(obj, U'1', U'2', U'3');
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AReadPrologueOfAsciiCharactersThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard      g(as_char32_bytes("123"));
    runtime_cvt obj{StdinCvt{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"}};
    expect_a_read_prologue(obj, U'1', U'2', U'3');
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AReadPrologueOfMixedWidthCharacters)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard   g(li_d_wei_bytes() + as_char32_bytes("cpp"));
    StdinCvt obj(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
    expect_a_read_prologue(obj, U'李', U'd', U'伟');
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AReadPrologueOfMixedWidthCharactersThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard      g(li_d_wei_bytes() + as_char32_bytes("cpp"));
    runtime_cvt obj{StdinCvt{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"}};
    expect_a_read_prologue(obj, U'李', U'd', U'伟');
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

#if IOV2_TEST_LINUX_GLIBC
namespace
{
    // An output stream with no prologue must not put anything on the descriptor
    // before the caller does: bos() and main_cont_beg() are bookkeeping only.
    template <typename T, typename Guard>
    void expect_an_empty_write_prologue(T& obj, Guard& g)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        EXPECT_EQ(g.contents(), "");
    }

    // A prologue written before main_cont_beg() has to be on the descriptor by the
    // time it returns.
    template <typename T, typename Guard>
    void expect_a_write_prologue(T& obj, Guard& g, const char32_t* text,
                                 const std::string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.put(text, 3);
        obj.main_cont_beg();
        EXPECT_EQ(g.contents(), expected);
    }
}
#endif

TEST(CodeCvtStdio, AnEmptyWritePrologueEmitsNothingOnStdout)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    StdoutCvt    obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
    expect_an_empty_write_prologue(obj, g);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AnEmptyWritePrologueEmitsNothingOnStderr)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    StderrCvt     obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
    expect_an_empty_write_prologue(obj, g);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AnEmptyWritePrologueEmitsNothingOnStdoutThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    runtime_cvt  obj{StdoutCvt{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"}};
    expect_an_empty_write_prologue(obj, g);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AnEmptyWritePrologueEmitsNothingOnStderrThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    runtime_cvt   obj{StderrCvt{rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8"}};
    expect_an_empty_write_prologue(obj, g);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfAsciiCharactersOnStdout)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true>   g;
    StdoutCvt      obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, g, text, as_char32_bytes("123"));
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfAsciiCharactersOnStderr)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false>  g;
    StderrCvt      obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, g, text, as_char32_bytes("123"));
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfAsciiCharactersOnStdoutThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true>   g;
    runtime_cvt    obj{StdoutCvt{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"}};
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, g, text, as_char32_bytes("123"));
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfAsciiCharactersOnStderrThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false>  g;
    runtime_cvt    obj{StderrCvt{rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8"}};
    const char32_t text[] = U"123";
    expect_a_write_prologue(obj, g, text, as_char32_bytes("123"));
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfMixedWidthCharactersOnStdout)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true>   g;
    StdoutCvt      obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, g, text, li_d_wei_bytes());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfMixedWidthCharactersOnStderr)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false>  g;
    StderrCvt      obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, g, text, li_d_wei_bytes());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfMixedWidthCharactersOnStdoutThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true>   g;
    runtime_cvt    obj{StdoutCvt{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"}};
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, g, text, li_d_wei_bytes());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWritePrologueOfMixedWidthCharactersOnStderrThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false>  g;
    runtime_cvt    obj{StderrCvt{rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8"}};
    const char32_t text[] = U"李d伟";
    expect_a_write_prologue(obj, g, text, li_d_wei_bytes());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedGetDecodesTheWholeSample)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard   g(external_sample());
    StdinCvt obj{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"};
    expect_chunked_get_reads_the_sample(obj, false);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedGetDecodesTheWholeSampleThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    iguard      g(external_sample());
    runtime_cvt obj{StdinCvt{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"}};
    expect_chunked_get_reads_the_sample(obj, false);
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutEncodesTheWholeSampleOnStdout)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    StdoutCvt    obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
    put_sample_in_chunks(obj, false);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutEncodesTheWholeSampleOnStderr)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    StderrCvt     obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
    put_sample_in_chunks(obj, false);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutEncodesTheWholeSampleOnStdoutThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    runtime_cvt  obj{StdoutCvt{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"}};
    put_sample_in_chunks(obj, false);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, ChunkedPutEncodesTheWholeSampleOnStderrThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    runtime_cvt   obj{StderrCvt{rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8"}};
    put_sample_in_chunks(obj, false);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

// One put() of everything must produce the same bytes as the chunked one.
TEST(CodeCvtStdio, AWholePutEncodesTheSameBytesOnStdout)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    StdoutCvt    obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
    put_sample_whole(obj);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWholePutEncodesTheSameBytesOnStderr)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    StderrCvt     obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
    put_sample_whole(obj);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWholePutEncodesTheSameBytesOnStdoutThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    runtime_cvt  obj{StdoutCvt{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"}};
    put_sample_whole(obj);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

TEST(CodeCvtStdio, AWholePutEncodesTheSameBytesOnStderrThroughARuntimeCvt)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<false> g;
    runtime_cvt   obj{StderrCvt{rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8"}};
    put_sample_whole(obj);
    EXPECT_EQ(g.contents(), external_sample());
#else
    GTEST_SKIP() << "code_cvt into char32_t needs wchar_t_is_utf32 (glibc)";
#endif
}

// Trying another encoding is the natural reaction to an encoding failure; it must
// work right after one, and switch for real.
TEST(CodeCvtStdio, ASwitchRecoversFromATaintAndSwitches)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true> g;
    StdioOut     obj{rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const wchar_t lone_surrogate[] = {L'\xD800'};
    EXPECT_THROW(obj.put(lone_surrogate, 1), cvt_error);
    EXPECT_TRUE(obj.is_tainted());

    EXPECT_NO_THROW(obj.adjust(code_cvt_switch{"zh_CN.GBK"}));
    EXPECT_FALSE(obj.is_tainted());

    code_cvt_access status;
    obj.retrieve(status);
    EXPECT_EQ(status.code, "zh_CN.GBK");

    const wchar_t zhong[] = {L'中'};
    obj.put(zhong, 1);
    obj.flush();
    EXPECT_EQ(g.contents(), "\xd6\xd0"); // 中 in GBK: the new encoding is in effect
#else
    GTEST_SKIP() << "needs glibc's zh_CN.GBK locale";
#endif
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The one case of test_code_cvt_mem_char_char32_t.cpp that puts wchar_t, not
 * char32_t, inside: char <-> wchar_t is available wherever wchar_t is, which
 * char <-> char32_t is not (it needs wchar_t_is_utf32).
 *
 * Then the UTF-16 wchar_t of Windows: under a UTF-8 locale a character outside the
 * BMP is a surrogate pair inside, which any call boundary can cut; under another code
 * page it cannot be encoded at all. Wine's CRT has no UTF-8 locale, so the UTF-8 cases
 * run on a real Windows only.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>
#include <support/platform_macros.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <type_traits>

using namespace IOv2;

#if IOV2_TEST_WINDOWS
namespace
{
    constexpr const char* kUtf8 = ".UTF-8";

    std::string bytes(std::initializer_list<unsigned> list)
    {
        std::string out;
        for (const unsigned b : list)
            out += static_cast<char>(b);
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
    const std::string  kBytes = bytes({0x61, 0xF0, 0x9F, 0x98, 0x80, 0xE6, 0x9D, 0x8E,
                                       0xF0, 0x90, 0x8D, 0x88, 0x7A});
    const std::wstring kUnits = units({0x61, 0xD83D, 0xDE00, 0x674E, 0xD800, 0xDF48, 0x7A});

    template <typename TRoot>
    void every_chunk_size_decodes_the_same_units()
    {
        for (std::size_t chunk = 1; chunk <= kUnits.size() + 1; ++chunk)
        {
            code_cvt<TRoot, wchar_t> obj{TRoot{mem_device(kBytes)}, kUtf8};
            ASSERT_EQ(obj.bos(), io_status::input);
            obj.main_cont_beg();

            std::wstring got;
            std::wstring buf(chunk, L'\0');
            for (std::size_t n; (n = obj.get(buf.data(), chunk)) != 0;)
                got.append(buf.data(), n);
            EXPECT_EQ(got, kUnits) << "chunk " << chunk;
            EXPECT_TRUE(obj.is_eof());
            EXPECT_EQ(obj.tell(), kUnits.size());
        }
    }

    template <typename TRoot>
    void every_chunk_size_encodes_the_same_bytes()
    {
        for (std::size_t chunk = 1; chunk <= kUnits.size() + 1; ++chunk)
        {
            code_cvt<TRoot, wchar_t> obj{TRoot{mem_device(std::string{})}, kUtf8};
            ASSERT_EQ(obj.bos(), io_status::output);
            obj.main_cont_beg();
            for (std::size_t i = 0; i < kUnits.size(); i += chunk)
                obj.put(kUnits.data() + i, std::min(chunk, kUnits.size() - i));
            auto [dev, err] = obj.detach();
            EXPECT_FALSE(err);
            EXPECT_EQ(dev.str(), kBytes) << "chunk " << chunk;
        }
    }
}
#endif

TEST(CodeCvtMemChar32, TraitsWithWcharAsTheInternalType)
{
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
}

TEST(CodeCvtMemCharWchar, AUtf8LocaleDecodesPairsInEveryChunkSize)
{
#if IOV2_TEST_WINDOWS
    every_chunk_size_decodes_the_same_units<rb_root_cvt<mem_device<char>>>();
    every_chunk_size_decodes_the_same_units<no_rb_root_cvt<mem_device<char>>>();
#else
    GTEST_SKIP() << "surrogate pairs are Windows' 16-bit wchar_t";
#endif
}

TEST(CodeCvtMemCharWchar, AUtf8LocaleEncodesPairsCutAtEveryChunkSize)
{
#if IOV2_TEST_WINDOWS
    every_chunk_size_encodes_the_same_bytes<rb_root_cvt<mem_device<char>>>();
    every_chunk_size_encodes_the_same_bytes<no_rb_root_cvt<mem_device<char>>>();
#else
    GTEST_SKIP() << "surrogate pairs are Windows' 16-bit wchar_t";
#endif
}

// The input ends in a pair read one unit at a time: the low surrogate is held after
// the bytes are gone, so the end is not reached until it is handed over.
TEST(CodeCvtMemCharWchar, AUtf8LocaleHandsOverAHeldLowSurrogateAfterTheLastByte)
{
#if IOV2_TEST_WINDOWS
    code_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{
        rb_root_cvt{mem_device(bytes({0x61, 0xF0, 0x9F, 0x98, 0x80}))}, kUtf8};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t c = 0;
    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, L'a');
    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, static_cast<wchar_t>(0xD83D));
    EXPECT_FALSE(obj.is_eof());
    EXPECT_THROW(obj.switch_to_put(), cvt_error) << "half a pair let the direction turn";

    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, static_cast<wchar_t>(0xDE00));
    EXPECT_TRUE(obj.is_eof());
    EXPECT_EQ(obj.get(&c, 1), 0u);
#else
    GTEST_SKIP() << "surrogate pairs are Windows' 16-bit wchar_t";
#endif
}

// A put that ends in a high surrogate writes nothing for it; closing the stream with
// the low surrogate never come is an incomplete character.
TEST(CodeCvtMemCharWchar, AUtf8LocaleReportsAHighSurrogateLeftAtDetach)
{
#if IOV2_TEST_WINDOWS
    code_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device(std::string{})}, kUtf8};
    ASSERT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const std::wstring text = units({0x61, 0xD83D});
    obj.put(text.data(), text.size());
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(err);
    EXPECT_EQ(dev.str(), "a");
#else
    GTEST_SKIP() << "surrogate pairs are Windows' 16-bit wchar_t";
#endif
}

TEST(CodeCvtMemCharWchar, AUtf8LocaleCannotEncodeALoneSurrogate)
{
#if IOV2_TEST_WINDOWS
    for (const std::wstring& text : {units({0x61, 0xDE00}), units({0xD83D, 0x61})})
    {
        code_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device(std::string{})}, kUtf8};
        ASSERT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        EXPECT_THROW(obj.put(text.data(), text.size()), cvt_error);
        EXPECT_TRUE(obj.is_tainted());
    }
#else
    GTEST_SKIP() << "surrogate pairs are Windows' 16-bit wchar_t";
#endif
}

// UTF-8 is judged as strictly as by the char8_t kernel: an encoded surrogate and an
// overlong form are invalid.
TEST(CodeCvtMemCharWchar, AUtf8LocaleRejectsWhatRfc3629Rejects)
{
#if IOV2_TEST_WINDOWS
    for (const std::string& in : {bytes({0xED, 0xA0, 0x80}), bytes({0xC0, 0xAF})})
    {
        code_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device(in)}, kUtf8};
        ASSERT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();
        wchar_t buf[4]{};
        EXPECT_THROW(obj.get(buf, 4), cvt_error);
    }
#else
    GTEST_SKIP() << "the strict path is Windows' UTF-8 locales";
#endif
}

// Code page 936 goes through the CRT as before: BMP characters convert, a surrogate
// pair cannot be encoded.
TEST(CodeCvtMemCharWchar, ACodePageWithoutSurrogatesCannotEncodeAPair)
{
#if IOV2_TEST_WINDOWS
    {
        code_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device(std::string{})}, ".936"};
        ASSERT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        const std::wstring li = units({0x674E});
        obj.put(li.data(), li.size());
        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        EXPECT_EQ(dev.str(), bytes({0xC0, 0xEE}));
    }
    {
        code_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device(std::string{})}, ".936"};
        ASSERT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        const std::wstring pair = units({0xD83D, 0xDE00});
        EXPECT_THROW(obj.put(pair.data(), pair.size()), cvt_error);
    }
#else
    GTEST_SKIP() << "code page 936 is a Windows locale name";
#endif
}

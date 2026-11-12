// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <type_traits>

using namespace IOv2;

// UTF-8 <-> 16-bit wchar_t: a character outside the BMP is a surrogate pair inside,
// and the pair can be cut by any call boundary.
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

TEST(CodeCvtMemChar8Wchar16, Traits)
{
    static_assert(wchar_t_is_utf16);
    static_assert(io_converter<RbCvt>);
    static_assert(io_converter<NoRbCvt>);
    static_assert(std::is_same_v<RbCvt::internal_type, wchar_t>);
    static_assert(std::is_same_v<RbCvt::external_type, char8_t>);

    code_cvt_access acc;
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    obj.retrieve(acc);
    EXPECT_EQ(acc.code, "(builtin)UTF-8");
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeDecodesTheSameUnits)
{
    every_chunk_size_decodes_the_same_units<rb_root_cvt<mem_device<char8_t>>>();
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeDecodesTheSameUnitsWithoutAReadBuffer)
{
    every_chunk_size_decodes_the_same_units<no_rb_root_cvt<mem_device<char8_t>>>();
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeEncodesTheSameBytes)
{
    every_chunk_size_encodes_the_same_bytes<rb_root_cvt<mem_device<char8_t>>>();
}

TEST(CodeCvtMemChar8Wchar16, EveryChunkSizeEncodesTheSameBytesWithoutAReadBuffer)
{
    every_chunk_size_encodes_the_same_bytes<no_rb_root_cvt<mem_device<char8_t>>>();
}

// The input ends in a pair read one unit at a time: the low surrogate is held in the
// converter after the bytes are gone, so the end is not reached until it is handed over.
TEST(CodeCvtMemChar8Wchar16, AHeldLowSurrogateComesOutAfterTheLastByte)
{
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
}

// seek(0) with a low surrogate held drops it: reading starts over from the first unit.
TEST(CodeCvtMemChar8Wchar16, SeekToTheStartDropsAHeldLowSurrogate)
{
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
}

// Half a pair handed over: turning round to writing is refused, the converter unchanged.
TEST(CodeCvtMemChar8Wchar16, TurningRoundIsRefusedInsideAPair)
{
    RbCvt obj{rb_root_cvt{mem_device(bytes({0xF0, 0x9F, 0x98, 0x80}))}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t c = 0;
    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_THROW(obj.switch_to_put(), cvt_error);
    EXPECT_FALSE(obj.is_tainted());

    ASSERT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, static_cast<wchar_t>(0xDE00));
}

// A put that ends in a high surrogate writes nothing for it yet; the low surrogate of
// the next put completes the character.
TEST(CodeCvtMemChar8Wchar16, AHighSurrogateEndingAPutWaitsForTheNext)
{
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
}

// Closing the stream with the low surrogate never come is an incomplete character.
TEST(CodeCvtMemChar8Wchar16, DetachWithAHighSurrogateHeldReportsIt)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    ASSERT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const std::wstring text = units({0x61, 0xD83D});
    obj.put(text.data(), text.size());
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(err);
    EXPECT_EQ(dev.str(), u8"a");
}

TEST(CodeCvtMemChar8Wchar16, ALoneLowSurrogateCannotBeEncoded)
{
    RbCvt obj{rb_root_cvt{mem_device(std::u8string{})}};
    ASSERT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const std::wstring text = units({0x61, 0xDE00});
    EXPECT_THROW(obj.put(text.data(), text.size()), cvt_error);
    EXPECT_TRUE(obj.is_tainted());
}

TEST(CodeCvtMemChar8Wchar16, AHighSurrogateNotFollowedByALowOneCannotBeEncoded)
{
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
}

// The UTF-8 side is as strict as with char32_t: an encoded surrogate is invalid.
TEST(CodeCvtMemChar8Wchar16, AnEncodedSurrogateIsAnInvalidSequence)
{
    RbCvt obj{rb_root_cvt{mem_device(bytes({0xED, 0xA0, 0x80}))}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t buf[4]{};
    EXPECT_THROW(obj.get(buf, 4), cvt_error);
}

// Input cut off inside a four-byte character: nothing comes out, and the read fails.
TEST(CodeCvtMemChar8Wchar16, InputCutOffInsideAPairFailsTheRead)
{
    RbCvt obj{rb_root_cvt{mem_device(bytes({0x61, 0xF0, 0x9F, 0x98}))}};
    ASSERT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t buf[4]{};
    ASSERT_EQ(obj.get(buf, 4), 1u);
    EXPECT_EQ(buf[0], L'a');
    EXPECT_THROW(obj.get(buf, 4), cvt_error);
}

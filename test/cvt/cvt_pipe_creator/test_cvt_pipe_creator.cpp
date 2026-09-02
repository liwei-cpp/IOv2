// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/cvt/crypt/hash_cvt.h>
#include <IOv2/cvt/crypt/vigenere_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/cvt_pipe_creator.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace IOv2;

namespace
{
    // The fixed sample is 4102 external bytes: 586 repetitions of the UTF-8 for
    // U'李' U'伟' (three bytes each) plus one ASCII byte that cycles 1..127, so
    // the stream mixes multi-byte and single-byte sequences and 4102 is not a
    // multiple of any chunk size used below.
    constexpr int         kBytes  = 4102;
    constexpr int         kChars  = 4102 / 7 * 3;
    constexpr const char* kKey    = "abcdefg";

    // Chunk sizes the put/get loops rotate through. They are coprime with the
    // 3-byte UTF-8 sequences on purpose: every call has to land mid-character
    // sooner or later, which is what exercises the pipe's partial-conversion
    // paths.
    constexpr std::size_t kChunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

    std::u32string source_chars()
    {
        std::u32string out;
        out.reserve(kChars);
        for (int i = 0; i < kChars; i += 3)
        {
            out.push_back(U'李');
            out.push_back(U'伟');
            out.push_back((i / 3) % 127 + 1);
        }
        return out;
    }

    // What a vigenere_cvt over the UTF-8 of source_chars() must produce: the key
    // is exactly seven bytes and the pattern repeats every seven bytes, so each
    // position's shift is fixed and the whole stream can be written out directly.
    std::string vigenere_bytes()
    {
        std::string out;
        out.resize(kBytes);
        for (int i = 0; i < kBytes; i += 7)
        {
            out[i + 0] = static_cast<char>('\xE6' + 'a');
            out[i + 1] = static_cast<char>('\x9D' + 'b');
            out[i + 2] = static_cast<char>('\x8E' + 'c');
            out[i + 3] = static_cast<char>('\xE4' + 'd');
            out[i + 4] = static_cast<char>('\xBC' + 'e');
            out[i + 5] = static_cast<char>('\x9F' + 'f');
            out[i + 6] = (i / 7) % 127 + 1 + 'g';
        }
        return out;
    }

    auto vigenere_then_code()
    {
        return Crypt::Classic::vigenere_cvt_creator(kKey) |
               code_cvt_creator<char, char32_t>("zh_CN.UTF-8");
    }

    // Writes source_chars() in rotating chunks and returns the bytes the device
    // ended up holding. With check_tell, tell() is verified after every chunk: it
    // counts internal characters consumed, not external bytes produced.
    template <typename Cvt>
    std::string put_in_chunks(Cvt& obj, bool check_tell)
    {
        const std::u32string i_lit = source_chars();
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t     total    = 0;
        const char32_t* cur      = i_lit.data();
        int             chunk_id = 0;
        while (total < kChars)
        {
            std::size_t n = std::min<std::size_t>(kChars - total, kChunks[chunk_id++]);
            obj.put(cur, n);
            chunk_id %= std::size(kChunks);
            cur   += n;
            total += n;
            // tell() is only declared when every stage supports positioning; a
            // hash stage removes it from the type altogether.
            if constexpr (requires { obj.tell(); })
            {
                if (check_tell) { EXPECT_EQ(obj.tell(), total); }
            }
        }

        auto [dev, err] = obj.detach();
        return dev.str();
    }

    // Reads back in rotating chunks until get() returns 0, then checks the whole
    // decoded stream character by character against the pattern put_in_chunks
    // writes.
    template <typename Cvt>
    void get_in_chunks(Cvt& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::vector<char32_t> out_buf(kBytes);
        std::size_t           total    = 0;
        char32_t*             cur      = out_buf.data();
        int                   chunk_id = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kBytes - total, kChunks[chunk_id++]);
            auto        s = obj.get(cur, n);
            chunk_id %= std::size(kChunks);
            cur   += s;
            total += s;
            EXPECT_EQ(obj.tell(), total);
            if (s == 0) break;
        }

        ASSERT_EQ(cur - out_buf.data(), kChars);
        out_buf.resize(kChars);

        auto it = out_buf.begin();
        for (std::size_t i = 0; i < out_buf.size(); i += 3)
        {
            EXPECT_EQ(*it++, U'李');
            EXPECT_EQ(*it++, U'伟');
            EXPECT_EQ(*it++, static_cast<char32_t>((i / 3) % 127 + 1));
        }
    }

    // Writes the whole sample in one call, then reads it back through a freshly
    // created pipe of the same shape. A pipe that only round-trips chunk by chunk
    // would pass get_in_chunks and still fail here.
    template <typename Cvt, typename Creator>
    void round_trip(Cvt& obj, const Creator& creator)
    {
        const std::u32string i_lit = source_chars();
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());

        auto [dev, err] = obj.detach();

        Cvt            back = creator.create(rb_root_cvt{mem_device(dev.str())});
        std::u32string read_back(kBytes * 2, U'\0');
        EXPECT_EQ(back.bos(), io_status::input);
        back.main_cont_beg();

        EXPECT_EQ(back.get(read_back.data(), kBytes * 2), kChars);
        EXPECT_EQ(read_back.substr(0, kChars), i_lit);
    }
}

TEST(CvtPipeCreator, PutThroughAStaticPipe)
{
    auto obj = vigenere_then_code().create(rb_root_cvt{mem_device("")});
    EXPECT_EQ(put_in_chunks(obj, true), vigenere_bytes());
}

TEST(CvtPipeCreator, PutThroughARuntimeCvt)
{
    auto        tmp = vigenere_then_code().create(rb_root_cvt{mem_device("")});
    runtime_cvt obj(std::move(tmp));
    EXPECT_EQ(put_in_chunks(obj, true), vigenere_bytes());
}

// operator| is left-associative, so `a | b | c | d` and `(a | b) | (c | d)` name
// different nestings of the same four stages. The bytes that come out must not
// depend on which nesting was written.
TEST(CvtPipeCreator, GroupingOfThePipeOperatorDoesNotChangeTheBytes)
{
    auto grouped = (Crypt::Classic::vigenere_cvt_creator(kKey) |
                    Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5)) |
                   (Comp::zlib_cvt_creator<char>(6) |
                    code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));
    auto flat = Crypt::Classic::vigenere_cvt_creator(kKey) |
                Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5) |
                Comp::zlib_cvt_creator<char>(6) |
                code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    auto obj = grouped.create(rb_root_cvt{mem_device("")});
    // A hash stage has no position of its own, so tell() is not available here.
    std::string expected = put_in_chunks(obj, false);

    auto        tmp = flat.create(rb_root_cvt{mem_device("")});
    runtime_cvt via_runtime(std::move(tmp));
    EXPECT_EQ(put_in_chunks(via_runtime, false), expected);
}

// The same four stages with the hash moved past the compressor: a runtime_cvt
// must still produce byte-for-byte what the statically typed pipe does.
TEST(CvtPipeCreator, ARuntimeCvtMatchesTheStaticPipeWithTheHashAfterZlib)
{
    auto creator = (Crypt::Classic::vigenere_cvt_creator(kKey) |
                    Comp::zlib_cvt_creator<char>(6)) |
                   (Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5) |
                    code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));

    auto        obj      = creator.create(rb_root_cvt{mem_device("")});
    std::string expected = put_in_chunks(obj, false);

    auto        tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt via_runtime(std::move(tmp));
    EXPECT_EQ(put_in_chunks(via_runtime, false), expected);
}

TEST(CvtPipeCreator, GetThroughAStaticPipe)
{
    auto obj = vigenere_then_code().create(rb_root_cvt{mem_device(vigenere_bytes())});
    get_in_chunks(obj);
}

TEST(CvtPipeCreator, GetThroughARuntimeCvt)
{
    auto        tmp = vigenere_then_code().create(rb_root_cvt{mem_device(vigenere_bytes())});
    runtime_cvt obj(std::move(tmp));
    get_in_chunks(obj);
}

TEST(CvtPipeCreator, RoundTripThroughAStaticPipe)
{
    auto creator = Crypt::Classic::vigenere_cvt_creator(kKey) |
                   Comp::zlib_cvt_creator<char>(6) |
                   code_cvt_creator<char, char32_t>("zh_CN.UTF-8");
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    round_trip(obj, creator);
}

TEST(CvtPipeCreator, RoundTripThroughARuntimeCvt)
{
    auto creator = Crypt::Classic::vigenere_cvt_creator(kKey) |
                   Comp::zlib_cvt_creator<char>(6) |
                   code_cvt_creator<char, char32_t>("zh_CN.UTF-8");
    auto        tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt obj(std::move(tmp));
    round_trip(obj, creator);
}

TEST(CvtPipeCreator, RoundTripWithTheTailGroupedToTheRight)
{
    auto creator = Crypt::Classic::vigenere_cvt_creator(kKey) |
                   (Comp::zlib_cvt_creator<char>(6) |
                    code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    round_trip(obj, creator);
}

TEST(CvtPipeCreator, RoundTripWithTheTailGroupedToTheRightThroughARuntimeCvt)
{
    auto creator = Crypt::Classic::vigenere_cvt_creator(kKey) |
                   (Comp::zlib_cvt_creator<char>(6) |
                    code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));
    auto        tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt obj(std::move(tmp));
    round_trip(obj, creator);
}

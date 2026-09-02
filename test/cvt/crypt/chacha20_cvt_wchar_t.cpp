// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/defs.h>
#include <cvt/crypt/chacha20_cvt.h>
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

using namespace IOv2;

// The char counterpart of these cases lives in chacha20_cvt.cpp. Here the
// converter's internal type is wchar_t while the device holds char, so each
// element becomes four ciphertext bytes -- which is what the truncation case at
// the bottom is about.
namespace
{
    using WcharCvt = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;

    constexpr std::size_t kSize   = 4102;
    constexpr std::size_t kChunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

    std::wstring sample()
    {
        std::wstring out;
        out.resize(kSize);
        for (std::size_t i = 0; i < kSize; i += 7)
        {
            out[i + 0] = L'\xE6';
            out[i + 1] = L'\x9D';
            out[i + 2] = L'\x8E';
            out[i + 3] = L'\xE4';
            out[i + 4] = L'\xBC';
            out[i + 5] = L'\x9F';
            out[i + 6] = (i / 7) % 127 + 1;
        }
        return out;
    }

    auto creator() { return Crypt::chacha20_cvt_creator<wchar_t>("liwei"); }

    // Writes the sample in rotating chunks and returns the ciphertext. With
    // move_between_chunks the converter is moved out and back between every
    // chunk: the keystream position lives in the cipher object, so a move that
    // dropped or restarted it would corrupt everything after the first chunk.
    template <typename T>
    std::string encrypt_in_chunks(T& obj, const std::wstring& plain, bool move_between_chunks)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t    total = 0;
        const wchar_t* cur   = plain.data();
        int            id    = 0;
        while (total < kSize)
        {
            std::size_t n = std::min<std::size_t>(kSize - total, kChunks[id++]);
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

        auto [dev, err] = obj.detach();
        return dev.str();
    }

    template <typename T>
    void expect_decrypts_in_chunks(const std::string& enc, const std::wstring& plain)
    {
        T obj(creator().create(rb_root_cvt{mem_device(enc)}));
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::wstring buf(kSize * 2, L'\0');
        std::size_t  total = 0;
        wchar_t*     cur   = buf.data();
        int          id    = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kSize * 2 - total, kChunks[id++]);
            T           moved(std::move(obj));
            auto        s = moved.get(cur, n);
            id %= std::size(kChunks);
            cur   += s;
            total += s;
            if (s == 0) break;
            obj = std::move(moved);
        }

        ASSERT_EQ(cur - buf.data(), static_cast<std::ptrdiff_t>(kSize));
        buf.resize(kSize);
        EXPECT_EQ(buf, plain);
    }
}

TEST(Chacha20CvtWchar, TraitsOverARbRootCvtOfChar)
{
    using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    // The keystream is generated forward from the IV, so there is no way to jump
    // to a position or to turn the stream around mid-way.
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(Chacha20CvtWchar, TraitsOverANoRbRootCvtOfChar8)
{
    using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char8_t>, false>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(Chacha20CvtWchar, MovingTheConverterBetweenChunksPreservesTheKeystream)
{
    const std::wstring plain = sample();
    auto               obj   = creator().create(rb_root_cvt{mem_device("")});
    const std::string  enc   = encrypt_in_chunks(obj, plain, true);
    expect_decrypts_in_chunks<WcharCvt>(enc, plain);
}

TEST(Chacha20CvtWchar, MovingTheConverterBetweenChunksPreservesTheKeystreamThroughARuntimeCvt)
{
    const std::wstring plain = sample();
    runtime_cvt        obj(creator().create(rb_root_cvt{mem_device("")}));
    const std::string  enc = encrypt_in_chunks(obj, plain, true);
    expect_decrypts_in_chunks<runtime_cvt<mem_device<char>, wchar_t>>(enc, plain);
}

TEST(Chacha20CvtWchar, ChunkedPutRoundTrips)
{
    const std::wstring plain = sample();
    auto               obj   = creator().create(rb_root_cvt{mem_device("")});
    const std::string  enc   = encrypt_in_chunks(obj, plain, false);

    auto dec = creator().create(rb_root_cvt{mem_device(enc)});
    EXPECT_EQ(dec.bos(), io_status::input);
    dec.main_cont_beg();

    std::wstring buf(kSize * 2, L'\0');
    EXPECT_EQ(dec.get(buf.data(), buf.size()), kSize);
    buf.resize(kSize);
    EXPECT_EQ(buf, plain);
}

TEST(Chacha20CvtWchar, ChunkedPutRoundTripsThroughARuntimeCvt)
{
    const std::wstring plain = sample();
    runtime_cvt        obj(creator().create(rb_root_cvt{mem_device("")}));
    const std::string  enc = encrypt_in_chunks(obj, plain, false);

    runtime_cvt dec(creator().create(rb_root_cvt{mem_device(enc)}));
    EXPECT_EQ(dec.bos(), io_status::input);
    dec.main_cont_beg();

    std::wstring buf(kSize * 2, L'\0');
    EXPECT_EQ(dec.get(buf.data(), buf.size()), kSize);
    buf.resize(kSize);
    EXPECT_EQ(buf, plain);
}

// Each session draws a fresh random IV, so the same plaintext under the same key
// must not encrypt to the same bytes twice. Equal ciphertexts would mean the IV
// is fixed, which is the classic keystream-reuse break.
TEST(Chacha20CvtWchar, TwoSessionsWithTheSameKeyProduceDifferentCiphertext)
{
    const std::wstring plain = sample();
    auto               obj1  = creator().create(rb_root_cvt{mem_device("")});
    auto               obj2  = creator().create(rb_root_cvt{mem_device("")});

    EXPECT_NE(encrypt_in_chunks(obj1, plain, false), encrypt_in_chunks(obj2, plain, false));
}

TEST(Chacha20CvtWchar, TwoSessionsWithTheSameKeyProduceDifferentCiphertextThroughARuntimeCvt)
{
    const std::wstring plain = sample();
    runtime_cvt        obj1(creator().create(rb_root_cvt{mem_device("")}));
    runtime_cvt        obj2(creator().create(rb_root_cvt{mem_device("")}));

    EXPECT_NE(encrypt_in_chunks(obj1, plain, false), encrypt_in_chunks(obj2, plain, false));
}

// One wchar_t encrypts to four ciphertext bytes after the IV. Dropping the last
// byte leaves a stream that ends in the middle of an element: the converter must
// report that as an error rather than hand back a half-decoded character.
TEST(Chacha20CvtWchar, AStreamThatEndsMidElementIsRejected)
{
    Crypt::chacha20_cvt_creator<wchar_t> key_creator("errkey");

    std::string enc;
    {
        auto enc_obj = key_creator.create(rb_root_cvt{mem_device("")});
        enc_obj.bos();
        enc_obj.main_cont_beg();
        wchar_t ch = L'A';
        enc_obj.put(&ch, 1);
        auto [dev, err] = enc_obj.detach();
        enc = dev.str(); // IV bytes + 4 bytes of ciphertext
    }
    ASSERT_GT(enc.size(), 4u);
    enc.resize(enc.size() - 1);

    WcharCvt dec_obj(rb_root_cvt{mem_device(enc)}, "errkey");
    dec_obj.bos(); // the IV still reads fine
    dec_obj.main_cont_beg();

    wchar_t buf[2];
    EXPECT_THROW((void)dec_obj.get(buf, 1), cvt_error);
}

// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/defs.h>
#include <cvt/crypt/chacha20_cvt.h>
#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <botan/secmem.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    using CharCvt = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;

    constexpr std::size_t kSize = 4102;

    // 586 repetitions of the UTF-8 for U'李' U'伟' plus one byte cycling 1..127.
    // ChaCha20 is a stream cipher, so what matters here is only that the sample is
    // long enough to cross several keystream blocks and that 4102 is not a
    // multiple of any chunk size below.
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

    constexpr std::size_t kChunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

    // Writes the sample in rotating chunks and returns the ciphertext. With
    // move_between_chunks the converter is moved out and back between every
    // chunk: the keystream position lives in the cipher object, so a move that
    // dropped or restarted it would corrupt everything after the first chunk.
    template <typename T>
    std::string encrypt_in_chunks(T& obj, const std::string& plain, bool move_between_chunks)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        std::size_t total = 0;
        const char* cur   = plain.data();
        int         id    = 0;
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

    // Reads the ciphertext back in rotating chunks, again optionally moving the
    // converter between every one, and checks it decrypts to the sample.
    template <typename T>
    void expect_decrypts_in_chunks(const std::string& enc, const std::string& plain,
                                   bool move_between_chunks)
    {
        T obj(CharCvt{rb_root_cvt{mem_device(enc)}, "liwei"});
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::string buf(kSize * 2, '\0');
        std::size_t total = 0;
        char*       cur   = buf.data();
        int         id    = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kSize * 2 - total, kChunks[id++]);
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

        ASSERT_EQ(cur - buf.data(), static_cast<std::ptrdiff_t>(kSize));
        buf.resize(kSize);
        EXPECT_EQ(buf, plain);
    }

    std::string encrypt_whole(const std::string& msg, const char* key)
    {
        CharCvt obj(rb_root_cvt{mem_device("")}, key);
        obj.bos();
        obj.main_cont_beg();
        obj.put(msg.data(), msg.size());
        auto [dev, err] = obj.detach();
        return dev.str();
    }

    std::string decrypt_whole(const std::string& enc, const char* key, std::size_t room)
    {
        CharCvt obj(rb_root_cvt{mem_device(enc)}, key);
        obj.bos();
        obj.main_cont_beg();
        std::string out(room, '\0');
        auto        n = obj.get(out.data(), out.size());
        out.resize(n);
        return out;
    }
}

TEST(Chacha20Cvt, TraitsOverARbRootCvtOfChar)
{
    using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    // The keystream is generated forward from the IV, so there is no way to jump
    // to a position or to turn the stream around mid-way.
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(Chacha20Cvt, TraitsOverANoRbRootCvtOfChar8)
{
    using CheckType = Crypt::chacha20_cvt<no_rb_root_cvt<mem_device<char8_t>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(Chacha20Cvt, MovingTheConverterBetweenChunksPreservesTheKeystream)
{
    const std::string plain = sample();
    CharCvt           obj(rb_root_cvt{mem_device("")}, "liwei");
    const std::string enc = encrypt_in_chunks(obj, plain, true);
    expect_decrypts_in_chunks<CharCvt>(enc, plain, true);
}

TEST(Chacha20Cvt, MovingTheConverterBetweenChunksPreservesTheKeystreamThroughARuntimeCvt)
{
    const std::string plain = sample();
    runtime_cvt       obj(CharCvt{rb_root_cvt{mem_device("")}, "liwei"});
    const std::string enc = encrypt_in_chunks(obj, plain, true);
    expect_decrypts_in_chunks<runtime_cvt<mem_device<char>, char>>(enc, plain, true);
}

TEST(Chacha20Cvt, ChunkedPutRoundTrips)
{
    const std::string plain = sample();
    CharCvt           obj(rb_root_cvt{mem_device("")}, "liwei");
    const std::string enc = encrypt_in_chunks(obj, plain, false);

    CharCvt dec(rb_root_cvt{mem_device(enc)}, "liwei");
    EXPECT_EQ(dec.bos(), io_status::input);
    dec.main_cont_beg();

    std::string buf(kSize * 2, '\0');
    EXPECT_EQ(dec.get(buf.data(), buf.size()), kSize);
    buf.resize(kSize);
    EXPECT_EQ(buf, plain);
}

TEST(Chacha20Cvt, ChunkedPutRoundTripsThroughARuntimeCvt)
{
    const std::string plain = sample();
    runtime_cvt       obj(CharCvt{rb_root_cvt{mem_device("")}, "liwei"});
    const std::string enc = encrypt_in_chunks(obj, plain, false);

    runtime_cvt dec(CharCvt{rb_root_cvt{mem_device(enc)}, "liwei"});
    EXPECT_EQ(dec.bos(), io_status::input);
    dec.main_cont_beg();

    std::string buf(kSize * 2, '\0');
    EXPECT_EQ(dec.get(buf.data(), buf.size()), kSize);
    buf.resize(kSize);
    EXPECT_EQ(buf, plain);
}

// Each session draws a fresh random IV, so the same plaintext under the same key
// must not encrypt to the same bytes twice. Equal ciphertexts would mean the IV
// is fixed, which is the classic keystream-reuse break.
TEST(Chacha20Cvt, TwoSessionsWithTheSameKeyProduceDifferentCiphertext)
{
    const std::string plain = sample();
    CharCvt           obj1(rb_root_cvt{mem_device("")}, "liwei");
    CharCvt           obj2(rb_root_cvt{mem_device("")}, "liwei");

    EXPECT_NE(encrypt_in_chunks(obj1, plain, false), encrypt_in_chunks(obj2, plain, false));
}

TEST(Chacha20Cvt, TwoSessionsWithTheSameKeyProduceDifferentCiphertextThroughARuntimeCvt)
{
    const std::string plain = sample();
    runtime_cvt       obj1(CharCvt{rb_root_cvt{mem_device("")}, "liwei"});
    runtime_cvt       obj2(CharCvt{rb_root_cvt{mem_device("")}, "liwei"});

    EXPECT_NE(encrypt_in_chunks(obj1, plain, false), encrypt_in_chunks(obj2, plain, false));
}

TEST(Chacha20Cvt, TheSameKeyRecoversTheMessage)
{
    const std::string msg = "Hello, world! This is a test message for ChaCha20.";
    EXPECT_EQ(decrypt_whole(encrypt_whole(msg, "key1"), "key1", msg.size()), msg);
}

TEST(Chacha20Cvt, ADifferentKeyDoesNotRecoverTheMessage)
{
    const std::string msg = "Hello, world! This is a test message for ChaCha20.";
    EXPECT_NE(decrypt_whole(encrypt_whole(msg, "key1"), "key2", msg.size()), msg);
}

// A passphrase is stretched into the 32-byte key, and an empty one carries no
// entropy at all, so it is refused rather than stretched into a fixed key.
TEST(Chacha20Cvt, AnEmptyPassphraseIsRejected)
{
    EXPECT_THROW(Crypt::chacha20_cvt_helpers::key_gen(""), cvt_error);
}

// The other constructor takes the 32 raw key bytes directly, skipping key_gen.
TEST(Chacha20Cvt, ARawKeyRoundTrips)
{
    Botan::secure_vector<uint8_t> key(32, 0xAB);
    const std::string             msg = "raw-key test message";

    std::string enc;
    {
        CharCvt obj(rb_root_cvt{mem_device("")}, key);
        obj.bos();
        obj.main_cont_beg();
        obj.put(msg.data(), msg.size());
        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        enc = dev.str();
    }

    CharCvt obj(rb_root_cvt{mem_device(enc)}, key);
    obj.bos();
    obj.main_cont_beg();
    std::string dec(msg.size() * 2, '\0');
    auto        n = obj.get(dec.data(), dec.size());
    dec.resize(n);
    EXPECT_EQ(dec, msg);
}

// ChaCha20 takes a 256-bit key; a raw key of any other length is a caller error
// and must not be silently padded.
TEST(Chacha20Cvt, ARawKeyOfTheWrongLengthIsRejected)
{
    Botan::secure_vector<uint8_t> bad_key(5, 0x00);
    EXPECT_THROW(CharCvt(rb_root_cvt{mem_device("")}, bad_key), cvt_error);
}

TEST(Chacha20Cvt, MoveAssignmentCarriesAnOpenStream)
{
    const std::string msg = "move assign test";
    CharCvt           obj1(rb_root_cvt{mem_device("")}, "movekey");
    CharCvt           obj2(rb_root_cvt{mem_device("")}, "movekey");

    obj1.bos();
    obj1.main_cont_beg();
    obj2 = std::move(obj1);
    obj2.put(msg.data(), msg.size());

    auto [dev, err] = obj2.detach();
    EXPECT_FALSE(err);
    EXPECT_FALSE(dev.str().empty());
}

// attach() starts a new session on the same converter: a new IV is drawn, so the
// second ciphertext differs from the first, and both still decrypt.
TEST(Chacha20Cvt, AttachStartsAFreshSessionWithANewIv)
{
    const std::string msg = "attach-cycle test message for chacha20";
    CharCvt           obj(rb_root_cvt{mem_device("")}, "attachkey");

    obj.bos();
    obj.main_cont_beg();
    obj.put(msg.data(), msg.size());
    auto [dev1, err1] = obj.detach();
    EXPECT_FALSE(err1);
    const std::string enc1 = dev1.str();

    obj.attach();
    obj.bos();
    obj.main_cont_beg();
    obj.put(msg.data(), msg.size());
    auto [dev2, err2] = obj.detach();
    EXPECT_FALSE(err2);
    const std::string enc2 = dev2.str();

    EXPECT_NE(enc1, enc2);
    EXPECT_EQ(decrypt_whole(enc1, "attachkey", msg.size() * 2), msg);
    EXPECT_EQ(decrypt_whole(enc2, "attachkey", msg.size() * 2), msg);
}

TEST(Chacha20Cvt, BosIsRejectedOnAMovedFromConverter)
{
    CharCvt src(rb_root_cvt{mem_device("")}, "attachkey");
    auto    moved = std::move(src);

    EXPECT_THROW((void)src.bos(), cvt_error);
}

TEST(Chacha20Cvt, AttachIsRejectedOnAMovedFromConverter)
{
    CharCvt src(rb_root_cvt{mem_device("")}, "attachkey");
    auto    moved = std::move(src);

    EXPECT_THROW(src.attach(), cvt_error);
}

// Opening for input starts by reading the 12-byte IV off the front of the
// stream. Five bytes is not an IV, and the converter must say so rather than
// decrypt with whatever it managed to read.
TEST(Chacha20Cvt, BosThrowsWhenTheStreamIsTooShortForTheIv)
{
    CharCvt obj(rb_root_cvt{mem_device("hello")}, "attachkey");
    EXPECT_THROW((void)obj.bos(), cvt_error);
}

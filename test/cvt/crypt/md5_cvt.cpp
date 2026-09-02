// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/cvt/crypt/hash_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    // RFC 1321 test vector: MD5("hello"), in the three formats hash_cvt can emit.
    const std::string hello_md5_hex_low = "5d41402abc4b2a76b9719d911017c592";
    const std::string hello_md5_hex_up  = "5D41402ABC4B2A76B9719D911017C592";
    const std::string hello_md5_binary  = "\x5D\x41\x40\x2A\xBC\x4B\x2A\x76\xB9\x71\x9D\x91\x10\x17\xC5\x92";

    // MD5 of the six UTF-8 bytes of u8"李伟", for the char8_t instantiation.
    const std::u8string liwei_md5_hex_low = u8"ffb031550e9681adbe2223cc408d48fc";
    const std::u8string liwei_md5_hex_up  = u8"FFB031550E9681ADBE2223CC408D48FC";
    const std::u8string liwei_md5_binary  = u8"\xFF\xB0\x31\x55\x0E\x96\x81\xAD\xBE\x22\x23\xCC\x40\x8D\x48\xFC";

    auto md5_over_empty_char()
    {
        return Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5)
                   .create(rb_root_cvt{mem_device{""}});
    }

    auto md5_over_empty_char8()
    {
        return Crypt::hash_cvt_creator<char8_t>(Crypt::hash_algo::MD5)
                   .create(rb_root_cvt{mem_device{u8""}});
    }

    // A converter forked after "he" carries the digest state with it, so only the
    // fork -- which goes on to see "llo" -- can produce MD5("hello"). The original,
    // finished at "he", must not.
    template <typename T, typename Fork>
    void expect_only_the_fork_completes_the_digest(T& obj, Fork fork)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);

        T forked = fork(obj);
        forked.put("llo", 3);

        auto [dev, err] = forked.detach();
        EXPECT_EQ(dev.str(), hello_md5_hex_low);

        auto [dev2, err2] = obj.detach();
        EXPECT_NE(dev2.str(), hello_md5_hex_low);
    }

    // Moving after "he" leaves the source with nothing to finish, so the digest
    // has to come out of the target alone.
    template <typename T, typename Transfer>
    void expect_the_move_target_completes_the_digest(T& obj, Transfer transfer)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);

        T moved = transfer(obj);
        moved.put("llo", 3);

        auto [dev, err] = moved.detach();
        EXPECT_EQ(dev.str(), hello_md5_hex_low);
    }

    // One digest of "hello" in whatever format the converter is currently set to.
    // The attach() after detach() is what makes the converter reusable for another
    // stream, and is the only place that path is exercised.
    template <typename T>
    void expect_hello_digest(T& obj, const std::string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        auto [dev, err] = obj.detach();
        obj.attach();
        EXPECT_EQ(dev.str(), expected);
    }

    template <typename T>
    void expect_hello_digest(T& obj, Crypt::hash_fmt fmt, const std::string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.adjust(Crypt::set_hash_fmt(fmt));
        obj.put("hello", 5);
        auto [dev, err] = obj.detach();
        obj.attach();
        EXPECT_EQ(dev.str(), expected);
    }

    template <typename T>
    void expect_liwei_digest(T& obj, const std::u8string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put(u8"李伟", 6);
        auto [dev, err] = obj.detach();
        obj.attach();
        EXPECT_EQ(dev.str(), expected);
    }

    template <typename T>
    void expect_liwei_digest(T& obj, Crypt::hash_fmt fmt, const std::u8string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.adjust(Crypt::set_hash_fmt(fmt));
        obj.put(u8"李伟", 6);
        auto [dev, err] = obj.detach();
        obj.attach();
        EXPECT_EQ(dev.str(), expected);
    }
}

TEST(Md5Cvt, TraitsOverARbRootCvtOfChar)
{
    using CheckType = Crypt::hash_cvt<rb_root_cvt<mem_device<char>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
    // A hash consumes input and emits a digest, so it is write-only, has no
    // positions, and cannot be turned around mid-stream.
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(!cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(Md5Cvt, TraitsOverANoRbRootCvtOfChar8)
{
    using CheckType = Crypt::hash_cvt<no_rb_root_cvt<mem_device<char8_t>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(!cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(Md5Cvt, ACopyConstructedForkCarriesTheDigestState)
{
    auto obj = md5_over_empty_char();
    expect_only_the_fork_completes_the_digest(obj, [](auto& src) { return decltype(md5_over_empty_char()){src}; });
}

TEST(Md5Cvt, ACopyConstructedForkCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_only_the_fork_completes_the_digest(obj, [](auto& src) { return runtime_cvt{src}; });
}

TEST(Md5Cvt, ACopyAssignedForkCarriesTheDigestState)
{
    auto obj = md5_over_empty_char();
    expect_only_the_fork_completes_the_digest(obj, [](auto& src)
    {
        auto dst = md5_over_empty_char();
        dst = src;
        return dst;
    });
}

TEST(Md5Cvt, ACopyAssignedForkCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_only_the_fork_completes_the_digest(obj, [](auto& src)
    {
        runtime_cvt dst{md5_over_empty_char()};
        dst = src;
        return dst;
    });
}

TEST(Md5Cvt, MoveConstructionCarriesTheDigestState)
{
    auto obj = md5_over_empty_char();
    expect_the_move_target_completes_the_digest(obj, [](auto& src)
    { return decltype(md5_over_empty_char()){std::move(src)}; });
}

TEST(Md5Cvt, MoveConstructionCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_the_move_target_completes_the_digest(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(Md5Cvt, MoveAssignmentCarriesTheDigestState)
{
    auto obj = md5_over_empty_char();
    expect_the_move_target_completes_the_digest(obj, [](auto& src)
    {
        auto dst = md5_over_empty_char();
        dst = std::move(src);
        return dst;
    });
}

TEST(Md5Cvt, MoveAssignmentCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_the_move_target_completes_the_digest(obj, [](auto& src)
    {
        runtime_cvt dst{md5_over_empty_char()};
        dst = std::move(src);
        return dst;
    });
}

// A stream that was opened and closed without a single put() has nothing to
// digest, so nothing is emitted -- not the digest of the empty string.
TEST(Md5Cvt, AStreamWithNoInputEmitsNothing)
{
    auto obj = md5_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(dev.str().empty());
}

TEST(Md5Cvt, AStreamWithNoInputEmitsNothingThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(dev.str().empty());
}

TEST(Md5Cvt, TheDigestDefaultsToLowerHex)
{
    auto obj = md5_over_empty_char();
    expect_hello_digest(obj, hello_md5_hex_low);
}

TEST(Md5Cvt, TheDigestDefaultsToLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_hello_digest(obj, hello_md5_hex_low);
}

TEST(Md5Cvt, TheDigestCanBeAskedForInLowerHex)
{
    auto obj = md5_over_empty_char();
    expect_hello_digest(obj, Crypt::hash_fmt::lower_hex, hello_md5_hex_low);
}

TEST(Md5Cvt, TheDigestCanBeAskedForInLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_hello_digest(obj, Crypt::hash_fmt::lower_hex, hello_md5_hex_low);
}

TEST(Md5Cvt, TheDigestCanBeAskedForInUpperHex)
{
    auto obj = md5_over_empty_char();
    expect_hello_digest(obj, Crypt::hash_fmt::upper_hex, hello_md5_hex_up);
}

TEST(Md5Cvt, TheDigestCanBeAskedForInUpperHexThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_hello_digest(obj, Crypt::hash_fmt::upper_hex, hello_md5_hex_up);
}

TEST(Md5Cvt, TheDigestCanBeAskedForAsRawBytes)
{
    auto obj = md5_over_empty_char();
    expect_hello_digest(obj, Crypt::hash_fmt::binary, hello_md5_binary);
}

TEST(Md5Cvt, TheDigestCanBeAskedForAsRawBytesThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_hello_digest(obj, Crypt::hash_fmt::binary, hello_md5_binary);
}

namespace
{
    // dump_hash closes the current digest, writes it followed by the given
    // separator, and starts a new one -- so one stream can carry several digests.
    template <typename T>
    void expect_dump_hash_separates_digests(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        obj.adjust(Crypt::dump_hash('\n'));
        obj.put("hello", 5);
        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), hello_md5_hex_low + '\n' + hello_md5_hex_low);
    }

    // A format change made after dump_hash applies to the digest that follows it,
    // not retroactively to the one already written.
    template <typename T>
    void expect_a_format_change_applies_to_the_next_digest(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        obj.adjust(Crypt::dump_hash('*'));
        obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
        obj.put("hello", 5);
        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), hello_md5_hex_low + '*' + hello_md5_hex_up);
    }
}

TEST(Md5Cvt, DumpHashSeparatesConsecutiveDigests)
{
    auto obj = md5_over_empty_char();
    expect_dump_hash_separates_digests(obj);
}

TEST(Md5Cvt, DumpHashSeparatesConsecutiveDigestsThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_dump_hash_separates_digests(obj);
}

TEST(Md5Cvt, AFormatChangeAppliesToTheNextDigest)
{
    auto obj = md5_over_empty_char();
    expect_a_format_change_applies_to_the_next_digest(obj);
}

TEST(Md5Cvt, AFormatChangeAppliesToTheNextDigestThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    expect_a_format_change_applies_to_the_next_digest(obj);
}

// The digest depends on the byte sequence, not on how it was split across put()
// calls: "he" then "llo" must hash the same as "hello" in one go.
TEST(Md5Cvt, TheDigestDoesNotDependOnHowPutsAreSplit)
{
    auto obj = md5_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("he", 2);
    obj.put("llo", 3);
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), hello_md5_hex_low);
}

TEST(Md5Cvt, TheDigestDoesNotDependOnHowPutsAreSplitThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char()};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("he", 2);
    obj.put("llo", 3);
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), hello_md5_hex_low);
}

// The same four format cases over char8_t, on input that is genuinely multi-byte:
// the hash is taken over the six UTF-8 code units, and the digest comes back in
// the device's own character type.
TEST(Md5Cvt, TheChar8DigestDefaultsToLowerHex)
{
    auto obj = md5_over_empty_char8();
    expect_liwei_digest(obj, liwei_md5_hex_low);
}

TEST(Md5Cvt, TheChar8DigestDefaultsToLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char8()};
    expect_liwei_digest(obj, liwei_md5_hex_low);
}

TEST(Md5Cvt, TheChar8DigestCanBeAskedForInLowerHex)
{
    auto obj = md5_over_empty_char8();
    expect_liwei_digest(obj, Crypt::hash_fmt::lower_hex, liwei_md5_hex_low);
}

TEST(Md5Cvt, TheChar8DigestCanBeAskedForInLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char8()};
    expect_liwei_digest(obj, Crypt::hash_fmt::lower_hex, liwei_md5_hex_low);
}

TEST(Md5Cvt, TheChar8DigestCanBeAskedForInUpperHex)
{
    auto obj = md5_over_empty_char8();
    expect_liwei_digest(obj, Crypt::hash_fmt::upper_hex, liwei_md5_hex_up);
}

TEST(Md5Cvt, TheChar8DigestCanBeAskedForInUpperHexThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char8()};
    expect_liwei_digest(obj, Crypt::hash_fmt::upper_hex, liwei_md5_hex_up);
}

TEST(Md5Cvt, TheChar8DigestCanBeAskedForAsRawBytes)
{
    auto obj = md5_over_empty_char8();
    expect_liwei_digest(obj, Crypt::hash_fmt::binary, liwei_md5_binary);
}

TEST(Md5Cvt, TheChar8DigestCanBeAskedForAsRawBytesThroughARuntimeCvt)
{
    runtime_cvt obj{md5_over_empty_char8()};
    expect_liwei_digest(obj, Crypt::hash_fmt::binary, liwei_md5_binary);
}

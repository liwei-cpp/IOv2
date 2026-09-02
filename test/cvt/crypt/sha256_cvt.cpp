// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/defs.h>
#include <cvt/crypt/hash_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace IOv2;

// The MD5 counterpart of most of these cases lives in md5_cvt.cpp; what is only
// here is the adjust/assignment edge cases at the bottom, which are about
// hash_cvt itself rather than about the algorithm.
namespace
{
    // FIPS 180-4 test vector: SHA-256("hello"), in the three formats hash_cvt can
    // emit.
    const std::string hello_hex_low =
        "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    const std::string hello_hex_up =
        "2CF24DBA5FB0A30E26E83B2AC5B9E29E1B161E5C1FA7425E73043362938B9824";
    const std::string hello_binary =
        "\x2C\xF2\x4D\xBA\x5F\xB0\xA3\x0E\x26\xE8\x3B\x2A\xC5\xB9\xE2\x9E"
        "\x1B\x16\x1E\x5C\x1F\xA7\x42\x5E\x73\x04\x33\x62\x93\x8B\x98\x24";

    // SHA-256 of the six UTF-8 bytes of u8"李伟", for the char8_t instantiation.
    const std::u8string liwei_hex_low =
        u8"5ed4d58e5d8948c2aa2f9ba57667cbb6c63c679c045b0d6009bac9060e66ec45";
    const std::u8string liwei_hex_up =
        u8"5ED4D58E5D8948C2AA2F9BA57667CBB6C63C679C045B0D6009BAC9060E66EC45";
    const std::u8string liwei_binary =
        u8"\x5E\xD4\xD5\x8E\x5D\x89\x48\xC2\xAA\x2F\x9B\xA5\x76\x67\xCB\xB6"
        u8"\xC6\x3C\x67\x9C\x04\x5B\x0D\x60\x09\xBA\xC9\x06\x0E\x66\xEC\x45";

    auto sha256_over_empty_char()
    {
        return Crypt::hash_cvt_creator<char>(Crypt::hash_algo::SHA256)
                   .create(rb_root_cvt{mem_device{""}});
    }

    auto sha256_over_empty_char8()
    {
        return Crypt::hash_cvt_creator<char8_t>(Crypt::hash_algo::SHA256)
                   .create(rb_root_cvt{mem_device{u8""}});
    }

    // A converter forked after "he" carries the digest state with it, so only the
    // fork -- which goes on to see "llo" -- can produce SHA-256("hello"). The
    // original, finished at "he", must not.
    template <typename T, typename Fork>
    void expect_only_the_fork_completes_the_digest(T& obj, Fork fork)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);

        T forked = fork(obj);
        forked.put("llo", 3);

        auto [dev, err] = forked.detach();
        EXPECT_EQ(dev.str(), hello_hex_low);

        auto [dev2, err2] = obj.detach();
        EXPECT_NE(dev2.str(), hello_hex_low);
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
        EXPECT_EQ(dev.str(), hello_hex_low);
    }

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

TEST(Sha256Cvt, ACopyConstructedForkCarriesTheDigestState)
{
    auto obj = sha256_over_empty_char();
    expect_only_the_fork_completes_the_digest(obj, [](auto& src)
    { return decltype(sha256_over_empty_char()){src}; });
}

TEST(Sha256Cvt, ACopyConstructedForkCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_only_the_fork_completes_the_digest(obj, [](auto& src) { return runtime_cvt{src}; });
}

TEST(Sha256Cvt, ACopyAssignedForkCarriesTheDigestState)
{
    auto obj = sha256_over_empty_char();
    expect_only_the_fork_completes_the_digest(obj, [](auto& src)
    {
        auto dst = sha256_over_empty_char();
        dst = src;
        return dst;
    });
}

TEST(Sha256Cvt, ACopyAssignedForkCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_only_the_fork_completes_the_digest(obj, [](auto& src)
    {
        runtime_cvt dst{sha256_over_empty_char()};
        dst = src;
        return dst;
    });
}

TEST(Sha256Cvt, MoveConstructionCarriesTheDigestState)
{
    auto obj = sha256_over_empty_char();
    expect_the_move_target_completes_the_digest(obj, [](auto& src)
    { return decltype(sha256_over_empty_char()){std::move(src)}; });
}

TEST(Sha256Cvt, MoveConstructionCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_the_move_target_completes_the_digest(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(Sha256Cvt, MoveAssignmentCarriesTheDigestState)
{
    auto obj = sha256_over_empty_char();
    expect_the_move_target_completes_the_digest(obj, [](auto& src)
    {
        auto dst = sha256_over_empty_char();
        dst = std::move(src);
        return dst;
    });
}

TEST(Sha256Cvt, MoveAssignmentCarriesTheDigestStateThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_the_move_target_completes_the_digest(obj, [](auto& src)
    {
        runtime_cvt dst{sha256_over_empty_char()};
        dst = std::move(src);
        return dst;
    });
}

// A stream that was opened and closed without a single put() has nothing to
// digest, so nothing is emitted -- not the digest of the empty string.
TEST(Sha256Cvt, AStreamWithNoInputEmitsNothing)
{
    auto obj = sha256_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(dev.str().empty());
}

TEST(Sha256Cvt, AStreamWithNoInputEmitsNothingThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(dev.str().empty());
}

TEST(Sha256Cvt, TheDigestDefaultsToLowerHex)
{
    auto obj = sha256_over_empty_char();
    expect_hello_digest(obj, hello_hex_low);
}

TEST(Sha256Cvt, TheDigestDefaultsToLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_hello_digest(obj, hello_hex_low);
}

TEST(Sha256Cvt, TheDigestCanBeAskedForInLowerHex)
{
    auto obj = sha256_over_empty_char();
    expect_hello_digest(obj, Crypt::hash_fmt::lower_hex, hello_hex_low);
}

TEST(Sha256Cvt, TheDigestCanBeAskedForInLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_hello_digest(obj, Crypt::hash_fmt::lower_hex, hello_hex_low);
}

TEST(Sha256Cvt, TheDigestCanBeAskedForInUpperHex)
{
    auto obj = sha256_over_empty_char();
    expect_hello_digest(obj, Crypt::hash_fmt::upper_hex, hello_hex_up);
}

TEST(Sha256Cvt, TheDigestCanBeAskedForInUpperHexThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_hello_digest(obj, Crypt::hash_fmt::upper_hex, hello_hex_up);
}

TEST(Sha256Cvt, TheDigestCanBeAskedForAsRawBytes)
{
    auto obj = sha256_over_empty_char();
    expect_hello_digest(obj, Crypt::hash_fmt::binary, hello_binary);
}

TEST(Sha256Cvt, TheDigestCanBeAskedForAsRawBytesThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_hello_digest(obj, Crypt::hash_fmt::binary, hello_binary);
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
        EXPECT_EQ(dev.str(), hello_hex_low + '\n' + hello_hex_low);
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
        EXPECT_EQ(dev.str(), hello_hex_low + '*' + hello_hex_up);
    }
}

TEST(Sha256Cvt, DumpHashSeparatesConsecutiveDigests)
{
    auto obj = sha256_over_empty_char();
    expect_dump_hash_separates_digests(obj);
}

TEST(Sha256Cvt, DumpHashSeparatesConsecutiveDigestsThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_dump_hash_separates_digests(obj);
}

TEST(Sha256Cvt, AFormatChangeAppliesToTheNextDigest)
{
    auto obj = sha256_over_empty_char();
    expect_a_format_change_applies_to_the_next_digest(obj);
}

TEST(Sha256Cvt, AFormatChangeAppliesToTheNextDigestThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    expect_a_format_change_applies_to_the_next_digest(obj);
}

// The digest depends on the byte sequence, not on how it was split across put()
// calls: "he" then "llo" must hash the same as "hello" in one go.
TEST(Sha256Cvt, TheDigestDoesNotDependOnHowPutsAreSplit)
{
    auto obj = sha256_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("he", 2);
    obj.put("llo", 3);
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), hello_hex_low);
}

TEST(Sha256Cvt, TheDigestDoesNotDependOnHowPutsAreSplitThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char()};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("he", 2);
    obj.put("llo", 3);
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), hello_hex_low);
}

// The same four format cases over char8_t, on input that is genuinely multi-byte:
// the hash is taken over the six UTF-8 code units, and the digest comes back in
// the device's own character type.
TEST(Sha256Cvt, TheChar8DigestDefaultsToLowerHex)
{
    auto obj = sha256_over_empty_char8();
    expect_liwei_digest(obj, liwei_hex_low);
}

TEST(Sha256Cvt, TheChar8DigestDefaultsToLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char8()};
    expect_liwei_digest(obj, liwei_hex_low);
}

TEST(Sha256Cvt, TheChar8DigestCanBeAskedForInLowerHex)
{
    auto obj = sha256_over_empty_char8();
    expect_liwei_digest(obj, Crypt::hash_fmt::lower_hex, liwei_hex_low);
}

TEST(Sha256Cvt, TheChar8DigestCanBeAskedForInLowerHexThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char8()};
    expect_liwei_digest(obj, Crypt::hash_fmt::lower_hex, liwei_hex_low);
}

TEST(Sha256Cvt, TheChar8DigestCanBeAskedForInUpperHex)
{
    auto obj = sha256_over_empty_char8();
    expect_liwei_digest(obj, Crypt::hash_fmt::upper_hex, liwei_hex_up);
}

TEST(Sha256Cvt, TheChar8DigestCanBeAskedForInUpperHexThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char8()};
    expect_liwei_digest(obj, Crypt::hash_fmt::upper_hex, liwei_hex_up);
}

TEST(Sha256Cvt, TheChar8DigestCanBeAskedForAsRawBytes)
{
    auto obj = sha256_over_empty_char8();
    expect_liwei_digest(obj, Crypt::hash_fmt::binary, liwei_binary);
}

TEST(Sha256Cvt, TheChar8DigestCanBeAskedForAsRawBytesThroughARuntimeCvt)
{
    runtime_cvt obj{sha256_over_empty_char8()};
    expect_liwei_digest(obj, Crypt::hash_fmt::binary, liwei_binary);
}

// dump_hash asks for the digest of the content so far. With no content there is
// nothing to close, so it does nothing rather than emitting the digest of the
// empty string.
TEST(Sha256Cvt, DumpHashBeforeAnyInputIsANoop)
{
    auto obj = sha256_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.adjust(Crypt::dump_hash{});
    auto [dev, err] = obj.detach();
    EXPECT_TRUE(dev.str().empty());
}

TEST(Sha256Cvt, DumpHashWithNoDelimiterConcatenatesTheDigests)
{
    auto obj = sha256_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("hello", 5);
    obj.adjust(Crypt::dump_hash{});
    obj.put("hello", 5);
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), hello_hex_low + hello_hex_low);
}

// adjust() takes any cvt_behavior; one it does not recognise is not an error,
// because a behaviour is meant to reach whichever stage of a pipe understands it.
TEST(Sha256Cvt, AnUnrecognisedBehaviourIsIgnored)
{
    struct unknown_behavior : cvt_behavior {};

    auto obj = sha256_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("hello", 5);
    obj.adjust(unknown_behavior{});
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), hello_hex_low);
}

// bos() reports input on a device that already has content, and hash_cvt has no
// input direction to offer -- so it refuses rather than returning a direction it
// cannot serve.
TEST(Sha256Cvt, BosThrowsWhenTheDeviceIsNotEmpty)
{
    auto obj = Crypt::hash_cvt_creator<char>(Crypt::hash_algo::SHA256)
                   .create(rb_root_cvt{mem_device{hello_hex_low}});
    EXPECT_THROW((void)obj.bos(), cvt_error);
}

TEST(Sha256Cvt, AttachIsRejectedOnAMovedFromConverter)
{
    auto src   = sha256_over_empty_char();
    auto moved = std::move(src);

    EXPECT_THROW(src.attach(), cvt_error);
}

TEST(Sha256Cvt, BosIsRejectedOnAMovedFromConverter)
{
    auto src   = sha256_over_empty_char();
    auto moved = std::move(src);

    EXPECT_THROW((void)src.bos(), cvt_error);
}

// Assigning over a converter that has unfinished content must close that content
// out first, or the digest it was accumulating would be lost silently. Both
// assignment operators do it, and so does the destructor.
TEST(Sha256Cvt, CopyAssignmentClosesTheTargetsPendingDigest)
{
    auto obj1 = sha256_over_empty_char();
    auto obj2 = sha256_over_empty_char();
    obj1.bos(); obj1.main_cont_beg(); obj1.put("hello", 5);
    obj2.bos(); obj2.main_cont_beg(); obj2.put("hello", 5);

    obj2 = obj1;
    auto [dev, err] = obj2.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(dev.str(), hello_hex_low);
}

TEST(Sha256Cvt, MoveAssignmentClosesTheTargetsPendingDigest)
{
    auto obj1 = sha256_over_empty_char();
    auto obj2 = sha256_over_empty_char();
    obj1.bos(); obj1.main_cont_beg(); obj1.put("hello", 5);
    obj2.bos(); obj2.main_cont_beg(); obj2.put("hello", 5);

    obj2 = std::move(obj1);
    auto [dev, err] = obj2.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(dev.str(), hello_hex_low);
}

TEST(Sha256Cvt, DestructionClosesThePendingDigest)
{
    auto obj = sha256_over_empty_char();
    obj.bos();
    obj.main_cont_beg();
    obj.put("hello", 5);
    // obj is destroyed at the end of the test with content still open; the
    // destructor has to close it without letting an exception escape.
}

TEST(Sha256Cvt, AnUnknownAlgorithmIsRejected)
{
    using KernelType = rb_root_cvt<mem_device<char>>;
    EXPECT_ANY_THROW(Crypt::hash_cvt<KernelType>(rb_root_cvt{mem_device{""}},
                                                 static_cast<Crypt::hash_algo>(255)));
}

// detach() cannot throw -- it returns the device and any error alongside it. An
// output format it cannot render is reported through that second return value.
TEST(Sha256Cvt, AnInvalidOutputFormatIsReportedThroughDetach)
{
    auto obj = sha256_over_empty_char();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("hello", 5);
    obj.adjust(Crypt::set_hash_fmt{static_cast<Crypt::hash_fmt>(255)});

    auto [dev, err] = obj.detach();
    EXPECT_NE(err, nullptr);
}

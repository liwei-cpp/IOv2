// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::messages<char>: looking a string up in a gettext catalogue.
 *
 * The catalogue is the .mo file under IOv2TestResources, compiled from the
 * project's own .po and holding two entries.  What the cases below separate is
 * the three answers translate() can give -- the catalogue's string when the
 * language is available, the key itself when it is not, and the empty string for
 * an empty key -- and, for the narrow instantiation only, the fact that the
 * codeset is a separate axis from the language: the same zh_CN catalogue comes
 * back as UTF-8 or as GBK depending on what was asked for.  Where wchar_t is not
 * Unicode (macOS) only a UTF-8 locale can be served, and anything else is refused.
 */
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/facet/messages.h>
#include <IOv2/facet/messages_details.h>

#include <gtest/gtest.h>

#include <support/exe_path.h>
#include <support/file_guard.h>
#include <support/mo_catalogue.h>
#include <support/platform_macros.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    using iov2_test::append_u32;
    using iov2_test::mo_tree;
    using iov2_test::one_entry_mo;

    struct messages_probe : base_ft<messages>
    {
        using base_ft<messages>::get_translate_dictionary;
    };

    // The catalogue lives beside the test binary, not in the source tree, so its
    // location has to be resolved at run time and handed to gettext.
    void bind_catalogue()
    {
        std::filesystem::path mo_path = exe_path();
        mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
        base_ft<messages>::bind_text_domain("messages", std::filesystem::canonical(mo_path));
    }

    messages<char> facet_for(const char* lang, const char* codeset)
    {
        bind_catalogue();
        return messages<char>(std::make_shared<messages_conf<char>>("messages", lang, codeset));
    }

    const std::string kPleaseUtf8    = "\xe8\xaf\xb7";              // 请
    const std::string kThankYouUtf8  = "\xe8\xb0\xa2\xe8\xb0\xa2";  // 谢谢

    std::string descriptor_only_mo(std::uint32_t original_length,
                                   std::uint32_t original_offset,
                                   std::uint32_t translation_length,
                                   std::uint32_t translation_offset)
    {
        std::string bytes;
        for (std::uint32_t value : {0x950412deu, 0u, 1u, 28u, 36u, 0u, 0u,
                                    original_length, original_offset,
                                    translation_length, translation_offset})
            append_u32(bytes, value, false);
        return bytes;
    }

    auto parse_mo(const std::string& bytes)
    {
        const std::string filename = "messages-parser-test.mo";
        file_guard guard(filename, bytes);
        return messages_probe::get_translate_dictionary(filename);
    }
}

TEST(MessagesChar, TheCharacterTypeIsChar)
{
    static_assert(std::is_same_v<messages<char>::char_type, char>);
}

// Whether translate takes a T. A concept, so that a call that does not compile is an
// answer rather than an error.
template <typename T>
concept translate_takes = requires(const messages<char>& m, T t) { m.translate(t); };

// A C string, a literal included, has an overload of its own returning by value: through
// a user-defined conversion MSVC finds the three string overloads ambiguous. A null
// pointer stays a compile error, as it was before that overload existed.
TEST(MessagesChar, ACStringIsTranslatedByValue)
{
    static_assert(std::is_same_v<decltype(std::declval<const messages<char>&>().translate("please")),
                                 std::string>);
    static_assert(translate_takes<const char*>);
    static_assert(!translate_takes<std::nullptr_t>);
}

// throw_if_fail takes a bool and nothing that merely converts to one, so a directory
// given as a literal after the codeset is not turned into true (char8_t has the case
// that runs).
TEST(MessagesChar, ThrowIfFailTakesOnlyABool)
{
    static_assert(!std::is_constructible_v<messages_conf<char>, std::string, std::string, std::string, int>);
    static_assert(std::is_constructible_v<messages_conf<char>, std::string, std::string, std::string, bool>);
    static_assert(std::is_constructible_v<messages_conf<char>, std::string, std::string, std::string, const char*>);
}

TEST(MessagesChar, AnAvailableCatalogueTranslates)
{
    const messages<char> obj = facet_for("zh_CN", "zh_CN.UTF-8");
    EXPECT_EQ(obj.translate("please"), kPleaseUtf8);
    EXPECT_EQ(obj.translate("thank you"), kThankYouUtf8);
}

// The header entry is the catalogue's own metadata, so a non-empty one is how a
// caller can tell a catalogue was found at all rather than silently skipped.
TEST(MessagesChar, AnAvailableCatalogueHasAHeaderEntry)
{
    const messages<char> obj = facet_for("zh_CN", "zh_CN.UTF-8");
    EXPECT_NE(obj.head_entry(), "");
}

// The codeset is the locale's. GBK is zh_CN.GBK on glibc and zh-CN's code page 936
// on Windows, reached through a Unicode wchar_t; macOS's wchar_t is not Unicode, so
// there a locale that is not UTF-8 is refused.
TEST(MessagesChar, TheCodesetDecidesHowTheTranslationIsEncoded)
{
    const messages<char> utf8 = facet_for("zh_CN", "zh_CN.UTF-8");
    EXPECT_EQ(utf8.translate("please"), kPleaseUtf8);
    EXPECT_EQ(utf8.translate("thank you"), kThankYouUtf8);

#if IOV2_TEST_LINUX_GLIBC || IOV2_TEST_WINDOWS
    const std::string kPleaseGbk   = "\xc7\xeb";          // 请
    const std::string kThankYouGbk = "\xd0\xbb\xd0\xbb";  // 谢谢
    const messages<char> gbk = facet_for("zh_CN", IOV2_TEST_WINDOWS ? "zh-CN" : "zh_CN.GBK");
    EXPECT_EQ(gbk.translate("please"), kPleaseGbk);
    EXPECT_EQ(gbk.translate("thank you"), kThankYouGbk);
    EXPECT_NE(gbk.head_entry(), "");
#else
    // The locale exists, so the refusal is about its encoding.
    ASSERT_NO_THROW((void)clocale_wrapper("de_DE.ISO8859-1"));
    bind_catalogue();
    EXPECT_THROW((void)messages_conf<char>("messages", "zh_CN", "de_DE.ISO8859-1"), io_error);
#endif
}

// In a UTF-8 locale the catalogue's bytes are taken as they are, unchecked, as
// messages<char8_t> takes them: a translation that is not valid UTF-8 still loads.
TEST(MessagesChar, AUtf8LocaleTakesTheBytesUnchecked)
{
    const mo_tree tree(std::filesystem::absolute("messages-char-unchecked"), "messages_unchecked",
                       one_entry_mo("key", std::string("\xff\xfe", 2)));
    const messages_conf<char> conf("messages_unchecked", "zh_CN", "zh_CN.UTF-8", tree.root());
    ASSERT_NE(conf.translate("key"), nullptr);
    EXPECT_EQ(*conf.translate("key"), std::string("\xff\xfe", 2));
}

// With no catalogue for the language, translate() is the identity: the key is
// already the message, in whatever language the source was written in.
TEST(MessagesChar, AMissingCatalogueLeavesTheKeyUntranslated)
{
    const messages<char> obj(std::make_shared<messages_conf<char>>("messages", "zh_HK", "zh_HK", false));
    EXPECT_EQ(obj.translate("please"), "please");
    EXPECT_EQ(obj.translate("thank you"), "thank you");
    EXPECT_EQ(obj.head_entry(), "");
    EXPECT_THROW(messages<char>(std::make_shared<messages_conf<char>>(
                     "messages", "zh_HK", "zh_HK")), io_error);
}

// An empty key would otherwise select the header entry, which is metadata and
// not a message, so it has to come back empty either way.
TEST(MessagesChar, AnEmptyKeyTranslatesToNothing)
{
    const messages<char> present = facet_for("zh_CN", "zh_CN.UTF-8");
    const messages<char> absent(std::make_shared<messages_conf<char>>("messages", "zh_HK", "zh_HK", false));
    EXPECT_EQ(present.translate(""), "");
    EXPECT_EQ(absent.translate(""), "");
}

TEST(MessagesChar, TheMoReaderAcceptsBothByteOrdersAndStopsAtEmbeddedNulls)
{
    for (bool big_endian : {false, true})
    {
        SCOPED_TRACE(big_endian ? "big endian" : "little endian");
        const auto dictionary = parse_mo(one_entry_mo(std::string("key\0plural", 10),
                                                       std::string("value\0other", 11),
                                                       big_endian));
        ASSERT_EQ(dictionary.size(), 1u);
        EXPECT_EQ(dictionary.at(u8"key"), u8"value");
    }
}

TEST(MessagesChar, TheMoReaderRejectsMalformedCatalogues)
{
    const std::string missing = "messages-parser-missing.mo";
    file_guard missing_guard(missing);
    EXPECT_THROW(messages_probe::get_translate_dictionary(missing), io_error);

    EXPECT_THROW(parse_mo(std::string("\xde\x12", 2)), io_error);
    EXPECT_THROW(parse_mo(std::string(8, '\0')), io_error);

    std::string header_only;
    append_u32(header_only, 0x950412deu, false);
    append_u32(header_only, 0, false);
    EXPECT_THROW(parse_mo(header_only), io_error);

    EXPECT_THROW(parse_mo(one_entry_mo("key", "value", false, 0x00010000u)), io_error);

    std::string implausible_count;
    for (std::uint32_t value : {0x950412deu, 0u, 2u, 28u, 44u, 0u, 0u})
        append_u32(implausible_count, value, false);
    EXPECT_THROW(parse_mo(implausible_count), io_error);

    constexpr std::uint32_t too_long = 64u * 1024u * 1024u + 1u;
    EXPECT_THROW(parse_mo(descriptor_only_mo(too_long, 44, 0, 44)), io_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(0, 44, too_long, 44)), io_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(45, 44, 0, 44)), io_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(1, 44, 45, 44) + "x"), io_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(1, 4096, 0, 44)), io_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(0, 44, 1, 4096)), io_error);
}

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
 * back as UTF-8 or as GBK depending on what was asked for.
 */
#include <IOv2/facet/messages.h>
#include <IOv2/facet/messages_details.h>

#include <gtest/gtest.h>

#include <support/exe_path.h>
#include <support/file_guard.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
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
        base_ft<messages>::bind_text_domain("messages", std::filesystem::canonical(mo_path).string());
    }

    messages<char> facet_for(const char* lang, const char* codeset)
    {
        bind_catalogue();
        return messages<char>(std::make_shared<messages_conf<char>>("messages", lang, codeset));
    }

    const std::string kPleaseUtf8    = "\xe8\xaf\xb7";              // 请
    const std::string kThankYouUtf8  = "\xe8\xb0\xa2\xe8\xb0\xa2";  // 谢谢
    const std::string kPleaseGbk     = "\xc7\xeb";                  // 请
    const std::string kThankYouGbk   = "\xd0\xbb\xd0\xbb";          // 谢谢

    void append_u32(std::string& bytes, std::uint32_t value, bool big_endian)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            const unsigned shift = big_endian ? 24 - 8 * i : 8 * i;
            bytes.push_back(static_cast<char>((value >> shift) & 0xffu));
        }
    }

    std::string one_entry_mo(const std::string& original, const std::string& translation,
                             bool big_endian = false, std::uint32_t revision = 0)
    {
        constexpr std::uint32_t header_size = 7 * sizeof(std::uint32_t);
        constexpr std::uint32_t original_table = header_size;
        constexpr std::uint32_t translation_table = original_table + 2 * sizeof(std::uint32_t);
        constexpr std::uint32_t string_data = translation_table + 2 * sizeof(std::uint32_t);

        std::string bytes;
        append_u32(bytes, 0x950412deu, big_endian);
        append_u32(bytes, revision, big_endian);
        append_u32(bytes, 1, big_endian);
        append_u32(bytes, original_table, big_endian);
        append_u32(bytes, translation_table, big_endian);
        append_u32(bytes, 0, big_endian);
        append_u32(bytes, 0, big_endian);
        append_u32(bytes, static_cast<std::uint32_t>(original.size()), big_endian);
        append_u32(bytes, string_data, big_endian);
        append_u32(bytes, static_cast<std::uint32_t>(translation.size()), big_endian);
        append_u32(bytes, string_data + static_cast<std::uint32_t>(original.size()), big_endian);
        bytes += original;
        bytes += translation;
        return bytes;
    }

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

TEST(MessagesChar, TheCodesetDecidesHowTheTranslationIsEncoded)
{
    const messages<char> utf8 = facet_for("zh_CN", "zh_CN.UTF-8");
    const messages<char> gbk  = facet_for("zh_CN", "zh_CN.GBK");

    EXPECT_EQ(utf8.translate("please"), kPleaseUtf8);
    EXPECT_EQ(gbk.translate("please"), kPleaseGbk);
    EXPECT_EQ(utf8.translate("thank you"), kThankYouUtf8);
    EXPECT_EQ(gbk.translate("thank you"), kThankYouGbk);
    EXPECT_NE(gbk.head_entry(), "");
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
                     "messages", "zh_HK", "zh_HK")), stream_error);
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
    EXPECT_THROW(messages_probe::get_translate_dictionary(missing), stream_error);

    EXPECT_THROW(parse_mo(std::string("\xde\x12", 2)), stream_error);
    EXPECT_THROW(parse_mo(std::string(8, '\0')), stream_error);

    std::string header_only;
    append_u32(header_only, 0x950412deu, false);
    append_u32(header_only, 0, false);
    EXPECT_THROW(parse_mo(header_only), stream_error);

    EXPECT_THROW(parse_mo(one_entry_mo("key", "value", false, 0x00010000u)), stream_error);

    std::string implausible_count;
    for (std::uint32_t value : {0x950412deu, 0u, 2u, 28u, 44u, 0u, 0u})
        append_u32(implausible_count, value, false);
    EXPECT_THROW(parse_mo(implausible_count), stream_error);

    constexpr std::uint32_t too_long = 64u * 1024u * 1024u + 1u;
    EXPECT_THROW(parse_mo(descriptor_only_mo(too_long, 44, 0, 44)), stream_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(0, 44, too_long, 44)), stream_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(45, 44, 0, 44)), stream_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(1, 44, 45, 44) + "x"), stream_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(1, 4096, 0, 44)), stream_error);
    EXPECT_THROW(parse_mo(descriptor_only_mo(0, 44, 1, 4096)), stream_error);
}

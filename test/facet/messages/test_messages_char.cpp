// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
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
#include <facet/messages.h>
#include <facet/messages_details.h>

#include <gtest/gtest.h>

#include <support/exe_path.h>

#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
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

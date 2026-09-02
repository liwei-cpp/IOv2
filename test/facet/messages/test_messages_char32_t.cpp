// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::messages<char32_t>: the same gettext catalogue, decoded into code
 * points.  The narrow instantiation takes a codeset argument because its result
 * is bytes; this one has no such axis -- a char32_t is already a code point --
 * so what is left to check is the three answers translate() can give.
 */
#include <facet/messages.h>

#include <gtest/gtest.h>

#include <support/exe_path.h>

#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
    void bind_catalogue()
    {
        std::filesystem::path mo_path = exe_path();
        mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
        base_ft<messages>::bind_text_domain("messages", std::filesystem::canonical(mo_path).string());
    }

    messages<char32_t> facet_for(const char* lang)
    {
        bind_catalogue();
        return messages<char32_t>(std::make_shared<messages_conf<char32_t>>("messages", lang));
    }
}

TEST(MessagesChar32, TheCharacterTypeIsChar32)
{
    static_assert(std::is_same_v<messages<char32_t>::char_type, char32_t>);
}

TEST(MessagesChar32, AnAvailableCatalogueTranslates)
{
    const messages<char32_t> obj = facet_for("zh_CN");
    EXPECT_EQ(obj.translate(U"please"), std::u32string(U"请"));
    EXPECT_EQ(obj.translate(U"thank you"), std::u32string(U"谢谢"));
}

TEST(MessagesChar32, AnAvailableCatalogueHasAHeaderEntry)
{
    const messages<char32_t> obj = facet_for("zh_CN");
    EXPECT_NE(obj.head_entry(), U"");
}

TEST(MessagesChar32, AMissingCatalogueLeavesTheKeyUntranslated)
{
    const messages<char32_t> obj(std::make_shared<messages_conf<char32_t>>("messages", "zh_HK", false));
    EXPECT_EQ(obj.translate(U"please"), std::u32string(U"please"));
    EXPECT_EQ(obj.translate(U"thank you"), std::u32string(U"thank you"));
    EXPECT_EQ(obj.head_entry(), U"");
}

TEST(MessagesChar32, AnEmptyKeyTranslatesToNothing)
{
    const messages<char32_t> present = facet_for("zh_CN");
    const messages<char32_t> absent(std::make_shared<messages_conf<char32_t>>("messages", "zh_HK", false));
    EXPECT_EQ(present.translate(U""), U"");
    EXPECT_EQ(absent.translate(U""), U"");
}

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::messages<wchar_t>: the same gettext catalogue, decoded into wide
 * characters.  The narrow instantiation takes a codeset argument because its
 * result is bytes; the wide one has no such axis -- a wchar_t is already a code
 * point -- so what is left to check is the three answers translate() can give.
 */
#include <IOv2/facet/messages.h>
#include <IOv2/facet/messages_details.h>

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

    messages<wchar_t> facet_for(const char* lang)
    {
        bind_catalogue();
        return messages<wchar_t>(std::make_shared<messages_conf<wchar_t>>("messages", lang));
    }
}

TEST(MessagesWchar, TheCharacterTypeIsWchar)
{
    static_assert(std::is_same_v<messages<wchar_t>::char_type, wchar_t>);
}

TEST(MessagesWchar, AnAvailableCatalogueTranslates)
{
    const messages<wchar_t> obj = facet_for("zh_CN");
    EXPECT_EQ(obj.translate(L"please"), std::wstring(L"请"));
    EXPECT_EQ(obj.translate(L"thank you"), std::wstring(L"谢谢"));
}

TEST(MessagesWchar, AnAvailableCatalogueHasAHeaderEntry)
{
    const messages<wchar_t> obj = facet_for("zh_CN");
    EXPECT_NE(obj.head_entry(), L"");
}

TEST(MessagesWchar, AMissingCatalogueLeavesTheKeyUntranslated)
{
    const messages<wchar_t> obj(std::make_shared<messages_conf<wchar_t>>("messages", "zh_HK", false));
    EXPECT_EQ(obj.translate(L"please"), std::wstring(L"please"));
    EXPECT_EQ(obj.translate(L"thank you"), std::wstring(L"thank you"));
    EXPECT_EQ(obj.head_entry(), L"");
    EXPECT_THROW(messages<wchar_t>(std::make_shared<messages_conf<wchar_t>>(
                     "messages", "zh_HK")), stream_error);
}

TEST(MessagesWchar, AnEmptyKeyTranslatesToNothing)
{
    const messages<wchar_t> present = facet_for("zh_CN");
    const messages<wchar_t> absent(std::make_shared<messages_conf<wchar_t>>("messages", "zh_HK", false));
    EXPECT_EQ(present.translate(L""), L"");
    EXPECT_EQ(absent.translate(L""), L"");
}

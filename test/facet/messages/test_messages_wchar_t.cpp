// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::messages<wchar_t>: the same gettext catalogue, decoded into wide
 * characters.  The narrow instantiation takes a codeset argument because its
 * result is bytes; where wchar_t is Unicode (UTF-32 on Linux, UTF-16 on Windows)
 * the wide one has no such axis, so what is left to check is the three answers
 * translate() can give, and that a character outside the BMP comes out whole.
 * macOS's wchar_t is a code point only under a UTF-8 locale, so there the
 * configuration is told one, and refuses any other.
 */
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/facet/messages.h>
#include <IOv2/facet/messages_details.h>

#include <gtest/gtest.h>

#include <support/exe_path.h>
#include <support/mo_catalogue.h>
#include <support/platform_macros.h>

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
        base_ft<messages>::bind_text_domain("messages", std::filesystem::canonical(mo_path));
    }

    // The one place that knows macOS's constructor takes a locale name.
    std::shared_ptr<messages_conf<wchar_t>> make_conf(const std::string& domain, const char* lang,
                                                      bool require = true,
                                                      const std::filesystem::path& dirname = {})
    {
#if IOV2_TEST_MACOS
        return std::make_shared<messages_conf<wchar_t>>(domain, lang, "zh_CN.UTF-8", dirname, require);
#else
        return std::make_shared<messages_conf<wchar_t>>(domain, lang, dirname, require);
#endif
    }

    messages<wchar_t> facet_for(const char* lang)
    {
        bind_catalogue();
        return messages<wchar_t>(make_conf("messages", lang));
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
    const messages<wchar_t> obj(make_conf("messages", "zh_HK", false));
    EXPECT_EQ(obj.translate(L"please"), std::wstring(L"please"));
    EXPECT_EQ(obj.translate(L"thank you"), std::wstring(L"thank you"));
    EXPECT_EQ(obj.head_entry(), L"");
    EXPECT_THROW(messages<wchar_t>(make_conf("messages", "zh_HK")), io_error);
}

TEST(MessagesWchar, AnEmptyKeyTranslatesToNothing)
{
    const messages<wchar_t> present = facet_for("zh_CN");
    const messages<wchar_t> absent(make_conf("messages", "zh_HK", false));
    EXPECT_EQ(present.translate(L""), L"");
    EXPECT_EQ(absent.translate(L""), L"");
}

// U+1F600 is one wchar_t where wchar_t holds code points (Linux, and macOS under its
// UTF-8 locale) and a surrogate pair where it is UTF-16 (Windows).
TEST(MessagesWchar, ACharacterOutsideTheBmpComesOutWhole)
{
    const iov2_test::mo_tree tree(std::filesystem::absolute("messages-wchar-astral"), "messages_astral",
                       iov2_test::one_entry_mo("smile", "\xF0\x9F\x98\x80"));
    const messages<wchar_t> obj(make_conf("messages_astral", "zh_CN", true, tree.root()));

    std::wstring expected;
    if constexpr (sizeof(wchar_t) == 2)
        expected = {static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE00)};
    else
        expected = {static_cast<wchar_t>(0x1F600)};
    EXPECT_EQ(obj.translate(L"smile"), expected);
}

// Under a locale that is not UTF-8, macOS's wchar_t holds the locale's own values, so
// there is no reliable conversion from the catalogue's UTF-8.
TEST(MessagesWchar, MacosRefusesALocaleThatIsNotUtf8)
{
#if IOV2_TEST_MACOS
    ASSERT_NO_THROW((void)clocale_wrapper("de_DE.ISO8859-1"));   // the locale exists
    bind_catalogue();
    EXPECT_THROW((void)messages_conf<wchar_t>("messages", "zh_CN", "de_DE.ISO8859-1"), io_error);
#else
    GTEST_SKIP() << "wchar_t is Unicode here, and the configuration takes no locale";
#endif
}

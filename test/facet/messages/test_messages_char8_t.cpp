// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::messages<char8_t>, and with it the language-selection rules the other
 * three instantiations share but do not exercise.
 *
 * A language argument is not one name: it is a gettext preference list, colon
 * separated, and the first entry with a catalogue wins.  When the argument is
 * empty the list comes from the environment instead, in the order LANGUAGE,
 * LC_ALL, LC_MESSAGES, LANG.  Both paths end at the same question -- is there a
 * catalogue for this language -- so filtered_lang() is what the cases below
 * read: it names the entry that was chosen, or is empty when none was.
 */
#include <facet/messages.h>

#include <gtest/gtest.h>

#include <support/exe_path.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
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

    messages<char8_t> facet_for(const char* lang)
    {
        bind_catalogue();
        return messages<char8_t>(std::make_shared<messages_conf<char8_t>>("messages", lang));
    }

    const std::u8string kPlease   = u8"\xe8\xaf\xb7";              // 请
    const std::u8string kThankYou = u8"\xe8\xb0\xa2\xe8\xb0\xa2";  // 谢谢

    // zh_CN has a catalogue in the test resources; fr_XX and zh_HK do not, which
    // is what makes them usable as the "keep looking" entries of a list.
    constexpr const char* kPresent = "zh_CN";
    constexpr const char* kAbsent  = "fr_XX";

    // The four variables filter_lang consults, in the order it consults them.
    constexpr const char* kLanguageVars[] = {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"};

    // Selecting a language from the environment means reading process-wide state,
    // so each case takes it over and hands it back exactly as it found it --
    // including the difference between an unset variable and an empty one.
    class MessagesChar8Env : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            for (const char* name : kLanguageVars)
            {
                const char* value = std::getenv(name);
                m_saved.push_back(value ? std::optional<std::string>(value) : std::nullopt);
                ::unsetenv(name);
            }
            bind_catalogue();
        }

        void TearDown() override
        {
            for (std::size_t i = 0; i < std::size(kLanguageVars); ++i)
            {
                if (m_saved[i]) ::setenv(kLanguageVars[i], m_saved[i]->c_str(), 1);
                else            ::unsetenv(kLanguageVars[i]);
            }
        }

        static messages<char8_t> facet_from_the_environment(bool require = true)
        {
            return messages<char8_t>(std::make_shared<messages_conf<char8_t>>("messages", "", require));
        }

        std::vector<std::optional<std::string>> m_saved;
    };
}

TEST(MessagesChar8, TheCharacterTypeIsChar8)
{
    static_assert(std::is_same_v<messages<char8_t>::char_type, char8_t>);
}

TEST(MessagesChar8, AnAvailableCatalogueTranslates)
{
    const messages<char8_t> obj = facet_for(kPresent);
    EXPECT_EQ(obj.translate(u8"please"), kPlease);
    EXPECT_EQ(obj.translate(u8"thank you"), kThankYou);
}

TEST(MessagesChar8, AnAvailableCatalogueHasAHeaderEntry)
{
    const messages<char8_t> obj = facet_for(kPresent);
    EXPECT_NE(obj.head_entry(), u8"");
}

TEST(MessagesChar8, AMissingCatalogueLeavesTheKeyUntranslated)
{
    const messages<char8_t> obj(std::make_shared<messages_conf<char8_t>>("messages", "zh_HK", false));
    EXPECT_EQ(obj.translate(u8"please"), u8"please");
    EXPECT_EQ(obj.translate(u8"thank you"), u8"thank you");
    EXPECT_EQ(obj.head_entry(), u8"");
}

TEST(MessagesChar8, AnEmptyKeyTranslatesToNothing)
{
    const messages<char8_t> present = facet_for(kPresent);
    const messages<char8_t> absent(std::make_shared<messages_conf<char8_t>>("messages", "zh_HK", false));
    EXPECT_EQ(present.translate(u8""), u8"");
    EXPECT_EQ(absent.translate(u8""), u8"");
}

// The list is walked left to right and the first entry with a catalogue wins,
// whether that is the last entry or the first.  Both orders are needed: they
// leave the search loop by different exits.
TEST(MessagesChar8, ALanguageListSkipsPastWhatIsUnavailable)
{
    const messages<char8_t> obj = facet_for("fr_XX:zh_CN");
    EXPECT_EQ(obj.filtered_lang(), kPresent);
    EXPECT_EQ(obj.translate(u8"please"), kPlease);
}

TEST(MessagesChar8, ALanguageListStopsAtTheFirstAvailableEntry)
{
    const messages<char8_t> obj = facet_for("zh_CN:fr_XX");
    EXPECT_EQ(obj.filtered_lang(), kPresent);
    EXPECT_EQ(obj.translate(u8"please"), kPlease);
}

// available() answers the same question the constructor asks, so it has to read
// a list the same way: available if any entry is, not only if the first is.
TEST(MessagesChar8, AvailabilityOfAListIsAvailabilityOfAnyEntry)
{
    bind_catalogue();
    EXPECT_TRUE(base_ft<messages>::available("messages", "zh_CN:fr_XX"));
    EXPECT_TRUE(base_ft<messages>::available("messages", "fr_XX:zh_CN"));
    EXPECT_FALSE(base_ft<messages>::available("messages", "fr_XX:zh_HK"));
    EXPECT_TRUE(base_ft<messages>::available("messages", kPresent));
    EXPECT_FALSE(base_ft<messages>::available("messages", kAbsent));
}

TEST_F(MessagesChar8Env, LanguageIsReadFirstAndMayItselfBeAList)
{
    ::setenv("LANGUAGE", "fr_XX:zh_CN", 1);
    const messages<char8_t> obj = facet_from_the_environment();
    EXPECT_EQ(obj.filtered_lang(), kPresent);
    EXPECT_EQ(obj.translate(u8"please"), kPlease);
}

TEST_F(MessagesChar8Env, TheOtherVariablesAreConsultedInTurn)
{
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"})
    {
        SCOPED_TRACE(name);
        ::setenv(name, kPresent, 1);
        EXPECT_EQ(facet_from_the_environment().filtered_lang(), kPresent);
        ::unsetenv(name);
    }
}

// LANGUAGE comes first, so a catalogue it names is taken even when a later
// variable names a different one.
TEST_F(MessagesChar8Env, LanguageOutranksTheLocaleVariables)
{
    ::setenv("LANGUAGE", kPresent, 1);
    ::setenv("LC_ALL", kAbsent, 1);
    EXPECT_EQ(facet_from_the_environment().filtered_lang(), kPresent);
}

// Nothing in the environment names an available catalogue, so nothing is
// selected -- and with the requirement relaxed that is an empty answer rather
// than a failure to construct.
TEST_F(MessagesChar8Env, NoAvailableLanguageSelectsNothing)
{
    ::setenv("LANG", kAbsent, 1);
    const messages<char8_t> obj = facet_from_the_environment(false);
    EXPECT_EQ(obj.filtered_lang(), "");
    EXPECT_EQ(obj.translate(u8"please"), u8"please");
}

TEST_F(MessagesChar8Env, AnEmptyEnvironmentSelectsNothing)
{
    const messages<char8_t> obj = facet_from_the_environment(false);
    EXPECT_EQ(obj.filtered_lang(), "");
}

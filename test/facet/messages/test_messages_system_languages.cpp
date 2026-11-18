// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The system's language list, which an empty language falls back on when no locale
 * is set (LC_ALL, LC_MESSAGES and LANG all unset, as gettext has it).
 *
 * Two parts.  The mapping from the system's BCP-47 names to .mo directory names is a
 * function of its input alone, so it is checked with fixed names on every platform.
 * The list itself is whatever the machine says -- Windows' UI languages, macOS's
 * preferred languages, nothing on Linux -- so the end-to-end cases build a catalogue
 * for the machine's own first choice, whatever it is, and skip where there is none.
 * The list is printed, so a CI log shows what each system really gives.
 */
#include <IOv2/facet/messages.h>
#include <IOv2/facet/messages_details.h>

#include <gtest/gtest.h>

#include <support/mo_catalogue.h>
#include <support/platform.h>
#include <support/platform_macros.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace IOv2;

namespace
{
    using names = std::vector<std::string>;

    names candidates(const names& system) { return detail::messages_language_candidates(system); }

    // The machine's own list, made into directory names.
    names system_candidates() { return candidates(detail::messages_system_languages()); }

    constexpr const char* kLocaleVars[] = {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"};

    // Takes the four variables over and hands them back as it found them, and puts a
    // catalogue under the machine's first choice, bound to a domain of its own.
    class MessagesSystemEnv : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            for (const char* name : kLocaleVars)
            {
                m_saved.push_back(iov2_test::sys::env_is_set(name)
                                      ? std::optional<std::string>(iov2_test::sys::get_env(name))
                                      : std::nullopt);
                iov2_test::sys::unset_env(name);
            }

            const names system = system_candidates();
            if (system.empty())
                GTEST_SKIP() << "this system keeps no language list besides the environment";
            m_first = system.front();
            m_tree.emplace(std::filesystem::absolute("messages-system-languages"), "messages_system",
                           iov2_test::one_entry_mo("key", "value"), m_first);
            base_ft<messages>::bind_text_domain("messages_system", m_tree->root());
        }

        void TearDown() override
        {
            for (std::size_t i = 0; i < std::size(kLocaleVars); ++i)
            {
                if (m_saved[i]) iov2_test::sys::set_env(kLocaleVars[i], *m_saved[i]);
                else            iov2_test::sys::unset_env(kLocaleVars[i]);
            }
        }

        static std::string selected()
        {
            return messages_conf<char8_t>("messages_system", "", false).filtered_lang();
        }

        std::vector<std::optional<std::string>> m_saved;
        std::string m_first;
        std::optional<iov2_test::mo_tree> m_tree;
    };
}

TEST(MessagesSystemLanguages, EachNameGivesItsRegionThenTheBareLanguage)
{
    EXPECT_EQ(candidates({"zh-CN", "en-US"}), (names{"zh_CN", "zh", "en_US", "en"}));
    EXPECT_EQ(candidates({"fr"}), (names{"fr"}));
}

// A script says how the language is written, which no .mo directory name carries; a
// region may be three digits (UN M.49).
TEST(MessagesSystemLanguages, AScriptIsDroppedAndANumericRegionKept)
{
    EXPECT_EQ(candidates({"zh-Hans-CN", "sr-Latn-RS", "es-419", "zh-Hant"}),
              (names{"zh_CN", "zh", "sr_RS", "sr", "es_419", "es"}));
}

TEST(MessagesSystemLanguages, CaseIsNormalisedAndAnUnderscoreSeparatesToo)
{
    EXPECT_EQ(candidates({"ZH_cn"}), (names{"zh_CN", "zh"}));
}

// An extension starts at a single-character subtag; a two-letter subtag past it is
// part of the extension ("co" for collation here), not a region.
TEST(MessagesSystemLanguages, NothingPastAnExtensionIsTakenForARegion)
{
    EXPECT_EQ(candidates({"en-US-u-ca-gregory", "de-u-co-phonebk"}),
              (names{"en_US", "en", "de"}));
}

TEST(MessagesSystemLanguages, RepeatsAndNamesWithNoLanguageAreDropped)
{
    EXPECT_EQ(candidates({"en-GB", "en-US", "", "x-private", "1234", "en"}),
              (names{"en_GB", "en", "en_US"}));
}

// What the machine says is printed for the log. Windows and macOS always have at
// least one language; Linux has no such list.
TEST(MessagesSystemLanguages, TheSystemListIsWhatThePlatformKeeps)
{
    const names system = detail::messages_system_languages();
    std::string joined;
    for (const std::string& n : system)
        joined += (joined.empty() ? "" : ", ") + n;
    std::printf("[   INFO   ] system languages: {%s}\n", joined.c_str());

#if IOV2_TEST_LINUX_GLIBC
    EXPECT_TRUE(system.empty());
#else
    EXPECT_FALSE(system.empty());
#endif
}

TEST_F(MessagesSystemEnv, WithNoLocaleSetTheSystemListDecides)
{
    EXPECT_EQ(selected(), m_first);
    const messages_conf<char8_t> conf("messages_system", "", true);
    ASSERT_NE(conf.translate(u8"key"), nullptr);
    EXPECT_EQ(*conf.translate(u8"key"), u8"value");
}

// LANGUAGE is a list of preferences, not a locale: when nothing in it has a catalogue
// and no locale is set, the system's list is still next.
TEST_F(MessagesSystemEnv, ALanguageListWithNoMatchStillFallsBackToTheSystem)
{
    iov2_test::sys::set_env("LANGUAGE", "fr_XX:de_XX");
    EXPECT_EQ(selected(), m_first);
}

// A locale set explicitly asks for its own language. With no catalogue for it the
// message stays untranslated, rather than turning into the system's language.
TEST_F(MessagesSystemEnv, AnExplicitLocaleKeepsTheSystemOut)
{
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"})
    {
        SCOPED_TRACE(name);
        iov2_test::sys::set_env(name, "fr_XX");
        EXPECT_EQ(selected(), "");
        iov2_test::sys::unset_env(name);
    }
}

// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/facet/collate.h>
#include <IOv2/facet/collate_details.h>
#include <IOv2/facet/ctype.h>
#include <IOv2/facet/ctype_details.h>
#include <IOv2/facet/facet_common.h>
#include <IOv2/facet/messages.h>
#include <IOv2/facet/messages_details.h>
#include <IOv2/facet/numeric.h>
#include <IOv2/facet/timeio.h>
#include <IOv2/facet/timeio_details.h>
#include <IOv2/locale/locale.h>

#include <support/exe_path.h>

#include <gtest/gtest.h>

#include <clocale>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
    // A composite facet is built by whichever of its create_rules the locale can
    // satisfy, in the order the rule lists them: collate<char> first, ctype<char>
    // only if the collate branch cannot be taken.
    struct test_ext1
    {
        using create_rules = IOv2::facet_create_rule<IOv2::collate<char>, IOv2::ctype<char>>;
        test_ext1(std::shared_ptr<IOv2::collate<char>> p_obj)
            : m_p1(std::move(p_obj))
        {}

        test_ext1(std::shared_ptr<IOv2::ctype<char>> p_obj)
            : m_p2(std::move(p_obj))
        {}

        std::shared_ptr<IOv2::collate<char>> m_p1;
        std::shared_ptr<IOv2::ctype<char>> m_p2;
    };

    // A pack is all-or-nothing: both confs must be present for this rule to fire.
    struct test_ext2
    {
        using create_rules = IOv2::facet_create_rule<IOv2::facet_create_pack<IOv2::ctype_conf<char>, IOv2::collate_conf<char>>>;

        test_ext2(std::shared_ptr<IOv2::ctype_conf<char>> p_obj1,
                  std::shared_ptr<IOv2::collate_conf<char>> p_obj2)
            : m_obj1(std::move(p_obj1))
            , m_obj2(std::move(p_obj2))
        {}

        std::shared_ptr<IOv2::ctype_conf<char>> m_obj1;
        std::shared_ptr<IOv2::collate_conf<char>> m_obj2;
    };

    // A rule whose alternatives are a single facet and a pack, so the fallback
    // branch itself depends on another composite.
    struct test_ext3
    {
        using create_rules = IOv2::facet_create_rule<IOv2::timeio_conf<char>,
                                                      IOv2::facet_create_pack<test_ext2, IOv2::numeric<char>>>;

        test_ext3(std::shared_ptr<IOv2::timeio_conf<char>> p_obj1)
            : m_obj1(std::move(p_obj1))
        {}

        test_ext3(std::shared_ptr<test_ext2> p_obj2,
                  std::shared_ptr<IOv2::numeric<char>> p_obj3)
            : m_obj2(std::move(p_obj2))
            , m_obj3(std::move(p_obj3))
        {}

        std::shared_ptr<IOv2::timeio_conf<char>> m_obj1;
        std::shared_ptr<test_ext2> m_obj2;
        std::shared_ptr<IOv2::numeric<char>> m_obj3;
    };

    // Derives from a facet the locale does hold, but declares no create_rules of
    // its own -- so nothing can synthesise it.
    struct test_ext4 : IOv2::timeio_conf<char>
    {
        using BT = IOv2::timeio_conf<char>;
        using BT::BT;
    };

    std::string resource_dir(const char* leaf)
    {
        std::filesystem::path p = exe_path();
        p = p.remove_filename() / ".." / leaf;
        return std::filesystem::canonical(p).string();
    }
}

TEST(LocaleChar, Traits)
{
    static_assert(std::is_nothrow_move_constructible_v<IOv2::locale<char>>);
    static_assert(std::is_nothrow_move_assignable_v<IOv2::locale<char>>);
    SUCCEED();
}

TEST(LocaleChar, ANamedLocaleHoldsTheConfForItsOwnCharType)
{
    auto loc = IOv2::locale<char>("C.UTF-8");

    EXPECT_TRUE(loc.has<IOv2::ctype_conf<char>>());
    EXPECT_TRUE(loc.get<IOv2::ctype_conf<char>>());
}

TEST(LocaleChar, AConfForAnotherCharTypeIsAbsent)
{
    auto loc = IOv2::locale<char>("C.UTF-8");

    EXPECT_FALSE(loc.has<IOv2::ctype_conf<wchar_t>>());
    EXPECT_FALSE(loc.get<IOv2::ctype_conf<wchar_t>>());
}

TEST(LocaleChar, RemoveDropsTheConf)
{
    auto loc = IOv2::locale<char>("C.UTF-8");

    auto loc_r = loc.remove<IOv2::ctype_conf<char>>();
    EXPECT_FALSE(loc_r.has<IOv2::ctype_conf<char>>());
    EXPECT_FALSE(loc_r.get<IOv2::ctype_conf<char>>());
}

TEST(LocaleChar, InvolveInstallsAConfiguredConf)
{
    auto loc = IOv2::locale<char>("C.UTF-8").involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc.has<IOv2::ctype_conf<char>>());
}

TEST(LocaleChar, ADerivedFacetIsBuiltOnceAndCached)
{
    auto loc1 = IOv2::locale<char>("zh_CN.UTF-8");

    EXPECT_TRUE(loc1.has<IOv2::ctype<char>>());
    auto p1 = loc1.get<IOv2::ctype<char>>();
    ASSERT_TRUE(p1);

    // The second get<>() must hand back the interned object, not rebuild it.
    auto p2 = loc1.get<IOv2::ctype<char>>();
    EXPECT_EQ(p1, p2);
}

TEST(LocaleChar, RemovingTheConfRemovesTheDerivedFacet)
{
    auto loc2 = IOv2::locale<char>("zh_CN.UTF-8").remove<IOv2::ctype_conf<char>>();

    EXPECT_FALSE(loc2.has<IOv2::ctype<char>>());
    EXPECT_FALSE(loc2.get<IOv2::ctype<char>>());
}

TEST(LocaleChar, InvolvingTheConfAgainRestoresTheDerivedFacet)
{
    auto loc2 = IOv2::locale<char>("zh_CN.UTF-8").remove<IOv2::ctype_conf<char>>();
    auto loc3 = loc2.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc3.has<IOv2::ctype<char>>());
    EXPECT_TRUE(loc3.get<IOv2::ctype<char>>());
}

TEST(LocaleChar, ACompositeTakesTheFirstRuleItCanSatisfy)
{
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext1>());
    auto p = loc1.get<test_ext1>();
    ASSERT_TRUE(p);

    EXPECT_TRUE(p->m_p1);
    EXPECT_FALSE(p->m_p2);
}

TEST(LocaleChar, ACompositeFallsBackWhenTheFirstRuleCannotBeSatisfied)
{
    auto loc2 = IOv2::locale<char>("en_US.UTF-8").remove<IOv2::collate_conf<char>>();

    EXPECT_TRUE(loc2.has<test_ext1>());
    auto p = loc2.get<test_ext1>();
    ASSERT_TRUE(p);

    EXPECT_FALSE(p->m_p1);
    EXPECT_TRUE(p->m_p2);
}

TEST(LocaleChar, ACompositeIsBuiltFromTheLocalesOwnConfs)
{
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext2>());
    auto ptr2 = loc1.get<test_ext2>();
    ASSERT_TRUE(ptr2);

    EXPECT_EQ(ptr2->m_obj1, loc1.get<IOv2::ctype_conf<char>>());
    EXPECT_EQ(ptr2->m_obj2, loc1.get<IOv2::collate_conf<char>>());
}

TEST(LocaleChar, ANestedRuleTakesItsSingleFacetBranchFirst)
{
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext3>());
    auto ptr = loc1.get<test_ext3>();
    ASSERT_TRUE(ptr);

    EXPECT_TRUE(ptr->m_obj1);
    EXPECT_EQ(ptr->m_obj1, loc1.get<IOv2::timeio_conf<char>>());
    EXPECT_FALSE(ptr->m_obj2);
    EXPECT_FALSE(ptr->m_obj3);
}

TEST(LocaleChar, ANestedRuleFallsBackToThePackOfComposites)
{
    auto loc2 = IOv2::locale<char>("en_US.UTF-8").remove<IOv2::timeio_conf<char>>();

    EXPECT_TRUE(loc2.has<test_ext3>());
    auto ptr = loc2.get<test_ext3>();
    ASSERT_TRUE(ptr);

    EXPECT_FALSE(ptr->m_obj1);
    EXPECT_TRUE(ptr->m_obj2);
    EXPECT_TRUE(ptr->m_obj3);
}

TEST(LocaleChar, RemovingOneConfLeavesTheOthersDerivable)
{
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");
    auto loc2 = loc1.remove<IOv2::timeio_conf<char>>();

    EXPECT_FALSE(loc2.has<IOv2::timeio<char>>());
    EXPECT_FALSE(loc2.get<IOv2::timeio<char>>());

    EXPECT_TRUE(loc2.has<IOv2::ctype<char>>());
    EXPECT_TRUE(loc2.get<IOv2::ctype<char>>());
}

TEST(LocaleChar, AFacetWithNoCreateRuleIsNeverSynthesised)
{
    // test_ext4 derives from a conf the locale does hold, but declares no
    // create_rules of its own, so there is nothing to build it from.
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");

    EXPECT_FALSE(loc1.has<test_ext4>());
    EXPECT_FALSE(loc1.get<test_ext4>());
}

TEST(LocaleChar, MessagesTranslateThroughTheBoundCatalogue)
{
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("IOv2TestResources"));

    auto loc = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");
    auto msg = loc.get<IOv2::messages<char>>();
    ASSERT_TRUE(msg);

    std::string ref1 = "\xe8\xaf\xb7";               //请
    std::string ref2 = "\xe8\xb0\xa2\xe8\xb0\xa2";   //谢谢
    EXPECT_EQ(msg->translate("please"), ref1);
    EXPECT_EQ(msg->translate("thank you"), ref2);
    EXPECT_EQ(msg->translate(""), "");
    EXPECT_NE(msg->head_entry(), "");
}

TEST(LocaleChar, AMissingCatalogueLeavesEveryMessageUntranslated)
{
    // The directory above bin/ holds no catalogue, so every lookup falls back to
    // the msgid and the header entry stays empty.
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("."));

    auto loc = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");
    auto msg = loc.get<IOv2::messages<char>>();
    ASSERT_TRUE(msg);

    EXPECT_EQ(msg->translate("please"), "please");
    EXPECT_EQ(msg->translate("thank you"), "thank you");
    EXPECT_EQ(msg->translate(""), "");
    EXPECT_EQ(msg->head_entry(), "");
}

TEST(LocaleChar, HasFindsACompositeAlreadyInTheCache)
{
    // has<composite>() cache-hit fast path: populating the derived-facet cache via
    // get<>() first means the following has<>() must find it in m_facets and return
    // true without rebuilding through the ft_wrapper.
    auto loc = IOv2::locale<char>("en_US.UTF-8");
    EXPECT_TRUE(loc.get<test_ext2>());
    EXPECT_TRUE(loc.has<test_ext2>());
}

TEST(LocaleChar, InvolveRejectsAnEmptyFacetPointer)
{
    auto loc = IOv2::locale<char>("en_US.UTF-8");
    EXPECT_THROW((void)loc.involve(nullptr), IOv2::stream_error);
}

TEST(LocaleChar, InitialLocaleNameRejectsAnUnresolvedCategory)
{
    // Only the five resolved LC categories are accepted; LC_ALL is not one of them.
    EXPECT_THROW((void)IOv2::locale<char>::initial_locale_name(LC_ALL), IOv2::stream_error);
}

TEST(LocaleChar, AnIdenticalInvolveMsgHandsBackTheInternedConf)
{
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("IOv2TestResources"));

    // The first involve_msg builds and interns the messages_conf; the second with an
    // identical (domain, lang, cvt) under the same bound directory must hit the cache
    // (try_get_msg's hit path, exercising msg_key equality) and hand back the very same
    // interned conf.
    auto loc1 = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");
    auto loc2 = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");

    auto c1 = loc1.get<IOv2::messages_conf<char>>();
    auto c2 = loc2.get<IOv2::messages_conf<char>>();
    ASSERT_TRUE(c1);
    ASSERT_TRUE(c2);
    EXPECT_EQ(c1, c2);
}

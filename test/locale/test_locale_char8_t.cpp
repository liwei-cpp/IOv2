// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

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

#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
    // A composite facet is built by whichever of its create_rules the locale can
    // satisfy, in the order the rule lists them: collate<char8_t> first, ctype<char8_t>
    // only if the collate branch cannot be taken.
    struct test_ext1
    {
        using create_rules = IOv2::facet_create_rule<IOv2::collate<char8_t>, IOv2::ctype<char8_t>>;
        test_ext1(std::shared_ptr<IOv2::collate<char8_t>> p_obj)
            : m_p1(std::move(p_obj))
        {}

        test_ext1(std::shared_ptr<IOv2::ctype<char8_t>> p_obj)
            : m_p2(std::move(p_obj))
        {}

        std::shared_ptr<IOv2::collate<char8_t>> m_p1;
        std::shared_ptr<IOv2::ctype<char8_t>> m_p2;
    };

    // A pack is all-or-nothing: both confs must be present for this rule to fire.
    struct test_ext2
    {
        using create_rules = IOv2::facet_create_rule<IOv2::facet_create_pack<IOv2::ctype_conf<char8_t>, IOv2::collate_conf<char8_t>>>;

        test_ext2(std::shared_ptr<IOv2::ctype_conf<char8_t>> p_obj1,
                  std::shared_ptr<IOv2::collate_conf<char8_t>> p_obj2)
            : m_obj1(std::move(p_obj1))
            , m_obj2(std::move(p_obj2))
        {}

        std::shared_ptr<IOv2::ctype_conf<char8_t>> m_obj1;
        std::shared_ptr<IOv2::collate_conf<char8_t>> m_obj2;
    };

    // A rule whose alternatives are a single facet and a pack, so the fallback
    // branch itself depends on another composite.
    struct test_ext3
    {
        using create_rules = IOv2::facet_create_rule<IOv2::timeio_conf<char8_t>,
                                                      IOv2::facet_create_pack<test_ext2, IOv2::numeric<char8_t>>>;

        test_ext3(std::shared_ptr<IOv2::timeio_conf<char8_t>> p_obj1)
            : m_obj1(std::move(p_obj1))
        {}

        test_ext3(std::shared_ptr<test_ext2> p_obj2,
                  std::shared_ptr<IOv2::numeric<char8_t>> p_obj3)
            : m_obj2(std::move(p_obj2))
            , m_obj3(std::move(p_obj3))
        {}

        std::shared_ptr<IOv2::timeio_conf<char8_t>> m_obj1;
        std::shared_ptr<test_ext2> m_obj2;
        std::shared_ptr<IOv2::numeric<char8_t>> m_obj3;
    };

    // Derives from a facet the locale does hold, but declares no create_rules of
    // its own -- so nothing can synthesise it.
    struct test_ext4 : IOv2::timeio_conf<char8_t>
    {
        using BT = IOv2::timeio_conf<char8_t>;
        using BT::BT;
    };


    // A rule whose two alternatives are the same facet template at two different
    // char types, so which branch fires depends on which of the two confs the
    // locale currently holds -- and the char8_t branch is listed second.
    struct test_ext5
    {
        using create_rules = IOv2::facet_create_rule<IOv2::ctype<char>, IOv2::ctype<char8_t>>;
        test_ext5(std::shared_ptr<IOv2::ctype<char8_t>> p_obj)
            : m_p1(std::move(p_obj))
        {}

        test_ext5(std::shared_ptr<IOv2::ctype<char>> p_obj)
            : m_p2(std::move(p_obj))
        {}

        std::shared_ptr<IOv2::ctype<char8_t>> m_p1;
        std::shared_ptr<IOv2::ctype<char>> m_p2;
    };

    std::string resource_dir(const char* leaf)
    {
        std::filesystem::path p = exe_path();
        p = p.remove_filename() / ".." / leaf;
        return std::filesystem::canonical(p).string();
    }
}

TEST(LocaleChar8, Traits)
{
    static_assert(std::is_nothrow_move_constructible_v<IOv2::locale<char8_t>>);
    static_assert(std::is_nothrow_move_assignable_v<IOv2::locale<char8_t>>);
    SUCCEED();
}

TEST(LocaleChar8, ANamedLocaleHoldsTheConfForItsOwnCharType)
{
    auto loc = IOv2::locale<char8_t>("C.UTF-8");

    EXPECT_TRUE(loc.has<IOv2::ctype_conf<char8_t>>());
    EXPECT_TRUE(loc.get<IOv2::ctype_conf<char8_t>>());
}

TEST(LocaleChar8, AConfForAnotherCharTypeIsAbsent)
{
    auto loc = IOv2::locale<char8_t>("C.UTF-8");

    EXPECT_FALSE(loc.has<IOv2::ctype_conf<char>>());
    EXPECT_FALSE(loc.get<IOv2::ctype_conf<char>>());
}

TEST(LocaleChar8, RemoveDropsTheConf)
{
    auto loc = IOv2::locale<char8_t>("C.UTF-8");

    auto loc_r = loc.remove<IOv2::ctype_conf<char8_t>>();
    EXPECT_FALSE(loc_r.has<IOv2::ctype_conf<char8_t>>());
    EXPECT_FALSE(loc_r.get<IOv2::ctype_conf<char8_t>>());
}

TEST(LocaleChar8, InvolveInstallsAConfiguredConf)
{
    auto loc = IOv2::locale<char8_t>("C.UTF-8").involve(std::make_shared<IOv2::ctype_conf<char8_t>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc.has<IOv2::ctype_conf<char8_t>>());
}

TEST(LocaleChar8, ADerivedFacetIsBuiltOnceAndCached)
{
    auto loc1 = IOv2::locale<char8_t>("zh_CN.UTF-8");

    EXPECT_TRUE(loc1.has<IOv2::ctype<char8_t>>());
    auto p1 = loc1.get<IOv2::ctype<char8_t>>();
    ASSERT_TRUE(p1);

    // The second get<>() must hand back the interned object, not rebuild it.
    auto p2 = loc1.get<IOv2::ctype<char8_t>>();
    EXPECT_EQ(p1, p2);
}

TEST(LocaleChar8, RemovingTheConfRemovesTheDerivedFacet)
{
    auto loc2 = IOv2::locale<char8_t>("zh_CN.UTF-8").remove<IOv2::ctype_conf<char8_t>>();

    EXPECT_FALSE(loc2.has<IOv2::ctype<char8_t>>());
    EXPECT_FALSE(loc2.get<IOv2::ctype<char8_t>>());
}

TEST(LocaleChar8, InvolvingTheConfAgainRestoresTheDerivedFacet)
{
    auto loc2 = IOv2::locale<char8_t>("zh_CN.UTF-8").remove<IOv2::ctype_conf<char8_t>>();
    auto loc3 = loc2.involve(std::make_shared<IOv2::ctype_conf<char8_t>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc3.has<IOv2::ctype<char8_t>>());
    EXPECT_TRUE(loc3.get<IOv2::ctype<char8_t>>());
}

TEST(LocaleChar8, ACompositeTakesTheFirstRuleItCanSatisfy)
{
    auto loc1 = IOv2::locale<char8_t>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext1>());
    auto p = loc1.get<test_ext1>();
    ASSERT_TRUE(p);

    EXPECT_TRUE(p->m_p1);
    EXPECT_FALSE(p->m_p2);
}

TEST(LocaleChar8, ACompositeFallsBackWhenTheFirstRuleCannotBeSatisfied)
{
    auto loc2 = IOv2::locale<char8_t>("en_US.UTF-8").remove<IOv2::collate_conf<char8_t>>();

    EXPECT_TRUE(loc2.has<test_ext1>());
    auto p = loc2.get<test_ext1>();
    ASSERT_TRUE(p);

    EXPECT_FALSE(p->m_p1);
    EXPECT_TRUE(p->m_p2);
}

TEST(LocaleChar8, ACompositeIsBuiltFromTheLocalesOwnConfs)
{
    auto loc1 = IOv2::locale<char8_t>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext2>());
    auto ptr2 = loc1.get<test_ext2>();
    ASSERT_TRUE(ptr2);

    EXPECT_EQ(ptr2->m_obj1, loc1.get<IOv2::ctype_conf<char8_t>>());
    EXPECT_EQ(ptr2->m_obj2, loc1.get<IOv2::collate_conf<char8_t>>());
}

TEST(LocaleChar8, ANestedRuleTakesItsSingleFacetBranchFirst)
{
    auto loc1 = IOv2::locale<char8_t>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext3>());
    auto ptr = loc1.get<test_ext3>();
    ASSERT_TRUE(ptr);

    EXPECT_TRUE(ptr->m_obj1);
    EXPECT_EQ(ptr->m_obj1, loc1.get<IOv2::timeio_conf<char8_t>>());
    EXPECT_FALSE(ptr->m_obj2);
    EXPECT_FALSE(ptr->m_obj3);
}

TEST(LocaleChar8, ANestedRuleFallsBackToThePackOfComposites)
{
    auto loc2 = IOv2::locale<char8_t>("en_US.UTF-8").remove<IOv2::timeio_conf<char8_t>>();

    EXPECT_TRUE(loc2.has<test_ext3>());
    auto ptr = loc2.get<test_ext3>();
    ASSERT_TRUE(ptr);

    EXPECT_FALSE(ptr->m_obj1);
    EXPECT_TRUE(ptr->m_obj2);
    EXPECT_TRUE(ptr->m_obj3);
}

TEST(LocaleChar8, RemovingOneConfLeavesTheOthersDerivable)
{
    auto loc1 = IOv2::locale<char8_t>("en_US.UTF-8");
    auto loc2 = loc1.remove<IOv2::timeio_conf<char8_t>>();

    EXPECT_FALSE(loc2.has<IOv2::timeio<char8_t>>());
    EXPECT_FALSE(loc2.get<IOv2::timeio<char8_t>>());

    EXPECT_TRUE(loc2.has<IOv2::ctype<char8_t>>());
    EXPECT_TRUE(loc2.get<IOv2::ctype<char8_t>>());
}

TEST(LocaleChar8, AFacetWithNoCreateRuleIsNeverSynthesised)
{
    // test_ext4 derives from a conf the locale does hold, but declares no
    // create_rules of its own, so there is nothing to build it from.
    auto loc1 = IOv2::locale<char8_t>("en_US.UTF-8");

    EXPECT_FALSE(loc1.has<test_ext4>());
    EXPECT_FALSE(loc1.get<test_ext4>());
}

TEST(LocaleChar8, ARuleSpanningTwoCharTypesTakesTheBranchItCanSatisfy)
{
    // A locale<char8_t> holds no ctype_conf<char>, so the first alternative of the
    // rule cannot fire and the char8_t one does.
    IOv2::locale<char8_t> loc1("C.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext5>());
    auto p = loc1.get<test_ext5>();
    ASSERT_TRUE(p);

    EXPECT_TRUE(p->m_p1);
    EXPECT_FALSE(p->m_p2);
}

TEST(LocaleChar8, InvolvingTheOtherCharTypesConfSwitchesTheBranch)
{
    IOv2::locale<char8_t> loc1("C.UTF-8");
    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc2.has<test_ext5>());
    auto p = loc2.get<test_ext5>();
    ASSERT_TRUE(p);

    EXPECT_FALSE(p->m_p1);
    EXPECT_TRUE(p->m_p2);
}

TEST(LocaleChar8, RemovingTheOtherCharTypesConfSwitchesTheBranchBack)
{
    IOv2::locale<char8_t> loc1("C.UTF-8");
    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    auto loc3 = loc2.remove<IOv2::ctype_conf<char>>();

    EXPECT_TRUE(loc3.has<test_ext5>());
    auto p = loc3.get<test_ext5>();
    ASSERT_TRUE(p);

    EXPECT_TRUE(p->m_p1);
    EXPECT_FALSE(p->m_p2);
}

TEST(LocaleChar8, RemovingBothConfsLeavesTheCompositeUnbuildable)
{
    IOv2::locale<char8_t> loc1("C.UTF-8");
    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    auto loc3 = loc2.remove<IOv2::ctype_conf<char>>();
    auto loc4 = loc3.remove<IOv2::ctype_conf<char8_t>>();

    EXPECT_FALSE(loc4.has<test_ext5>());
    EXPECT_FALSE(loc4.get<test_ext5>());
}

TEST(LocaleChar8, MessagesTranslateThroughTheBoundCatalogue)
{
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("IOv2TestResources"));

    // No conversion name is given: the catalogue is already UTF-8, which is what a
    // char8_t messages facet wants, so the facet must take it through unconverted.
    auto loc = IOv2::locale<char8_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<char8_t>>();
    ASSERT_TRUE(msg);

    std::u8string ref1 = u8"\xe8\xaf\xb7";               //请
    std::u8string ref2 = u8"\xe8\xb0\xa2\xe8\xb0\xa2";   //谢谢
    EXPECT_EQ(msg->translate(u8"please"), ref1);
    EXPECT_EQ(msg->translate(u8"thank you"), ref2);
    EXPECT_EQ(msg->translate(u8""), u8"");
    EXPECT_NE(msg->head_entry(), u8"");
}

TEST(LocaleChar8, AMissingCatalogueLeavesEveryMessageUntranslated)
{
    // The directory above bin/ holds no catalogue, so every lookup falls back to
    // the msgid and the header entry stays empty.
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("."));

    auto loc = IOv2::locale<char8_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<char8_t>>();
    ASSERT_TRUE(msg);

    EXPECT_EQ(msg->translate(u8"please"), u8"please");
    EXPECT_EQ(msg->translate(u8"thank you"), u8"thank you");
    EXPECT_EQ(msg->translate(u8""), u8"");
    EXPECT_EQ(msg->head_entry(), u8"");

    // The permissive failure above must not poison the cache: a strict retry of
    // the same lookup still observes and reports the missing catalogue.
    EXPECT_THROW((void)IOv2::locale<char8_t>("en_US.UTF-8")
                     .involve_msg("messages", "zh_CN", true),
                 IOv2::stream_error);
}

TEST(LocaleChar8, AnIdenticalInvolveMsgHandsBackTheInternedConf)
{
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("IOv2TestResources"));

    auto loc1 = IOv2::locale<char8_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto loc2 = IOv2::locale<char8_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");

    auto c1 = loc1.get<IOv2::messages_conf<char8_t>>();
    auto c2 = loc2.get<IOv2::messages_conf<char8_t>>();
    ASSERT_TRUE(c1);
    ASSERT_TRUE(c2);
    EXPECT_EQ(c1, c2);
}

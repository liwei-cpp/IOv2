// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
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
    // satisfy, in the order the rule lists them: collate<wchar_t> first, ctype<wchar_t>
    // only if the collate branch cannot be taken.
    struct test_ext1
    {
        using create_rules = IOv2::facet_create_rule<IOv2::collate<wchar_t>, IOv2::ctype<wchar_t>>;
        test_ext1(std::shared_ptr<IOv2::collate<wchar_t>> p_obj)
            : m_p1(std::move(p_obj))
        {}

        test_ext1(std::shared_ptr<IOv2::ctype<wchar_t>> p_obj)
            : m_p2(std::move(p_obj))
        {}

        std::shared_ptr<IOv2::collate<wchar_t>> m_p1;
        std::shared_ptr<IOv2::ctype<wchar_t>> m_p2;
    };

    // A pack is all-or-nothing: both confs must be present for this rule to fire.
    struct test_ext2
    {
        using create_rules = IOv2::facet_create_rule<IOv2::facet_create_pack<IOv2::ctype_conf<wchar_t>, IOv2::collate_conf<wchar_t>>>;

        test_ext2(std::shared_ptr<IOv2::ctype_conf<wchar_t>> p_obj1,
                  std::shared_ptr<IOv2::collate_conf<wchar_t>> p_obj2)
            : m_obj1(std::move(p_obj1))
            , m_obj2(std::move(p_obj2))
        {}

        std::shared_ptr<IOv2::ctype_conf<wchar_t>> m_obj1;
        std::shared_ptr<IOv2::collate_conf<wchar_t>> m_obj2;
    };

    // A rule whose alternatives are a single facet and a pack, so the fallback
    // branch itself depends on another composite.
    struct test_ext3
    {
        using create_rules = IOv2::facet_create_rule<IOv2::timeio_conf<wchar_t>,
                                                      IOv2::facet_create_pack<test_ext2, IOv2::numeric<wchar_t>>>;

        test_ext3(std::shared_ptr<IOv2::timeio_conf<wchar_t>> p_obj1)
            : m_obj1(std::move(p_obj1))
        {}

        test_ext3(std::shared_ptr<test_ext2> p_obj2,
                  std::shared_ptr<IOv2::numeric<wchar_t>> p_obj3)
            : m_obj2(std::move(p_obj2))
            , m_obj3(std::move(p_obj3))
        {}

        std::shared_ptr<IOv2::timeio_conf<wchar_t>> m_obj1;
        std::shared_ptr<test_ext2> m_obj2;
        std::shared_ptr<IOv2::numeric<wchar_t>> m_obj3;
    };

    // Derives from a facet the locale does hold, but declares no create_rules of
    // its own -- so nothing can synthesise it.
    struct test_ext4 : IOv2::timeio_conf<wchar_t>
    {
        using BT = IOv2::timeio_conf<wchar_t>;
        using BT::BT;
    };


    // A rule whose two alternatives are the same facet template at two different
    // char types, so which branch fires depends on which of the two confs the
    // locale currently holds -- and the wchar_t branch is listed second.
    struct test_ext5
    {
        using create_rules = IOv2::facet_create_rule<IOv2::ctype<char>, IOv2::ctype<wchar_t>>;
        test_ext5(std::shared_ptr<IOv2::ctype<wchar_t>> p_obj)
            : m_p1(std::move(p_obj))
        {}

        test_ext5(std::shared_ptr<IOv2::ctype<char>> p_obj)
            : m_p2(std::move(p_obj))
        {}

        std::shared_ptr<IOv2::ctype<wchar_t>> m_p1;
        std::shared_ptr<IOv2::ctype<char>> m_p2;
    };

    std::string resource_dir(const char* leaf)
    {
        std::filesystem::path p = exe_path();
        p = p.remove_filename() / ".." / leaf;
        return std::filesystem::canonical(p).string();
    }
}

TEST(LocaleWchar, Traits)
{
    static_assert(std::is_nothrow_move_constructible_v<IOv2::locale<wchar_t>>);
    static_assert(std::is_nothrow_move_assignable_v<IOv2::locale<wchar_t>>);
    SUCCEED();
}

TEST(LocaleWchar, ANamedLocaleHoldsTheConfForItsOwnCharType)
{
    auto loc = IOv2::locale<wchar_t>("C.UTF-8");

    EXPECT_TRUE(loc.has<IOv2::ctype_conf<wchar_t>>());
    EXPECT_TRUE(loc.get<IOv2::ctype_conf<wchar_t>>());
}

TEST(LocaleWchar, AConfForAnotherCharTypeIsAbsent)
{
    auto loc = IOv2::locale<wchar_t>("C.UTF-8");

    EXPECT_FALSE(loc.has<IOv2::ctype_conf<char>>());
    EXPECT_FALSE(loc.get<IOv2::ctype_conf<char>>());
}

TEST(LocaleWchar, RemoveDropsTheConf)
{
    auto loc = IOv2::locale<wchar_t>("C.UTF-8");

    auto loc_r = loc.remove<IOv2::ctype_conf<wchar_t>>();
    EXPECT_FALSE(loc_r.has<IOv2::ctype_conf<wchar_t>>());
    EXPECT_FALSE(loc_r.get<IOv2::ctype_conf<wchar_t>>());
}

TEST(LocaleWchar, InvolveInstallsAConfiguredConf)
{
    auto loc = IOv2::locale<wchar_t>("C.UTF-8").involve(std::make_shared<IOv2::ctype_conf<wchar_t>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc.has<IOv2::ctype_conf<wchar_t>>());
}

TEST(LocaleWchar, ADerivedFacetIsBuiltOnceAndCached)
{
    auto loc1 = IOv2::locale<wchar_t>("zh_CN.UTF-8");

    EXPECT_TRUE(loc1.has<IOv2::ctype<wchar_t>>());
    auto p1 = loc1.get<IOv2::ctype<wchar_t>>();
    ASSERT_TRUE(p1);

    // The second get<>() must hand back the interned object, not rebuild it.
    auto p2 = loc1.get<IOv2::ctype<wchar_t>>();
    EXPECT_EQ(p1, p2);
}

TEST(LocaleWchar, RemovingTheConfRemovesTheDerivedFacet)
{
    auto loc2 = IOv2::locale<wchar_t>("zh_CN.UTF-8").remove<IOv2::ctype_conf<wchar_t>>();

    EXPECT_FALSE(loc2.has<IOv2::ctype<wchar_t>>());
    EXPECT_FALSE(loc2.get<IOv2::ctype<wchar_t>>());
}

TEST(LocaleWchar, InvolvingTheConfAgainRestoresTheDerivedFacet)
{
    auto loc2 = IOv2::locale<wchar_t>("zh_CN.UTF-8").remove<IOv2::ctype_conf<wchar_t>>();
    auto loc3 = loc2.involve(std::make_shared<IOv2::ctype_conf<wchar_t>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc3.has<IOv2::ctype<wchar_t>>());
    EXPECT_TRUE(loc3.get<IOv2::ctype<wchar_t>>());
}

TEST(LocaleWchar, ACompositeTakesTheFirstRuleItCanSatisfy)
{
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext1>());
    auto p = loc1.get<test_ext1>();
    ASSERT_TRUE(p);

    EXPECT_TRUE(p->m_p1);
    EXPECT_FALSE(p->m_p2);
}

TEST(LocaleWchar, ACompositeFallsBackWhenTheFirstRuleCannotBeSatisfied)
{
    auto loc2 = IOv2::locale<wchar_t>("en_US.UTF-8").remove<IOv2::collate_conf<wchar_t>>();

    EXPECT_TRUE(loc2.has<test_ext1>());
    auto p = loc2.get<test_ext1>();
    ASSERT_TRUE(p);

    EXPECT_FALSE(p->m_p1);
    EXPECT_TRUE(p->m_p2);
}

TEST(LocaleWchar, ACompositeIsBuiltFromTheLocalesOwnConfs)
{
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext2>());
    auto ptr2 = loc1.get<test_ext2>();
    ASSERT_TRUE(ptr2);

    EXPECT_EQ(ptr2->m_obj1, loc1.get<IOv2::ctype_conf<wchar_t>>());
    EXPECT_EQ(ptr2->m_obj2, loc1.get<IOv2::collate_conf<wchar_t>>());
}

TEST(LocaleWchar, ANestedRuleTakesItsSingleFacetBranchFirst)
{
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");

    EXPECT_TRUE(loc1.has<test_ext3>());
    auto ptr = loc1.get<test_ext3>();
    ASSERT_TRUE(ptr);

    EXPECT_TRUE(ptr->m_obj1);
    EXPECT_EQ(ptr->m_obj1, loc1.get<IOv2::timeio_conf<wchar_t>>());
    EXPECT_FALSE(ptr->m_obj2);
    EXPECT_FALSE(ptr->m_obj3);
}

TEST(LocaleWchar, ANestedRuleFallsBackToThePackOfComposites)
{
    auto loc2 = IOv2::locale<wchar_t>("en_US.UTF-8").remove<IOv2::timeio_conf<wchar_t>>();

    EXPECT_TRUE(loc2.has<test_ext3>());
    auto ptr = loc2.get<test_ext3>();
    ASSERT_TRUE(ptr);

    EXPECT_FALSE(ptr->m_obj1);
    EXPECT_TRUE(ptr->m_obj2);
    EXPECT_TRUE(ptr->m_obj3);
}

TEST(LocaleWchar, RemovingOneConfLeavesTheOthersDerivable)
{
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");
    auto loc2 = loc1.remove<IOv2::timeio_conf<wchar_t>>();

    EXPECT_FALSE(loc2.has<IOv2::timeio<wchar_t>>());
    EXPECT_FALSE(loc2.get<IOv2::timeio<wchar_t>>());

    EXPECT_TRUE(loc2.has<IOv2::ctype<wchar_t>>());
    EXPECT_TRUE(loc2.get<IOv2::ctype<wchar_t>>());
}

TEST(LocaleWchar, AFacetWithNoCreateRuleIsNeverSynthesised)
{
    // test_ext4 derives from a conf the locale does hold, but declares no
    // create_rules of its own, so there is nothing to build it from.
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");

    EXPECT_FALSE(loc1.has<test_ext4>());
    EXPECT_FALSE(loc1.get<test_ext4>());
}

TEST(LocaleWchar, ARuleSpanningTwoCharTypesTakesTheBranchItCanSatisfy)
{
    // A default-constructed locale<wchar_t> holds no ctype_conf<char>, so the first
    // alternative of the rule cannot fire and the wchar_t one does.
    IOv2::locale<wchar_t> loc1;

    EXPECT_TRUE(loc1.has<test_ext5>());
    auto p = loc1.get<test_ext5>();
    ASSERT_TRUE(p);

    EXPECT_TRUE(p->m_p1);
    EXPECT_FALSE(p->m_p2);
}

TEST(LocaleWchar, InvolvingTheOtherCharTypesConfSwitchesTheBranch)
{
    IOv2::locale<wchar_t> loc1;
    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));

    EXPECT_TRUE(loc2.has<test_ext5>());
    auto p = loc2.get<test_ext5>();
    ASSERT_TRUE(p);

    EXPECT_FALSE(p->m_p1);
    EXPECT_TRUE(p->m_p2);
}

TEST(LocaleWchar, RemovingTheOtherCharTypesConfSwitchesTheBranchBack)
{
    IOv2::locale<wchar_t> loc1;
    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    auto loc3 = loc2.remove<IOv2::ctype_conf<char>>();

    EXPECT_TRUE(loc3.has<test_ext5>());
    auto p = loc3.get<test_ext5>();
    ASSERT_TRUE(p);

    EXPECT_TRUE(p->m_p1);
    EXPECT_FALSE(p->m_p2);
}

TEST(LocaleWchar, RemovingBothConfsLeavesTheCompositeUnbuildable)
{
    IOv2::locale<wchar_t> loc1;
    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    auto loc3 = loc2.remove<IOv2::ctype_conf<char>>();
    auto loc4 = loc3.remove<IOv2::ctype_conf<wchar_t>>();

    EXPECT_FALSE(loc4.has<test_ext5>());
    EXPECT_FALSE(loc4.get<test_ext5>());
}

TEST(LocaleWchar, MessagesTranslateThroughTheBoundCatalogue)
{
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("IOv2TestResources"));

    // No conversion name is given, so the facet has to reach the UTF-8 catalogue
    // through the locale's own converter rather than a named one.
    auto loc = IOv2::locale<wchar_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<wchar_t>>();
    ASSERT_TRUE(msg);

    std::wstring ref1 = L"请";
    std::wstring ref2 = L"谢谢";
    EXPECT_EQ(msg->translate(L"please"), ref1);
    EXPECT_EQ(msg->translate(L"thank you"), ref2);
    EXPECT_EQ(msg->translate(L""), L"");
    EXPECT_NE(msg->head_entry(), L"");
}

TEST(LocaleWchar, AMissingCatalogueLeavesEveryMessageUntranslated)
{
    // The directory above bin/ holds no catalogue, so every lookup falls back to
    // the msgid and the header entry stays empty.
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", resource_dir("."));

    auto loc = IOv2::locale<wchar_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<wchar_t>>();
    ASSERT_TRUE(msg);

    EXPECT_EQ(msg->translate(L"please"), L"please");
    EXPECT_EQ(msg->translate(L"thank you"), L"thank you");
    EXPECT_EQ(msg->translate(L""), L"");
    EXPECT_EQ(msg->head_entry(), L"");
}

#include <locale/locale.h>

#include <common/dump_info.h>
#include <common/exe_path.h>
#include <common/verify.h>

#include <type_traits>

void test_locale_wchar_t_traits()
{
    dump_info("Test locale<wchar_t> traits...");
    static_assert(std::is_nothrow_move_constructible_v<IOv2::locale<wchar_t>>);
    static_assert(std::is_nothrow_move_assignable_v<IOv2::locale<wchar_t>>);
    dump_info("Done\n");
}

void test_locale_wchar_t_1()
{
    dump_info("Test locale<wchar_t> case 1...");
    
    auto loc = IOv2::locale<wchar_t>("C.UTF-8");

    {
        VERIFY(loc.has<IOv2::ctype_conf<wchar_t>>());
        auto obj = loc.get<IOv2::ctype_conf<wchar_t>>();
        VERIFY(obj);
    }

    {
        VERIFY(!(loc.has<IOv2::ctype_conf<char>>()));
        auto obj = loc.get<IOv2::ctype_conf<char>>();
        VERIFY(!obj);
    }

    auto loc_r = loc.remove<IOv2::ctype_conf<wchar_t>>();
    {
        VERIFY(!(loc_r.has<IOv2::ctype_conf<wchar_t>>()));
        auto obj = loc_r.get<IOv2::ctype_conf<wchar_t>>();
        VERIFY(!obj);
    }

    dump_info("Done\n");
}

void test_locale_wchar_t_2()
{
    dump_info("Test locale<wchar_t> case 2...");
    
    auto loc = IOv2::locale<wchar_t>("C.UTF-8").involve(std::make_shared<IOv2::ctype_conf<wchar_t>>("zh_CN.UTF-8"));
    
    VERIFY(loc.has<IOv2::ctype_conf<wchar_t>>());

    dump_info("Done\n");
}

void test_locale_wchar_t_3()
{
    dump_info("Test locale<wchar_t> case 3...");
    
    auto loc1 = IOv2::locale<wchar_t>("zh_CN.UTF-8");
    {
        VERIFY(loc1.has<IOv2::ctype<wchar_t>>());
        auto p1 = loc1.get<IOv2::ctype<wchar_t>>();
        VERIFY(p1);

        auto p2 = loc1.get<IOv2::ctype<wchar_t>>();
        VERIFY(p1 == p2);
    }

    auto loc2 = loc1.remove<IOv2::ctype_conf<wchar_t>>();
    {
        VERIFY(!(loc2.has<IOv2::ctype<wchar_t>>()));
        auto p1 = loc2.get<IOv2::ctype<wchar_t>>();
        VERIFY(!p1);
    }

    auto loc3 = loc2.involve(std::make_shared<IOv2::ctype_conf<wchar_t>>("zh_CN.UTF-8"));
    {
        VERIFY(loc3.has<IOv2::ctype<wchar_t>>());
        auto p1 = loc3.get<IOv2::ctype<wchar_t>>();
        VERIFY(p1);
    }
    
    dump_info("Done\n");
}

namespace
{
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
    
    struct test_ext4 : IOv2::timeio_conf<wchar_t>
    {
        using BT = IOv2::timeio_conf<wchar_t>;
        using BT::BT;
    };

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
}

void test_locale_wchar_t_4()
{
    dump_info("Test locale<wchar_t> case 4...");
    
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");
    {
        VERIFY(loc1.has<test_ext1>());
        auto p = loc1.get<test_ext1>();

        VERIFY(p->m_p1);
        VERIFY(!(p->m_p2));
    }

    auto loc2 = loc1.remove<IOv2::collate_conf<wchar_t>>();
    {
        VERIFY(loc2.has<test_ext1>());
        auto p = loc2.get<test_ext1>();

        VERIFY(!(p->m_p1));
        VERIFY(p->m_p2);
    }
    
    dump_info("Done\n");
}

void test_locale_wchar_t_5()
{
    dump_info("Test locale<wchar_t> case 5...");
    
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");
    
    VERIFY(loc1.has<test_ext2>());
    auto ptr2 = loc1.get<test_ext2>();
    VERIFY(ptr2);

    VERIFY(ptr2->m_obj1 == loc1.get<IOv2::ctype_conf<wchar_t>>());
    VERIFY(ptr2->m_obj2 == loc1.get<IOv2::collate_conf<wchar_t>>());
    
    dump_info("Done\n");
}

void test_locale_wchar_t_6()
{
    dump_info("Test locale<wchar_t> case 6...");
    
    auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");
    {
        VERIFY(loc1.has<test_ext3>());
        auto ptr = loc1.get<test_ext3>();
        VERIFY(ptr);

        VERIFY(ptr->m_obj1);
        VERIFY(ptr->m_obj1 == loc1.get<IOv2::timeio_conf<wchar_t>>());
        VERIFY(!(ptr->m_obj2));
        VERIFY(!(ptr->m_obj3));
    }

    auto loc2 = loc1.remove<IOv2::timeio_conf<wchar_t>>();
    {
        VERIFY(loc2.has<test_ext3>());
        auto ptr = loc2.get<test_ext3>();
        VERIFY(ptr);

        VERIFY(!(ptr->m_obj1));
        VERIFY(ptr->m_obj2);
        VERIFY(ptr->m_obj3);
    }
    
    dump_info("Done\n");
}

void test_locale_wchar_t_7()
{
    dump_info("Test locale<wchar_t> case 7...");
    
    {
        auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");
        auto loc2 = loc1.remove<IOv2::timeio_conf<wchar_t>>();
        
        VERIFY(!(loc2.has<IOv2::timeio<wchar_t>>()));
        auto ptr = loc2.get<IOv2::timeio<wchar_t>>();
        VERIFY(!ptr);

        VERIFY(loc2.has<IOv2::ctype<wchar_t>>());
        auto ptr2 = loc2.get<IOv2::ctype<wchar_t>>();
        VERIFY(ptr2);
    }

    {
        auto loc1 = IOv2::locale<wchar_t>("en_US.UTF-8");
        VERIFY(!(loc1.has<test_ext4>()));
        auto ptr = loc1.get<test_ext4>();
        VERIFY(!ptr);
    }
    dump_info("Done\n");
}

void test_locale_wchar_t_8()
{
    dump_info("Test locale<wchar_t> case 8...");

    IOv2::locale<wchar_t> loc1;
    {
        VERIFY(loc1.has<test_ext5>());
        auto p = loc1.get<test_ext5>();

        VERIFY(p->m_p1);
        VERIFY(!(p->m_p2));
    }

    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    {
        VERIFY(loc2.has<test_ext5>());
        auto p = loc2.get<test_ext5>();

        VERIFY(!(p->m_p1));
        VERIFY(p->m_p2);
    }

    auto loc3 = loc2.remove<IOv2::ctype_conf<char>>();
    {
        VERIFY(loc3.has<test_ext5>());
        auto p = loc3.get<test_ext5>();

        VERIFY(p->m_p1);
        VERIFY(!(p->m_p2));
    }

    auto loc4 = loc3.remove<IOv2::ctype_conf<wchar_t>>();
    {
        VERIFY(!(loc4.has<test_ext5>()));
        auto p = loc4.get<test_ext5>();
        VERIFY(!p);
    }

    dump_info("Done\n");
}

void test_locale_wchar_t_9()
{
    dump_info("Test locale<wchar_t> case 9...");

    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    auto loc = IOv2::locale<wchar_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<wchar_t>>();

    std::wstring ref1 = L"请";
    std::wstring ref2 = L"谢谢";
    VERIFY(msg->translate(L"please") == ref1);
    VERIFY(msg->translate(L"thank you") == ref2);
    VERIFY(msg->translate(L"") == L"");
    VERIFY(msg->head_entry() != L"");

    dump_info("Done\n");
}

void test_locale_wchar_t_10()
{
    dump_info("Test locale<wchar_t> case 10...");

    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / "..";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    auto loc = IOv2::locale<wchar_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<wchar_t>>();

    VERIFY(msg->translate(L"please") == L"please");
    VERIFY(msg->translate(L"thank you") == L"thank you");
    VERIFY(msg->translate(L"") == L"");
    VERIFY(msg->head_entry() == L"");

    dump_info("Done\n");
}
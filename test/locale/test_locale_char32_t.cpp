#include <locale/locale.h>

#include <common/dump_info.h>
#include <common/exe_path.h>
#include <common/verify.h>

void test_locale_char32_t_1()
{
    dump_info("Test locale<char32_t> case 1...");
    
    auto loc = IOv2::locale<char32_t>("C.UTF-8");

    {
        if (!loc.has<IOv2::ctype_conf<char32_t>>()) throw std::runtime_error("locale::has error");
        auto obj = loc.get<IOv2::ctype_conf<char32_t>>();
        if (!obj) throw std::runtime_error("locale::get error");
    }
    
    {
        if (loc.has<IOv2::ctype_conf<char>>()) throw std::runtime_error("locale::has error");
        auto obj = loc.get<IOv2::ctype_conf<char>>();
        if (obj) throw std::runtime_error("locale::get error");
    }
    
    auto loc_r = loc.remove<IOv2::ctype_conf<char32_t>>();
    {
        if (loc_r.has<IOv2::ctype_conf<char32_t>>()) throw std::runtime_error("locale::has error");
        auto obj = loc_r.get<IOv2::ctype_conf<char32_t>>();
        if (obj) throw std::runtime_error("locale::get error");
    }

    dump_info("Done\n");
}

void test_locale_char32_t_2()
{
    dump_info("Test locale<char32_t> case 2...");
    
    auto loc = IOv2::locale<char32_t>("C.UTF-8").involve(std::make_shared<IOv2::ctype_conf<char32_t>>("zh_CN.UTF-8"));
    
    if (!loc.has<IOv2::ctype_conf<char32_t>>()) throw std::runtime_error("locale::has error");

    dump_info("Done\n");
}

void test_locale_char32_t_3()
{
    dump_info("Test locale<char32_t> case 3...");
    
    auto loc1 = IOv2::locale<char32_t>("zh_CN.UTF-8");
    {
        if (!loc1.has<IOv2::ctype<char32_t>>()) throw std::runtime_error("locale::has error");
        auto p1 = loc1.get<IOv2::ctype<char32_t>>();
        if (!p1) throw std::runtime_error("locale::get error");
        
        auto p2 = loc1.get<IOv2::ctype<char32_t>>();
        if (p1 != p2) throw std::runtime_error("locale::get error");
    }
    
    auto loc2 = loc1.remove<IOv2::ctype_conf<char32_t>>();
    {
        if (loc2.has<IOv2::ctype<char32_t>>()) throw std::runtime_error("locale::has error");
        auto p1 = loc2.get<IOv2::ctype<char32_t>>();
        if (p1) throw std::runtime_error("locale::get error");
    }
    
    auto loc3 = loc2.involve(std::make_shared<IOv2::ctype_conf<char32_t>>("zh_CN.UTF-8"));
    {
        if (!loc3.has<IOv2::ctype<char32_t>>()) throw std::runtime_error("locale::has error");
        auto p1 = loc3.get<IOv2::ctype<char32_t>>();
        if (!p1) throw std::runtime_error("locale::get error");
    }
    
    dump_info("Done\n");
}

namespace
{
    struct test_ext1
    {
        using create_rules = IOv2::facet_create_rule<IOv2::collate<char32_t>, IOv2::ctype<char32_t>>;
        test_ext1(std::shared_ptr<IOv2::collate<char32_t>> p_obj)
            : m_p1(std::move(p_obj))
        {}
        
        test_ext1(std::shared_ptr<IOv2::ctype<char32_t>> p_obj)
            : m_p2(std::move(p_obj))
        {}
        
        std::shared_ptr<IOv2::collate<char32_t>> m_p1;
        std::shared_ptr<IOv2::ctype<char32_t>> m_p2;
    };
    
    struct test_ext2
    {
        using create_rules = IOv2::facet_create_rule<IOv2::facet_create_pack<IOv2::ctype_conf<char32_t>, IOv2::collate_conf<char32_t>>>;
     
        test_ext2(std::shared_ptr<IOv2::ctype_conf<char32_t>> p_obj1,
                  std::shared_ptr<IOv2::collate_conf<char32_t>> p_obj2)
            : m_obj1(std::move(p_obj1))
            , m_obj2(std::move(p_obj2))
        {}
        
        std::shared_ptr<IOv2::ctype_conf<char32_t>> m_obj1;
        std::shared_ptr<IOv2::collate_conf<char32_t>> m_obj2;
    };
    
    struct test_ext3
    {
        using create_rules = IOv2::facet_create_rule<IOv2::timeio_conf<char32_t>,
                                                      IOv2::facet_create_pack<test_ext2, IOv2::numeric<char32_t>>>;
     
        test_ext3(std::shared_ptr<IOv2::timeio_conf<char32_t>> p_obj1)
            : m_obj1(std::move(p_obj1))
        {}
        
        test_ext3(std::shared_ptr<test_ext2> p_obj2,
                  std::shared_ptr<IOv2::numeric<char32_t>> p_obj3)
            : m_obj2(std::move(p_obj2))
            , m_obj3(std::move(p_obj3))
        {}
        
        std::shared_ptr<IOv2::timeio_conf<char32_t>> m_obj1;
        std::shared_ptr<test_ext2> m_obj2;
        std::shared_ptr<IOv2::numeric<char32_t>> m_obj3;
    };
    
    struct test_ext4 : IOv2::timeio_conf<char32_t>
    {
        using BT = IOv2::timeio_conf<char32_t>;
        using BT::BT;
    };

    struct test_ext5
    {
        using create_rules = IOv2::facet_create_rule<IOv2::ctype<char>, IOv2::ctype<char32_t>>;
        test_ext5(std::shared_ptr<IOv2::ctype<char32_t>> p_obj)
            : m_p1(std::move(p_obj))
        {}
        
        test_ext5(std::shared_ptr<IOv2::ctype<char>> p_obj)
            : m_p2(std::move(p_obj))
        {}
        
        std::shared_ptr<IOv2::ctype<char32_t>> m_p1;
        std::shared_ptr<IOv2::ctype<char>> m_p2;
    };
}

void test_locale_char32_t_4()
{
    dump_info("Test locale<char32_t> case 4...");
    
    auto loc1 = IOv2::locale<char32_t>("en_US.UTF-8");
    {
        if (!loc1.has<test_ext1>()) throw std::runtime_error("locale::has error");
        auto p = loc1.get<test_ext1>();
    
        if (!p->m_p1) throw std::runtime_error("locale::get error");
        if (p->m_p2) throw std::runtime_error("locale::get error");
    }
    
    auto loc2 = loc1.remove<IOv2::collate_conf<char32_t>>();
    {
        if (!loc2.has<test_ext1>()) throw std::runtime_error("locale::has error");
        auto p = loc2.get<test_ext1>();
    
        if (p->m_p1) throw std::runtime_error("locale::get error");
        if (!p->m_p2) throw std::runtime_error("locale::get error");
    }
    
    dump_info("Done\n");
}

void test_locale_char32_t_5()
{
    dump_info("Test locale<char32_t> case 5...");
    
    auto loc1 = IOv2::locale<char32_t>("en_US.UTF-8");
    
    if (!loc1.has<test_ext2>()) throw std::runtime_error("locale::has error");
    auto ptr2 = loc1.get<test_ext2>();
    if (!ptr2) throw std::runtime_error("locale::get error");
    
    if (ptr2->m_obj1 != loc1.get<IOv2::ctype_conf<char32_t>>()) throw std::runtime_error("locale::get error");
    if (ptr2->m_obj2 != loc1.get<IOv2::collate_conf<char32_t>>()) throw std::runtime_error("locale::get error");
    
    dump_info("Done\n");
}

void test_locale_char32_t_6()
{
    dump_info("Test locale<char32_t> case 6...");
    
    auto loc1 = IOv2::locale<char32_t>("en_US.UTF-8");
    {
        if (!loc1.has<test_ext3>()) throw std::runtime_error("locale::has error");
        auto ptr = loc1.get<test_ext3>();
        if (!ptr) throw std::runtime_error("locale::get error");
        
        if (!ptr->m_obj1) throw std::runtime_error("locale::get error");
        if (ptr->m_obj1 != loc1.get<IOv2::timeio_conf<char32_t>>()) throw std::runtime_error("locale::get error");
        if (ptr->m_obj2) throw std::runtime_error("locale::get error");
        if (ptr->m_obj3) throw std::runtime_error("locale::get error");
    }
    
    auto loc2 = loc1.remove<IOv2::timeio_conf<char32_t>>();
    {
        if (!loc2.has<test_ext3>()) throw std::runtime_error("locale::has error");
        auto ptr = loc2.get<test_ext3>();
        if (!ptr) throw std::runtime_error("locale::get error");
        
        if (ptr->m_obj1) throw std::runtime_error("locale::get error");
        if (!ptr->m_obj2) throw std::runtime_error("locale::get error");
        if (!ptr->m_obj3) throw std::runtime_error("locale::get error");
    }
    
    dump_info("Done\n");
}

void test_locale_char32_t_7()
{
    dump_info("Test locale<char32_t> case 7...");
    
    {
        auto loc1 = IOv2::locale<char32_t>("en_US.UTF-8");
        auto loc2 = loc1.remove<IOv2::timeio_conf<char32_t>>();
        
        if (loc2.has<IOv2::timeio<char32_t>>()) throw std::runtime_error("locale::has error");
        auto ptr = loc2.get<IOv2::timeio<char32_t>>();
        if (ptr) throw std::runtime_error("locale::get error");
        
        if (!loc2.has<IOv2::ctype<char32_t>>()) throw std::runtime_error("locale::has error");
        auto ptr2 = loc2.get<IOv2::ctype<char32_t>>();
        if (!ptr2) throw std::runtime_error("locale::get error");
    }

    {
        auto loc1 = IOv2::locale<char32_t>("en_US.UTF-8");
        if (loc1.has<test_ext4>()) throw std::runtime_error("locale::has error");
        auto ptr = loc1.get<test_ext4>();
        if (ptr) throw std::runtime_error("locale::get error");
    }
    dump_info("Done\n");
}

void test_locale_char32_t_8()
{
    dump_info("Test locale<char32_t> case 8...");

    IOv2::locale<char32_t> loc1;
    {
        if (!loc1.has<test_ext5>()) throw std::runtime_error("locale::has error");
        auto p = loc1.get<test_ext5>();
    
        if (!p->m_p1) throw std::runtime_error("locale::get error");
        if (p->m_p2) throw std::runtime_error("locale::get error");
    }
    
    auto loc2 = loc1.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    {
        if (!loc2.has<test_ext5>()) throw std::runtime_error("locale::has error");
        auto p = loc2.get<test_ext5>();
    
        if (p->m_p1) throw std::runtime_error("locale::get error");
        if (!p->m_p2) throw std::runtime_error("locale::get error");
    }
    
    auto loc3 = loc2.remove<IOv2::ctype_conf<char>>();
    {
        if (!loc3.has<test_ext5>()) throw std::runtime_error("locale::has error");
        auto p = loc3.get<test_ext5>();
    
        if (!p->m_p1) throw std::runtime_error("locale::get error");
        if (p->m_p2) throw std::runtime_error("locale::get error");
    }
    
    auto loc4 = loc3.remove<IOv2::ctype_conf<char32_t>>();
    {
        if (loc4.has<test_ext5>()) throw std::runtime_error("locale::has error");
        auto p = loc4.get<test_ext5>();
        if (p) throw std::runtime_error("locale::get error");
    }

    dump_info("Done\n");
}

void test_locale_char32_t_9()
{
    dump_info("Test locale<char32_t> case 9...");

    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    auto loc = IOv2::locale<char32_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<char32_t>>();

    std::u32string ref1 = U"请";
    std::u32string ref2 = U"谢谢";
    VERIFY(msg->translate(U"please") == ref1);
    VERIFY(msg->translate(U"thank you") == ref2);
    VERIFY(msg->translate(U"") == U"");
    VERIFY(msg->head_entry() != U"");

    dump_info("Done\n");
}

void test_locale_char32_t_10()
{
    dump_info("Test locale<char32_t> case 10...");

    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / "..";;
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    auto loc = IOv2::locale<char32_t>("en_US.UTF-8").involve_msg("messages", "zh_CN");
    auto msg = loc.get<IOv2::messages<char32_t>>();

    VERIFY(msg->translate(U"please") == U"please");
    VERIFY(msg->translate(U"thank you") == U"thank you");
    VERIFY(msg->translate(U"") == U"");
    VERIFY(msg->head_entry() == U"");

    dump_info("Done\n");
}
#include <locale/locale.h>

#include <support/dump_info.h>
#include <support/exe_path.h>
#include <support/verify.h>

#include <type_traits>

void test_locale_char_traits()
{
    dump_info("Test locale<char> traits...");
    static_assert(std::is_nothrow_move_constructible_v<IOv2::locale<char>>);
    static_assert(std::is_nothrow_move_assignable_v<IOv2::locale<char>>);
    dump_info("Done\n");
}

void test_locale_char_1()
{
    dump_info("Test locale<char> case 1...");
    
    auto loc = IOv2::locale<char>("C.UTF-8");

    {
        VERIFY(loc.has<IOv2::ctype_conf<char>>());
        auto obj = loc.get<IOv2::ctype_conf<char>>();
        VERIFY(obj);
    }

    {
        VERIFY(!(loc.has<IOv2::ctype_conf<wchar_t>>()));
        auto obj = loc.get<IOv2::ctype_conf<wchar_t>>();
        VERIFY(!obj);
    }

    auto loc_r = loc.remove<IOv2::ctype_conf<char>>();
    {
        VERIFY(!(loc_r.has<IOv2::ctype_conf<char>>()));
        auto obj = loc_r.get<IOv2::ctype_conf<char>>();
        VERIFY(!obj);
    }

    dump_info("Done\n");
}

void test_locale_char_2()
{
    dump_info("Test locale<char> case 2...");
    
    auto loc = IOv2::locale<char>("C.UTF-8").involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    
    VERIFY(loc.has<IOv2::ctype_conf<char>>());

    dump_info("Done\n");
}

void test_locale_char_3()
{
    dump_info("Test locale<char> case 3...");
    
    auto loc1 = IOv2::locale<char>("zh_CN.UTF-8");
    {
        VERIFY(loc1.has<IOv2::ctype<char>>());
        auto p1 = loc1.get<IOv2::ctype<char>>();
        VERIFY(p1);

        auto p2 = loc1.get<IOv2::ctype<char>>();
        VERIFY(p1 == p2);
    }

    auto loc2 = loc1.remove<IOv2::ctype_conf<char>>();
    {
        VERIFY(!(loc2.has<IOv2::ctype<char>>()));
        auto p1 = loc2.get<IOv2::ctype<char>>();
        VERIFY(!p1);
    }

    auto loc3 = loc2.involve(std::make_shared<IOv2::ctype_conf<char>>("zh_CN.UTF-8"));
    {
        VERIFY(loc3.has<IOv2::ctype<char>>());
        auto p1 = loc3.get<IOv2::ctype<char>>();
        VERIFY(p1);
    }
    
    dump_info("Done\n");
}

namespace
{
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
    
    struct test_ext4 : IOv2::timeio_conf<char>
    {
        using BT = IOv2::timeio_conf<char>;
        using BT::BT;
    };
}

void test_locale_char_4()
{
    dump_info("Test locale<char> case 4...");
    
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");
    {
        VERIFY(loc1.has<test_ext1>());
        auto p = loc1.get<test_ext1>();

        VERIFY(p->m_p1);
        VERIFY(!(p->m_p2));
    }

    auto loc2 = loc1.remove<IOv2::collate_conf<char>>();
    {
        VERIFY(loc2.has<test_ext1>());
        auto p = loc2.get<test_ext1>();

        VERIFY(!(p->m_p1));
        VERIFY(p->m_p2);
    }
    
    dump_info("Done\n");
}

void test_locale_char_5()
{
    dump_info("Test locale<char> case 5...");
    
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");
    
    VERIFY(loc1.has<test_ext2>());
    auto ptr2 = loc1.get<test_ext2>();
    VERIFY(ptr2);

    VERIFY(ptr2->m_obj1 == loc1.get<IOv2::ctype_conf<char>>());
    VERIFY(ptr2->m_obj2 == loc1.get<IOv2::collate_conf<char>>());
    
    dump_info("Done\n");
}

void test_locale_char_6()
{
    dump_info("Test locale<char> case 6...");
    
    auto loc1 = IOv2::locale<char>("en_US.UTF-8");
    {
        VERIFY(loc1.has<test_ext3>());
        auto ptr = loc1.get<test_ext3>();
        VERIFY(ptr);

        VERIFY(ptr->m_obj1);
        VERIFY(ptr->m_obj1 == loc1.get<IOv2::timeio_conf<char>>());
        VERIFY(!(ptr->m_obj2));
        VERIFY(!(ptr->m_obj3));
    }

    auto loc2 = loc1.remove<IOv2::timeio_conf<char>>();
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

void test_locale_char_7()
{
    dump_info("Test locale<char> case 7...");
    
    {
        auto loc1 = IOv2::locale<char>("en_US.UTF-8");
        auto loc2 = loc1.remove<IOv2::timeio_conf<char>>();
        
        VERIFY(!(loc2.has<IOv2::timeio<char>>()));
        auto ptr = loc2.get<IOv2::timeio<char>>();
        VERIFY(!ptr);

        VERIFY(loc2.has<IOv2::ctype<char>>());
        auto ptr2 = loc2.get<IOv2::ctype<char>>();
        VERIFY(ptr2);
    }

    {
        auto loc1 = IOv2::locale<char>("en_US.UTF-8");
        VERIFY(!(loc1.has<test_ext4>()));
        auto ptr = loc1.get<test_ext4>();
        VERIFY(!ptr);
    }
    dump_info("Done\n");
}

void test_locale_char_8()
{
    dump_info("Test locale<char> case 8...");

    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    auto loc = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");
    auto msg = loc.get<IOv2::messages<char>>();

    std::string ref1 = "\xe8\xaf\xb7";               //请
    std::string ref2 = "\xe8\xb0\xa2\xe8\xb0\xa2";   //谢谢
    VERIFY(msg->translate("please") == ref1);
    VERIFY(msg->translate("thank you") == ref2);
    VERIFY(msg->translate("") == "");
    VERIFY(msg->head_entry() != "");

    dump_info("Done\n");
}

void test_locale_char_9()
{
    dump_info("Test locale<char> case 9...");

    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / "..";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    auto loc = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");
    auto msg = loc.get<IOv2::messages<char>>();

    VERIFY(msg->translate("please") == "please");
    VERIFY(msg->translate("thank you") == "thank you");
    VERIFY(msg->translate("") == "");
    VERIFY(msg->head_entry() == "");

    dump_info("Done\n");
}

void test_locale_char_10()
{
    dump_info("Test locale<char> case 10...");

    // has<composite>() cache-hit fast path: populating the derived-facet cache via
    // get<>() first means the following has<>() must find it in m_facets and return
    // true without rebuilding through the ft_wrapper.
    {
        auto loc = IOv2::locale<char>("en_US.UTF-8");
        auto p = loc.get<test_ext2>();
        VERIFY(p);
        VERIFY(loc.has<test_ext2>());
    }

    // involve() rejects an empty facet pointer.
    {
        auto loc = IOv2::locale<char>("en_US.UTF-8");
        bool threw = false;
        try { (void)loc.involve(nullptr); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // initial_locale_name() rejects LC categories outside the five resolved ones.
    {
        bool threw = false;
        try { (void)IOv2::locale<char>::initial_locale_name(LC_ALL); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

void test_locale_char_11()
{
    dump_info("Test locale<char> case 11...");

    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    // The first involve_msg builds and interns the messages_conf; the second with an
    // identical (domain, lang, cvt) under the same bound directory must hit the cache
    // (try_get_msg's hit path, exercising msg_key equality) and hand back the very same
    // interned conf.
    auto loc1 = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");
    auto loc2 = IOv2::locale<char>("en_US.UTF-8").involve_msg("messages", "zh_CN", "zh_CN.UTF-8");

    auto c1 = loc1.get<IOv2::messages_conf<char>>();
    auto c2 = loc2.get<IOv2::messages_conf<char>>();
    VERIFY(!(!c1 || !c2));
    VERIFY(c1 == c2);

    dump_info("Done\n");
}
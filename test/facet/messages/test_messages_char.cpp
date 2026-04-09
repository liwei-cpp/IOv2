#include <facet/messages.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <common/dump_info.h>
#include <common/exe_path.h>
#include <common/verify.h>

void test_messages_facet_char_common_1()
{
    dump_info("Test messages<char> common case 1...");
    static_assert(std::is_same_v<IOv2::messages<char>::char_type, char>);

    dump_info("Done\n");
}

void test_messages_char_translate_1()
{
    dump_info("Test messages<char>::translate case 1...");
    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    const IOv2::messages<char> obj(std::make_shared<IOv2::messages_conf<char>>("messages", "zh_CN", "zh_CN.UTF-8"));

    std::string ref1 = "\xe8\xaf\xb7";               //请
    std::string ref2 = "\xe8\xb0\xa2\xe8\xb0\xa2";   //谢谢
    VERIFY(obj.translate("please") == ref1);
    VERIFY(obj.translate("thank you") == ref2);
    VERIFY(obj.translate("") == "");
    VERIFY(obj.head_entry() != "");

    dump_info("Done\n");
}

void test_messages_char_translate_2()
{
    dump_info("Test messages<char>::translate case 2...");

    const IOv2::messages<char> obj(std::make_shared<IOv2::messages_conf<char>>("messages", "zh_HK", "zh_HK", false));

    VERIFY(obj.translate("please") == "please");
    VERIFY(obj.translate("thank you") == "thank you");
    VERIFY(obj.translate("") == "");
    VERIFY(obj.head_entry() == "");

    dump_info("Done\n");
}

void test_messages_char_translate_3()
{
    dump_info("Test messages<char>::translate case 3...");
    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    const IOv2::messages<char> obj(std::make_shared<IOv2::messages_conf<char>>("messages", "zh_CN", "zh_CN.GBK"));

    std::string ref1 = "\xc7\xeb";               //请
    std::string ref2 = "\xd0\xbb\xd0\xbb";       //谢谢
    VERIFY(obj.translate("please") == ref1);
    VERIFY(obj.translate("thank you") == ref2);
    VERIFY(obj.translate("") == "");
    VERIFY(obj.head_entry() != "");

    dump_info("Done\n");
}
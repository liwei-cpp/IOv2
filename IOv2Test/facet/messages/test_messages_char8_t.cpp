#include <facet/messages.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <common/dump_info.h>
#include <common/exe_path.h>
#include <common/verify.h>

void test_messages_facet_char8_t_common_1()
{
    dump_info("Test messages<char8_t> common case 1...");
    static_assert(std::is_same_v<IOv2::messages<char8_t>::char_type, char8_t>);

    dump_info("Done\n");
}

void test_messages_char8_t_translate_1()
{
    dump_info("Test messages<char8_t>::translate case 1...");
    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2Test" / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    const IOv2::messages<char8_t> obj(std::make_shared<IOv2::messages_conf<char8_t>>("messages", "zh_CN"));

    std::u8string ref1 = u8"\xe8\xaf\xb7";               //请
    std::u8string ref2 = u8"\xe8\xb0\xa2\xe8\xb0\xa2";   //谢谢
    VERIFY(obj.translate(u8"please") == ref1);
    VERIFY(obj.translate(u8"thank you") == ref2);
    VERIFY(obj.translate(u8"") == u8"");
    VERIFY(obj.head_entry() != u8"");

    dump_info("Done\n");
}

void test_messages_char8_t_translate_2()
{
    dump_info("Test messages<char8_t>::translate case 2...");

    const IOv2::messages<char8_t> obj(std::make_shared<IOv2::messages_conf<char8_t>>("messages", "zh_HK", false));

    VERIFY(obj.translate(u8"please") == u8"please");
    VERIFY(obj.translate(u8"thank you") == u8"thank you");
    VERIFY(obj.translate(u8"") == u8"");
    VERIFY(obj.head_entry() == u8"");

    dump_info("Done\n");
}
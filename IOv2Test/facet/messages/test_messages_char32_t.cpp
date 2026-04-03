#include <facet/messages.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <common/dump_info.h>
#include <common/exe_path.h>
#include <common/verify.h>

void test_messages_facet_char32_t_common_1()
{
    dump_info("Test messages<char32_t> common case 1...");
    static_assert(std::is_same_v<IOv2::messages<char32_t>::char_type, char32_t>);

    dump_info("Done\n");
}

void test_messages_char32_t_translate_1()
{
    dump_info("Test messages<char32_t>::translate case 1...");
    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2Test" / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    const IOv2::messages<char32_t> obj(std::make_shared<IOv2::messages_conf<char32_t>>("messages", "zh_CN"));

    std::u32string ref1 = U"请";
    std::u32string ref2 = U"谢谢";
    VERIFY(obj.translate(U"please") == ref1);
    VERIFY(obj.translate(U"thank you") == ref2);
    VERIFY(obj.translate(U"") == U"");
    VERIFY(obj.head_entry() != U"");

    dump_info("Done\n");
}

void test_messages_char32_t_translate_2()
{
    dump_info("Test messages<char32_t>::translate case 2...");

    const IOv2::messages<char32_t> obj(std::make_shared<IOv2::messages_conf<char32_t>>("messages", "zh_HK", false));

    VERIFY(obj.translate(U"please") == U"please");
    VERIFY(obj.translate(U"thank you") == U"thank you");
    VERIFY(obj.translate(U"") == U"");
    VERIFY(obj.head_entry() == U"");

    dump_info("Done\n");
}
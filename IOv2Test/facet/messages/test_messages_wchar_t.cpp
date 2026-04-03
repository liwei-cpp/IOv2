#include <facet/messages.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <common/dump_info.h>
#include <common/exe_path.h>
#include <common/verify.h>

void test_messages_facet_wchar_t_common_1()
{
    dump_info("Test messages<wchar_t> common case 1...");
    static_assert(std::is_same_v<IOv2::messages<wchar_t>::char_type, wchar_t>);

    dump_info("Done\n");
}

void test_messages_wchar_t_translate_1()
{
    dump_info("Test messages<wchar_t>::translate case 1...");
    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2Test" / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    const IOv2::messages<wchar_t> obj(std::make_shared<IOv2::messages_conf<wchar_t>>("messages", "zh_CN"));

    std::wstring ref1 = L"请";
    std::wstring ref2 = L"谢谢";
    VERIFY(obj.translate(L"please") == ref1);
    VERIFY(obj.translate(L"thank you") == ref2);
    VERIFY(obj.translate(L"") == L"");
    VERIFY(obj.head_entry() != L"");

    dump_info("Done\n");
}

void test_messages_wchar_t_translate_2()
{
    dump_info("Test messages<wchar_t>::translate case 2...");

    const IOv2::messages<wchar_t> obj(std::make_shared<IOv2::messages_conf<wchar_t>>("messages", "zh_HK", false));

    VERIFY(obj.translate(L"please") == L"please");
    VERIFY(obj.translate(L"thank you") == L"thank you");
    VERIFY(obj.translate(L"") == L"");
    VERIFY(obj.head_entry() == L"");

    dump_info("Done\n");
}
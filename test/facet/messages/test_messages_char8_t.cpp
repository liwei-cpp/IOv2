#include <facet/messages.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <cstdlib>

#include <support/dump_info.h>
#include <support/exe_path.h>
#include <support/verify.h>

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
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
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

void test_messages_char8_t_translate_3()
{
    dump_info("Test messages<char8_t>::translate case 3 (colon-separated language list)...");
    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    // "fr_XX:zh_CN": fr_XX unavailable -> the while loop in match_lang advances
    // past the ':' (line 631: start = end + 1), then zh_CN is returned via the
    // trailing last_str path.
    {
        const IOv2::messages<char8_t> obj(
            std::make_shared<IOv2::messages_conf<char8_t>>("messages", "fr_XX:zh_CN"));
        VERIFY(obj.filtered_lang() == "zh_CN");
        VERIFY(obj.translate(u8"please") == u8"\xe8\xaf\xb7");
    }

    // "zh_CN:fr_XX": zh_CN is available so it is returned immediately from
    // inside the while loop (line 630: return cur_lang).
    {
        const IOv2::messages<char8_t> obj(
            std::make_shared<IOv2::messages_conf<char8_t>>("messages", "zh_CN:fr_XX"));
        VERIFY(obj.filtered_lang() == "zh_CN");
    }

    // available() with a colon-containing lang exercises the private
    // available(td, lang) overload's colon branch (the lang.find(':') path).
    VERIFY(IOv2::base_ft<IOv2::messages>::available("messages", "zh_CN:fr_XX"));
    VERIFY(!IOv2::base_ft<IOv2::messages>::available("messages", "fr_XX:zh_HK"));

    dump_info("Done\n");
}

void test_messages_char8_t_translate_4()
{
    dump_info("Test messages<char8_t>::translate case 4 (environment variable language fallback)...");
    std::filesystem::path mo_path = exe_path();
    mo_path = mo_path.remove_filename() / ".." / "IOv2TestResources";
    mo_path = std::filesystem::canonical(mo_path);
    IOv2::base_ft<IOv2::messages>::bind_text_domain("messages", mo_path.string());

    // Save the current state of the four env vars that filter_lang inspects.
    const bool had_language    = (std::getenv("LANGUAGE")    != nullptr);
    const bool had_lc_all      = (std::getenv("LC_ALL")      != nullptr);
    const bool had_lc_messages = (std::getenv("LC_MESSAGES") != nullptr);
    const bool had_lang        = (std::getenv("LANG")        != nullptr);
    const std::string saved_language    = had_language    ? std::getenv("LANGUAGE")    : "";
    const std::string saved_lc_all      = had_lc_all      ? std::getenv("LC_ALL")      : "";
    const std::string saved_lc_messages = had_lc_messages ? std::getenv("LC_MESSAGES") : "";
    const std::string saved_lang        = had_lang        ? std::getenv("LANG")        : "";

    unsetenv("LANGUAGE");
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    unsetenv("LANG");

    // LANGUAGE colon-list: "fr_XX:zh_CN" -> fr_XX fails, zh_CN succeeds.
    // Covers getenv("LANGUAGE") path (lines 655-658) plus match_lang line 631.
    setenv("LANGUAGE", "fr_XX:zh_CN", 1);
    {
        const IOv2::messages<char8_t> obj(
            std::make_shared<IOv2::messages_conf<char8_t>>("messages", ""));
        VERIFY(obj.filtered_lang() == "zh_CN");
        VERIFY(obj.translate(u8"please") == u8"\xe8\xaf\xb7");
    }
    unsetenv("LANGUAGE");

    // LC_ALL fallback: covers the for-loop path (lines 661-671).
    setenv("LC_ALL", "zh_CN", 1);
    {
        const IOv2::messages<char8_t> obj(
            std::make_shared<IOv2::messages_conf<char8_t>>("messages", ""));
        VERIFY(obj.filtered_lang() == "zh_CN");
    }
    unsetenv("LC_ALL");

    // All fallbacks absent or unavailable -> filter_lang returns "" (line 675).
    setenv("LANG", "fr_XX", 1);   // exists but no .mo file for this locale
    {
        const IOv2::messages<char8_t> obj(
            std::make_shared<IOv2::messages_conf<char8_t>>("messages", "", false));
        VERIFY(obj.filtered_lang() == "");
    }
    unsetenv("LANG");

    // Restore original env vars.
    if (had_language)    setenv("LANGUAGE",    saved_language.c_str(),    1);
    if (had_lc_all)      setenv("LC_ALL",      saved_lc_all.c_str(),      1);
    if (had_lc_messages) setenv("LC_MESSAGES", saved_lc_messages.c_str(), 1);
    if (had_lang)        setenv("LANG",        saved_lang.c_str(),        1);

    dump_info("Done\n");
}
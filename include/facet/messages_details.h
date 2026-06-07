#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/code_cvt.h>
#include <cvt/cvt_facilities.h>
#include <cvt/root_cvt.h>
#include <device/mem_device.h>
#include <facet/facet_common.h>

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace IOv2
{
template <typename CharT> class messages;

template <>
class base_ft<messages> : public abs_ft
{
    struct file_closer
    {
        file_closer(std::FILE* fp)
            : m_fp(fp) {}
        file_closer(const file_closer&) = delete;
        file_closer& operator=(const file_closer&) = delete;
        ~file_closer() { fclose(m_fp); }
    private:
        std::FILE* m_fp;
    };

public:
    static void bind_text_domain(const std::string& domain, const std::string& dirname)
    {
        std::lock_guard<std::mutex> guard(s_domain_mutex);
        auto it = s_domain_dirs.find(domain);
        if (it != s_domain_dirs.end())
            it->second = dirname;
        else
            s_domain_dirs.insert({domain, dirname});
    }

    // Resolve the domain's dirname once, then delegate to the snapshot-taking
    // overload so a single public call observes one consistent dirname even if
    // another thread rebinds the domain via bind_text_domain() concurrently.
    static bool available(const std::string& domain, const std::string& lang)
    {
        return available(get_dirname(domain), domain, lang);
    }

    static std::string filter_lang(const std::string& domain, const std::string& p_lang)
    {
        return filter_lang(get_dirname(domain), domain, p_lang);
    }

    base_ft(size_t id, const std::string& p_domain, const std::string& p_lang)
        : abs_ft(id)
        // Snapshot the dirname first (declared before the members below, so it
        // is initialised first) and feed that same snapshot to filter_lang and
        // into m_domain_info. Together with messages_conf::init reading it back
        // via dirname(), the whole construction resolves the .mo location from
        // one dirname, so the recorded domain_info can never disagree with the
        // dictionary actually loaded.
        , m_dirname(get_dirname(p_domain))
        , m_filtered_lang(filter_lang(m_dirname, p_domain, p_lang))
        , m_domain_info('[' + p_domain + "] [" + p_lang + '(' + m_filtered_lang + ')' + "] [" +
                        m_dirname + "]")
    {}

    const std::string& domain_info() const { return m_domain_info; }
    const std::string& filtered_lang() const { return m_filtered_lang; }

protected:
    // The directory bound to this domain (gettext's `dirname`), snapshotted at
    // construction. Derived configs read this so their dictionary load uses the
    // same directory that domain_info() reports.
    const std::string& dirname() const { return m_dirname; }

    static std::string get_domain_file(const std::string& dirname, const std::string& domain, const std::string& lang)
    {
        std::filesystem::path dom{dirname};
        dom = dom / lang / "LC_MESSAGES" / (domain + ".mo");
        return dom.string();
    }

    // Scope: this only implements gettext()/dgettext() semantics, i.e. a
    // plain one-to-one (msgid -> msgstr) lookup.
    //
    // Each .mo string is therefore read as a NUL-terminated C string: the
    // record's stored `length` may be longer (GNU gettext packs extra data
    // into a single record using embedded NUL/EOT separators), but anything
    // past the first '\0' is intentionally ignored here. That matches gettext's
    // own lookup, which compares msgids with strcmp() and so stops at the first
    // NUL, and it keeps the map key identical to what a caller passes in.
    //
    // Encoding: strings are assumed to be UTF-8 (the default that GNU gettext
    // >= 0.22's msgfmt produces). The .mo charset declared in the header
    // entry's "Content-Type: ...; charset=..." is neither read nor honoured
    // here, so a .mo in another encoding (a legacy file, or one built with
    // msgfmt --no-convert) is NOT transcoded: its non-ASCII msgstrs would be
    // misdecoded. msgids are unaffected, as gettext recommends they be US-ASCII.
    //
    // Consequently the following are NOT supported (their extra payload is
    // dropped on purpose, not by accident):
    //   - ngettext()/dngettext() plural forms: the original is stored as
    //     "msgid\0msgid_plural" and the translation as "msgstr[0]\0msgstr[1]..".
    //     Supporting them needs the header's "Plural-Forms:" rule, length-based
    //     (not C-string) record storage, and an n-aware lookup API.
    //   - pgettext() message contexts: the original is stored as
    //     "msgctxt\x04msgid" (EOT-separated, no NUL), so the key here is the
    //     whole "msgctxt\x04msgid" blob; a context-aware API would need to
    //     assemble that key explicitly.
    // Revisit this function (and the lookup API) if ngettext/msgctxt support is
    // ever required.
    static std::unordered_map<std::u8string, std::u8string> get_translate_dictionary(const std::string& filename)
    {
        std::FILE* fp = fopen(filename.c_str(), "rb");
        if (!fp)
            throw stream_error("get_translate_dictionary fail: cannot open file " + filename);

        file_closer guard(fp);

        if (std::fseek(fp, 0, SEEK_END) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");
        const long file_size = std::ftell(fp);
        if (file_size < 0)
            throw stream_error("get_translate_dictionary fail: file inconsistent");
        if (std::fseek(fp, 0, SEEK_SET) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");

        auto read_num = [fp](unsigned char* buf, bool need_swap)
        {
            if (std::fread(buf, 1, 4, fp) != 4)
                throw stream_error("get_translate_dictionary fail: invalid format");

            std::uint32_t res = static_cast<std::uint32_t>(buf[0])
                              | (static_cast<std::uint32_t>(buf[1]) << 8)
                              | (static_cast<std::uint32_t>(buf[2]) << 16)
                              | (static_cast<std::uint32_t>(buf[3]) << 24);
            if (need_swap)
                res = std::byteswap(res);
            return res;
        };

        unsigned char buff[8];
        if (std::fread(buff, 1, 8, fp) != 8)
            throw stream_error("get_translate_dictionary fail: invalid format");

        bool need_swap = false;
        if ((buff[0] == 0x95) && (buff[1] == 0x04) && (buff[2] == 0x12) && (buff[3] == 0xde))
            need_swap = (std::endian::native == std::endian::little);
        else if ((buff[0] == 0xde) && (buff[1] == 0x12) && (buff[2] == 0x04) && (buff[3] == 0x95))
            need_swap = (std::endian::native == std::endian::big);
        else throw stream_error("get_translate_dictionary fail: invalid format");

        if ((buff[4] != 0) || (buff[5] != 0) || (buff[6] != 0) || (buff[7] != 0))
            throw stream_error("get_translate_dictionary fail: invalid format");

        auto str_num = read_num(buff, need_swap);
        auto ori_offset = read_num(buff, need_swap);
        auto aim_offset = read_num(buff, need_swap);

        // Each entry needs an 8-byte (length, offset) descriptor in both the
        // original and translation tables, so a valid str_num cannot exceed
        // file_size / 16. Rejecting larger counts caps the O(str_num) growth of
        // oris and the result map, which the per-string length cap does not.
        if (str_num > static_cast<std::uint64_t>(file_size) / 16u)
            throw stream_error("get_translate_dictionary fail: implausible string count");

        // The .mo file offsets (and string lengths) are unsigned 32-bit, but
        // std::fseek takes a signed long. Where long is 64-bit (e.g. LP64
        // Linux) every uint32_t is representable, so all seeks below are exact.
        // On a 32-bit-long platform (LLP64/ILP32) an offset >= 2 GiB would
        // convert to a negative long and seek wrongly — but that can only arise
        // from a > 2 GiB or corrupt .mo, and every fseek/fread return value is
        // checked, so the worst case is a clean "invalid format" throw, never a
        // misread or out-of-bounds access.
        if (std::fseek(fp, ori_offset, SEEK_SET) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");

        // The .mo format stores each string length as an unbounded 32-bit
        // integer, so the buffer is grown to fit each entry rather than
        // assuming a fixed maximum. A sanity cap guards against a corrupt or
        // malicious file requesting a huge allocation.
        constexpr std::uint32_t k_max_str_len = 64u * 1024u * 1024u; // 64 MiB
        std::vector<char8_t> str_buf;
        std::vector<std::u8string> oris;
        for (size_t i = 0; i < str_num; ++i)
        {
            auto length = read_num(buff, need_swap);
            auto offset = read_num(buff, need_swap);
            auto cur_pos = std::ftell(fp);
            if (cur_pos == -1L) throw stream_error("get_translate_dictionary fail: file inconsistent");

            if (length > k_max_str_len)
                throw stream_error("get_translate_dictionary fail: string too long");
            if (str_buf.size() < static_cast<size_t>(length) + 1)
                str_buf.resize(static_cast<size_t>(length) + 1);

            if (std::fseek(fp, offset, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");

            if (std::fread(str_buf.data(), 1, length, fp) != length)
                throw stream_error("get_translate_dictionary fail: invalid format");

            str_buf[length] = u8'\0';
            oris.push_back(str_buf.data());

            if (std::fseek(fp, cur_pos, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");
        }

        if (std::fseek(fp, aim_offset, SEEK_SET) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");
        std::unordered_map<std::u8string, std::u8string> res;

        for (size_t i = 0; i < str_num; ++i)
        {
            auto length = read_num(buff, need_swap);
            auto offset = read_num(buff, need_swap);
            auto cur_pos = std::ftell(fp);
            if (cur_pos == -1L) throw stream_error("get_translate_dictionary fail: file inconsistent");

            if (length > k_max_str_len)
                throw stream_error("get_translate_dictionary fail: string too long");
            if (str_buf.size() < static_cast<size_t>(length) + 1)
                str_buf.resize(static_cast<size_t>(length) + 1);

            if (std::fseek(fp, offset, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");

            if (std::fread(str_buf.data(), 1, length, fp) != length)
                throw stream_error("get_translate_dictionary fail: invalid format");

            str_buf[length] = u8'\0';
            res.insert({oris[i], str_buf.data()});

            if (std::fseek(fp, cur_pos, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");
        }

        return res;
    }

private:
    // Snapshot-taking implementations: the dirname is resolved once by the
    // caller and threaded through here, so neither recursion into available()
    // nor the language scan re-reads s_domain_dirs.
    static bool available(const std::string& dirname, const std::string& domain, const std::string& lang)
    {
        if (lang.empty() || lang.find(':') != std::string::npos)
        {
            std::string resolved = filter_lang(dirname, domain, lang);
            if (resolved.empty())
                return false;
            return std::filesystem::exists(get_domain_file(dirname, domain, resolved));
        }
        return std::filesystem::exists(get_domain_file(dirname, domain, lang));
    }

    static std::string filter_lang(const std::string& dirname, const std::string& domain, const std::string& p_lang)
    {
        auto match_lang = [&dirname, &domain](const std::string& str_lang)
        {
            std::size_t start = 0;
            std::size_t end;
            while ((end = str_lang.find(':', start)) != std::string::npos)
            {
                auto cur_lang = str_lang.substr(start, end - start);
                if ((!cur_lang.empty()) && (base_ft<messages>::available(dirname, domain, cur_lang)))
                    return cur_lang;
                start = end + 1;
            }

            auto last_str = str_lang.substr(start);
            if ((!last_str.empty()) && (base_ft<messages>::available(dirname, domain, last_str)))
                return last_str;

            return std::string{};
        };

        if (p_lang.empty())
        {
            // Environment fallback (mirrors gettext): LANGUAGE, then
            // LC_ALL / LC_MESSAGES / LANG. std::getenv is NOT synchronised
            // against a concurrent setenv()/putenv() on another thread — the
            // returned pointer and the bytes it addresses may be invalidated
            // mid-read — and neither C++ nor POSIX offers a portable
            // thread-safe environment read, so a lock here could not close the
            // race against mutators elsewhere. This code therefore assumes the
            // process environment is not modified concurrently with messages
            // facet construction (true for the usual "configure locale once at
            // startup" usage). Each value is copied into a std::string at once
            // to keep the read window minimal.
            std::string res;
            if (const char* p = std::getenv("LANGUAGE"))
            {
                res = match_lang(p);
                if (!res.empty()) return res;
            }

            for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"})
            {
                if (const char* p = std::getenv(var); p && p[0] != '\0')
                {
                    res = p;
                    // Unlike LANGUAGE, these are a single locale name, not a
                    // ':'-separated list (gettext never splits them). A value
                    // containing ':' is therefore not a valid locale name:
                    // skip it and try the next variable.
                    if (res.find(':') == std::string::npos && base_ft<messages>::available(dirname, domain, res))
                        return res;
                }
            }

            return "";
        }
        else
            return match_lang(p_lang);
    }

    inline static std::unordered_map<std::string, std::string> s_domain_dirs;
    inline static std::mutex s_domain_mutex;

    static std::string get_dirname(const std::string& domain)
    {
        const static std::string def_dir = "/usr/share/locale";

        std::lock_guard<std::mutex> guard(s_domain_mutex);
        auto it = s_domain_dirs.find(domain);
        return (it == s_domain_dirs.end()) ? def_dir : it->second;
    }

private:
    // Declared first: initialised before m_filtered_lang / m_domain_info, both of
    // which depend on this snapshot.
    const std::string m_dirname;
    const std::string m_filtered_lang;
    const std::string m_domain_info;
};

template <typename CharT> class messages_conf;

template <>
class messages_conf<char8_t> : public ft_basic<messages<char8_t>>
{
public:
    messages_conf(const std::string& domain, const std::string& lang, bool throw_if_fail = true)
        : ft_basic<messages<char8_t>>(domain, lang)
        , m_dict(init(this->dirname(), domain, this->filtered_lang(), throw_if_fail))
    {}

    // Returns a pointer to the translation, or nullptr when `ori` is not in the
    // dictionary. Returning a pointer (rather than `ori` on a miss) keeps this
    // lookup from ever aliasing the caller's argument, so it neither dangles nor
    // copies; the messages facet layers the gettext pass-through (return the
    // original on a miss) on top, where it can do so safely per value category.
    // nullptr unambiguously means "not found", distinct from a found translation
    // that happens to be empty.
    virtual const std::u8string* translate(const std::u8string& ori) const
    {
        auto it = m_dict.find(ori);
        return it != m_dict.end() ? &it->second : nullptr;
    }

private:
    static std::unordered_map<std::u8string, std::u8string> init(const std::string& dirname, const std::string& domain, const std::string& lang, bool throw_if_fail)
    {
        try
        {
            if (lang.empty())
                throw stream_error("messages_conf init fail: no language available");
            else
                return get_translate_dictionary(get_domain_file(dirname, domain, lang));
        }
        catch(...)
        {
            if (throw_if_fail)
                throw;
        }
        return std::unordered_map<std::u8string, std::u8string>{};
    }

private:
    const std::unordered_map<std::u8string, std::u8string> m_dict;
};

template <typename CharT>
    requires std::is_same_v<CharT, char32_t> ||
                (std::is_same_v<CharT, wchar_t> &&
                 wchar_t_is_utf32)
class messages_conf<CharT> : public ft_basic<messages<CharT>>
{
    using TString = std::basic_string<CharT>;
public:
    messages_conf(const std::string& domain, const std::string& lang, bool throw_if_fail = true)
        : ft_basic<messages<CharT>>(domain, lang)
        , m_dict(init(this->dirname(), domain, this->filtered_lang(), throw_if_fail))
    {}

private:
    static std::unordered_map<TString, TString> init(const std::string& dirname, const std::string& domain, const std::string& lang, bool throw_if_fail)
    {
        try
        {
            if (lang.empty())
                throw stream_error("messages_conf init fail: no language available");

            std::unordered_map<TString, TString> res;
            auto tmp_dict = base_ft<messages>::get_translate_dictionary(base_ft<messages>::get_domain_file(dirname, domain, lang));
            for (const auto& [k, v] : tmp_dict)
            {
                const std::u32string uk = detail::to_u32string(k.c_str());
                const std::u32string uv = detail::to_u32string(v.c_str());
                res.insert({TString(uk.begin(), uk.end()),
                            TString(uv.begin(), uv.end())});
            }
            return res;
        }
        catch(...)
        {
            if (throw_if_fail)
                throw;
        }
        return std::unordered_map<TString, TString>{};
    }

public:
    // See the char8_t specialization: nullptr means "not found".
    virtual const TString* translate(const TString& ori) const
    {
        auto it = m_dict.find(ori);
        return it != m_dict.end() ? &it->second : nullptr;
    }

private:
    const std::unordered_map<TString, TString> m_dict;
};

template <>
class messages_conf<char> : public ft_basic<messages<char>>
{
public:
    messages_conf(const std::string& domain, const std::string& lang, const std::string& cvt_ft, bool throw_if_fail = true)
        : ft_basic<messages<char>>(domain, lang)
        , m_dict(init(this->dirname(), domain, this->filtered_lang(), cvt_ft, throw_if_fail))
    {}

    // See the char8_t specialization: nullptr means "not found".
    virtual const std::string* translate(const std::string& ori) const
    {
        auto it = m_dict.find(ori);
        return it != m_dict.end() ? &it->second : nullptr;
    }

private:
    static std::unordered_map<std::string, std::string> init(const std::string& dirname, const std::string& domain, const std::string& lang, const std::string& cvt_ft, bool throw_if_fail)
    {
        try
        {
            if constexpr (wchar_t_is_utf32)
            {
                if (lang.empty())
                    throw stream_error("messages_conf init fail: no language available");

                std::unordered_map<std::string, std::string> res;
                auto tmp_dict = get_translate_dictionary(get_domain_file(dirname, domain, lang));

                auto cvt = IOv2::code_cvt_creator<char, wchar_t>(cvt_ft).create(rb_root_cvt{mem_device{""}});

                for (const auto& [k, v] : tmp_dict)
                {
                    const std::u32string uk = detail::to_u32string(k.c_str());
                    const std::u32string uv = detail::to_u32string(v.c_str());
                    std::wstring wk(uk.begin(), uk.end());
                    std::wstring wv(uv.begin(), uv.end());

                    cvt.bos(); cvt.main_cont_beg();
                    cvt.put(wk.data(), wk.size());
                    auto [dev_k, err_k] = cvt.detach();
                    cvt.attach(mem_device{""});
                    if (err_k) std::rethrow_exception(err_k);
                    std::string ck = dev_k.str();

                    cvt.bos(); cvt.main_cont_beg();
                    cvt.put(wv.data(), wv.size());
                    auto [dev_v, err_v] = cvt.detach();
                    cvt.attach(mem_device{""});
                    if (err_v) std::rethrow_exception(err_v);
                    std::string cv = dev_v.str();

                    res.insert({std::move(ck), std::move(cv)});
                }
                return res;
            }
            else
                throw stream_error("messages_conf init error: unsupported wchar_t / char32_t");
        }
        catch(...)
        {
            if (throw_if_fail)
                throw;
        }
        return std::unordered_map<std::string, std::string>{};
    }

private:
    const std::unordered_map<std::string, std::string> m_dict;
};
}

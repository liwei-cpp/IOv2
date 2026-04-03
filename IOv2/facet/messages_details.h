#pragma once
#include <bit>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <list>
#include <vector>

#include <common/metafunctions.h>
#include <common/defs.h>
#include <cvt/cvt_facilities.h>
#include <cvt/root_cvt.h>
#include <cvt/code_cvt.h>
#include <device/mem_device.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>

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
        file_closer& operator= (const file_closer&) = delete;
        ~file_closer() { fclose(m_fp); }
    private:
        std::FILE* m_fp;
    };

public:
    static void bind_text_domain(const std::string& domain, const std::string& val)
    {
        auto it = domain_map().find(domain);
        if (it != domain_map().end())
            it->second = val;
        else
            domain_map().insert({domain, val});
    }

    static bool available(const std::string& domain, const std::string& lang)
    {
        if (lang.empty() || lang.find(':') != std::string::npos)
        {
            return available(domain, filter_lang(domain, lang));
        }
        return std::filesystem::exists(get_domain_file(domain, lang));
    }

    static std::string filter_lang(const std::string& domain, const std::string& p_lang)
    {
        auto match_lang = [&domain](const std::string& str_lang)
        {
            std::size_t start = 0;
            std::size_t end;
            while ((end = str_lang.find(':', start)) != std::string::npos)
            {
                auto cur_lang = str_lang.substr(start, end - start);
                if (base_ft<messages>::available(domain, cur_lang))
                    return cur_lang;
                start = end + 1;
            }

            auto last_str = str_lang.substr(start);
            if ((!last_str.empty()) && (base_ft<messages>::available(domain, last_str)))
                return last_str;

            return std::string{};
        };

        if (p_lang.empty())
        {
            std::string res;
            if (std::getenv("LANGUAGE") != nullptr)
            {
                res = match_lang(std::getenv("LANGUAGE"));
                if (!res.empty()) return res;
            }

            res = std::setlocale(LC_ALL, nullptr);
            if ((!res.empty()) && (base_ft<messages>::available(domain, res))) return res;

            res = std::setlocale(LC_MESSAGES, nullptr);
            if ((!res.empty()) && (base_ft<messages>::available(domain, res))) return res;

            res = std::getenv("LANG");
            if ((!res.empty()) && (base_ft<messages>::available(domain, res))) return res;
            return "";
        }
        else
            return match_lang(p_lang);
    }

    base_ft(size_t id, const std::string& p_domain, const std::string& p_lang)
        : abs_ft(id)
        , m_filtered_lang(filter_lang(p_domain, p_lang))
        , m_domain_info('[' + p_domain + "] [" + p_lang + '(' + m_filtered_lang + ')' + "] [" + 
                        get_text_domain(p_domain) + "]")
    {}

    const std::string& domain_info() const { return m_domain_info; }
    const std::string& filtered_lang() const { return m_filtered_lang; }

protected:
    static std::string get_domain_file(const std::string& domain, const std::string& lang)
    {
        std::filesystem::path dom{get_text_domain(domain)};
        dom = dom / lang / "LC_MESSAGES" / (domain + ".mo");
        return dom.string();
    }

    static std::unordered_map<std::u8string, std::u8string> get_translate_dictionary(const std::string& filename)
    {
        std::FILE* fp = fopen(filename.c_str(), "rb");
        if (!fp)
            throw stream_error("get_translate_dictionary fail: cannot open file" + filename);

        file_closer guard(fp);
        auto read_num = [fp](unsigned char* buf, bool need_swap)
        {
            if (std::fread(buf, 1, 4, fp) != 4)
                throw stream_error("get_translate_dictionary fail: invalid format");

            std::uint16_t res = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
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

        if (std::fseek(fp, ori_offset, SEEK_SET) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");

        std::vector<char> str_buf(65536 + 1);
        std::vector<std::u8string> oris;
        for (size_t i = 0; i < str_num; ++i)
        {
            auto length = read_num(buff, need_swap);
            auto offset = read_num(buff, need_swap);
            auto cur_pos = std::ftell(fp);
            if (cur_pos == -1L) throw stream_error("get_translate_dictionary fail: file inconsistent");
        
            if (std::fseek(fp, offset, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");
        
            if (std::fread(str_buf.data(), 1, length, fp) != length)
                throw stream_error("get_translate_dictionary fail: invalid format");
        
            str_buf[length] = '\0';
            oris.push_back((char8_t*)(str_buf.data()));
        
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
        
            if (std::fseek(fp, offset, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");
        
            if (std::fread(str_buf.data(), 1, length, fp) != length)
                throw stream_error("get_translate_dictionary fail: invalid format");
        
            str_buf[length] = '\0';
            res.insert({oris[i], (char8_t*)(str_buf.data())});
        
            if (std::fseek(fp, cur_pos, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");
        }
    
        return res;
    }

private:
    static std::unordered_map<std::string, std::string>& domain_map()
    {
        static std::unordered_map<std::string, std::string> inst;
        return inst;
    }

    static std::string get_text_domain(const std::string& domain)
    {
        const static std::string def_path = "/usr/share/locale";

        auto it = domain_map().find(domain);
        return (it == domain_map().end()) ? def_path : it->second;
    }

private:
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
        , m_dict(init(domain, this->filtered_lang(), throw_if_fail))
    {}

    virtual const std::u8string& translate(const std::u8string& ori) const
    {
        if (auto it = m_dict.find(ori); it != m_dict.end())
            return it->second;
        return ori;
    }

private:
    static std::unordered_map<std::u8string, std::u8string> init(const std::string& domain, const std::string& lang, bool throw_if_fail)
    {
        try
        {
            if (lang.empty())
                throw stream_error("messages_conf init fail: no language available");
            else
                return get_translate_dictionary(get_domain_file(domain, lang));
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
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class messages_conf<CharT> : public ft_basic<messages<CharT>>
{
    using TString = std::basic_string<CharT>;
public:
    messages_conf(const std::string& domain, const std::string& lang, bool throw_if_fail = true)
        : ft_basic<messages<CharT>>(domain, lang)
        , m_dict(init(domain, this->filtered_lang(), throw_if_fail))
    {}

private:
    static std::unordered_map<TString, TString> init(const std::string& domain, const std::string& lang, bool throw_if_fail)
    {
        try
        {
            if (lang.empty())
                throw stream_error("messages_conf init fail: no language available");

            std::unordered_map<TString, TString> res;
            auto tmp_dict = base_ft<messages>::get_translate_dictionary(base_ft<messages>::get_domain_file(domain, lang));
            for (const auto& [k, v] : tmp_dict)
                res.insert({(const CharT *)(to_u32string(k.c_str()).c_str()),
                            (const CharT *)(to_u32string(v.c_str()).c_str())});
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
    virtual const TString& translate(const TString& ori) const
    {
        if (auto it = m_dict.find(ori); it != m_dict.end())
            return it->second;
        return ori;
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
        , m_dict(init(domain, this->filtered_lang(), cvt_ft, throw_if_fail))
    {}

    virtual const std::string& translate(const std::string& ori) const
    {
        if (auto it = m_dict.find(ori); it != m_dict.end())
            return it->second;
        return ori;
    }

private:
    static std::unordered_map<std::string, std::string> init(const std::string& domain, const std::string& lang, const std::string& cvt_ft, bool throw_if_fail)
    {
        try
        {
            if constexpr ((sizeof(char32_t) == sizeof(wchar_t)) && 
                          (static_cast<wchar_t>(U'李') == L'李') &&
                          (static_cast<char32_t>(L'伟') == U'伟'))
            {
                if (lang.empty())
                    throw stream_error("messages_conf init fail: no language available");

                std::unordered_map<std::string, std::string> res;
                auto tmp_dict = get_translate_dictionary(get_domain_file(domain, lang));

                auto cvt = IOv2::code_cvt_creator<char, wchar_t>(cvt_ft).create(make_root_cvt<true>(mem_device{""}));

                for (const auto& [k, v] : tmp_dict)
                {
                    std::wstring wk = (const wchar_t *)(to_u32string(k.c_str()).c_str());
                    std::wstring wv = (const wchar_t *)(to_u32string(v.c_str()).c_str());

                    cvt.bos(); cvt.main_cont_beg();
                    cvt.put(wk.data(), wk.size());
                    std::string ck = cvt.attach(mem_device{""}).str();

                    cvt.bos(); cvt.main_cont_beg();
                    cvt.put(wv.data(), wv.size());
                    std::string cv = cvt.attach(mem_device{""}).str();

                    res.insert({std::move(ck), std::move(cv)});
                }
                return res;
            }
            else
                throw stream_error("messages_conf init error: upsurpotted wchar_t / char32_t");
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
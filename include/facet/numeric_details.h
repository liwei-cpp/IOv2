#pragma once
#include <langinfo.h>
#include <cstring>
#include <string>
#include <vector>

#include <facet/facet_common.h>
#include <facet/facet_helper.h>
#include <common/clocale_wrapper.h>
#include <cvt/cvt_facilities.h>

namespace IOv2
{
template <typename CharT> class numeric_conf;
template <typename CharT> class numeric;

template <>
class numeric_conf<char> : public ft_basic<numeric<char>>
{
public:
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = '.';
            m_thousands_sep = ',';
            m_true_name = "true";
            m_false_name = "false";
        }
        else
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            
            m_decimal_point = FacetHelper::string_to_char_convert(nl_langinfo(DECIMAL_POINT), name);
            m_thousands_sep = FacetHelper::string_to_char_convert(nl_langinfo(THOUSANDS_SEP), name);
                    
            if (m_thousands_sep != '\0')
            {
                const char* src = nl_langinfo(GROUPING);
                const size_t len = strlen(src);
                if (len != 0)
                {
                    m_grouping.resize(len);
                    for (size_t i = 0; i < len; ++i)
                        m_grouping[i] = (uint8_t)(src[i]);
                }
            }
            
            auto yesStr = nl_langinfo(YESSTR);
            m_true_name = yesStr ? yesStr : "true";
            
            auto noStr = nl_langinfo(NOSTR);
            m_false_name = noStr ? noStr : "false";
        }
    }
    
public:
    virtual char decimal_point() const { return m_decimal_point; }
    virtual char thousands_sep() const { return m_thousands_sep; }
    virtual const std::string& truename() const { return m_true_name; }
    virtual const std::string& falsename() const { return m_false_name; }
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }
    
private:
    char m_decimal_point;
    char m_thousands_sep;
    std::string m_true_name;
    std::string m_false_name;
    std::vector<uint8_t> m_grouping;
};

template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> || 
                (std::is_same_v<CharT, char32_t> && 
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class numeric_conf<CharT> : public ft_basic<numeric<CharT>>
{
public:
    using char_type = CharT;
    
public:
    numeric_conf(const std::string& name)
        : ft_basic<numeric<CharT>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            if constexpr (std::is_same_v<CharT, wchar_t>)
            {
                m_decimal_point = L'.';
                m_thousands_sep = L',';
                m_true_name = L"true";
                m_false_name = L"false";
            }
            else
            {
                m_decimal_point = U'.';
                m_thousands_sep = U',';
                m_true_name = U"true";
                m_false_name = U"false";
            }
            return;
        }

        clocale_wrapper inter_locale(name.c_str());
        clocale_user guard(inter_locale);

        m_decimal_point = FacetHelper::nl_langinfo_char<CharT>(DECIMAL_POINT, name, static_cast<CharT>('.'));
        m_thousands_sep = FacetHelper::nl_langinfo_char<CharT>(THOUSANDS_SEP, name, static_cast<CharT>('\0'));
                
        if (m_thousands_sep != '\0')
        {
            const char* src = nl_langinfo(GROUPING);
            const size_t len = strlen(src);
            if (len != 0)
            {
                m_grouping.resize(len);
                for (size_t i = 0; i < len; ++i)
                    m_grouping[i] = (uint8_t)(src[i]);
            }
        }
        
        auto yesStr = nl_langinfo(YESSTR);
        if (yesStr == nullptr)
        {
            if constexpr (std::is_same_v<CharT, wchar_t>)
                m_true_name = L"true";
            else
                m_true_name = U"true";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_true_name = to_wstring(yesStr, name);
            else
                m_true_name = to_u32string(yesStr, name);
        }
        
        auto noStr = nl_langinfo(NOSTR);
        if (noStr == nullptr)
        {
            if constexpr (std::is_same_v<CharT, wchar_t>)
                m_false_name = L"false";
            else
                m_false_name = U"false";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_false_name = to_wstring(noStr, name);
            else
                m_false_name = to_u32string(noStr, name);
        }
    }
    
public:
    virtual CharT decimal_point() const { return m_decimal_point; }
    virtual CharT thousands_sep() const { return m_thousands_sep; }
    virtual const std::basic_string<CharT>& truename() const { return m_true_name; }
    virtual const std::basic_string<CharT>& falsename() const { return m_false_name; }
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }
    
private:
    CharT m_decimal_point;
    CharT m_thousands_sep;
    std::basic_string<CharT>    m_true_name;
    std::basic_string<CharT>    m_false_name;
    std::vector<uint8_t>        m_grouping;
};

template <>
class numeric_conf<char8_t> : public ft_basic<numeric<char8_t>>
{
public:
    using char_type = char8_t;
    
public:
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char8_t>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = u8'.';
            m_thousands_sep = u8',';
            m_true_name = u8"true";
            m_false_name = u8"false";
            return;
        }

        numeric_conf<char32_t> numeric_temp(name);
        {
            const auto input = numeric_temp.decimal_point();
            auto output = to_u8string(input);
            m_decimal_point = (output.size() != 1) ? u8'.' : output[0];
        }

        {
            const auto input = numeric_temp.thousands_sep();
            auto output = to_u8string(input);
            m_thousands_sep = (output.size() != 1) ? u8',' : output[0];
        }

        m_true_name = to_u8string(numeric_temp.truename());
        m_false_name = to_u8string(numeric_temp.falsename());

        m_grouping = numeric_temp.grouping();
    }
    
public:
    virtual char8_t decimal_point() const { return m_decimal_point; }
    virtual char8_t thousands_sep() const { return m_thousands_sep; }
    virtual const std::basic_string<char8_t>& truename() const { return m_true_name; }
    virtual const std::basic_string<char8_t>& falsename() const { return m_false_name; }
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }
    
private:
    char8_t m_decimal_point;
    char8_t m_thousands_sep;
    std::basic_string<char8_t>  m_true_name;
    std::basic_string<char8_t>  m_false_name;
    std::vector<uint8_t>        m_grouping;
};
}
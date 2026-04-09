#pragma once
#include <string>
#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <device/mem_device.h>

namespace IOv2
{
    inline std::wstring to_wstring(const std::string& val, const std::string& locale_name)
    {
        using cvt_type = code_cvt<root_cvt<mem_device<char>, true>, wchar_t>;
        cvt_type tmp_cvt(make_root_cvt<true>(mem_device(val)), locale_name);
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();
        
        std::wstring res;
        wchar_t res_buf[8];
        while (true)
        {
            auto c = tmp_cvt.get(res_buf, 8);
            if (c == 0) break;
            res.insert(res.size(), res_buf, c);
        };

        return res;
    }

    inline std::u32string to_u32string(const char* val, const std::string& locale_name)
    {
        using cvt_type = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
        cvt_type tmp_cvt(make_root_cvt<true>(mem_device(val)), locale_name);
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();
        
        std::u32string res;
        char32_t res_buf[8];
        while (true)
        {
            auto c = tmp_cvt.get(res_buf, 8);
            if (c == 0) break;
            res.insert(res.size(), res_buf, c);
        };

        return res;
    }

    inline std::u32string to_u32string(const char8_t* val)
    {
        using cvt_type = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
        cvt_type tmp_cvt(make_root_cvt<true>(mem_device(val)));
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();
        
        std::u32string res;
        char32_t res_buf[8];
        while (true)
        {
            auto c = tmp_cvt.get(res_buf, 8);
            if (c == 0) break;
            res.insert(res.size(), res_buf, c);
        };

        return res;
    }

    inline std::u8string to_u8string(const std::u32string& val)
    {
        using cvt_type = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
        cvt_type tmp_cvt(make_root_cvt<true>(mem_device<char8_t>()));
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        tmp_cvt.put(val.data(), val.size());
        return tmp_cvt.detach().str();
    }

    inline std::u8string to_u8string(char32_t val)
    {
        using cvt_type = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
        cvt_type tmp_cvt(make_root_cvt<true>(mem_device(u8"")));
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();
        
        tmp_cvt.put(&val, 1);
        tmp_cvt.flush();
        return tmp_cvt.device().str();
    }
}
#pragma once
#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <device/mem_device.h>

#include <array>
#include <string>

namespace IOv2
{
    inline std::wstring to_wstring(const std::string& val, const std::string& locale_name)
    {
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device(val)}, locale_name);
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        std::wstring res;
        std::array<wchar_t, 8> res_buf{};
        while (true)
        {
            auto c = tmp_cvt.get(res_buf.data(), res_buf.size());
            if (c == 0) break;
            res.insert(res.size(), res_buf.data(), c);
        };

        return res;
    }

    inline std::u32string to_u32string(const char* val, const std::string& locale_name)
    {
        if (val == nullptr) [[unlikely]] return {};
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device(val)}, locale_name);
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        std::u32string res;
        std::array<char32_t, 8> res_buf{};
        while (true)
        {
            auto c = tmp_cvt.get(res_buf.data(), res_buf.size());
            if (c == 0) break;
            res.insert(res.size(), res_buf.data(), c);
        };

        return res;
    }

    inline std::u32string to_u32string(const char8_t* val)
    {
        if (val == nullptr) [[unlikely]] return {};
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device(val)});
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        std::u32string res;
        std::array<char32_t, 8> res_buf{};
        while (true)
        {
            auto c = tmp_cvt.get(res_buf.data(), res_buf.size());
            if (c == 0) break;
            res.insert(res.size(), res_buf.data(), c);
        };

        return res;
    }

    inline std::u8string to_u8string(const std::u32string& val)
    {
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device<char8_t>()});
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        tmp_cvt.put(val.data(), val.size());
        return tmp_cvt.detach().str();
    }

    inline std::u8string to_u8string(char32_t val)
    {
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device<char8_t>()});
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        tmp_cvt.put(&val, 1);
        return tmp_cvt.detach().str();
    }
}
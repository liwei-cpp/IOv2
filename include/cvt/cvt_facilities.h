#pragma once
#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <device/mem_device.h>

#include <array>
#include <string>
#include <string_view>

namespace IOv2
{
    /**
     * @brief Convert a locale-encoded narrow string to a wide string.
     *
     * @param val          Input bytes interpreted under @p locale_name.
     * @param locale_name  Locale name passed to the underlying codecvt_kernel.
     *
     * @return Decoded wide string.
     *
     * @throws cvt_error If @p val ends in the middle of a multi-byte sequence
     *                   (partial trailing bytes), or contains an invalid
     *                   sequence under the locale. This is NOT a best-effort
     *                   conversion — callers must pass a complete byte stream.
     */
    inline std::wstring to_wstring(std::string_view val, const std::string& locale_name)
    {
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device(std::string(val))}, locale_name);
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        std::wstring res;
        std::array<wchar_t, 8> res_buf{};
        while (true)
        {
            auto c = tmp_cvt.get(res_buf.data(), res_buf.size());
            if (c == 0) break;
            res.insert(res.size(), res_buf.data(), c);
        }

        return res;
    }

    /**
     * @brief Convert a locale-encoded narrow string to UTF-32.
     *
     * @param val          Input bytes interpreted under @p locale_name.
     * @param locale_name  Locale name passed to the underlying codecvt_kernel.
     *
     * @return Decoded UTF-32 string.
     *
     * @throws cvt_error If @p val ends in the middle of a multi-byte sequence
     *                   (partial trailing bytes), or contains an invalid
     *                   sequence under the locale. This is NOT a best-effort
     *                   conversion — callers must pass a complete byte stream.
     */
    inline std::u32string to_u32string(std::string_view val, const std::string& locale_name)
    {
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device(std::string(val))}, locale_name);
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        std::u32string res;
        std::array<char32_t, 8> res_buf{};
        while (true)
        {
            auto c = tmp_cvt.get(res_buf.data(), res_buf.size());
            if (c == 0) break;
            res.insert(res.size(), res_buf.data(), c);
        }

        return res;
    }

    /**
     * @brief Convert a UTF-8 string to UTF-32.
     *
     * @param val Input UTF-8 bytes.
     *
     * @return Decoded UTF-32 string.
     *
     * @throws cvt_error If @p val ends in the middle of a UTF-8 sequence
     *                   (partial trailing bytes), or contains any invalid
     *                   UTF-8 (overlong, surrogate, out-of-range, malformed
     *                   continuation). This is NOT a best-effort conversion —
     *                   callers must pass a complete, well-formed UTF-8 stream.
     */
    inline std::u32string to_u32string(std::u8string_view val)
    {
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device(std::u8string(val))});
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        std::u32string res;
        std::array<char32_t, 8> res_buf{};
        while (true)
        {
            auto c = tmp_cvt.get(res_buf.data(), res_buf.size());
            if (c == 0) break;
            res.insert(res.size(), res_buf.data(), c);
        }

        return res;
    }

    /**
     * @brief Convert a UTF-32 string to UTF-8.
     *
     * @param val Input string. Every element must be a Unicode Scalar Value:
     *            a code point in [0, 0x10FFFF] and outside the surrogate
     *            range [0xD800, 0xDFFF]. `std::u32string` does not enforce
     *            this — callers are responsible for sanitizing input.
     *
     * @return UTF-8 encoded string.
     *
     * @throws cvt_error If `val` contains any non-USV element. On throw,
     *                   no partial result is observable.
     */
    inline std::u8string to_u8string(const std::u32string& val)
    {
        using cvt_type = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
        cvt_type tmp_cvt(rb_root_cvt{mem_device<char8_t>()});
        tmp_cvt.bos();
        tmp_cvt.main_cont_beg();

        tmp_cvt.put(val.data(), val.size());
        return tmp_cvt.detach().str();
    }

    /**
     * @brief Convert a single UTF-32 code point to UTF-8.
     *
     * @param val A Unicode Scalar Value: a code point in [0, 0x10FFFF] and
     *            outside the surrogate range [0xD800, 0xDFFF].
     *
     * @return UTF-8 encoded representation of `val`.
     *
     * @throws cvt_error If `val` is not a USV.
     */
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

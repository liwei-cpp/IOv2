/**
 * @file cvt_facilities.h
 * @lang{ZH}
 * 转换辅助工具函数，提供字符编码转换的便捷接口。
 * 所有函数均位于 IOv2::detail 命名空间，属于内部实现细节，不构成公开 API。
 * @endif
 *
 * @lang{EN}
 * Conversion facility functions providing convenient interfaces for character
 * encoding conversions. All functions reside in the IOv2::detail namespace
 * and are internal implementation details, not part of the public API.
 * @endif
 */
#pragma once
#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <device/mem_device.h>

#include <array>
#include <string>
#include <string_view>

namespace IOv2::detail
{
    /**
     * @lang{ZH}
     * 将区域设置编码的窄字符串转换为宽字符串。
     *
     * @param val          在 @p locale_name 区域设置下解释的输入字节序列。
     * @param locale_name  传递给底层 codecvt_kernel 的区域设置名称。
     *
     * @return 解码后的宽字符串。
     *
     * @throws cvt_error 若 @p val 在多字节序列中途结束（末尾存在不完整字节），
     *                   或在该区域设置下包含无效序列，则抛出此异常。
     *                   本函数不进行尽力转换（best-effort），调用方必须传入完整的字节流。
     * @endif
     *
     * @lang{EN}
     * Convert a locale-encoded narrow string to a wide string.
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
     * @endif
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
     * @lang{ZH}
     * 将区域设置编码的窄字符串转换为 UTF-32 字符串。
     *
     * @param val          在 @p locale_name 区域设置下解释的输入字节序列。
     * @param locale_name  传递给底层 codecvt_kernel 的区域设置名称。
     *
     * @return 解码后的 UTF-32 字符串。
     *
     * @throws cvt_error 若 @p val 在多字节序列中途结束（末尾存在不完整字节），
     *                   或在该区域设置下包含无效序列，则抛出此异常。
     *                   本函数不进行尽力转换（best-effort），调用方必须传入完整的字节流。
     * @endif
     *
     * @lang{EN}
     * Convert a locale-encoded narrow string to UTF-32.
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
     * @endif
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
     * @lang{ZH}
     * 将 UTF-8 字符串转换为 UTF-32 字符串。
     *
     * @param val 输入的 UTF-8 字节序列。
     *
     * @return 解码后的 UTF-32 字符串。
     *
     * @throws cvt_error 若 @p val 在 UTF-8 序列中途结束（末尾存在不完整字节），
     *                   或包含任何无效的 UTF-8 编码（超长编码、代理码点、超出范围、
     *                   格式错误的续接字节），则抛出此异常。
     *                   本函数不进行尽力转换（best-effort），调用方必须传入完整且格式正确的 UTF-8 流。
     * @endif
     *
     * @lang{EN}
     * Convert a UTF-8 string to UTF-32.
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
     * @endif
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
     * @lang{ZH}
     * 将 UTF-32 字符串转换为 UTF-8 字符串。
     *
     * @param val 输入字符串。每个元素必须是 Unicode 标量值（Unicode Scalar Value，USV）：
     *            即码点值在 [0, 0x10FFFF] 范围内，且不属于代理区间 [0xD800, 0xDFFF]。
     *            `std::u32string` 本身不强制此约束，调用方有责任在传入前完成输入校验。
     *
     * @return UTF-8 编码的字符串。
     *
     * @throws cvt_error 若 `val` 包含任何非 USV 元素，则抛出此异常。
     *                   抛出异常时，不会产生任何可观测的部分结果。
     * @endif
     *
     * @lang{EN}
     * Convert a UTF-32 string to UTF-8.
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
     * @endif
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
     * @lang{ZH}
     * 将单个 UTF-32 码点转换为 UTF-8 字符串。
     *
     * @param val 一个 Unicode 标量值（USV）：码点值在 [0, 0x10FFFF] 范围内，
     *            且不属于代理区间 [0xD800, 0xDFFF]。
     *
     * @return `val` 对应的 UTF-8 编码表示。
     *
     * @throws cvt_error 若 `val` 不是有效的 USV，则抛出此异常。
     * @endif
     *
     * @lang{EN}
     * Convert a single UTF-32 code point to UTF-8.
     *
     * @param val A Unicode Scalar Value: a code point in [0, 0x10FFFF] and
     *            outside the surrogate range [0xD800, 0xDFFF].
     *
     * @return UTF-8 encoded representation of `val`.
     *
     * @throws cvt_error If `val` is not a USV.
     * @endif
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

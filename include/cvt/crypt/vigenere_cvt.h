/**
 * @file vigenere_cvt.h
 * @lang{ZH}
 * 维吉尼亚密码转换器（仅供教学和演示用途）。
 *
 * @warning **请勿用于生产安全场景。**
 *
 * 本文件提供两个组件：
 * - `vigenere_cvt`：基于维吉尼亚多表替换密码的双向流转换器，
 *   支持读（解密）和写（加密）两种方向，以及可选的定位操作。
 * - `vigenere_cvt_creator`：工厂类，用于在管道中统一构造 `vigenere_cvt` 实例。
 * @endif
 *
 * @lang{EN}
 * Vigenère cipher converter, for educational and demonstration purposes only.
 *
 * @warning **NOT FOR PRODUCTION SECURITY USE.**
 *
 * This file provides two components:
 * - `vigenere_cvt`: A bidirectional stream converter based on the Vigenère
 *   polyalphabetic substitution cipher, supporting both read (decryption) and
 *   write (encryption) directions as well as optional positioning.
 * - `vigenere_cvt_creator`: A factory class for constructing `vigenere_cvt`
 *   instances within a pipeline.
 * @endif
 */

#pragma once
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2::Crypt::Classic
{
/**
 * @lang{ZH}
 * 维吉尼亚密码转换器（仅供教学和演示用途）。
 *
 * @warning **请勿用于生产安全场景。**
 *
 * 维吉尼亚密码是一种历史性多表替换密码，自 19 世纪以来已被彻底破解，
 * 不提供任何密码学安全性，绝不应用于保护敏感数据。
 *
 * 已知漏洞：
 * - Kasiski 检验可仅凭密文推断密钥长度。
 * - 频率分析可将每个位置视为简单的凯撒密码逐一破解。
 * - 对已知明文攻击和选择明文攻击无任何抵抗力。
 *
 * 如需真正的加密能力，请使用 `chacha20_cvt` 或其他现代密码。
 *
 * 本类仅适用于：
 * - 教学目的（了解古典密码学）。
 * - 简单混淆（非安全用途）。
 * - 历史/演示目的。
 * @endif
 *
 * @lang{EN}
 * Vigenère cipher converter, for educational and demonstration purposes only.
 *
 * @warning **NOT FOR PRODUCTION SECURITY USE.**
 *
 * The Vigenère cipher is a historical polyalphabetic substitution cipher that
 * has been completely broken since the 19th century. It provides NO cryptographic
 * security and should NEVER be used to protect sensitive data.
 *
 * Known vulnerabilities:
 * - Kasiski examination can determine key length from ciphertext alone.
 * - Frequency analysis breaks each position as a simple Caesar cipher.
 * - No resistance to known-plaintext or chosen-plaintext attacks.
 *
 * For actual encryption needs, use `chacha20_cvt` or other modern ciphers.
 *
 * This class is provided for:
 * - Educational purposes (learning about classical ciphers).
 * - Simple obfuscation (NOT security).
 * - Historical/demonstration purposes.
 *
 * @see chacha20_cvt
 * @endif
 */
template <io_converter KernelType>
    requires std::is_integral_v<typename KernelType::internal_type>
class vigenere_cvt : public abs_cvt<vigenere_cvt<KernelType>, KernelType, typename KernelType::internal_type, true, true>
{
    using BT = abs_cvt<vigenere_cvt<KernelType>, KernelType, typename KernelType::internal_type, true, true>;
    friend BT;  // for put_main, get_main, and private CRTP hooks
    constexpr static size_t s_buf_len = 16;
public:
    using device_type = typename KernelType::device_type;
    using internal_type = typename KernelType::internal_type;
    using external_type = internal_type;

public:
    /**
     * @lang{ZH}
     * 构造维吉尼亚密码转换器。
     * @endif
     *
     * @lang{EN}
     * Constructs a Vigenère cipher converter.
     * @endif
     *
     * @param dev
     * @lang{ZH} 底层 IO 转换核心，以移动语义传入。 @endif
     * @lang{EN} The underlying IO converter kernel, transferred by move. @endif
     *
     * @param s
     * @lang{ZH} 密钥字符串，不得为空；其字符类型须与 `internal_type` 一致。 @endif
     * @lang{EN} The key string; must not be empty; its character type must match
     * `internal_type`. @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若 `s` 为空。 @endif
     * @lang{EN} If `s` is empty. @endif
     */
    vigenere_cvt(KernelType dev, std::basic_string_view<internal_type> s)
        : BT(std::move(dev))
        , m_key([s]
                {
                    if (s.empty())
                        throw cvt_error("vigenere_cvt: cannot create vigenere converter with empty key");
                    return std::vector<internal_type>(s.begin(), s.end());
                }())
    {}

private:
    /**
     * @lang{ZH}
     * `abs_cvt::detach()` 的 CRTP 钩子，在 kernel 层 `detach()` 之前调用。
     *
     * 将密钥偏移量 `m_pos` 归零，确保后续 `attach()` 后从密钥起始位置开始加解密。
     * 本操作不会抛出异常，始终返回 `nullptr`。
     * 本函数为 `noexcept`，此约束由 `abs_cvt` 的 `static_assert` 强制。
     *
     * @return 始终为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::detach()`, called before the kernel-level `detach()`.
     *
     * Resets the key-offset position `m_pos` to zero so that a subsequent `attach()`
     * starts encryption/decryption from the beginning of the key.
     * This operation never throws and always returns `nullptr`.
     * Must be `noexcept` — enforced by a `static_assert` in `abs_cvt`.
     *
     * @return Always `nullptr`.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        m_pos = 0;
        return nullptr;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::attach()` 的 CRTP 钩子，在 kernel 层 `attach()` 之后调用。
     *
     * 验证密钥非空，防止已移动走的对象被误用。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::attach()`, called after the kernel-level `attach()`.
     *
     * Validates that the key is non-empty, guarding against misuse of a moved-from object.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若密钥为空（已移动走的对象）。 @endif
     * @lang{EN} If the key is empty (moved-from object). @endif
     */
    void attach_impl()
    {
        if (m_key.empty())
            throw cvt_error("vigenere_cvt::attach fail: empty key");
    }

    /**
     * @lang{ZH}
     * `abs_cvt::main_cont_beg()` 的 CRTP 钩子，在 kernel 层 `main_cont_beg()` 之后调用。
     *
     * 将密钥偏移量 `m_pos` 归零，使主内容阶段的加解密从密钥起始位置开始。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::main_cont_beg()`, called after the kernel-level
     * `main_cont_beg()`.
     *
     * Resets the key-offset position `m_pos` to zero so that main-content
     * encryption/decryption starts from the beginning of the key.
     * @endif
     */
    void main_cont_beg_impl()
    {
        m_pos = 0;
    }

    /**
     * @lang{ZH}
     * 在无符号域内执行模减法，返回 `(a - b) mod 2^N`，用于解密。
     *
     * 使用无符号运算以避免有符号整数溢出未定义行为（例如当 `internal_type` 为
     * `int32_t` 时）。回转为 `internal_type` 在 C++20 二补数保证下行为明确。
     * 与 `add_wrap` 互为逆运算，确保加解密在 mod-2^N 群中精确互逆。
     * @endif
     *
     * @lang{EN}
     * Performs modular subtraction in the unsigned domain, returning `(a - b) mod 2^N`,
     * used for decryption.
     *
     * Uses unsigned arithmetic to avoid signed-integer-overflow UB (e.g. when
     * `internal_type` is `int32_t`). The back-cast to `internal_type` is well-defined
     * under C++20's two's-complement guarantee. Is the exact inverse of `add_wrap`,
     * ensuring encryption and decryption are precise inverses in the mod-2^N group.
     * @endif
     */
    static constexpr internal_type sub_wrap(internal_type a, internal_type b) noexcept
    {
        using U = std::make_unsigned_t<internal_type>;
        return static_cast<internal_type>(static_cast<U>(a) - static_cast<U>(b));
    }
    /**
     * @lang{ZH}
     * 在无符号域内执行模加法，返回 `(a + b) mod 2^N`，用于加密。
     *
     * 与 `sub_wrap` 互为逆运算，确保加解密在 mod-2^N 群中精确互逆。
     * @endif
     *
     * @lang{EN}
     * Performs modular addition in the unsigned domain, returning `(a + b) mod 2^N`,
     * used for encryption.
     *
     * Is the exact inverse of `sub_wrap`, ensuring encryption and decryption are
     * precise inverses in the mod-2^N group.
     * @endif
     */
    static constexpr internal_type add_wrap(internal_type a, internal_type b) noexcept
    {
        using U = std::make_unsigned_t<internal_type>;
        return static_cast<internal_type>(static_cast<U>(a) + static_cast<U>(b));
    }

    /**
     * @lang{ZH}
     * 解密主内容流并以 `internal_type` 元素为单位输出。
     *
     * 从 `reader` 读取密文，按密钥循环位置对每个元素执行 `sub_wrap`（减去密钥字符），
     * 输出到用户缓冲区 `to`。密钥偏移量 `m_pos` 在每次元素处理后递增。
     * @endif
     *
     * @lang{EN}
     * Decrypts the main-content stream and delivers it in `internal_type`-aligned units.
     *
     * Reads ciphertext from `reader` and applies `sub_wrap` (subtracts the key character
     * at the current cyclic key position) to each element, writing the result to the
     * user buffer `to`. The key-offset `m_pos` is incremented after each element.
     * @endif
     *
     * @param[in,out] reader The buffered reader.
     * @param[out]    to     The user output buffer.
     * @param[in]     to_max The maximum number of elements to deliver.
     *
     * @return
     * @lang{ZH} 实际解密并写入用户缓冲的元素数量。 @endif
     * @lang{EN} The number of elements actually decrypted into the user buffer. @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若密钥为空（已移动走的对象）。 @endif
     * @lang{EN} If the key is empty (moved-from object). @endif
     */
    size_t get_main(cvt_reader<KernelType>& reader, internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (to_max == 0) return 0;
        if (m_key.empty())
            throw cvt_error("vigenere_cvt: key not initialized (moved-from object?)");

        size_t total_count = 0;
        reader.reset(s_buf_len);
        while (total_count != to_max)
        {
            const size_t dest_size = std::min(s_buf_len, to_max - total_count);
            auto [ptr, cur_size] = reader.get_buf(dest_size);
            if (cur_size == 0)
                return total_count;

            for (size_t i = 0; i < cur_size; ++i)
            {
                size_t pos = (m_pos++) % m_key.size();
                const internal_type src = *ptr++;
                *to++ = sub_wrap(src, m_key[pos]);
            }
            total_count += cur_size;
        }
        return total_count;
    }

    /**
     * @lang{ZH}
     * 加密主内容流并写入 kernel。
     *
     * 对用户缓冲区 `to` 中的每个元素执行 `add_wrap`（加上密钥循环位置处的密钥字符），
     * 通过 `writer` 将密文提交到 kernel。密钥偏移量 `m_pos` 在每次元素处理后递增。
     * @endif
     *
     * @lang{EN}
     * Encrypts the main-content stream and writes it to the kernel.
     *
     * Applies `add_wrap` (adds the key character at the current cyclic key position)
     * to each element in the user buffer `to`, then commits the ciphertext to the
     * kernel via `writer`. The key-offset `m_pos` is incremented after each element.
     * @endif
     *
     * @param[in,out] writer  The buffered writer.
     * @param[in]     to      The user input buffer to encrypt.
     * @param[in]     to_size The number of `internal_type` elements to encrypt.
     *
     * @throws cvt_error
     * @lang{ZH} 若密钥为空（已移动走的对象）。 @endif
     * @lang{EN} If the key is empty (moved-from object). @endif
     */
    void put_main(cvt_writer<KernelType>& writer, const internal_type* to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (to_size == 0) return;
        if (m_key.empty())
            throw cvt_error("vigenere_cvt: key not initialized (moved-from object?)");

        writer.reset(s_buf_len);

        size_t total_count = 0;
        while (total_count != to_size)
        {
            const size_t dest_size = std::min(s_buf_len, to_size - total_count);
            auto ptr = writer.put_buf(dest_size);

            for (size_t i = 0; i < dest_size; ++i)
            {
                size_t pos = (m_pos++) % m_key.size();
                const internal_type src = *to++;
                *ptr++ = add_wrap(src, m_key[pos]);
            }
            total_count += dest_size;
        }
        // commit() is the responsibility of abs_cvt::put.
    }

    /**
     * @lang{ZH}
     * `abs_cvt::tell()` 的 CRTP 钩子，返回当前密钥偏移量。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::tell()`. Returns the current key-offset.
     * @endif
     *
     * @return
     * @lang{ZH} 当前密钥偏移量 `m_pos`。 @endif
     * @lang{EN} The current key-offset `m_pos`. @endif
     */
    [[nodiscard]] size_t tell_impl() const
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        return m_pos;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::seek()` 的 CRTP 钩子，将 kernel 定位到绝对位置 `pos`。
     *
     * 调用 `m_kernel.seek(pos)` 后通过 `m_kernel.tell()` 重新同步 `m_pos`，
     * 而非直接使用 `pos`。原因：kernel 可能因对齐、夹紧或内部缓冲而实际落在
     * 不同位置；`tell()` 返回 kernel 的真实位置，确保密钥流偏移与字节流偏移对齐。
     * 若 `seek()` 抛出，仍尝试 `tell()` 以保持同步；若 `tell()` 也抛出，
     * 则无法确定 kernel 的实际位置，密钥流将失同步 —— 必须 taint。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::seek()`, positioning the kernel to the absolute
     * position `pos`.
     *
     * Calls `m_kernel.seek(pos)` and then re-syncs `m_pos` via `m_kernel.tell()`
     * rather than using `pos` directly. This is necessary because the kernel may
     * land on a different position due to alignment, clamping, or internal buffering;
     * `tell()` returns the kernel's actual position, keeping the keystream offset
     * aligned with the byte stream. If `seek()` throws, `tell()` is still attempted
     * to maintain synchronisation; if `tell()` also throws, the kernel's actual
     * position is indeterminate and the keystream would desync — the converter must taint.
     * @endif
     *
     * @param pos
     * @lang{ZH} 目标绝对字节偏移量。 @endif
     * @lang{EN} The target absolute byte offset. @endif
     *
     * @throws
     * @lang{ZH}
     * 重新抛出 `seek()` 的异常（若有）；若 `tell()` 抛出则抛出 `tell()` 的异常
     * （`seek()` 的异常被丢弃，因为 `tell()` 失败意味着位置不可知）。
     * @endif
     * @lang{EN}
     * Re-throws the `seek()` exception (if any); if `tell()` throws, throws the
     * `tell()` exception (the `seek()` exception is discarded because a `tell()`
     * failure means the position is indeterminate).
     * @endif
     */
    void seek_impl(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        std::exception_ptr seek_err;
        try { BT::m_kernel.seek(pos); }
        catch (...) { seek_err = std::current_exception(); }

        try { m_pos = BT::m_kernel.tell(); }
        catch (...)
        {
            BT::set_tainted();
            if (seek_err) std::rethrow_exception(seek_err);
            throw;
        }

        if (seek_err) std::rethrow_exception(seek_err);
    }

    /**
     * @lang{ZH}
     * `abs_cvt::rseek()` 的 CRTP 钩子，将 kernel 相对当前位置移动 `pos` 字节。
     *
     * 逻辑与 `seek_impl()` 完全相同，但调用 `m_kernel.rseek(pos)` 执行相对定位。
     * 详见 `seek_impl()` 的文档。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::rseek()`, positioning the kernel relative to the
     * current position by `pos` bytes.
     *
     * The logic is identical to `seek_impl()`, but calls `m_kernel.rseek(pos)` for
     * relative positioning. See `seek_impl()` for the full rationale.
     * @endif
     *
     * @param pos
     * @lang{ZH} 相对于当前位置的字节偏移量。 @endif
     * @lang{EN} Byte offset relative to the current position. @endif
     */
    void rseek_impl(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        // See seek_impl() above for the same resync-via-tell rationale.
        std::exception_ptr rseek_err;
        try { BT::m_kernel.rseek(pos); }
        catch (...) { rseek_err = std::current_exception(); }

        try { m_pos = BT::m_kernel.tell(); }
        catch (...)
        {
            BT::set_tainted();
            if (rseek_err) std::rethrow_exception(rseek_err);
            throw;
        }

        if (rseek_err) std::rethrow_exception(rseek_err);
    }
private:
    size_t                     m_pos{0};
    std::vector<internal_type> m_key;
};

/**
 * @lang{ZH}
 * `vigenere_cvt` 的工厂类，用于在转换器管道中统一构造维吉尼亚密码转换器实例。
 *
 * 在构造时验证并存储密钥；每次调用 `create()` 均使用同一密钥。
 * @endif
 *
 * @lang{EN}
 * Factory class for `vigenere_cvt`, for uniform construction of Vigenère cipher
 * converter instances within a pipeline.
 *
 * Validates and stores the key at construction time; subsequent `create()` calls
 * reuse the same key.
 * @endif
 *
 * @tparam TChar
 * @lang{ZH} 密钥及数据流的字符类型。 @endif
 * @lang{EN} The character type of the key and data stream. @endif
 */
template <typename TChar>
class vigenere_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    explicit vigenere_cvt_creator(const std::basic_string<TChar>& key)
        : m_key(key)
    {
        if (key.empty())
            throw cvt_error("Cannot create vigenere_cvt_creator with empty key");
    }

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        static_assert(std::is_same_v<typename TKernel::internal_type, TChar>);
        return vigenere_cvt<TKernel>{std::forward<TKernel>(kernel), m_key};
    }
private:
    std::basic_string<TChar> m_key;
};

template <typename TChar>
vigenere_cvt_creator(const TChar*) -> vigenere_cvt_creator<TChar>;
}

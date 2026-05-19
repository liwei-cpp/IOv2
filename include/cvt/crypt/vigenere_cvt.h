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
/// @brief Vigenère cipher converter for educational and demonstration purposes.
///
/// @warning **NOT FOR PRODUCTION SECURITY USE**
///
/// The Vigenère cipher is a historical polyalphabetic substitution cipher
/// that has been completely broken since the 19th century. It provides
/// NO cryptographic security and should NEVER be used to protect sensitive data.
///
/// Known vulnerabilities:
/// - Kasiski examination can determine key length from ciphertext alone
/// - Frequency analysis breaks each position as a simple Caesar cipher
/// - No resistance to known-plaintext or chosen-plaintext attacks
///
/// For actual encryption needs, use `chacha20_cvt` or other modern ciphers.
///
/// This class is provided for:
/// - Educational purposes (learning about classical ciphers)
/// - Simple obfuscation (NOT security)
/// - Historical/demonstration purposes
///
/// @see chacha20_cvt for cryptographically secure encryption
template <io_converter KernelType>
    requires std::is_integral_v<typename KernelType::internal_type>
class vigenere_cvt : public abs_cvt<vigenere_cvt<KernelType>, KernelType, typename KernelType::internal_type, false, true>
{
    using BT = abs_cvt<vigenere_cvt<KernelType>, KernelType, typename KernelType::internal_type, false, true>;
    friend BT;  // for put_main, get_main, and private CRTP hooks
    constexpr static size_t s_buf_len = 16;
public:
    using device_type = typename KernelType::device_type;
    using internal_type = typename KernelType::internal_type;
    using external_type = internal_type;

public:
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

    void attach_impl()
    {
        if (m_key.empty())
            throw cvt_error("vigenere_cvt::attach fail: empty key");
    }

    /**
     * CRTP hook for `abs_cvt::main_cont_beg()`, called after the kernel-level
     * `main_cont_beg()`.
     *
     * Resets the key-offset position `m_pos` to zero so that main-content
     * encryption/decryption starts from the beginning of the key.
     */
    void main_cont_beg_impl()
    {
        m_pos = 0;
    }

    // Vigenère arithmetic is performed in the unsigned domain to avoid
    // signed integer overflow UB when internal_type is a signed multi-byte
    // type (e.g. int32_t). Unsigned subtraction/addition is well-defined
    // (modulo 2^N wrap), and the back-cast to internal_type is well-defined
    // under C++20's two's-complement guarantee. Encryption (add_wrap) and
    // decryption (sub_wrap) remain exact inverses in the mod-2^N group.
    static constexpr internal_type sub_wrap(internal_type a, internal_type b) noexcept
    {
        using U = std::make_unsigned_t<internal_type>;
        return static_cast<internal_type>(static_cast<U>(a) - static_cast<U>(b));
    }
    static constexpr internal_type add_wrap(internal_type a, internal_type b) noexcept
    {
        using U = std::make_unsigned_t<internal_type>;
        return static_cast<internal_type>(static_cast<U>(a) + static_cast<U>(b));
    }

    size_t get_main(cvt_reader<KernelType>& reader, internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
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

    void put_main(cvt_writer<KernelType>& writer, const internal_type* to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
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

public:
    /// positioning
    [[nodiscard]] size_t tell() const
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::assert_not_tainted();
        return m_pos;
    }

    void seek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::assert_not_tainted();
        // The kernel is not required to land exactly on `pos` (e.g. clamping,
        // alignment, internal buffering), so the keystream index must be
        // resynced via tell() rather than using `pos` directly.
        //
        // The kernel only needs to provide a basic exception guarantee on
        // seek(): if seek throws, the kernel position may have moved. Either
        // way, the kernel is in *some* well-defined position, and tell() will
        // report it. So we always attempt to resync m_pos via tell(), even
        // when seek throws — that keeps the keystream index aligned with the
        // kernel's real position and lets the user safely retry or continue.
        //
        // Only when tell() itself throws can we no longer determine where the
        // kernel landed; that desync would silently corrupt the keystream, so
        // we taint. If both seek() and tell() throw, the seek exception wins
        // (it is what the user actually invoked).
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

    void rseek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::assert_not_tainted();
        // See seek() above for the same resync-via-tell rationale.
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

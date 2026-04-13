#pragma once
#include <stdexcept>
#include <string_view>
#include <vector>
#include <cvt/cvt_concepts.h>
#include <cvt/abs_cvt.h>
#include <cvt/root_cvt.h>

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
class vigenere_cvt : public abs_cvt<KernelType, typename KernelType::internal_type, true, false, true>
{
    constexpr static size_t s_buf_len = 16;
public:
    using device_type = typename KernelType::device_type;
    using internal_type = typename KernelType::internal_type;
    using external_type = internal_type;

private:
    using BT = abs_cvt<KernelType, internal_type, true, false, true>;

public:
    vigenere_cvt(KernelType dev, std::basic_string_view<internal_type> s)
        : BT(std::move(dev))
        , m_bos_done(false)
        , m_pos(0)
        , m_key(s.begin(), s.end())
    {
        if (s.empty())
            throw cvt_error("Cannot create vigener converter with empty key");
    }

// mandatory methods
public:
    device_type attach(device_type&& dev = device_type{})
    {
        m_bos_done = false;
        m_pos = 0;
        return BT::m_kernel.attach(std::move(dev));
    }

    device_type detach()
    {
        m_bos_done = false;
        m_pos = 0;
        return BT::detach();
    }

    void main_cont_beg()
    {
        m_bos_done = true;
        m_pos = 0;
        return BT::m_kernel.main_cont_beg();
    }

// optional methods
public:
    size_t get(internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (!m_bos_done) return BT::get_bos(to, to_max);

        size_t total_count = 0;
        auto rd = this->reader(s_buf_len);
        while (total_count != to_max)
        {
            const size_t dest_size = std::min(s_buf_len, to_max - total_count);
            auto [ptr, cur_size] = rd.get_buf(dest_size);
            if (cur_size == 0)
                return total_count;

            for (size_t i = 0; i < cur_size; ++i)
            {
                size_t pos = (m_pos++) % m_key.size();
                *to++ = (*ptr++) - m_key[pos];
            }
            total_count += cur_size;
        }
        return total_count;
    }
    
    void put(const internal_type* to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (!m_bos_done) return BT::put_bos(to, to_size);

        auto wt = this->writer(s_buf_len);

        size_t total_count = 0;
        while (total_count != to_size)
        {
            const size_t dest_size = std::min(s_buf_len, to_size - total_count);
            auto ptr = wt.put_buf(dest_size);

            for (size_t i = 0; i < dest_size; ++i)
            {
                size_t pos = (m_pos++) % m_key.size();
                *ptr++ = (*to++) + m_key[pos];
            }
            total_count += dest_size;
        }
    }
    
    /// positioning
    size_t tell() const
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        return m_pos;
    }
    
    void seek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::m_kernel.seek(pos);
        m_pos = pos;
    }
    
    void rseek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::m_kernel.rseek(pos);
        m_pos = BT::m_kernel.tell();
    }
private:
    bool                       m_bos_done;
    size_t                     m_pos;
    std::vector<internal_type> m_key;
};

template <typename TChar>
class vigenere_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    vigenere_cvt_creator(const std::basic_string<TChar>& key)
        : m_key(key) {}

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
vigenere_cvt_creator(const TChar []) -> vigenere_cvt_creator<TChar>;
}
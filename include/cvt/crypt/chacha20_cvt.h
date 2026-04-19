#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <botan/auto_rng.h>
#include <botan/stream_cipher.h>
#include <botan/hash.h>

#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>

namespace IOv2::Crypt
{
namespace chacha20_cvt_helpers
{
inline Botan::secure_vector<uint8_t> key_gen(std::string_view key)
{
    auto hash = Botan::HashFunction::create("SHA-256");
    if (!hash)
        throw cvt_error("chacha20 key generation fail: cannot create SHA-256 hash");
    
    hash->update(reinterpret_cast<const uint8_t*>(key.data()), key.size());
    return hash->final();
}
}

template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (std::is_integral_v<typename KernelType::internal_type> &&
              sizeof(typename KernelType::internal_type) == sizeof(uint8_t))
class chacha20_cvt : public abs_cvt<chacha20_cvt<KernelType, TInt>, KernelType, TInt, true, false, false>
{
    using BT = abs_cvt<chacha20_cvt<KernelType, TInt>, KernelType, TInt, true, false, false>;
    friend BT; // for put_main and get_main
public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

    static constexpr size_t block_size = 64;

public:
    chacha20_cvt(KernelType kernel, std::string_view key)
        : BT(std::move(kernel))
        , m_cipher(Botan::StreamCipher::create("ChaCha20"))
    {
        if (!m_cipher)
            throw cvt_error("chacha20_cvt constructor fail: cannot create the stream cipher");

        m_key = chacha20_cvt_helpers::key_gen(key);
    }

    chacha20_cvt(KernelType kernel, const Botan::secure_vector<uint8_t>& key)
        : BT(std::move(kernel))
        , m_cipher(Botan::StreamCipher::create("ChaCha20"))
        , m_key(key)
    {
        if (!m_cipher)
            throw cvt_error("chacha20_cvt constructor fail: cannot create the stream cipher");
    }
    
    chacha20_cvt(const chacha20_cvt&) = delete;
    chacha20_cvt& operator=(const chacha20_cvt&) = delete;
    chacha20_cvt(chacha20_cvt&& val) = default;
    chacha20_cvt& operator=(chacha20_cvt&& val) = default;

// mandatory methods
public:
    device_type attach(device_type&& dev = device_type{})
    {
        m_cipher->clear();
        return BT::attach(std::move(dev));
    }
    
    device_type detach()
    {
        m_cipher->clear();
        return BT::detach();
    }
    
    io_status bos()
    {
        BT::bos();
        const size_t iv_len = m_cipher->default_iv_length();
        if (BT::m_io_status == io_status::input)
        {
            auto rd = this->reader(iv_len);
            auto ptr = rd.template get_buf<true>(iv_len);

            m_cipher->set_key(m_key);
            m_cipher->set_iv(reinterpret_cast<const uint8_t*>(ptr), iv_len);
        }
        else if (BT::m_io_status == io_status::output)
        {
            Botan::AutoSeeded_RNG rng;
            auto wt = this->writer(iv_len);
            auto ptr = wt.put_buf(iv_len);
            rng.randomize(reinterpret_cast<uint8_t*>(ptr), iv_len);

            m_cipher->set_key(m_key);
            m_cipher->set_iv(reinterpret_cast<const uint8_t*>(ptr), iv_len);
            wt.commit();
        }
        else
            throw cvt_error("chacha20_cvt::bos fail: neither in input nor output mode");
        return BT::m_io_status;
    }

    void main_cont_beg()
    {
        BT::main_cont_beg();
    }

// optional methods
private:
    size_t get_main(internal_type* _to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        constexpr size_t max_type_limit = std::numeric_limits<size_t>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));
        to_max = (max_chunk / sizeof(internal_type) > to_max)
               ? (to_max * sizeof(internal_type))
               : max_chunk;

        uint8_t* to = reinterpret_cast<uint8_t*>(_to);

        auto rd = this->reader(block_size);
        size_t res = 0;
        while (res != to_max)
        {
            size_t dest_size = std::min<size_t>(block_size, to_max - res);
            auto [ptr, cur_len] = rd.get_buf(dest_size);
            if (cur_len == 0) break;
            m_cipher->cipher(reinterpret_cast<const uint8_t*>(ptr), to, cur_len);
            to += cur_len;
            res += cur_len;
        }

        if (res % sizeof(internal_type))
            throw cvt_error("chacha20_cvt::get fails: partial sequence");
        return res / sizeof(internal_type);
    }
    
    void put_main(const internal_type* _to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        constexpr size_t max_type_limit = std::numeric_limits<size_t>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        const uint8_t* to = reinterpret_cast<const uint8_t*>(_to);

        auto wt = this->writer(block_size);
        while (to_size > 0)
        {
            size_t aim_output = (max_chunk / sizeof(internal_type) > to_size)
                              ? (to_size * sizeof(internal_type))
                              : max_chunk;
            size_t to_i = 0;
            while (to_i != aim_output)
            {
                size_t dest_size = std::min<size_t>(block_size, aim_output - to_i);
                auto ptr = wt.put_buf(dest_size);
                m_cipher->cipher(to + to_i, reinterpret_cast<uint8_t*>(ptr), dest_size);
                to_i += dest_size;
            }
            to += aim_output;
            to_size -= aim_output / sizeof(internal_type);
        }
        wt.commit();
    }
private:
    std::unique_ptr<Botan::StreamCipher> m_cipher;
    Botan::secure_vector<uint8_t>        m_key;
};

template <typename TInt>
struct chacha20_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    chacha20_cvt_creator(std::string_view key)
        : m_key(chacha20_cvt_helpers::key_gen(key)) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return chacha20_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_key};
    }

private:
    Botan::secure_vector<uint8_t> m_key;
};
}
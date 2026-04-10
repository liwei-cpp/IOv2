#pragma once
#include <memory>
#include <string>
#include <vector>
#include <botan/auto_rng.h>
#include <botan/stream_cipher.h>
#include <botan/hash.h>

#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>

namespace IOv2::Crypt
{
template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (std::is_integral_v<typename KernelType::internal_type> &&
              sizeof(typename KernelType::internal_type) == sizeof(uint8_t))
class chacha20_cvt : public abs_cvt<KernelType, TInt, true, false, false>
{
public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

private:
    using BT = abs_cvt<KernelType, TInt, true, false, false>;

public:
    chacha20_cvt(KernelType kernel, const std::string& key)
        : BT(std::move(kernel))
        , m_cipher(Botan::StreamCipher::create("ChaCha20"))
        , m_bos_done(false)
    {
        if (!m_cipher)
            throw cvt_error("chacha20_cvt constructor fail: cannot create the stream cipher");

        auto hash = Botan::HashFunction::create("SHA-256");
        if (!hash)
            throw cvt_error("chacha20_cvt::bos fail: cannot create SHA-256 hash");
        
        hash->update((const uint8_t*)m_key.data(), m_key.size());
        m_key = hash->final();
    }
    
    chacha20_cvt(const chacha20_cvt&) = delete;
    chacha20_cvt& operator=(const chacha20_cvt&) = delete;
    chacha20_cvt(chacha20_cvt&& val) = default;
    chacha20_cvt& operator= (chacha20_cvt&& val) = default;

// mandatory methods
public:
    device_type attach(device_type&& dev = device_type{})
    {
        m_cipher->clear();
        m_bos_done = false;
        return BT::attach(std::move(dev));
    }
    
    device_type detach()
    {
        m_cipher->clear();
        m_bos_done = false;
        return BT::detach();
    }
    
    io_status bos()
    {
        if (m_bos_done)
            throw cvt_error("chacha20_cvt::bos fail: not in bos phase");

        m_io_status = BT::bos();
        const size_t iv_len = m_cipher->default_iv_length();
        if (m_io_status == io_status::input)
        {
            auto rd = this->reader(iv_len);
            auto ptr = rd.template get_buf<true>(iv_len);

            m_cipher->set_key(m_key);
            m_cipher->set_iv((uint8_t*)ptr, iv_len);
        }
        else if (m_io_status == io_status::output)
        {
            Botan::AutoSeeded_RNG rng;
            auto wt = this->writer(iv_len);
            auto ptr = wt.put_buf(iv_len);
            rng.randomize((uint8_t*)ptr, iv_len);

            m_cipher->set_key(m_key);
            m_cipher->set_iv((uint8_t*)ptr, iv_len);
        }
        else
            throw cvt_error("chacha20_cvt::bos fail: neither in input nor output mode");
        return m_io_status;
    }

    void main_cont_beg()
    {
        m_bos_done = true;
        BT::main_cont_beg();
    }

// optional methods
public:
    size_t get(internal_type* _to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (!m_bos_done) return BT::get_bos(_to, to_max);

        if (m_io_status == io_status::output)
            throw cvt_error("chacha20_cvt::get fails: not available");

        uint8_t* to = (uint8_t*)_to;
        to_max *= sizeof(internal_type);

        auto rd = this->reader(64);
        size_t res = 0;
        while (res != to_max)
        {
            size_t dest_size = std::min<size_t>(64, to_max - res);
            auto [ptr, cur_len] = rd.get_buf(dest_size);
            if (cur_len == 0) break;
            m_cipher->cipher((uint8_t*)ptr, to, cur_len);
            to += cur_len;
            res += cur_len;
        }

        if (res % sizeof(internal_type))
            throw cvt_error("chacha20_cvt::get fails: partial sequence");
        return res / sizeof(internal_type);
    }
    
    void put(const internal_type* _to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (!m_bos_done) return BT::put_bos(_to, to_size);

        if (m_io_status == io_status::input)
            throw cvt_error("chacha20_cvt::put fails: not available");

        auto wt = this->writer(64);
        uint8_t* to = (uint8_t*)_to;
        to_size *= sizeof(internal_type);

        size_t to_i = 0;
        while (to_i != to_size)
        {
            size_t dest_size = std::min<size_t>(64, to_size - to_i);
            auto ptr = wt.put_buf(dest_size);
            m_cipher->cipher(to + to_i, (uint8_t *)ptr, dest_size);
            to_i += dest_size;
        }
        return;
    }
private:
    std::unique_ptr<Botan::StreamCipher> m_cipher;
    Botan::secure_vector<uint8_t>        m_key;
    bool                                 m_bos_done;
    io_status                            m_io_status;
};

template <typename TInt>
struct chacha20_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    chacha20_cvt_creator(const std::string& key)
        : m_key(key) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return chacha20_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_key};
    }

private:
    std::string m_key;
};
}
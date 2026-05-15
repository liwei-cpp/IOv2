#pragma once
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>

#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <botan/auto_rng.h>
#include <botan/hash.h>
#include <botan/stream_cipher.h>

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
class chacha20_cvt : public abs_cvt<chacha20_cvt<KernelType, TInt>, KernelType, TInt, false, false>
{
    using BT = abs_cvt<chacha20_cvt<KernelType, TInt>, KernelType, TInt, false, false>;
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
    void attach(device_type&& dev = device_type{})
    {
        if (!m_cipher)
        {
            m_cipher = Botan::StreamCipher::create("ChaCha20");
            if (!m_cipher)
                throw cvt_error("chacha20_cvt::attach fail: cannot recreate the stream cipher");
        }
        else
            m_cipher->clear();
        BT::attach(std::move(dev));
    }

    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        std::exception_ptr local_err;
        if (m_cipher)
        {
            try { m_cipher->clear(); }
            catch (...) { local_err = std::current_exception(); }
        }
        auto [dev, inner_err] = BT::detach();
        return { std::move(dev), local_err ? local_err : inner_err };
    }

    io_status bos()
    {
        BT::bos();
        const size_t iv_len = m_cipher->default_iv_length();
        std::vector<external_type> iv_buf(iv_len);

        if (BT::m_io_status == io_status::input)
        {
            if constexpr (cvt_cpt::support_get<KernelType>)
            {
                const size_t n = BT::m_kernel.get(iv_buf.data(), iv_len);
                if (n != iv_len)
                    throw cvt_error("chacha20_cvt::bos fail: incomplete IV read");

                m_cipher->set_key(m_key);
                m_cipher->set_iv(reinterpret_cast<const uint8_t*>(iv_buf.data()), iv_len);
            }
        }
        else if (BT::m_io_status == io_status::output)
        {
            if constexpr (cvt_cpt::support_put<KernelType>)
            {
                Botan::AutoSeeded_RNG rng;
                rng.randomize(reinterpret_cast<uint8_t*>(iv_buf.data()), iv_len);

                m_cipher->set_key(m_key);
                m_cipher->set_iv(reinterpret_cast<const uint8_t*>(iv_buf.data()), iv_len);

                BT::m_kernel.put(iv_buf.data(), iv_len);
            }
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
    size_t get_main(cvt_reader<KernelType>& reader, internal_type* _to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        constexpr size_t max_type_limit = std::numeric_limits<size_t>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));
        to_max = (max_chunk / sizeof(internal_type) > to_max)
               ? (to_max * sizeof(internal_type))
               : max_chunk;

        uint8_t* to = reinterpret_cast<uint8_t*>(_to);

        reader.reset(block_size);
        size_t res = 0;
        while (res != to_max)
        {
            size_t dest_size = std::min<size_t>(block_size, to_max - res);
            auto [ptr, cur_len] = reader.get_buf(dest_size);
            if (cur_len == 0) break;
            m_cipher->cipher(reinterpret_cast<const uint8_t*>(ptr), to, cur_len);
            to += cur_len;
            res += cur_len;
        }

        if (res % sizeof(internal_type))
            throw cvt_error("chacha20_cvt::get fails: partial sequence");
        return res / sizeof(internal_type);
    }

    void put_main(cvt_writer<KernelType>& writer, const internal_type* _to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        constexpr size_t max_type_limit = std::numeric_limits<size_t>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        const uint8_t* to = reinterpret_cast<const uint8_t*>(_to);

        writer.reset(block_size);
        while (to_size > 0)
        {
            size_t aim_output = (max_chunk / sizeof(internal_type) > to_size)
                              ? (to_size * sizeof(internal_type))
                              : max_chunk;
            size_t to_i = 0;
            while (to_i != aim_output)
            {
                size_t dest_size = std::min<size_t>(block_size, aim_output - to_i);
                auto ptr = writer.put_buf(dest_size);
                m_cipher->cipher(to + to_i, reinterpret_cast<uint8_t*>(ptr), dest_size);
                to_i += dest_size;
            }
            to += aim_output;
            to_size -= aim_output / sizeof(internal_type);
        }
        // commit() is the responsibility of abs_cvt::put.
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

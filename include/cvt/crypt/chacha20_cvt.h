#pragma once
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <botan/auto_rng.h>
#include <botan/hash.h>
#include <botan/secmem.h>
#include <botan/stream_cipher.h>

namespace IOv2::Crypt
{
namespace chacha20_cvt_helpers
{
inline Botan::secure_vector<uint8_t> key_gen(std::string_view key)
{
    // Reject empty input: hashing an empty string would silently yield the
    // publicly known constant SHA-256("") and use it as the ChaCha20 key,
    // providing zero confidentiality. The secure_vector-based constructor
    // already rejects invalid key lengths; mirror that contract here.
    if (key.empty())
        throw cvt_error("chacha20 key generation fail: key must not be empty");

    auto hash = Botan::HashFunction::create("SHA-256");
    if (!hash)
        throw cvt_error("chacha20 key generation fail: cannot create SHA-256 hash");

    hash->update(reinterpret_cast<const uint8_t*>(key.data()), key.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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

    static_assert(sizeof(external_type) == 1,
        "chacha20_cvt requires external_type (kernel's internal_type) to be 1 byte");

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
        if (!m_cipher->valid_keylength(key.size()))
            throw cvt_error("chacha20_cvt constructor fail: invalid key length");
    }

    chacha20_cvt(const chacha20_cvt&) = delete;
    chacha20_cvt& operator=(const chacha20_cvt&) = delete;
    chacha20_cvt(chacha20_cvt&& val) = default;
    chacha20_cvt& operator=(chacha20_cvt&& val) = default;
    ~chacha20_cvt() = default;

// optional methods
private:
    /**
     * @lang{ZH}
     * `abs_cvt::detach()` 的 CRTP 钩子，在 kernel 层 `detach()` 之前调用。
     *
     * 若 `m_cipher` 有效（非 moved-from 状态），调用 `m_cipher->clear()` 清除密钥和 IV 状态；
     * 异常以 `exception_ptr` 形式返回，由调用方（`abs_cvt::detach()`）按 first-failure-wins
     * 与 kernel 层异常合并。本函数为 `noexcept`，此约束由 `abs_cvt` 的 `static_assert` 强制。
     *
     * @return 捕获到的首个清理异常；无异常时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::detach()`, called before the kernel-level `detach()`.
     *
     * If `m_cipher` is valid (not a moved-from object), calls `m_cipher->clear()` to
     * wipe the key and IV state. Any captured exception is returned as an `exception_ptr`;
     * the caller (`abs_cvt::detach()`) merges it with the kernel-layer exception under
     * first-failure-wins. Must be `noexcept` — enforced by a `static_assert` in `abs_cvt`.
     *
     * @return The first captured cleanup exception, or `nullptr` if none.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        std::exception_ptr local_err = nullptr;
        if (m_cipher)
        {
            try { m_cipher->clear(); }
            catch (...) { local_err = std::current_exception(); }
        }
        return local_err;
    }

    void attach_impl()
    {
        if (!m_cipher || m_key.empty())
            throw cvt_error("chacha20_cvt::attach fail: cipher or key missing (moved-from object?)");
        m_cipher->clear();
    }

    void bos_impl()
    {
        if (!m_cipher || m_key.empty())
            throw cvt_error("chacha20_cvt::bos fail: cipher or key missing (moved-from object?)");

        const size_t iv_len = m_cipher->default_iv_length();
        if (iv_len == 0)
            throw cvt_error("chacha20_cvt::bos fail: cipher reports zero IV length");
        std::vector<external_type> iv_buf(iv_len);

        if (BT::m_io_status == io_status::input)
        {
            if constexpr (cvt_cpt::support_get<KernelType>)
            {
                const size_t n = BT::m_kernel.get(iv_buf.data(), iv_len);
                if (n != iv_len)
                    throw cvt_error("chacha20_cvt::bos fail: incomplete IV read");

                m_cipher->set_key(m_key);
                m_cipher->set_iv(reinterpret_cast<const uint8_t*>(iv_buf.data()), iv_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            }
            else
                throw cvt_error("chacha20_cvt::bos fail: input mode but kernel does not support get");
        }
        else if (BT::m_io_status == io_status::output)
        {
            if constexpr (cvt_cpt::support_put<KernelType>)
            {
                Botan::AutoSeeded_RNG rng;
                rng.randomize(reinterpret_cast<uint8_t*>(iv_buf.data()), iv_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

                m_cipher->set_key(m_key);
                m_cipher->set_iv(reinterpret_cast<const uint8_t*>(iv_buf.data()), iv_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

                BT::m_kernel.put(iv_buf.data(), iv_len);
            }
            else
                throw cvt_error("chacha20_cvt::bos fail: output mode but kernel does not support put");
        }
        else
            throw cvt_error("chacha20_cvt::bos fail: neither in input nor output mode");
    }

    size_t get_main(cvt_reader<KernelType>& reader, internal_type* _to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (!m_cipher)
            throw cvt_error("chacha20_cvt::get_main fail: cipher not initialized (moved-from object?)");

        constexpr size_t max_type_limit = std::numeric_limits<size_t>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        auto* to = reinterpret_cast<uint8_t*>(_to); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        reader.reset(block_size);
        size_t total_res = 0;   // total bytes decrypted across all outer iterations

        // Mirrors put_main: the outer loop splits a large request into
        // max_chunk-sized slices so that (to_max * sizeof) never overflows.
        // Each inner loop fills one slice or stops on EOF.
        while (to_max > 0)
        {
            const size_t aim = (max_chunk / sizeof(internal_type) > to_max)
                             ? (to_max * sizeof(internal_type))
                             : max_chunk;

            size_t chunk_res = 0;   // bytes decrypted in this slice
            bool eof = false;
            while (chunk_res != aim)
            {
                const size_t dest_size = std::min<size_t>(block_size, aim - chunk_res);
                auto [ptr, cur_len] = reader.get_buf(dest_size);
                if (cur_len == 0) { eof = true; break; }
                m_cipher->cipher(reinterpret_cast<const uint8_t*>(ptr), to, cur_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                to += cur_len;
                chunk_res += cur_len;
            }
            total_res += chunk_res;
            to_max -= aim / sizeof(internal_type);
            if (eof) break;
        }

        if (total_res % sizeof(internal_type))
            throw cvt_error("chacha20_cvt::get_main fail: partial sequence");
        return total_res / sizeof(internal_type);
    }

    void put_main(cvt_writer<KernelType>& writer, const internal_type* _to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (!m_cipher)
            throw cvt_error("chacha20_cvt::put_main fail: cipher not initialized (moved-from object?)");

        constexpr size_t max_type_limit = std::numeric_limits<size_t>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        const auto* to = reinterpret_cast<const uint8_t*>(_to); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

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
                m_cipher->cipher(to + to_i, reinterpret_cast<uint8_t*>(ptr), dest_size); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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
    explicit chacha20_cvt_creator(std::string_view key)
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

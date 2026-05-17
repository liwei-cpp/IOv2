#pragma once
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

#include <concepts>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <botan/hash.h>
#include <botan/hex.h>

namespace IOv2::Crypt
{
enum class hash_algo : unsigned short
{
    MD5,
    SHA256,
};

enum class hash_fmt : unsigned char
{
    binary,
    upper_hex,
    lower_hex
};

struct set_hash_fmt : cvt_behavior
{
    explicit set_hash_fmt(hash_fmt val)
        : m_val(val) {}

    hash_fmt m_val;
};

struct dump_hash : cvt_behavior
{
    explicit dump_hash(std::optional<uint8_t> delim = std::nullopt)
        : m_delim(delim) {}

    std::optional<uint8_t> m_delim;
};

template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (std::is_integral_v<typename KernelType::internal_type> &&
              sizeof(typename KernelType::internal_type) == sizeof(uint8_t) &&
              std::is_trivially_copyable_v<TInt> &&
              std::has_unique_object_representations_v<TInt> &&
              cvt_cpt::support_put<KernelType>)
class hash_cvt : public abs_cvt<hash_cvt<KernelType, TInt>, KernelType, TInt, false, false>
{
    using BT = abs_cvt<hash_cvt<KernelType, TInt>, KernelType, TInt, false, false>;
    friend BT;  // for put_main

public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

public:
    hash_cvt(KernelType kernel, hash_algo algo)
        : BT(std::move(kernel))
        , m_hash(Botan::HashFunction::create_or_throw(algo_to_str(algo)))
        , m_has_main_cont(false)
        , m_out_fmt(hash_fmt::lower_hex)
    {}

    hash_cvt(const hash_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_hash(val.m_hash ? val.m_hash->copy_state() : nullptr)
        , m_has_main_cont(val.m_hash ? val.m_has_main_cont : false)
        , m_out_fmt(val.m_out_fmt)
    {}

    hash_cvt(hash_cvt&& val) noexcept
        : BT(std::move(val))
        , m_hash(std::move(val.m_hash))
        , m_has_main_cont(val.m_has_main_cont)
        , m_out_fmt(val.m_out_fmt)
    {
        val.m_has_main_cont = false;
    }

    hash_cvt& operator=(const hash_cvt& val)
    {
        if (this != &val)
        {
            // 1. Flush current data first to avoid double-dump if temp's
            //    destructor runs due to exception during copy construction
            if (m_has_main_cont)
                dump_stream();

            // 2. Construct full copy (may throw, but avoids wrong-data-dump bug)
            hash_cvt temp(val);

            // 3. Move-assign from temp (noexcept, per io_converter concept)
            *this = std::move(temp);
        }
        return *this;
    }

    hash_cvt& operator=(hash_cvt&& val) noexcept
    {
        if (this != &val)
        {
            // Best-effort flush: if dump_stream fails, we accept data loss
            // since the old state is being replaced anyway.
            if (m_has_main_cont)
            {
                try { dump_stream(); }
                catch (...) {} // NOLINT(bugprone-empty-catch)
            }
            m_hash = std::move(val.m_hash);
            m_has_main_cont = val.m_has_main_cont;
            m_out_fmt = val.m_out_fmt;
            val.m_has_main_cont = false;

            BT::operator=(std::move(val));
        }
        return *this;
    }

    ~hash_cvt()
    {
        if (m_has_main_cont)
        {
            try { dump_stream(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }
    }

// mandatory methods
public:
    io_status bos()
    {
        auto res = BT::bos();
        try
        {
            if (res != io_status::output)
                throw cvt_error("hash_cvt::bos fail: only output mode is supported");

            if (!m_hash)
                throw cvt_error("hash_cvt::bos fail: hash not initialized (moved-from object?)");
            m_hash->clear();
        }
        catch (...)
        {
            BT::set_tainted();
            throw;
        }
        return io_status::output;
    }

    void adjust(const cvt_behavior& acc)
    {
        if (const set_hash_fmt* shf_ptr = dynamic_cast<const set_hash_fmt*>(&acc); shf_ptr)
            m_out_fmt = shf_ptr->m_val;
        else if (const dump_hash* dh_ptr = dynamic_cast<const dump_hash*>(&acc); dh_ptr)
        {
            if (m_has_main_cont)
            {
                dump_stream();
                if (dh_ptr->m_delim)
                {
                    const uint8_t delim = *dh_ptr->m_delim;
                    BT::m_kernel.put(reinterpret_cast<const external_type*>(&delim), 1);
                }
            }
        }

        return BT::adjust(acc);
    }

// optional methods
private:
    /**
     * @lang{ZH}
     * `abs_cvt::detach()` 的 CRTP 钩子，在 kernel 层 `detach()` 之前调用。
     *
     * 若当前处于主内容阶段（`m_has_main_cont`），尝试调用 `dump_stream()` 将哈希结果写出；
     * 若 `dump_stream()` 抛出异常，则手动将 `m_has_main_cont` 置为 `false` 并尝试
     * `m_hash->clear()` 清除残余状态（`clear()` 的异常被吞掉，避免掩盖首要错误）。
     * 成功路径下 `dump_stream()` 自行置 `m_has_main_cont = false`。
     * 最后将 `m_out_fmt` 重置为默认值。
     *
     * 异常以 `exception_ptr` 形式返回，由调用方（`abs_cvt::detach()`）按 first-failure-wins
     * 与 kernel 层异常合并。本函数为 `noexcept`，此约束由 `abs_cvt` 的 `static_assert` 强制。
     *
     * @return 捕获到的首个清理异常；无异常时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::detach()`, called before the kernel-level `detach()`.
     *
     * If currently in the main-content phase (`m_has_main_cont`), attempts to call
     * `dump_stream()` to write out the hash result. If `dump_stream()` throws, resets
     * `m_has_main_cont` to `false` and attempts `m_hash->clear()` to wipe residual
     * state (the `clear()` exception is swallowed to avoid obscuring the primary error).
     * On the success path, `dump_stream()` itself resets `m_has_main_cont`.
     * Finally, `m_out_fmt` is reset to its default value.
     *
     * Any captured exception is returned as an `exception_ptr`; the caller
     * (`abs_cvt::detach()`) merges it with the kernel-layer exception under
     * first-failure-wins. Must be `noexcept` — enforced by a `static_assert` in `abs_cvt`.
     *
     * @return The first captured cleanup exception, or `nullptr` if none.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        std::exception_ptr local_err = nullptr;
        try { if (m_has_main_cont) dump_stream(); }
        catch (...)
        {
            local_err = std::current_exception();
            m_has_main_cont = false;
            // Clear any residual hash state so a subsequent attach()+put()
            // does not accumulate on top of half-finished data.  If clear()
            // itself fails, swallow the exception — we are already in error
            // recovery from a failed dump_stream(), and surfacing a secondary
            // failure here would only obscure the primary error.
            if (m_hash)
            {
                try { m_hash->clear(); }
                catch (...) {} // NOLINT(bugprone-empty-catch)
            }
        }
        m_out_fmt = hash_fmt::lower_hex;
        return local_err;
    }

    void attach_impl()
    {
        if (!m_hash)
            throw cvt_error("hash_cvt::attach fail: hash not initialized (moved-from object?)");
        m_hash->clear();
    }

    void put_main(cvt_writer<KernelType>& /*writer*/, const internal_type* to, size_t to_size)
    {
        if (!m_hash)
            throw cvt_error("hash_cvt::put_main fail: hash not initialized (moved-from object?)");

        // Overflow check: ensure to_size * sizeof(internal_type) won't wrap
        constexpr size_t max_safe = std::numeric_limits<size_t>::max() / sizeof(internal_type);
        if (to_size > max_safe)
            throw cvt_error("hash_cvt::put_main fail: size overflow");

        if (to_size == 0) return;
        m_hash->update(reinterpret_cast<const uint8_t*>(to), to_size * sizeof(internal_type));
        m_has_main_cont = true;
    }

private:
    static const char* algo_to_str(hash_algo algo)
    {
        switch (algo)
        {
        case hash_algo::MD5:    return "MD5";
        case hash_algo::SHA256: return "SHA-256";
        default:
            throw cvt_error("algo_to_str: invalid algorithm");
        }
    }

    void dump_stream()
    {
        if (!m_has_main_cont) return;
        if (!m_hash) return;

        // Use copy_state()->final() to preserve original state until put() succeeds.
        // This provides strong exception safety: if put() fails, state is unchanged
        // and the operation can be retried.
        auto cp_state = m_hash->copy_state();
        if (!cp_state)
            throw cvt_error("hash_cvt::dump_stream fail: cannot copy hash state");
        auto digest = cp_state->final();
        if (digest.empty())
            throw cvt_error("hash_cvt::dump_stream fail: cannot get hash result");

        switch (m_out_fmt)
        {
        case hash_fmt::binary:
            BT::m_kernel.put(reinterpret_cast<const external_type*>(digest.data()), digest.size());
            break;
        case hash_fmt::upper_hex:
            {
                std::string hex_string = Botan::hex_encode(digest);
                BT::m_kernel.put(reinterpret_cast<const external_type*>(hex_string.data()), hex_string.size());
            }
            break;
        case hash_fmt::lower_hex:
            {
                std::string hex_string = Botan::hex_encode(digest, false);
                BT::m_kernel.put(reinterpret_cast<const external_type*>(hex_string.data()), hex_string.size());
            }
            break;
        default:
            throw cvt_error("hash_cvt::dump_stream fail: invalid output format");
        }

        // Only clear state after successful output
        m_has_main_cont = false;
        try
        {
            m_hash->clear();
        }
        catch (...)
        {
            BT::set_tainted();
            throw;
        }
    }
private:
    std::unique_ptr<Botan::HashFunction> m_hash;
    bool         m_has_main_cont;
    hash_fmt     m_out_fmt;
};

template <typename TInt>
struct hash_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    explicit hash_cvt_creator(hash_algo algo)
        : m_algo(algo)
    {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return hash_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_algo};
    }

private:
    const hash_algo m_algo;
};
}

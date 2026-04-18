#pragma once
#include <memory>
#include <botan/hash.h>
#include <botan/hex.h>

#include <cvt/cvt_concepts.h>
#include <cvt/abs_cvt.h>

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
    set_hash_fmt(hash_fmt val)
        : m_val(val) {}

    hash_fmt m_val;
};

struct dump_hash : cvt_behavior
{
    dump_hash(uint8_t delim)
        : m_delim(delim) {}

    uint8_t m_delim;
};

template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (std::is_integral_v<typename KernelType::internal_type> &&
              sizeof(typename KernelType::internal_type) == sizeof(uint8_t))
class hash_cvt : public abs_cvt<KernelType, TInt, true, false, false>
{
    static_assert(cvt_cpt::support_put<KernelType>);

public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

private:
    using BT = abs_cvt<KernelType, internal_type, true, false, false>;

public:
    hash_cvt(KernelType kernel, hash_algo algo)
        : BT(std::move(kernel))
        , m_hash(Botan::HashFunction::create_or_throw(algo_to_str(algo)))
        , m_bos_done(false)
        , m_has_main_cont(false)
        , m_out_fmt(hash_fmt::lower_hex)
    {}

    hash_cvt(const hash_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_hash(val.m_hash->copy_state())
        , m_bos_done(val.m_bos_done)
        , m_has_main_cont(val.m_has_main_cont)
        , m_out_fmt(val.m_out_fmt)
    {}

    hash_cvt(hash_cvt&& val)
        : BT(std::move(val))
        , m_hash(std::move(val.m_hash))
        , m_bos_done(val.m_bos_done)
        , m_has_main_cont(val.m_has_main_cont)
        , m_out_fmt(val.m_out_fmt)
    {
        val.m_has_main_cont = false;
    }

    hash_cvt& operator=(const hash_cvt& val)
    {
        if (m_has_main_cont)
            dump_stream();
        m_hash = val.m_hash->copy_state();
        m_bos_done = val.m_bos_done;
        m_has_main_cont = val.m_has_main_cont;
        m_out_fmt = val.m_out_fmt;

        BT::operator=(val);
        return *this;
    }
    
    hash_cvt& operator=(hash_cvt&& val)
    {
        if (m_has_main_cont)
            dump_stream();
        m_hash = std::move(val.m_hash);
        m_bos_done = val.m_bos_done;
        m_has_main_cont = val.m_has_main_cont;
        m_out_fmt = val.m_out_fmt;
        val.m_has_main_cont = false;

        BT::operator=(std::move(val));
        return *this;
    }
    
    ~hash_cvt()
    {
        if (m_has_main_cont)
            dump_stream();
    }

// mandatory methods
public:
    io_status bos()
    {
        auto res = BT::bos();
        if (res != io_status::output)
            throw cvt_error("hash_cvt::bos fail: only output mode is supported.");
        return io_status::output;
    }

    void main_cont_beg()
    {
        m_bos_done = true;
        BT::main_cont_beg();
    }
    
    device_type attach(device_type&& dev = device_type{})
    {
        if (m_has_main_cont)
            dump_stream();
        m_bos_done = false;
        m_has_main_cont = false;
        m_out_fmt = hash_fmt::lower_hex;
        return BT::attach(std::move(dev));
    }

    device_type detach()
    {
        if (m_has_main_cont)
            dump_stream();
        m_bos_done = false;
        m_has_main_cont = false;
        m_out_fmt = hash_fmt::lower_hex;
        return BT::detach();
    }
    
    void adjust(const cvt_behavior& acc)
    {
        if (const set_hash_fmt* shf_ptr = dynamic_cast<const set_hash_fmt*>(&acc); shf_ptr)
            m_out_fmt = shf_ptr->m_val;
        else if (const dump_hash* dh_ptr = dynamic_cast<const dump_hash*>(&acc); dh_ptr)
        {
            (dh_ptr->m_delim) ? dump_stream(reinterpret_cast<external_type*>(const_cast<uint8_t*>(&dh_ptr->m_delim)))
                              : dump_stream();
        }

        return BT::adjust(acc);
    }
    
// optional methods
public:
    void put(const internal_type* to, size_t to_size)
    {
        if (!m_bos_done)
            return BT::m_kernel.put(to, to_size);

        m_has_main_cont = true;
        m_hash->update((const uint8_t*)to, to_size * sizeof(internal_type));
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

    void dump_stream(external_type* delim = nullptr)
    {
        if (!m_has_main_cont) return;
        if (!m_hash) return;
        
        auto digest = m_hash->final();
        if (digest.empty())
            throw cvt_error("hash_cvt::dump_stream fail: cannot get hash result.");

        switch (m_out_fmt)
        {
        case hash_fmt::binary:
            BT::m_kernel.put((const external_type*)digest.data(), digest.size());
            break;
        case hash_fmt::upper_hex:
            {
                std::string hex_string = Botan::hex_encode(digest);
                BT::m_kernel.put((const external_type*)hex_string.data(), hex_string.size());
            }
            break;
        case hash_fmt::lower_hex:
            {
                std::string hex_string = Botan::hex_encode(digest, false);
                BT::m_kernel.put((const external_type*)hex_string.data(), hex_string.size());
            }
            break;
        default:
            throw cvt_error("hash_cvt::dump_stream fail: invalid output format.");
        }
        if (delim)
            BT::m_kernel.put(delim, 1);

        m_hash->clear();
        m_has_main_cont = false;
    }
private:
    std::unique_ptr<Botan::HashFunction> m_hash;
    bool         m_bos_done;
    bool         m_has_main_cont;
    hash_fmt     m_out_fmt;
};

template <typename TInt>
struct hash_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    hash_cvt_creator(hash_algo algo)
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
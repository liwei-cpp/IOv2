#pragma once
#include <cvt/cvt_concepts.h>
#include <memory>

namespace IOv2
{
template <io_device TDevice, typename TInt>
class abs_runtime_cvt_imp
{
public:
    using device_type = TDevice;
    using internal_type = TInt;
    using external_type = typename device_type::char_type;

public:
    abs_runtime_cvt_imp() = default;
    abs_runtime_cvt_imp(const abs_runtime_cvt_imp&) = default;
    abs_runtime_cvt_imp& operator= (const abs_runtime_cvt_imp&) = delete;
    abs_runtime_cvt_imp& operator= (abs_runtime_cvt_imp&&) = delete;
    virtual ~abs_runtime_cvt_imp() = default;
    
public:
    virtual std::unique_ptr<abs_runtime_cvt_imp> clone() const & = 0;
    
    virtual const device_type& device() const & = 0;
    virtual device_type detach() = 0;
    virtual device_type attach(device_type&& dev = device_type{}) = 0;
    virtual void adjust(const cvt_behavior& acc) = 0;
    virtual void retrieve(cvt_status& acc) const = 0;
    virtual bool is_eos() = 0;

    virtual io_status bos() = 0;
    virtual void main_cont_beg() = 0;
    
    virtual size_t get(internal_type* to, size_t to_max) = 0;
    virtual void put(const internal_type* to, size_t to_size) = 0;
    virtual void flush() = 0;

    virtual size_t tell() const = 0;
    virtual void seek(size_t pos) = 0;
    virtual void rseek(size_t pos) = 0;
    virtual void switch_to_get() = 0;
    virtual void switch_to_put() = 0;
};

template <io_converter KernelType>
class runtime_cvt_imp : public abs_runtime_cvt_imp<typename KernelType::device_type, typename KernelType::internal_type>
{
    using BT = abs_runtime_cvt_imp<typename KernelType::device_type, typename KernelType::internal_type>;
public:
    using device_type = typename KernelType::device_type;
    using internal_type = typename KernelType::internal_type;
    using external_type = typename device_type::char_type;

public:
    runtime_cvt_imp(const KernelType& kernel)
        : m_kernel(kernel)
        , m_io_status(io_status::neutral) {}

    runtime_cvt_imp(KernelType&& kernel)
        : m_kernel(std::move(kernel))
        , m_io_status(io_status::neutral) {}

public:
    std::unique_ptr<BT> clone() const & override
    {
        if constexpr (std::copy_constructible<KernelType>)
            return std::make_unique<runtime_cvt_imp>(*this);
        else
            throw cvt_error("runtime_cvt fail: kernel does not support copy construction");
    }
    
    const device_type& device() const & override
    {
        return m_kernel.device();
    }
    
    device_type detach() override
    {
        m_io_status = io_status::neutral;
        return m_kernel.detach();
    }

    device_type attach(device_type&& dev = device_type{}) override
    {
        m_io_status = io_status::neutral;
        return m_kernel.attach(std::move(dev));
    }

    void adjust(const cvt_behavior& acc) override
    {
        return m_kernel.adjust(acc);
    }

    void retrieve(cvt_status& acc) const override
    {
        return m_kernel.retrieve(acc);
    }
    
    bool is_eos() override
    {
        return m_kernel.is_eos();
    }

    io_status bos() override
    {
        m_io_status = m_kernel.bos();
        return m_io_status;
    }

    void main_cont_beg() override
    {
        return m_kernel.main_cont_beg();
    }
    
    size_t get(internal_type* to, size_t to_max) override
    {
        if constexpr(!cvt_cpt::support_get<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support get");
        else
            return m_kernel.get(to, to_max);
    }

    void put(const internal_type* to, size_t to_size) override
    {
        if constexpr(!cvt_cpt::support_put<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support put");
        else
            return m_kernel.put(to, to_size);
    }

    void flush() override
    {
        if constexpr(!cvt_cpt::support_put<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support put");
        else
            return m_kernel.flush();
    }

    size_t tell() const override
    {
        if constexpr (!cvt_cpt::support_positioning<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support positioning");
        else
            return m_kernel.tell();
    }

    void seek(size_t pos) override
    {
        if constexpr (!cvt_cpt::support_positioning<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support positioning");
        else
            m_kernel.seek(pos);
    }

    void rseek(size_t pos) override
    {
        if constexpr (!cvt_cpt::support_positioning<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support positioning");
        else
            m_kernel.rseek(pos);
    }

    void switch_to_get() override
    {
        if (m_io_status == io_status::input) return;
        if constexpr (!cvt_cpt::support_io_switch<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support I/O switch");
        else
        {
            m_kernel.switch_to_get();
            m_io_status = io_status::input;
        }
    }

    void switch_to_put() override
    {
        if (m_io_status == io_status::output) return;
        if constexpr (!cvt_cpt::support_io_switch<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support I/O switch");
        else
        {
            m_kernel.switch_to_put();
            m_io_status = io_status::output;
        }
    }

private:
    KernelType m_kernel;
    io_status m_io_status;
};

template <io_device TDevice, typename TInt>
class runtime_cvt
{
public:
    using device_type = TDevice;
    using internal_type = TInt;
    using external_type = typename device_type::char_type;

public:
    template <io_converter KernelType>
        requires ((!std::is_same_v<KernelType, runtime_cvt>) &&
                  (std::copy_constructible<KernelType>))
    runtime_cvt(const KernelType& kernel)
        : m_ptr(std::make_unique<runtime_cvt_imp<KernelType>>(kernel)) {}

    template <io_converter KernelType>
        requires (!std::is_same_v<KernelType, runtime_cvt>)
    runtime_cvt(KernelType&& kernel)
        : m_ptr(std::make_unique<runtime_cvt_imp<KernelType>>(std::move(kernel))) {}

    runtime_cvt(const runtime_cvt& val)
        : m_ptr(val.m_ptr ? val.m_ptr->clone() : nullptr) {}
        
    runtime_cvt& operator= (const runtime_cvt& val)
    {
        if (this != &val)
            m_ptr = val.m_ptr ? val.m_ptr->clone() : nullptr;
        return *this;
    }

    runtime_cvt(runtime_cvt&&) = default;
    runtime_cvt& operator= (runtime_cvt&&) = default;

public:
    const device_type& device() const &
    {
        return m_ptr->device();
    }
    
    device_type detach()
    {
        return m_ptr->detach();
    }

    device_type attach(device_type&& dev = device_type{})
    {
        return m_ptr->attach(std::move(dev));
    }

    void adjust(const cvt_behavior& acc)
    {
        return m_ptr->adjust(acc);
    }

    void retrieve(cvt_status& acc) const
    {
        return m_ptr->retrieve(acc);
    }    

    bool is_eos()
    {
        return m_ptr->is_eos();
    }

    io_status bos()
    {
        return m_ptr->bos();
    }

    void main_cont_beg()
    {
        return m_ptr->main_cont_beg();
    }
    
    size_t get(internal_type* to, size_t to_max)
    {
        return m_ptr->get(to, to_max);
    }

    void put(const internal_type* to, size_t to_size)
    {
        return m_ptr->put(to, to_size);
    }

    void flush()
    {
        return m_ptr->flush();
    }

    size_t tell() const
    {
        return m_ptr->tell();
    }

    void seek(size_t pos)
    {
        m_ptr->seek(pos);
    }

    void rseek(size_t pos)
    {
        m_ptr->rseek(pos);
    }

    void switch_to_get()
    {
        return m_ptr->switch_to_get();
    }

    void switch_to_put()
    {
        return m_ptr->switch_to_put();
    }
private:
    std::unique_ptr<abs_runtime_cvt_imp<TDevice, TInt>> m_ptr;
};

template <io_converter KernelType>
runtime_cvt(const KernelType&) -> runtime_cvt<typename KernelType::device_type, typename KernelType::internal_type>;

template <io_converter KernelType>
runtime_cvt(KernelType&&) -> runtime_cvt<typename KernelType::device_type, typename KernelType::internal_type>;
}
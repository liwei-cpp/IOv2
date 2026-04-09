#pragma once
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/cvt_concepts.h>
#include <cvt/abs_cvt.h>
#include <device/device_concepts.h>

namespace IOv2
{
template <io_device DeviceType, bool HasInBuffer>
class root_cvt
{
    template <io_converter KernelType>
    friend class root_cvt_reader;

    template <io_converter KernelType>
    friend class root_cvt_writer;

public:
    constexpr static size_t s_buffer_length = 2048;

    using device_type = DeviceType;
    using internal_type = typename device_type::char_type;
    using external_type = typename device_type::char_type;

public:
    explicit root_cvt(device_type device)
        : m_device(std::move(device))
        , m_bos_len(0)
        , m_buffer(s_buffer_length)
        , m_buf_cur(m_buffer.data())
        , m_buf_end(m_buffer.data())
        , m_io_status(io_status::neutral) {}
        
    root_cvt(const root_cvt& val)
        requires (std::copy_constructible<device_type>)
        : m_device(val.m_device)
        , m_bos_len(val.m_bos_len)
        , m_buffer(val.m_buffer)
        , m_io_status(val.m_io_status)
    {
        size_t cp = val.m_buf_cur - val.m_buffer.data();
        m_buf_cur = m_buffer.data() + cp;
        
        size_t ep = val.m_buf_end - val.m_buffer.data();
        m_buf_end = m_buffer.data() + ep;
    }

    root_cvt(root_cvt&& val)
        : m_device(std::move(val.m_device))
        , m_bos_len(val.m_bos_len)
        , m_io_status(val.m_io_status)
    {
        size_t cp = val.m_buf_cur - val.m_buffer.data();
        size_t ep = val.m_buf_end - val.m_buffer.data();
        
        m_buffer = std::move(val.m_buffer);
        m_buf_cur = m_buffer.data() + cp;
        m_buf_end = m_buffer.data() + ep;
        val.m_buf_cur = val.m_buf_end = nullptr;
        val.m_io_status = io_status::neutral;
    }

    root_cvt& operator= (const root_cvt& val)
        requires (std::is_copy_assignable_v<device_type>)
    {
        if constexpr (dev_cpt::support_put<device_type>)
            flush();

        m_device = val.m_device;
        m_bos_len = val.m_bos_len;
        m_buffer = val.m_buffer;

        size_t cp = val.m_buf_cur - val.m_buffer.data();
        m_buf_cur = m_buffer.data() + cp;
        
        size_t ep = val.m_buf_end - val.m_buffer.data();
        m_buf_end = m_buffer.data() + ep;
        
        m_io_status = val.m_io_status;
        return *this;
    }
    
    root_cvt& operator= (root_cvt&& val)
    {
        if constexpr (dev_cpt::support_put<device_type>)
            flush();

        m_device = std::move(val.m_device);
        m_bos_len = val.m_bos_len;
        
        size_t cp = val.m_buf_cur - val.m_buffer.data();
        size_t ep = val.m_buf_end - val.m_buffer.data();
        
        m_buffer = std::move(val.m_buffer);
        m_buf_cur = m_buffer.data() + cp;
        m_buf_end = m_buffer.data() + ep;
        val.m_buf_cur = val.m_buf_end = nullptr;
        
        m_io_status = val.m_io_status;
        val.m_io_status = io_status::neutral;
        return *this;
    }
    
    ~root_cvt()
    {
        if constexpr (dev_cpt::support_put<device_type>)
            flush();
    }

// mandatory methods
public:
    const device_type& device() const & { return m_device; }
    
    device_type detach()
    {
        if (m_io_status == io_status::output)
        {
            if constexpr (dev_cpt::support_put<device_type>)
                flush();
        }
        else if ((m_io_status == io_status::input) &&
                 (m_buf_end != m_buffer.data()))
        {
            if constexpr (dev_cpt::support_positioning<device_type>)
                seek(tell());
        }

        return std::move(m_device);
    }

    device_type attach(device_type&& dev = device_type{})
    {
        auto res = detach();
        m_device = std::move(dev);
        m_bos_len = 0;
        
        m_buffer.resize(s_buffer_length);
        m_buf_cur = m_buffer.data();
        m_buf_end = m_buffer.data();
        m_io_status = io_status::neutral;
        return res;
    }

    void adjust(const cvt_behavior&) {}
    void retrieve(cvt_status&) const {}
    
    void main_cont_beg()
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status == io_status::output)
                flush();
        }

        if constexpr (dev_cpt::support_positioning<device_type>)
            m_bos_len = m_device.dtell() - (m_buf_end - m_buf_cur);
    }
    
    io_status bos()
    {
        if constexpr (dev_cpt::support_get<device_type> &&
                      dev_cpt::support_put<device_type>)
        {
            if (m_device.deos())
                m_io_status = io_status::output;
            else
                m_io_status = io_status::input;
        }
        else if constexpr (dev_cpt::support_get<device_type>)
            m_io_status = io_status::input;
        else if constexpr (dev_cpt::support_put<device_type>)
            m_io_status = io_status::output;
        else
            static_assert(DependencyFalse<device_type>, "device does not support get nor put");

        return m_io_status;
    }

    bool is_eos()
    {
        switch(m_io_status)
        {
        case io_status::output:
            if constexpr (dev_cpt::support_put<device_type>)
            {
                if (m_buf_cur != m_buffer.data())
                    flush();
            }
            return m_device.deos();
        case io_status::input:
            if (m_buf_cur != m_buf_end) return false;
            return m_device.deos();
        default:
            return m_device.deos();
        }
    }

// optional methods
public:
    /// get
    size_t get(internal_type* to, size_t to_max)
        requires (dev_cpt::support_get<device_type>)
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status != io_status::input)
                switch_to_get();
        }
        if (m_io_status != io_status::input)
            throw cvt_error("root_cvt::get fails: invalid io status");

        if constexpr (!HasInBuffer)
            return m_device.dget(to, to_max);
        else
        {
            if (m_buf_cur == m_buf_end)
            {
                const size_t act_len = m_device.dget(m_buffer.data(), s_buffer_length);
                m_buf_cur = m_buffer.data();
                m_buf_end = m_buf_cur + act_len;
            }

            const size_t res_len = std::min<size_t>(m_buf_end - m_buf_cur, to_max);
            std::copy(m_buf_cur, m_buf_cur + res_len, to);
            m_buf_cur += res_len;
            return res_len;
        }
    }

    /// put
    void put(const internal_type* to, size_t to_size)
        requires (dev_cpt::support_put<device_type>)
    {
        if constexpr (dev_cpt::support_get<device_type>)
        {
            if (m_io_status != io_status::output)
                switch_to_put();
        }
        if (m_io_status != io_status::output)
            throw cvt_error("root_cvt::get fails: invalid io status");

        const size_t buf_used = m_buf_cur - m_buffer.data();
        const size_t remain = s_buffer_length - buf_used;
        if (to_size < remain)
        {
            m_buf_cur = std::copy(to, to + to_size, m_buf_cur);
            return;
        }
        
        m_device.dput(m_buffer.data(), buf_used);
        if (to_size < s_buffer_length / 2)
            m_buf_cur = std::copy(to, to + to_size, m_buffer.data());
        else
        {
            m_buf_cur = m_buffer.data();
            m_device.dput(to, to_size);
        }
    }
    
    void flush()
        requires (dev_cpt::support_put<device_type>)
    {
        if (m_io_status != io_status::output) return;

        if ((m_buf_cur != nullptr) && (m_buf_cur != m_buffer.data()))
        {
            m_device.dput(m_buffer.data(), m_buf_cur - m_buffer.data());
            m_buf_cur = m_buffer.data();
        }
        m_device.dflush();
    }
    
    /// positioning
    size_t tell() const
        requires (dev_cpt::support_positioning<device_type>)
    {
        size_t device_tell = m_device.dtell();

        switch(m_io_status)
        {
        case io_status::output:
            return device_tell - m_bos_len + (m_buf_cur - m_buffer.data());
        case io_status::input:
            return device_tell - m_bos_len - (m_buf_end - m_buf_cur);
        default:
            throw cvt_error("root_cvt::tell fails: invalid io status");
        }
    }
    
    void seek(size_t pos)
        requires (dev_cpt::support_positioning<device_type>)
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status == io_status::output)
                flush();
        }

        m_device.dseek(pos + m_bos_len);
        if (m_io_status == io_status::input)
            m_buf_cur = m_buf_end;
    }
    
    void rseek(size_t pos)
        requires (dev_cpt::support_positioning<device_type>)
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status == io_status::output)
                flush();
        }

        if (m_bos_len == 0)
        {
            m_device.drseek(pos);
            if (m_io_status == io_status::input)
                m_buf_cur = m_buf_end;
            return;
        }

        size_t cur_pos = m_device.dtell();
        m_device.drseek(pos);
        if (m_device.dtell() < m_bos_len)
        {
            m_device.dseek(cur_pos);
            throw cvt_error("root_cvt::rseek fails: out of boundary");
        }

        if (m_io_status == io_status::input)
            m_buf_cur = m_buf_end;
    }
    
    // io switch
    void switch_to_get()
        requires (dev_cpt::support_get<device_type> && dev_cpt::support_put<device_type>)
    {
        switch(m_io_status)
        {
        case io_status::input:
            return;
        case io_status::output:
            flush();
            [[fallthrough]];
        default:
            m_io_status = io_status::input;
            m_buf_cur = m_buf_end = m_buffer.data();
        }
    }
    
    void switch_to_put()
        requires (dev_cpt::support_get<device_type> && dev_cpt::support_put<device_type>)
    {
        switch(m_io_status)
        {
        case io_status::output:
            return;
        case io_status::input:
            if (m_buf_cur != m_buf_end)
            {
                if constexpr (!dev_cpt::support_positioning<device_type>)
                    throw cvt_error("root_cvt::switch_to_put fails: device does not support positioning");
                seek(tell());
            }
            [[fallthrough]];
        default:
            m_io_status = io_status::output;
            m_buf_cur = m_buf_end = m_buffer.data();
        }
    }

private:
    device_type                 m_device;
    size_t                      m_bos_len;
    std::vector<external_type>  m_buffer;
    external_type*              m_buf_cur;
    external_type*              m_buf_end;
    io_status                   m_io_status;
};

template <bool HasInBuffer, io_device DeviceType>
auto make_root_cvt(DeviceType dev)
{
    return root_cvt<DeviceType, HasInBuffer>(std::move(dev));
}

template <io_converter KernelType>
class root_cvt_reader;

template <io_device DeviceType>
class root_cvt_reader<root_cvt<DeviceType, true>>
{
    using KernelType = root_cvt<DeviceType, true>;
    using char_type = typename KernelType::internal_type;

public:
    root_cvt_reader(KernelType& kernel, size_t buf_size)
        : m_kernel(kernel)
        , m_buf_size(buf_size)
    {}

    template <bool Saturate = false>
    auto get_buf(size_t to_max)
    {
        if (to_max == 0)
            throw cvt_error("cvt_reader::get_buf fail, read size cannot be zero.");
        if (to_max > m_buf_size)
            throw cvt_error("cvt_reader::get_buf fail, read size too large.");

        if constexpr (dev_cpt::support_put<DeviceType>)
        {
            if (m_kernel.m_io_status != io_status::input)
                m_kernel.switch_to_get();
        }
        if (m_kernel.m_io_status != io_status::input)
            throw cvt_error("cvt_reader::get_buf fail, invalid io status");

        const size_t remain = m_kernel.m_buf_end - m_kernel.m_buf_cur;
        // need to read, then read at most as it can
        if (remain < to_max)
        {
            if (remain != 0)
                m_kernel.m_buf_end = std::copy(m_kernel.m_buf_cur, m_kernel.m_buf_end, m_kernel.m_buffer.data());
            else
                m_kernel.m_buf_end = m_kernel.m_buffer.data();

            m_kernel.m_buf_cur = m_kernel.m_buffer.data();
            m_kernel.m_buf_end += m_kernel.m_device.dget(m_kernel.m_buf_end, KernelType::s_buffer_length - remain);
        }

        if constexpr (Saturate)
        {
            const auto aim_ptr = to_max + m_kernel.m_buf_cur;
            while (m_kernel.m_buf_end < aim_ptr)
            {
                auto new_size = m_kernel.m_device.dget(m_kernel.m_buf_end, aim_ptr - m_kernel.m_buf_end);
                if (new_size == 0)
                    throw cvt_error("get_buf<Saturate> fail: meet eos");
                m_kernel.m_buf_end += new_size;
            }
        }

        if constexpr (!Saturate)
        {
            auto res_len = std::min<size_t>(m_kernel.m_buf_end - m_kernel.m_buf_cur, to_max);
            std::pair<const char_type*, size_t> res{m_kernel.m_buf_cur, res_len};
            m_kernel.m_buf_cur += res_len;
            return res;
        }
        else
        {
            const char_type* res = m_kernel.m_buf_cur;
            m_kernel.m_buf_cur += to_max;
            return res;
        }
    }

    void rollback(size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_reader::rollback fail, length cannot be zero.");
        if ((m_kernel.m_buf_cur == nullptr) || (len > static_cast<size_t>(m_kernel.m_buf_cur - m_kernel.m_buffer.data())))
            throw cvt_error("cvt_reader::rollback fail, rollback length too large.");
        m_kernel.m_buf_cur -= len;
    }

private:
    KernelType& m_kernel;
    const size_t m_buf_size;
};

template <io_converter KernelType>
class root_cvt_writer;

template <io_device DeviceType, bool HasInBuffer>
class root_cvt_writer<root_cvt<DeviceType, HasInBuffer>>
{
    using KernelType = root_cvt<DeviceType, HasInBuffer>;
    using char_type = typename KernelType::internal_type;

public:
    root_cvt_writer(KernelType& kernel, size_t buf_size)
        : m_kernel(kernel)
        , m_buf_size(buf_size)
    {}

    char_type* put_buf(size_t len)
    {
        if (len == 0)
            throw cvt_error("root_cvt_writer::put_buf fail, write size cannot be zero.");
        if (len > m_buf_size)
            throw cvt_error("root_cvt_writer::put_buf fail, write size too large.");

        if constexpr (dev_cpt::support_get<DeviceType>)
        {
            if (m_kernel.m_io_status != io_status::output)
                m_kernel.switch_to_put();
        }
        if (m_kernel.m_io_status != io_status::output)
            throw cvt_error("root_cvt_writer::put_buf fail, invalid io status");

        const size_t buf_used = m_kernel.m_buf_cur - m_kernel.m_buffer.data();
        const size_t remain = KernelType::s_buffer_length - buf_used;
        if (len < remain)
        {
            auto res = m_kernel.m_buf_cur;
            m_kernel.m_buf_cur += len;
            return res;
        }
        
        m_kernel.m_device.dput(m_kernel.m_buffer.data(), buf_used);
        m_kernel.m_buf_cur = m_kernel.m_buffer.data() + len;
        return m_kernel.m_buffer.data();
    }

    void rollback(size_t len)
    {
        if (len == 0)
            throw cvt_error("root_cvt_writer::rollback fail, length cannot be zero.");
        if ((m_kernel.m_buf_cur == nullptr) || (len > static_cast<size_t>(m_kernel.m_buf_cur - m_kernel.m_buffer.data())))
            throw cvt_error("root_cvt_writer::rollback fail, rollback length too large.");
        m_kernel.m_buf_cur -= len;
    }

private:
    KernelType& m_kernel;
    const size_t m_buf_size;
};

template <io_device DeviceType, bool HasInBuffer>
class cvt_io<root_cvt<DeviceType, HasInBuffer>>
{
    using KernelType = root_cvt<DeviceType, HasInBuffer>;
    using char_type = typename KernelType::internal_type;

public:
    auto reader(KernelType& kernel, size_t buf_size)
    {
        if constexpr (HasInBuffer)
        {
            if (buf_size > KernelType::s_buffer_length)
                throw cvt_error("cvt_io::reader construction fail: buffer too large");
            return root_cvt_reader<KernelType>(kernel, buf_size);
        }
        else
        {
            buffer.resize(buf_size);
            return cvt_reader(kernel, buffer);
        }
    }

    auto writer(KernelType& kernel, size_t buf_size)
    {
        if (buf_size > KernelType::s_buffer_length)
            throw cvt_error("cvt_io::writer construction fail: buffer too large");
        return root_cvt_writer<KernelType>(kernel, buf_size);
    }

private:
    std::vector<char_type> buffer;
};
}
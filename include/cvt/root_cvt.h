#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <device/mem_device.h>

#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2
{
/// @brief Root converter class for I/O operations, managing device buffers.
///
/// @note Moved-from objects: After a move operation (move construction or move
///       assignment), the source object is left in a valid but unspecified state.
///       The only safe operations on a moved-from object are:
///       - Destruction
///       - Assignment (copy or move)
///       Calling any other method on a moved-from object results in undefined behavior.
///       This follows standard C++ conventions for moved-from objects.
template <io_device DeviceType, bool HasInBuffer>
class root_cvt
{
    template <io_converter KernelType>
    friend class cvt_reader;

    template <io_converter KernelType>
    friend class cvt_writer;

public:
    constexpr static size_t s_buffer_length = 2048;

    constexpr static bool s_has_buffer = HasInBuffer;
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
        // Defensive: handle moved-from source gracefully to avoid UB from
        // nullptr arithmetic. Moved-from objects have m_buf_cur == nullptr.
        if (val.m_buf_cur == nullptr)
        {
            m_buf_cur = m_buffer.data();
            m_buf_end = m_buffer.data();
        }
        else
        {
            size_t cp = val.m_buf_cur - val.m_buffer.data();
            m_buf_cur = m_buffer.data() + cp;

            size_t ep = val.m_buf_end - val.m_buffer.data();
            m_buf_end = m_buffer.data() + ep;
        }
    }

    root_cvt(root_cvt&& val) noexcept(
                std::is_nothrow_move_constructible_v<device_type> &&
                std::is_nothrow_move_assignable_v<std::vector<external_type>>
            )
        : m_device(std::move(val.m_device))
        , m_bos_len(val.m_bos_len)
        , m_io_status(val.m_io_status)
    {
        size_t cp = 0;
        size_t ep = 0;

        if (val.m_buf_cur)
        {
            cp = val.m_buf_cur - val.m_buffer.data();
            ep = val.m_buf_end - val.m_buffer.data();
        }

        m_buffer = std::move(val.m_buffer);
        m_buf_cur = m_buffer.data() + cp;
        m_buf_end = m_buffer.data() + ep;

        val.m_buf_cur = val.m_buf_end = nullptr;
        val.m_io_status = io_status::neutral;
    }

    root_cvt& operator=(const root_cvt& val)
        requires (std::is_copy_assignable_v<device_type>)
    {
        if (this == &val) return *this;

        if constexpr (dev_cpt::support_put<device_type>)
            flush();

        m_device = val.m_device;
        m_bos_len = val.m_bos_len;
        m_buffer = val.m_buffer;

        // Defensive: handle moved-from source gracefully to avoid UB from
        // nullptr arithmetic. Moved-from objects have m_buf_cur == nullptr.
        if (val.m_buf_cur == nullptr)
        {
            m_buf_cur = m_buffer.data();
            m_buf_end = m_buffer.data();
        }
        else
        {
            size_t cp = val.m_buf_cur - val.m_buffer.data();
            m_buf_cur = m_buffer.data() + cp;

            size_t ep = val.m_buf_end - val.m_buffer.data();
            m_buf_end = m_buffer.data() + ep;
        }

        m_io_status = val.m_io_status;
        return *this;
    }

    root_cvt& operator=(root_cvt&& val)
    {
        if (this == &val) return *this;

        if constexpr (dev_cpt::support_put<device_type>)
            flush();

        m_device = std::move(val.m_device);
        m_bos_len = val.m_bos_len;

        size_t cp = 0;
        size_t ep = 0;

        if (val.m_buf_cur)
        {
            cp = val.m_buf_cur - val.m_buffer.data();
            ep = val.m_buf_end - val.m_buffer.data();
        }

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
        {
            try
            {
                flush();
            }
            catch (...) {// NOLINT(bugprone-empty-catch)
                // Ignore exceptions in destructor to prevent std::terminate
            }
        }
    }

// mandatory methods
public:
    device_type& device() { return m_device; }
    
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
        {
            // Invariant: dtell() >= buffered data. The buffer is filled via dget(),
            // which advances the device position by the amount read. Consuming data
            // from the buffer only moves m_buf_cur forward, never increasing the
            // buffered amount beyond what was read from the device.
            const size_t buffered = static_cast<size_t>(m_buf_end - m_buf_cur);
            assert(m_device.dtell() >= buffered);
            m_bos_len = m_device.dtell() - buffered;
        }
    }

    io_status bos()
    {
        if constexpr (dev_cpt::support_get<device_type> &&
                      dev_cpt::support_put<device_type>)
        {
            if (m_device.deof())
                m_io_status = io_status::output;
            else
                m_io_status = io_status::input;
        }
        else if constexpr (dev_cpt::support_get<device_type>)
            m_io_status = io_status::input;
        else if constexpr (dev_cpt::support_put<device_type>)
            m_io_status = io_status::output;
        else
            static_assert(dependent_false_v<device_type>, "device does not support get nor put");

        return m_io_status;
    }

    bool is_eof()
        requires (dev_cpt::support_get<device_type>)
    {
        switch(m_io_status)
        {
        case io_status::output:
            if constexpr (dev_cpt::support_put<device_type>)
            {
                if (m_buf_cur != m_buffer.data())
                    flush();
            }
            return m_device.deof();
        case io_status::input:
            if (m_buf_cur != m_buf_end) return false;
            return m_device.deof();
        default:
            throw cvt_error("root_cvt::is_eof fails: invalid io status"); 
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
            throw cvt_error("root_cvt::put fails: invalid io status");

        const size_t buf_used = m_buf_cur - m_buffer.data();
        const size_t remain = s_buffer_length - buf_used;
        if (to_size < remain)
        {
            m_buf_cur = std::copy(to, to + to_size, m_buf_cur);
            return;
        }

        if (to_size == remain)
        {
            std::copy(to, to + to_size, m_buf_cur);
            m_device.dput(m_buffer.data(), s_buffer_length);
            m_buf_cur = m_buffer.data();
            return;
        }

        if (buf_used > 0)
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
    }
    
    /// positioning
    size_t tell() const
        requires (dev_cpt::support_positioning<device_type>)
    {
        const size_t device_tell = m_device.dtell();

        switch(m_io_status)
        {
        case io_status::output:
        {
            // Invariant: device_tell >= m_bos_len. The device position only advances
            // via flush operations, never moving backward from the stream start.
            assert(device_tell >= m_bos_len);
            const size_t buf_used = static_cast<size_t>(m_buf_cur - m_buffer.data());
            return device_tell - m_bos_len + buf_used;
        }
        case io_status::input:
        {
            // Invariant: device_tell - m_bos_len >= buffered. Same invariant as
            // main_cont_beg(): buffer data comes from dget(), which advances device
            // position by the amount read.
            const size_t buffered = static_cast<size_t>(m_buf_end - m_buf_cur);
            assert(device_tell >= m_bos_len);
            assert(device_tell - m_bos_len >= buffered);
            return device_tell - m_bos_len - buffered;
        }
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

        const size_t size_from_device = m_device.dsize();
        if (m_bos_len > size_from_device)
            throw cvt_error("root_cvt::rseek fails: bos_len exceeds device size");
        if (size_from_device - m_bos_len < pos)
            throw cvt_error("root_cvt::rseek fails: out of boundary");

        m_device.drseek(pos);
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
    // std::vector is intentionally used over std::array: move operations are O(1)
    // (pointer swap), whereas std::array move degrades to a full element-wise copy.
    std::vector<external_type>  m_buffer;
    external_type*              m_buf_cur;
    external_type*              m_buf_end;
    io_status                   m_io_status;
};

// HasInBuffer is ignored for mem_device specialization:
// mem_device directly exposes buffer pointers and never needs an internal buffer.
// Both rb_root_cvt and no_rb_root_cvt are valid wrappers and behave identically here.
template <typename CharT,
          typename Traits,
          typename Allocator,
          bool HasInBuffer>
class root_cvt<mem_device<CharT, Traits, Allocator>, HasInBuffer>
{
public:
    using device_type = mem_device<CharT, Traits, Allocator>;
    using internal_type = typename device_type::char_type;
    using external_type = typename device_type::char_type;

public:
    explicit root_cvt(device_type device)
        : m_device(std::move(device))
        , m_bos_len(0)
        , m_io_status(io_status::neutral) {}

    root_cvt(const root_cvt& val)
        : m_device(val.m_device)
        , m_bos_len(val.m_bos_len)
        , m_io_status(val.m_io_status)
    {}

    root_cvt(root_cvt&& val)
        : m_device(std::move(val.m_device))
        , m_bos_len(val.m_bos_len)
        , m_io_status(val.m_io_status)
    {}

    root_cvt& operator=(const root_cvt& val)
    {
        if (this == &val) return *this;
        m_device = val.m_device;
        m_bos_len = val.m_bos_len;
        m_io_status = val.m_io_status;
        return *this;
    }
    
    root_cvt& operator=(root_cvt&& val)
    {
        if (this == &val) return *this;
        m_device = std::move(val.m_device);
        m_bos_len = val.m_bos_len;
        m_io_status = val.m_io_status;
        val.m_io_status = io_status::neutral;
        return *this;
    }
    
    ~root_cvt() = default;

public:
    device_type& device() { return m_device; }
    
    device_type detach()
    {
        return std::move(m_device);
    }

    device_type attach(device_type&& dev = device_type{})
    {
        auto res = detach();
        m_device = std::move(dev);
        m_bos_len = 0;
        
        m_io_status = io_status::neutral;
        return res;
    }

    void adjust(const cvt_behavior&) {}
    void retrieve(cvt_status&) const {}
    
    void main_cont_beg()
    {
        m_bos_len = m_device.dtell();
    }
    
    io_status bos()
    {
        m_io_status = (m_device.deof()) ? io_status::output : io_status::input;
        return m_io_status;
    }

    bool is_eof()
    {
        return m_device.deof();
    }

    size_t get(internal_type* to, size_t to_max)
    {
        switch_to_get();
        return m_device.dget(to, to_max);
    }

    void put(const internal_type* to, size_t to_size)
    {
        switch_to_put();
        m_device.dput(to, to_size);
    }

    void flush() {}
    
    /// positioning
    size_t tell() const
    {
        return m_device.dtell() - m_bos_len;
    }
    
    void seek(size_t pos)
    {
        m_device.dseek(pos + m_bos_len);
    }
    
    void rseek(size_t pos)
    {
        const size_t size_from_device = m_device.dsize();
        if (m_bos_len > size_from_device)
            throw cvt_error("root_cvt::rseek fails: bos_len exceeds device size");
        if (size_from_device - m_bos_len < pos)
            throw cvt_error("root_cvt::rseek fails: out of boundary");

        m_device.drseek(pos);
    }

    // io switch
    void switch_to_get()
    {
        m_io_status = io_status::input;
    }
    
    void switch_to_put()
    {
        m_io_status = io_status::output;
    }

private:
    device_type                 m_device;
    size_t                      m_bos_len;
    io_status                   m_io_status;
};

template <io_device DeviceType>
class rb_root_cvt : public root_cvt<DeviceType, true>
{
    using root_cvt<DeviceType, true>::root_cvt;
};

template <io_device DeviceType>
rb_root_cvt(DeviceType) -> rb_root_cvt<DeviceType>;

template <io_device DeviceType>
class no_rb_root_cvt : public root_cvt<DeviceType, false>
{
    using root_cvt<DeviceType, false>::root_cvt;
};

template <io_device DeviceType>
no_rb_root_cvt(DeviceType) -> no_rb_root_cvt<DeviceType>;

// cvt_reader specialization for root_cvt with buffer: directly use root_cvt's internal buffer
template <io_converter KernelType>
    requires (std::is_base_of_v<root_cvt<typename KernelType::device_type, true>, KernelType> &&
              !is_mem_device<typename KernelType::device_type>)
class cvt_reader<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    explicit cvt_reader(KernelType* kernel)
        : m_kernel(kernel) {}

    cvt_reader(const cvt_reader&) = delete;
    cvt_reader& operator=(const cvt_reader&) = delete;
    cvt_reader(cvt_reader&&) = default;
    cvt_reader& operator=(cvt_reader&&) = default;
    ~cvt_reader() = default;

    void set_kernel(KernelType* kernel) { m_kernel = kernel; }

    void reset(size_t) {}

    /// @brief Retrieves data from root_cvt's internal buffer, reading from device if needed.
    ///
    /// @tparam Saturate Controls behavior when insufficient data is available:
    ///   - false (default): Returns available data (may be less than requested).
    ///                      Return type: std::pair<const char_type*, size_t>
    ///   - true: Requires exactly to_max chars; throws if not enough data.
    ///           Return type: const char_type*
    ///
    /// @param to_max Number of characters to retrieve.
    /// @return When Saturate=false: pair of (pointer, actual_count).
    ///         When Saturate=true: pointer to exactly to_max chars.
    /// @throws cvt_error If Saturate=true and end-of-stream is reached before
    ///         reading to_max characters.
    template <bool Saturate = false>
    auto get_buf(size_t to_max)
    {
        if (m_kernel == nullptr)
            throw cvt_error("cvt_reader::get_buf fail, kernel is null.");
        if (to_max == 0)
            throw cvt_error("cvt_reader::get_buf fail, read size cannot be zero.");
        if (to_max > KernelType::s_buffer_length)
            throw cvt_error("cvt_reader::get_buf fail, read size too large.");

        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_kernel->m_io_status != io_status::input)
                m_kernel->switch_to_get();
        }
        if (m_kernel->m_io_status != io_status::input)
            throw cvt_error("cvt_reader::get_buf fail, invalid io status");

        const size_t remain = m_kernel->m_buf_end - m_kernel->m_buf_cur;
        // need to read, then read at most as it can
        if (remain < to_max)
        {
            if (remain != 0)
                m_kernel->m_buf_end = std::copy(m_kernel->m_buf_cur, m_kernel->m_buf_end, m_kernel->m_buffer.data());
            else
                m_kernel->m_buf_end = m_kernel->m_buffer.data();

            m_kernel->m_buf_cur = m_kernel->m_buffer.data();
            m_kernel->m_buf_end += m_kernel->m_device.dget(m_kernel->m_buf_end, KernelType::s_buffer_length - remain);
        }

        if constexpr (Saturate)
        {
            const auto aim_ptr = to_max + m_kernel->m_buf_cur;
            while (m_kernel->m_buf_end < aim_ptr)
            {
                auto new_size = m_kernel->m_device.dget(m_kernel->m_buf_end, aim_ptr - m_kernel->m_buf_end);
                if (new_size == 0)
                    throw cvt_error("get_buf<Saturate> fail: meet eos");
                m_kernel->m_buf_end += new_size;
            }
        }

        if constexpr (!Saturate)
        {
            auto res_len = std::min<size_t>(m_kernel->m_buf_end - m_kernel->m_buf_cur, to_max);
            std::pair<const char_type*, size_t> res{m_kernel->m_buf_cur, res_len};
            m_kernel->m_buf_cur += res_len;
            return res;
        }
        else
        {
            const char_type* res = m_kernel->m_buf_cur;
            m_kernel->m_buf_cur += to_max;
            return res;
        }
    }

    void rollback(size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_reader::rollback fail, length cannot be zero.");
        if ((m_kernel == nullptr) || (m_kernel->m_buf_cur == nullptr) || (len > static_cast<size_t>(m_kernel->m_buf_cur - m_kernel->m_buffer.data())))
            throw cvt_error("cvt_reader::rollback fail, rollback length too large.");
        m_kernel->m_buf_cur -= len;
    }

private:
    KernelType* m_kernel;
};

// cvt_writer specialization for root_cvt: directly use root_cvt's internal buffer
template <io_converter KernelType>
    requires (std::is_base_of_v<root_cvt<typename KernelType::device_type, KernelType::s_has_buffer>, KernelType> &&
              !is_mem_device<typename KernelType::device_type>)
class cvt_writer<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    explicit cvt_writer(KernelType* kernel)
        : m_kernel(kernel) {}

    cvt_writer(const cvt_writer&) = delete;
    cvt_writer& operator=(const cvt_writer&) = delete;
    cvt_writer(cvt_writer&&) = default;
    cvt_writer& operator=(cvt_writer&&) = default;
    ~cvt_writer() = default;

    void set_kernel(KernelType* kernel) { m_kernel = kernel; }

    void reset(size_t buf_size)
    {
        m_buf_size = buf_size;
        if (m_buf_size > KernelType::s_buffer_length)
            throw cvt_error("cvt_writer::reset fail, buf_size exceeds buffer capacity");
    }

    char_type* put_buf(size_t len)
    {
        if (m_kernel == nullptr)
            throw cvt_error("cvt_writer::put_buf fail, kernel is null.");
        if (len == 0)
            throw cvt_error("cvt_writer::put_buf fail, write size cannot be zero.");
        if (len > m_buf_size)
            throw cvt_error("cvt_writer::put_buf fail, write size too large.");

        if constexpr (dev_cpt::support_get<device_type>)
        {
            if (m_kernel->m_io_status != io_status::output)
                m_kernel->switch_to_put();
        }
        if (m_kernel->m_io_status != io_status::output)
            throw cvt_error("cvt_writer::put_buf fail, invalid io status");

        const size_t buf_used = m_kernel->m_buf_cur - m_kernel->m_buffer.data();
        const size_t remain = KernelType::s_buffer_length - buf_used;
        if (len < remain)
        {
            auto res = m_kernel->m_buf_cur;
            m_kernel->m_buf_cur += len;
            return res;
        }

        m_kernel->m_device.dput(m_kernel->m_buffer.data(), buf_used);
        m_kernel->m_buf_cur = m_kernel->m_buffer.data() + len;
        return m_kernel->m_buffer.data();
    }

    void rollback(size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_writer::rollback fail, length cannot be zero.");
        if ((m_kernel == nullptr) || (m_kernel->m_buf_cur == nullptr) || (len > static_cast<size_t>(m_kernel->m_buf_cur - m_kernel->m_buffer.data())))
            throw cvt_error("cvt_writer::rollback fail, rollback length too large.");
        m_kernel->m_buf_cur -= len;
    }

    void commit() {}

private:
    KernelType* m_kernel;
    size_t m_buf_size = 0;
};

// cvt_reader specialization for mem_device: directly access device buffer
template <io_converter KernelType>
    requires ((std::is_base_of_v<root_cvt<typename KernelType::device_type, true>, KernelType> ||
               std::is_base_of_v<root_cvt<typename KernelType::device_type, false>, KernelType>) &&
              is_mem_device<typename KernelType::device_type>)
class cvt_reader<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    explicit cvt_reader(KernelType* kernel)
        : m_kernel(kernel) {}

    cvt_reader(const cvt_reader&) = delete;
    cvt_reader& operator=(const cvt_reader&) = delete;
    cvt_reader(cvt_reader&&) = default;
    cvt_reader& operator=(cvt_reader&&) = default;
    ~cvt_reader() = default;

    void set_kernel(KernelType* kernel) { m_kernel = kernel; }

    void reset(size_t) {}

    /// @brief Forwards to mem_device::get_buf. See mem_device::get_buf for details.
    /// @tparam Saturate When true, requires exact count and returns const char_type*.
    ///                  When false (default), returns std::pair<const char_type*, size_t>.
    template <bool Saturate = false>
    auto get_buf(size_t to_max)
    {
        if (m_kernel == nullptr)
            throw cvt_error("cvt_reader::get_buf fail, kernel is null.");
        return m_kernel->device().template get_buf<Saturate>(to_max);
    }

    void rollback(size_t len)
    {
        if (m_kernel == nullptr)
            throw cvt_error("cvt_reader::rollback fail, kernel is null.");
        m_kernel->device().get_rollback(len);
    }

private:
    KernelType* m_kernel;
};

// cvt_writer specialization for mem_device: directly access device buffer
template <io_converter KernelType>
    requires ((std::is_base_of_v<root_cvt<typename KernelType::device_type, true>, KernelType> ||
               std::is_base_of_v<root_cvt<typename KernelType::device_type, false>, KernelType>) &&
              is_mem_device<typename KernelType::device_type>)
class cvt_writer<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    explicit cvt_writer(KernelType* kernel)
        : m_kernel(kernel) {}

    cvt_writer(const cvt_writer&) = delete;
    cvt_writer& operator=(const cvt_writer&) = delete;
    cvt_writer(cvt_writer&&) = default;
    cvt_writer& operator=(cvt_writer&&) = default;
    ~cvt_writer() = default;

    void set_kernel(KernelType* kernel) { m_kernel = kernel; }

    void reset(size_t) {}

    char_type* put_buf(size_t len)
    {
        if (m_kernel == nullptr)
            throw cvt_error("cvt_writer::put_buf fail, kernel is null.");
        return m_kernel->device().put_buf(len);
    }

    void rollback(size_t len)
    {
        if (m_kernel == nullptr)
            throw cvt_error("cvt_writer::rollback fail, kernel is null.");
        m_kernel->device().put_rollback(len);
    }

    void commit() {}

private:
    KernelType* m_kernel;
};
}
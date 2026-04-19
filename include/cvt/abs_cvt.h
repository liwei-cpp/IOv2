#pragma once
#include <cvt/cvt_concepts.h>
#include <bit>
#include <cstring>
#include <exception>
#include <vector>
namespace IOv2 
{
    template <io_converter KernelType>
    class cvt_reader
    {
        using char_type = typename KernelType::internal_type;
    public:
        cvt_reader(KernelType& kernel, std::vector<char_type>& buffer)
            : m_refkernel(kernel)
            , m_refbuffer(buffer)
        {}

        cvt_reader(const cvt_reader&) = delete;
        cvt_reader(cvt_reader&&) = default;

        ~cvt_reader()
        {
            m_refbuffer.clear();
        }
    
        template <bool Saturate = false>
        auto get_buf(size_t to_max)
        {
            if (to_max == 0)
                throw cvt_error("cvt_io::reader::get_buf fail, read size cannot be zero.");
            if (to_max > m_refbuffer.size())
                throw cvt_error("cvt_io::reader::get_buf fail, read size too large.");

            const auto rollback_size = m_end_pos - m_cur_pos;
            if (to_max <= rollback_size)
            {
                std::pair<const char_type*, size_t> res{m_refbuffer.data() + m_cur_pos, to_max};
                m_cur_pos += to_max;
                if constexpr (!Saturate)
                    return res;
                else
                    return res.first;
            }

            if (to_max > m_refbuffer.size() - m_cur_pos)
            {
                std::copy(m_refbuffer.data() + m_cur_pos, m_refbuffer.data() + m_end_pos, m_refbuffer.data());
                m_cur_pos = 0;
                m_end_pos = rollback_size;
            }
            const size_t aim_size = to_max - rollback_size;
            auto read_size = m_refkernel.get(m_refbuffer.data() + m_end_pos, aim_size);
            m_end_pos += read_size;

            if constexpr (Saturate)
            {
                while (read_size < aim_size)
                {
                    const size_t new_aim_size = aim_size - read_size;
                    const auto new_read_size = m_refkernel.get(m_refbuffer.data() + m_end_pos, new_aim_size);
                    if (new_read_size == 0)
                        throw cvt_error("get_buf<Saturate> fail: meet eos");
                    m_end_pos += new_read_size;
                    read_size += new_read_size;
                }
            }

            if constexpr(!Saturate)
            {
                std::pair<const char_type*, size_t> res{m_refbuffer.data() + m_cur_pos, rollback_size + read_size};
                m_cur_pos = m_end_pos;
                return res;
            }
            else
            {
                const char_type* res = m_refbuffer.data() + m_cur_pos;
                m_cur_pos = m_end_pos;
                return res;
            }
        }

        void rollback(size_t len)
        {
            if (len == 0)
                throw cvt_error("cvt_io::reader::rollback fail, length cannot be zero.");
            if (len > m_cur_pos)
                throw cvt_error("cvt_io::reader::rollback fail, rollback length too large.");
            m_cur_pos -= len;
        }

    private:
        KernelType& m_refkernel;
        std::vector<char_type>& m_refbuffer;
        size_t m_cur_pos = 0;
        size_t m_end_pos = 0;
    };

    template <io_converter KernelType>
    class cvt_writer
    {
        using char_type = typename KernelType::internal_type;
    public:
        cvt_writer(KernelType& kernel, std::vector<char_type>& buffer)
            : m_refkernel(kernel)
            , m_refbuffer(buffer)
        {}

        cvt_writer(const cvt_writer&) = delete;
        cvt_writer(cvt_writer&&) = default;

        ~cvt_writer()
        {
            try
            {
                commit();
                m_refbuffer.clear();
            }
            catch(...)
            {}
        }
    
        char_type* put_buf(size_t len)
        {
            if (len == 0)
                throw cvt_error("cvt_io::writer::put_buf fail, write size cannot be zero.");
            if (len > m_refbuffer.size())
                throw cvt_error("cvt_io::writer::put_buf fail, write size too large.");
        
            size_t remain = m_refbuffer.size() - m_prev_len;
            if (remain < len)
            {
                m_refkernel.put(m_refbuffer.data(), m_prev_len);
                remain = m_refbuffer.size();
                m_prev_len = 0;
            }
        
            auto res = m_refbuffer.data() + m_prev_len;
            m_prev_len += len;
            return res;
        }
    
        void rollback(size_t len)
        {
            if (len == 0)
                throw cvt_error("cvt_io::writer::rollback fail, length cannot be zero.");
            if (len > m_prev_len)
                throw cvt_error("cvt_io::writer::rollback fail, rollback length too large.");
            m_prev_len -= len;
        }

        void commit()
        {
            if (m_prev_len != 0)
            {
                m_refkernel.put(m_refbuffer.data(), m_prev_len);
                m_prev_len = 0;
            }
        }
    
    private:
        KernelType& m_refkernel;
        std::vector<char_type>& m_refbuffer;
        size_t m_prev_len = 0;
    };

    template <io_converter KernelType>
    class cvt_io
    {
        using char_type = typename KernelType::internal_type;
    public:
        auto reader(KernelType& kernel, size_t buf_size)
        {
            m_buffer.resize(buf_size);
            return cvt_reader(kernel, m_buffer);
        }

        auto writer(KernelType& kernel, size_t buf_size)
        {
            m_buffer.resize(buf_size);
            return cvt_writer(kernel, m_buffer);
        }

    private:
        std::vector<char_type> m_buffer;
    };

    template <typename CurrentType,
              io_converter KernelType,
              typename InternalType,
              bool default_flush = true,
              bool default_positioning = true,
              bool default_io_switch = true>
    class abs_cvt
    {
    public:
        using device_type = typename KernelType::device_type;
        using external_type = typename KernelType::internal_type;
        using internal_type = InternalType;

    private:
        // for bos: write s_bos_chunk external type at once. This will avoid allocating large memory, and use memory efficiently.
        constexpr static size_t s_bos_chunk = 16;  

    public:
        abs_cvt(KernelType kernel)
            : m_kernel(std::move(kernel)) {}
    
        abs_cvt(const abs_cvt&) = default;
        abs_cvt(abs_cvt&& val)
            : m_kernel(std::move(val.m_kernel))
            , m_io_status(val.m_io_status)
            , m_is_bos_done(val.m_is_bos_done)
            , m_cvt_io(std::move(val.m_cvt_io))
        {
            val.m_io_status = io_status::neutral;
            val.m_is_bos_done = false;
        }

        abs_cvt& operator=(const abs_cvt&) = default;
        abs_cvt& operator=(abs_cvt&& val)
        {
            if (this != &val)
            {
                m_kernel = std::move(val.m_kernel);
                m_io_status = val.m_io_status;
                val.m_io_status = io_status::neutral;
                m_is_bos_done = val.m_is_bos_done;
                val.m_is_bos_done = false;
                m_cvt_io = std::move(val.m_cvt_io);
            }
            return *this;
        }

        ~abs_cvt() = default;
    
    // mandatory methods
    public:
        const device_type& device() const & { return m_kernel.device(); }
        device_type detach()
        {
            m_io_status = io_status::neutral;
            m_is_bos_done = false;
            return m_kernel.detach();
        }
    
        device_type attach(device_type&& dev = device_type{})
        {
            m_io_status = io_status::neutral;
            m_is_bos_done = false;
            return m_kernel.attach(std::move(dev));
        }
    
        void adjust(const cvt_behavior& b)
        {
            m_kernel.adjust(b);
        }

        void retrieve(cvt_status& s) const
        {
            m_kernel.retrieve(s);
        }
        
        io_status bos()
        {
            if (m_io_status != io_status::neutral)
                throw cvt_error("abs_cvt::bos fail: Cannot call bos with un-neutral status.");
            if (m_is_bos_done)
                throw cvt_error("abs_cvt::bos fail: Cannot call bos multiple times.");

            m_io_status = m_kernel.bos();
            return m_io_status;
        }

        void main_cont_beg()
        {
            m_is_bos_done = true;
            m_kernel.main_cont_beg();
        }

        bool is_eos()
        {
            return m_kernel.is_eos();
        }

    // optional methods
    public:
        size_t get(internal_type* to, size_t to_max)
            requires requires(CurrentType& t, internal_type* data, size_t len) {
                { t.get_main(data, len) } -> std::same_as<size_t>;
            }
        {
            if (!m_is_bos_done)
            {
                if (to_max == 0) return 0;

                constexpr size_t ext_size = sizeof(external_type);

                auto rd = this->reader(s_bos_chunk);
                char* dest_bytes = reinterpret_cast<char*>(to);
                const char* dest_bytes_end = reinterpret_cast<const char*>(to + to_max);

                // Read full external_type chunks
                while (dest_bytes + ext_size <= dest_bytes_end)
                {
                    const size_t units_to_read_now = std::min((size_t)((dest_bytes_end - dest_bytes) / ext_size), s_bos_chunk);

                    const auto* src_buf = rd.template get_buf<true>(units_to_read_now);
                    std::memcpy(dest_bytes, src_buf, units_to_read_now * ext_size);
                    dest_bytes += units_to_read_now * ext_size;
                }

                // Read the final partial internal_type if needed
                const size_t final_remainder_bytes = dest_bytes_end - dest_bytes;
                if (final_remainder_bytes > 0)
                {
                    const auto* src_buf = rd.template get_buf<true>(1);
                    std::memcpy(dest_bytes, src_buf, final_remainder_bytes);
                    dest_bytes += final_remainder_bytes;
                }

                return to_max;
            }
            else
            {
                if (m_io_status != io_status::input)
                {
                    if constexpr (cvt_cpt::support_io_switch<CurrentType>)
                        static_cast<CurrentType*>(this)->switch_to_get();
                    else
                        throw cvt_error("abs_cvt::get fail: cannot switch to input mode");
                }
                return static_cast<CurrentType*>(this)->get_main(to, to_max);
            }
        }

        void put(const internal_type* to, size_t to_size)
            requires requires(CurrentType& t, const internal_type* data, size_t len) {
                { t.put_main(data, len) } -> std::same_as<void>;
            }
        {
            if (!m_is_bos_done)
            {
                if (to_size == 0) return;

                constexpr size_t ext_size = sizeof(external_type);

                auto to_bytes = reinterpret_cast<const char*>(to);
                auto to_bytes_end = reinterpret_cast<const char*>(to + to_size);

                auto wt = this->writer(s_bos_chunk);
                while (to_bytes + ext_size <= to_bytes_end)
                {
                    auto dest_count = std::min<size_t>((to_bytes_end - to_bytes) / ext_size, s_bos_chunk);
                    auto ptr = wt.put_buf(dest_count);
                    std::memcpy(reinterpret_cast<char*>(ptr), to_bytes, dest_count * ext_size);
                    to_bytes += dest_count * ext_size;
                }

                if (to_bytes < to_bytes_end)
                {
                    auto ptr = wt.put_buf(1);
                    std::memcpy(reinterpret_cast<char*>(ptr), to_bytes, to_bytes_end - to_bytes);
                    std::memset(reinterpret_cast<char*>(ptr) + (to_bytes_end - to_bytes), 0, ext_size - (to_bytes_end - to_bytes));
                }

                wt.commit();
            }
            else
            {
                if (m_io_status != io_status::output)
                {
                    if constexpr (cvt_cpt::support_io_switch<CurrentType>)
                        static_cast<CurrentType*>(this)->switch_to_put();
                    else
                        throw cvt_error("abs_cvt::put fail: cannot switch to output mode");
                }
                static_cast<CurrentType*>(this)->put_main(to, to_size);
            }
        }

        void flush()
            requires (default_flush && cvt_cpt::support_put<KernelType>)
        {
            m_kernel.flush();
        }

        size_t tell() const
            requires (default_positioning && cvt_cpt::support_positioning<KernelType>)
        {
            return m_kernel.tell();
        }
        
        void seek(size_t pos)
            requires (default_positioning && cvt_cpt::support_positioning<KernelType>)
        {
            m_kernel.seek(pos);
        }
        
        void rseek(size_t pos)
            requires (default_positioning && cvt_cpt::support_positioning<KernelType>)
        {
            m_kernel.rseek(pos);
        }
        
        void switch_to_get()
            requires (default_io_switch && cvt_cpt::support_io_switch<KernelType>)
        {
            m_kernel.switch_to_get();
            m_io_status = io_status::input;
        }

        void switch_to_put()
            requires (default_io_switch && cvt_cpt::support_io_switch<KernelType>)
        {
            m_kernel.switch_to_put();
            m_io_status = io_status::output;
        }

    protected:
        auto reader(size_t buf_size) { return m_cvt_io.reader(m_kernel, buf_size); }
        auto writer(size_t buf_size) { return m_cvt_io.writer(m_kernel, buf_size); }

    protected:
        KernelType  m_kernel;
        io_status   m_io_status = io_status::neutral;
        bool        m_is_bos_done = false;
    private:
        cvt_io<KernelType> m_cvt_io;
    };
}
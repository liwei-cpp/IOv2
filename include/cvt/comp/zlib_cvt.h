#pragma once
#include <zlib.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>

namespace IOv2::Comp
{
struct zlib_sync_flush : cvt_behavior
{
    zlib_sync_flush(bool sync_flush)
        : m_sync_flush(sync_flush) {}

    bool m_sync_flush;
};

template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (sizeof(typename KernelType::internal_type) == sizeof(unsigned char))
class zlib_cvt : public abs_cvt<KernelType, TInt, false, false, false>
{
    using BT = abs_cvt<KernelType, TInt, false, false, false>;

public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

private:
    constexpr static unsigned CHUNK = std::max<unsigned>(16, sizeof(internal_type));

public:
    zlib_cvt(KernelType kernel, unsigned put_level = 6)
        : BT(std::move(kernel))
        , m_put_level(put_level)
        , m_bos_done(false)
        , m_sync_flush(false)
    {
        if (m_put_level > 9) m_put_level = 9;
    }

    zlib_cvt(const zlib_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_put_level(val.m_put_level)
        , m_bos_done(val.m_bos_done)
        , m_sync_flush(val.m_sync_flush)
    {
        try
        {
            if (BT::m_io_status == io_status::output)
                zerr("zlib_cvt copy constructor fail", deflateCopy(&m_strm, const_cast<z_stream*>(&val.m_strm)));
            else if (BT::m_io_status == io_status::input)
                zerr("zlib_cvt copy constructor fail", inflateCopy(&m_strm, const_cast<z_stream*>(&val.m_strm)));
        }
        catch(...)
        {
            BT::m_io_status = io_status::neutral;
            throw;
        }
    }
    
    zlib_cvt(zlib_cvt&& val)
        : BT(std::move(val))
        , m_put_level(val.m_put_level)
        , m_bos_done(val.m_bos_done)
        , m_sync_flush(val.m_sync_flush)
    {
        try
        {
            if (BT::m_io_status == io_status::output)
            {
                zerr("zlib_cvt move constructor fail", deflateCopy(&m_strm, &val.m_strm));
                deflateEnd(&val.m_strm);
            }
            else if (BT::m_io_status == io_status::input)
            {
                zerr("zlib_cvt move constructor fail", inflateCopy(&m_strm, &val.m_strm));
                inflateEnd(&val.m_strm);
            }
        }
        catch(...)
        {
            BT::m_io_status = io_status::neutral;
            throw;
        }
    }
    
    zlib_cvt& operator=(const zlib_cvt& val)
    {
        if (this == &val) return *this;
        close_stream();
        BT::operator=(val);
        m_put_level = val.m_put_level;
        m_bos_done = val.m_bos_done;
        m_sync_flush = val.m_sync_flush;

        try
        {
            if (BT::m_io_status == io_status::output)
                zerr("zlib_cvt copy assignment fail", deflateCopy(&m_strm, const_cast<z_stream*>(&val.m_strm)));
            else if (BT::m_io_status == io_status::input)
                zerr("zlib_cvt copy assignment fail", inflateCopy(&m_strm, const_cast<z_stream*>(&val.m_strm)));
            return *this;            
        }
        catch(...)
        {
            BT::m_io_status = io_status::neutral;
            throw;
        }
    }
    
    zlib_cvt& operator=(zlib_cvt&& val)
    {
        if (this == &val) return *this;
        close_stream();
        BT::operator=(std::move(val));
        m_put_level = val.m_put_level;
        m_bos_done = val.m_bos_done;
        m_sync_flush = val.m_sync_flush;

        try
        {
            if (BT::m_io_status == io_status::output)
            {
                zerr("zlib_cvt move assignment fail", deflateCopy(&m_strm, &val.m_strm));
                deflateEnd(&val.m_strm);
            }
            else if (BT::m_io_status == io_status::input)
            {
                zerr("zlib_cvt move assignment fail", inflateCopy(&m_strm, &val.m_strm));
                inflateEnd(&val.m_strm);
            }
            return *this;
        }
        catch(...)
        {
            BT::m_io_status = io_status::neutral;
            throw;
        }
    }

    ~zlib_cvt()
    {
        try
        {
            close_stream();
        }
        catch (...) {}
    }

// mandatory methods
public:
    device_type attach(device_type&& dev = device_type{})
    {
        close_stream();
        return BT::attach(std::move(dev));
    }
    
    device_type detach()
    {
        close_stream();
        return BT::detach();
    }
    
    void adjust(const cvt_behavior& acc)
    {
        if (const zlib_sync_flush* ptr = dynamic_cast<const zlib_sync_flush*>(&acc); ptr)
            m_sync_flush = ptr->m_sync_flush;

        return BT::adjust(acc);
    }

    void main_cont_beg()
    {
        BT::main_cont_beg();

        m_bos_done = true;
        if (BT::m_io_status == io_status::output)
            flush();
        else if (BT::m_io_status == io_status::neutral)
            throw cvt_error("zlib_cvt::main_cont_beg fail: no get_bos or put_bos is called before calling main_cont_beg");
    }
    
    io_status bos()
    {
        BT::bos();
        if (BT::m_io_status == io_status::input)
        {
            m_strm.zalloc = Z_NULL;
            m_strm.zfree = Z_NULL;
            m_strm.opaque = Z_NULL;
            m_strm.avail_in = 0;
            m_strm.next_in = Z_NULL;
            auto ret = inflateInit(&m_strm);
            if (ret != Z_OK) throw cvt_error("zlib_cvt::bos fail: Cannot initialize zlib.");

            auto rd = this->reader(2);
            const external_type* ptr = nullptr;
            if constexpr (cvt_cpt::support_get<KernelType>)
                ptr = rd.template get_buf<true>(2);
            else throw cvt_error("zlib_cvt::bos fail: kernel does not support get.");

            m_strm.avail_in = 2;
            m_strm.next_in = (unsigned char*)ptr;
            
            unsigned char ch;
            m_strm.avail_out = 1;
            m_strm.next_out = &ch;
            
            ret = inflate(&m_strm, Z_NO_FLUSH);
            zerr("zlib_cvt::bos fail", ret);
            
            if ((m_strm.avail_in != 0) || (m_strm.avail_out != 1)) throw cvt_error("zlib_cvt::bos fail: Invalid zlib header");
            m_strm.next_in = nullptr;
            m_strm.avail_out = 0;
            m_strm.next_out = nullptr;
        }
        else if (BT::m_io_status == io_status::output)
        {
            m_strm.zalloc = Z_NULL;
            m_strm.zfree = Z_NULL;
            m_strm.opaque = Z_NULL;

            auto ret = deflateInit(&m_strm, (int)m_put_level);
            if (ret != Z_OK) throw cvt_error("zlib_cvt::bos fail: Cannot initialize zlib.");

            auto wt = this->writer(3);
            auto ptr = wt.put_buf(3);

            m_strm.avail_in = 0;
            m_strm.next_in = nullptr;
            m_strm.avail_out = 3;
            m_strm.next_out = (unsigned char*)(ptr);
            ret = deflate(&m_strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);

            if (m_strm.avail_out != 1) throw cvt_error("zlib_cvt::bos fail: Invalid zlib header");

            wt.rollback(1);
            wt.commit();

            m_strm.avail_out = 0;
            m_strm.next_out = nullptr;
        }
        else
            throw cvt_error("zlib_cvt::bos fail: invalid response value.");
        return BT::m_io_status;
    }

// optional methods
public:
    size_t get(internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (!m_bos_done) return BT::get_bos(to, to_max);

        if (BT::m_io_status != io_status::input)
            throw cvt_error("zlib_cvt::get fails: not available");

        // Note: We read one byte at a time intentionally. Decompression has
        // unpredictable expansion ratio - a few compressed bytes could expand
        // to overflow the output buffer. This keeps the code simple and correct.
        // The underlying converter already buffers, so this doesn't cause syscalls.
        auto rd = this->reader(1);

        constexpr size_t max_type_limit = std::numeric_limits<decltype(m_strm.avail_out)>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        auto aim_output = (max_chunk / sizeof (internal_type) > to_max)
                        ? static_cast<decltype(m_strm.avail_out)>(to_max * sizeof (internal_type))
                        : static_cast<decltype(m_strm.avail_out)>(max_chunk);

        m_strm.avail_out = aim_output;
        m_strm.next_out = reinterpret_cast<unsigned char*>(to);
        auto ret = inflate(&m_strm, Z_NO_FLUSH);
        zerr<true>("zlib_cvt::get fail", ret);

        while (m_strm.avail_out && ret != Z_STREAM_END)
        {
            auto [ptr, len] = rd.get_buf(1);
            if (len == 0) break;
            m_strm.next_in = (unsigned char*)(ptr);
            m_strm.avail_in = 1;
            ret = inflate(&m_strm, Z_NO_FLUSH);
            assert(m_strm.avail_in == 0);
            zerr<true>("zlib_cvt::get fail", ret);
        }

        auto res = aim_output - m_strm.avail_out;
        m_strm.next_in = nullptr;
        m_strm.avail_in = 0;
        m_strm.next_out = nullptr;
        m_strm.avail_out = 0;

        if (res % sizeof(internal_type))
            throw cvt_error("zlib_cvt::get fails: partial sequence");
        return res / sizeof(internal_type);
    }

    void put(const internal_type* _to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (!m_bos_done) return BT::put_bos(_to, to_size);

        if (BT::m_io_status != io_status::output)
            throw cvt_error("zlib_cvt::put fails: not available");

        constexpr size_t max_type_limit = std::numeric_limits<decltype(m_strm.avail_out)>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        auto to = reinterpret_cast<const unsigned char*>(_to);

        auto wt = this->writer(CHUNK);
        while (to_size > 0)
        {
            auto aim_output = (max_chunk / sizeof (internal_type) > to_size)
                            ? static_cast<decltype(m_strm.avail_out)>(to_size * sizeof (internal_type))
                            : static_cast<decltype(m_strm.avail_out)>(max_chunk);

            size_t write_size = 0;
            while (aim_output != write_size)
            {
                m_strm.next_in = const_cast<unsigned char*>(to + write_size);
                size_t cur_put_size = static_cast<size_t>(aim_output - write_size);
                m_strm.avail_in = cur_put_size;
                m_strm.next_out = (unsigned char*)(wt.put_buf(CHUNK));
                m_strm.avail_out = CHUNK;

                auto ret = deflate(&m_strm, Z_NO_FLUSH);
                zerr("zlib_cvt::put fail", ret);
                write_size += cur_put_size - m_strm.avail_in;

                if (m_strm.avail_out)
                    wt.rollback(m_strm.avail_out);
            }
            to += aim_output;
            to_size -= aim_output / sizeof (internal_type);
        }
        wt.commit();

        m_strm.next_out = nullptr;
        m_strm.avail_out = 0;
        m_strm.next_in = nullptr;
        m_strm.avail_in = 0;
    }
    
    void flush()
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (!m_bos_done)
            return BT::m_kernel.flush();
        
        if (BT::m_io_status != io_status::output)
            throw cvt_error("zlib_cvt::flush fails: not available");

        if (m_sync_flush)
        {
            auto wt = this->writer(CHUNK);
            m_strm.next_in = nullptr;
            m_strm.avail_in = 0;
            while (true)
            {
                m_strm.next_out = (unsigned char*)(wt.put_buf(CHUNK));
                m_strm.avail_out = CHUNK;
                zerr("zlib_cvt::flush fail", deflate(&m_strm, Z_SYNC_FLUSH));
                if (m_strm.avail_out)
                {
                    wt.rollback(m_strm.avail_out);
                    break;
                }
            }
            wt.commit();
            m_strm.next_out = nullptr;
            m_strm.avail_out = 0;
            m_strm.next_in = nullptr;
            m_strm.avail_in = 0;
            BT::m_kernel.flush();
        }
    }

private:
    template <bool IgnoreBufError = false>
    void zerr(const char* info, int ret)
    {
        switch (ret)
        {
        case Z_ERRNO:
            throw cvt_error(std::string(info) + ": zlib_cvt IO error");
        case Z_STREAM_ERROR:
            throw cvt_error(std::string(info) + ": invalid compression level");
        case Z_DATA_ERROR:
            throw cvt_error(std::string(info) + ": invalid or incomplete deflate data");
        case Z_MEM_ERROR:
            throw cvt_error(std::string(info) + ": out of memory");
        case Z_VERSION_ERROR:
            throw cvt_error(std::string(info) + ": zlib version mismatch!");
        case Z_BUF_ERROR:
            if constexpr (!IgnoreBufError)
                throw cvt_error(std::string(info) + ": no progress possible (Z_BUF_ERROR)");
            break;
        default:
            if (ret < 0)
                throw cvt_error(std::string(info) + ": unknown zlib error " + std::to_string(ret));
        }
    }

    void close_stream()
    {
        if (BT::m_io_status == io_status::output)
        {
            if (m_bos_done)
            {
                auto wt = this->writer(CHUNK);
                m_strm.next_in = nullptr;
                m_strm.avail_in = 0;
                while (true)
                {
                    m_strm.next_out = (unsigned char*)(wt.put_buf(CHUNK));
                    m_strm.avail_out = CHUNK;
                    zerr("zlib_cvt::put_end fail", deflate(&m_strm, Z_FINISH));
                    if (m_strm.avail_out)
                    {
                        wt.rollback(m_strm.avail_out);
                        break;
                    }
                }
                wt.commit();
                deflateEnd(&m_strm);
            }
        }
        else if (BT::m_io_status == io_status::input)
            inflateEnd(&m_strm);

        m_bos_done = false;
        BT::m_io_status = io_status::neutral;
        m_sync_flush = false;
        m_strm.next_out = nullptr;
        m_strm.avail_out = 0;
    }
private:
    unsigned    m_put_level;
    bool        m_bos_done;
    bool        m_sync_flush;
    
    z_stream    m_strm{};
};

template <typename TInt>
class zlib_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    zlib_cvt_creator(unsigned level)
        : m_level(level) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return zlib_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_level};
    }
private:
    unsigned m_level;
};
}
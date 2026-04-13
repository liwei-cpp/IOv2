#pragma once
#include <deque>
#include <optional>
#include <utility>
#include <cvt/runtime_cvt.h>

#include <io/io_concepts.h>
#include <common/streambuf_defs.h>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class streambuf
{
public:
    using device_type = TDevice;
    using char_type = TChar;
public:
    explicit streambuf(TDevice dev)
        : m_cvt(make_root_cvt<false>(std::move(dev)))
    {
        init_cvt();
    }

    template <cvt_creator TCreator>
    streambuf(TDevice dev, const TCreator& creator)
        : m_cvt(creator.create(make_root_cvt<false>(std::move(dev))))
    {
        init_cvt();
    }

    streambuf(const streambuf& val) = default;

    streambuf(streambuf&&) = default;
    streambuf& operator=(const streambuf&) = default;
    streambuf& operator=(streambuf&&) = default;

public:
    /// get
    std::optional<char_type> sgetc()
    {
        switch_to_get();

        if (!m_read_buf.empty()) return m_read_buf.front();
        
        char_type c;
        if (m_cvt.get(&c, 1) == 0) return std::optional<char_type>{};
        m_read_buf.push_front(c);
        return c;
    }

    std::optional<char_type> sbumpc()
    {
        switch_to_get();

        if (!m_read_buf.empty())
        {
            char_type c = m_read_buf.front();
            m_read_buf.pop_front();
            return c;
        }
        
        char_type c;
        if (m_cvt.get(&c, 1) == 0) return std::optional<char_type>{};
        return c;
    }

    std::optional<char_type> snextc()
    {
        if (!sbumpc().has_value())
            return std::optional<char_type>{};
        return sgetc();
    }

    size_t sgetn(char_type* s, size_t n)
    {
        if ((s == nullptr) || (n == 0)) return 0;

        switch_to_get();

        size_t res = 0;
        while (!m_read_buf.empty())
        {
            *s++ = m_read_buf.front();
            m_read_buf.pop_front();
            if (++res == n) break;
        }

        while (res < n)
        {
            size_t c = m_cvt.get(s, n - res);
            if (c == 0) break;
            res += c;
            s += c;
        }
        return res;
    }

    /// put
    void sputc(char_type ch)
    {
        switch_to_put();
        m_cvt.put(&ch, 1);
    }

    void sputn(const char_type* s, size_t n)
    {
        switch_to_put();
        m_cvt.put(s, n);
    }
    
    void flush()
    {
        switch_to_put();
        m_cvt.flush();
    }

    /// put_back
    void sputbackc(char_type ch)
    {
        switch_to_get();
        m_read_buf.push_front(ch);
    }

    /// positioning
    size_t tell() const
    {
        const size_t res1 = m_cvt.tell();
        const size_t res2 = m_read_buf.size();
        if (res1 < res2) return 0;
        return res1 - res2;
    }
    
    void seek(size_t pos)
    {
        m_read_buf.clear();
        m_cvt.seek(pos);
    }
    
    void rseek(size_t pos)
    {
        m_read_buf.clear();
        m_cvt.rseek(pos);
    }

    /// io switch
    void switch_to_get()
    {
        m_cvt.switch_to_get();
    }

    void switch_to_put()
    {
        if (m_read_buf.empty()) return m_cvt.switch_to_put();
        
        size_t pos = tell();
        m_cvt.seek(pos);
        m_cvt.switch_to_put();

        m_read_buf.clear();
    }
    
    /// others
    const device_type& device() const &
    {
        return m_cvt.device();
    }

    device_type detach()
    {
        m_read_buf.clear();
        return m_cvt.detach();
    }

    device_type attach(device_type&& dev = device_type{})
    {
        m_read_buf.clear();
        auto res = m_cvt.attach(std::move(dev));
        init_cvt();
        return res;
    }

    void adjust(const cvt_behavior& acc)
    {
        return m_cvt.adjust(acc);
    }

    void retrieve(cvt_status& acc) const
    {
        return m_cvt.retrieve(acc);
    }

private:
    void init_cvt()
    {
        m_cvt.bos();
        m_cvt.main_cont_beg();
    }

private:
    runtime_cvt<TDevice, TChar> m_cvt;
    std::deque<char_type> m_read_buf;
};

template <io_device TDevice, typename TChar>
class istreambuf
{
public:
    using device_type = TDevice;
    using char_type = TChar;

public:
    explicit istreambuf(TDevice dev, bool has_in_buf = true)
        : m_cvt(has_in_buf ? runtime_cvt<TDevice, TChar>(make_root_cvt<true>(std::move(dev)))
                           : runtime_cvt<TDevice, TChar>(make_root_cvt<false>(std::move(dev))))
    {
        init_cvt();
    }

    template <cvt_creator TCreator>
    istreambuf(TDevice dev, const TCreator& creator, bool has_in_buf = true)
        : m_cvt(has_in_buf ? runtime_cvt<TDevice, TChar>(creator.create(make_root_cvt<true>(std::move(dev))))
                           : runtime_cvt<TDevice, TChar>(creator.create(make_root_cvt<false>(std::move(dev)))))
    {
        init_cvt();
    }

public:
    /// get
    std::optional<char_type> sgetc()
    {
        if (!m_read_buf.empty()) return m_read_buf.front();
        
        char_type c;
        if (m_cvt.get(&c, 1) == 0) return std::optional<char_type>{};
        m_read_buf.push_front(c);
        return c;
    }

    std::optional<char_type> sbumpc()
    {
        if (!m_read_buf.empty())
        {
            char_type c = m_read_buf.front();
            m_read_buf.pop_front();
            return c;
        }
        
        char_type c;
        if (m_cvt.get(&c, 1) == 0) return std::optional<char_type>{};
        return c;
    }

    std::optional<char_type> snextc()
    {
        if (!sbumpc().has_value())
            return std::optional<char_type>{};
        return sgetc();
    }

    size_t sgetn(char_type* s, size_t n)
    {
        if ((s == nullptr) || (n == 0)) return 0;

        size_t res = 0;
        while (!m_read_buf.empty())
        {
            *s++ = m_read_buf.front();
            m_read_buf.pop_front();
            if (++res == n) break;
        }

        while (res < n)
        {
            size_t c = m_cvt.get(s, n - res);
            if (c == 0) break;
            res += c;
            s += c;
        }
        return res;
    }

    /// put_back
    void sputbackc(char_type ch)
    {
        m_read_buf.push_front(ch);
    }

    /// positioning
    size_t tell() const
    {
        const size_t res1 = m_cvt.tell();
        const size_t res2 = m_read_buf.size();
        if (res1 < res2) return 0;
        return res1 - res2;
    }
    
    void seek(size_t pos)
    {
        m_read_buf.clear();
        m_cvt.seek(pos);
    }
    
    void rseek(size_t pos)
    {
        m_read_buf.clear();
        m_cvt.rseek(pos);
    }

    /// others
    const device_type& device() const &
    {
        return m_cvt.device();
    }

    device_type detach()
    {
        m_read_buf.clear();
        return m_cvt.detach();
    }

    device_type attach(device_type&& dev = device_type{})
    {
        m_read_buf.clear();
        auto res = m_cvt.attach(std::move(dev));
        init_cvt();
        return res;
    }

    void adjust(const cvt_behavior& acc)
    {
        return m_cvt.adjust(acc);
    }

    void retrieve(cvt_status& acc) const
    {
        return m_cvt.retrieve(acc);
    }

private:
    void init_cvt()
    {
        auto res = m_cvt.bos();
        m_cvt.main_cont_beg();
        if (res != io_status::input)
            m_cvt.switch_to_get();
    }

private:
    runtime_cvt<TDevice, TChar> m_cvt;
    std::deque<char_type> m_read_buf;
};

template <io_device TDevice, typename TChar>
class ostreambuf
{
public:
    using device_type = TDevice;
    using char_type = TChar;

public:
    explicit ostreambuf(TDevice dev)
        : m_cvt(make_root_cvt<false>(std::move(dev)))
    {
        init_cvt();
    }

    template <cvt_creator TCreator>
    ostreambuf(TDevice dev, const TCreator& creator)
        : m_cvt(creator.create(make_root_cvt<false>(std::move(dev))))
    {
        init_cvt();
    }
    
public:
    /// put
    void sputc(char_type ch)
    {
        m_cvt.put(&ch, 1);
    }

    void sputn(const char_type* s, size_t n)
    {
        m_cvt.put(s, n);
    }
    
    void flush()
    {
        m_cvt.flush();
    }

    /// positioning
    size_t tell() const
    {
        return m_cvt.tell();
    }
    
    void seek(size_t pos)
    {
        m_cvt.seek(pos);
    }
    
    void rseek(size_t pos)
    {
        m_cvt.rseek(pos);
    }

    /// others
    const device_type& device() const &
    {
        return m_cvt.device();
    }

    device_type detach()
    {
        return m_cvt.detach();
    }

    device_type attach(device_type&& dev = device_type{})
    {
        auto res = m_cvt.attach(std::move(dev));
        init_cvt();
        return res;
    }

    void adjust(const cvt_behavior& acc)
    {
        return m_cvt.adjust(acc);
    }

    void retrieve(cvt_status& acc) const
    {
        return m_cvt.retrieve(acc);
    }

private:
    void init_cvt()
    {
        auto res = m_cvt.bos();
        m_cvt.main_cont_beg();
        if (res != io_status::output)
            m_cvt.switch_to_put();
    }

private:
    runtime_cvt<TDevice, TChar> m_cvt;
};

template <io_device TDevice>
streambuf(TDevice) -> streambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
streambuf(TDevice, const TCreator&) -> streambuf<TDevice, ext_to_int<TDevice, TCreator>>;

template <io_device TDevice>
istreambuf(TDevice) -> istreambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
istreambuf(TDevice, const TCreator&) -> istreambuf<TDevice, ext_to_int<TDevice, TCreator>>;

template <io_device TDevice>
ostreambuf(TDevice) -> ostreambuf<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
ostreambuf(TDevice, const TCreator&) -> ostreambuf<TDevice, ext_to_int<TDevice, TCreator>>;
}
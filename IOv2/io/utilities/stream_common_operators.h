#pragma once

#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>

namespace IOv2
{
template <typename T, typename TDevice, typename TChar>
struct stream_common_operators
{
    size_t tell()
    {
        T& obj = static_cast<T&>(*this);
        try
        {
            return obj.m_streambuf.tell();
        }
        catch(...)
        {
            obj.handle_exception(std::current_exception());
        }

        return static_cast<size_t>(-1);
    }

    T& seek(size_t pos)
    {
        T& obj = static_cast<T&>(*this);

        try
        {
            obj.clear(obj.rdstate() & ~ios_defs::eofbit);
            obj.m_streambuf.seek(pos);
        }
        catch(...)
        {
            obj.handle_exception(std::current_exception());
        }

        return obj;
    }
    
    T& rseek(size_t pos)
    {
        T& obj = static_cast<T&>(*this);

        try
        {
            obj.clear(obj.rdstate() & ~ios_defs::eofbit);
            obj.m_streambuf.rseek(pos);
        }
        catch(...)
        {
            obj.handle_exception(std::current_exception());
        }

        return obj;
    }

    const TDevice& device() const &
    {
        const T& obj = static_cast<const T&>(*this);
        return obj.m_streambuf.device();
    }

    TDevice detach()
    {
        T& obj = static_cast<T&>(*this);
        return obj.m_streambuf.detach();
    }

    TDevice attach(TDevice&& dev = TDevice{})
    {
        T& obj = static_cast<T&>(*this);
        return obj.m_streambuf.attach(std::move(dev));
    }

    void adjust(const cvt_behavior& acc)
    {
        T& obj = static_cast<T&>(*this);
        return obj.m_streambuf.adjust(acc);
    }

    void retrieve(cvt_status& acc) const
    {
        T& obj = static_cast<T&>(*this);
        return obj.m_streambuf.retrieve(acc);        
    }

    abs_ostream* tie(abs_ostream* str)
    {
        T& obj = static_cast<T&>(*this);
        auto res = obj.m_tie_stream;
        obj.m_tie_stream = str;
        return res;
    }

    abs_ostream* tie()
    {
        T& obj = static_cast<T&>(*this);
        return obj.m_tie_stream;
    }

    const IOv2::locale<TChar>& locale() const
    {
        const T& obj = static_cast<const T&>(*this);
        return obj.m_locale;
    }
    
    IOv2::locale<TChar> locale(const IOv2::locale<TChar>& loc)
    {
        T& obj = static_cast<T&>(*this);
        auto res = std::move(obj.m_locale);
        obj.m_locale = loc;
        obj.access_callbacks(obj.m_locale);
        return res;
    }

    std::mutex& io_mutex()
    {
        T& obj = static_cast<T&>(*this);
        return obj.m_io_mutex;
    }
};
}
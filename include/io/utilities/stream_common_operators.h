#pragma once

#include <common/copyable_mutex.h>

#include <exception>
#include <utility>

#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>

namespace IOv2
{
template <typename TDevice, typename TChar>
struct stream_common_operators
{
    template <typename TSelf>
    size_t tell(this TSelf& self)
    {
        try
        {
            return self.m_streambuf.tell();
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return static_cast<size_t>(-1);
    }

    template <typename TSelf>
    TSelf& seek(this TSelf& self, size_t pos)
    {
        try
        {
            self.clear(self.rdstate() & ~ios_defs::eofbit);
            self.m_streambuf.seek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    TSelf& rseek(this TSelf& self, size_t pos)
    {
        try
        {
            self.clear(self.rdstate() & ~ios_defs::eofbit);
            self.m_streambuf.rseek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    TDevice& device(this TSelf& self)
    {
        return self.m_streambuf.device();
    }

    template <typename TSelf>
    std::pair<TDevice, std::exception_ptr> detach(this TSelf& self) noexcept
    {
        return self.m_streambuf.detach();
    }

    template <typename TSelf>
    void attach(this TSelf& self, TDevice&& dev = TDevice{})
    {
        self.m_streambuf.attach(std::move(dev));
    }

    template <typename TSelf>
    void adjust(this TSelf& self, const cvt_behavior& acc)
    {
        return self.m_streambuf.adjust(acc);
    }

    template <typename TSelf>
    void retrieve(this const TSelf& self, cvt_status& acc)
    {
        return self.m_streambuf.retrieve(acc);
    }

    template <typename TSelf>
    abs_ostream* tie(this TSelf& self, abs_ostream* str)
    {
        auto res = self.m_tie_stream;
        self.m_tie_stream = str;
        return res;
    }

    template <typename TSelf>
    abs_ostream* tie(this TSelf& self)
    {
        return self.m_tie_stream;
    }

    template <typename TSelf>
    const IOv2::locale<TChar>& locale(this const TSelf& self)
    {
        return self.m_locale;
    }

    template <typename TSelf>
    IOv2::locale<TChar> locale(this TSelf& self, const IOv2::locale<TChar>& loc)
    {
        auto res = std::move(self.m_locale);
        self.m_locale = loc;
        try
        {
            self.access_callbacks(self.m_locale);
        }
        catch (...)
        {
            self.handle_exception(std::current_exception());
        }
        return res;
    }

    template <typename TSelf>
    copyable_mutex& io_mutex(this TSelf& self)
    {
        return self.m_io_mutex;
    }
};
}
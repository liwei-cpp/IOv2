#pragma once
#include <type_traits>

#include <common/sing_temp.h>
#include <cvt/code_cvt_stdio.h>
#include <cvt/runtime_cvt.h>
#include <device/std_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/ostream.h>
#include <io/utilities/ostream_operators.h>
#include <io/utilities/stream_common_operators.h>

namespace IOv2
{
class __cout;
class __cerr;
class __clog;
class __wcout;
class __wcerr;
class __wclog;

template <typename T, typename TDevice, typename TChar>
class stdout_api : public ios_base<TChar>
                 , public ostream_operators<T, TChar>
                 , public stream_common_operators<T, TDevice, TChar>
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using out_sentry_type = out_sentry<T, false, true>;

    friend out_sentry_type;
    friend ostream_operators<T, TChar>;
    friend stream_common_operators<T, device_type, TChar>;

public:
    stdout_api()
        : m_streambuf(device_type{}) {}

    template <cvt_creator TCreator>
    stdout_api(const TCreator& creator)
        : m_streambuf(device_type{}, creator) {}

public:
    bool sync_with_stdio(bool sync = true)
    {
        auto res = m_sync_with_stdio;
        m_sync_with_stdio = sync;
        return res;
    }

    std::string code() const
        requires std::is_same_v<TChar, wchar_t>
    {
        code_cvt_access acc;
        m_streambuf.retrieve(acc);
        return acc.code;
    }

    std::string switch_code(const std::string& new_code)
        requires std::is_same_v<TChar, wchar_t>
    {
        auto res = code();
        if (res != new_code)
        {
            code_cvt_switch acc(new_code);
            m_streambuf.adjust(acc);
        }
        return res;
    }

    device_type detach() = delete;
    device_type attach(device_type&& dev = device_type{}) = delete;

    void reset() // mainly used for unit-test
    {
        this->clear();
        this->exceptions(ios_defs::goodbit);
        m_streambuf.attach();
    }

protected:
    ostreambuf<device_type, char_type> m_streambuf;
    abs_ostream* m_tie_stream = nullptr;
    locale<char_type> m_locale;
    std::mutex        m_io_mutex;
    bool m_sync_with_stdio = true;
};

/// __cout
class __cout : public stdout_api<__cout, std_device<STDOUT_FILENO>, char>
             , public sing_temp<__cout>
{
    using BT = stdout_api<__cout, std_device<STDOUT_FILENO>, char>;
    friend sing_temp<__cout>;

private:
    __cout() = default;
    __cout(const __cout&) = delete;
    __cout& operator= (const __cout&) = delete;

    ~__cout()
    {
        flush();
    }
};

static __cout::init _cout_init;
static __cout& cout = *__cout::ptr();


/// cerr
class __cerr : public stdout_api<__cerr, std_device<STDERR_FILENO>, char>
             , public sing_temp<__cerr>
{
    using BT = stdout_api<__cerr, std_device<STDERR_FILENO>, char>;
    friend sing_temp<__cerr>;

private:
    __cerr()
        : BT()
    {
        tie(&cout);
        setf(ios_defs::unitbuf);
    }

    __cerr(const __cerr&) = delete;
    __cerr& operator= (const __cerr&) = delete;

    ~__cerr()
    {
        flush();
    }
};

static __cerr::init _cerr_init;
static __cerr& cerr = *__cerr::ptr();

/// clog
class __clog : public stdout_api<__clog, std_device<STDERR_FILENO>, char>
             , public sing_temp<__clog>
{
    using BT = stdout_api<__clog, std_device<STDERR_FILENO>, char>;
    friend sing_temp<__clog>;

private:
    __clog() = default;

    __clog(const __clog&) = delete;
    __clog& operator= (const __clog&) = delete;

    ~__clog()
    {
        flush();
    }
};

static __clog::init _clog_init;
static __clog& clog = *__clog::ptr();

/// wcout
class __wcout : public stdout_api<__wcout, std_device<STDOUT_FILENO>, wchar_t>
              , public sing_temp<__wcout>
{
    using BT = stdout_api<__wcout, std_device<STDOUT_FILENO>, wchar_t>;
    friend sing_temp<__wcout>;

private:
    __wcout()
        : BT(code_cvt_stdio_creator(std::setlocale(LC_CTYPE, nullptr)))
    {}

    __wcout(const __wcout&) = delete;
    __wcout& operator= (const __wcout&) = delete;

    ~__wcout()
    {
        flush();
    }
};

static __wcout::init _wcout_init;
static __wcout& wcout = *__wcout::ptr();

/// wcerr
class __wcerr : public stdout_api<__wcerr, std_device<STDERR_FILENO>, wchar_t>
              , public sing_temp<__wcerr>
{
    using BT = stdout_api<__wcerr, std_device<STDERR_FILENO>, wchar_t>;
    friend sing_temp<__wcerr>;

private:
    __wcerr()
        : BT(code_cvt_stdio_creator(std::setlocale(LC_CTYPE, nullptr)))
    {
        tie(&wcout);
        setf(ios_defs::unitbuf);
    }

    __wcerr(const __wcerr&) = delete;
    __wcerr& operator= (const __wcerr&) = delete;

    ~__wcerr()
    {
        flush();
    }
};

static __wcerr::init _wcerr_init;
static __wcerr& wcerr = *__wcerr::ptr();

/// wclog
class __wclog : public stdout_api<__wclog, std_device<STDERR_FILENO>, wchar_t>
              , public sing_temp<__wclog>
{
    using BT = stdout_api<__wclog, std_device<STDERR_FILENO>, wchar_t>;
    friend sing_temp<__wclog>;

private:
    __wclog()
        : BT(code_cvt_stdio_creator(std::setlocale(LC_CTYPE, nullptr)))
    {}

    __wclog(const __wclog&) = delete;
    __wclog& operator= (const __wclog&) = delete;

    ~__wclog()
    {
        flush();
    }
};

static __wclog::init _wclog_init;
static __wclog& wclog = *__wclog::ptr();
}
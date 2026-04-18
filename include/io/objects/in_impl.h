#pragma once
#include <common/sing_temp.h>
#include <cvt/code_cvt_stdio.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/std_device.h>
#include <io/istream.h>
#include <io/objects/out_impl.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/stream_common_operators.h>

namespace IOv2
{
class __cin;
class __wcin;

template <typename T, io_device TDevice, typename TChar>
class stdin_api : public ios_base<TChar>
                , public istream_operators<T, TChar>
                , public stream_common_operators<T, TDevice, TChar>
{
    friend istream_operators<T, TChar>;
    friend stream_common_operators<T, TDevice, TChar>;

public:
    using device_type = TDevice;
    using char_type = TChar;
    using in_sentry_type = in_sentry<T, false>;
    friend in_sentry_type;

public:
    stdin_api()
        : m_streambuf(device_type{}, false)
    {}

    template <cvt_creator TCreator>
    stdin_api(const TCreator& creator)
        : m_streambuf(device_type{}, creator, false)
    {}

public:
    bool sync_with_stdio(bool sync = true)
    {
        auto old_sync_state = m_sync_with_stdio;
        if (old_sync_state == sync)
            return old_sync_state;
        m_sync_with_stdio = sync;

        auto dev = m_streambuf.detach();
        if constexpr (std::is_same_v<char_type, char>)
            m_streambuf = istreambuf<device_type, char_type>(std::move(dev), !sync);
        else if constexpr (std::is_same_v<char_type, wchar_t>)
            m_streambuf = istreambuf<device_type, wchar_t>(std::move(dev), code_cvt_stdio_creator(code()), !sync);
        else
            static_assert(DependencyFalse<char_type>, "invalid character type");
        return old_sync_state;
    }

    device_type detach()= delete;
    device_type attach(device_type&&) = delete;

    void reset() // mainly used for unit-test
    {
        this->clear();
        this->exceptions(ios_defs::goodbit);
        m_streambuf.attach();
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

protected:
    istreambuf<device_type, char_type>      m_streambuf;
    abs_ostream*                            m_tie_stream = nullptr;
    locale<char_type>                       m_locale;
    std::mutex                              m_io_mutex;
    bool m_sync_with_stdio = true;
};


/// cin
class __cin : public stdin_api<__cin, std_device<STDIN_FILENO>, char>
            , public sing_temp<__cin>
{
    using BT = stdin_api<__cin, std_device<STDIN_FILENO>, char>;
    friend sing_temp<__cin>;

private:
    __cin()
        : BT()
    {
        tie(&cout);
    }

    __cin(const __cin&) = delete;
    __cin& operator=(const __cin&) = delete;
};

static __cin::init _cin_init;
static __cin& cin = *__cin::ptr();

/// wcin
class __wcin : public stdin_api<__wcin, std_device<STDIN_FILENO>, wchar_t>
             , public sing_temp<__wcin>
{
    using BT = stdin_api<__wcin, std_device<STDIN_FILENO>, wchar_t>;
    friend sing_temp<__wcin>;

private:
    __wcin()
        : BT(code_cvt_stdio_creator(std::setlocale(LC_CTYPE, nullptr)))
    {
        tie(&wcout);
    }

    __wcin(const __wcin&) = delete;
    __wcin& operator=(const __wcin&) = delete;
};

static __wcin::init _wcin_init;
static __wcin& wcin = *__wcin::ptr();
}
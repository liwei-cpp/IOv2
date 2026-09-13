// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <cstdlib>
#include <mutex>

#include <IOv2/common/copyable_mutex.h>
#include <IOv2/common/sing_temp.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/utilities/istream_operators.h>
#include <IOv2/io/utilities/stream_common_operators.h>
#include <IOv2/locale/locale.h>

namespace IOv2
{
class cin_t;
class wcin_t;

template <typename T, io_device TDevice, typename TChar>
class stdin_api : public ios_state<TChar>
                , public istream_operators<TChar>
                , public stream_common_operators
{
    friend istream_operators<TChar>;
    friend stream_common_operators;

public:
    using device_type = TDevice;
    using char_type = TChar;
    using in_sentry_type = in_sentry<T, false>;
    using in_iter_type = istreambuf_iterator<istreambuf<device_type, char_type>>;
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

        auto [dev, err] = m_streambuf.detach();
        if constexpr (std::is_same_v<char_type, char>)
            m_streambuf = istreambuf<device_type, char_type>(std::move(dev), !sync);
        else if constexpr (std::is_same_v<char_type, wchar_t>)
            m_streambuf = istreambuf<device_type, wchar_t>(std::move(dev), code_cvt_stdio_creator(code()), !sync);
        else
            static_assert(dependent_false_v<char_type>, "invalid character type");
        if (err) std::rethrow_exception(err);
        return old_sync_state;
    }

    std::pair<device_type, std::exception_ptr> detach() = delete;
    void attach(device_type&&) = delete;

    void reset() // mainly used for unit-test
    {
        std::lock_guard guard(this->io_mutex());
        this->clear();
        this->exceptions(ios_defs::goodbit);
        m_streambuf.attach();
    }

    std::string code() const
        requires std::is_same_v<TChar, wchar_t>
    {
        std::lock_guard guard(this->io_mutex());
        code_cvt_access acc;
        m_streambuf.retrieve(acc);
        return acc.code;
    }

    std::string switch_code(const std::string& new_code)
        requires std::is_same_v<TChar, wchar_t>
    {
        std::lock_guard guard(this->io_mutex());
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
    IOv2::locale<char_type>                 m_locale;
    bool m_sync_with_stdio = true;
};


/// cin
class cin_t : public stdin_api<cin_t, std_device<STDIN_FILENO>, char>
            , public sing_temp<cin_t>
{
    using BT = stdin_api<cin_t, std_device<STDIN_FILENO>, char>;
    friend sing_temp<cin_t>;

private:
    cin_t()
        : BT()
        , sing_temp<cin_t>([](cin_t*) noexcept {})   // never destroyed at exit, like std::cin
    {
        tie(&cout);
    }

    cin_t(const cin_t&) = delete;
    cin_t& operator=(const cin_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API cin_t& cin;   // defined in iov2_objects.cpp
#else
inline cin_t::init _cin_init;
inline cin_t&      cin = *cin_t::ptr();
#endif

/// wcin
class wcin_t : public stdin_api<wcin_t, std_device<STDIN_FILENO>, wchar_t>
             , public sing_temp<wcin_t>
{
    using BT = stdin_api<wcin_t, std_device<STDIN_FILENO>, wchar_t>;
    friend sing_temp<wcin_t>;

private:
    wcin_t()
        : BT(code_cvt_stdio_creator(IOv2::locale<char>::initial_locale_name(LC_CTYPE)))
        , sing_temp<wcin_t>([](wcin_t*) noexcept {})   // never destroyed at exit, like std::wcin
    {
        tie(&wcout);
    }

    wcin_t(const wcin_t&) = delete;
    wcin_t& operator=(const wcin_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API wcin_t& wcin;   // defined in iov2_objects.cpp
#else
inline wcin_t::init _wcin_init;
inline wcin_t&      wcin = *wcin_t::ptr();
#endif
}

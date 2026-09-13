// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <mutex>
#include <type_traits>

#include <IOv2/common/copyable_atomic.h>
#include <IOv2/common/copyable_mutex.h>
#include <IOv2/common/sing_temp.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/utilities/ostream_operators.h>
#include <IOv2/io/utilities/stream_common_operators.h>
#include <IOv2/locale/locale.h>

namespace IOv2
{
class cout_t;
class cerr_t;
class clog_t;
class wcout_t;
class wcerr_t;
class wclog_t;

template <typename T, typename TDevice, typename TChar>
class stdout_api : public ios_state<TChar>
                 , public out_flusher<T>
                 , public ostream_operators<TChar>
                 , public stream_common_operators
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using out_sentry_type = out_sentry<T, false, true>;
    using out_iter_type = ostreambuf_iterator<ostreambuf<device_type, char_type>>;

    friend out_sentry_type;
    friend out_flusher<T>;
    friend ostream_operators<TChar>;
    friend stream_common_operators;

public:
    stdout_api()
        : m_streambuf(device_type{}) {}

    template <cvt_creator TCreator>
    stdout_api(const TCreator& creator)
        : m_streambuf(device_type{}, creator) {}

public:
    /**
     * @lang{ZH}
     * @brief 切换本流是否与 C stdio 同步：同步时每次插入结束都把本流缓冲推进 stdio 缓冲，
     * 以保持与 `printf` 等的交错顺序；不同步时本流自行缓冲。
     *
     * 只是一次原子交换，不会失败；与并发的插入操作安全竞争。
     *
     * @param sync `true` 为同步（默认），`false` 为自行缓冲。
     * @return 调用前的同步状态。
     * @endif
     *
     * @lang{EN}
     * @brief Switches whether this stream is synchronized with C stdio: synchronized
     * pushes this stream's buffer into the stdio buffer at the end of every insertion,
     * keeping the interleaving with `printf` and friends; unsynchronized buffers on its
     * own.
     *
     * A single atomic exchange, cannot fail; safe against concurrent insertions.
     *
     * @param sync `true` for synchronized (the default), `false` for own buffering.
     * @return The synchronization state before the call.
     * @endif
     */
    bool sync_with_stdio(bool sync = true) noexcept
    {
        return m_sync_with_stdio.exchange(sync);
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

    std::pair<device_type, std::exception_ptr> detach() = delete;
    void attach(device_type&& dev = device_type{}) = delete;

    void reset() // mainly used for unit-test
    {
        std::lock_guard guard(this->io_mutex());
        this->clear();
        this->exceptions(ios_defs::goodbit);
        m_streambuf.attach();
    }

protected:
    ostreambuf<device_type, char_type> m_streambuf;
    IOv2::locale<char_type> m_locale;
    copyable_atomic<bool> m_sync_with_stdio{true};   ///< @lang{ZH} 为 true 时每次插入结束（输出哨兵析构）都把本流缓冲推进 stdio 缓冲；与进程退出时的刷新无关。原子量，使 `sync_with_stdio()` 可与并发输出操作安全竞争。 @endif @lang{EN} When true, every insertion (the output sentry's destructor) pushes this stream's buffer into the stdio buffer; unrelated to the flush at process exit. Atomic so `sync_with_stdio()` is safe against concurrent output operations. @endif
};

/// cout_t
class cout_t : public stdout_api<cout_t, std_device<STDOUT_FILENO>, char>
             , public sing_temp<cout_t>
{
    using BT = stdout_api<cout_t, std_device<STDOUT_FILENO>, char>;
    friend sing_temp<cout_t>;

private:
    cout_t()
        : sing_temp<cout_t>([](cout_t* p) noexcept { try { p->flush(); } catch (...) {} })
    {}

    cout_t(const cout_t&) = delete;
    cout_t& operator=(const cout_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API cout_t& cout;   // defined in iov2_objects.cpp
#else
inline cout_t::init _cout_init;
inline cout_t&      cout = *cout_t::ptr();
#endif


/// cerr
class cerr_t : public stdout_api<cerr_t, std_device<STDERR_FILENO>, char>
             , public sing_temp<cerr_t>
{
    using BT = stdout_api<cerr_t, std_device<STDERR_FILENO>, char>;
    friend sing_temp<cerr_t>;

private:
    cerr_t()
        : BT()
        , sing_temp<cerr_t>([](cerr_t* p) noexcept { try { p->flush(); } catch (...) {} })
    {
        tie(&cout);
        setf(ios_defs::unitbuf);
    }

    cerr_t(const cerr_t&) = delete;
    cerr_t& operator=(const cerr_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API cerr_t& cerr;   // defined in iov2_objects.cpp
#else
inline cerr_t::init _cerr_init;
inline cerr_t&      cerr = *cerr_t::ptr();
#endif

/// clog
class clog_t : public stdout_api<clog_t, std_device<STDERR_FILENO>, char>
             , public sing_temp<clog_t>
{
    using BT = stdout_api<clog_t, std_device<STDERR_FILENO>, char>;
    friend sing_temp<clog_t>;

private:
    clog_t()
        : sing_temp<clog_t>([](clog_t* p) noexcept { try { p->flush(); } catch (...) {} })
    {}

    clog_t(const clog_t&) = delete;
    clog_t& operator=(const clog_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API clog_t& clog;   // defined in iov2_objects.cpp
#else
inline clog_t::init _clog_init;
inline clog_t&      clog = *clog_t::ptr();
#endif

/// wcout
class wcout_t : public stdout_api<wcout_t, std_device<STDOUT_FILENO>, wchar_t>
              , public sing_temp<wcout_t>
{
    using BT = stdout_api<wcout_t, std_device<STDOUT_FILENO>, wchar_t>;
    friend sing_temp<wcout_t>;

private:
    wcout_t()
        : BT(code_cvt_stdio_creator(IOv2::locale<char>::initial_locale_name(LC_CTYPE)))
        , sing_temp<wcout_t>([](wcout_t* p) noexcept { try { p->flush(); } catch (...) {} })
    {}

    wcout_t(const wcout_t&) = delete;
    wcout_t& operator=(const wcout_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API wcout_t& wcout;   // defined in iov2_objects.cpp
#else
inline wcout_t::init _wcout_init;
inline wcout_t&      wcout = *wcout_t::ptr();
#endif

/// wcerr
class wcerr_t : public stdout_api<wcerr_t, std_device<STDERR_FILENO>, wchar_t>
              , public sing_temp<wcerr_t>
{
    using BT = stdout_api<wcerr_t, std_device<STDERR_FILENO>, wchar_t>;
    friend sing_temp<wcerr_t>;

private:
    wcerr_t()
        : BT(code_cvt_stdio_creator(IOv2::locale<char>::initial_locale_name(LC_CTYPE)))
        , sing_temp<wcerr_t>([](wcerr_t* p) noexcept { try { p->flush(); } catch (...) {} })
    {
        tie(&wcout);
        setf(ios_defs::unitbuf);
    }

    wcerr_t(const wcerr_t&) = delete;
    wcerr_t& operator=(const wcerr_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API wcerr_t& wcerr;   // defined in iov2_objects.cpp
#else
inline wcerr_t::init _wcerr_init;
inline wcerr_t&      wcerr = *wcerr_t::ptr();
#endif

/// wclog
class wclog_t : public stdout_api<wclog_t, std_device<STDERR_FILENO>, wchar_t>
              , public sing_temp<wclog_t>
{
    using BT = stdout_api<wclog_t, std_device<STDERR_FILENO>, wchar_t>;
    friend sing_temp<wclog_t>;

private:
    wclog_t()
        : BT(code_cvt_stdio_creator(IOv2::locale<char>::initial_locale_name(LC_CTYPE)))
        , sing_temp<wclog_t>([](wclog_t* p) noexcept { try { p->flush(); } catch (...) {} })
    {}

    wclog_t(const wclog_t&) = delete;
    wclog_t& operator=(const wclog_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API wclog_t& wclog;   // defined in iov2_objects.cpp
#else
inline wclog_t::init _wclog_init;
inline wclog_t&      wclog = *wclog_t::ptr();
#endif
}

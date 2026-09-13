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
    /**
     * @lang{ZH}
     * @brief 切换本流是否与 C stdio 同步：同步时逐字节读 `stdin`，不同步时自带读缓冲。
     *
     * 切换意味着换掉整个 streambuf（先 `detach()` 旧的，再以同一设备重建）。已缓冲但未消费的
     * 输入按 streambuf.h 的 `detach()` 契约丢弃；因此应在任何 stdin 读取之前调用。
     *
     * 本函数**不会失败**。重建 streambuf 只可能因内存耗尽或（`wchar_t`）当前编码的 locale
     * 数据库在运行期间消失而抛出——两者都是运行环境已坏、调用方无从处置的情形，此时直接
     * `std::abort()`，而不是留下一个半换的流。这与 `sing_temp::init` 构造标准流失败即
     * `abort` 的策略一致；libstdc++ 在同一位置失败后留下的是悬垂的 `rdbuf`（未定义行为）。
     *
     * @param sync `true` 为同步（默认），`false` 为自带缓冲。
     * @return 调用前的同步状态。
     * @endif
     *
     * @lang{EN}
     * @brief Switches whether this stream is synchronized with C stdio: synchronized
     * reads `stdin` byte by byte, unsynchronized reads through its own buffer.
     *
     * Switching replaces the whole streambuf (`detach()` the old one, rebuild on the
     * same device). Input that was buffered but not yet consumed is discarded per the
     * `detach()` contract in streambuf.h; call this before any stdin read.
     *
     * This function **cannot fail**. Rebuilding the streambuf can only throw on memory
     * exhaustion or (`wchar_t`) when the locale database of the current code vanished
     * while the process runs -- both mean the runtime environment is broken and there
     * is nothing the caller could do, so this calls `std::abort()` instead of leaving a
     * half-replaced stream. This matches the `sing_temp::init` policy of aborting when
     * a standard stream cannot be constructed; libstdc++ failing at the same point
     * leaves a dangling `rdbuf` (undefined behavior).
     *
     * @param sync `true` for synchronized (the default), `false` for own buffering.
     * @return The synchronization state before the call.
     * @endif
     */
    bool sync_with_stdio(bool sync = true) noexcept
    {
        std::lock_guard guard(this->io_mutex());
        auto old_sync_state = m_sync_with_stdio;
        if (old_sync_state == sync)
            return old_sync_state;

        try {
            auto [dev, err] = m_streambuf.detach();
            if (err) std::abort();
            if constexpr (std::is_same_v<char_type, char>)
                m_streambuf = istreambuf<device_type, char_type>(std::move(dev), !sync);
            else if constexpr (std::is_same_v<char_type, wchar_t>)
                m_streambuf = istreambuf<device_type, wchar_t>(std::move(dev), code_cvt_stdio_creator(code()), !sync);
            else
                static_assert(dependent_false_v<char_type>, "invalid character type");
        } catch (...) {
            // Memory exhaustion or a vanished locale database: no usable stream to
            // hand back. Same policy as sing_temp::init.
            std::abort();
        }
        m_sync_with_stdio = sync;
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

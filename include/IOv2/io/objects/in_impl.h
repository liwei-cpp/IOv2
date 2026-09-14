// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file in_impl.h
 * @lang{ZH}
 * 定义标准输入流的实现模板 `stdin_api`，以及两个标准输入流对象 `cin`（`char`）与 `wcin`
 * （`wchar_t`）。
 *
 * `stdin_api` 不由 `istream` 派生，而是同样把一条 `istreambuf`（其下依次是转换器管线与固定 fd
 * 的设备 `std_device<STDIN_FILENO>`）与一个 `locale` 组合起来，对外接口来自 `ios_state`（状态位
 * 与异常掩码）、`istream_operators`（输入操作）与 `stream_common_operators`（`tell()` /
 * `locale()` 等）三个基类。与 `istream` 的差别都来自「设备是固定 fd 的进程级单例」：多了
 * `sync_with_stdio()`、`reset()` 与（宽流）`code()` / `switch_code()`，换设备的 `detach()` /
 * `attach()` 则被 `= delete`。
 *
 * 两个流对象经 `sing_temp` 成为进程级单例，退出钩子为空——与 `std::cin` 一样，退出时不析构，
 * 退出阶段既有引用仍然有效。构造时各自 `tie()` 到同字符类型的输出流（`cin` → `cout`，
 * `wcin` → `wcout`）。
 *
 * @note 一般不直接包含本头文件，而是包含 `IOv2/io/objects/objects.h`：入口那里还有一次切换全部
 *       八个标准流的 `sync_with_stdio()` 与 `ws` / `endl` 等操纵符，并说明了本系列头文件不带来
 *       哪些能力。
 * @endif
 *
 * @lang{EN}
 * Defines `stdin_api`, the implementation template behind the standard input streams, along
 * with the two standard input stream objects `cin` (`char`) and `wcin` (`wchar_t`).
 *
 * `stdin_api` does not derive from `istream`; it combines, in the same way, an `istreambuf`
 * (below which sit the converter pipeline and the fixed-fd device `std_device<STDIN_FILENO>`)
 * with a `locale`, and takes its interface from three bases -- `ios_state` (the state bits and
 * the exception mask), `istream_operators` (the input operations) and
 * `stream_common_operators` (`tell()` / `locale()` and friends). Every difference from
 * `istream` follows from the device being a fixed fd owned by a process-wide singleton: it
 * adds `sync_with_stdio()`, `reset()` and, on the wide stream, `code()` / `switch_code()`,
 * while `detach()` / `attach()`, which would replace the device, are `= delete`.
 *
 * Both stream objects are process-wide singletons through `sing_temp` with an empty exit
 * hook -- like `std::cin` they are not destroyed at exit, so existing references stay valid
 * once exit begins. Each ties itself at construction to the output stream of the same
 * character type (`cin` to `cout`, `wcin` to `wcout`).
 *
 * @note Prefer including `IOv2/io/objects/objects.h` over this header: the entry point also
 *       brings the `sync_with_stdio()` that switches all eight standard streams at once and
 *       the `ws` / `endl` manipulators, and documents what this family of headers does not
 *       bring in.
 * @endif
 */
#pragma once
#include <IOv2/common/iov2_export.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/common/sing_temp.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/streambuf.h>
#include <IOv2/io/streambuf_iterator.h>
#include <IOv2/io/utilities/istream_operators.h>
#include <IOv2/io/utilities/stream_common_operators.h>
#include <IOv2/locale/locale.h>

#include <clocale>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

#include <unistd.h>

namespace IOv2
{
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
     * 输入按 streambuf.h 的 `detach()` 契约丢弃；因此应在任何 stdin 读取之前调用，也**不得**在
     * 一次提取进行中（例如用户 `io_traits::sread` 里）重入调用：`io_mutex()` 是递归锁不会拦，
     * 但正在使用的 streambuf 会被整个换掉。
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
     * `detach()` contract in streambuf.h; call this before any stdin read, and never
     * re-enter it from inside an extraction (a user `io_traits::sread`, say): `io_mutex()`
     * is recursive and will not stop it, but the streambuf in use is replaced wholesale.
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

    /**
     * @lang{ZH}
     * `stream_common_operators` 的换设备接口在标准流上删除：本流的设备是固定的 fd 0，取出去
     * 就再也装不回来，换进去等于给一个进程级单例改写底层来源。需要「在同一 fd 上从头开始」
     * 请用 `reset()`（它走的是 streambuf 那一层的 `attach()`，装一个同 fd 的缺省设备）。
     * @endif
     *
     * @lang{EN}
     * The device-replacing interface of `stream_common_operators` is deleted on the standard
     * streams: this stream's device is the fixed fd 0, taking it out leaves no way to put it
     * back, and putting another one in rewrites the source of a process-wide singleton. To
     * start over on the same fd use `reset()`, which goes through the `attach()` one layer
     * down, in the streambuf, with a default device on the same fd.
     * @endif
     */
    std::pair<device_type, std::exception_ptr> detach() = delete;
    void attach(device_type&&) = delete;

    /**
     * @lang{ZH}
     * @brief 在同一 fd 上继续：清状态位与异常掩码，丢弃已缓冲但未消费的输入，重新附接
     * 设备并重新初始化转换器。`stdin` 是普通文件时也不会回到开头。
     *
     * 供需要放弃残余输入的场合使用——例如交互程序在出错后丢掉这一行剩下的内容重新提示。
     * 它**不是**出错后的必经之路：解码失败后 `clear()` 即可继续，解码器已复位到初始状态、
     * 从坏字节之后对齐读取，`switch_code()` 也随之可用。
     *
     * 复位的范围只有状态位、异常掩码，以及缓冲与转换器的内部状态。格式状态（含 `skipws`）、
     * `width()`、`precision()`、`fill()`、locale、`sync_with_stdio()`、`tie()` 与
     * `switch_code()` 选定的编码都**保持原样**（`std::basic_ios::clear` 同样不动格式状态）；
     * 拿它在单元测试用例之间复位时要留意这一点：上个用例留下的 `noskipws` 不会被清掉。
     *
     * @note 换设备本身失败（`cvtfailbit` / `otherfailbit`，例如宽流重建编码转换状态失败）时转换器
     *       没有初始化完，流不可用，须再次 `reset()`，`clear()` 不够。丢弃缓冲这一步在 fd 0 上不会
     *       失败（stdin 不可定位，重定位那步的异常在 `streambuf::detach` 里就被有意吞掉了），置
     *       `devfailbit` 的那条路在这里走不到；形状与 `stdout_api::reset()` 保持一致。
     * @endif
     * @lang{EN}
     * @brief Carries on on the same fd: clears the state bits and the exception mask,
     * drops input that was buffered but not yet consumed, reattaches the device and
     * re-initializes the converter. A `stdin` that is a regular file does not rewind.
     *
     * For the cases that want to abandon the pending input -- an interactive program
     * discarding the rest of a line after an error before prompting again. It is **not**
     * the required step after a failure: after a decode failure `clear()` is enough to
     * carry on -- the decoder has reset to its initial state and reads on, aligned, from
     * the byte after the bad one, and `switch_code()` is available again as well.
     *
     * What is reset is the state bits, the exception mask, and the internal state of the
     * buffer and the converter -- nothing else. The format flags (`skipws` among them),
     * `width()`, `precision()`, `fill()`, the locale, `sync_with_stdio()`, `tie()` and the
     * encoding chosen with `switch_code()` all **stay as they are**
     * (`std::basic_ios::clear` likewise leaves the format state alone); a unit test
     * resetting between cases has to keep that in mind, as the `noskipws` left behind by
     * the previous case is still set.
     *
     * @note When replacing the device itself fails (`cvtfailbit` / `otherfailbit`, e.g. a
     *       wide stream unable to rebuild its conversion state) the converter is left
     *       uninitialized, the stream is unusable, and another `reset()` is required --
     *       `clear()` is not enough. Dropping the buffer cannot fail on fd 0 (stdin is not
     *       positionable, and the exception from that reposition is swallowed on purpose in
     *       `streambuf::detach`), so the `devfailbit` path is unreachable here; the shape is
     *       kept identical to `stdout_api::reset()`.
     * @endif
     */
    void reset()
    {
        std::lock_guard guard(this->io_mutex());
        this->clear();
        this->exceptions(ios_defs::goodbit);

        auto detached = m_streambuf.detach();

        try { m_streambuf.attach(); }
        catch (...) { this->handle_exception(std::current_exception()); }

        if (detached.second) this->handle_exception(detached.second);
    }

    /**
     * @lang{ZH}
     * @brief 本流当前把字节解码成 `wchar_t` 时使用的编码（locale）名。
     *
     * 返回的是转换器**实际持有**的那个 locale 自己报出的名字，不是当初传进去的实参：
     * 以 `""` 切换过的话，得到的是那一刻查环境得到的具体名字，不是 `""`；别名也按平台
     * 规范化（glibc 把 `"POSIX"` 报成 `"C"`）。不做 I/O。返回值可再交给 `switch_code()`
     * 得到同一个编码，`sync_with_stdio()` 重建 streambuf 时正是用它来复原编码。
     *
     * @return 当前编码名。
     * @endif
     *
     * @lang{EN}
     * @brief The name of the encoding (locale) this stream currently uses to decode bytes
     * into `wchar_t`.
     *
     * The name the locale the converter **actually holds** reports for itself, not the
     * argument it was given: after a switch to `""` this is the concrete name the environment
     * resolved to at that moment, not `""`, and aliases come back platform-normalized (glibc
     * reports `"POSIX"` as `"C"`). Does no I/O. The result can be handed back to
     * `switch_code()` to get the same encoding, and it is what `sync_with_stdio()` uses to
     * carry the encoding over when it rebuilds the streambuf.
     *
     * @return The current encoding name.
     * @endif
     */
    std::string code() const
        requires std::is_same_v<TChar, wchar_t>
    {
        std::lock_guard guard(this->io_mutex());
        code_cvt_access acc;
        m_streambuf.retrieve(acc);
        return acc.code;
    }

    /**
     * @lang{ZH}
     * @brief 切换本流把字节解码成 `wchar_t` 时使用的编码（locale）。
     *
     * `new_code` 与 `code()` 相同时什么也不做。与本流其余操作不同，失败**不落状态位，而是
     * 抛出**：`code_cvt_stdio::adjust` 把所有可能抛出的步骤都放在提交之前，因此抛出时编码、
     * 状态位、已缓冲的字节都没有改变（强保证）。解码失败之后不必先 `reset()`：`clear()` 即可
     * 让解码器回到初始状态，`switch_code()` 也随之可用。
     *
     * @param new_code 新的编码名，须为 `newlocale()` 接受的 locale 名。`""` 按 POSIX 规则
     *        查环境（`LC_ALL` > `LC_CTYPE` > `LANG` > `"C"`）：查在此刻发生，切换成功后
     *        `code()` 报的是查到的具体名字，不是 `""`。
     * @return 调用前的编码名。
     * @throws cvt_error 该名字不被 `newlocale()` 接受，或编码转换状态不处于初始状态；两种情况
     *         都不切换。详见 `cvt/code_cvt_stdio.h`。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the encoding (locale) this stream uses to decode bytes into `wchar_t`.
     *
     * Does nothing when `new_code` equals `code()`. Unlike the stream's other operations, a
     * failure **throws instead of setting a state bit**: `code_cvt_stdio::adjust` puts every
     * potentially-throwing step before the commit, so on a throw the encoding, the state bits
     * and the buffered bytes are all unchanged (the strong guarantee). A decode failure needs
     * no `reset()` first: `clear()` already puts the decoder back in its initial state, and
     * `switch_code()` is available again with it.
     *
     * @param new_code The new encoding name; must be a locale name `newlocale()` accepts.
     *        `""` means "look at the environment" per POSIX (`LC_ALL` > `LC_CTYPE` > `LANG` >
     *        `"C"`); the lookup happens at this moment, and once the switch succeeds `code()`
     *        reports the concrete name it resolved to, not `""`.
     * @return The encoding name before the call.
     * @throws cvt_error The name is not accepted by `newlocale()`, or the encoding conversion
     *         state is not in its initial state; neither case switches. See
     *         `cvt/code_cvt_stdio.h`.
     * @endif
     */
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
inline cin_t&      cin = _cin_init.get();
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
inline wcin_t&      wcin = _wcin_init.get();
#endif
}

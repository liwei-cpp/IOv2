// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file in_impl.h
 * @lang{ZH}
 * 定义标准输入流的实现模板 `stdin_api`，以及两个标准输入流对象 `cin`（`char`）与 `wcin`
 * （`wchar_t`）。
 *
 * `stdin_api` 不由 `istream` 派生，而是同样把一条 `ichannel`（其下依次是转换器管线与固定 fd
 * 的设备 `std_device<STDIN_FILENO>`）与一个 `locale` 组合起来，对外接口来自 `ios_state`（状态位
 * 与异常掩码）、`istream_operators`（输入操作）与 `stream_common_operators`（`tell()` /
 * `locale()` 等）三个基类。与 `istream` 的差别都来自「设备是固定 fd 的进程级单例」：多了
 * `sync_with_stdio()` / `synced_with_stdio()`、`reset()` 与（宽流）`code()` / `switch_code()`，换设备的
 * `detach()` / `attach()` 则被 `= delete`。
 *
 * 两个流对象经 `sing_temp` 成为进程级单例，退出钩子为空——与 `std::cin` 一样，退出时不析构，
 * 退出阶段既有引用仍然有效。构造时各自 `tie()` 到同字符类型的输出流（`cin` → `cout`，
 * `wcin` → `wcout`）。
 *
 * @note 「退出阶段仍可用」只覆盖本库自己的流对象、locale、facet 与 facet 触到的进程级数据；
 *       用户经 `locale(loc)` 装进来的自定义 facet 若持有会在退出时析构的全局量，不在此列。
 *       详见 `out_impl.h` 的同名说明。
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
 * `stdin_api` does not derive from `istream`; it combines, in the same way, an `ichannel`
 * (below which sit the converter pipeline and the fixed-fd device `std_device<STDIN_FILENO>`)
 * with a `locale`, and takes its interface from three bases -- `ios_state` (the state bits and
 * the exception mask), `istream_operators` (the input operations) and
 * `stream_common_operators` (`tell()` / `locale()` and friends). Every difference from
 * `istream` follows from the device being a fixed fd owned by a process-wide singleton: it
 * adds `sync_with_stdio()` / `synced_with_stdio()`, `reset()` and, on the wide stream, `code()` /
 * `switch_code()`, while `detach()` / `attach()`, which would replace the device, are `= delete`.
 *
 * Both stream objects are process-wide singletons through `sing_temp` with an empty exit
 * hook -- like `std::cin` they are not destroyed at exit, so existing references stay valid
 * once exit begins. Each ties itself at construction to the output stream of the same
 * character type (`cin` to `cout`, `wcin` to `wcout`).
 *
 * @note "Still usable while the process exits" covers this library's own stream objects,
 *       locales, facets and the process-wide data those facets reach; a user facet installed
 *       through `locale(loc)` that holds a global with a destructor is outside that. See the
 *       note of the same name in `out_impl.h`.
 *
 * @note Prefer including `IOv2/io/objects/objects.h` over this header: the entry point also
 *       brings the `sync_with_stdio()` that switches all eight standard streams at once and
 *       the `ws` / `endl` manipulators, and documents what this family of headers does not
 *       bring in.
 * @endif
 */
#pragma once
#include <IOv2/common/copyable_atomic.h>
#include <IOv2/common/iov2_export.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/common/sing_temp.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/iochannel.h>
#include <IOv2/io/iochannel_iterator.h>
#include <IOv2/io/utilities/istream_operators.h>
#include <IOv2/io/utilities/stream_common_operators.h>
#include <IOv2/locale/locale.h>

#include <clocale>
#include <exception>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

#include <unistd.h>

namespace IOv2
{
template <typename T, io_device TDevice, typename TChar>
    requires std::is_same_v<TDevice, std_device<STDIN_FILENO>>
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
    using in_iter_type = ichannel_iterator<ichannel<device_type, char_type>>;
    friend in_sentry_type;

public:
    stdin_api()
        : m_channel(device_type{}, false)
    {}

    template <cvt_creator TCreator>
    stdin_api(const TCreator& creator)
        : m_channel(device_type{}, creator, false)
    {}

public:
    /**
     * @lang{ZH}
     * @brief 切换本流是否与 C stdio 同步：同步时逐字节读 `stdin`，不同步时自带读缓冲。
     *
     * 切换意味着换掉整个 iochannel（先 `detach()` 旧的，再以同一设备重建）。已缓冲但未消费的
     * 输入按 `io/iochannel.h` 的 `detach()` 契约丢弃；因此应在任何 stdin 读取之前调用，也**不得**在
     * 一次提取进行中（例如用户 `io_traits::sread` 里）重入调用：`io_mutex()` 是递归锁不会拦，
     * 但正在使用的 iochannel 会被整个换掉。
     *
     * @warning 这里的「同步」只表示**不带读缓冲、每次 `read(0)` 只要本次操作所需的字节**
     *          （格式化提取与 `get` / `getline` 因逐字符探分隔符而逐字节，`read(buf, n)` 则是
     *          一次 `read(0, buf, n)`），不是与 C stdio 共享缓冲：本流的设备
     *          直接用 POSIX `read()`，绕过 `stdin` 的 `FILE` 缓冲（见 `device/std_device.h`
     *          的类级 `@warning`）。因此与 `std::cin` 不同：(1) 格式化提取探到的分隔符留在本流
     *          的读缓冲里，`getchar()` / `fgets()` 看不到它、拿到的是它之后的字节，而
     *          `cin.get()` 能把它取回；(2) 反过来，终端上一次 `getchar()` 会让 stdio 读走整行，
     *          本流随后只能看见下一行；(3) `cin` 与 `wcin` 也各有自己的读缓冲，一方探过的字节
     *          另一方看不到。三种形态都**不丢字节**，只是跨边界不可见。请勿在 `stdin` 上混用
     *          本流与 C stdio 函数或另一条标准输入流。
     *
     * 查询当前状态请用 `synced_with_stdio()`：本函数的无参形式等于 `sync_with_stdio(true)`，
     * 会真的切换——在输入流上「调一次查、再调一次设回去」等于重建两次 iochannel，已缓冲的
     * 输入随之丢失。
     *
     * 失败按本库统一的方式报告：置状态位，`exceptions()` 掩码含该位时才抛出。重建 iochannel
     * 只可能因内存耗尽或（`wchar_t`）当前编码的 locale 数据库在运行期间消失而失败——都是运行
     * 环境已坏的情形，实际不可达。
     *
     * @param sync `true` 为同步（默认），`false` 为自带缓冲。
     * @return 调用前的同步状态；重建失败时同步状态未改变，返回的就是当前状态。
     * @note 重建失败时旧 iochannel 已经 detach、新的没建起来，流停在**未附接**状态：此后每次
     *       要读设备的操作都按状态位失败（`clear()` 之后是 `cvtfailbit`；`putback()`、`code()`、
     *       `switch_code()` 不碰设备，照常成功），`clear()` 不够，须 `reset()`
     *       在同一 fd 上重新附接。同步标志保持原值，因此「流报告的模式」与「它实际怎么读」始终一致。
     *       本函数不像别的失败那样只是「这一次没做成」，而是会让流暂时不可用，故值得单独提醒。
     * @endif
     *
     * @lang{EN}
     * @brief Switches whether this stream is synchronized with C stdio: synchronized
     * reads `stdin` byte by byte, unsynchronized reads through its own buffer.
     *
     * Switching replaces the whole iochannel (`detach()` the old one, rebuild on the
     * same device). Input that was buffered but not yet consumed is discarded per the
     * `detach()` contract in `io/iochannel.h`; call this before any stdin read, and **never**
     * re-enter it from inside an extraction (a user `io_traits::sread`, say): `io_mutex()`
     * is recursive and will not stop it, but the iochannel in use is replaced wholesale.
     *
     * @warning "Synchronized" here means **no read buffer: each `read(0)` asks for just what
     *          the current operation needs** (formatted extraction and `get` / `getline` go
     *          byte by byte because they probe for the delimiter one character at a time;
     *          `read(buf, n)` is a single `read(0, buf, n)`), not sharing a buffer with C
     *          stdio: this stream's device calls POSIX `read()`
     *          directly and bypasses the `FILE` buffer of `stdin` (see the class-level
     *          `@warning` in `device/std_device.h`). Unlike `std::cin`, therefore: (1) the
     *          delimiter a formatted extraction peeks at stays in this stream's read buffer,
     *          where `getchar()` / `fgets()` never see it -- they get the byte after it --
     *          while `cin.get()` returns it; (2) conversely, on a terminal one `getchar()`
     *          lets stdio read the whole line, after which this stream only sees the next
     *          one; (3) `cin` and `wcin` have separate read buffers as well, so a byte one of
     *          them peeked at is invisible to the other. **No byte is lost** in any of the
     *          three; they are only invisible across the boundary. Do not mix this stream
     *          with C stdio functions, or with the other standard input stream, on `stdin`.
     *
     * To ask for the current state use `synced_with_stdio()`: with no argument this one means
     * `sync_with_stdio(true)` and does switch -- on an input stream, "call once to read it,
     * call again to put it back" rebuilds the iochannel twice and loses whatever it had
     * buffered.
     *
     * A failure is reported the way this library reports every other one: a state bit is
     * set, and it throws only when the `exceptions()` mask includes that bit. Rebuilding the
     * iochannel can only fail on memory exhaustion or, on `wchar_t`, when the locale database
     * of the current code vanished while the process runs -- a broken runtime environment,
     * unreachable in practice.
     *
     * @param sync `true` for synchronized (the default), `false` for own buffering.
     * @return The synchronization state before the call; when the rebuild fails the state is
     *         unchanged, so that is also the current one.
     * @note When the rebuild fails the old iochannel has been detached and the new one was
     *       never built, leaving the stream **unattached**: every operation that reads the
     *       device then fails through the state bits (`cvtfailbit` once `clear()`ed;
     *       `putback()`, `code()` and `switch_code()` do not touch the device and succeed as
     *       usual), `clear()` is not enough, and
     *       `reset()` is what attaches a fresh device on the same fd. The flag keeps its old
     *       value, so what the stream reports and how it actually reads never disagree.
     *       Unlike most failures this one leaves the stream unusable for a while, which is why
     *       it is called out here.
     * @endif
     */
    bool sync_with_stdio(bool sync = true)
    {
        std::lock_guard guard(this->io_mutex());
        auto old_sync_state = m_sync_with_stdio.load();
        if (old_sync_state == sync)
            return old_sync_state;

        auto [dev, err] = m_channel.detach();
        try {
            if constexpr (std::is_same_v<char_type, char>)
                m_channel = ichannel<device_type, char_type>(std::move(dev), !sync);
            else if constexpr (std::is_same_v<char_type, wchar_t>)
            {
                // Straight from the iochannel, not through code(): that wrapper reports a
                // failure as a state bit and an empty name, which would rebuild on the
                // environment's encoding instead of the current one.
                code_cvt_access acc;
                m_channel.retrieve(acc);
                m_channel = ichannel<device_type, wchar_t>(std::move(dev), code_cvt_stdio_creator(acc.code), !sync);
            }
            else
                static_assert(dependent_false_v<char_type>, "invalid character type");
        } catch (...) {
            // The iochannel was detached and the new one was never built: every
            // operation that reads the device now fails until reset() attaches a
            // fresh device. The flag stays where it was, so what the stream reports
            // and how it actually reads still agree.
            this->handle_exception(std::current_exception());
            return old_sync_state;
        }
        m_sync_with_stdio.store(sync);
        if (err) this->handle_exception(err);
        return old_sync_state;
    }

    /**
     * @lang{ZH}
     * @brief 查询本流当前是否与 C stdio 同步。
     *
     * 纯读，不切换任何东西——`sync_with_stdio()` 不是 getter，它的无参形式等于
     * `sync_with_stdio(true)`。
     *
     * @return 当前的同步状态。
     * @endif
     *
     * @lang{EN}
     * @brief Asks whether this stream is currently synchronized with C stdio.
     *
     * A pure read that switches nothing -- `sync_with_stdio()` is not a getter; with no
     * argument it means `sync_with_stdio(true)`.
     *
     * @return The current synchronization state.
     * @endif
     */
    [[nodiscard]] bool synced_with_stdio() const noexcept
    {
        return m_sync_with_stdio.load();
    }

    /**
     * @lang{ZH}
     * `stream_common_operators` 的换设备接口在标准流上删除：本流的设备是固定的 fd 0，取出去
     * 就再也装不回来，换进去等于给一个进程级单例改写底层来源。需要「在同一 fd 上从头开始」
     * 请用 `reset()`（它走的是 iochannel 那一层的 `attach()`，装一个同 fd 的缺省设备）。
     * @endif
     *
     * @lang{EN}
     * The device-replacing interface of `stream_common_operators` is deleted on the standard
     * streams: this stream's device is the fixed fd 0, taking it out leaves no way to put it
     * back, and putting another one in rewrites the source of a process-wide singleton. To
     * start over on the same fd use `reset()`, which goes through the `attach()` one layer
     * down, in the iochannel, with a default device on the same fd.
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
     * 从坏字节之后对齐读取，`switch_code()` 也随之可用。与 `sync_with_stdio()` 一样，不要在
     * 一次提取进行中（用户 `io_traits::sread` 里）重入调用：不会崩，但本次提取之后已缓冲的
     * 输入随之丢弃。
     *
     * 复位的范围只有状态位、异常掩码，以及缓冲与转换器的内部状态。格式状态（含 `skipws`）、
     * `width()`、`precision()`、`fill()`、locale、`sync_with_stdio()`、`tie()` 与
     * `switch_code()` 选定的编码都**保持原样**（`std::basic_ios::clear` 同样不动格式状态）；
     * 拿它在单元测试用例之间复位时要留意这一点：上个用例留下的 `noskipws` 不会被清掉。
     *
     * @note 重新附接这一步在标准流上**没有可失败的操作**：装的是同一 fd 的缺省设备，不分配、
     *       不做 I/O、不重建 locale（转换状态的「重建」只是 `mbstate_t` 复位）——实测五个标准流在
     *       全部分配失败且 fd 指向 `/dev/full` 时 `reset()` 均 0 次分配、状态位全 0。围住它的
     *       `handle_exception` 只是兜底：若将来这里真抛了什么，转换器会停在未初始化状态，流不可用，
     *       须再次 `reset()`，`clear()` 不够。丢弃缓冲这一步在 fd 0 上同样不会失败（stdin 不可定位，
     *       重定位那步的异常在 `iochannel::detach` 里就被有意吞掉了），置 `devfailbit` 的那条路在这里
     *       走不到；除输出侧多一步旧设备 `dflush()` 外，形状与 `stdout_api::reset()` 一致。
     * @endif
     *
     * @lang{EN}
     * @brief Carries on on the same fd: clears the state bits and the exception mask,
     * drops input that was buffered but not yet consumed, reattaches the device and
     * re-initializes the converter. A `stdin` that is a regular file does not rewind.
     *
     * For the cases that want to abandon the pending input -- an interactive program
     * discarding the rest of a line after an error before prompting again. It is **not**
     * the required step after a failure: after a decode failure `clear()` is enough to
     * carry on -- the decoder has reset to its initial state and reads on, aligned, from
     * the byte after the bad one, and `switch_code()` is available again as well. As with
     * `sync_with_stdio()`, do not re-enter it from inside an extraction (a user
     * `io_traits::sread`): nothing crashes, but the input buffered beyond that extraction
     * is discarded with it.
     *
     * What is reset is the state bits, the exception mask, and the internal state of the
     * buffer and the converter -- nothing else. The format flags (`skipws` among them),
     * `width()`, `precision()`, `fill()`, the locale, `sync_with_stdio()`, `tie()` and the
     * encoding chosen with `switch_code()` all **stay as they are**
     * (`std::basic_ios::clear` likewise leaves the format state alone); a unit test
     * resetting between cases has to keep that in mind, as the `noskipws` left behind by
     * the previous case is still set.
     *
     * @note Reattaching has **nothing that can fail** on a standard stream: it installs a
     *       default device on the same fd, allocates nothing, does no I/O and rebuilds no
     *       locale ("rebuilding" the conversion state is an `mbstate_t` reset) -- measured on
     *       all five standard streams with every allocation failing and the fd on
     *       `/dev/full`: zero allocations, no state bit. The `handle_exception` around it is
     *       only a backstop: should something ever throw there, the converter is left
     *       uninitialized, the stream is unusable, and another `reset()` is required --
     *       `clear()` is not enough. Dropping the buffer cannot fail on fd 0 either (stdin is
     *       not positionable, and the exception from that reposition is swallowed on purpose
     *       in `iochannel::detach`), so the `devfailbit` path is unreachable here; apart from
     *       the output side's extra `dflush()` of the old device, the shape is the same as
     *       `stdout_api::reset()`.
     * @endif
     */
    void reset()
    {
        std::lock_guard guard(this->io_mutex());
        this->clear();
        this->exceptions(ios_defs::goodbit);

        auto detached = m_channel.detach();

        try { m_channel.attach(); }
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
     * 得到同一个编码。
     *
     * 只是 `retrieve(code_cvt_access)` 的包装，走本流通用的加锁与错误处理：失败（只在转换器
     * 内核已被移出时发生，正常生命周期不可达）经 `handle_exception` 置 `cvtfailbit`、返回空串，
     * `exceptions()` 掩码含该位时抛出。
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
     * `switch_code()` to get the same encoding.
     *
     * A thin wrapper over `retrieve(code_cvt_access)`, so it shares the stream's locking and
     * error handling: a failure (only when the converter's kernel has been moved out, which a
     * live stream never reaches) goes through `handle_exception`, sets `cvtfailbit` and yields
     * an empty string; it throws when the `exceptions()` mask includes that bit.
     *
     * @return The current encoding name.
     * @endif
     */
    std::string code()
        requires std::is_same_v<TChar, wchar_t>
    {
        code_cvt_access acc;
        this->retrieve(acc);
        return acc.code;
    }

    /**
     * @lang{ZH}
     * @brief 切换本流把字节解码成 `wchar_t` 时使用的编码（locale）。
     *
     * `new_code` 与 `code()` 相同时什么也不做——比较的是 `code()` 报出的**解析后**的名字与实参
     * 本身，所以 `""` 与别名（当前是 `zh_CN.UTF-8` 时给 `zh_CN.utf8`、当前是 `C` 时给 `POSIX`）
     * 都不会命中早退，会真的重建一次（结果相同，代价是一次 `newlocale`）。只是
     * `adjust(code_cvt_switch)` 的包装，走本流
     * 通用的加锁与错误处理：失败按状态位报告，`exceptions()` 掩码含该位时才抛出；失败时编码、
     * 已缓冲的字节都没有改变（`code_cvt_stdio::adjust` 把所有可能失败的步骤都放在提交之前）。
     * 解码失败之后不必先 `reset()`：`clear()` 即可让解码器回到初始状态，`switch_code()` 也随之
     * 可用。
     *
     * @param new_code 新的编码名，须为 `newlocale()` 接受的 locale 名。`""` 按 POSIX 规则
     *        查环境（`LC_ALL` > `LC_CTYPE` > `LANG` > `"C"`）：查在此刻发生，切换成功后
     *        `code()` 报的是查到的具体名字，不是 `""`。
     * @return 调用前的编码名；失败时它仍是当前编码名。判断成败请在进入前保证 `good()`，之后查
     *         `cvt_fail()`；或者直接比较 `code()` 与目标——本函数不设 `good()` 门槛，状态位已置时
     *         照常切换、位不变。
     * @note 置 `cvtfailbit`：该名字不被 `newlocale()` 接受（含内嵌 NUL 的名字按全长拒绝，不在
     *       第一个 NUL 处截断），或编码转换状态不处于初始状态
     *       （例如输入在一个多字节字符中间到达 EOF，此时 `clear()` 不够、须 `reset()`）。
     *       详见 `cvt/code_cvt_stdio.h`。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the encoding (locale) this stream uses to decode bytes into `wchar_t`.
     *
     * Does nothing when `new_code` equals `code()` -- the comparison is between the
     * **resolved** name `code()` reports and the argument itself, so `""` and aliases
     * (`zh_CN.utf8` while the current name is `zh_CN.UTF-8`, `POSIX` while it is `C`) miss the
     * early exit and do rebuild once, to the same result, at the cost of one `newlocale`.
     * A thin wrapper over
     * `adjust(code_cvt_switch)`, so it shares the stream's locking and error handling: a
     * failure is reported through the state bits and throws only when the `exceptions()` mask
     * includes the bit; on failure the encoding and the buffered bytes are unchanged
     * (`code_cvt_stdio::adjust` puts every step that can fail before the commit). A decode
     * failure needs no `reset()` first: `clear()` already puts the decoder back in its initial
     * state, and `switch_code()` is available again with it.
     *
     * @param new_code The new encoding name; must be a locale name `newlocale()` accepts.
     *        `""` means "look at the environment" per POSIX (`LC_ALL` > `LC_CTYPE` > `LANG` >
     *        `"C"`); the lookup happens at this moment, and once the switch succeeds `code()`
     *        reports the concrete name it resolved to, not `""`.
     * @return The encoding name before the call; on failure that is still the current one.
     *         To tell the two apart enter with `good()` and check `cvt_fail()` afterwards, or
     *         compare `code()` with the target: this function has no `good()` gate, so with a
     *         state bit already set it switches as usual and leaves the bits alone.
     * @note Sets `cvtfailbit`: the name is not accepted by `newlocale()` (a name with an
     *       embedded NUL is rejected at its full length, not cut at the first NUL), or the encoding
     *       conversion state is not in its initial state (input that hit EOF in the middle of
     *       a multibyte character, say -- there `clear()` is not enough and `reset()` is).
     *       See `cvt/code_cvt_stdio.h`.
     * @endif
     */
    std::string switch_code(const std::string& new_code)
        requires std::is_same_v<TChar, wchar_t>
    {
        auto res = code();
        if (res != new_code)
        {
            code_cvt_switch acc(new_code);
            this->adjust(acc);
        }
        return res;
    }

protected:
    ichannel<device_type, char_type>      m_channel;
    IOv2::locale<char_type>                 m_locale;
    copyable_atomic<bool> m_sync_with_stdio{true};   ///< @lang{ZH} 为 true 时逐字节读 `stdin`，为 false 时自带读缓冲；该语义在构造 iochannel 时就固化进 kernel 类型，故除本标志的读写外无人查询它。写入在 `io_mutex()` 之下，原子量只为让 `synced_with_stdio()` 与全库其它查询函数一样无锁读取。 @endif @lang{EN} When true this stream reads `stdin` byte by byte, when false through its own buffer; that semantics is baked into the kernel type when the iochannel is built, so nothing but this flag's own reads and writes consults it. Writes happen under `io_mutex()`; the atomic is only so that `synced_with_stdio()` reads lock-free like the library's other query functions. @endif
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

// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file stdin_root_cvt.h
 * @lang{ZH}
 * 标准输入流的根转换器（`stdin_root_cvt`）及切换它的行为策略（`stdin_sync`）。
 * @endif
 *
 * @lang{EN}
 * The root converter of the standard input streams (`stdin_root_cvt`) and the behavior
 * policy that switches it (`stdin_sync`).
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/cvt/abs_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/device/std_device.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <utility>
#include <vector>

namespace IOv2
{
/**
 * @lang{ZH}
 * `stdin_root_cvt` 的行为策略：切换它是否与 C stdio 同步。
 * @endif
 *
 * @lang{EN}
 * Behavior policy for `stdin_root_cvt`: switches whether it is synchronized with C stdio.
 * @endif
 */
struct stdin_sync final : cvt_behavior
{
    explicit stdin_sync(bool s) noexcept : synced(s) {}
    bool synced; ///< 为 `true` 时同步 / Synchronized when `true`.
};

/**
 * @lang{ZH}
 * 标准输入流的根转换器：建在 fd 0 的 `std_input_device` 上，带读缓冲，同步与否是运行时的开关。
 *
 * 不同步时，需要向设备补字节就把缓冲能装的都读进来，设备直接读 fd 0。同步时只向设备要
 * 调用方还缺的那几个字节，不往前多读——逐字符读就是逐字节读设备；设备同时切到 stdio 模式
 * （`std_device::stdio_mode()`），经 `stdin` 的 `FILE` 读，与 C stdio 共用缓冲。切到同步时
 * 缓冲里已预读的字节照常先交出，交完才按需读，所以切换不丢输入。经 `adjust(stdin_sync{...})`
 * 切换；`attach()` 装入的新设备（`reset()` 与转换器自动恢复装的是缺省构造的设备）按当前模式
 * 设置。
 *
 * 上层转换器经 `cvt_reader` 回退尚未解码的字节时（如 `wcin` 读到输入末尾只剩半个多字节字符），
 * 缓冲里剩下的字节整体经 `std_device::dunget()` 还给设备，同步时 C stdio 随后也读得到；设备
 * 拒绝时（fd 模式，或 Windows），字节照旧留在缓冲里。
 *
 * 只用于输入：没有写、定位与换向，`bos()` 总是输入方向。不派生自 `root_cvt`。
 * @endif
 *
 * @lang{EN}
 * The root converter of the standard input streams: built on the `std_input_device` on fd 0,
 * with a read buffer, and whether it is synchronized is a runtime switch.
 *
 * Unsynchronized, it reads as much as the buffer can take whenever it has to go to the
 * device, and the device reads fd 0 directly. Synchronized, it asks the device only for the
 * bytes the caller is still short of and reads nothing ahead -- a character-at-a-time read
 * reads the device a byte at a time; the device switches to stdio mode along with it
 * (`std_device::stdio_mode()`), reading through `stdin`'s `FILE` and sharing its buffer with
 * C stdio. Bytes already read ahead when it switches to synchronized are handed out first as
 * usual, and only then is reading on demand, so a switch loses no input. The mode is
 * switched through `adjust(stdin_sync{...})`; a device installed by `attach()` (`reset()`
 * and a converter's automatic recovery install a default-constructed one) is set to the
 * current mode.
 *
 * When a converter above rolls back bytes it has not decoded through `cvt_reader` (`wcin`
 * left with half a multibyte character at the end of the input, say), what is left in the
 * buffer goes back to the device as a whole through `std_device::dunget()`, so that C stdio
 * reads it next when synchronized; if the device refuses (fd mode, or Windows), the bytes
 * stay in the buffer as before.
 *
 * Input only: no writing, positioning or switching, and `bos()` always settles on input. It
 * does not derive from `root_cvt`.
 * @endif
 */
class stdin_root_cvt
{
    template <io_converter KernelType>
    friend class cvt_reader;

public:
    /// @lang{ZH} 读缓冲的字节容量。 @endif @lang{EN} Byte capacity of the read buffer. @endif
    constexpr static std::size_t s_buffer_length = 2048;

    using device_type = std_input_device;
    using internal_type = char;
    using external_type = char;

    /**
     * @lang{ZH}
     * 接管设备，并把它设成 `synced` 对应的读法。
     * @endif
     *
     * @lang{EN}
     * Takes over the device and sets it to the read mode `synced` stands for.
     * @endif
     */
    explicit stdin_root_cvt(device_type dev, bool synced)
        : m_device(std::move(dev))
        , m_buffer(s_buffer_length)
        , m_synced(synced)
    {
        m_device.stdio_mode(synced);
    }

    /// @lang{ZH} 移动后，源对象只能析构或被赋值。 @endif @lang{EN} A moved-from object may only be destroyed or assigned to. @endif
    stdin_root_cvt(stdin_root_cvt&&) noexcept = default;
    stdin_root_cvt& operator=(stdin_root_cvt&&) noexcept = default;
    stdin_root_cvt(const stdin_root_cvt&) = delete;
    stdin_root_cvt& operator=(const stdin_root_cvt&) = delete;
    ~stdin_root_cvt() = default;

    device_type& device() noexcept { return m_device; }

    /**
     * @lang{ZH}
     * 交出设备。stdin 不可定位，已读进缓冲、尚未交出的字节随缓冲丢弃。
     * @endif
     *
     * @lang{EN}
     * Hands the device back. stdin cannot be repositioned, so bytes read into the buffer and
     * not yet handed out are dropped with it.
     * @endif
     */
    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        m_cur = m_end = 0;
        return {std::move(m_device), nullptr};
    }

    /**
     * @lang{ZH}
     * 装入新设备，先把它设成当前的同步模式；缓冲清空。
     * @endif
     *
     * @lang{EN}
     * Installs a new device, set to the current synchronization mode first; the buffer is
     * emptied.
     * @endif
     */
    void attach(device_type&& dev = device_type{})
    {
        dev.stdio_mode(m_synced);
        m_device = std::move(dev);
        m_buffer.resize(s_buffer_length);
        m_cur = m_end = 0;
    }

    /// @lang{ZH} 总是输入方向。 @endif @lang{EN} Always input. @endif
    io_status bos() noexcept { return io_status::input; }

    /// @lang{ZH} stdin 不可定位，没有要记下的流首位置。 @endif @lang{EN} stdin cannot be repositioned, so there is no stream origin to record. @endif
    void main_cont_beg() noexcept {}

    /**
     * @lang{ZH}
     * `stdin_sync` 切换同步与否，只翻标志与设备的读法，不碰缓冲；其余行为策略不起作用。
     * @endif
     *
     * @lang{EN}
     * A `stdin_sync` switches synchronization by flipping the flag and the device's read mode
     * alone, leaving the buffer as it is; other behavior policies have no effect.
     * @endif
     */
    void adjust(const cvt_behavior& b)
    {
        if (const auto* s = dynamic_cast<const stdin_sync*>(&b); s)
        {
            m_synced = s->synced;
            m_device.stdio_mode(m_synced);
        }
    }

    void retrieve(cvt_status&) const {}

    /// @lang{ZH} 缓冲里还有字节就不是末尾，否则问设备。 @endif @lang{EN} Not at the end while the buffer holds bytes; otherwise the device decides. @endif
    bool is_eof()
    {
        if (m_cur != m_end) return false;
        return m_device.deof();
    }

    /**
     * @lang{ZH}
     * 读取至多 `to_max` 个字节。缓冲里有字节（不同步时预读的，或回退时设备没收回的）就只交出
     * 这些，不再向设备要，可能短读。缓冲空了才去读设备：同步时不预读，直接读进 `to`，至多
     * `to_max` 个，不经缓冲；不同步时先补满整个缓冲，再从中交出。
     * @endif
     *
     * @lang{EN}
     * Reads up to `to_max` bytes. While the buffer holds bytes (read ahead unsynchronized, or
     * rolled back and refused by the device) it hands out only those and does not go to the
     * device, so the read may be short. Only with the buffer empty does it read the device:
     * synchronized, nothing is read ahead, so it reads up to `to_max` bytes straight into
     * `to`, bypassing the buffer; unsynchronized, it fills the whole buffer first and hands
     * out from there.
     * @endif
     */
    std::size_t get(internal_type* to, std::size_t to_max)
    {
        if (m_cur == m_end)
        {
            if (m_synced) return m_device.dget(to, to_max);
            m_end = m_device.dget(m_buffer.data(), s_buffer_length);
            m_cur = 0;
        }
        const std::size_t res = std::min(m_end - m_cur, to_max);
        std::copy_n(m_buffer.data() + m_cur, res, to);
        m_cur += res;
        return res;
    }

private:
    device_type m_device;
    std::vector<char> m_buffer;    ///< @lang{ZH} 读缓冲；未交出的字节在 `[m_cur, m_end)`。 @endif @lang{EN} The read buffer; bytes not yet handed out are in `[m_cur, m_end)`. @endif
    std::size_t m_cur = 0;
    std::size_t m_end = 0;
    bool m_synced;                 ///< @lang{ZH} 为 `true` 时同步：不预读，设备为 stdio 模式。 @endif @lang{EN} Synchronized when `true`: nothing read ahead, the device in stdio mode. @endif
};

/**
 * @lang{ZH}
 * `stdin_root_cvt` 的 `cvt_reader`：直接在根的缓冲里读，按同步与否决定向设备要多少，并且
 * 回退时把缓冲里剩下的字节还给设备。
 * @endif
 *
 * @lang{EN}
 * The `cvt_reader` of `stdin_root_cvt`: it reads in the root's own buffer, asks the device
 * for as much as the synchronization calls for, and on a rollback hands what is left in the
 * buffer back to the device.
 * @endif
 */
template <>
class cvt_reader<stdin_root_cvt>
{
public:
    explicit cvt_reader(stdin_root_cvt& kernel, std::vector<char>&, std::size_t&, std::size_t&)
        : m_kernel(kernel) {}

    cvt_reader(const cvt_reader&) = delete;
    cvt_reader& operator=(const cvt_reader&) = delete;
    cvt_reader(cvt_reader&&) = delete;
    cvt_reader& operator=(cvt_reader&&) = delete;
    ~cvt_reader() = default;

    /// @lang{ZH} 空操作：缓冲由根管理。 @endif @lang{EN} No-op: the root manages the buffer. @endif
    void reset(std::size_t) {}

    /**
     * @lang{ZH}
     * 取 `to_max` 个字节，缓冲里不够时向设备补：剩下的字节先移到开头，不同步时补满缓冲，
     * 同步时只补还缺的。字节留在根的缓冲里，供调用方原地解码、必要时回退。`Saturate` 为
     * `false` 时返回（指针，实际数量），可能少于 `to_max`；为 `true` 时要求恰好 `to_max` 个，
     * 返回指针，读满之前到达末尾则抛出。
     * @endif
     *
     * @lang{EN}
     * Takes `to_max` bytes, refilling from the device when the buffer falls short: what is
     * left moves to the front, and the refill fills the buffer unsynchronized and covers only
     * the shortfall synchronized. The bytes stay in the root's buffer, for the caller to
     * decode in place and roll back if need be. With `Saturate` false it returns (pointer,
     * count), possibly fewer than `to_max`; with `Saturate` true it requires exactly
     * `to_max`, returns the pointer, and throws if the end comes first.
     * @endif
     * @throws cvt_error @lang{ZH} `to_max` 为 0 或超过 `s_buffer_length`；`Saturate` 时读满前到达末尾。 @endif
     *                   @lang{EN} `to_max` is 0 or exceeds `s_buffer_length`; with `Saturate`, the end comes first. @endif
     */
    template <bool Saturate = false>
    auto get_buf(std::size_t to_max)
    {
        if (to_max == 0)
            throw cvt_error("cvt_reader::get_buf fail, read size cannot be zero");
        if (to_max > stdin_root_cvt::s_buffer_length)
            throw cvt_error("cvt_reader::get_buf fail, read size too large");

        auto& k = m_kernel;
        char* const buf = k.m_buffer.data();
        const std::size_t remain = k.m_end - k.m_cur;
        if (remain < to_max)
        {
            std::copy(buf + k.m_cur, buf + k.m_end, buf);
            k.m_cur = 0;
            k.m_end = remain;
            const std::size_t room = stdin_root_cvt::s_buffer_length - remain;
            const std::size_t want = k.m_synced ? std::min(to_max - remain, room) : room;
            k.m_end += k.m_device.dget(buf + k.m_end, want);
        }

        if constexpr (Saturate)
        {
            while (k.m_end < k.m_cur + to_max)
            {
                const std::size_t got = k.m_device.dget(buf + k.m_end, k.m_cur + to_max - k.m_end);
                if (got == 0)
                    throw cvt_error("get_buf<Saturate> fail: meet eos");
                k.m_end += got;
            }
            const char* res = buf + k.m_cur;
            k.m_cur += to_max;
            return res;
        }
        else
        {
            const std::size_t len = std::min(k.m_end - k.m_cur, to_max);
            std::pair<const char*, std::size_t> res{buf + k.m_cur, len};
            k.m_cur += len;
            return res;
        }
    }

    /**
     * @lang{ZH}
     * 把读取游标后退 `len` 个字节，再把缓冲里剩下的全部字节经 `dunget()` 整体还给设备——
     * 只还回退的那几个，排在它们后面的字节就会先被读到。设备拒绝时（fd 模式，或 Windows）
     * 字节留在缓冲里，下一次读取照旧先交出它们。
     * @endif
     *
     * @lang{EN}
     * Moves the read cursor back by `len` bytes, then hands everything left in the buffer back
     * to the device through `dunget()` as a whole -- handing back only the rolled-back bytes
     * would let the bytes behind them be read first. If the device refuses (fd mode, or
     * Windows), the bytes stay in the buffer and the next read hands them out first as before.
     * @endif
     * @throws cvt_error @lang{ZH} `len` 为 0 或超过已交出的字节数。 @endif
     *                   @lang{EN} `len` is 0 or exceeds the bytes handed out. @endif
     */
    void rollback(std::size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_reader::rollback fail, length cannot be zero");
        auto& k = m_kernel;
        if (len > k.m_cur)
            throw cvt_error("cvt_reader::rollback fail, rollback length too large");

        k.m_cur -= len;
        if (k.m_device.dunget(k.m_buffer.data() + k.m_cur, k.m_end - k.m_cur))
            k.m_cur = k.m_end = 0;
    }

private:
    stdin_root_cvt& m_kernel;
};
}

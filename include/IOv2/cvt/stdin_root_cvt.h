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
#include <IOv2/common/iov2_export.h>
#include <IOv2/common/sing_temp.h>
#include <IOv2/cvt/abs_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/device/std_device.h>

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <exception>
#include <iterator>
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

namespace detail
{
/**
 * @lang{ZH}
 * `stdin_root_cvt` 背后真正干活的部分：持有设备、读缓冲与是否同步，并实现全部读取逻辑。
 * 它是单例（`stdin_shared_imp`），`stdin_root_cvt` 只是放进转换器管线里的句柄，把调用转给它；
 * 于是所有标准输入流（`cin`、`wcin`）读同一块缓冲、同一个设备，同步与否也随之一致。
 *
 * 不可拷贝、不可移动，由 `sing_temp` 构造、永不析构。操作由使用它的流的对象锁（`cin`、`wcin`
 * 共用的那一把）串行化；`synced()` 另可不加锁调用，供 `synced_with_stdio()` 查询。
 * @endif
 *
 * @lang{EN}
 * The part that does the work behind `stdin_root_cvt`: it holds the device, the read buffer
 * and whether it is synchronized, and implements all the reading. It is a singleton
 * (`stdin_shared_imp`), and `stdin_root_cvt` is only the handle that goes into a converter
 * pipeline and passes calls on to it; so every standard input stream (`cin`, `wcin`) reads
 * the same buffer and the same device, and they agree on the synchronization as well.
 *
 * Neither copyable nor movable; `sing_temp` builds it and it is never destroyed. The streams
 * using it serialize its operations with their object lock (the one `cin` and `wcin` share);
 * `synced()` may also be called without it, for `synced_with_stdio()`.
 * @endif
 */
class stdin_root_cvt_imp : public sing_temp<stdin_root_cvt_imp>
{
    friend sing_temp<stdin_root_cvt_imp>;

    // Built once by sing_temp, synchronized like a fresh cin, and never destroyed at exit,
    // like cin and wcin, which may still read through it while the program exits.
    stdin_root_cvt_imp()
        : sing_temp<stdin_root_cvt_imp>([](stdin_root_cvt_imp*) noexcept {})
        , m_buffer(s_buffer_length)
        , m_synced(true)
    {
        m_device.stdio_mode(true);
    }

public:
    /// @lang{ZH} 读缓冲的字节容量。 @endif @lang{EN} Byte capacity of the read buffer. @endif
    constexpr static std::size_t s_buffer_length = 2048;

    stdin_root_cvt_imp(const stdin_root_cvt_imp&) = delete;
    stdin_root_cvt_imp& operator=(const stdin_root_cvt_imp&) = delete;
    stdin_root_cvt_imp(stdin_root_cvt_imp&&) = delete;
    stdin_root_cvt_imp& operator=(stdin_root_cvt_imp&&) = delete;
    ~stdin_root_cvt_imp() = default;

    std_input_device& device() noexcept { return m_device; }

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
    std::pair<std_input_device, std::exception_ptr> detach() noexcept
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
    void attach(std_input_device&& dev)
    {
        dev.stdio_mode(synced());
        m_device = std::move(dev);
        m_cur = m_end = 0;
    }

    /// @lang{ZH} 切换同步与否：只翻标志与设备的读法，不碰缓冲。 @endif @lang{EN} Switches synchronization: flips the flag and the device's read mode alone, leaving the buffer as it is. @endif
    void synchronize(bool s)
    {
        m_synced.store(s);
        m_device.stdio_mode(s);
    }

    /// @lang{ZH} 当前是否同步；不必持锁。 @endif @lang{EN} Whether it is synchronized now; no lock needed. @endif
    [[nodiscard]] bool synced() const noexcept { return m_synced.load(); }

    /// @lang{ZH} 缓冲里还有字节就不是末尾，否则问设备。 @endif @lang{EN} Not at the end while the buffer holds bytes; otherwise the device decides. @endif
    bool is_eof()
    {
        if (m_cur != m_end) return false;
        return m_device.deof();
    }

    /**
     * @lang{ZH}
     * 读取至多 `to_max` 个字节。缓冲里有字节（不同步时预读的，或回退、退回时设备没收下的）就只
     * 交出这些，不再向设备要，可能短读。缓冲空了才去读设备：同步时不预读，直接读进 `to`，至多
     * `to_max` 个，不经缓冲；不同步时先补满整个缓冲，再从中交出。
     * @endif
     *
     * @lang{EN}
     * Reads up to `to_max` bytes. While the buffer holds bytes (read ahead unsynchronized, or
     * rolled back or handed back and refused by the device) it hands out only those and does
     * not go to the device, so the read may be short. Only with the buffer empty does it read the device:
     * synchronized, nothing is read ahead, so it reads up to `to_max` bytes straight into
     * `to`, bypassing the buffer; unsynchronized, it fills the whole buffer first and hands
     * out from there.
     * @endif
     */
    std::size_t get(char* to, std::size_t to_max)
    {
        if (m_cur == m_end)
        {
            restore_size();
            if (synced()) return m_device.dget(to, to_max);
            m_end = m_device.dget(m_buffer.data(), s_buffer_length);
            m_cur = 0;
        }
        const std::size_t res = std::min(m_end - m_cur, to_max);
        std::copy_n(m_buffer.data() + m_cur, res, to);
        m_cur += res;
        return res;
    }

    /**
     * @lang{ZH}
     * 供 `cvt_reader` 用：取 `to_max` 个字节，缓冲里不够时向设备补——剩下的字节先移到开头，
     * 不同步时补满缓冲，同步时只补还缺的。字节留在缓冲里，供调用方原地解码、必要时回退。
     * `Saturate` 为 `false` 时返回（指针，实际数量），可能少于 `to_max`；为 `true` 时要求恰好
     * `to_max` 个，返回指针，读满之前到达末尾则抛出。
     * @endif
     *
     * @lang{EN}
     * For `cvt_reader`: takes `to_max` bytes, refilling from the device when the buffer falls
     * short -- what is left moves to the front, and the refill fills the buffer unsynchronized
     * and covers only the shortfall synchronized. The bytes stay in the buffer, for the caller
     * to decode in place and roll back if need be. With `Saturate` false it returns (pointer,
     * count), possibly fewer than `to_max`; with `Saturate` true it requires exactly `to_max`,
     * returns the pointer, and throws if the end comes first.
     * @endif
     * @throws cvt_error @lang{ZH} `to_max` 为 0 或超过 `s_buffer_length`；`Saturate` 时读满前到达末尾。 @endif
     *                   @lang{EN} `to_max` is 0 or exceeds `s_buffer_length`; with `Saturate`, the end comes first. @endif
     */
    template <bool Saturate = false>
    auto get_buf(std::size_t to_max)
    {
        if (to_max == 0)
            throw cvt_error("cvt_reader::get_buf fail, read size cannot be zero");
        if (to_max > s_buffer_length)
            throw cvt_error("cvt_reader::get_buf fail, read size too large");

        if (m_cur == m_end) restore_size();
        char* const buf = m_buffer.data();
        const std::size_t remain = m_end - m_cur;
        if (remain < to_max)
        {
            std::copy(buf + m_cur, buf + m_end, buf);
            m_cur = 0;
            m_end = remain;
            const std::size_t room = s_buffer_length - remain;
            const std::size_t want = synced() ? std::min(to_max - remain, room) : room;
            m_end += m_device.dget(buf + m_end, want);
        }

        if constexpr (Saturate)
        {
            while (m_end < m_cur + to_max)
            {
                const std::size_t got = m_device.dget(buf + m_end, m_cur + to_max - m_end);
                if (got == 0)
                    throw cvt_error("get_buf<Saturate> fail: meet eos");
                m_end += got;
            }
            const char* res = buf + m_cur;
            m_cur += to_max;
            return res;
        }
        else
        {
            const std::size_t len = std::min(m_end - m_cur, to_max);
            std::pair<const char*, std::size_t> res{buf + m_cur, len};
            m_cur += len;
            return res;
        }
    }

    /**
     * @lang{ZH}
     * 供 `cvt_reader` 用：把读取游标后退 `len` 个字节，再把缓冲里剩下的全部字节经 `dunget()`
     * 整体还给设备——只还回退的那几个，排在它们后面的字节就会先被读到。设备拒绝时（fd 模式，
     * 或 Windows）字节留在缓冲里，下一次读取照旧先交出它们。
     * @endif
     *
     * @lang{EN}
     * For `cvt_reader`: moves the read cursor back by `len` bytes, then hands everything left
     * in the buffer back to the device through `dunget()` as a whole -- handing back only the
     * rolled-back bytes would let the bytes behind them be read first. If the device refuses
     * (fd mode, or Windows), the bytes stay in the buffer and the next read hands them out
     * first as before.
     * @endif
     * @throws cvt_error @lang{ZH} `len` 为 0 或超过已交出的字节数。 @endif
     *                   @lang{EN} `len` is 0 or exceeds the bytes handed out. @endif
     */
    void rollback(std::size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_reader::rollback fail, length cannot be zero");
        if (len > m_cur)
            throw cvt_error("cvt_reader::rollback fail, rollback length too large");

        m_cur -= len;
        if (m_device.dunget(m_buffer.data() + m_cur, m_end - m_cur))
            m_cur = m_end = 0;
    }

    /**
     * @lang{ZH}
     * 接手上层退回的字节 `[first, last)`，使它们成为下一次读取最先得到的字节。
     *
     * 缓冲为空时先试 `dunget()`，设备收下就交给了 C stdio。设备拒绝时（fd 模式，或 Windows），
     * 或缓冲里还有字节时（它们在流中排在退回的字节之后，只还退回的字节会颠倒顺序），字节进
     * 缓冲，放在剩余字节的前面；此时 C stdio 看不到它们，但所有标准输入流都读得到。
     *
     * 退回的通常是刚读走的字节，写回游标之前的位置即可，不移动任何数据。前面放不下时，剩余
     * 字节移到缓冲末尾，把空闲空间都留在前面，之后的退回不再移动；总空间也不够就成倍扩容，
     * 没有上限：这些字节本来就占着上层的内存，设上限也省不下什么。扩大的缓冲读空后恢复原来
     * 的大小。
     * @endif
     *
     * @lang{EN}
     * Takes the bytes `[first, last)` handed back from above, so that the next read gets them
     * first.
     *
     * With the buffer empty it tries `dunget()` first, and the bytes the device takes are C
     * stdio's. If the device refuses (fd mode, or Windows), or the buffer still holds bytes
     * (those come after the ones handed back, so giving the device only these would turn the
     * order around), the bytes go into the buffer in front of what is left there; C stdio does
     * not see them then, but every standard input stream reads them.
     *
     * What comes back is usually what was just read, which goes back in front of the cursor
     * with nothing moved. When there is no room in front, what is left moves to the end of the
     * buffer, leaving all the free space in front so that later calls move nothing; when the
     * buffer as a whole is too small it doubles, without a limit: the bytes already occupy
     * memory above, so a limit would save nothing. A grown buffer returns to its usual size
     * once drained.
     * @endif
     * @throws std::bad_alloc @lang{ZH} 扩容失败；此时缓冲、设备与 `FILE` 都没有改变。 @endif
     *                        @lang{EN} Growing failed; the buffer, the device and the `FILE` are then unchanged. @endif
     */
    template <std::bidirectional_iterator It>
        requires std::same_as<std::iter_value_t<It>, char>
    void unget(It first, It last)
    {
        if (first == last) return;
        if (m_cur == m_end && m_device.dunget(first, last)) return;

        const auto n = static_cast<std::size_t>(std::distance(first, last));
        if (n > m_cur) make_room(n);
        m_cur -= n;
        std::copy(first, last, m_buffer.data() + m_cur);
    }

private:
    // Moves what is left to the very end, so that at least n bytes fit in front of it and
    // later calls find room too. A larger buffer is built before anything changes, so a
    // bad_alloc leaves the state as it was.
    void make_room(std::size_t n)
    {
        const std::size_t remain = m_end - m_cur;
        char* const buf = m_buffer.data();
        if (n + remain > m_buffer.size())
        {
            std::vector<char> larger(std::max(n + remain, m_buffer.size() * 2));
            std::copy(buf + m_cur, buf + m_end, larger.data() + larger.size() - remain);
            m_buffer.swap(larger);
        }
        else
            std::copy_backward(buf + m_cur, buf + m_end, buf + m_buffer.size());
        m_end = m_buffer.size();
        m_cur = m_end - remain;
    }

    // Gives back what unget() grew the buffer by; only for a buffer with nothing left in it.
    void restore_size()
    {
        if (m_buffer.size() == s_buffer_length) return;
        m_cur = m_end = 0;
        m_buffer.resize(s_buffer_length);
        m_buffer.shrink_to_fit();
    }

private:
    std_input_device m_device;
    std::vector<char> m_buffer;    ///< @lang{ZH} 读缓冲；未交出的字节在 `[m_cur, m_end)`。 @endif @lang{EN} The read buffer; bytes not yet handed out are in `[m_cur, m_end)`. @endif
    std::size_t m_cur = 0;
    std::size_t m_end = 0;
    std::atomic<bool> m_synced;    ///< @lang{ZH} 为 `true` 时同步：不预读，设备为 stdio 模式。 @endif @lang{EN} Synchronized when `true`: nothing read ahead, the device in stdio mode. @endif
};

/**
 * @lang{ZH}
 * 唯一的那个 `stdin_root_cvt_imp`，所有 `stdin_root_cvt` 句柄都经它工作。与 `cin` 一样由
 * `sing_temp` 在静态初始化时构造、永不析构，访问它只是读一个引用。`IOV2_SHARED` 下是
 * `libiov2.so` 里唯一一份（定义在 `iov2_objects.cpp`），头文件模式下是本程序内的 `inline`
 * 变量；须在 `cin`、`wcin` 之前初始化，本头文件被 `in_impl.h` 先于它们包含即保证这一点。
 * @endif
 *
 * @lang{EN}
 * The one `stdin_root_cvt_imp`, through which every `stdin_root_cvt` handle works. Like
 * `cin`, `sing_temp` builds it during static initialization and never destroys it, so
 * reaching it is reading a reference. Under `IOV2_SHARED` it is the single copy inside
 * `libiov2.so` (defined in `iov2_objects.cpp`); in header-only mode it is an `inline`
 * variable of this program. It must be initialized before `cin` and `wcin`, which
 * `in_impl.h` ensures by including this header before defining them.
 * @endif
 */
#if defined(IOV2_SHARED)
extern IOV2_API stdin_root_cvt_imp& stdin_shared_imp;   // defined in iov2_objects.cpp
#else
/// @cond INTERNAL
inline stdin_root_cvt_imp::init _stdin_imp_init;
/// @endcond
inline stdin_root_cvt_imp& stdin_shared_imp = _stdin_imp_init.get();
#endif
}

/**
 * @lang{ZH}
 * 标准输入流的根转换器：建在 fd 0 的 `std_input_device` 上，带读缓冲，同步与否是运行时的开关。
 *
 * 本类只是句柄，不存状态：设备、缓冲与同步标志都在唯一的 `detail::stdin_shared_imp` 里，
 * 读取逻辑也在那里实现，所有标准输入流（`cin`、`wcin`）都经它工作。于是它们读同一块缓冲：
 * 不同步时一个流预读的字节另一个流接着读得到；同步与否也只有一份，切换一个就切换了全部；
 * `reset()` 或转换器的自动恢复（二者都重新装入设备）丢掉的已预读字节，对所有标准输入流都
 * 丢掉。这些操作由 `cin`、`wcin` 共用的对象锁串行化。
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
 * 流把偷看、压回的字符经 `unget()` 交回来时同样先试 `dunget()`，设备拒绝或缓冲里还有字节时
 * 放进缓冲，见 `stdin_root_cvt_imp::unget()`。
 *
 * 只用于输入：没有写、定位与换向，`bos()` 总是输入方向。不派生自 `root_cvt`。
 * @endif
 *
 * @lang{EN}
 * The root converter of the standard input streams: built on the `std_input_device` on fd 0,
 * with a read buffer, and whether it is synchronized is a runtime switch.
 *
 * This class is only a handle and holds no state: the device, the buffer and the
 * synchronization flag live in the one `detail::stdin_shared_imp`, which implements the
 * reading as well, and every standard input stream (`cin`, `wcin`) works through it. So they
 * read one buffer: unsynchronized, bytes one stream read ahead are there for the other to read
 * on; there is one synchronization setting, so switching one switches them all; and bytes read
 * ahead that `reset()` or a converter's automatic recovery drops (both install a device
 * again) are dropped for every standard input stream. The object lock `cin` and `wcin` share
 * serializes these operations.
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
 * A stream handing back what it peeked at or had put back through `unget()` goes to
 * `dunget()` first as well, and the bytes go into the buffer if the device refuses or the
 * buffer still holds bytes; see `stdin_root_cvt_imp::unget()`.
 *
 * Input only: no writing, positioning or switching, and `bos()` always settles on input. It
 * does not derive from `root_cvt`.
 * @endif
 */
class stdin_root_cvt
{
public:
    using device_type = std_input_device;
    using internal_type = char;
    using external_type = char;

    /// @lang{ZH} 句柄不存状态，所有操作都转给 `detail::stdin_shared_imp`。 @endif @lang{EN} The handle holds no state; every operation goes to `detail::stdin_shared_imp`. @endif
    stdin_root_cvt() noexcept = default;

    stdin_root_cvt(stdin_root_cvt&&) noexcept = default;
    stdin_root_cvt& operator=(stdin_root_cvt&&) noexcept = default;
    stdin_root_cvt(const stdin_root_cvt&) = delete;
    stdin_root_cvt& operator=(const stdin_root_cvt&) = delete;
    ~stdin_root_cvt() = default;

    device_type& device() noexcept { return detail::stdin_shared_imp.device(); }

    /// @lang{ZH} 见 `stdin_root_cvt_imp::detach()`：共享的缓冲随之清空，所有标准输入流都受影响。 @endif @lang{EN} See `stdin_root_cvt_imp::detach()`: the shared buffer is emptied, for every standard input stream. @endif
    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        return detail::stdin_shared_imp.detach();
    }

    /// @lang{ZH} 见 `stdin_root_cvt_imp::attach()`。 @endif @lang{EN} See `stdin_root_cvt_imp::attach()`. @endif
    void attach(device_type&& dev = device_type{})
    {
        detail::stdin_shared_imp.attach(std::move(dev));
    }

    /// @lang{ZH} 总是输入方向。 @endif @lang{EN} Always input. @endif
    io_status bos() noexcept { return io_status::input; }

    /// @lang{ZH} stdin 不可定位，没有要记下的流首位置。 @endif @lang{EN} stdin cannot be repositioned, so there is no stream origin to record. @endif
    void main_cont_beg() noexcept {}

    /**
     * @lang{ZH}
     * `stdin_sync` 切换同步与否（`stdin_root_cvt_imp::synchronize()`），所有标准输入流随之一起
     * 切换；其余行为策略不起作用。
     * @endif
     *
     * @lang{EN}
     * A `stdin_sync` switches synchronization (`stdin_root_cvt_imp::synchronize()`), for every
     * standard input stream at once; other behavior policies have no effect.
     * @endif
     */
    void adjust(const cvt_behavior& b)
    {
        if (const auto* s = dynamic_cast<const stdin_sync*>(&b); s)
            detail::stdin_shared_imp.synchronize(s->synced);
    }

    void retrieve(cvt_status&) const {}

    /// @lang{ZH} 见 `stdin_root_cvt_imp::is_eof()`。 @endif @lang{EN} See `stdin_root_cvt_imp::is_eof()`. @endif
    bool is_eof() { return detail::stdin_shared_imp.is_eof(); }

    /// @lang{ZH} 见 `stdin_root_cvt_imp::get()`。 @endif @lang{EN} See `stdin_root_cvt_imp::get()`. @endif
    std::size_t get(internal_type* to, std::size_t to_max)
    {
        return detail::stdin_shared_imp.get(to, to_max);
    }

    /// @lang{ZH} 见 `stdin_root_cvt_imp::unget()`。 @endif @lang{EN} See `stdin_root_cvt_imp::unget()`. @endif
    template <std::bidirectional_iterator It>
        requires std::same_as<std::iter_value_t<It>, char>
    void unget(It first, It last)
    {
        detail::stdin_shared_imp.unget(first, last);
    }
};

/**
 * @lang{ZH}
 * `stdin_root_cvt` 的 `cvt_reader`：直接在 `stdin_root_cvt_imp` 的缓冲里读，`get_buf()` 与
 * `rollback()` 都转给它。
 * @endif
 *
 * @lang{EN}
 * The `cvt_reader` of `stdin_root_cvt`: it reads in the buffer of `stdin_root_cvt_imp`, and
 * `get_buf()` and `rollback()` both go to it.
 * @endif
 */
template <>
class cvt_reader<stdin_root_cvt>
{
public:
    // The handle holds no state, so there is nothing to keep of it.
    explicit cvt_reader(stdin_root_cvt&, std::vector<char>&, std::size_t&, std::size_t&) noexcept {}

    cvt_reader(const cvt_reader&) = delete;
    cvt_reader& operator=(const cvt_reader&) = delete;
    cvt_reader(cvt_reader&&) = delete;
    cvt_reader& operator=(cvt_reader&&) = delete;
    ~cvt_reader() = default;

    /// @lang{ZH} 空操作：缓冲由根管理。 @endif @lang{EN} No-op: the root manages the buffer. @endif
    void reset(std::size_t) {}

    /// @lang{ZH} 见 `stdin_root_cvt_imp::get_buf()`。 @endif @lang{EN} See `stdin_root_cvt_imp::get_buf()`. @endif
    template <bool Saturate = false>
    auto get_buf(std::size_t to_max)
    {
        return detail::stdin_shared_imp.get_buf<Saturate>(to_max);
    }

    /// @lang{ZH} 见 `stdin_root_cvt_imp::rollback()`。 @endif @lang{EN} See `stdin_root_cvt_imp::rollback()`. @endif
    void rollback(std::size_t len)
    {
        detail::stdin_shared_imp.rollback(len);
    }
};
}

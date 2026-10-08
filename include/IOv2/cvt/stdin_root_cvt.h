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
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/device_concepts.h>

#include <algorithm>
#include <cstddef>
#include <utility>

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
 * `stdin_root_cvt` 翻转同步标志时通知的对象。通知在根上发出，`stdin_sync` 无论从哪条
 * 路径送来都会到达。实现必须 `noexcept`，且不得重入通道：通知时通道正处在 adjust 之中。
 * @endif
 *
 * @lang{EN}
 * What `stdin_root_cvt` notifies when it flips its synchronization flag. The notice is sent
 * at the root, so a `stdin_sync` reaches it whichever path it came by. Implementations must
 * be `noexcept` and must not re-enter the channel: it is in the middle of an adjust then.
 * @endif
 */
struct stdin_sync_listener
{
    virtual void stdin_synced(bool synced) noexcept = 0;

protected:
    virtual ~stdin_sync_listener() = default;
};

/**
 * @lang{ZH}
 * 标准输入流的根转换器：一个带读缓冲的根，同步与否是运行时的开关。
 *
 * 不同步时与 `rb_root_cvt` 完全相同，需要向设备补字节时把缓冲能装的都读进来。同步时只向
 * 设备要调用方还缺的那几个字节，不往前多读——逐字符读就是逐字节读设备。切到同步时缓冲里
 * 已预读的字节照常先交出，交完才按需读，所以切换不丢输入。经 `adjust(stdin_sync{...})`
 * 切换；`root_cvt::get` 与带缓冲根的 `cvt_reader` 在向设备补字节时（缓冲空了，或剩下的
 * 不够本次所需）询问 `read_ahead()`，其它根没有它，不受影响。
 * @endif
 *
 * @lang{EN}
 * The root converter of the standard input streams: a root with a read buffer, whose being
 * synchronized is a runtime switch.
 *
 * Unsynchronized it is exactly `rb_root_cvt`, reading as much as the buffer can take
 * whenever it has to go to the device. Synchronized, it asks the device only for the bytes
 * the caller is still short of and reads nothing ahead -- a character-at-a-time read reads
 * the device a byte at a time. Bytes already read ahead when it switches to synchronized are
 * handed out first as usual, and only then is reading on demand, so a switch loses no
 * input. The mode is switched through `adjust(stdin_sync{...})`; `root_cvt::get` and the
 * buffered-root `cvt_reader` ask `read_ahead()` whenever they go to the device for more
 * bytes (the buffer is empty, or what is left falls short of the request), and other
 * roots, which have none, are not affected.
 * @endif
 *
 * @tparam TDevice
 * @lang{ZH} 底层设备类型。 @endif
 * @lang{EN} The underlying device type. @endif
 */
template <io_device TDevice>
class stdin_root_cvt : public rb_root_cvt<TDevice>
{
    using BT = rb_root_cvt<TDevice>;

public:
    explicit stdin_root_cvt(TDevice dev, bool synced, stdin_sync_listener* listener = nullptr)
        : BT(std::move(dev))
        , m_synced(synced)
        , m_listener(listener)
    {}

    // No copy: whether a copy should notify the listener is unclear, so there is none.
    stdin_root_cvt(const stdin_root_cvt&) = delete;
    stdin_root_cvt& operator=(const stdin_root_cvt&) = delete;

    /**
     * @lang{ZH}
     * 移动构造接过监听者，源对象不再通知。移动赋值保留自己的监听者——它属于持有本根的
     * 对象，而非被赋进来的值——同步标志因此改变时照常通知。
     * @endif
     *
     * @lang{EN}
     * Move construction takes the listener over and the source no longer notifies. Move
     * assignment keeps its own listener -- it belongs to whatever holds this root, not to the
     * value assigned in -- and notifies it as usual when the synchronization flag changes.
     * @endif
     */
    stdin_root_cvt(stdin_root_cvt&& val) noexcept
        : BT(std::move(val))
        , m_synced(val.m_synced)
        , m_listener(std::exchange(val.m_listener, nullptr))
    {}

    stdin_root_cvt& operator=(stdin_root_cvt&& val) noexcept
    {
        if (this == &val) return *this;
        BT::operator=(std::move(val));
        set_synced(val.m_synced);
        return *this;
    }

    /**
     * @lang{ZH}
     * 向设备补字节时要多少：`need` 是调用方还缺的，`room` 是缓冲尾部还能装的（缓冲里剩有
     * 未交出的字节时，它们已移到开头，`room` 小于整个缓冲）。
     * @endif
     *
     * @lang{EN}
     * How many bytes to ask the device for when going to it for more: `need` is what the
     * caller is still short of, `room` what the tail of the buffer can still take (with
     * bytes not yet handed out left in the buffer, they have moved to its front and `room` is
     * less than the whole buffer).
     * @endif
     */
    [[nodiscard]] std::size_t read_ahead(std::size_t need, std::size_t room) const noexcept
    {
        return m_synced ? std::min(need, room) : room;
    }

    /**
     * @lang{ZH}
     * `stdin_sync` 切换同步与否，只翻标志、通知监听者，不碰缓冲；其余交给基类。
     * @endif
     *
     * @lang{EN}
     * A `stdin_sync` switches synchronization by flipping the flag alone and notifying the
     * listener, leaving the buffer as it is; anything else goes to the base class.
     * @endif
     */
    void adjust(const cvt_behavior& b)
    {
        if (const auto* s = dynamic_cast<const stdin_sync*>(&b); s)
            set_synced(s->synced);
        BT::adjust(b);
    }

private:
    void set_synced(bool synced) noexcept
    {
        m_synced = synced;
        if (m_listener)
            m_listener->stdin_synced(synced);
    }

    bool m_synced; ///< 为 `true` 时不预读 / Reads nothing ahead when `true`.
    stdin_sync_listener* m_listener; ///< 翻转时通知，可为空 / Notified on a flip; may be null.
};
}

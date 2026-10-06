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
struct stdin_sync : cvt_behavior
{
    explicit stdin_sync(bool s) noexcept : synced(s) {}
    bool synced; ///< 为 `true` 时同步 / Synchronized when `true`.
};

/**
 * @lang{ZH}
 * 标准输入流的根转换器：一个带读缓冲的根，同步与否是运行时的开关。
 *
 * 不同步时与 `rb_root_cvt` 完全相同，缓冲空了就整块读。同步时缓冲空了只向设备要调用方
 * 缺的那几个字节，不往前多读——逐字符读就是逐字节读设备。切到同步时缓冲里已预读的字节
 * 照常先交出，交完才按需读，所以切换不丢输入。经 `adjust(stdin_sync{...})` 切换；
 * `root_cvt::get` 与带缓冲根的 `cvt_reader` 在补缓冲时询问 `read_ahead()`，其它根没有它，
 * 不受影响。
 * @endif
 *
 * @lang{EN}
 * The root converter of the standard input streams: a root with a read buffer, whose being
 * synchronized is a runtime switch.
 *
 * Unsynchronized it is exactly `rb_root_cvt`, reading a whole buffer once the buffer is
 * empty. Synchronized, an empty buffer asks the device only for the bytes the caller is
 * short of and reads nothing ahead -- a character-at-a-time read reads the device a byte at
 * a time. Bytes already read ahead when it switches to synchronized are handed out first as
 * usual, and only then is reading on demand, so a switch loses no input. Switched through
 * `adjust(stdin_sync{...})`; `root_cvt::get` and the buffered-root `cvt_reader` ask
 * `read_ahead()` when they refill, and other roots, which have none, are not affected.
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
    explicit stdin_root_cvt(TDevice dev, bool synced)
        : BT(std::move(dev))
        , m_synced(synced)
    {}

    /**
     * @lang{ZH}
     * 缓冲空了时向设备要多少字节：`need` 是调用方缺的，`room` 是缓冲能装的。
     * @endif
     *
     * @lang{EN}
     * How many bytes to ask the device for once the buffer is empty: `need` is what the
     * caller is short of, `room` what the buffer can take.
     * @endif
     */
    [[nodiscard]] std::size_t read_ahead(std::size_t need, std::size_t room) const noexcept
    {
        return m_synced ? std::min(need, room) : room;
    }

    /**
     * @lang{ZH}
     * `stdin_sync` 切换同步与否，只翻标志，不碰缓冲；其余交给基类。
     * @endif
     *
     * @lang{EN}
     * A `stdin_sync` switches synchronization by flipping the flag alone, leaving the buffer
     * as it is; anything else goes to the base class.
     * @endif
     */
    void adjust(const cvt_behavior& b)
    {
        if (const auto* s = dynamic_cast<const stdin_sync*>(&b); s)
            m_synced = s->synced;
        BT::adjust(b);
    }

private:
    bool m_synced; ///< 为 `true` 时不预读 / Reads nothing ahead when `true`.
};
}

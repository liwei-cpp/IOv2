/**
 * @file abs_cvt.h
 * @lang{ZH}
 * 抽象转换器基础设施。
 *
 * 本文件提供三个核心组件：
 * - `cvt_reader`：带缓冲的读取辅助类，支持试探性读取与回退。
 * - `cvt_writer`：带缓冲的写入辅助类，支持延迟刷出与回退。
 * - `abs_cvt`：基于 CRTP 的抽象转换器基类，封装 BOS（流起始）处理逻辑
 *   以及可选的 flush、定位、IO 方向切换能力。
 * @endif
 *
 * @lang{EN}
 * Abstract converter infrastructure.
 *
 * This file provides three core components:
 * - `cvt_reader`: A buffered read helper that supports speculative reads and rollback.
 * - `cvt_writer`: A buffered write helper that supports deferred flushing and rollback.
 * - `abs_cvt`: A CRTP-based abstract converter base class that encapsulates BOS
 *   (Beginning-Of-Stream) handling and optional flush, positioning, and IO-direction
 *   switching capabilities.
 * @endif
 */

#pragma once
#include <cvt/cvt_concepts.h>

#include <cstring>
#include <exception>
#include <utility>
#include <vector>

namespace IOv2
{
    /**
     * @lang{ZH}
     * 带缓冲的读取辅助类。
     *
     * 封装对底层 kernel 的缓冲读取操作。内部维护两个游标：
     * - `m_cur_pos`：下一次消费数据的起始位置（当前读取位置）。
     * - `m_end_pos`：缓冲区中有效数据的末尾位置。
     *
     * 该类不可复制也不可移动，设计用于在转换过程中作为短生命周期的辅助对象使用。
     * @endif
     *
     * @lang{EN}
     * Buffered read helper class.
     *
     * Wraps buffered read operations against an underlying kernel. Two internal
     * cursors are maintained:
     * - `m_cur_pos`: start of the next data to be consumed (current read position).
     * - `m_end_pos`: end of valid data in the buffer.
     *
     * The class is neither copyable nor movable and is designed as a short-lived
     * helper object used during conversion.
     * @endif
     *
     * @tparam KernelType
     * @lang{ZH}
     * 底层 IO 转换核心类型，须满足 `io_converter` concept。
     * @endif
     * @lang{EN}
     * The underlying IO converter kernel type, which must satisfy the `io_converter` concept.
     * @endif
     */
    template <io_converter KernelType>
    class cvt_reader
    {
        using char_type = typename KernelType::internal_type;
    public:
        /**
         * @lang{ZH}
         * 构造函数。
         *
         * `cvt_reader` 不拥有 kernel 和 buffer 的所有权，调用方须保证二者在
         * `cvt_reader` 生命周期内有效。
         * @endif
         *
         * @lang{EN}
         * Constructor.
         *
         * `cvt_reader` does not own the kernel or buffer; the caller must ensure
         * both remain valid for the lifetime of this object.
         * @endif
         *
         * @param kernel
         * @lang{ZH} 对底层转换核心的引用。 @endif
         * @lang{EN} Reference to the underlying conversion kernel. @endif
         *
         * @param buffer
         * @lang{ZH} 对外部提供的缓冲区的引用，用于暂存从 kernel 读取的数据。 @endif
         * @lang{EN} Reference to an externally provided buffer used to stage data read from the kernel. @endif
         */
        explicit cvt_reader(KernelType& kernel, std::vector<char_type>& buffer)
            : m_kernel(kernel)
            , m_buffer(buffer) {}

        cvt_reader(const cvt_reader&) = delete;
        cvt_reader& operator=(const cvt_reader&) = delete;
        cvt_reader(cvt_reader&&) = delete;
        cvt_reader& operator=(cvt_reader&&) = delete;
        ~cvt_reader() = default;

        /**
         * @lang{ZH}
         * 重置缓冲区大小并清空所有位置游标。
         *
         * 调用后缓冲区大小被设置为 `buf_size`，`m_cur_pos` 和 `m_end_pos` 均归零，
         * 先前缓冲的所有数据将被丢弃。
         * @endif
         *
         * @lang{EN}
         * Resize the buffer and clear all position cursors.
         *
         * After the call the buffer capacity is set to `buf_size`, and both
         * `m_cur_pos` and `m_end_pos` are reset to zero, discarding any previously
         * buffered data.
         * @endif
         *
         * @param buf_size
         * @lang{ZH} 新的缓冲区元素个数。 @endif
         * @lang{EN} New buffer capacity in number of elements. @endif
         */
        void reset(size_t buf_size)
        {
            m_buffer.resize(buf_size);
            m_cur_pos = 0;
            m_end_pos = 0;
        }

        /**
         * @lang{ZH}
         * 从缓冲区（或必要时从 kernel）获取数据。
         *
         * 模板参数 `Saturate` 控制当缓冲区数据不足时的行为：
         * - `Saturate=false`（默认）：尽力而为，返回实际可用的数据量，可能少于
         *   `to_max`。返回类型为 `std::pair<const char_type*, size_t>`，其中
         *   `second` 为实际返回的元素数量。
         * - `Saturate=true`：饱和模式，保证恰好返回 `to_max` 个字符；若在读取到
         *   足够数据前遭遇流末尾，则抛出 `cvt_error`。返回类型为
         *   `const char_type*`。
         *
         * 返回的指针指向内部缓冲区，在下一次调用 `get_buf` 或 `reset` 之前有效。
         * @endif
         *
         * @lang{EN}
         * Retrieve data from the buffer, reading from the kernel if needed.
         *
         * The template parameter `Saturate` controls behavior when insufficient
         * data is available:
         * - `Saturate=false` (default): Best-effort; returns however much data is
         *   available, which may be less than `to_max`.
         *   Return type: `std::pair<const char_type*, size_t>` where `second` is
         *   the actual number of elements returned.
         * - `Saturate=true`: Saturating mode; guarantees exactly `to_max` characters
         *   are returned; throws `cvt_error` if end-of-stream is reached before
         *   `to_max` characters are available.
         *   Return type: `const char_type*`.
         *
         * The returned pointer refers to the internal buffer and is valid until the
         * next call to `get_buf` or `reset`.
         * @endif
         *
         * @tparam Saturate
         * @lang{ZH}
         * `false`（默认）时返回实际可用数据；`true` 时要求恰好返回 `to_max` 个字符，
         * 不足则抛异常。
         * @endif
         * @lang{EN}
         * When `false` (default), returns available data; when `true`, requires
         * exactly `to_max` characters and throws on short read.
         * @endif
         *
         * @param to_max
         * @lang{ZH} 期望获取的字符数量，不得为零，且不得超过缓冲区容量。 @endif
         * @lang{EN} Number of characters to retrieve; must not be zero and must not
         * exceed the buffer capacity. @endif
         *
         * @return
         * @lang{ZH}
         * `Saturate=false` 时返回 `std::pair<const char_type*, size_t>`（指针与实际数量）；
         * `Saturate=true` 时返回 `const char_type*`（恰好指向 `to_max` 个字符）。
         * @endif
         * @lang{EN}
         * When `Saturate=false`: `std::pair<const char_type*, size_t>` (pointer and actual count).
         * When `Saturate=true`: `const char_type*` pointing to exactly `to_max` characters.
         * @endif
         *
         * @throws cvt_error
         * @lang{ZH}
         * 若 `to_max` 为零；若 `to_max` 超过缓冲区容量；或在 `Saturate=true` 时
         * 在读取足够数据之前遭遇流末尾。
         * @endif
         * @lang{EN}
         * If `to_max` is zero; if `to_max` exceeds the buffer capacity; or, when
         * `Saturate=true`, if end-of-stream is reached before `to_max` characters
         * are available.
         * @endif
         */
        template <bool Saturate = false>
        auto get_buf(size_t to_max)
        {
            if (to_max == 0)
                throw cvt_error("cvt_reader::get_buf fail, read size cannot be zero");
            if (to_max > m_buffer.size())
                throw cvt_error("cvt_reader::get_buf fail, read size too large");

            const auto rollback_size = m_end_pos - m_cur_pos;
            if (to_max <= rollback_size)
            {
                std::pair<const char_type*, size_t> res{m_buffer.data() + m_cur_pos, to_max};
                m_cur_pos += to_max;
                if constexpr (!Saturate)
                    return res;
                else
                    return res.first;
            }

            if (to_max > m_buffer.size() - m_cur_pos)
            {
                std::copy(m_buffer.data() + m_cur_pos, m_buffer.data() + m_end_pos, m_buffer.data());
                m_cur_pos = 0;
                m_end_pos = rollback_size;
            }
            const size_t aim_size = to_max - rollback_size;
            auto read_size = m_kernel.get(m_buffer.data() + m_end_pos, aim_size);
            m_end_pos += read_size;

            if constexpr (Saturate)
            {
                while (read_size < aim_size)
                {
                    const size_t new_aim_size = aim_size - read_size;
                    const auto new_read_size = m_kernel.get(m_buffer.data() + m_end_pos, new_aim_size);
                    if (new_read_size == 0)
                        throw cvt_error("get_buf<Saturate> fail: meet eos");
                    m_end_pos += new_read_size;
                    read_size += new_read_size;
                }
            }

            if constexpr (!Saturate)
            {
                std::pair<const char_type*, size_t> res{m_buffer.data() + m_cur_pos, rollback_size + read_size};
                m_cur_pos = m_end_pos;
                return res;
            }
            else
            {
                const char_type* res = m_buffer.data() + m_cur_pos;
                m_cur_pos = m_end_pos;
                return res;
            }
        }

        /**
         * @lang{ZH}
         * 回退已消耗的字符，将当前读取位置向前移动 `len` 个元素。
         *
         * 用于支持"试探性读取后回退"模式：调用方先通过 `get_buf` 读取数据，
         * 若不满足条件则调用 `rollback` 撤销消耗，使同一段数据可被再次读取。
         * @endif
         *
         * @lang{EN}
         * Roll back consumed characters by advancing the current read position
         * backwards by `len` elements.
         *
         * Supports the speculative-read-then-rollback pattern: the caller reads
         * data via `get_buf`, and if the data does not satisfy a condition, calls
         * `rollback` to undo the consumption so the same data can be read again.
         * @endif
         *
         * @param len
         * @lang{ZH} 要回退的元素数量，不得为零，且不得超过已消耗的字节数。 @endif
         * @lang{EN} Number of elements to roll back; must not be zero and must not
         * exceed the number of already-consumed elements. @endif
         *
         * @throws cvt_error
         * @lang{ZH} 若 `len` 为零，或 `len` 超过已消耗的元素数量。 @endif
         * @lang{EN} If `len` is zero, or if `len` exceeds the number of consumed elements. @endif
         */
        void rollback(size_t len)
        {
            if (len == 0)
                throw cvt_error("cvt_reader::rollback fail, length cannot be zero");
            if (len > m_cur_pos)
                throw cvt_error("cvt_reader::rollback fail, rollback length too large");
            m_cur_pos -= len;
        }

    private:
        /** @lang{ZH} 对底层转换核心的引用，用于在缓冲区耗尽时补充数据。 @endif
         *  @lang{EN} Reference to the underlying kernel, used to refill the buffer when exhausted. @endif */
        KernelType& m_kernel;

        /** @lang{ZH} 对外部缓冲区的引用，暂存从 kernel 读取的数据。 @endif
         *  @lang{EN} Reference to the external buffer that stages data read from the kernel. @endif */
        std::vector<char_type>& m_buffer;

        /** @lang{ZH} 下一次消费数据的起始位置（当前读取游标）。 @endif
         *  @lang{EN} Start position of the next data to be consumed (current read cursor). @endif */
        size_t m_cur_pos = 0;

        /** @lang{ZH} 缓冲区中有效数据的末尾位置（独占上界）。 @endif
         *  @lang{EN} End position of valid data in the buffer (exclusive upper bound). @endif */
        size_t m_end_pos = 0;
    };

    /**
     * @lang{ZH}
     * 带缓冲的写入辅助类。
     *
     * 封装对底层 kernel 的缓冲写入操作。数据首先积累在内部缓冲区中，仅在缓冲区
     * 空间不足或显式调用 `commit()` 时才批量写入 kernel，从而减少对 kernel 的
     * 调用次数。
     *
     * 内部维护 `m_prev_len`，记录已填充但尚未刷出到 kernel 的数据字节数。
     *
     * 该类不可复制也不可移动，设计用于在转换过程中作为短生命周期的辅助对象使用。
     * @endif
     *
     * @lang{EN}
     * Buffered write helper class.
     *
     * Wraps buffered write operations against an underlying kernel. Data is first
     * accumulated in an internal buffer and flushed to the kernel in bulk only when
     * the buffer is full or `commit()` is called explicitly, reducing the number of
     * kernel write calls.
     *
     * The internal `m_prev_len` field tracks the amount of data that has been
     * placed in the buffer but not yet flushed to the kernel.
     *
     * The class is neither copyable nor movable and is designed as a short-lived
     * helper object used during conversion.
     * @endif
     *
     * @tparam KernelType
     * @lang{ZH}
     * 底层 IO 转换核心类型，须满足 `io_converter` concept。
     * @endif
     * @lang{EN}
     * The underlying IO converter kernel type, which must satisfy the `io_converter` concept.
     * @endif
     */
    template <io_converter KernelType>
    class cvt_writer
    {
        using char_type = typename KernelType::internal_type;
    public:
        /**
         * @lang{ZH}
         * 构造函数。
         *
         * `cvt_writer` 不拥有 kernel 和 buffer 的所有权，调用方须保证二者在
         * `cvt_writer` 生命周期内有效。
         * @endif
         *
         * @lang{EN}
         * Constructor.
         *
         * `cvt_writer` does not own the kernel or buffer; the caller must ensure
         * both remain valid for the lifetime of this object.
         * @endif
         *
         * @param kernel
         * @lang{ZH} 对底层转换核心的引用。 @endif
         * @lang{EN} Reference to the underlying conversion kernel. @endif
         *
         * @param buffer
         * @lang{ZH} 对外部提供的缓冲区的引用，用于暂存待写入 kernel 的数据。 @endif
         * @lang{EN} Reference to an externally provided buffer used to stage data before writing to the kernel. @endif
         */
        explicit cvt_writer(KernelType& kernel, std::vector<char_type>& buffer)
            : m_kernel(kernel)
            , m_buffer(buffer) {}

        cvt_writer(const cvt_writer&) = delete;
        cvt_writer& operator=(const cvt_writer&) = delete;
        cvt_writer(cvt_writer&&) = delete;
        cvt_writer& operator=(cvt_writer&&) = delete;
        ~cvt_writer() = default;

        /**
         * @lang{ZH}
         * 重置缓冲区大小并清空积累的数据。
         *
         * 调用后缓冲区大小被设置为 `buf_size`，`m_prev_len` 归零，先前积累但
         * 尚未刷出的所有数据将被丢弃（不写入 kernel）。
         * @endif
         *
         * @lang{EN}
         * Resize the buffer and discard any accumulated unflushed data.
         *
         * After the call the buffer capacity is set to `buf_size` and `m_prev_len`
         * is reset to zero. Any previously accumulated but unflushed data is
         * discarded without being written to the kernel.
         * @endif
         *
         * @param buf_size
         * @lang{ZH} 新的缓冲区元素个数。 @endif
         * @lang{EN} New buffer capacity in number of elements. @endif
         */
        void reset(size_t buf_size)
        {
            m_buffer.resize(buf_size);
            m_prev_len = 0;
        }

        /**
         * @lang{ZH}
         * 申请写入槽，返回指向缓冲区中可写入位置的指针。
         *
         * 若缓冲区剩余空间不足以容纳 `len` 个元素，则先将已积累的数据刷出到
         * kernel，再返回缓冲区起始位置的指针。调用方应将恰好 `len` 个元素写入
         * 返回的指针位置；写入完成后无需额外操作，下一次 `put_buf` 调用或
         * `commit` 会正确处理这些数据。
         * @endif
         *
         * @lang{EN}
         * Allocate a write slot and return a pointer to the writable position in
         * the buffer.
         *
         * If the remaining buffer space is insufficient to hold `len` elements, the
         * accumulated data is first flushed to the kernel before the pointer is
         * returned. The caller must write exactly `len` elements to the returned
         * pointer; no further action is required after writing — the data will be
         * handled correctly by the next `put_buf` call or by `commit`.
         * @endif
         *
         * @param len
         * @lang{ZH} 期望写入的元素数量，不得为零，且不得超过缓冲区容量。 @endif
         * @lang{EN} Number of elements to write; must not be zero and must not
         * exceed the buffer capacity. @endif
         *
         * @return
         * @lang{ZH} 指向缓冲区中可安全写入 `len` 个元素的起始位置的指针。 @endif
         * @lang{EN} Pointer to the start of a region in the buffer safe to write `len` elements into. @endif
         *
         * @throws cvt_error
         * @lang{ZH} 若 `len` 为零，或 `len` 超过缓冲区容量。 @endif
         * @lang{EN} If `len` is zero, or if `len` exceeds the buffer capacity. @endif
         */
        char_type* put_buf(size_t len)
        {
            if (len == 0)
                throw cvt_error("cvt_writer::put_buf fail, write size cannot be zero");
            if (len > m_buffer.size())
                throw cvt_error("cvt_writer::put_buf fail, write size too large");

            size_t remain = m_buffer.size() - m_prev_len;
            if (remain < len)
            {
                m_kernel.put(m_buffer.data(), m_prev_len);
                remain = m_buffer.size();
                m_prev_len = 0;
            }

            auto res = m_buffer.data() + m_prev_len;
            m_prev_len += len;
            return res;
        }

        /**
         * @lang{ZH}
         * 回退已写入缓冲区但尚未 commit 的数据。
         *
         * 将 `m_prev_len` 向前缩减 `len` 个元素，使这部分槽位可被重新使用。
         * 仅用于回退尚未刷出到 kernel 的数据；已刷出的数据无法回退。
         * @endif
         *
         * @lang{EN}
         * Roll back data that has been written into the buffer but not yet committed.
         *
         * Reduces `m_prev_len` by `len` elements, freeing those slots for reuse.
         * Only data that has not yet been flushed to the kernel can be rolled back;
         * data already flushed cannot be undone.
         * @endif
         *
         * @param len
         * @lang{ZH} 要回退的元素数量，不得为零，且不得超过 `m_prev_len`。 @endif
         * @lang{EN} Number of elements to roll back; must not be zero and must not
         * exceed `m_prev_len`. @endif
         *
         * @throws cvt_error
         * @lang{ZH} 若 `len` 为零，或 `len` 超过 `m_prev_len`。 @endif
         * @lang{EN} If `len` is zero, or if `len` exceeds `m_prev_len`. @endif
         */
        void rollback(size_t len)
        {
            if (len == 0)
                throw cvt_error("cvt_writer::rollback fail, length cannot be zero");
            if (len > m_prev_len)
                throw cvt_error("cvt_writer::rollback fail, rollback length too large");
            m_prev_len -= len;
        }

        /**
         * @lang{ZH}
         * 将缓冲区中所有尚未刷出的数据强制写入 kernel。
         *
         * 调用后 `m_prev_len` 归零。若缓冲区为空，则不执行任何 kernel 写操作。
         * @endif
         *
         * @lang{EN}
         * Flush all buffered data that has not yet been written to the kernel.
         *
         * After the call `m_prev_len` is reset to zero. If the buffer is empty no
         * kernel write is performed.
         * @endif
         */
        void commit()
        {
            if (m_prev_len != 0)
            {
                m_kernel.put(m_buffer.data(), m_prev_len);
                m_prev_len = 0;
            }
        }

    private:
        /** @lang{ZH} 对底层转换核心的引用，用于在缓冲区满时将数据刷出。 @endif
         *  @lang{EN} Reference to the underlying kernel, used to flush data when the buffer is full. @endif */
        KernelType& m_kernel;

        /** @lang{ZH} 对外部缓冲区的引用，暂存待写入 kernel 的数据。 @endif
         *  @lang{EN} Reference to the external buffer that stages data to be written to the kernel. @endif */
        std::vector<char_type>& m_buffer;

        /** @lang{ZH} 缓冲区中已填充但尚未刷出到 kernel 的元素数量。 @endif
         *  @lang{EN} Number of elements that have been placed in the buffer but not yet flushed to the kernel. @endif */
        size_t m_prev_len = 0;
    };

    /**
     * @lang{ZH}
     * 基于 CRTP 的抽象转换器基类。
     *
     * `abs_cvt` 为所有具体转换器提供公共骨架，负责：
     * - **BOS（Beginning-Of-Stream）处理**：在 `bos()` 调用后、`main_cont_beg()`
     *   调用前的阶段，`get`/`put` 直接以原始字节方式传递数据（不经过派生类的
     *   `get_main`/`put_main`），用于读写文件头、魔数等流起始特殊数据。
     *   BOS 阶段写入时，若输入数据字节数不是 `sizeof(external_type)` 的整数倍，
     *   末尾不足部分将以零填充。
     * - **设备管理**：通过 `device()`、`detach()`、`attach()` 管理底层设备。
     * - **行为与状态**：通过 `adjust()` 和 `retrieve()` 配置和查询转换参数。
     * - **可选能力**：通过模板参数和 `requires` 子句按需暴露 flush、定位
     *   (`tell`/`seek`/`rseek`) 和 IO 方向切换能力。
     *
     * 派生类须实现 `get_main(cvt_reader<KernelType>&, internal_type*, size_t)`
     * 和/或 `put_main(cvt_writer<KernelType>&, const internal_type*, size_t)`
     * 来处理主内容阶段的实际转换逻辑。
     *
     * @note 调用顺序：成员函数必须按以下顺序调用：
     *       1. **bos()**：必须首先调用，以建立初始 IO 状态。
     *       2. **BOS 阶段**：可多次调用 `get()` 或多次调用 `put()`，但不可混合调用。
     *          此阶段用于读写流起始数据（如文件头、魔数）。
     *       3. **main_cont_beg()**：标记 BOS 阶段结束，进入主内容阶段。
     *       4. **主内容阶段**：可调用所有其他成员函数（`get`、`put`、`flush`、
     *          `tell`、`seek`、`rseek`、`switch_to_get`、`switch_to_put` 等）。
     *       违反此调用顺序将导致未定义行为。
     *
     * @note 派生类生命周期方法的执行顺序：派生类重写 `bos()`、`main_cont_beg()`、
     *       `attach()`、`detach()`、`close_stream()` 等生命周期方法时，必须按
     *       下列规则编排"修改本层成员"与"调用 `BT::xxx()`"的先后顺序，以保证
     *       异常安全与层间状态一致：
     *       - **起始/初始化操作**（`bos`、`main_cont_beg`）：**先调用 `BT::xxx()`，
     *         再修改本层成员**。语义类比 C++ 构造函数的"基类先于派生类构造"：
     *         若下层抛出异常，本层状态保持不变（强异常安全）；若下层成功，
     *         下层已就绪可供本层读取（例如读取 `BT::m_io_status` 决定本层行为）。
     *       - **结束/清理操作**（`detach`、`close_stream`、用于终结的 `flush`）：
     *         **先冲刷/清理本层状态，再调用 `BT::xxx()`**。语义类比 C++ 析构函数
     *         的"派生类先于基类析构"：本层须在下层关闭前把待写数据冲入下层，
     *         否则冲入将无处可去。
     * @endif
     *
     * @lang{EN}
     * CRTP-based abstract converter base class.
     *
     * `abs_cvt` provides a common skeleton for all concrete converters and is
     * responsible for:
     * - **BOS (Beginning-Of-Stream) handling**: Between the `bos()` call and the
     *   `main_cont_beg()` call, `get`/`put` pass data through as raw bytes without
     *   invoking the derived class's `get_main`/`put_main`. This allows reading and
     *   writing stream-leading data such as file headers and magic numbers. During
     *   BOS writes, if the input byte count is not a multiple of
     *   `sizeof(external_type)`, the final partial unit is zero-padded.
     * - **Device management**: Manages the underlying device via `device()`,
     *   `detach()`, and `attach()`.
     * - **Behavior and status**: Configures and queries conversion parameters via
     *   `adjust()` and `retrieve()`.
     * - **Optional capabilities**: Exposes flush, positioning (`tell`/`seek`/`rseek`),
     *   and IO-direction switching on demand through template parameters and
     *   `requires` clauses.
     *
     * Derived classes must implement `get_main(cvt_reader<KernelType>&, internal_type*, size_t)`
     * and/or `put_main(cvt_writer<KernelType>&, const internal_type*, size_t)` to
     * handle the actual conversion logic in the main-content phase.
     *
     * @note Calling sequence: Member functions must be called in the following order:
     *       1. **bos()**: Must be called first to establish the initial IO status.
     *       2. **BOS phase**: May call `get()` multiple times or `put()` multiple times,
     *          but must not mix get and put calls. This phase is for reading/writing
     *          stream-leading data (e.g., file headers, magic numbers).
     *       3. **main_cont_beg()**: Marks the end of the BOS phase and enters the
     *          main-content phase.
     *       4. **Main-content phase**: All other member functions may be called
     *          (`get`, `put`, `flush`, `tell`, `seek`, `rseek`, `switch_to_get`,
     *          `switch_to_put`, etc.).
     *       Violating this calling sequence results in undefined behavior.
     *
     * @note Ordering rule for derived lifecycle methods: When a derived class
     *       overrides `bos()`, `main_cont_beg()`, `attach()`, `detach()`,
     *       `close_stream()`, or similar lifecycle methods, the order of
     *       "mutate this layer's members" vs. "call `BT::xxx()`" must follow
     *       the rule below to guarantee exception safety and inter-layer
     *       state consistency:
     *       - **Begin / init operations** (`bos`, `main_cont_beg`): **call
     *         `BT::xxx()` first, then mutate this layer's members**. This
     *         mirrors C++ constructor semantics ("base before derived"): if
     *         the lower layer throws, this layer's state is unchanged (strong
     *         exception guarantee); if it succeeds, the lower layer is ready
     *         and this layer may inspect its state (e.g., read
     *         `BT::m_io_status`) to decide what to do.
     *       - **End / teardown operations** (`detach`, `close_stream`,
     *         finalizing `flush`): **mutate/flush this layer first, then call
     *         `BT::xxx()`**. This mirrors C++ destructor semantics ("derived
     *         before base"): this layer must drain pending data into the
     *         lower layer before the lower layer is torn down — otherwise the
     *         drained data has nowhere to go.
     *
     * @par CRTP 可选钩子 / CRTP Optional Hooks
     * @lang{ZH}
     * 派生类可实现以下零个或多个私有钩子函数。`abs_cvt` 通过
     * `if constexpr (requires { ... })` 在编译期检测各钩子是否存在，并在对应的
     * 生命周期节点调用。所有钩子须声明为 `private`，并在派生类中添加
     * `friend BT` 授权 `abs_cvt` 访问。
     *
     * - **`get_main(cvt_reader<KernelType>&, internal_type*, size_t) -> size_t`**
     *   由 `abs_cvt::get` 在主内容阶段调用，执行解码逻辑。
     *   返回值为实际解码的 `internal_type` 元素个数。
     *   允许抛出异常；调用方会正确传播。
     *
     * - **`put_main(cvt_writer<KernelType>&, const internal_type*, size_t)`**
     *   由 `abs_cvt::put` 在主内容阶段调用，执行编码逻辑。
     *   禁止自行调用 `cvt_writer::commit()`；commit 由 `abs_cvt::put` 在成功路径统一负责。
     *   允许抛出异常；调用方会设置污染标志并传播。
     *
     * - **`flush_impl()`**
     *   由 `abs_cvt::flush` 在调用 `m_kernel.flush()` 之前调用，执行派生层特有的冲刷逻辑
     *   （例如 zlib sync flush）。允许抛出异常；调用方会设置污染标志并传播。
     *
     * - **`detach_impl() noexcept -> std::exception_ptr`**
     *   由 `abs_cvt::detach` 在调用 `m_kernel.detach()` 之前、以及 `abs_cvt::attach`
     *   在安装新设备之前调用，执行派生层清理（如关闭旧流、重置内部状态）。
     *   **必须声明为 `noexcept`**（由 `abs_cvt` 内的 `static_assert` 强制）。
     *   捕获到的异常须以 `std::exception_ptr` 形式返回；调用方按 first-failure-wins
     *   与 kernel 层异常合并后统一透传。无异常时返回 `nullptr`。
     *
     * - **`attach_impl()`**
     *   由 `abs_cvt::attach` 在新设备安装并重置状态字段之后调用，执行派生层的
     *   初始化（如验证配置、重置加密上下文）。允许抛出异常；调用方会设置污染
     *   标志并传播。`detach_impl()` 捕获到的旧流错误会在 `attach_impl()` 成功
     *   返回后再抛出。
     * @endif
     *
     * @lang{EN}
     * Derived classes may implement zero or more of the following private hooks.
     * `abs_cvt` detects each hook at compile time via `if constexpr (requires { ... })`
     * and invokes it at the documented lifecycle point. All hooks must be declared
     * `private`; the derived class must add `friend BT` to grant `abs_cvt` access.
     *
     * - **`get_main(cvt_reader<KernelType>&, internal_type*, size_t) -> size_t`**
     *   Called by `abs_cvt::get` during the main-content phase to perform decoding.
     *   Returns the number of `internal_type` elements actually decoded.
     *   May throw; the caller propagates the exception normally.
     *
     * - **`put_main(cvt_writer<KernelType>&, const internal_type*, size_t)`**
     *   Called by `abs_cvt::put` during the main-content phase to perform encoding.
     *   Must NOT call `cvt_writer::commit()` itself; `abs_cvt::put` is the sole
     *   committer on the success path. May throw; the caller sets the taint flag
     *   and rethrows.
     *
     * - **`flush_impl()`**
     *   Called by `abs_cvt::flush` before `m_kernel.flush()`, to perform any
     *   derived-layer flushing (e.g., a zlib sync flush).
     *   May throw; the caller sets the taint flag and rethrows.
     *
     * - **`detach_impl() noexcept -> std::exception_ptr`**
     *   Called by `abs_cvt::detach` before `m_kernel.detach()`, and by
     *   `abs_cvt::attach` before installing the new device, to perform
     *   derived-layer cleanup (e.g., closing the old stream, resetting internal
     *   state). **Must be declared `noexcept`** (enforced by a `static_assert`
     *   inside `abs_cvt`). Any captured exception must be returned as a
     *   `std::exception_ptr`; the caller merges it with the kernel-layer exception
     *   under first-failure-wins and forwards the combined result. Return `nullptr`
     *   if none.
     *
     * - **`attach_impl()`**
     *   Called by `abs_cvt::attach` after the new device has been installed and
     *   the state fields reset, to perform derived-layer initialization (e.g.,
     *   validating configuration, resetting a cipher context). May throw; the
     *   caller sets the taint flag and rethrows. Any error captured by
     *   `detach_impl()` for the old stream is rethrown only after `attach_impl()`
     *   returns successfully.
     * @endif
     *
     * @par Exception Safety / Tainted State
     * Output streaming cannot in general offer the strong exception
     * guarantee: a single `put` may auto-flush several chunks to the kernel
     * before encountering a malformed code unit, and those already-flushed
     * bytes cannot be rolled back — the resulting byte sequence on the
     * underlying device is therefore truncated or otherwise inconsistent.
     * To prevent silent corruption from continued use after such a failure,
     * `abs_cvt` maintains a *tainted* flag (`m_is_tainted`). The contract is:
     *   - `put` wraps its entire body in a catch-all and sets
     *     `m_is_tainted = true` on any exception before rethrowing. It also
     *     calls `assert_not_tainted()` on entry, so a tainted converter
     *     refuses further IO.
     *   - `get` does NOT taint on its own failures: read errors only
     *     advance the kernel cursor and may leave a per-call mbstate
     *     undefined, but the underlying byte stream itself is unchanged
     *     and the caller can reseek to recover. `get` does, however,
     *     honour any pre-existing taint via `assert_not_tainted()`.
     *   - Derived classes MUST NOT call `cvt_writer::commit()` themselves;
     *     `abs_cvt::put` is the sole committer on the success path. Any
     *     exception thrown out of `put_main` taints the converter, and
     *     uncommitted buffer contents are dropped at writer destruction.
     *     This is a deliberate semantic change from the old
     *     "commit-the-prefix-then-throw" pattern: encoding errors discard
     *     the in-progress batch instead of producing a partial commit.
     *   - Derived classes MAY call `assert_not_tainted()` at the entry of
     *     positioning, IO-direction switching, or any other operation whose
     *     correctness depends on a known-good kernel state.
     *   - The flag is cleared by `attach` and `detach`, and is propagated
     *     by copy/move ctors and assignments (the source of a move is
     *     reset to a clean, empty state).
     * @endif
     *
     * @tparam CurrentType
     * @lang{ZH} CRTP 派生类自身的类型。 @endif
     * @lang{EN} The CRTP derived class type itself. @endif
     *
     * @tparam KernelType
     * @lang{ZH} 底层 IO 转换核心类型，须满足 `io_converter` concept。 @endif
     * @lang{EN} The underlying IO converter kernel type, which must satisfy the `io_converter` concept. @endif
     *
     * @tparam InternalType
     * @lang{ZH} 派生类对外暴露的字符类型（即 `get`/`put` 接口所使用的元素类型）。 @endif
     * @lang{EN} The character type exposed by the derived class (the element type used by the `get`/`put` interface). @endif
     *
     * @tparam default_positioning
     * @lang{ZH}
     * 若为 `true`（默认），且 kernel 支持定位，则 `tell()`/`seek()`/`rseek()` 方法可用。
     * @endif
     * @lang{EN}
     * When `true` (default) and the kernel supports positioning, the `tell()`/`seek()`/`rseek()` methods are available.
     * @endif
     *
     * @tparam default_io_switch
     * @lang{ZH}
     * 若为 `true`（默认），且 kernel 支持 IO 方向切换，则 `switch_to_get()`/`switch_to_put()` 方法可用。
     * @endif
     * @lang{EN}
     * When `true` (default) and the kernel supports IO-direction switching, the `switch_to_get()`/`switch_to_put()` methods are available.
     * @endif
     */
    template <typename CurrentType,
              io_converter KernelType,
              typename InternalType,
              bool default_positioning = true,
              bool default_io_switch = true>
    class abs_cvt
    {
    public:
        /** @lang{ZH} 底层设备类型，由 KernelType 提供。 @endif
         *  @lang{EN} The underlying device type, provided by KernelType. @endif */
        using device_type = typename KernelType::device_type;

        /** @lang{ZH} 外部字符类型（即 kernel 的内部类型），用于 BOS 阶段的原始字节传递。 @endif
         *  @lang{EN} The external character type (the kernel's internal type), used for raw byte passthrough during the BOS phase. @endif */
        using external_type = typename KernelType::internal_type;

        /** @lang{ZH} 派生类对外暴露的字符类型，即 `get`/`put` 接口的元素类型。 @endif
         *  @lang{EN} The character type exposed by the derived class; the element type of the `get`/`put` interface. @endif */
        using internal_type = InternalType;

    private:
        /** @lang{ZH}
         *  BOS 阶段每次向 kernel 写出的 `external_type` 元素数量。
         *  分块处理可避免为大型流起始数据一次性分配大块内存，提升内存使用效率。
         *  @endif
         *  @lang{EN}
         *  Number of `external_type` elements written to the kernel per chunk during the BOS phase.
         *  Chunked processing avoids allocating a large memory block for large stream-leading data,
         *  improving memory efficiency.
         *  @endif */
        constexpr static size_t s_bos_chunk = 16;

    public:
        /**
         * @lang{ZH}
         * 从 kernel 构造转换器，获取 kernel 的所有权。
         * @endif
         *
         * @lang{EN}
         * Construct the converter from a kernel, taking ownership of it.
         * @endif
         *
         * @param kernel
         * @lang{ZH} 底层转换核心，以移动方式转交所有权。 @endif
         * @lang{EN} The underlying conversion kernel; ownership is transferred via move. @endif
         */
        abs_cvt(KernelType kernel)
            : m_kernel(std::move(kernel)) {}

        /**
         * @lang{ZH}
         * 拷贝构造函数。复制 kernel、IO 状态和 BOS 完成标志；临时 IO 缓冲区不复制。
         * @endif
         *
         * @lang{EN}
         * Copy constructor. Copies the kernel, IO status, and BOS-done flag;
         * the temporary IO buffer is not copied.
         * @endif
         */
        abs_cvt(const abs_cvt& val)
            : m_kernel(val.m_kernel)
            , m_io_status(val.m_io_status)
            , m_is_bos_done(val.m_is_bos_done)
            , m_is_tainted(val.m_is_tainted) {}

        /**
         * @lang{ZH}
         * 移动构造函数。转移 kernel 所有权；源对象的 IO 状态被重置为 `neutral`，
         * BOS 完成标志被重置为 `false`。
         * @endif
         *
         * @lang{EN}
         * Move constructor. Transfers ownership of the kernel; the source object's
         * IO status is reset to `neutral` and its BOS-done flag is reset to `false`.
         * @endif
         */
        abs_cvt(abs_cvt&& val) noexcept
            : m_kernel(std::move(val.m_kernel))
            , m_io_status(val.m_io_status)
            , m_is_bos_done(val.m_is_bos_done)
            , m_is_tainted(val.m_is_tainted)
        {
            val.m_io_status = io_status::neutral;
            val.m_is_bos_done = false;
            val.m_is_tainted = false;
        }

        /**
         * @lang{ZH}
         * 拷贝赋值运算符。复制 kernel、IO 状态和 BOS 完成标志。
         * `cvt_reader`/`cvt_writer` 始终引用 `&m_kernel`，赋值后无需额外处理。
         * @endif
         *
         * @lang{EN}
         * Copy assignment operator. Copies the kernel, IO status, and BOS-done flag.
         * `cvt_reader`/`cvt_writer` always reference `&m_kernel`, so no additional
         * fixup is needed after assignment.
         * @endif
         */
        abs_cvt& operator=(const abs_cvt& val)
        {
            if (this != &val)
            {
                m_kernel = val.m_kernel;
                m_io_status = val.m_io_status;
                m_is_bos_done = val.m_is_bos_done;
                m_is_tainted = val.m_is_tainted;
                // m_reader and m_writer keep pointing to &m_kernel, no change needed
            }
            return *this;
        }

        /**
         * @lang{ZH}
         * 移动赋值运算符。转移 kernel 所有权；源对象的 IO 状态和 BOS 标志均被重置。
         * @endif
         *
         * @lang{EN}
         * Move assignment operator. Transfers ownership of the kernel; the source
         * object's IO status and BOS-done flag are both reset.
         * @endif
         */
        abs_cvt& operator=(abs_cvt&& val) noexcept
        {
            if (this != &val)
            {
                m_kernel = std::move(val.m_kernel);
                m_io_status = val.m_io_status;
                val.m_io_status = io_status::neutral;
                m_is_bos_done = val.m_is_bos_done;
                val.m_is_bos_done = false;
                m_is_tainted = val.m_is_tainted;
                val.m_is_tainted = false;
            }
            return *this;
        }

        ~abs_cvt() = default;

    // mandatory methods
    public:
        /**
         * @lang{ZH}
         * 返回对底层设备的引用。
         * @endif
         *
         * @lang{EN}
         * Return a reference to the underlying device.
         * @endif
         *
         * @return
         * @lang{ZH} 对底层设备的左值引用。 @endif
         * @lang{EN} An lvalue reference to the underlying device. @endif
         */
        device_type& device() { return m_kernel.device(); }

        /**
         * @lang{ZH}
         * 分离并返回底层设备，同时将转换器状态重置为初始状态。
         *
         * 调用后 `m_io_status` 恢复为 `neutral`，`m_is_bos_done` 与 `m_is_tainted`
         * 恢复为 `false`。
         *
         * 本函数为 `noexcept`，清理异常通过返回值的 `exception_ptr` 透传（first-failure-wins）：
         * - 若派生类实现了 `detach_impl() noexcept`，则先调用之；其异常优先于 kernel 的异常。
         *   编译器会通过 `static_assert` 强制要求 `detach_impl()` 必须声明为 `noexcept`。
         * - 随后调用 `m_kernel.detach()` 完成 kernel 层的分离与清理。
         *
         * 设备始终通过返回值的 `first` 无条件交还给调用方。
         * @endif
         *
         * @lang{EN}
         * Detach and return the underlying device, resetting the converter state
         * to its initial state.
         *
         * After the call `m_io_status` is reset to `neutral`, and both `m_is_bos_done`
         * and `m_is_tainted` are reset to `false`.
         *
         * This function is `noexcept`; cleanup exceptions are forwarded via the returned
         * `exception_ptr` (first-failure-wins):
         * - If the derived class implements `detach_impl() noexcept`, it is called first;
         *   its exception (if any) takes priority over the kernel's exception.
         *   A `static_assert` enforces that `detach_impl()` must be declared `noexcept`.
         * - `m_kernel.detach()` is then called to complete kernel-level detachment and cleanup.
         *
         * The device is always handed back unconditionally via `first`.
         * @endif
         *
         * @return
         * @lang{ZH} pair：`first` 为已从 kernel 中分离的底层设备对象（右值），`second` 为清理过程中捕获的首个异常（`nullptr` 表示无异常）。 @endif
         * @lang{EN} A pair: `first` is the device detached from the kernel (rvalue); `second` is the first exception captured during cleanup (`nullptr` if none). @endif
         */
        std::pair<device_type, std::exception_ptr> detach() noexcept
        {
            std::exception_ptr local_err = nullptr;
            if constexpr (requires(CurrentType& t) { t.detach_impl(); })
            {
                static_assert(noexcept(std::declval<CurrentType&>().detach_impl()),
                              "detach_impl() must be noexcept");
                local_err = static_cast<CurrentType*>(this)->detach_impl();
            }

            auto [dev, inner_err] = m_kernel.detach();
            m_io_status = io_status::neutral;
            m_is_bos_done = false;
            m_is_tainted = false;
            return { std::move(dev), local_err ? local_err : inner_err };
        }

        /**
         * @lang{ZH}
         * 将新设备附加到转换器，并重置转换器状态。执行顺序如下：
         *
         * 1. 若派生类实现了 `detach_impl() noexcept`，先调用之，捕获并暂存其异常
         *    （first-failure-wins）。
         * 2. 调用 `m_kernel.attach()` 安装新设备，并将 `m_io_status`、`m_is_bos_done`、
         *    `m_is_tainted` 重置为初始值。若此步抛出，异常直接传播，`m_is_tainted` 被设置。
         * 3. 若派生类实现了 `attach_impl()`，调用之执行派生层初始化。若此步抛出，
         *    `m_is_tainted` 被设置并传播。
         * 4. 若步骤 1 捕获到异常，在步骤 2–3 均成功后重新抛出。
         *
         * 原设备在 `m_kernel.attach()` 内部静默析构，不会归还调用方。若需要保留
         * 原设备，应先单独调用 `detach()` 取得设备，再调用本函数装入新设备。
         * 若 `dev` 为默认构造值（空设备），则等效于清除当前设备。
         * @endif
         *
         * @lang{EN}
         * Attach a new device to the converter and reset the converter state.
         * The call sequence is:
         *
         * 1. If the derived class implements `detach_impl() noexcept`, call it first
         *    and stash any returned exception (first-failure-wins).
         * 2. Call `m_kernel.attach()` to install the new device and reset
         *    `m_io_status`, `m_is_bos_done`, and `m_is_tainted` to their initial
         *    values. If this step throws, the exception propagates and
         *    `m_is_tainted` is set.
         * 3. If the derived class implements `attach_impl()`, call it to perform
         *    derived-layer initialization. If this step throws, `m_is_tainted` is
         *    set and the exception propagates.
         * 4. If step 1 captured an exception, rethrow it after steps 2–3 succeed.
         *
         * The previously held device is silently destroyed inside `m_kernel.attach()`
         * and is not returned to the caller. Callers who need to preserve the old
         * device must call `detach()` first to retrieve it, then call this function
         * to install the new one. Passing a default-constructed (empty) `dev` is
         * equivalent to clearing the current device.
         * @endif
         *
         * @param dev
         * @lang{ZH} 要附加的新设备（以移动方式转交所有权），默认为空设备。 @endif
         * @lang{EN} The new device to attach (ownership transferred via move); defaults to an empty device. @endif
         */
        void attach(device_type&& dev = device_type{})
        {
            std::exception_ptr local_err = nullptr;
            if constexpr (requires(CurrentType& t) { t.detach_impl(); })
            {
                static_assert(noexcept(std::declval<CurrentType&>().detach_impl()),
                              "detach_impl() must be noexcept");
                local_err = static_cast<CurrentType*>(this)->detach_impl();
            }

            try
            {
                m_kernel.attach(std::move(dev));
                m_io_status = io_status::neutral;
                m_is_bos_done = false;
                m_is_tainted = false;

                if constexpr (requires(CurrentType& t) { t.attach_impl(); })
                    static_cast<CurrentType*>(this)->attach_impl();
            }
            catch (...)
            {
                m_is_tainted = true;
                throw;
            }

            if (local_err) std::rethrow_exception(local_err);
        }

        /**
         * @lang{ZH}
         * 调整转换器的行为参数。
         *
         * 将 `b` 中描述的行为配置转发给底层 kernel。
         * @endif
         *
         * @lang{EN}
         * Adjust the converter's behavior parameters.
         *
         * Forwards the behavior configuration described in `b` to the underlying kernel.
         * @endif
         *
         * @param b
         * @lang{ZH} 描述期望行为的配置对象。 @endif
         * @lang{EN} A configuration object describing the desired behavior. @endif
         */
        void adjust(const cvt_behavior& b)
        {
            m_kernel.adjust(b);
        }

        /**
         * @lang{ZH}
         * 获取当前转换器的状态信息。
         *
         * 将 kernel 的状态写入 `s` 中。
         * @endif
         *
         * @lang{EN}
         * Retrieve the current converter status information.
         *
         * Writes the kernel's status into `s`.
         * @endif
         *
         * @param s
         * @lang{ZH} 用于接收状态信息的输出对象。 @endif
         * @lang{EN} Output object to receive the status information. @endif
         */
        void retrieve(cvt_status& s) const
        {
            m_kernel.retrieve(s);
        }

        /**
         * @lang{ZH}
         * 启动流，进入 BOS（Beginning-Of-Stream）阶段。
         *
         * 将 IO 状态设置为 kernel 返回的初始状态。此方法只能在 `m_io_status` 为
         * `neutral` 且尚未调用过 `bos()` 时调用，否则抛出异常。
         * 在调用 `main_cont_beg()` 之前，`get`/`put` 将以原始字节模式直接传递数据。
         *
         * @par 设计说明
         * 初始 IO 方向由底层 kernel 基于流是否包含数据来确定，这是为了保护数据完整性：
         * - **流有内容 → 输入模式**：避免意外覆盖现有数据。例如，覆盖 UTF-8 文件的
         *   头部可能导致整个文件无法正确解析。
         * - **流无内容 → 输出模式**：空流没有可读内容，只能写入。
         *
         * 若需覆盖非空文件，应在打开前先清空（truncate）文件，而非强制进入输出模式。
         * @endif
         *
         * @lang{EN}
         * Start the stream and enter the BOS (Beginning-Of-Stream) phase.
         *
         * Sets the IO status to the initial state returned by the kernel. This
         * method may only be called when `m_io_status` is `neutral` and `bos()` has
         * not been called before; otherwise an exception is thrown. Until
         * `main_cont_beg()` is called, `get`/`put` will pass data through in raw
         * byte mode.
         *
         * @par Design Rationale
         * The initial IO direction is determined by the underlying kernel based on
         * whether the stream contains data. This design protects data integrity:
         * - **Stream has content → input mode**: Prevents accidental overwriting of
         *   existing data. For example, overwriting the header of a UTF-8 file could
         *   corrupt the entire file and make it unparseable.
         * - **Stream is empty → output mode**: An empty stream has nothing to read,
         *   so writing is the only meaningful operation.
         *
         * To overwrite a non-empty file, truncate it before opening rather than
         * forcing output mode.
         * @endif
         *
         * @return
         * @lang{ZH} kernel 报告的初始 `io_status` 值。 @endif
         * @lang{EN} The initial `io_status` value reported by the kernel. @endif
         *
         * @throws cvt_error
         * @lang{ZH} 若当前 IO 状态不为 `neutral`，或 `bos()` 已被调用过。 @endif
         * @lang{EN} If the current IO status is not `neutral`, or if `bos()` has already been called. @endif
         */
        io_status bos()
        {
            if (m_io_status != io_status::neutral)
                throw cvt_error("abs_cvt::bos fail: cannot call bos with un-neutral status");
            if (m_is_bos_done)
                throw cvt_error("abs_cvt::bos fail: cannot call bos multiple times");

            m_io_status = m_kernel.bos();
            return m_io_status;
        }

        /**
         * @lang{ZH}
         * 标记 BOS 阶段结束，进入主内容处理阶段。
         *
         * 调用后 `m_is_bos_done` 被置为 `true`，后续 `get`/`put` 将调用派生类的
         * `get_main`/`put_main` 执行实际的编码转换逻辑。
         * @endif
         *
         * @lang{EN}
         * Mark the end of the BOS phase and enter the main-content processing phase.
         *
         * After the call `m_is_bos_done` is set to `true`, and subsequent `get`/`put`
         * calls will invoke the derived class's `get_main`/`put_main` to perform the
         * actual encoding conversion logic.
         * @endif
         */
        void main_cont_beg()
        {
            m_kernel.main_cont_beg();
            m_is_bos_done = true;
        }

        /**
         * @lang{ZH}
         * 检查是否已到达流末尾（EOF）。
         *
         * 仅在 `KernelType` 支持 get 操作时可用。
         * @endif
         *
         * @lang{EN}
         * Check whether the end of the stream (EOF) has been reached.
         *
         * Only available when `KernelType` supports the get operation.
         * @endif
         *
         * @return
         * @lang{ZH} 若已到达流末尾则返回 `true`，否则返回 `false`。 @endif
         * @lang{EN} `true` if the end of the stream has been reached; `false` otherwise. @endif
         */
        bool is_eof()
            requires (cvt_cpt::support_get<KernelType>)
        {
            return m_kernel.is_eof();
        }

    // optional methods
    public:
        /**
         * @lang{ZH}
         * 从流中读取最多 `to_max` 个 `internal_type` 元素。
         *
         * 在 BOS 阶段（`m_is_bos_done == false`），数据以原始字节方式直接读取，
         * 不经过派生类的 `get_main`，以支持文件头等特殊起始数据的读取。
         * BOS 阶段要求数据完整：若在读取足够字节之前遭遇流末尾，视为格式错误
         * 并抛出异常（与 `get_main` 可返回不足数据不同，BOS 阶段是全有或全无）。
         *
         * 进入主阶段后，若当前 IO 方向不是 `input`，且派生类支持 IO 方向切换，
         * 则自动调用 `switch_to_get()`；否则抛出异常。
         *
         * 此方法仅在派生类提供符合签名的 `get_main` 时通过 requires 子句启用。
         * @endif
         *
         * @lang{EN}
         * Read up to `to_max` elements of `internal_type` from the stream.
         *
         * During the BOS phase (`m_is_bos_done == false`), data is read as raw bytes
         * without going through the derived class's `get_main`, enabling reading of
         * stream-leading data such as file headers. The BOS phase requires complete
         * data: if end-of-stream is reached before enough bytes are available, it is
         * treated as a format error and an exception is thrown (unlike `get_main`,
         * which may return partial data, the BOS phase is all-or-nothing).
         *
         * Once in the main phase, if the current IO direction is not `input` and the
         * derived class supports IO-direction switching, `switch_to_get()` is called
         * automatically; otherwise an exception is thrown.
         *
         * This method is only enabled via a `requires` clause when the derived class
         * provides a conforming `get_main` member function.
         * @endif
         *
         * @param to
         * @lang{ZH} 指向写入结果的目标缓冲区的指针，至少能容纳 `to_max` 个元素。 @endif
         * @lang{EN} Pointer to the destination buffer to write results into; must have capacity for at least `to_max` elements. @endif
         *
         * @param to_max
         * @lang{ZH} 期望读取的最大元素数量。 @endif
         * @lang{EN} Maximum number of elements to read. @endif
         *
         * @return
         * @lang{ZH} 实际读取的元素数量，可能少于 `to_max`（BOS 阶段除外，其返回值等于 `to_max`）。 @endif
         * @lang{EN} Actual number of elements read; may be less than `to_max` (except during the BOS phase, which always returns `to_max`). @endif
         *
         * @throws cvt_error
         * @lang{ZH}
         * 若 BOS 阶段在读取足够数据之前遭遇流末尾；或主阶段无法切换到 input 方向。
         * @endif
         * @lang{EN}
         * If the BOS phase encounters end-of-stream before reading enough data; or if
         * the main phase cannot switch to input direction.
         * @endif
         */
        size_t get(internal_type* to, size_t to_max)
            requires requires(CurrentType& t, cvt_reader<KernelType>& r, internal_type* data, size_t len) {
                { t.get_main(r, data, len) } -> std::same_as<size_t>;
            }
        {
            // Read failures do not taint: they only advance the kernel cursor
            // and possibly leave a per-call mbstate undefined; the underlying
            // stream content is unchanged, and callers can reseek to recover.
            // We still honour any pre-existing taint so that a writer that
            // corrupted the stream cannot then read back a confused image.
            assert_not_tainted();

            cvt_reader<KernelType> reader(m_kernel, m_tmp_io_buffer);

            if (!m_is_bos_done)
            {
                if (to_max == 0) return 0;

                constexpr size_t ext_size = sizeof(external_type);

                reader.reset(s_bos_chunk);
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                char* dest_bytes = reinterpret_cast<char*>(to);
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                const char* dest_bytes_end = reinterpret_cast<const char*>(to + to_max);

                // BOS phase requires complete data: if the stream ends unexpectedly
                // before providing enough bytes, this is a format error and we throw.
                // Unlike get_main() which may return partial data, BOS is all-or-nothing.

                // Read full external_type chunks
                while (dest_bytes + ext_size <= dest_bytes_end)
                {
                    const size_t units_to_read_now = std::min((size_t)((dest_bytes_end - dest_bytes) / ext_size), s_bos_chunk);

                    const auto* src_buf = reader.template get_buf<true>(units_to_read_now);
                    std::memcpy(dest_bytes, src_buf, units_to_read_now * ext_size);
                    dest_bytes += units_to_read_now * ext_size;
                }

                // Read the final partial internal_type if needed
                const size_t final_remainder_bytes = dest_bytes_end - dest_bytes;
                if (final_remainder_bytes > 0)
                {
                    const auto* src_buf = reader.template get_buf<true>(1);
                    std::memcpy(dest_bytes, src_buf, final_remainder_bytes);
                    dest_bytes += final_remainder_bytes;
                }

                return to_max;
            }
            else
            {
                if (m_io_status != io_status::input)
                {
                    if constexpr (cvt_cpt::support_io_switch<CurrentType>)
                        static_cast<CurrentType*>(this)->switch_to_get();
                    else
                        throw cvt_error("abs_cvt::get fail: cannot switch to input mode");
                }
                return static_cast<CurrentType*>(this)->get_main(reader, to, to_max);
            }
        }

        /**
         * @lang{ZH}
         * 向流中写入 `to_size` 个 `internal_type` 元素。
         *
         * 在 BOS 阶段（`m_is_bos_done == false`），数据以原始字节方式直接写出，
         * 不经过派生类的 `put_main`，以支持文件头等特殊起始数据的写入。
         * BOS 阶段以 `sizeof(external_type)` 为单位分块写出完整的 `external_type`
         * 元素；若最后一个单元不足 `sizeof(external_type)` 字节，则以零填充。
         *
         * 进入主阶段后，若当前 IO 方向不是 `output`，且派生类支持 IO 方向切换，
         * 则自动调用 `switch_to_put()`；否则抛出异常。
         *
         * 此方法仅在派生类提供符合签名的 `put_main` 时通过 requires 子句启用。
         * @endif
         *
         * @lang{EN}
         * Write `to_size` elements of `internal_type` to the stream.
         *
         * During the BOS phase (`m_is_bos_done == false`), data is written as raw
         * bytes without going through the derived class's `put_main`, enabling
         * writing of stream-leading data such as file headers. The BOS phase writes
         * complete `external_type` units (`sizeof(external_type)` bytes each); if
         * the final unit is smaller than `sizeof(external_type)` bytes, it is
         * zero-padded.
         *
         * Once in the main phase, if the current IO direction is not `output` and the
         * derived class supports IO-direction switching, `switch_to_put()` is called
         * automatically; otherwise an exception is thrown.
         *
         * This method is only enabled via a `requires` clause when the derived class
         * provides a conforming `put_main` member function.
         * @endif
         *
         * @param to
         * @lang{ZH} 指向要写入数据的源缓冲区的指针，至少包含 `to_size` 个元素。 @endif
         * @lang{EN} Pointer to the source data buffer; must contain at least `to_size` elements. @endif
         *
         * @param to_size
         * @lang{ZH} 要写入的元素数量。 @endif
         * @lang{EN} Number of elements to write. @endif
         *
         * @throws cvt_error
         * @lang{ZH} 若主阶段无法切换到 output 方向。 @endif
         * @lang{EN} If the main phase cannot switch to output direction. @endif
         */
        void put(const internal_type* to, size_t to_size)
            requires requires(CurrentType& t, cvt_writer<KernelType>& w, const internal_type* data, size_t len) {
                { t.put_main(w, data, len) } -> std::same_as<void>;
            }
        {
            assert_not_tainted();

            // IO-direction switching does not write bytes to the output stream:
            // a failed switch_to_put() leaves the stream unchanged, so the
            // converter is not corrupted and must not be tainted.  Perform the
            // switch BEFORE entering the taint-on-exception region so that only
            // actual kernel writes can cause tainting.
            if (m_is_bos_done && m_io_status != io_status::output)
            {
                if constexpr (cvt_cpt::support_io_switch<CurrentType>)
                    static_cast<CurrentType*>(this)->switch_to_put();
                else
                    throw cvt_error("abs_cvt::put fail: cannot switch to output mode");
            }

            try
            {
                cvt_writer<KernelType> writer(m_kernel, m_tmp_io_buffer);
                if (!m_is_bos_done)
                {
                    if (to_size == 0) return;

                    constexpr size_t ext_size = sizeof(external_type);

                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    auto to_bytes = reinterpret_cast<const char*>(to);
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    auto to_bytes_end = reinterpret_cast<const char*>(to + to_size);

                    // BOS phase writes complete external_type units. If the input data
                    // is not a multiple of ext_size, pad the final unit with zeros.

                    writer.reset(s_bos_chunk);
                    while (to_bytes + ext_size <= to_bytes_end)
                    {
                        auto dest_count = std::min<size_t>((to_bytes_end - to_bytes) / ext_size, s_bos_chunk);
                        auto ptr = writer.put_buf(dest_count);
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                        std::memcpy(reinterpret_cast<char*>(ptr), to_bytes, dest_count * ext_size);
                        to_bytes += dest_count * ext_size;
                    }

                    if (to_bytes < to_bytes_end)
                    {
                        // The while loop above guarantees remaining < ext_size.
                        // The modulo is logically redundant but helps the compiler's
                        // static analyzer prove the bound for memset size calculation.
                        size_t remaining = static_cast<size_t>(to_bytes_end - to_bytes) % ext_size;
                        auto ptr = writer.put_buf(1);
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                        std::memcpy(reinterpret_cast<char*>(ptr), to_bytes, remaining);
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                        std::memset(reinterpret_cast<char*>(ptr) + remaining, 0, ext_size - remaining);
                    }

                    writer.commit();
                }
                else
                {
                    // switch_to_put() was already called above if needed;
                    // m_io_status is guaranteed to be output at this point.
                    static_cast<CurrentType*>(this)->put_main(writer, to, to_size);
                    // commit() is the sole responsibility of abs_cvt::put: derived
                    // put_main implementations must NOT call writer.commit() themselves.
                    // If put_main throws, this commit is skipped and any uncommitted
                    // buffer contents are dropped at writer destruction; the catch
                    // below taints the converter.
                    writer.commit();
                }
            }
            catch (...)
            {
                m_is_tainted = true;
                throw;
            }
        }

        /**
         * @lang{ZH}
         * 将所有缓冲数据强制刷出到底层设备。
         *
         * 仅在派生类实现了 `put_main` 时可用。若当前不处于输出模式，则为空操作直接返回。
         *
         * 若派生类实现了 `flush_impl()`，则先调用 `flush_impl()` 执行派生类特定的刷出逻辑
         * （例如 zlib 的 sync flush），再调用 `m_kernel.flush()` 将数据推送至底层设备。
         * 若派生类未实现 `flush_impl()`，则直接调用 `m_kernel.flush()`。
         *
         * 若刷出过程中抛出异常，转换器将进入污染状态，后续操作均不可用。
         * @endif
         *
         * @lang{EN}
         * Force all buffered data to be flushed to the underlying device.
         *
         * Only available when the derived class implements `put_main`. If the converter
         * is not currently in output mode, this is a no-op and returns immediately.
         *
         * If the derived class implements `flush_impl()`, it is called first to perform
         * any derived-class-specific flushing (e.g., a zlib sync flush), after which
         * `m_kernel.flush()` is called to push data through to the underlying device.
         * If no `flush_impl()` is provided, `m_kernel.flush()` is called directly.
         *
         * If an exception is thrown during flushing, the converter enters a tainted
         * state and further operations are unavailable.
         * @endif
         */
        void flush()
            requires requires(CurrentType& t, cvt_writer<KernelType>& w, const internal_type* data, size_t len) {
                { t.put_main(w, data, len) } -> std::same_as<void>;
            }
        {
            if (m_io_status != io_status::output)
                return;

            assert_not_tainted();
            try
            {
                if constexpr (requires(CurrentType& t) { t.flush_impl(); })
                    static_cast<CurrentType*>(this)->flush_impl();
                m_kernel.flush();
            }
            catch (...)
            {
                m_is_tainted = true;
                throw;
            }
        }

        /**
         * @lang{ZH}
         * 返回当前流位置（以 `external_type` 元素为单位）。
         *
         * 仅在 `default_positioning` 为 `true` 且 kernel 支持定位时可用。
         * @endif
         *
         * @lang{EN}
         * Return the current stream position in units of `external_type` elements.
         *
         * Only available when `default_positioning` is `true` and the kernel
         * supports positioning.
         * @endif
         *
         * @return
         * @lang{ZH} 当前流位置（从流起始处的 `external_type` 元素偏移量）。 @endif
         * @lang{EN} The current stream position as an offset in `external_type` elements from the start of the stream. @endif
         */
        [[nodiscard]] size_t tell() const
            requires (default_positioning && cvt_cpt::support_positioning<KernelType>)
        {
            return m_kernel.tell();
        }

        /**
         * @lang{ZH}
         * 将流位置移动到绝对位置 `pos`（从流起始处计算，以 `external_type` 元素为单位）。
         *
         * 仅在 `default_positioning` 为 `true` 且 kernel 支持定位时可用。
         * @endif
         *
         * @lang{EN}
         * Seek to absolute position `pos` measured from the start of the stream in
         * units of `external_type` elements.
         *
         * Only available when `default_positioning` is `true` and the kernel
         * supports positioning.
         * @endif
         *
         * @param pos
         * @lang{ZH} 目标绝对位置（从流起始处的 `external_type` 元素偏移量）。 @endif
         * @lang{EN} Target absolute position as an offset in `external_type` elements from the start of the stream. @endif
         */
        void seek(size_t pos)
            requires (default_positioning && cvt_cpt::support_positioning<KernelType>)
        {
            m_kernel.seek(pos);
        }

        /**
         * @lang{ZH}
         * 将流位置移动到反向绝对位置 `pos`（从流末尾处向前计算，以 `external_type` 元素为单位）。
         *
         * 仅在 `default_positioning` 为 `true` 且 kernel 支持定位时可用。
         * @endif
         *
         * @lang{EN}
         * Seek to reverse absolute position `pos` measured backwards from the end
         * of the stream in units of `external_type` elements.
         *
         * Only available when `default_positioning` is `true` and the kernel
         * supports positioning.
         * @endif
         *
         * @param pos
         * @lang{ZH} 目标反向位置（从流末尾向前的 `external_type` 元素偏移量）。 @endif
         * @lang{EN} Target reverse position as an offset in `external_type` elements measured backwards from the end of the stream. @endif
         */
        void rseek(size_t pos)
            requires (default_positioning && cvt_cpt::support_positioning<KernelType>)
        {
            m_kernel.rseek(pos);
        }

        /**
         * @lang{ZH}
         * 将 IO 方向切换为输入（读取）模式，并将 `m_io_status` 更新为 `input`。
         *
         * 仅在 `default_io_switch` 为 `true` 且 kernel 支持 IO 方向切换时可用。
         * @endif
         *
         * @lang{EN}
         * Switch the IO direction to input (read) mode and update `m_io_status`
         * to `input`.
         *
         * Only available when `default_io_switch` is `true` and the kernel supports
         * IO-direction switching.
         * @endif
         */
        void switch_to_get()
            requires (default_io_switch && cvt_cpt::support_io_switch<KernelType>)
        {
            m_kernel.switch_to_get();
            m_io_status = io_status::input;
        }

        /**
         * @lang{ZH}
         * 将 IO 方向切换为输出（写入）模式，并将 `m_io_status` 更新为 `output`。
         *
         * 仅在 `default_io_switch` 为 `true` 且 kernel 支持 IO 方向切换时可用。
         * @endif
         *
         * @lang{EN}
         * Switch the IO direction to output (write) mode and update `m_io_status`
         * to `output`.
         *
         * Only available when `default_io_switch` is `true` and the kernel supports
         * IO-direction switching.
         * @endif
         */
        void switch_to_put()
            requires (default_io_switch && cvt_cpt::support_io_switch<KernelType>)
        {
            m_kernel.switch_to_put();
            m_io_status = io_status::output;
        }

    protected:
        // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
        /** @lang{ZH} 底层 IO 转换核心，持有对底层设备的所有权。 @endif
         *  @lang{EN} The underlying IO conversion kernel, which owns the underlying device. @endif */
        KernelType  m_kernel;

        /** @lang{ZH}
         *  当前 IO 方向状态：`neutral`（未激活）、`input`（读取模式）或 `output`（写入模式）。
         *  由 `bos()`、`switch_to_get()`、`switch_to_put()`、`detach()` 和 `attach()` 更新。
         *  @endif
         *  @lang{EN}
         *  Current IO direction state: `neutral` (inactive), `input` (read mode), or `output` (write mode).
         *  Updated by `bos()`, `switch_to_get()`, `switch_to_put()`, `detach()`, and `attach()`.
         *  @endif */
        io_status   m_io_status = io_status::neutral;

        /** @lang{ZH}
         *  标记 BOS 阶段是否已完成。`false` 表示仍处于 BOS 阶段（直接传递原始字节）；
         *  `true` 表示已进入主内容阶段（调用派生类的 `get_main`/`put_main`）。
         *  由 `main_cont_beg()` 置为 `true`，由 `detach()` 和 `attach()` 重置为 `false`。
         *  @endif
         *  @lang{EN}
         *  Indicates whether the BOS phase has been completed. `false` means still in the
         *  BOS phase (raw byte passthrough); `true` means the main-content phase has been
         *  entered (invoking the derived class's `get_main`/`put_main`).
         *  Set to `true` by `main_cont_beg()`; reset to `false` by `detach()` and `attach()`.
         *  @endif */
        bool        m_is_bos_done = false;
        // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

        /**
         * @lang{ZH}
         * 若当前转换器已被标记为 tainted，则抛出 `cvt_error`；否则什么也不做。
         *
         * 派生类应在依赖底层 kernel 状态完整性的操作（如定位、IO 方向切换等）
         * 入口处调用此函数，以快速失败、阻止在已损坏的状态上继续操作。
         * @endif
         *
         * @lang{EN}
         * Throws `cvt_error` if the converter has been marked tainted; otherwise
         * does nothing.
         *
         * Derived classes should call this at the entry of any operation whose
         * correctness depends on the kernel being in a known-good state — e.g.,
         * positioning or IO-direction switching — so that further use after a
         * partial-failure is refused fast rather than silently corrupting data.
         * @endif
         *
         * @throws cvt_error
         * @lang{ZH} 若 `m_is_tainted` 为 `true`。 @endif
         * @lang{EN} If `m_is_tainted` is `true`. @endif
         */
        void assert_not_tainted() const
        {
            if (m_is_tainted)
                throw cvt_error("abs_cvt: converter is in a tainted state; reattach a device to recover");
        }

        /**
         * @lang{ZH}
         * 将当前转换器标记为 tainted。一旦置位，后续依赖完整内部状态的操作将
         * 通过 `assert_not_tainted()` 立即抛出 `cvt_error`；唯一的恢复路径是
         * 调用 `attach()` 或 `detach()`（二者均会重置该标志）。
         *
         * 派生类应在某个变更操作中途失败、对象处于不一致状态时调用此函数，
         * 从而把"已损坏但尚未销毁"的对象与正常对象区分开。
         *
         * 注意：本函数仅置 `m_is_tainted = true`，不会触碰任何其他状态字段
         * （包括 `m_io_status` 和 `m_is_bos_done`）。这样做避免任何潜在的
         * 副作用——错误路径上越少的状态写入越好。
         * @endif
         *
         * @lang{EN}
         * Marks the current converter as tainted. Once set, any subsequent
         * operation that relies on a known-good internal state will throw
         * `cvt_error` via `assert_not_tainted()`; the only recovery path is
         * to call `attach()` or `detach()`, both of which clear the flag.
         *
         * Derived classes should call this when a mutating operation has
         * failed midway and the object is left in an inconsistent state, so
         * that "broken-but-not-destroyed" objects are distinguished from
         * healthy ones.
         *
         * Note: this function only sets `m_is_tainted = true`; it deliberately
         * does not touch any other state field (including `m_io_status` and
         * `m_is_bos_done`). The fewer writes on the error path, the fewer
         * potential side effects.
         * @endif
         */
        void set_tainted() noexcept
        {
            m_is_tainted = true;
        }

    private:
        /** @lang{ZH}
         *  临时 IO 缓冲区，供 `cvt_reader` 和 `cvt_writer` 在每次 `get`/`put` 调用时共用。
         *  缓冲区大小在首次使用时由 `reset()` 按需设置。
         *  @endif
         *  @lang{EN}
         *  Temporary IO buffer shared by `cvt_reader` and `cvt_writer` across each `get`/`put` call.
         *  The buffer is sized on demand by `reset()` at first use.
         *  @endif */
        std::vector<external_type> m_tmp_io_buffer;

        /** @lang{ZH}
         *  Tainted 标志：当 `put` 抛出异常时被置为 `true`，表示底层流的字节
         *  序列可能已被部分写入（auto-flush 已写入的字节无法回退）。一旦置位，
         *  后续的 `put`/`get` 会立即抛出，派生类亦可通过 `assert_not_tainted()`
         *  在其它操作入口处显式拒绝执行。由 `attach()`/`detach()` 重置为 `false`。
         *  注意：`get` 失败本身不会污染该标志。
         *  @endif
         *  @lang{EN}
         *  Tainted flag: set to `true` whenever `put` propagates an exception.
         *  Indicates that the underlying byte stream may have been partially
         *  written (auto-flushed bytes are not rollback-able). Once set,
         *  further `put`/`get` calls fail fast; derived classes may also
         *  query it via `assert_not_tainted()` at the entry of other
         *  state-dependent operations. Reset to `false` by `attach()`/`detach()`.
         *  Note: failures inside `get` do NOT set this flag on their own.
         *  @endif */
        bool m_is_tainted = false;
    };
}

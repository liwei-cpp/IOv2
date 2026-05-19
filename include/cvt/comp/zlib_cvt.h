/**
 * @file zlib_cvt.h
 * @lang{ZH}
 * zlib 压缩/解压转换器定义文件。
 * 本文件实现了基于 zlib 的 deflate（压缩）和 inflate（解压）转换器 `zlib_cvt`，
 * 以及用于流式管道构造的工厂类 `zlib_cvt_creator`。
 * `zlib_cvt` 继承自 `abs_cvt`（禁用默认定位与 IO 方向切换），BOS 阶段负责读写
 * 2 字节的 zlib 流头，主内容阶段通过 `get_main`/`put_main` 执行实际的解压/压缩。
 * @endif
 *
 * @lang{EN}
 * zlib compression/decompression converter definition file.
 * This file implements `zlib_cvt`, a deflate (compression) and inflate (decompression)
 * converter based on zlib, together with `zlib_cvt_creator`, a factory class for
 * constructing it in streaming pipelines.
 * `zlib_cvt` derives from `abs_cvt` with default positioning and IO-direction switching
 * both disabled. During the BOS phase it reads or writes the 2-byte zlib stream header;
 * the main-content phase performs actual decompression/compression through
 * `get_main`/`put_main`.
 * @endif
 */
#pragma once

#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <zlib.h>

namespace IOv2::Comp
{
/**
 * @lang{ZH}
 * 控制 `zlib_cvt::flush()` 是否触发 `Z_SYNC_FLUSH` 语义的行为策略类。
 *
 * `zlib_cvt` 的默认行为是 `sync_flush == false`，此时 `flush()` 不向下游写入任何
 * 同步标记——先前 `put()` 已送入 zlib 但尚未完成 LZ77 窗口匹配的数据仍保留在
 * zlib 内部缓冲区中，等到 `close_stream` 以 `Z_FINISH` 最终化时才写出。
 *
 * 默认采用此语义是有意为之：`Z_SYNC_FLUSH` 并非免费——每次调用会插入一个
 * 5 字节的空存储块（`00 00 00 FF FF`）作为流同步点，并强制 zlib 放弃尚在
 * 优化中的 LZ77 部分匹配，频繁同步刷会使压缩率趋近 1:1。不需要流级同步的
 * 调用方不应承担这一开销。
 *
 * 当确实需要同步点时，可通过 `cvt.adjust(zlib_sync_flush{true})` 启用：
 * - 通过网络流式传输，接收方需要在明确边界处开始解压；
 * - 需要字节级延迟的交互式工具（以压缩率换取低延迟）；
 * - 任何下游可能在流中途开始解压的分帧场景。
 * @endif
 *
 * @lang{EN}
 * Behavior policy class that controls whether `zlib_cvt::flush()` triggers
 * `Z_SYNC_FLUSH` semantics.
 *
 * The default for `zlib_cvt` is `sync_flush == false`, in which case `flush()` does
 * NOT emit a sync marker — data that has been passed to `deflate()` on previous `put()`
 * calls but is still inside zlib's LZ77 window stays buffered until `close_stream`
 * finalizes the stream with `Z_FINISH`.
 *
 * This default is deliberate: `Z_SYNC_FLUSH` is not free. Each invocation inserts a
 * 5-byte empty stored block (`00 00 00 FF FF`) as a stream sync point AND forces zlib
 * to abandon partial LZ77 matches it was still trying to optimize — frequent
 * sync-flushing can degrade the compression ratio toward 1:1. Callers who do not need
 * stream-level synchronization should not pay that cost.
 *
 * Enable it via `cvt.adjust(zlib_sync_flush{true})` when synchronization points are
 * required:
 * - Streaming over a network where the receiver must start decompression at
 *   well-defined boundaries;
 * - Interactive tools that need byte-level latency at the cost of compression ratio;
 * - Any framing where downstream may begin decompression mid-stream.
 * @endif
 */
struct zlib_sync_flush : cvt_behavior
{
    explicit zlib_sync_flush(bool sync_flush)
        : m_sync_flush(sync_flush) {}

    bool m_sync_flush;
};

/**
 * @lang{ZH}
 * 基于 zlib 的压缩/解压转换器。
 *
 * 继承自 `abs_cvt`，将 `default_positioning` 和 `default_io_switch` 均设为 `false`：
 * 压缩流具有状态性，既不支持随机定位，也不支持在压缩和解压方向之间切换。
 *
 * **BOS 阶段**（`bos()` 后、`main_cont_beg()` 前）：
 * - 输出（压缩）模式：调用 `deflateInit`，立即以 `Z_NO_FLUSH` 触发 zlib 输出
 *   2 字节流头，并通过内核写出。
 * - 输入（解压）模式：调用 `inflateInit`，从内核读取 2 字节流头并送入 `inflate`
 *   以完成头部验证。
 *
 * **主内容阶段**（`main_cont_beg()` 后）：
 * - 压缩：`put_main` 调用 `deflate(Z_NO_FLUSH)` 将用户数据压缩后写入内核。
 * - 解压：`get_main` 调用 `inflate(Z_NO_FLUSH)` 从内核读取压缩数据并解压给调用方。
 *
 * **流的终结**（`close_stream()`）：
 * - 压缩：调用 `deflate(Z_FINISH)` 循环直到 `Z_STREAM_END`，将所有剩余压缩字节
 *   写入内核，最后调用 `deflateEnd`。
 * - 解压：直接调用 `inflateEnd`。
 *
 * @tparam KernelType 内核转换器类型，须满足 `io_converter`；其 `internal_type` 的
 *                   大小须等于 `unsigned char`（即字节级内核）。
 *                   / The kernel converter type, must satisfy `io_converter`; its
 *                   `internal_type` must have the same size as `unsigned char` (byte-level kernel).
 * @tparam TInt       本转换器对外暴露的内部类型（即 `get`/`put` 接口的元素类型）。
 *                   须满足 `std::is_trivially_copyable`。默认等于内核的 `internal_type`。
 *                   / The internal type exposed by this converter (the element type of the
 *                   `get`/`put` interface). Must satisfy `std::is_trivially_copyable`.
 *                   Defaults to the kernel's `internal_type`.
 * @endif
 *
 * @lang{EN}
 * zlib-based compression/decompression converter.
 *
 * Derives from `abs_cvt` with both `default_positioning` and `default_io_switch` set
 * to `false`: compressed streams are stateful and support neither random seeking nor
 * switching between compression and decompression directions.
 *
 * **BOS phase** (after `bos()`, before `main_cont_beg()`):
 * - Output (compression) mode: calls `deflateInit`, immediately triggers a `Z_NO_FLUSH`
 *   call to make zlib emit the 2-byte stream header, and writes it through the kernel.
 * - Input (decompression) mode: calls `inflateInit`, reads the 2-byte stream header
 *   from the kernel, and feeds it to `inflate` for header validation.
 *
 * **Main-content phase** (after `main_cont_beg()`):
 * - Compression: `put_main` calls `deflate(Z_NO_FLUSH)` to compress user data and
 *   write it through the kernel.
 * - Decompression: `get_main` calls `inflate(Z_NO_FLUSH)` to read compressed data
 *   from the kernel and decompress it for the caller.
 *
 * **Stream finalization** (`close_stream()`):
 * - Compression: loops `deflate(Z_FINISH)` until `Z_STREAM_END`, flushing all
 *   remaining compressed bytes through the kernel, then calls `deflateEnd`.
 * - Decompression: calls `inflateEnd` directly.
 *
 * @tparam KernelType The kernel converter type, must satisfy `io_converter`; its
 *                   `internal_type` must have the same size as `unsigned char`
 *                   (byte-level kernel).
 * @tparam TInt       The internal type exposed by this converter (the element type of
 *                   the `get`/`put` interface). Must satisfy `std::is_trivially_copyable`.
 *                   Defaults to the kernel's `internal_type`.
 * @endif
 */
template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (sizeof(typename KernelType::internal_type) == sizeof(unsigned char) &&
              std::is_trivially_copyable_v<TInt>)
class zlib_cvt : public abs_cvt<zlib_cvt<KernelType, TInt>, KernelType, TInt, false, false>
{
    using BT = abs_cvt<zlib_cvt<KernelType, TInt>, KernelType, TInt, false, false>;
    friend BT;  // for put_main, get_main, and private CRTP hooks

    /**
     * @lang{ZH}
     * `inflateInit` 失败路径上的 RAII 守卫。
     * 若在 `inflateInit` 成功后、流被正式接管前发生异常，析构函数自动调用
     * `inflateEnd` 并将 `m_strm` 重置为 `nullptr`，确保 `close_stream`
     * 和析构函数在后续调用中看到空指针并跳过重复清理。
     * 成功路径上调用 `release()` 可阻止守卫执行清理（流转由 `m_strm` 管理）。
     * @endif
     *
     * @lang{EN}
     * RAII guard for the `inflateInit` failure path.
     * If an exception occurs after `inflateInit` succeeds but before the stream is
     * formally taken over, the destructor automatically calls `inflateEnd` and resets
     * `m_strm` to `nullptr`, ensuring that subsequent calls to `close_stream` and the
     * destructor see a null pointer and skip redundant cleanup.
     * Calling `release()` on the success path prevents the guard from running cleanup
     * (ownership is retained by `m_strm`).
     * @endif
     */
    struct inflate_guard
    {
        std::unique_ptr<z_stream>& strm_ref;
        bool released = false;

        explicit inflate_guard(std::unique_ptr<z_stream>& s) noexcept : strm_ref(s) {}
        ~inflate_guard() noexcept
        {
            if (!released && strm_ref)
            {
                inflateEnd(strm_ref.get());
                strm_ref.reset();
            }
        }
        void release() noexcept { released = true; }

        inflate_guard(const inflate_guard&) = delete;
        inflate_guard& operator=(const inflate_guard&) = delete;
        inflate_guard(inflate_guard&&) = delete;
        inflate_guard& operator=(inflate_guard&&) = delete;
    };

    /**
     * @lang{ZH}
     * `deflateInit` 失败路径上的 RAII 守卫。
     * 若在 `deflateInit` 成功后、流被正式接管前发生异常，析构函数自动调用
     * `deflateEnd` 并将 `m_strm` 重置为 `nullptr`，确保 `close_stream`
     * 和析构函数在后续调用中看到空指针并跳过重复清理。
     * 成功路径上调用 `release()` 可阻止守卫执行清理（流转由 `m_strm` 管理）。
     * @endif
     *
     * @lang{EN}
     * RAII guard for the `deflateInit` failure path.
     * If an exception occurs after `deflateInit` succeeds but before the stream is
     * formally taken over, the destructor automatically calls `deflateEnd` and resets
     * `m_strm` to `nullptr`, ensuring that subsequent calls to `close_stream` and the
     * destructor see a null pointer and skip redundant cleanup.
     * Calling `release()` on the success path prevents the guard from running cleanup
     * (ownership is retained by `m_strm`).
     * @endif
     */
    struct deflate_guard
    {
        std::unique_ptr<z_stream>& strm_ref;
        bool released = false;

        explicit deflate_guard(std::unique_ptr<z_stream>& s) noexcept : strm_ref(s) {}
        ~deflate_guard() noexcept
        {
            if (!released && strm_ref)
            {
                deflateEnd(strm_ref.get());
                strm_ref.reset();
            }
        }
        void release() noexcept { released = true; }

        deflate_guard(const deflate_guard&) = delete;
        deflate_guard& operator=(const deflate_guard&) = delete;
        deflate_guard(deflate_guard&&) = delete;
        deflate_guard& operator=(deflate_guard&&) = delete;
    };

    /**
     * @lang{ZH}
     * 作用域退出时将 `m_strm` 的输入/输出指针和计数清零的 RAII 守卫。
     * 确保 `get_main`、`put_main`、`flush_impl` 抛出异常时，`m_strm` 不会
     * 持有指向已离开作用域的读缓冲区、写缓冲区或用户缓冲区的悬空指针。
     * 不触及 `m_strm` 自身——zlib 流对象的清理由 `close_stream`、
     * `inflate_guard` 和 `deflate_guard` 负责。
     * @endif
     *
     * @lang{EN}
     * RAII guard that clears `m_strm`'s input/output pointers and counts on scope exit.
     * Ensures that when `get_main`, `put_main`, or `flush_impl` throw an exception,
     * `m_strm` is not left holding dangling pointers into reader, writer, or user
     * buffers that have since gone out of scope.
     * Does NOT touch `m_strm` itself — cleanup of the zlib stream object is the
     * responsibility of `close_stream`, `inflate_guard`, and `deflate_guard`.
     * @endif
     */
    struct io_buf_guard
    {
        z_stream& strm;
        explicit io_buf_guard(z_stream& s) noexcept : strm(s) {}
        ~io_buf_guard() noexcept
        {
            strm.next_in  = nullptr;
            strm.avail_in = 0;
            strm.next_out = nullptr;
            strm.avail_out = 0;
        }
        io_buf_guard(const io_buf_guard&) = delete;
        io_buf_guard& operator=(const io_buf_guard&) = delete;
        io_buf_guard(io_buf_guard&&) = delete;
        io_buf_guard& operator=(io_buf_guard&&) = delete;
    };

public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

private:
    /// @lang{ZH} deflate/inflate 输出缓冲区的最小字节容量，至少为 16 且不小于 sizeof(internal_type)。 @endif
    /// @lang{EN} Minimum byte capacity of the deflate/inflate output buffer; at least 16 and no less than sizeof(internal_type). @endif
    constexpr static unsigned CHUNK = std::max<unsigned>(16, sizeof(internal_type));

    /// @lang{ZH} zlib 流头的字节长度（固定为 2 字节）。 @endif
    /// @lang{EN} Byte length of the zlib stream header (fixed at 2 bytes). @endif
    constexpr static size_t zlib_header_size = 2;

public:
    /**
     * @lang{ZH}
     * 从内核和压缩等级构造 zlib 转换器。
     * 压缩等级超过 9 时自动截断为 9；解压方向忽略此参数（等级仅影响 `deflateInit`）。
     * @endif
     *
     * @lang{EN}
     * Construct the zlib converter from a kernel and compression level.
     * A level greater than 9 is silently clamped to 9; the level is ignored on the
     * decompression path as it only affects `deflateInit`.
     * @endif
     *
     * @param kernel    底层内核转换器（移动接管所有权）。 / The underlying kernel converter (ownership transferred via move).
     * @param put_level deflate 压缩等级（0–9，默认 6）。 / deflate compression level (0–9, default 6).
     */
    zlib_cvt(KernelType kernel, unsigned put_level = 6)
        : BT(std::move(kernel))
        , m_put_level(put_level)
    {
        if (m_put_level > 9) m_put_level = 9;
    }

    /**
     * @lang{ZH}
     * 拷贝构造函数。复制内核、压缩等级、`m_sync_flush` 及 `m_stream_ended` 标志。
     * 若当前处于输出模式，调用 `deflateCopy` 深拷贝 zlib deflate 状态；
     * 若处于输入模式，调用 `inflateCopy` 深拷贝 inflate 状态。
     * 若处于 `neutral` 模式，则无需拷贝 zlib 状态（`m_strm` 为空）。
     * `deflateCopy`/`inflateCopy` 抛出异常时，`m_strm` 被重置、IO 状态恢复为
     * `neutral`，确保对象处于一致状态。
     * @endif
     *
     * @lang{EN}
     * Copy constructor. Copies the kernel, compression level, `m_sync_flush`,
     * and `m_stream_ended` flag.
     * If currently in output mode, calls `deflateCopy` to deep-copy the deflate state;
     * if in input mode, calls `inflateCopy` to deep-copy the inflate state.
     * In `neutral` mode no zlib state needs to be copied (`m_strm` is null).
     * If `deflateCopy`/`inflateCopy` throws, `m_strm` is reset and IO status is
     * restored to `neutral`, leaving the object in a consistent state.
     * @endif
     */
    zlib_cvt(const zlib_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_put_level(val.m_put_level)
        , m_sync_flush(val.m_sync_flush)
        , m_stream_ended(val.m_stream_ended)
    {
        try
        {
            if (BT::m_io_status == io_status::output)
            {
                m_strm = std::make_unique<z_stream>();
                zerr("zlib_cvt copy constructor fail", deflateCopy(m_strm.get(), val.m_strm.get())); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            }
            else if (BT::m_io_status == io_status::input)
            {
                m_strm = std::make_unique<z_stream>();
                zerr("zlib_cvt copy constructor fail", inflateCopy(m_strm.get(), val.m_strm.get())); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            }
        }
        catch(...)
        {
            m_strm.reset();
            BT::m_io_status = io_status::neutral;
            m_stream_ended = false;
            throw;
        }
    }

    /**
     * @lang{ZH}
     * 移动构造函数。转移 `m_strm` 的 `unique_ptr` 所有权，使 `z_stream` 对象的
     * 地址保持不变。zlib 内部状态通过 `state->strm` 反向引用 `z_stream` 结构体；
     * 若浅拷贝结构体会使该指针悬空，`deflate`/`inflate` 将返回 `Z_STREAM_ERROR`。
     * 转移 `unique_ptr` 可保留原地址，使反向指针持续有效。
     * @endif
     *
     * @lang{EN}
     * Move constructor. Transfers ownership of the `m_strm` `unique_ptr`, keeping
     * the address of the `z_stream` object unchanged. zlib's internal state holds a
     * back-pointer (`state->strm`) to the `z_stream` struct; a shallow struct copy
     * would leave that pointer stale, causing `deflate`/`inflate` to return
     * `Z_STREAM_ERROR`. Transferring the `unique_ptr` preserves the address and
     * keeps the back-pointer valid.
     * @endif
     */
    zlib_cvt(zlib_cvt&& val) noexcept
        : BT(std::move(val))
        , m_put_level(val.m_put_level)
        , m_sync_flush(val.m_sync_flush)
        , m_stream_ended(val.m_stream_ended)
        , m_strm(std::move(val.m_strm))
    {}

    /**
     * @lang{ZH}
     * 拷贝赋值运算符。
     * 先关闭旧流（捕获可能的错误），再拷贝新状态（包括 `deflateCopy`/`inflateCopy`）。
     * 若新状态拷贝失败，将对象标记为 tainted 后重新抛出。若新状态拷贝成功但旧流
     * 关闭失败，则在返回前重新抛出 `close_stream` 的异常——确保赋值结果始终可用，
     * 同时不丢失关闭失败的错误信息。
     * @endif
     *
     * @lang{EN}
     * Copy assignment operator.
     * Closes the old stream first (capturing any error), then copies the new state
     * (including `deflateCopy`/`inflateCopy`).
     * If copying the new state fails, the object is marked tainted before rethrowing.
     * If copying succeeds but the old stream close failed, the `close_stream` exception
     * is rethrown before returning — ensuring the assignment result is always usable
     * while not silently discarding the close failure.
     * @endif
     */
    zlib_cvt& operator=(const zlib_cvt& val)
    {
        if (this == &val) return *this;

        // close_stream failure indicates the OLD output stream could not be
        // finalized on its device; it leaves *this's in-memory zlib state
        // self-consistent (guards inside close_stream reset m_strm / status).
        // It does NOT damage the new assignment we are about to perform, so
        // capture it and rethrow after the new state is fully installed.
        std::exception_ptr close_err;
        try { close_stream(); }
        catch (...) { close_err = std::current_exception(); }

        try
        {
            BT::operator=(val);
            m_put_level = val.m_put_level;
            m_sync_flush = val.m_sync_flush;
            m_stream_ended = val.m_stream_ended;

            if (BT::m_io_status == io_status::output)
            {
                m_strm = std::make_unique<z_stream>();
                zerr("zlib_cvt copy assignment fail", deflateCopy(m_strm.get(), val.m_strm.get())); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            }
            else if (BT::m_io_status == io_status::input)
            {
                m_strm = std::make_unique<z_stream>();
                zerr("zlib_cvt copy assignment fail", inflateCopy(m_strm.get(), val.m_strm.get())); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            }
        }
        catch(...)
        {
            m_strm.reset();
            m_stream_ended = false;
            BT::set_tainted();
            throw;
        }

        if (close_err) std::rethrow_exception(close_err);
        return *this;
    }

    /**
     * @lang{ZH}
     * 移动赋值运算符（`noexcept`）。
     * 先关闭旧流（忽略任何错误），再转移所有成员，包括 `m_strm` 的 `unique_ptr`。
     * 转移 `unique_ptr` 而非拷贝 `z_stream` 结构体，可保留 zlib 内部反向指针的有效性
     * （详见移动构造函数的说明）。
     * @endif
     *
     * @lang{EN}
     * Move assignment operator (`noexcept`).
     * Closes the old stream first (discarding any error), then transfers all members,
     * including the `m_strm` `unique_ptr`.
     * Transferring the `unique_ptr` rather than copying the `z_stream` struct preserves
     * the validity of zlib's internal back-pointer (see the move constructor for details).
     * @endif
     */
    zlib_cvt& operator=(zlib_cvt&& val) noexcept
    {
        if (this == &val) return *this;

        try { close_stream(); }
        catch (...) {} // NOLINT(bugprone-empty-catch)

        BT::operator=(std::move(val));
        m_put_level = val.m_put_level;
        m_sync_flush = val.m_sync_flush;
        m_stream_ended = val.m_stream_ended;
        m_strm = std::move(val.m_strm);  // preserves z_stream address; see move ctor

        return *this;
    }

    /**
     * @lang{ZH}
     * 析构函数（`noexcept`）。调用 `close_stream()` 最终化输出流；若抛出异常则静默忽略。
     * @endif
     *
     * @lang{EN}
     * Destructor (`noexcept`). Calls `close_stream()` to finalize the output stream;
     * any exception thrown is silently discarded.
     * @endif
     */
    ~zlib_cvt() noexcept
    {
        try
        {
            close_stream();
        }
        catch (...) {} // NOLINT(bugprone-empty-catch)
    }

// mandatory methods
public:
    /**
     * @lang{ZH}
     * 查询是否已到达 zlib 压缩流末尾。
     * `m_stream_ended` 是 zlib 级别的 EOF 锁存位，在 `get_main` 中 `inflate()` 返回
     * `Z_STREAM_END` 时置位。`BT::is_eof()` 反映内核的可读字节数视图，可能滞后于
     * zlib 流结束（zlib 的尾部/校验字段可能仍驻留在内核缓冲区中）。
     * 两个条件任一为真均代表调用方视角的真实流结束，因此取逻辑或。
     * @endif
     *
     * @lang{EN}
     * Query whether the end of the zlib compressed stream has been reached.
     * `m_stream_ended` is the zlib-level EOF latch, set by `get_main` when `inflate()`
     * reports `Z_STREAM_END`. `BT::is_eof()` reflects the kernel's bytes-available
     * view, which can lag behind the zlib stream end (zlib's trailer/checksum may still
     * reside in the kernel buffer after the decompressed payload is exhausted).
     * Either condition represents a true end-of-stream from the caller's perspective,
     * so they are OR'd together.
     * @endif
     *
     * @return 若已到达流末尾（zlib 流已结束或内核无更多数据），返回 `true`。
     *         / `true` if the end of the stream has been reached (zlib stream ended or kernel has no more data).
     */
    bool is_eof()
        requires (cvt_cpt::support_get<KernelType>)
    {
        // m_stream_ended is the zlib-level EOF latch set by get_main when
        // inflate() reports Z_STREAM_END.  BT::is_eof() reflects the
        // underlying kernel's bytes-available view, which can lag behind the
        // zlib stream end (zlib's trailer/footer may sit inside the kernel
        // buffer even after the decompressed payload is exhausted).  Either
        // condition is a true end-of-stream for the caller, so OR them.
        return m_stream_ended || BT::is_eof();
    }

private:
    /**
     * @lang{ZH}
     * `abs_cvt::detach()` 的 CRTP 钩子，在 kernel 层 `detach()` 之前调用。
     *
     * 负责执行 `zlib_cvt` 层的清理（`close_stream()`），并将捕获到的异常以
     * `exception_ptr` 形式返回；调用方（`abs_cvt::detach()`）负责按 first-failure-wins
     * 与 kernel 层异常合并。本函数为 `noexcept`，此约束由 `abs_cvt` 的 `static_assert` 强制。
     *
     * @return 捕获到的首个清理异常；无异常时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::detach()`, called before the kernel-level `detach()`.
     *
     * Performs `zlib_cvt`-layer cleanup (`close_stream()`) and returns any captured
     * exception as an `exception_ptr`; the caller (`abs_cvt::detach()`) merges it
     * with the kernel-layer exception under first-failure-wins.
     * Must be `noexcept` — enforced by a `static_assert` in `abs_cvt`.
     *
     * @return The first captured cleanup exception, or `nullptr` if none.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        std::exception_ptr local_err = nullptr;
        try { close_stream(); }
        catch (...) { local_err = std::current_exception(); }
        return local_err;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::bos()` 的 CRTP 钩子，在 kernel `bos()` 返回后调用。
     *
     * 此时 `BT::m_io_status` 已被更新为 kernel 确定的初始方向，本函数根据该方向
     * 初始化 zlib 状态并处理 2 字节流头：
     * - **输出（压缩）模式**：调用 `deflateInit`，以 `Z_NO_FLUSH` 和零输入触发
     *   deflate 输出 2 字节流头，验证恰好输出 2 字节且无残余待发字节，
     *   然后通过内核写出该头部。
     * - **输入（解压）模式**：调用 `inflateInit`，从内核读取 2 字节流头，
     *   送入 `inflate` 验证头部格式；不支持预置字典（`Z_NEED_DICT`），遇到时抛出。
     *
     * 若任何步骤失败，`m_strm` 已被守卫（`inflate_guard`/`deflate_guard`）或手动
     * 重置为 `nullptr`；异常由调用方 `abs_cvt::bos()` 捕获，并负责将 IO 状态重置
     * 为 `neutral`、标记 tainted 后重新抛出。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::bos()`, called after the kernel's `bos()` returns.
     *
     * At this point `BT::m_io_status` has already been updated to the initial
     * direction determined by the kernel. This hook initializes zlib state and
     * processes the 2-byte stream header accordingly:
     * - **Output (compression) mode**: calls `deflateInit`, triggers deflate with
     *   `Z_NO_FLUSH` and zero input to emit the 2-byte stream header, verifies
     *   exactly 2 bytes were emitted with no pending bytes remaining, then writes
     *   the header through the kernel.
     * - **Input (decompression) mode**: calls `inflateInit`, reads the 2-byte stream
     *   header from the kernel, and feeds it to `inflate` for header validation.
     *   Preset dictionaries (`Z_NEED_DICT`) are not supported and cause a throw.
     *
     * If any step fails, `m_strm` has already been reset to `nullptr` by the guards
     * (`inflate_guard`/`deflate_guard`) or manually. The exception propagates to
     * `abs_cvt::bos()`, which resets `m_io_status` to `neutral` and marks the
     * converter tainted before rethrowing.
     * @endif
     *
     * @throws cvt_error 若 zlib 初始化失败、流头格式非法或内核不支持所需的读写方向。
     *                   / If zlib initialization fails, the stream header is invalid, or
     *                   the kernel does not support the required read/write direction.
     */
    void bos_impl()
    {
        if (BT::m_io_status == io_status::input)
        {
            if constexpr (cvt_cpt::support_get<KernelType>)
            {
                m_strm = std::make_unique<z_stream>();
                m_strm->zalloc = Z_NULL;
                m_strm->zfree = Z_NULL;
                m_strm->opaque = Z_NULL;
                m_strm->avail_in = 0;
                m_strm->next_in = Z_NULL;
                m_strm->state = Z_NULL;
                auto ret = inflateInit(m_strm.get());
                if (ret != Z_OK) { m_strm.reset(); throw cvt_error("zlib_cvt::bos fail: Cannot initialize zlib."); }
                inflate_guard zlib_guard(m_strm);

                // Contract: kernel.get() on BOS path guarantees to return exactly the
                // requested byte count, or throw an exception if insufficient data is
                // available. The assert below is purely defensive, verifying this
                // invariant during development. It is intentionally compiled out in
                // release builds.
                std::array<external_type, zlib_header_size> header_buf{};
                [[maybe_unused]] const size_t n = BT::m_kernel.get(header_buf.data(), zlib_header_size);
                assert(n == zlib_header_size);

                m_strm->avail_in = zlib_header_size;
                m_strm->next_in = reinterpret_cast<unsigned char*>(header_buf.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

                unsigned char ch = 0;
                m_strm->avail_out = 1;
                m_strm->next_out = &ch;

                ret = inflate(m_strm.get(), Z_NO_FLUSH);
                if (ret == Z_NEED_DICT)
                    throw cvt_error("zlib_cvt::bos fail: preset dictionary not supported");
                zerr("zlib_cvt::bos fail", ret);

                if ((m_strm->avail_in != 0) || (m_strm->avail_out != 1)) throw cvt_error("zlib_cvt::bos fail: Invalid zlib header");
                m_strm->next_in = nullptr;
                m_strm->avail_out = 0;
                m_strm->next_out = nullptr;

                zlib_guard.release();
            }
            else
                throw cvt_error("zlib_cvt::bos fail: BT returned input but KernelType lacks get support");
        }
        else if (BT::m_io_status == io_status::output)
        {
            if constexpr (cvt_cpt::support_put<KernelType>)
            {
                m_strm = std::make_unique<z_stream>();
                m_strm->zalloc = Z_NULL;
                m_strm->zfree = Z_NULL;
                m_strm->opaque = Z_NULL;
                m_strm->state = Z_NULL;

                auto ret = deflateInit(m_strm.get(), static_cast<int>(m_put_level));
                if (ret != Z_OK) { m_strm.reset(); throw cvt_error("zlib_cvt::bos fail: Cannot initialize zlib."); }
                deflate_guard zlib_guard(m_strm);

                // Contract enforced below: after this deflate() call we require
                // exactly zlib_header_size bytes in the output buffer and zero
                // bytes/bits still pending inside zlib. Z_NO_FLUSH on a fresh
                // stream with no input is expected to flush the queued header
                // and nothing else. The post-checks turn that expectation into
                // a hard invariant: any future zlib behavior change (more
                // bytes, fewer bytes, or bytes left buffered) surfaces here as
                // a loud failure rather than silent framing corruption.
                std::array<external_type, zlib_header_size + 1> header_buf{};

                m_strm->avail_in = 0;
                m_strm->next_in = nullptr;
                m_strm->avail_out = zlib_header_size + 1;
                m_strm->next_out = reinterpret_cast<unsigned char*>(header_buf.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                ret = deflate(m_strm.get(), Z_NO_FLUSH);
                zerr("zlib_cvt::bos fail", ret);

                if (m_strm->avail_out != 1)
                    throw cvt_error("zlib_cvt::bos fail: zlib did not emit exactly the header");

                unsigned pending_bytes = 0;
                int pending_bits = 0;
                if (deflatePending(m_strm.get(), &pending_bytes, &pending_bits) != Z_OK ||
                    pending_bytes != 0 || pending_bits != 0)
                    throw cvt_error("zlib_cvt::bos fail: zlib has unflushed bytes after header");

                BT::m_kernel.put(header_buf.data(), zlib_header_size);

                m_strm->avail_out = 0;
                m_strm->next_out = nullptr;

                zlib_guard.release();
            }
            else
                throw cvt_error("zlib_cvt::bos fail: BT returned output but KernelType lacks put support");
        }
        else
            throw cvt_error("zlib_cvt::bos fail: invalid response value.");
    }

    /**
     * @lang{ZH}
     * `abs_cvt::main_cont_beg()` 的 CRTP 钩子，在 kernel 层 `main_cont_beg()` 之后调用。
     *
     * - 若当前为输出模式，调用 `BT::flush()` 将已在 BOS 阶段写入内核缓冲区的
     *   zlib 流头刷出到底层设备。此时 `m_strm` 必须为有效的 zlib 流（由 `bos()`
     *   的不变量保证）。
     * - 若当前为 `neutral` 模式，说明 `bos()` 未被调用，抛出异常。
     * 允许抛出异常；调用方会将 `m_io_status` 重置为 `neutral` 并设置污染标志后透传。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::main_cont_beg()`, called after the kernel-level
     * `main_cont_beg()`.
     *
     * - If currently in output mode, calls `BT::flush()` to flush the zlib stream
     *   header (written into the kernel buffer during the BOS phase) to the underlying
     *   device. At this point `m_strm` is guaranteed to be a valid zlib stream by the
     *   `bos()` invariant.
     * - If currently in `neutral` mode, `bos()` has not been called and an exception
     *   is thrown.
     * May throw; the caller resets `m_io_status` to `neutral` and sets the taint
     * flag before rethrowing.
     * @endif
     *
     * @throws cvt_error 若 `bos()` 尚未被调用（IO 状态为 `neutral`）。
     *                   / If `bos()` has not yet been called (IO status is `neutral`).
     */
    void main_cont_beg_impl()
    {
        if (BT::m_io_status == io_status::output)
        {
            // Invariant: bos() either allocates m_strm and leaves m_io_status as
            // input/output, or restores neutral and taints the converter on failure.
            // Reaching here with m_io_status == output therefore implies m_strm is
            // a fully-initialized zlib stream; if the converter were tainted,
            // BT::flush() would throw before flush_impl() dereferences m_strm.
            assert(m_strm);
            BT::flush();
        }
        else if (BT::m_io_status == io_status::neutral)
            throw cvt_error("zlib_cvt::main_cont_beg fail: bos() has not been called before main_cont_beg");
    }

    /**
     * @lang{ZH}
     * 主内容阶段的解压实现（由 `abs_cvt::get` 调用）。
     * 调用 `inflate(Z_NO_FLUSH)` 将内核提供的压缩数据解压至 `to` 缓冲区。
     * 若 `m_stream_ended` 已置位（inflate 曾报告 `Z_STREAM_END`），直接返回 0，
     * 不再向已结束的 z_stream 送入更多数据。
     * 每次从内核一次读取一字节（有意为之：解压膨胀比不可预测，批量读取可能溢出输出缓冲区；
     * 内核已有缓冲，单字节读取不会产生额外系统调用）。
     * @endif
     *
     * @lang{EN}
     * Main-content-phase decompression implementation (called by `abs_cvt::get`).
     * Calls `inflate(Z_NO_FLUSH)` to decompress data provided by the kernel into the
     * `to` buffer.
     * If `m_stream_ended` is already set (inflate previously reported `Z_STREAM_END`),
     * returns 0 immediately without feeding more data into the finished z_stream.
     * Reads one byte at a time from the kernel (intentional: the decompression expansion
     * ratio is unpredictable and bulk reads could overflow the output buffer; the kernel
     * already buffers, so single-byte reads do not incur extra syscalls).
     * @endif
     *
     * @param reader 封装内核读取的缓冲读取辅助对象。 / Buffered read helper wrapping the kernel.
     * @param to     解压结果的输出缓冲区。 / Output buffer for the decompressed data.
     * @param to_max 期望解压的最大元素数量。 / Maximum number of elements to decompress.
     * @return 实际解压的元素数量。 / Actual number of elements decompressed.
     * @throws cvt_error 若压缩流被截断，或 zlib 报告致命错误，或输出字节数不是 sizeof(internal_type) 的整数倍。
     *                   / If the compressed stream is truncated, zlib reports a fatal error,
     *                   or the output byte count is not a multiple of sizeof(internal_type).
     */
    size_t get_main(cvt_reader<KernelType>& reader, internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        // Once inflate() has reported Z_STREAM_END on this stream, further
        // inflate() calls on the same z_stream would either fail with
        // Z_DATA_ERROR or attempt to interpret trailing bytes as a new
        // concatenated zlib stream.  Both are wrong for a single-stream
        // contract.  Surface end-of-stream as a zero-byte read so callers
        // see a clean EOF instead.  Reset by close_stream's state_guard.
        if (m_stream_ended) return 0;

        // Note: We read one byte at a time intentionally. Decompression has
        // unpredictable expansion ratio - a few compressed bytes could expand
        // to overflow the output buffer. This keeps the code simple and correct.
        // The underlying converter already buffers, so this doesn't cause syscalls.
        reader.reset(1);

        // Ensures m_strm's in/out pointers and counts are cleared on every exit
        // path, so a throw cannot leave next_in pointing into reader's buffer
        // after the reader is destroyed.
        io_buf_guard buf_guard{*m_strm};

        constexpr size_t max_type_limit = std::numeric_limits<decltype(m_strm->avail_out)>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        auto aim_output = (max_chunk / sizeof(internal_type) > to_max)
                        ? static_cast<decltype(m_strm->avail_out)>(to_max * sizeof(internal_type))
                        : static_cast<decltype(m_strm->avail_out)>(max_chunk);

        m_strm->avail_out = aim_output;
        m_strm->next_out = reinterpret_cast<unsigned char*>(to); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        auto ret = inflate(m_strm.get(), Z_NO_FLUSH);
        zerr<true>("zlib_cvt::get fail", ret);

        while (m_strm->avail_out && ret != Z_STREAM_END)
        {
            auto [ptr, len] = reader.get_buf(1);
            if (len == 0) break;
            m_strm->next_in = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(ptr)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-const-cast)
            m_strm->avail_in = 1;
            // Snapshot before the call so we can prove zlib made progress on at
            // least one axis (input consumed or output produced). The Z_OK
            // contract guarantees this, but enforcing it here turns a
            // hypothetical infinite loop into an explicit failure if the
            // library ever diverges from its contract.
            const auto prev_avail_in = m_strm->avail_in;
            const auto prev_avail_out = m_strm->avail_out;
            ret = inflate(m_strm.get(), Z_NO_FLUSH);
            // Invariant: When output space is available, inflate() always consumes
            // all provided input into its internal state, even if no output is
            // produced yet. This assert guards against zlib misbehavior or memory
            // corruption during development.
            assert(m_strm->avail_in == 0);
            zerr("zlib_cvt::get fail", ret);
            if (m_strm->avail_in == prev_avail_in && m_strm->avail_out == prev_avail_out)
                throw cvt_error("zlib_cvt::get fail: zlib made no progress");
        }

        // Latch end-of-stream as soon as inflate() observes it, before any
        // post-condition checks below.  If a check throws afterwards, this
        // ensures is_eof() still reports true and any future get_main call
        // short-circuits to 0 instead of re-entering inflate() on a finished
        // stream.  The latch is a pure value write and cannot throw.
        if (ret == Z_STREAM_END)
            m_stream_ended = true;

        // Exiting the loop with output space still available means the underlying
        // kernel ran out of bytes before zlib reached Z_STREAM_END. The compressed
        // stream is truncated and partial output would silently corrupt caller data.
        if (ret != Z_STREAM_END && m_strm->avail_out != 0)
            throw cvt_error("zlib_cvt::get fail: compressed stream truncated");

        auto res = aim_output - m_strm->avail_out;
        // m_strm in/out fields are cleared by io_buf_guard on scope exit.

        if (res % sizeof(internal_type))
            throw cvt_error("zlib_cvt::get fail: partial sequence");

        return res / sizeof(internal_type);
    }

    /**
     * @lang{ZH}
     * 主内容阶段的压缩实现（由 `abs_cvt::put` 调用）。
     * 调用 `deflate(Z_NO_FLUSH)` 将 `_to` 中的数据压缩后写入内核（通过 `cvt_writer`）。
     * 按 `CHUNK` 大小申请输出缓冲区槽，若压缩未用完整个槽，调用 `writer.rollback`
     * 回退未使用的部分，防止未初始化数据被提交到内核。
     * 发生异常时同样回退未使用的槽位，确保内核缓冲区末尾始终指向最后一个有效压缩字节。
     * @endif
     *
     * @lang{EN}
     * Main-content-phase compression implementation (called by `abs_cvt::put`).
     * Calls `deflate(Z_NO_FLUSH)` to compress data from `_to` and writes it into the
     * kernel through `cvt_writer`.
     * Allocates output buffer slots in `CHUNK`-sized chunks; if deflate does not fill
     * an entire slot, calls `writer.rollback` to release the unused portion, preventing
     * uninitialized data from being committed to the kernel.
     * On exception, also rolls back any unused slot to ensure the kernel buffer tail
     * always points to the last valid compressed byte.
     * @endif
     *
     * @param writer  封装内核写入的缓冲写入辅助对象。 / Buffered write helper wrapping the kernel.
     * @param _to     待压缩的源数据缓冲区。 / Source data buffer to compress.
     * @param to_size 待压缩的元素数量。 / Number of elements to compress.
     * @throws cvt_error 若 zlib 报告致命错误或 zlib 未取得进展。
     *                   / If zlib reports a fatal error or zlib makes no progress.
     */
    void put_main(cvt_writer<KernelType>& writer, const internal_type* _to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        constexpr size_t max_type_limit = std::numeric_limits<decltype(m_strm->avail_out)>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        auto to = reinterpret_cast<const unsigned char*>(_to); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        writer.reset(CHUNK);

        // Ensures m_strm's in/out pointers and counts are cleared on every exit
        // path, so a throw cannot leave next_in / next_out pointing into the
        // caller's input or writer's buffer after they go out of scope.
        io_buf_guard buf_guard{*m_strm};

        while (to_size > 0)
        {
            auto aim_output = (max_chunk / sizeof(internal_type) > to_size)
                            ? static_cast<decltype(m_strm->avail_out)>(to_size * sizeof(internal_type))
                            : static_cast<decltype(m_strm->avail_out)>(max_chunk);

            size_t write_size = 0;
            while (aim_output != write_size)
            {
                m_strm->next_in = const_cast<unsigned char*>(to + write_size); // NOLINT(cppcoreguidelines-pro-type-const-cast)
                auto cur_put_size = static_cast<size_t>(aim_output - write_size);
                m_strm->avail_in = static_cast<decltype(m_strm->avail_in)>(cur_put_size);
                m_strm->next_out = reinterpret_cast<unsigned char*>(writer.put_buf(CHUNK)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                m_strm->avail_out = CHUNK;

                // put_buf reserved CHUNK bytes directly inside the kernel's
                // persistent output buffer (root_cvt / mem_device specializations
                // of cvt_writer commit at put_buf, not at commit()).  If anything
                // below throws, the trailing avail_out bytes are uninitialized
                // memory still inside the kernel's cursor range and would be
                // flushed to the device on the next put / close_stream / dtor.
                // Roll the tail back on the failure path so the kernel buffer
                // ends exactly at the last valid deflate output byte.
                try
                {
                    // Snapshot before the call so we can prove zlib made progress on
                    // at least one axis (input consumed or output produced). The
                    // Z_OK contract guarantees this, but enforcing it here turns a
                    // hypothetical infinite loop into an explicit failure if the
                    // library ever diverges from its contract.
                    const auto prev_avail_in = m_strm->avail_in;
                    const auto prev_avail_out = m_strm->avail_out;
                    auto ret = deflate(m_strm.get(), Z_NO_FLUSH);
                    zerr("zlib_cvt::put fail", ret);
                    if (m_strm->avail_in == prev_avail_in && m_strm->avail_out == prev_avail_out)
                        throw cvt_error("zlib_cvt::put fail: zlib made no progress");
                    write_size += cur_put_size - m_strm->avail_in;
                }
                catch (...)
                {
                    // rollback's throw conditions (len==0, len>reserved) are
                    // unreachable here: we just reserved CHUNK and avail_out<=CHUNK.
                    // The catch is defensive cleanup-path policy - swallow any
                    // future regression in rollback rather than mask the original
                    // exception.
                    if (m_strm->avail_out > 0)
                    {
                        try { writer.rollback(m_strm->avail_out); } catch (...) {} // NOLINT(bugprone-empty-catch)
                    }
                    throw;
                }

                if (m_strm->avail_out)
                    writer.rollback(m_strm->avail_out);
            }
            to += aim_output;
            to_size -= aim_output / sizeof(internal_type);
        }
        // commit() is the responsibility of abs_cvt::put.
        // m_strm in/out fields are cleared by io_buf_guard on scope exit.
    }

    /**
     * @lang{ZH}
     * 由 `abs_cvt::flush()` 调用的 zlib 层刷出逻辑。
     * 依据 `m_sync_flush` 标志决定行为：
     * - `m_sync_flush == true`：向下游发出 `Z_SYNC_FLUSH` 边界，使接收方能够在
     *   明确边界处开始解压。
     * - `m_sync_flush == false`：空操作；zlib 内部待发字节仍保留在缓冲区中，
     *   等到 `close_stream` 或后续 `put` 时才写出。
     * 关于此选项的成本权衡，参见 `zlib_sync_flush` 的说明。
     * @endif
     *
     * @lang{EN}
     * zlib-layer flush logic called by `abs_cvt::flush()`.
     * Behavior is determined by the `m_sync_flush` flag:
     * - `m_sync_flush == true`: emits a `Z_SYNC_FLUSH` boundary downstream so that
     *   the receiver can begin decompression at a well-defined point.
     * - `m_sync_flush == false`: no-op; zlib's internal pending bytes remain buffered
     *   until `close_stream` or a subsequent `put` writes them out.
     * For the cost trade-off of this option, see the `zlib_sync_flush` documentation.
     * @endif
     */
    void flush_impl()
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (m_sync_flush)
        {
            io_buf_guard buf_guard{*m_strm};

            std::array<external_type, CHUNK> local_buf{};
            m_strm->next_in = nullptr;
            m_strm->avail_in = 0;
            while (true)
            {
                m_strm->next_out = reinterpret_cast<unsigned char*>(local_buf.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                m_strm->avail_out = CHUNK;
                zerr("zlib_cvt::flush_impl fail", deflate(m_strm.get(), Z_SYNC_FLUSH));
                const size_t written = CHUNK - m_strm->avail_out;
                if (written > 0)
                    BT::m_kernel.put(local_buf.data(), written);
                if (m_strm->avail_out)
                    break;
            }
        }
    }

    /**
     * @lang{ZH}
     * 调整转换器行为参数。
     * 若 `acc` 为 `zlib_sync_flush` 实例，则更新 `m_sync_flush` 标志；
     * 同时将 `acc` 转发给基类 `BT::adjust()`。
     * @endif
     *
     * @lang{EN}
     * Adjust converter behavior parameters.
     * If `acc` is a `zlib_sync_flush` instance, updates the `m_sync_flush` flag;
     * also forwards `acc` to the base-class `BT::adjust()`.
     * @endif
     *
     * @param acc 行为策略对象。 / The behavior policy object.
     */
    void adjust_impl(const cvt_behavior& acc)
    {
        if (auto* ptr = dynamic_cast<const zlib_sync_flush*>(&acc); ptr)
            m_sync_flush = ptr->m_sync_flush;
    }

private:
    /**
     * @lang{ZH}
     * 将 zlib 返回码转换为 `cvt_error` 异常。
     * 仅对非负返回码（`Z_OK`、`Z_STREAM_END` 等）不抛出。
     * 模板参数 `IgnoreBufError` 控制 `Z_BUF_ERROR` 的处理：
     * - `false`（默认）：`Z_BUF_ERROR` 作为错误抛出；
     * - `true`：`Z_BUF_ERROR` 被静默忽略（用于 `get_main` 中，`Z_BUF_ERROR` 表示
     *   "当前无可用输入"，这是正常情况而非错误）。
     * @endif
     *
     * @lang{EN}
     * Translate a zlib return code into a `cvt_error` exception.
     * Non-negative return codes (`Z_OK`, `Z_STREAM_END`, etc.) do not throw.
     * The template parameter `IgnoreBufError` controls handling of `Z_BUF_ERROR`:
     * - `false` (default): `Z_BUF_ERROR` is treated as an error and throws;
     * - `true`: `Z_BUF_ERROR` is silently ignored (used in `get_main` where
     *   `Z_BUF_ERROR` means "no input available right now", which is normal).
     * @endif
     *
     * @tparam IgnoreBufError 若为 `true`，`Z_BUF_ERROR` 不抛出异常。 / If `true`, `Z_BUF_ERROR` does not throw.
     * @param info 异常消息的前缀，用于标识调用位置。 / Prefix for the exception message, identifying the call site.
     * @param ret  zlib 函数返回的状态码。 / The status code returned by a zlib function.
     * @throws cvt_error 若 `ret` 为负值（`IgnoreBufError` 控制 `Z_BUF_ERROR` 的处理）。
     *                   / If `ret` is negative (with `IgnoreBufError` controlling `Z_BUF_ERROR`).
     */
    template <bool IgnoreBufError = false>
    void zerr(const char* info, int ret)
    {
        switch (ret)
        {
        case Z_ERRNO:
            throw cvt_error(std::string(info) + ": zlib i/o error");
        case Z_STREAM_ERROR:
            // Z_STREAM_ERROR has two meanings depending on the call site:
            // deflateInit returns it for an invalid compression level argument;
            // inflate / deflate / inflateEnd / deflateEnd return it when zlib
            // detects an inconsistent internal stream state (often a symptom
            // of memory corruption, ABI mismatch, or concurrent access).
            throw cvt_error(std::string(info) + ": invalid argument or inconsistent stream state (Z_STREAM_ERROR)");
        case Z_DATA_ERROR:
            throw cvt_error(std::string(info) + ": invalid or incomplete deflate data");
        case Z_MEM_ERROR:
            throw cvt_error(std::string(info) + ": out of memory");
        case Z_VERSION_ERROR:
            throw cvt_error(std::string(info) + ": zlib version mismatch");
        case Z_BUF_ERROR:
            if constexpr (!IgnoreBufError)
                throw cvt_error(std::string(info) + ": no progress possible (Z_BUF_ERROR)");
            break;
        default:
            if (ret < 0)
                throw cvt_error(std::string(info) + ": unknown zlib error " + std::to_string(ret));
        }
    }

    /**
     * @lang{ZH}
     * 最终化并关闭当前 zlib 流。
     * `state_guard` 在作用域退出时无条件将 `m_is_bos_done`、`m_io_status`、
     * `m_sync_flush`、`m_stream_ended` 重置为初始值。
     * - 若 `m_strm` 为空（`bos()` 从未被调用，或已被守卫/手动重置），直接返回。
     * - **输出模式，BOS 已完成**（`m_is_bos_done == true`）：循环调用 `deflate(Z_FINISH)`
     *   直到 `Z_STREAM_END`，将所有剩余压缩字节写入内核；`deflate_guard` 在作用域退出时
     *   调用 `deflateEnd` 并重置 `m_strm`。
     * - **输出模式，BOS 未完成**（`m_is_bos_done == false`）：`bos()` 已通过 `deflateInit`
     *   初始化 `m_strm`，但 `main_cont_beg()` 未曾调用；跳过 `Z_FINISH` 循环，直接调用
     *   `deflateEnd` 释放 zlib 状态。
     * - **输入模式**：先调用 `inflateEnd`（并重置 `m_strm`），再检查返回值——
     *   确保即使 `inflateEnd` 返回 `Z_STREAM_ERROR`，`m_strm` 已置空，
     *   防止后续调用重复 `inflateEnd` 已释放的状态。
     * @endif
     *
     * @lang{EN}
     * Finalize and close the current zlib stream.
     * A `state_guard` unconditionally resets `m_is_bos_done`, `m_io_status`,
     * `m_sync_flush`, and `m_stream_ended` to their initial values on scope exit.
     * - If `m_strm` is null (`bos()` was never called, or it was reset by guards/manually),
     *   returns immediately.
     * - **Output mode, BOS completed** (`m_is_bos_done == true`): loops `deflate(Z_FINISH)`
     *   until `Z_STREAM_END`, writing all remaining compressed bytes to the kernel;
     *   `deflate_guard` calls `deflateEnd` and resets `m_strm` on scope exit.
     * - **Output mode, BOS not completed** (`m_is_bos_done == false`): `bos()` initialized
     *   `m_strm` via `deflateInit` but `main_cont_beg()` was never called; skips the
     *   `Z_FINISH` loop and calls `deflateEnd` directly to free the zlib state.
     * - **Input mode**: calls `inflateEnd` first (resetting `m_strm`), then checks the
     *   return code — ensures that even if `inflateEnd` returns `Z_STREAM_ERROR`,
     *   `m_strm` is already null, preventing a subsequent call from calling `inflateEnd`
     *   again on already-freed state.
     * @endif
     *
     * @throws cvt_error 若 deflate/inflate 最终化失败。 / If deflate/inflate finalization fails.
     */
    void close_stream()
    {
        struct state_guard
        {
            zlib_cvt& self; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
            state_guard(zlib_cvt& p_self) : self(p_self) {}
            state_guard(const state_guard&) = delete;
            state_guard& operator=(const state_guard&) = delete;
            state_guard(state_guard&&) = delete;
            state_guard& operator=(state_guard&&) = delete;
            ~state_guard()
            {
                self.BT::m_is_bos_done = false;
                self.BT::m_io_status = io_status::neutral;
                self.m_sync_flush = false;
                self.m_stream_ended = false;
            }
        } sg{*this};

        // Skip cleanup if zlib was never successfully initialized (m_strm is
        // null until bos() calls inflateInit/deflateInit, and is reset to null
        // by the guards on both the success and failure paths).
        if (!m_strm)
            return;

        if (BT::m_io_status == io_status::output)
        {
            if (!BT::m_is_bos_done)
            {
                // bos() ran (m_strm is valid) but main_cont_beg() was never called.
                // No user data was written, so skip the Z_FINISH loop and just free
                // the zlib state.
                deflateEnd(m_strm.get());
                m_strm.reset();
            }
            else
            {
                std::array<external_type, CHUNK> local_buf{};
                deflate_guard g(m_strm);  // calls deflateEnd + resets m_strm on scope exit
                m_strm->next_in = nullptr;
                m_strm->avail_in = 0;
                while (true)
                {
                    m_strm->next_out = reinterpret_cast<unsigned char*>(local_buf.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    m_strm->avail_out = CHUNK;
                    auto ret = deflate(m_strm.get(), Z_FINISH);
                    zerr("zlib_cvt::close_stream fail", ret);
                    const size_t written = CHUNK - m_strm->avail_out;
                    if (written > 0)
                        BT::m_kernel.put(local_buf.data(), written);
                    if (ret == Z_STREAM_END)
                        break;
                    // Z_FINISH must reach Z_STREAM_END in finite iterations.
                    // If deflate returned Z_OK and did not even fill CHUNK
                    // (avail_out > 0), it is claiming "no more output right
                    // now" without declaring end-of-stream.  Since we feed
                    // no input on this path, the next iteration will hit
                    // the exact same state and loop forever.  Surface the
                    // inconsistency instead.
                    if (m_strm->avail_out > 0)
                        throw cvt_error("zlib_cvt::close_stream fail: Z_FINISH returned without stream end");
                }
            }
        }
        else if (BT::m_io_status == io_status::input)
        {
            // inflateEnd frees zlib's internal state regardless of the return
            // code, so reset m_strm BEFORE potentially throwing.  Otherwise a
            // Z_STREAM_ERROR throw would leave m_strm non-null; the next entry
            // to close_stream would skip the early-return guard and call
            // inflateEnd again on already-freed state.
            auto ret = inflateEnd(m_strm.get());
            m_strm.reset();
            zerr("zlib_cvt::close_stream fail", ret);
        }
    }
private:
    unsigned m_put_level; ///< @lang{ZH} deflate 压缩等级（0–9），在构造时截断至 9。 @endif @lang{EN} deflate compression level (0–9), clamped to 9 at construction. @endif

    /**
     * @lang{ZH}
     * `flush()` 的同步刷模式标志。
     * `false`（默认）：`flush()` 是 zlib 内部待发字节的空操作；
     * `true`：`flush()` 发出 `Z_SYNC_FLUSH` 边界。
     * 通过 `adjust(zlib_sync_flush{...})` 切换。详见 `zlib_sync_flush` 的成本权衡说明。
     * @endif
     *
     * @lang{EN}
     * Sync-flush mode flag for `flush()`.
     * `false` (default): `flush()` is a no-op for zlib's internal pending bytes;
     * `true`: `flush()` emits a `Z_SYNC_FLUSH` boundary.
     * Toggled via `adjust(zlib_sync_flush{...})`. See `zlib_sync_flush` for the
     * cost trade-off rationale.
     * @endif
     */
    bool m_sync_flush{false};

    /**
     * @lang{ZH}
     * 流结束锁存位。在 `get_main` 中 `inflate()` 返回 `Z_STREAM_END` 时置 `true`。
     * 后续 `get_main` 调用直接返回 0，不再向已结束的流送入压缩数据
     * （否则会产生 `Z_DATA_ERROR` 或被误解为新的拼接 zlib 流）。
     * 由 `close_stream` 的 `state_guard` 及拷贝/移动/赋值路径安装新流时重置。
     * @endif
     *
     * @lang{EN}
     * End-of-stream latch. Set to `true` inside `get_main` when `inflate()` returns
     * `Z_STREAM_END`. Subsequent `get_main` calls short-circuit to 0 instead of
     * feeding more compressed bytes into the finished stream (which would otherwise
     * produce `Z_DATA_ERROR` or be misinterpreted as a new concatenated zlib stream).
     * Reset by `close_stream`'s `state_guard` and by copy/move/assignment paths
     * that install a fresh stream.
     * @endif
     */
    bool m_stream_ended{false};

    /// @lang{ZH} 当前活跃的 zlib 流对象；在 `bos()` 中初始化，在 `close_stream()` 中释放。 @endif
    /// @lang{EN} The currently active zlib stream object; initialized in `bos()` and released in `close_stream()`. @endif
    std::unique_ptr<z_stream> m_strm;
};

/**
 * @lang{ZH}
 * `zlib_cvt` 的工厂类，用于在管道中构造压缩/解压转换器。
 * 持有压缩等级，通过 `create(kernel)` 将任意内核包装为 `zlib_cvt` 实例。
 * 通常通过 `operator|` 与其他工厂组合使用。
 * @endif
 *
 * @lang{EN}
 * Factory class for `zlib_cvt`, used to construct compression/decompression converters
 * in a pipeline. Stores the compression level and wraps any compatible kernel into a
 * `zlib_cvt` instance via `create(kernel)`.
 * Typically composed with other factories via `operator|`.
 * @endif
 *
 * @tparam TInt 目标转换器的内部数据类型。 / The internal data type of the target converter.
 */
template <typename TInt>
class zlib_cvt_creator
{
public:
    using category = CvtCreatorCategory;

    /**
     * @lang{ZH}
     * 以指定压缩等级构造工厂。
     * @endif
     *
     * @lang{EN}
     * Construct the factory with the specified compression level.
     * @endif
     *
     * @param level deflate 压缩等级（0–9）。 / deflate compression level (0–9).
     */
    explicit zlib_cvt_creator(unsigned level)
        : m_level(level) {}

    /**
     * @lang{ZH}
     * 将内核包装为 `zlib_cvt` 实例。
     * @endif
     *
     * @lang{EN}
     * Wrap a kernel into a `zlib_cvt` instance.
     * @endif
     *
     * @tparam TKernel 满足 `io_converter` 的内核类型。 / The kernel type satisfying `io_converter`.
     * @param kernel  待包装的内核（完美转发）。 / The kernel to wrap (perfect-forwarded).
     * @return 以 `kernel` 和存储的压缩等级构造的 `zlib_cvt` 实例。
     *         / A `zlib_cvt` instance constructed from `kernel` and the stored compression level.
     */
    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return zlib_cvt<std::remove_cvref_t<TKernel>, TInt>{std::forward<TKernel>(kernel), m_level};
    }
private:
    unsigned m_level; ///< @lang{ZH} 传递给 `deflateInit` 的压缩等级。 @endif @lang{EN} Compression level passed to `deflateInit`. @endif
};
}

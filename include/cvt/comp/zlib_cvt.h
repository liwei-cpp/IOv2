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
// adjust() behavior that toggles Z_SYNC_FLUSH semantics on zlib_cvt::flush().
//
// Default for zlib_cvt is sync_flush == false, in which case flush() does NOT
// emit a sync marker - bytes already deflate()'d on previous put() calls are
// still resident in the downstream kernel buffer chain and will reach the
// device through subsequent put / close_stream paths, but data still sitting
// inside zlib's LZ77 window stays buffered until close_stream finalizes the
// stream with Z_FINISH.
//
// This default is deliberate: Z_SYNC_FLUSH is not free.  Each invocation
// inserts a 5-byte empty stored block (00 00 00 FF FF) as a stream sync point
// AND forces zlib to abandon partial LZ77 matches it was still trying to
// optimize - frequent sync-flushing can degrade the compression ratio toward
// 1:1.  Callers who do not need stream-level synchronization should not pay
// that cost.
//
// Set sync_flush == true via cvt.adjust(zlib_sync_flush{true}) when you do
// need synchronization points: streaming over a network where the receiver
// must be able to start decompression at well-defined boundaries, interactive
// tools that need byte-level latency at the cost of ratio, or any framing
// where downstream may begin decompression mid-stream.
struct zlib_sync_flush : cvt_behavior
{
    explicit zlib_sync_flush(bool sync_flush)
        : m_sync_flush(sync_flush) {}

    bool m_sync_flush;
};

template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (sizeof(typename KernelType::internal_type) == sizeof(unsigned char))
class zlib_cvt : public abs_cvt<zlib_cvt<KernelType, TInt>, KernelType, TInt, false, false>
{
    using BT = abs_cvt<zlib_cvt<KernelType, TInt>, KernelType, TInt, false, false>;
    friend BT;  // for put_main and get_main

    // On the failure path, call inflateEnd/deflateEnd and reset the unique_ptr
    // so that close_stream() (and the destructor) see a null m_strm and skip
    // redundant cleanup.  release() prevents the guard from running End on the
    // success path (the stream remains alive, owned by m_strm).
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

    // Resets next_in / avail_in / next_out / avail_out on scope exit so that
    // any throw from get_main / put_main / do_flush cannot leave m_strm holding
    // pointers into reader / writer / user buffers that have since gone out of
    // scope.  Does NOT touch m_strm itself - cleanup of the zlib stream object
    // is the job of close_stream / inflate_guard / deflate_guard.
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
    constexpr static unsigned CHUNK = std::max<unsigned>(16, sizeof(internal_type));
    constexpr static size_t zlib_header_size = 2;

public:
    zlib_cvt(KernelType kernel, unsigned put_level = 6)
        : BT(std::move(kernel))
        , m_put_level(put_level)
    {
        if (m_put_level > 9) m_put_level = 9;
    }

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

    // Move constructor: transfer the unique_ptr so the z_stream address is
    // unchanged.  zlib's internal state keeps a back-pointer (state->strm) to
    // the z_stream struct; a shallow struct copy would leave that pointer
    // stale, causing deflate/inflate to return Z_STREAM_ERROR.  Moving the
    // unique_ptr preserves the address and keeps the back-pointer valid.
    zlib_cvt(zlib_cvt&& val) noexcept
        : BT(std::move(val))
        , m_put_level(val.m_put_level)
        , m_sync_flush(val.m_sync_flush)
        , m_stream_ended(val.m_stream_ended)
        , m_strm(std::move(val.m_strm))
    {}

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
    void attach(device_type&& dev = device_type{})
    {
        // close_stream failure only means the OLD output stream could not be
        // finalized on its old device.  Its state_guard resets m_io_status /
        // m_is_bos_done / m_sync_flush, and the inflate_guard / deflate_guard
        // inside close_stream have already released m_strm.  So the failure
        // does NOT prevent us from installing the new device.  If we let it
        // propagate here, the caller's `dev` (already moved into our parameter)
        // would be destroyed during stack unwinding and silently lost.  Capture
        // the exception, install the new device first, then rethrow.
        std::exception_ptr close_err;
        try { close_stream(); }
        catch (...) { close_err = std::current_exception(); }

        BT::attach(std::move(dev));

        if (close_err) std::rethrow_exception(close_err);
    }

    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        std::exception_ptr local_err;
        try { close_stream(); }
        catch (...) { local_err = std::current_exception(); }
        auto [dev, inner_err] = BT::detach();
        return { std::move(dev), local_err ? local_err : inner_err };
    }

    void adjust(const cvt_behavior& acc)
    {
        if (auto* ptr = dynamic_cast<const zlib_sync_flush*>(&acc); ptr)
            m_sync_flush = ptr->m_sync_flush;

        return BT::adjust(acc);
    }

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

    void main_cont_beg()
    {
        BT::main_cont_beg();

        if (BT::m_io_status == io_status::output)
        {
            // Invariant: bos() either allocates m_strm and leaves m_io_status as
            // input/output, or restores neutral and taints the converter on failure.
            // Reaching here with m_io_status == output therefore implies m_strm is
            // a fully-initialized zlib stream; if the converter were tainted,
            // BT::flush() would throw before do_flush() dereferences m_strm.
            assert(m_strm);
            BT::flush();
        }
        else if (BT::m_io_status == io_status::neutral)
            throw cvt_error("zlib_cvt::main_cont_beg fail: bos() has not been called before main_cont_beg");
    }

    io_status bos()
    {
        BT::bos();
        // After BT::bos() succeeds, m_io_status is no longer neutral. Any failure
        // from here on must restore the invariant "m_io_status != neutral implies
        // m_strm is a fully-initialized zlib stream" before propagating, otherwise
        // copy/assign, do_flush, and the direction-switch branches in
        // abs_cvt::get/put will dereference a null m_strm. Reset the status and
        // taint the converter so subsequent get/put/flush refuse to run until
        // the caller reattaches a device.
        try
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
                {
                    throw cvt_error("zlib_cvt::bos fail: BT returned input but KernelType lacks get support");
                }
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
                {
                    throw cvt_error("zlib_cvt::bos fail: BT returned output but KernelType lacks put support");
                }
            }
            else
                throw cvt_error("zlib_cvt::bos fail: invalid response value.");
        }
        catch (...)
        {
            // m_strm has already been reset to nullptr by inflate_guard /
            // deflate_guard (or by the manual reset on inflateInit/deflateInit
            // failure) before the exception reaches here.
            BT::m_io_status = io_status::neutral;
            BT::set_tainted();
            throw;
        }
        return BT::m_io_status;
    }

// optional methods
private:
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

    // flush() entry from abs_cvt.  Honours the m_sync_flush toggle:
    //   - sync_flush == true  -> emit Z_SYNC_FLUSH so the downstream
    //     decompressor can resume from a well-defined boundary.
    //   - sync_flush == false -> no-op for zlib's internal LZ77 state.
    //     Bytes already deflate()'d on previous put() calls remain in the
    //     downstream kernel buffer chain and reach the device through
    //     subsequent put / close_stream paths; no user data is lost.
    // See the comment on zlib_sync_flush for why this is opt-in.
    void do_flush()
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
                zerr("zlib_cvt::do_flush fail", deflate(m_strm.get(), Z_SYNC_FLUSH));
                const size_t written = CHUNK - m_strm->avail_out;
                if (written > 0)
                    BT::m_kernel.put(local_buf.data(), written);
                if (m_strm->avail_out)
                    break;
            }
        }
    }

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

    void close_stream()
    {
        struct state_guard
        {
            zlib_cvt& self;
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
            if (BT::m_is_bos_done)
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
    unsigned    m_put_level;
    // false (default) -> flush() is a no-op for zlib's internal pending bytes;
    // true            -> flush() emits Z_SYNC_FLUSH boundary.
    // Toggled via adjust(zlib_sync_flush{...}).  See zlib_sync_flush doc for
    // the cost trade-off behind the default.
    bool        m_sync_flush{false};
    // Set to true inside get_main when inflate() returns Z_STREAM_END.  Any
    // subsequent get_main entry short-circuits to 0 instead of feeding more
    // compressed bytes into a finished stream (which would otherwise produce
    // Z_DATA_ERROR or be misinterpreted as a new concatenated zlib stream).
    // Reset by close_stream's state_guard and by copy/move/assignment paths
    // that install a fresh stream.
    bool        m_stream_ended{false};

    std::unique_ptr<z_stream> m_strm;
};

template <typename TInt>
class zlib_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    explicit zlib_cvt_creator(unsigned level)
        : m_level(level) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return zlib_cvt<std::remove_cvref_t<TKernel>, TInt>{std::forward<TKernel>(kernel), m_level};
    }
private:
    unsigned m_level;
};
}

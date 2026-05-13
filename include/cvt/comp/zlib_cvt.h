#pragma once

#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <zlib.h>

namespace IOv2::Comp
{
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
        , m_strm(std::move(val.m_strm))
    {}

    zlib_cvt& operator=(const zlib_cvt& val)
    {
        if (this == &val) return *this;
        close_stream();
        BT::operator=(val);
        m_put_level = val.m_put_level;
        m_sync_flush = val.m_sync_flush;

        try
        {
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
            return *this;
        }
        catch(...)
        {
            m_strm.reset();
            BT::set_tainted();
            throw;
        }
    }

    zlib_cvt& operator=(zlib_cvt&& val) noexcept
    {
        if (this == &val) return *this;

        try { close_stream(); }
        catch (...) {} // NOLINT(bugprone-empty-catch)

        BT::operator=(std::move(val));
        m_put_level = val.m_put_level;
        m_sync_flush = val.m_sync_flush;
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
        close_stream();
        BT::attach(std::move(dev));
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

    void main_cont_beg()
    {
        BT::main_cont_beg();

        if (BT::m_io_status == io_status::output)
            BT::flush();
        else if (BT::m_io_status == io_status::neutral)
            throw cvt_error("zlib_cvt::main_cont_beg fail: bos() has not been called before main_cont_beg");
    }

    io_status bos()
    {
        BT::bos();
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
        }
        else
            throw cvt_error("zlib_cvt::bos fail: invalid response value.");
        return BT::m_io_status;
    }

// optional methods
private:
    size_t get_main(cvt_reader<KernelType>& reader, internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        // Note: We read one byte at a time intentionally. Decompression has
        // unpredictable expansion ratio - a few compressed bytes could expand
        // to overflow the output buffer. This keeps the code simple and correct.
        // The underlying converter already buffers, so this doesn't cause syscalls.
        reader.reset(1);

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

        // Exiting the loop with output space still available means the underlying
        // kernel ran out of bytes before zlib reached Z_STREAM_END. The compressed
        // stream is truncated and partial output would silently corrupt caller data.
        if (ret != Z_STREAM_END && m_strm->avail_out != 0)
            throw cvt_error("zlib_cvt::get fail: compressed stream truncated");

        auto res = aim_output - m_strm->avail_out;
        m_strm->next_in = nullptr;
        m_strm->avail_in = 0;
        m_strm->next_out = nullptr;
        m_strm->avail_out = 0;

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

                if (m_strm->avail_out)
                    writer.rollback(m_strm->avail_out);
            }
            to += aim_output;
            to_size -= aim_output / sizeof(internal_type);
        }
        // commit() is the responsibility of abs_cvt::put.

        m_strm->next_out = nullptr;
        m_strm->avail_out = 0;
        m_strm->next_in = nullptr;
        m_strm->avail_in = 0;
    }

    void do_flush()
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (m_sync_flush)
        {
            struct buf_guard
            {
                z_stream& strm;
                ~buf_guard() noexcept
                {
                    strm.next_in = nullptr;
                    strm.avail_in = 0;
                    strm.next_out = nullptr;
                    strm.avail_out = 0;
                }
            } guard{*m_strm};

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
            throw cvt_error(std::string(info) + ": invalid compression level");
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
            }
        } sg{*this};

        // Skip cleanup if zlib was never successfully initialized (m_strm is
        // null until bos() calls inflateInit/deflateInit, and is reset to null
        // by the guards on both the success and failure paths).
        if (!m_strm)
            return;

        if (BT::m_io_status == io_status::output)
        {
            deflate_guard g(m_strm);  // calls deflateEnd + resets m_strm on scope exit
            if (BT::m_is_bos_done)
            {
                std::array<external_type, CHUNK> local_buf{};
                m_strm->next_in = nullptr;
                m_strm->avail_in = 0;
                while (true)
                {
                    m_strm->next_out = reinterpret_cast<unsigned char*>(local_buf.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    m_strm->avail_out = CHUNK;
                    zerr("zlib_cvt::close_stream fail", deflate(m_strm.get(), Z_FINISH));
                    const size_t written = CHUNK - m_strm->avail_out;
                    if (written > 0)
                        BT::m_kernel.put(local_buf.data(), written);
                    if (m_strm->avail_out)
                        break;
                }
            }
        }
        else if (BT::m_io_status == io_status::input)
        {
            inflateEnd(m_strm.get());
            m_strm.reset();
        }
    }
private:
    unsigned    m_put_level;
    bool        m_sync_flush{false};

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

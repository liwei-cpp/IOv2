/**
 * @file istream_operators.h
 * @lang{ZH}
 * 定义了输入流的格式化与非格式化提取设施。
 * 包含输入哨兵 `in_sentry`、输入流概念 `istream_type`、承载各类提取操作
 * （`get`/`peek`/`read`/`ignore`/`putback`）的 `istream_operators` 混入基类，
 * 以及提取运算符 `operator>>`：一条泛型的（把类型分派到扩展点
 * `io_traits<TChar, TValue>`），外加一条给 `ios_base` 函数指针操纵符的。
 * @endif
 *
 * @lang{EN}
 * Defines the formatted and unformatted extraction facilities for input streams.
 * Includes the input sentry `in_sentry`, the input-stream concept `istream_type`,
 * the `istream_operators` mix-in base that carries the various extraction operations
 * (`get`/`peek`/`read`/`ignore`/`putback`), and the extraction `operator>>`: one generic
 * (dispatching the type to the `io_traits<TChar, TValue>` extension point) plus one for
 * function-pointer `ios_base` manipulators.
 * @endif
 */
#pragma once

#include <common/defs.h>
#include <common/metafunctions.h>
#include <facet/ctype.h>
#include <io/traits/traits_base.h>
#include <io/io_base.h>
#include <io/streambuf_iterator.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <iterator>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 输入操作的 RAII 哨兵：在每次提取操作的入口统一完成前置准备。
 *
 * 哨兵负责校验流的有效性、刷新关联（tie）流，并在需要时跳过前导空白，
 * 从而让每个提取操作以一致的前置条件开始。哨兵不可拷贝、不可移动。
 * @tparam TStream 关联的输入流类型。
 * @tparam involve_output 若为 `true`，表示该流同时支持输出，构造时会将底层缓冲区
 *                        切换到读取模式（`switch_to_get`）。
 * @endif
 *
 * @lang{EN}
 * @brief RAII sentry for input operations: performs the shared setup at the entry of
 *        every extraction operation.
 *
 * The sentry validates the stream, flushes the tied stream,
 * and optionally skips leading whitespace, so that each extraction begins with a
 * consistent set of preconditions. The sentry is neither copyable nor movable.
 * @tparam TStream The associated input stream type.
 * @tparam involve_output If `true`, the stream also supports output, and construction
 *                        switches the underlying buffer to get mode (`switch_to_get`).
 * @endif
 */
template <typename TStream, bool involve_output>
struct in_sentry
{
    /**
     * @lang{ZH}
     * @brief 构造输入哨兵：校验流、刷新关联流，并按需跳过前导空白。
     *
     * @warning **调用方必须已经持有 `is.io_mutex()`，且要持到自己的 `catch` 之后。** 哨兵自己
     *          不加锁：它在 `try` 块末尾就析构了，而 `catch` 里的 `handle_exception` 需要在锁内
     *          更新流状态，才能让成功路径与失败路径对同一把 `io_mutex()` 的可见性保持一致。锁因此
     *          必须是调用方的局部变量，而不是哨兵的成员。该前置条件无法在运行期校验——
     *          `copyable_mutex` 不记录属主，递归锁的 `try_lock()` 也分不清"我已持有"与"无人持有"。
     *
     * 关联流的刷新走 `abs_flusher::try_flush()`，取不到对方的锁就跳过，绝不阻塞。本线程因此可以
     * 安全地在持有本流锁的状态下发起它：tie 这条用户看不见的加锁边永远不会成为等待边，死锁只可能
     * 由用户自己能定序的锁构成。
     * @param is 要操作的输入流。
     * @param noskip 若为 `true`，则不跳过前导空白；若为 `false`，按当前 locale 的
     *               空白定义跳过前导空白。
     * @throw stream_error 若流无效，或缺少 ctype facet。
     * @throw eof_error 若在跳过空白的过程中到达输入结尾。
     * @endif
     * @lang{EN}
     * @brief Constructs the input sentry: validates the stream, flushes the tied stream, and
     * skips leading whitespace if requested.
     *
     * @warning **The caller must already hold `is.io_mutex()`, and must keep holding it past its
     *          own `catch`.** The sentry does not lock: it is destroyed at the end of the
     *          enclosing `try`, while `handle_exception` in the `catch` needs the lock to update
     *          the stream state, so that the success and failure paths stay consistent with
     *          respect to the same `io_mutex()`. The lock therefore has to be a local of the
     *          caller rather than a member of the sentry. The precondition cannot be checked at
     *          run time -- `copyable_mutex` tracks no owner, and a recursive mutex's `try_lock()`
     *          cannot tell "this thread already holds it" from "nobody holds it".
     *
     * The tied stream is flushed through `abs_flusher::try_flush()`, which skips the flush rather
     * than wait when the target's lock cannot be taken. This thread can therefore start it safely
     * while holding its own stream's lock: the tie edge -- the one lock edge the user cannot see
     * -- never becomes a waiting edge, so any deadlock can only be built from locks the user is
     * able to order.
     * @param is The input stream to operate on.
     * @param noskip If `true`, leading whitespace is not skipped; if `false`, leading
     *               whitespace is skipped according to the current locale's definition.
     * @throw stream_error If the stream is invalid or the ctype facet is missing.
     * @throw eof_error If end of input is reached while skipping whitespace.
     * @endif
     */
    in_sentry(TStream& is, bool noskip)
        : m_is(is)
    {
        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");

        if (auto* tied = is.tie())
        {
            try { tied->try_flush(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }

        if constexpr (involve_output)
            is.m_streambuf.switch_to_get();

        if (!noskip)
        {
            try
            {
                auto ct = is.m_locale.template get<IOv2::ctype<typename TStream::char_type>>();
                if (!ct)
                    throw stream_error{"istream ignore_ws fail: no ctype facet"};
                auto c = is.m_streambuf.sgetc();
                while (c.has_value() &&
                        ct->is_any(base_ft<ctype>::space, c.value()))
                {
                    c = is.m_streambuf.snextc();
                }

                if (!c.has_value())
                    throw eof_error{};
            }
            catch(...)
            {
                is.handle_exception(std::current_exception());
            }
        }

        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");
    }

    ~in_sentry() = default;

    in_sentry(const in_sentry&) = delete;
    in_sentry& operator=(const in_sentry&) = delete;
    in_sentry(in_sentry&&) = delete;
    in_sentry& operator=(in_sentry&&) = delete;

private:
    TStream& m_is;
};

/**
 * @lang{ZH}
 * @brief 类型特征的主模板：默认判定任意类型都不是 `in_sentry`。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Primary template of the type trait: by default any type is not an `in_sentry`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename>
struct is_in_sentry_impl
{
    constexpr static bool value = false;
};

/**
 * @lang{ZH}
 * @brief `is_in_sentry_impl` 的偏特化：对 `in_sentry` 实例判定为真。
 * @endif
 *
 * @lang{EN}
 * @brief Partial specialization of `is_in_sentry_impl`: true for instances of `in_sentry`.
 * @endif
 */
template <typename TStream, bool involve_output>
struct is_in_sentry_impl<in_sentry<TStream, involve_output>>
{
    constexpr static bool value = true;
};

/**
 * @lang{ZH}
 * @brief 判定某类型是否为 `in_sentry` 实例的概念。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept that checks whether a type is an instance of `in_sentry`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename T>
concept is_in_sentry = is_in_sentry_impl<T>::value;

/**
 * @lang{ZH}
 * @brief 分隔符处理策略标签：**消费**分隔符（提取后从流中移除，getline 语义）。
 * @endif
 *
 * @lang{EN}
 * @brief Delimiter-policy tag: **consume** the delimiter (removed from the stream after
 *        extraction, getline semantics).
 * @endif
 */
struct cons_sep;

/**
 * @lang{ZH}
 * @brief 分隔符处理策略标签：**保留**分隔符（留在流中，get 语义）。
 * @endif
 *
 * @lang{EN}
 * @brief Delimiter-policy tag: **keep** the delimiter (left in the stream, get semantics).
 * @endif
 */
struct keep_sep;

/**
 * @lang{ZH}
 * @brief C 字符串处理策略标签：在输出末尾**追加**空字符结尾（`'\0'`）。
 * @endif
 *
 * @lang{EN}
 * @brief C-string-policy tag: **append** a null terminator (`'\0'`) at the end of the output.
 * @endif
 */
struct app_zt;

/**
 * @lang{ZH}
 * @brief C 字符串处理策略标签：**不**追加空字符结尾。
 * @endif
 *
 * @lang{EN}
 * @brief C-string-policy tag: do **not** append a null terminator.
 * @endif
 */
struct no_zt;

template <typename TChar>
struct istream_operators;

/**
 * @lang{ZH}
 * @brief 输入流类型的概念。
 *
 * 一个类型要成为输入流，必须提供 `in_sentry_type` 与 `char_type` 类型、可返回其
 * locale，且其 `in_sentry_type` 满足 `is_in_sentry`；同时它必须派生自
 * `ios_base<char_type>`、`io_state_and_exp` 与 `istream_operators<char_type>`。
 * @note `io_state_and_exp` 这一条是必需的，不只是描述性的：本概念约束下的代码会直接调用
 *       `handle_exception()`（提取运算符的 `catch`、`in_sentry` 的构造）与 `operator bool`。
 *       缺了它，这些调用要到模板**体**实例化时才报错，诊断落在库的内部实现里，而不是落在
 *       “这个类型不是输入流”上。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept for an input stream type.
 *
 * To qualify as an input stream, a type must expose the `in_sentry_type` and `char_type`
 * types, be able to return its locale, and have an `in_sentry_type` that satisfies
 * `is_in_sentry`; it must also derive from `ios_base<char_type>`, `io_state_and_exp` and
 * `istream_operators<char_type>`.
 * @note The `io_state_and_exp` clause is a requirement, not just a description: code
 *       constrained by this concept calls `handle_exception()` (the extraction operator's
 *       `catch`, `in_sentry`'s constructor) and `operator bool` directly. Without it those
 *       calls only fail once the template **body** is instantiated, putting the diagnostic
 *       deep inside the library's implementation rather than on "this type is not an input
 *       stream".
 * @tparam T The type under inspection.
 * @endif
 */
template <typename T>
concept istream_type =
    requires (T a)
    {
        typename T::in_sentry_type;
        typename T::char_type;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_in_sentry<typename T::in_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>> &&
    std::derived_from<T, io_state_and_exp> &&
    std::derived_from<T, istream_operators<typename T::char_type>>;

/**
 * @lang{ZH}
 * @brief 为输入流提供各类提取操作的混入（mix-in）基类。
 *
 * 本模板集中承载 `get`、`peek`、`read`、`ignore`、`putback` 等成员，供具体输入流类型
 * 派生使用；这些成员通过 deducing-this（`this TSelf& self`）以派生类的具体类型执行操作。
 * 每个操作先取本流的 `io_mutex()`，再在锁内构造输入哨兵以完成前置准备，并将异常统一交由流的
 * `handle_exception` 处理，从而按异常掩码更新流状态。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Mix-in base that provides the various extraction operations for input streams.
 *
 * This template centralizes members such as `get`, `peek`, `read`, `ignore`, and `putback`
 * for concrete input stream types to derive from; these members use deducing-this
 * (`this TSelf& self`) to operate on the concrete derived type. Each operation takes the
 * stream's `io_mutex()` and then constructs an input sentry under it to handle the setup, and
 * routes exceptions through the stream's `handle_exception`, which updates the stream state
 * according to the exception mask.
 * @tparam TChar The character type.
 * @endif
 */
template <typename TChar>
struct istream_operators
{
    /**
     * @lang{ZH}
     * @brief 从流中提取单个字符并以 `std::optional` 返回。
     *
     * 这是非格式化提取，不跳过前导空白。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 提取到的字符；若无字符可提取（到达 EOF），则返回空的 `std::optional`。
     * @note 若无字符可提取，会置位 `eofbit`/`failbit`（按流的异常掩码，可能改为抛出）。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts a single character from the stream and returns it as a `std::optional`.
     *
     * This is an unformatted extraction and does not skip leading whitespace.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The extracted character; an empty `std::optional` if no character could be
     *         extracted (EOF reached).
     * @note If no character can be extracted, `eofbit`/`failbit` are set (or, subject to the
     *       stream's exception mask, an exception is thrown instead).
     * @endif
     */
    template <typename TSelf>
    std::optional<TChar> get(this TSelf& self)
    {
        std::optional<TChar> c;
        bool at_eof = false;
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            c = self.m_streambuf.sbumpc();
            if (!c.has_value())
            {
                at_eof = true;
                throw stream_error{"istream get fail: no character extracted"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }
        return c;
    }

    /**
     * @lang{ZH}
     * @brief 从流中提取单个字符并存入 `c`。
     *
     * 这是非格式化提取，不跳过前导空白。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param c 用于接收提取字符的引用；仅在成功提取时被写入。
     * @return 流自身的引用。
     * @note 若无字符可提取，会置位 `eofbit`/`failbit`，且 `c` 保持不变。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts a single character from the stream and stores it in `c`.
     *
     * This is an unformatted extraction and does not skip leading whitespace.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param c Reference receiving the extracted character; written only on success.
     * @return A reference to the stream itself.
     * @note If no character can be extracted, `eofbit`/`failbit` are set and `c` is left
     *       unchanged.
     * @endif
     */
    template <typename TSelf>
    TSelf& get(this TSelf& self, TChar& c)
    {
        bool at_eof = false;
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            auto tmp = self.m_streambuf.sbumpc();
            if (tmp.has_value()) c = tmp.value();
            else
            {
                at_eof = true;
                throw stream_error{"istream get fail: no character extracted"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 将字符提取到输出序列，直到遇到分隔符、缓冲区满或到达 EOF。
     *
     * 这是 `get`/`getline` 系列的策略化底层实现，行为由两个策略标签控制：
     * - `DelimPolicy`：`cons_sep` 表示消费分隔符（getline 语义，分隔符被读走但不写入
     *   输出）；`keep_sep` 表示保留分隔符（get 语义，分隔符留在流中）。
     * - `CStrPolicy`：`app_zt` 表示在末尾追加空字符结尾（此时最多写入 `n-1` 个字符）；
     *   `no_zt` 表示不追加结尾。
     * @tparam DelimPolicy 分隔符处理策略：`cons_sep` 或 `keep_sep`。
     * @tparam CStrPolicy C 字符串处理策略：`app_zt` 或 `no_zt`。
     * @tparam TOut 输出目标类型：可为指针或输出迭代器。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param s 输出目标（指针或输出迭代器）。
     * @param n 缓冲区容量；当 `CStrPolicy` 为 `app_zt` 时，最多写入 `n-1` 个字符。
     * @param delim 分隔符。
     * @return 指向最后一个写入位置之后的输出迭代器/指针。
     * @throw stream_error 若 `s` 为空指针、`n` 为 0、未提取到任何字符，或
     *        （`cons_sep` 下）在缓冲区容量内未找到分隔符。
     * @note 到达 EOF 时置位 `eofbit`。在异常路径上，若为 `app_zt`，仍会尽力写入空字符结尾。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts characters into an output sequence until a delimiter, a full buffer,
     *        or EOF is reached.
     *
     * This is the policy-based low-level implementation behind the `get`/`getline` family;
     * its behavior is controlled by two policy tags:
     * - `DelimPolicy`: `cons_sep` consumes the delimiter (getline semantics; the delimiter is
     *   read but not written to the output); `keep_sep` keeps it (get semantics; the delimiter
     *   stays in the stream).
     * - `CStrPolicy`: `app_zt` appends a null terminator (at most `n-1` characters are then
     *   written); `no_zt` appends no terminator.
     * @tparam DelimPolicy Delimiter policy: `cons_sep` or `keep_sep`.
     * @tparam CStrPolicy C-string policy: `app_zt` or `no_zt`.
     * @tparam TOut The output target type: either a pointer or an output iterator.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param s The output target (pointer or output iterator).
     * @param n The buffer capacity; when `CStrPolicy` is `app_zt`, at most `n-1` characters
     *          are written.
     * @param delim The delimiter.
     * @return An output iterator/pointer past the last written position.
     * @throw stream_error If `s` is a null pointer, `n` is 0, no character was extracted, or
     *        (under `cons_sep`) the delimiter was not found within the buffer capacity.
     * @note Sets `eofbit` on reaching EOF. On the exception path, a null terminator is still
     *       best-effort written when `app_zt` is in effect.
     * @endif
     */
    template <typename DelimPolicy, typename CStrPolicy, typename TOut, typename TSelf>
        requires ((std::is_same_v<DelimPolicy, cons_sep> || std::is_same_v<DelimPolicy, keep_sep>) &&
                  (std::is_same_v<CStrPolicy, app_zt> || std::is_same_v<CStrPolicy, no_zt>))
    TOut get(this TSelf& self, TOut s, size_t n, TChar delim)
    {
        constexpr bool is_cstr = std::is_same_v<CStrPolicy, app_zt>;

        size_t gcount = 0;
        bool at_eof = false;
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            if constexpr (std::is_pointer_v<TOut>)
            {
                if (s == nullptr)
                    throw stream_error("istream get fail: null character sequence");
            }
            if (n == 0)
                throw stream_error("istream get fail: zero buffer size");
            auto c = self.m_streambuf.sgetc();
            while ((gcount + is_cstr < n) &&
                   (c.has_value()) &&
                   (c.value() != delim))
            {
                *s++ = c.value();
                ++gcount;
                c = self.m_streambuf.snextc();
            }

            at_eof = (gcount + is_cstr < n) && (!c.has_value());

            if constexpr (std::is_same_v<DelimPolicy, cons_sep>)
            {
                if (c.has_value())
                {
                    if (c.value() == delim)
                    {
                        self.m_streambuf.sbumpc();
                        ++gcount;
                    }
                    else
                        throw stream_error("istream getline fail: delimiter not found within buffer capacity");
                }
            }

            if (gcount == 0)
                throw stream_error{"istream get fail: no character extracted"};

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            if constexpr (is_cstr)
            {
                if constexpr (std::is_pointer_v<TOut>)
                {
                    if (s != nullptr && n != 0)
                        *s++ = TChar{};
                }
                else if (n != 0)
                    *s++ = TChar{};
            }
            self.handle_exception(std::current_exception(), at_eof);
            return s;
        }

        if constexpr (is_cstr)
            *s++ = TChar{};
        return s;
    }

    /**
     * @lang{ZH}
     * @brief 将字符提取到输出序列，直到遇到换行符、缓冲区满或到达 EOF。
     *
     * 等价于以当前 locale 下 `'\n'` 的宽化结果作为分隔符调用三参数版本
     * `get<DelimPolicy, CStrPolicy>(s, n, delim)`。
     * @tparam DelimPolicy 分隔符处理策略：`cons_sep` 或 `keep_sep`。
     * @tparam CStrPolicy C 字符串处理策略：`app_zt` 或 `no_zt`。
     * @tparam TOut 输出目标类型：可为指针或输出迭代器。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param s 输出目标（指针或输出迭代器）。
     * @param n 缓冲区容量；当 `CStrPolicy` 为 `app_zt` 时，最多写入 `n-1` 个字符。
     * @return 指向最后一个写入位置之后的输出迭代器/指针。
     * @throw stream_error 若缺少 ctype facet（用于宽化 `'\n'`）；其余异常与三参数版本一致。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts characters into an output sequence until a newline, a full buffer,
     *        or EOF is reached.
     *
     * Equivalent to calling the three-argument overload
     * `get<DelimPolicy, CStrPolicy>(s, n, delim)` with the delimiter set to the widened
     * `'\n'` under the current locale.
     * @tparam DelimPolicy Delimiter policy: `cons_sep` or `keep_sep`.
     * @tparam CStrPolicy C-string policy: `app_zt` or `no_zt`.
     * @tparam TOut The output target type: either a pointer or an output iterator.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param s The output target (pointer or output iterator).
     * @param n The buffer capacity; when `CStrPolicy` is `app_zt`, at most `n-1` characters
     *          are written.
     * @return An output iterator/pointer past the last written position.
     * @throw stream_error If the ctype facet (used to widen `'\n'`) is missing; other
     *        exceptions match the three-argument overload.
     * @endif
     */
    template <typename DelimPolicy, typename CStrPolicy, typename TOut, typename TSelf>
        requires ((std::is_same_v<DelimPolicy, cons_sep> || std::is_same_v<DelimPolicy, keep_sep>) &&
                  (std::is_same_v<CStrPolicy, app_zt> || std::is_same_v<CStrPolicy, no_zt>))
    TOut get(this TSelf& self, TOut s, size_t n)
    {
        TChar delim;
        {
            std::lock_guard guard(self.io_mutex());
            try
            {
                auto ct = self.m_locale.template get<IOv2::ctype<TChar>>();
                if (!ct)
                    throw stream_error{"istream get fail: no ctype facet"};
                delim = ct->widen('\n');
            }
            catch(...)
            {
                if constexpr (std::is_same_v<CStrPolicy, app_zt>)
                {
                    if constexpr (std::is_pointer_v<TOut>)
                    {
                        if (s != nullptr && n != 0)
                            *s++ = TChar{};
                    }
                    else if (n != 0)
                        *s++ = TChar{};
                }
                self.handle_exception(std::current_exception());
                return s;
            }
        }

        return self.template get<DelimPolicy, CStrPolicy, TOut>(s, n, delim);
    }

    /**
     * @lang{ZH}
     * @brief 查看流中的下一个字符但不提取它。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 下一个字符；若已到达 EOF，则返回空的 `std::optional`。
     * @throw eof_error 若已到达 EOF（按流的异常掩码，可能改为置位状态位）。
     * @endif
     *
     * @lang{EN}
     * @brief Reads the next character in the stream without extracting it.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The next character; an empty `std::optional` if EOF has been reached.
     * @throw eof_error If EOF has been reached (or, subject to the stream's exception mask,
     *        a status bit is set instead).
     * @endif
     */
    template <typename TSelf>
    std::optional<TChar> peek(this TSelf& self)
    {
        std::optional<TChar> c;
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            c = self.m_streambuf.sgetc();
            if (!c.has_value()) throw eof_error{};
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return c;
    }

    /**
     * @lang{ZH}
     * @brief 从流中读取恰好 `n` 个字符到 `s`。
     *
     * 这是非格式化提取。若在读满 `n` 个字符前到达 EOF，则视为失败。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param s 目标缓冲区。当 `n != 0` 时不得为空指针。
     * @param n 要读取的字符数。
     * @return 指向最后一个写入位置之后的指针（即 `s + 实际读取数`）。
     * @throw stream_error 若 `s` 为空指针而 `n != 0`，或无法读满 `n` 个字符。
     * @note 未能读满时置位 `eofbit`。
     * @endif
     *
     * @lang{EN}
     * @brief Reads exactly `n` characters from the stream into `s`.
     *
     * This is an unformatted extraction. Reaching EOF before `n` characters are read is
     * treated as a failure.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param s The destination buffer. Must not be a null pointer when `n != 0`.
     * @param n The number of characters to read.
     * @return A pointer past the last written position (i.e. `s + characters actually read`).
     * @throw stream_error If `s` is a null pointer while `n != 0`, or if `n` characters could
     *        not be read.
     * @note Sets `eofbit` when fewer than `n` characters could be read.
     * @endif
     */
    template <typename TSelf>
    TChar* read(this TSelf& self, TChar* s, size_t n)
    {
        size_t gcount = 0;
        bool at_eof = false;
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            if (s == nullptr && n != 0)
                throw stream_error{"istream read fail: null character sequence"};
            self.m_streambuf.sgetn(s, n, &gcount);
            if (gcount != n)
            {
                at_eof = true;
                throw stream_error{"istream read fail: cannot read enough characters"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }
        return s + gcount;
    }

    /**
     * @lang{ZH}
     * @brief 丢弃流中最多 `n` 个字符。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param n 要丢弃的字符数，默认为 1。
     * @return 流自身的引用。
     * @note 若在丢弃 `n` 个字符前到达 EOF，则置位 `eofbit`。
     * @endif
     *
     * @lang{EN}
     * @brief Discards up to `n` characters from the stream.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param n The number of characters to discard; defaults to 1.
     * @return A reference to the stream itself.
     * @note Sets `eofbit` if EOF is reached before `n` characters are discarded.
     * @endif
     */
    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n = 1)
    {
        bool at_eof = false;
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);

            for (size_t gcount = 0; gcount < n; ++gcount)
            {
                if (self.m_streambuf.is_eof())
                {
                    at_eof = true;
                    break;
                }
                self.m_streambuf.sbumpc();
            }

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 丢弃流中最多 `n` 个字符，直到遇到并丢弃分隔符为止。
     *
     * 若在丢弃 `n` 个字符之内遇到 `delim`，则该分隔符也会被丢弃（计入丢弃计数）。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param n 最多丢弃的字符数。为 0 时直接返回。
     * @param delim 分隔符。
     * @return 流自身的引用。
     * @note 若在遇到分隔符或丢弃满 `n` 个字符前到达 EOF，则置位 `eofbit`。
     * @endif
     *
     * @lang{EN}
     * @brief Discards up to `n` characters from the stream, up to and including a delimiter.
     *
     * If `delim` is encountered within `n` characters, that delimiter is discarded as well
     * (counted toward the discard count).
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param n The maximum number of characters to discard. Returns immediately if 0.
     * @param delim The delimiter.
     * @return A reference to the stream itself.
     * @note Sets `eofbit` if EOF is reached before the delimiter is found or `n` characters
     *       are discarded.
     * @endif
     */
    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n, TChar delim)
    {
        size_t gcount = 0;
        bool at_eof = false;
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            if (n == 0) return self;

            auto c = self.m_streambuf.sgetc();
            while (gcount < n
                    && c.has_value()
                    && (c.value() != delim))
            {
                ++gcount;
                c = self.m_streambuf.snextc();
            }

            at_eof = (gcount < n) && (!c.has_value());

            if (gcount < n)
            {
                if (c.has_value())
                {
                    ++gcount;
                    self.m_streambuf.sbumpc();
                }
            }

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 将字符 `c` 放回流中，使其成为下一个被读取的字符。
     *
     * 放回前会清除 `eofbit`，因此可在到达 EOF 后重新放回字符继续读取。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param c 要放回的字符。
     * @return 流自身的引用。
     * @note 若底层缓冲区不支持放回该字符，会置位失败状态（按流的异常掩码可能改为抛出）。
     * @endif
     *
     * @lang{EN}
     * @brief Puts the character `c` back into the stream so it becomes the next character read.
     *
     * `eofbit` is cleared before the put-back, so a character can be put back and read again
     * after EOF was reached.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param c The character to put back.
     * @return A reference to the stream itself.
     * @note If the underlying buffer cannot put the character back, a failure state is set
     *       (or, subject to the stream's exception mask, an exception is thrown).
     * @endif
     */
    template <typename TSelf>
    TSelf& putback(this TSelf& self, TChar c)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            self.unset_state(IOv2::ios_defs::eofbit);
            self.m_streambuf.sputbackc(c);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 取输入迭代器；可选地附加一个“已观察到输入结束”的报告位。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param saw_eof 可选的报告位；生存期必须覆盖迭代器及其所有副本，`nullptr` 表示不
     *                上报。详见 istreambuf_iterator。
     * @return 绑定到本流缓冲区的 `istreambuf_iterator`。
     * @endif
     * @lang{EN}
     * @brief Gets an input iterator; optionally attaches an "observed end of input" flag.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param saw_eof Optional report flag; its lifetime must cover the iterator and all
     *                copies, `nullptr` means do not report. See istreambuf_iterator.
     * @return An `istreambuf_iterator` bound to this stream's buffer.
     * @endif
     */
private:
    template <typename TSelf>
    auto i_iter(this TSelf& self, bool* saw_eof = nullptr)
    {
        return istreambuf_iterator(self.m_streambuf, saw_eof);
    }

    /**
     * @lang{ZH}
     * @brief 声明格式化提取运算符为友元，使其可访问私有的 `i_iter`。
     * @endif
     *
     * @lang{EN}
     * @brief Befriends the formatted extraction operator so it can access the private `i_iter`.
     * @endif
     */
    template <istream_type U, typename TValue>
    friend U& operator>>(U& obj, TValue&& value);
};

/**
 * @lang{ZH}
 * @brief 提取运算符：从流中解析出一个 `TValue` 类型的值，或应用一个操纵符。
 *
 * 本运算符是提取侧**唯一**的入口。它在 `if constexpr` 里挑出可用的通道：流形式
 * `sread(stream, value)`（操纵符）与迭代器形式 `sread(iter, end, io, loc, value)`（格式化提取，
 * 目标类型先过一道解析上下文 `TCtx`）。都不可用时就地 `static_assert`，并按"扩展点是否存在"
 * 给出两条不同的诊断。与插入侧不同，这里**不做退化**：提取按引用写回，数组目标退化成指针就会
 * 丢掉长度。
 *
 * @note 同一个 `io_traits` 特化**只应提供其中一种形式**的 `sread`。两种形式靠实参个数区分、
 *       不会互相误配，但同时提供时选中哪一种**不作保证**。
 * @note 形参是转发引用而非 `TValue&`。工厂函数产出的操纵符（`setw(5)`、`get_money(x)`）都是
 *       纯右值，绑不上非常量左值引用；从前那是靠两条按值传的 `operator>>` 特事特办的。改成
 *       转发引用之后，"能不能写进去"由第二档的探测自己判定：右值目标一律以 `const` 左值
 *       (`TTarget`) 探测——`get_money(x)` 的 `sread` 不修改操纵符对象本身、收 const 引用，
 *       因而通过；而 `int` 的 `sread` 收 `int&`，`is >> 5` 便落到 `static_assert`，不会静默地
 *       解析进一个临时量。常量左值目标同理被挡下。
 * @note 探测里直接写 `io_traits<TChar, TCtx>::sread(...)` 是安全的：未特化时它是不完整类型，
 *       在 requires 表达式里属于可 SFINAE 的替换失败，结果为 `false` 而非硬错误。
 * @tparam T 输入流类型。
 * @tparam TValue 目标类型（或操纵符类型）。
 * @param obj 输入流。
 * @param value 接收解析结果的对象，或操纵符。
 * @return 流自身的引用。
 * @throw stream_error 若在未提取到任何值时已处于 EOF。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction operator: parses a value of type `TValue` from the stream, or applies a
 *        manipulator.
 *
 * This is the **only** entry point on the extraction side. An `if constexpr` chain picks whichever
 * channel is usable: the stream form `sread(stream, value)` (manipulators) or the iterator form
 * `sread(iter, end, io, loc, value)` (formatted extraction, with the target type relayed through
 * the parse-context type `TCtx`). When neither is usable a `static_assert` fires in place, with
 * one of two diagnostics depending on whether the extension point exists at all. Unlike the
 * insertion side there is **no decay rung**: extraction writes back through a reference, and
 * decaying an array target to a pointer would throw away its length.
 *
 * @note One `io_traits` specialization should provide **only one of the two forms** of `sread`.
 *       The forms differ in argument count and so cannot be mistaken for each other, but which
 *       one is picked when both are present is **unspecified**.
 * @note The parameter is a forwarding reference rather than `TValue&`. Manipulators produced by
 *       a factory (`setw(5)`, `get_money(x)`) are prvalues and cannot bind to a non-const lvalue
 *       reference; that used to be worked around with two by-value `operator>>` overloads. With
 *       a forwarding reference, "can this be written into" is decided by the second rung's own
 *       probe: an rvalue target is always probed as a `const` lvalue (`TTarget`) -- the `sread`
 *       of `get_money(x)` does not modify the manipulator object and takes a const reference, so
 *       it passes, while the one for `int` takes `int&`, so `is >> 5` lands on the
 *       `static_assert` instead of silently parsing into a temporary. A const lvalue target is
 *       rejected the same way.
 * @note Naming `io_traits<TChar, TCtx>::sread(...)` directly in the probes is safe: where it is
 *       not specialized it is an incomplete type, which inside a requires-expression is a
 *       SFINAE-able substitution failure yielding `false` rather than a hard error.
 * @tparam T The input stream type.
 * @tparam TValue The target type (or the manipulator type).
 * @param obj The input stream.
 * @param value The object receiving the parsed result, or the manipulator.
 * @return A reference to the stream itself.
 * @throw stream_error If EOF is already reached with no value extracted.
 * @endif
 */
template <istream_type T, typename TValue>
T& operator>>(T& obj, TValue&& value)
{
    using TChar   = typename T::char_type;
    using TV      = std::remove_cvref_t<TValue>;
    using TCtx    = typename parse_context_type<TChar, TV>::type;
    using TTarget = std::conditional_t<std::is_lvalue_reference_v<TValue>,
                                       std::remove_reference_t<TValue>, const TV>;

    constexpr bool stream_r = requires
        { io_traits<TChar, TV>::sread(obj, value); };
    constexpr bool iter_v   = std::is_same_v<TCtx, TV> && requires (TTarget& v)
        { io_traits<TChar, TV>::sread(obj.i_iter(), std::default_sentinel,
                                      obj, obj.locale(), v); };
    constexpr bool iter_c   = !std::is_same_v<TCtx, TV> && requires (TTarget& v, TCtx& c)
        {
            io_traits<TChar, TCtx>::sread(obj.i_iter(), std::default_sentinel,
                                          obj, obj.locale(), c);
            v = static_cast<TV>(c);
        };

    if constexpr (stream_r)
    {
        try
        {
            io_traits<TChar, TV>::sread(obj, value);
        }
        catch (...)
        {
            obj.handle_exception(std::current_exception());
        }
    }
    else if constexpr (iter_v || iter_c)
    {
        using sentry_type = typename T::in_sentry_type;

        bool saw_eof = false;
        std::lock_guard guard(obj.io_mutex());
        try
        {
            auto iter = obj.i_iter(&saw_eof);
            bool skip = bool(obj.flags() & ios_defs::skipws);
            sentry_type cerb(obj, !skip);

            if (obj.eof())
                throw stream_error("istream extraction fail: reached EOF with no value extracted");

            if constexpr (iter_v)
                io_traits<TChar, TV>::sread(iter, std::default_sentinel, obj, obj.locale(), value);
            else
            {
                TCtx tmp = [&value]() -> TCtx {
                    if constexpr (requires (const TV& v)
                                  { parse_context_type<TChar, TV>::make_parse_context(v); })
                        return parse_context_type<TChar, TV>::make_parse_context(value);
                    else
                        return TCtx{};
                }();
                io_traits<TChar, TCtx>::sread(iter, std::default_sentinel, obj, obj.locale(), tmp);
                value = static_cast<TV>(tmp);
            }

            if (saw_eof) obj.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            obj.handle_exception(std::current_exception(), saw_eof);
        }
    }
    else if constexpr (has_io_traits<TChar, TV> || has_io_traits<TChar, TCtx>)
        static_assert(dependent_false_v<TValue>,
            "IOv2: cannot extract into this type. An io_traits<char_type, TValue> exists but "
            "offers no sread() usable with this stream. The usual cause is a direction mismatch "
            "-- the type provides swrite() only and is meant for insertion (write `os << x`, not "
            "`is >> x`). Otherwise the target is not writable: extraction needs a non-const "
            "lvalue, so `is >> 5` and `is >> const_obj` are rejected here. A fill character whose "
            "type differs from the stream's char_type lands here too. "
            "See io/traits/traits_base.h.");
    else
        static_assert(dependent_false_v<TValue>,
            "IOv2: cannot extract into this type. No io_traits<char_type, TValue> is defined for "
            "it. Define one -- see io/traits/traits_base.h -- or extract into a supported type "
            "such as an arithmetic type, a character type, CharT* or std::basic_string.");

    return obj;
}

/**
 * @lang{ZH}
 * @brief 提取运算符：应用一个作用于 `ios_base<char_type>&` 的函数指针操纵符。
 *
 * 与插入侧同形状的那条完全对称，理由与行为都相同；详见
 * `operator<<(T&, void (*)(ios_base<typename T::char_type>&))`。
 * @param obj 输入流。
 * @param pf 操纵符；为空指针时置 `strfailbit`。
 * @return 流自身的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction operator: applies a function-pointer manipulator acting on
 *        `ios_base<char_type>&`.
 *
 * Exactly symmetric to the insertion-side overload of the same shape, with the same reasons and
 * the same behaviour; see `operator<<(T&, void (*)(ios_base<typename T::char_type>&))`.
 * @param obj The input stream.
 * @param pf The manipulator; a null pointer sets `strfailbit`.
 * @return A reference to the stream itself.
 * @endif
 */
template <istream_type T>
T& operator>>(T& obj, void (*pf)(ios_base<typename T::char_type>&))
{
    apply_ios_manip<typename T::char_type>(obj, pf);
    return obj;
}

}

/**
 * @file ori_facet_buf.h
 * @lang{ZH}
 * 定义了 `ori_facet_buf` 单例——基础 facet 与 messages facet 的进程级缓存 / 驻留
 * （interning）池，并在程序启动时从环境变量解析各 `LC_*` 类别的初始 locale 名称。
 * 同时定义 messages 缓存的键类型 `detail::msg_key` 及其 `std::hash` 特化。
 * @endif
 *
 * @lang{EN}
 * Defines the `ori_facet_buf` singleton -- a process-wide cache / interning pool for
 * base facets and messages facets -- and resolves the initial per-`LC_*` locale names
 * from the environment at program startup. Also defines the messages-cache key type
 * `detail::msg_key` and its `std::hash` specialization.
 * @endif
 */
#pragma once
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <common/iov2_export.h>
#include <common/lru_cache.h>
#include <common/sing_temp.h>
#include <facet/facet_common.h>
#include <facet/messages_details.h>

#include <clocale>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace IOv2::detail
{
/**
 * @lang{ZH}
 * messages 缓存的键：`(domain, lang, dirname, cvt)`。
 *
 * 采用程序定义的结构体而非 `std::tuple`，以便为其特化 `std::hash`——`lru_cache`
 * 以 `std::hash<TK>` 为键，而标准库不为 `std::tuple` 提供 `std::hash`。各分量保持
 * 独立字段（而非以 '\n' 拼接的字符串），使得即便某一分量自身含有分隔符，不同的
 * `(domain, lang, dirname, cvt)` 四元组也始终映射到不同的键。
 *
 * `dirname` 是 `.mo` 文件所在目录（由 `base_ft<messages>::get_dirname(domain)` 解析）。
 * facet 的实际内容依赖它，而它可经 `bind_text_domain` 在运行期被改写；因此必须计入键，
 * 否则对同一 domain 重绑目录后、若两处目录选中同一语言，会命中并返回用旧目录构建的陈旧
 * facet。
 * @endif
 *
 * @lang{EN}
 * Key for the messages cache: `(domain, lang, dirname, cvt)`.
 *
 * A program-defined struct rather than a `std::tuple` so that `std::hash` can be
 * specialized for it -- `lru_cache` keys on `std::hash<TK>`, and the standard library
 * provides no `std::hash` for `std::tuple`. Keeping the components as distinct fields
 * (instead of a '\n'-joined string) keeps distinct `(domain, lang, dirname, cvt)`
 * tuples mapped to distinct keys even when a component itself contains the separator.
 *
 * `dirname` is the directory holding the `.mo` file (resolved by
 * `base_ft<messages>::get_dirname(domain)`). The facet's content depends on it, and it
 * can be changed at runtime via `bind_text_domain`; it must therefore be part of the
 * key. Otherwise, after rebinding a domain to a new directory, a lookup would still
 * hit and return the stale facet built from the old directory whenever both
 * directories select the same language.
 * @endif
 */
struct msg_key
{
    std::string domain;
    std::string lang;
    std::string dirname;
    std::string cvt;
    bool operator==(const msg_key&) const = default;
};
} // namespace IOv2::detail

namespace std
{
/**
 * @lang{ZH}
 * @brief `detail::msg_key` 的 `std::hash` 特化，供 `lru_cache` 使用。
 *
 * 逐字段哈希 `(domain, lang, dirname, cvt)`，再以 boost 风格的混合式（golden-ratio
 * 常量加移位）合并，从而依赖各分量各自的 `std::hash<std::string>`。
 * @endif
 *
 * @lang{EN}
 * @brief `std::hash` specialization for `detail::msg_key`, as required by `lru_cache`.
 *
 * Hashes the fields `(domain, lang, dirname, cvt)` individually and combines them with
 * a boost-style mixer (a golden-ratio constant plus shifts), relying on each
 * component's own `std::hash<std::string>`.
 * @endif
 */
template <>
struct hash<IOv2::detail::msg_key>
{
    std::size_t operator()(const IOv2::detail::msg_key& k) const noexcept
    {
        // Golden-ratio constant sized to std::size_t (2^bits / phi). The 64-bit
        // 0x9e3779b97f4a7c15 mixes the high half of a 64-bit seed that the 32-bit
        // 0x9e3779b9 leaves largely untouched; chosen at compile time so the mixer
        // matches the actual width of std::size_t on every platform. (The unselected
        // ternary branch is a well-defined explicit truncation, valid on either width.)
        constexpr std::size_t golden = sizeof(std::size_t) >= 8
            ? static_cast<std::size_t>(0x9e3779b97f4a7c15ULL)
            : static_cast<std::size_t>(0x9e3779b9UL);

        const std::hash<std::string> h;
        std::size_t seed = h(k.domain);
        seed ^= h(k.lang) + golden + (seed << 6) + (seed >> 2);
        seed ^= h(k.dirname) + golden + (seed << 6) + (seed >> 2);
        seed ^= h(k.cvt) + golden + (seed << 6) + (seed >> 2);
        return seed;
    }
};
} // namespace std

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 基础 facet 与 messages facet 的进程级缓存 / 驻留（interning）单例。
 *
 * `ori_facet_buf` 是继承自 `sing_temp` 的单例，承担两项职责：
 *
 * - **facet 驻留**：`try_get` / `try_get_msg` / `put_msg` 以 `(facet 类型 id, 名称)`
 *   或 `(facet 类型 id, msg_key)` 为键缓存不可变的 facet conf，使等价的 facet 在整个
 *   程序中最多构建一次并被共享。缓存以 `lru_cache` 有界（每种 facet 类型至多
 *   `s_cache_capacity` 条），因此即便 locale 名称 / 消息键来自可变（外部）输入，内存
 *   也保持有界。
 * - **初始 locale 名称**：构造时经 `resolve_locale` 从环境变量一次性解析各 `LC_*` 类别
 *   的名称，之后经 `locale_name` 只读提供。
 *
 * @note 线程安全由内部 `m_mutex` 提供；但 facet 的**构造发生在锁外**（见 `try_get`），
 * 以避免序列化构造，并允许 facet 构造函数重入本缓存。
 * @endif
 *
 * @lang{EN}
 * @brief Process-wide caching / interning singleton for base facets and messages facets.
 *
 * `ori_facet_buf` is a `sing_temp`-based singleton with two responsibilities:
 *
 * - **Facet interning**: `try_get` / `try_get_msg` / `put_msg` cache immutable facet
 *   confs keyed by `(facet type id, name)` or `(facet type id, msg_key)`, so an
 *   equivalent facet is built at most once program-wide and shared. The caches are
 *   bounded by `lru_cache` (at most `s_cache_capacity` entries per facet type), so
 *   memory stays bounded even when locale names / message keys come from variable
 *   (external) input.
 * - **Initial locale names**: resolved once from the environment via `resolve_locale`
 *   at construction and served read-only through `locale_name`.
 *
 * @note Thread safety is provided internally via `m_mutex`; however facet
 * *construction happens outside the lock* (see `try_get`) to avoid serializing
 * construction and to allow a facet constructor to re-enter this cache.
 * @endif
 */
class ori_facet_buf : public sing_temp<ori_facet_buf>
{
    friend sing_temp<ori_facet_buf>;

public:
    /**
     * @lang{ZH}
     * @brief 取得（必要时构建并缓存）名为 `name` 的基础 facet conf `TF`。
     *
     * 以 `(TF::id(), name)` 为键在缓存中查找：命中则返回共享实例；未命中则**在锁外**
     * 构造，再重新加锁插入。若期间有其它线程（或重入调用）已插入同键项，则返回既有项、
     * 丢弃本次构造的对象，从而保证等价 facet 在程序中唯一。
     *
     * @tparam TF 要获取的基础 facet conf 类型（提供 `TF::id()` 且可由 `name` 构造）。
     * @param name 该 facet 使用的 locale 名称。
     * @return 指向共享 facet conf 的 `std::shared_ptr<abs_ft>`。
     * @endif
     *
     * @lang{EN}
     * @brief Get (building and caching if needed) the base facet conf `TF` named `name`.
     *
     * Looks the cache up by `(TF::id(), name)`: on a hit it returns the shared instance;
     * on a miss it constructs *outside the lock*, then re-locks to insert. If another
     * thread (or a re-entrant call) inserted the same key meanwhile, the existing entry
     * is returned and the object built here is discarded, keeping an equivalent facet
     * unique program-wide.
     *
     * @tparam TF The base facet conf type to get (providing `TF::id()` and constructible
     *         from `name`).
     * @param name The locale name to use for this facet.
     * @return A `std::shared_ptr<abs_ft>` to the shared facet conf.
     * @endif
     */
    template <typename TF>
    std::shared_ptr<abs_ft> try_get(const std::string& name)
    {
        const facet_id_t id = TF::id();

        {
            std::scoped_lock guard(m_mutex);
            if (auto it = m_cache.find(id); it != m_cache.end())
            {
                if (auto cached = it->second.get(name))
                    return *cached;
            }
        }

        // Construct the facet *outside* the lock: a facet constructor may be
        // expensive and -- crucially -- may itself call back into ori_facet_buf, so
        // building under m_mutex would both serialize all construction and risk a
        // self-deadlock on this non-recursive mutex. Re-lock only to insert, and
        // double-check: if another thread (or a re-entrant call) inserted the same
        // (id, name) meanwhile, return that entry and discard the object built here.
        auto obj = std::make_shared<TF>(name);

        std::scoped_lock guard(m_mutex);
        auto& sub_cache = m_cache[id];
        if (auto cached = sub_cache.get(name))
            return *cached;
        // The get() above missed under this same lock, so the key is absent and put()
        // simply inserts (no existing value to overwrite).
        sub_cache.put(name, obj);
        return obj;
    }

    /**
     * @lang{ZH}
     * @brief 从 messages 缓存中查找已缓存的 messages facet；未命中返回 `nullptr`（只查不建）。
     *
     * 以 `(messages_conf<TChar>::id(), msg_key{domain, lang, dirname, cvt})` 为键查找。
     * 与 `try_get` 不同，本函数**不构建**新 facet——加载 / 构建由调用方
     * （`locale::involve_msg`）完成，成功后再经 `put_msg` 写回缓存。
     *
     * @tparam TChar 字符类型。
     * @param domain 文本域名称。
     * @param lang 语言字符串。
     * @param dirname `.mo` 文件所在目录。
     * @param cvt 目标编码名（默认空）。
     * @return 命中则返回 `std::shared_ptr<messages_conf<TChar>>`，否则为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Look up an already-cached messages facet; returns `nullptr` on a miss (lookup only, never builds).
     *
     * Keyed by `(messages_conf<TChar>::id(), msg_key{domain, lang, dirname, cvt})`.
     * Unlike `try_get`, this never *builds* a facet -- loading / construction is done by
     * the caller (`locale::involve_msg`) and interned afterwards via `put_msg` on
     * success.
     *
     * @tparam TChar The character type.
     * @param domain The text domain name.
     * @param lang The language string.
     * @param dirname The directory holding the `.mo` file.
     * @param cvt The target encoding name (empty by default).
     * @return A `std::shared_ptr<messages_conf<TChar>>` on a hit, or `nullptr` otherwise.
     * @endif
     */
    template <typename TChar>
    std::shared_ptr<messages_conf<TChar>> try_get_msg(const std::string& domain, const std::string& lang, const std::string& dirname, const std::string& cvt = "")
    {
        const facet_id_t id = messages_conf<TChar>::id();

        std::scoped_lock guard(m_mutex);
        if (auto it = m_msg_cache.find(id); it != m_msg_cache.end())
        {
            if (auto cached = it->second.get(detail::msg_key{.domain=domain, .lang=lang, .dirname=dirname, .cvt=cvt}))
                return std::static_pointer_cast<messages_conf<TChar>>(*cached);
        }
        return nullptr;
    }

    /**
     * @lang{ZH}
     * @brief 将一个已构建成功的 messages facet 写入 messages 缓存并返回被驻留的指针。
     *
     * 以 `(messages_conf<TChar>::id(), msg_key{...})` 为键：若在同一把锁下发现已有同键项
     * （其它线程抢先插入），则返回既有项、丢弃 `ptr`；否则插入 `ptr`。`ptr` 为空时不做
     * 任何缓存、原样返回。据此保证等价的 messages facet 唯一。
     *
     * @tparam TChar 字符类型。
     * @param ptr 待缓存的 messages facet（可为空）。
     * @param domain 文本域名称。
     * @param lang 语言字符串。
     * @param dirname `.mo` 文件所在目录。
     * @param cvt 目标编码名（默认空）。
     * @return 被驻留的共享指针：若已有同键项则为既有项，否则为 `ptr`（含 `ptr` 为空时）。
     * @endif
     *
     * @lang{EN}
     * @brief Intern a successfully-built messages facet into the messages cache and return the interned pointer.
     *
     * Keyed by `(messages_conf<TChar>::id(), msg_key{...})`: if an entry with the same
     * key already exists under the same lock (another thread inserted first), that entry
     * is returned and `ptr` is discarded; otherwise `ptr` is inserted. A null `ptr` is
     * returned as-is without caching. This keeps an equivalent messages facet unique.
     *
     * @tparam TChar The character type.
     * @param ptr The messages facet to cache (may be null).
     * @param domain The text domain name.
     * @param lang The language string.
     * @param dirname The directory holding the `.mo` file.
     * @param cvt The target encoding name (empty by default).
     * @return The interned shared pointer: the existing entry if one is present,
     *         otherwise `ptr` (including when `ptr` is null).
     * @endif
     */
    template <typename TChar>
    std::shared_ptr<messages_conf<TChar>> put_msg(std::shared_ptr<messages_conf<TChar>> ptr, const std::string& domain, const std::string& lang, const std::string& dirname, const std::string& cvt = "")
    {
        if (ptr)
        {
            const facet_id_t id = messages_conf<TChar>::id();
            const detail::msg_key key{.domain=domain, .lang=lang, .dirname=dirname, .cvt=cvt};

            std::scoped_lock guard(m_mutex);
            auto& sub_cache = m_msg_cache[id];
            if (auto cached = sub_cache.get(key))
                return std::static_pointer_cast<messages_conf<TChar>>(*cached);
            // get() missed under this same lock, so the key is absent: put() inserts.
            sub_cache.put(key, ptr);
        }
        return ptr;
    }

    /**
     * @lang{ZH}
     * 返回某个 `LC_*` 类别在**程序启动时由环境变量解析得到**的 locale 名称。
     *
     * 这些名称在单例构造（静态初始化期、`main` 之前、单线程）时由
     * `resolve_locale` 一次性解析并保存，此后只读、不再变化；因此该访问器返回的是
     * 不可变成员的常量引用，无需加锁，也不会与 `setlocale` 写者发生数据竞争——
     * 它根本不读取进程级全局 C locale。
     *
     * @note 这意味着本库的 locale 来源是**环境变量**，而**不**跟随运行期对
     * `std::setlocale` 的编程式修改。
     *
     * @throws stream_error 当 `category` 不是已解析的五个类别之一
     *         （`LC_CTYPE` / `LC_COLLATE` / `LC_MONETARY` / `LC_NUMERIC` / `LC_TIME`）时抛出，
     *         例如传入 `LC_ALL` 或 `LC_MESSAGES`。
     * @endif
     *
     * @lang{EN}
     * Return the locale name for an `LC_*` category as *resolved from the
     * environment at program startup*.
     *
     * The names are resolved once by `resolve_locale` during singleton
     * construction (static-init time, before `main`, single-threaded) and are
     * read-only thereafter; this accessor hands back a const reference to an
     * immutable member, so it needs no lock and cannot race a `setlocale` writer
     * -- it never reads the process-wide global C locale at all.
     *
     * @note Consequently the library's locale source is the *environment*; it does
     * *not* follow runtime programmatic changes via `std::setlocale`.
     *
     * @throws stream_error if `category` is not one of the five resolved categories
     *         (`LC_CTYPE` / `LC_COLLATE` / `LC_MONETARY` / `LC_NUMERIC` / `LC_TIME`),
     *         e.g. when passed `LC_ALL` or `LC_MESSAGES`.
     * @endif
     */
    const std::string& locale_name(int category) const
    {
        switch (category)
        {
        case LC_COLLATE:
            return m_collate;
        case LC_CTYPE:
            return m_ctype;
        case LC_MONETARY:
            return m_monetary;
        case LC_NUMERIC:
            return m_numeric;
        case LC_TIME:
            return m_time;
        default:
            throw stream_error(
                "locale_name: unsupported LC category " + std::to_string(category)
                + " (only LC_CTYPE/COLLATE/MONETARY/NUMERIC/TIME are resolved).");
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 单例语义：不可拷贝、不可移动；生命周期由 `sing_temp` 管理。
     * @endif
     *
     * @lang{EN}
     * @brief Singleton semantics: non-copyable and non-movable; lifetime managed by `sing_temp`.
     * @endif
     */
    ~ori_facet_buf() = default;
    ori_facet_buf(const ori_facet_buf&) = delete;
    ori_facet_buf& operator=(const ori_facet_buf&) = delete;
    ori_facet_buf(ori_facet_buf&&) = delete;
    ori_facet_buf& operator=(ori_facet_buf&&) = delete;

private:
    /**
     * @lang{ZH}
     * @brief 私有构造：单例创建时经 `resolve_locale` 从环境变量解析五个 `LC_*` 类别的
     * 初始 locale 名称。仅 `sing_temp`（友元）可调用。
     * @endif
     *
     * @lang{EN}
     * @brief Private constructor: on singleton creation, resolves the initial locale
     * names for the five `LC_*` categories from the environment via `resolve_locale`.
     * Callable only by `sing_temp` (a friend).
     * @endif
     */
    ori_facet_buf()
        : m_ctype(resolve_locale("LC_CTYPE")),
          m_collate(resolve_locale("LC_COLLATE")),
          m_monetary(resolve_locale("LC_MONETARY")),
          m_numeric(resolve_locale("LC_NUMERIC")),
          m_time(resolve_locale("LC_TIME"))
    {
    }

    /**
     * @lang{ZH}
     * 按 POSIX/glibc 中 `setlocale(category, "")` 的解析顺序，从环境变量推导出某个
     * 类别应使用的 locale 名称：`LC_ALL` > `LC_<category>` > `LANG` > `"C"`，其中
     * 每一级都要求该变量**已设置且非空**才被采用。解析出的名称随后会经 C 库校验
     * （`newlocale`）：若无法实例化（如非法的 `LANG`），回退为 `"C"`。
     *
     * @note 优先级是**短路**的，且校验在选定之后才进行：一旦较高优先级的变量已设置且非空，
     * 即采用其值，**不再**下探更低优先级的变量。因此当较高优先级的变量被设为一个无法实例化
     * 的名称（如非法的 `LC_ALL`）时，本函数直接回退到 `"C"`，**不会**改用某个本可用的较低优先级
     * 变量（如合法的 `LANG`）。这与 POSIX 语义一致——`LC_ALL` 一旦设置即无条件决定所有类别；
     * 区别仅在于 glibc 对非法名是硬失败，而本函数宽松地降级为 `"C"`。
     *
     * 仅在单例构造期（静态初始化、`main` 之前、单线程）被调用，此时不存在其它线程，
     * 也没有 `setenv` 写者，因此对 `getenv` 的读取 by-construction 无竞争。
     * @endif
     *
     * @lang{EN}
     * Derive the locale name for a category from the environment, following the
     * same precedence as POSIX/glibc `setlocale(category, "")`:
     * `LC_ALL` > `LC_<category>` > `LANG` > `"C"`, where each level is used only if
     * that variable is *set and non-empty*. The resolved name is then validated via
     * the C library (`newlocale`); if it cannot be instantiated (e.g. a bogus
     * `LANG`) it falls back to `"C"`.
     *
     * @note The precedence short-circuits, and validation happens *after* selection:
     * the first variable that is set and non-empty wins, and lower-precedence variables
     * are *not* consulted afterwards. So if a higher-precedence variable names an
     * uninstantiable locale (e.g. a bogus `LC_ALL`), this falls straight back to `"C"`
     * and does *not* instead pick up a perfectly usable lower-precedence variable (e.g.
     * a valid `LANG`). This matches POSIX semantics -- a set `LC_ALL` unconditionally
     * determines every category; the only divergence is that glibc fails hard on a bad
     * name whereas this leniently degrades to `"C"`.
     *
     * Called only during singleton construction (static-init, before `main`,
     * single-threaded); no other thread and no `setenv` writer exists yet, so these
     * `getenv` reads are race-free by construction.
     * @endif
     */
    static std::string resolve_locale(const char* category_var)
    {
        std::string name = "C";
        if (const char* all = std::getenv("LC_ALL"); all && *all)
            name = all;
        else if (const char* cat = std::getenv(category_var); cat && *cat)
            name = cat;
        else if (const char* lang = std::getenv("LANG"); lang && *lang)
            name = lang;

        // Validate the resolved name against the C library: clocale_wrapper builds a
        // locale_t via newlocale and throws if the name cannot be instantiated. An
        // unusable name (e.g. a bogus LANG) falls back to the neutral "C". Done here,
        // once per category at construction, so locale_name stays a plain O(1) read.
        try
        {
            clocale_wrapper probe(name.c_str());
            return name;
        }
        catch (const cvt_error&)
        {
            return "C";
        }
    }

private:
    /**
     * @lang{ZH} 各 `LC_*` 类别在构造时从环境变量解析得到的 locale 名称（见
     * `resolve_locale` / `locale_name`）。静态初始化期写入一次，此后不可变——可无锁
     * 并发读取。 @endif
     * @lang{EN} Per-category locale names resolved from the environment at construction
     * time (see `resolve_locale` / `locale_name`). Written once during static init, then
     * immutable -- safe to read concurrently without locking. @endif
     */
    std::string m_ctype;
    std::string m_collate;
    std::string m_monetary;
    std::string m_numeric;
    std::string m_time;

    /**
     * @lang{ZH} **每种 facet 类型**所缓存的不同条目数上限（`m_cache` 按 name，
     * `m_msg_cache` 按 `(domain, lang, dirname, cvt)`）。超过后 `lru_cache` 淘汰最久未用
     * 项，从而在 locale 名称 / 消息键来自可变（如外部）输入时限定内存。外层 map 的键空间
     * ——每种 facet 类型 id 一条——已被程序实例化的 facet 类型数天然限定。淘汰只会在下次
     * 未命中时重建一个等价、不可变的 facet；没有任何逻辑依赖 facet 的身份，故这纯属驻留
     * 优化，而非正确性保证。 @endif
     * @lang{EN} Upper bound on the number of distinct entries cached *per facet type*
     * (per name for `m_cache`, per `(domain, lang, dirname, cvt)` for `m_msg_cache`).
     * Past this, the `lru_cache` evicts the least-recently-used entry, bounding memory
     * under workloads that derive locale names / message keys from variable (e.g.
     * external) input. The outer map's key space -- one entry per facet type id -- is
     * already bounded by the program's instantiated facet types. Eviction only forces a
     * rebuild of an equivalent, immutable facet on the next miss; nothing relies on facet
     * identity, so this is purely an interning optimization, not a correctness
     * guarantee. @endif
     */
    static constexpr std::size_t s_cache_capacity = 256;

    /**
     * @lang{ZH} 基础 facet 缓存：外层以 facet 类型 id 为键，内层为以 locale 名称为键的
     * LRU 缓存。 @endif
     * @lang{EN} Base-facet cache: outer map keyed by facet type id, inner an LRU cache
     * keyed by locale name. @endif
     */
    std::unordered_map<facet_id_t, lru_cache<std::string, std::shared_ptr<abs_ft>, s_cache_capacity>> m_cache;
    /**
     * @lang{ZH} messages facet 缓存：外层以 facet 类型 id 为键，内层为以 `detail::msg_key`
     * 为键的 LRU 缓存。 @endif
     * @lang{EN} Messages-facet cache: outer map keyed by facet type id, inner an LRU
     * cache keyed by `detail::msg_key`. @endif
     */
    std::unordered_map<facet_id_t, lru_cache<detail::msg_key, std::shared_ptr<abs_ft>, s_cache_capacity>> m_msg_cache;
    /**
     * @lang{ZH} 保护 `m_cache` / `m_msg_cache`。`try_get` 中的 facet 构造发生在**本锁
     * 之外**（仅在查找与插入时持锁），故该 mutex 绝不跨 facet 构造函数持有；这既让构造不被
     * 序列化，也使重入的 facet 构造在非递归 mutex 下仍然安全。锁还必须覆盖**每一次查找**：
     * `lru_cache::get()` 会重排 LRU 链表（是一次会修改状态的"触碰"），故并发 get 会竞争
     * ——这里没有无锁读路径。 @endif
     * @lang{EN} Guards `m_cache` / `m_msg_cache`. Facet construction in `try_get` happens
     * *outside* this lock (it is taken only to look up and to insert), so the mutex is
     * never held across a facet constructor. That keeps construction unserialized and
     * makes a re-entrant facet ctor safe even though the mutex is non-recursive. The lock
     * must also cover every *lookup*: `lru_cache::get()` reorders the LRU list (a mutating
     * "touch"), so concurrent gets would race -- there is no lock-free read path here. @endif
     */
    std::mutex m_mutex;
};

/**
 * @lang{ZH}
 * `ori_facet_buf` 单例的全局访问引用。共享库构建（`IOV2_SHARED`）下由
 * `iov2_objects.cpp` 定义并跨库导出，以保证全程序唯一实例；静态构建下就地初始化并
 * 绑定到单例指针。库内代码通过它访问 facet 缓存与初始 locale 名称。
 * @endif
 *
 * @lang{EN}
 * Global access reference to the `ori_facet_buf` singleton. In a shared-library build
 * (`IOV2_SHARED`) it is defined in `iov2_objects.cpp` and exported across the library
 * boundary to guarantee a single program-wide instance; in a static build it is
 * initialized in place and bound to the singleton pointer. Library code reaches the
 * facet caches and the initial locale names through it.
 * @endif
 */
#if defined(IOV2_SHARED)
extern IOV2_API ori_facet_buf& s_ori_facet_buf;   // defined in iov2_objects.cpp
#else
inline ori_facet_buf::init _ori_facet_buf_init;
inline ori_facet_buf&      s_ori_facet_buf = *ori_facet_buf::ptr();
#endif
}

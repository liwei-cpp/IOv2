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
class ori_facet_buf : public sing_temp<ori_facet_buf>
{
    friend sing_temp<ori_facet_buf>;

public:
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
    ~ori_facet_buf() = default;
    ori_facet_buf(const ori_facet_buf&) = delete;
    ori_facet_buf& operator=(const ori_facet_buf&) = delete;
    ori_facet_buf(ori_facet_buf&&) = delete;
    ori_facet_buf& operator=(ori_facet_buf&&) = delete;

private:
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
        catch (...)
        {
            return "C";
        }
    }

private:
    // Per-category locale names resolved from the environment at construction time
    // (see resolve_locale / locale_name). Written once during static init, then
    // immutable -- safe to read concurrently without locking.
    std::string m_ctype;
    std::string m_collate;
    std::string m_monetary;
    std::string m_numeric;
    std::string m_time;

    // Upper bound on the number of distinct entries cached *per facet type* (per name
    // for m_cache, per (domain, lang, dirname, cvt) for m_msg_cache). Past this, the lru_cache
    // evicts the least-recently-used entry, bounding memory under workloads that derive
    // locale names / message keys from variable (e.g. external) input. The outer map's
    // key space -- one entry per facet type id -- is already bounded by the program's
    // instantiated facet types. Eviction only forces a rebuild of an equivalent,
    // immutable facet on the next miss; nothing relies on facet identity, so this is
    // purely an interning optimization, not a correctness guarantee.
    static constexpr std::size_t s_cache_capacity = 256;

    std::unordered_map<facet_id_t, lru_cache<std::string, std::shared_ptr<abs_ft>, s_cache_capacity>> m_cache;
    std::unordered_map<facet_id_t, lru_cache<detail::msg_key, std::shared_ptr<abs_ft>, s_cache_capacity>> m_msg_cache;
    // Guards m_cache / m_msg_cache. Facet construction in try_get happens *outside*
    // this lock (it is taken only to look up and to insert), so the mutex is never
    // held across a facet constructor. That keeps construction unserialized and
    // makes a re-entrant facet ctor safe even though the mutex is non-recursive.
    //
    // The lock must also cover every *lookup*: lru_cache::get() reorders the LRU list
    // (it is a mutating "touch"), so concurrent gets would race -- there is no
    // lock-free read path here.
    std::mutex m_mutex;
};

#if defined(IOV2_SHARED)
extern IOV2_API ori_facet_buf& s_ori_facet_buf;   // defined in iov2_objects.cpp
#else
inline ori_facet_buf::init _ori_facet_buf_init;
inline ori_facet_buf&      s_ori_facet_buf = *ori_facet_buf::ptr();
#endif
}

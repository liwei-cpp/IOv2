#pragma once
#include <facet/collate.h>
#include <facet/ctype.h>
#include <facet/facet_common.h>
#include <facet/messages.h>
#include <facet/monetary.h>
#include <facet/numeric.h>
#include <facet/timeio.h>
#include <locale/ori_facet_buf.h>

#include <bit>
#include <clocale>
#include <concepts>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace IOv2
{
template <typename T>
struct type_id
{
    inline static const void* s_id = nullptr;
};

template <typename T>
size_t type_id_v() noexcept
{
    return std::bit_cast<size_t>(&(type_id<T>::s_id));
}

// type_id_v keys m_facets by the address of type_id<T>::s_id. This mirrors
// ft_basic::id() (facet_common.h), which guards the same idiom. std::bit_cast is
// well-formed only when sizeof(To) == sizeof(From); since we bit_cast a pointer
// into size_t, the assert must require *equal* size, not merely "fits". With the
// correct condition a platform where the sizes differ trips this assert with a
// clear diagnostic instead of an opaque error on the bit_cast line below.
static_assert(sizeof(void*) == sizeof(size_t),
              "type_id_v uses std::bit_cast<size_t>(pointer), which is well-formed "
              "only when sizeof(void*) == sizeof(size_t)");

/**
 * @lang{ZH}
 * @brief 管理一组 facet 的本地化对象。
 *
 * `locale` 是值语义类型，遵循与标准库容器 / `std::shared_ptr` **实例**相同的
 * 线程契约（即 "thread-compatible" / 基本保证）：
 *
 * - **并发只读是安全的**：多个线程可以同时对同一个 `const locale` 实例调用
 *   只读 / const 操作（`get`、`has`、`involve`、`involve_msg`、`remove` 以及
 *   拷贝构造）。这些操作不改变 locale 的逻辑状态；`get` / `has` 虽然会惰性填充
 *   内部派生 facet 缓存（`m_facets`），但该缓存改动由 `m_facet_mutex` 内部同步，
 *   因此并发 const 调用之间不会产生数据竞争。
 * - **赋值是写操作，需要调用方外部同步**：对某个 `locale` 实例执行 `operator=`
 *   或移动赋值时，调用方必须保证此期间没有其它线程访问**同一个实例**（无论读
 *   还是写）。这与 `std::string`、标准容器、单个 `std::shared_ptr` 实例的契约
 *   完全一致。
 *
 * @warning 不要在一个线程对某 `locale` 实例赋值的同时，让另一线程对**同一实例**
 * 调用 `get` / `has`。对复合 facet（如 `numeric`、`timeio`）而言，`get<TF>()` 是
 * 一次 "查缓存 → 构建 → 写回缓存" 的复合操作，期间会多次、分别地加锁读取各
 * 依赖 facet 的 conf。即便每次单独访问都在锁内，整个复合操作也**无法**相对于
 * 一次整体赋值保持原子：并发赋值可能使最终写入 `m_facets` 的对象由"已被替换掉
 * 的旧状态"构建而来，导致缓存与当前 conf 不一致。注意这**不是**内存安全 / UB
 * 问题（所有 `shared_ptr` 访问都在锁内，不存在撕裂读或悬空），仅是逻辑状态不
 * 一致；它必须靠上述契约（赋值期间独占访问）来避免，而非靠在 `get` 内加更细的
 * 锁——复合操作的原子性无法由内部 per-operation 加锁提供。
 *
 * @note `involve` / `involve_msg` / `remove` 均为 const：它们基于 `*this` 的一份
 * 一致快照构造并返回**新的** `locale`，从不原地修改 `*this`（写时拷贝）。因此一个
 * `locale` 实例唯一的"变异面"就是赋值；按上述契约同步赋值即可。
 * @endif
 *
 * @lang{EN}
 * @brief Localization object managing a set of facets.
 *
 * `locale` is a value type that follows the same threading contract as the
 * standard-library containers and `std::shared_ptr` *instances* (the
 * "thread-compatible" / basic guarantee):
 *
 * - **Concurrent reads are safe**: multiple threads may call read-only / const
 *   operations (`get`, `has`, `involve`, `involve_msg`, `remove`, and copy
 *   construction) on the same `const locale` instance simultaneously. These
 *   operations do not change the locale's logical state; although `get` / `has`
 *   lazily populate the internal derived-facet cache (`m_facets`), that cache
 *   mutation is synchronized internally by `m_facet_mutex`, so concurrent const
 *   calls do not race.
 * - **Assignment is a write and requires external synchronization**: when
 *   `operator=` or move-assignment is applied to a `locale` instance, the caller
 *   must ensure no other thread accesses that *same instance* (read or write)
 *   for the duration. This is exactly the contract of `std::string`, the
 *   standard containers, and a single `std::shared_ptr` instance.
 *
 * @warning Do not assign to a `locale` instance in one thread while another
 * thread calls `get` / `has` on the *same* instance. For a composite facet (e.g.
 * `numeric`, `timeio`), `get<TF>()` is a compound "check cache -> build ->
 * insert" operation that locks and reads each dependency facet's conf
 * separately, several times. Even though each individual access is locked, the
 * whole compound operation *cannot* be atomic with respect to a whole-object
 * assignment: a concurrent assignment can cause the object finally inserted into
 * `m_facets` to have been built from a now-replaced state, leaving the cache
 * inconsistent with the current confs. This is *not* a memory-safety / UB
 * problem (every `shared_ptr` access is locked -- no torn reads, no dangling);
 * it is a logical-consistency one, and it must be avoided via the contract above
 * (exclusive access during assignment) rather than by finer-grained locking
 * inside `get` -- compound-operation atomicity cannot be provided by internal
 * per-operation locks.
 *
 * @note `involve` / `involve_msg` / `remove` are const: they build and return a
 * *new* `locale` from a single consistent snapshot of `*this` and never mutate
 * `*this` in place (copy-on-write). The only mutating surface of a `locale`
 * instance is therefore assignment; synchronizing assignment per the contract
 * above is sufficient.
 * @endif
 *
 * @tparam TChar
 * @lang{ZH} 该 locale 各 facet 所用的字符类型。 @endif
 * @lang{EN} The character type used by this locale's facets. @endif
 */
template <typename TChar>
class locale
{
    template <typename T>
    struct ft_wrapper;

    template <typename... T>
    struct ft_wrapper<facet_create_pack<T...>>
    {
        explicit ft_wrapper(const locale& l)
            : m_ref(l) {}

        bool has() const
        {
            return (m_ref.has<T>() && ...);
        }

        template<typename TF>
        std::shared_ptr<TF> get()
        {
            return get_helper<TF, facet_create_pack<T...>>();
        }

    private:
        template<typename TF, typename TRemArray, typename... TProcessed>
        std::shared_ptr<TF> get_helper(TProcessed&&... params)
        {
            if constexpr(facet_create_pack_size<TRemArray> > 0)
            {
                using head = typename facet_create_pack_head<TRemArray>::type;
                using tail = typename facet_create_pack_tail<TRemArray>::type;

                std::shared_ptr<head> cur = m_ref.get<head>();
                if (cur)
                    return get_helper<TF, tail>(std::forward<TProcessed>(params)..., std::move(cur));
                else
                    return nullptr;
            }
            else
            {
                return std::make_shared<TF>(std::forward<TProcessed>(params)...);
            }
        }

        const locale& m_ref;
    };

    template <typename... T>
    struct ft_wrapper<facet_create_rule<T...>>
    {
        explicit ft_wrapper(const locale& l)
            : m_ref(l) {}

        bool has() const
        {
            return has_helper<T...>();
        }

        template<typename TF>
        std::shared_ptr<TF> get()
        {
            return get_helper<TF, T...>();
        }

    private:
        template <typename... TRem>
            requires (sizeof...(TRem) == 0)
        bool has_helper() const
        {
            return false;
        }

        template <typename TC, typename... TRem>
        bool has_helper() const
        {
            if constexpr(is_nonempty_facet_create_pack<TC>)
            {
                auto checked = ft_wrapper<TC>(m_ref);
                if (checked.has()) return true;
                else return has_helper<TRem...>();
            }
            else
            {
                if (m_ref.has<TC>()) return true;
                else return has_helper<TRem...>();
            }
        }

        template <typename TF>
        std::shared_ptr<TF> get_helper()
        {
            return nullptr;
        }

        template <typename TF, typename TC, typename... TSub>
        std::shared_ptr<TF> get_helper()
        {
            if constexpr(is_nonempty_facet_create_pack<TC>)
            {
                auto creator = ft_wrapper<TC>(m_ref);

                std::shared_ptr<TF> obj = creator.template get<TF>();
                if (obj) return obj;
                return get_helper<TF, TSub...>();
            }
            else
            {
                std::shared_ptr<TC> obj = m_ref.get<TC>();
                if (obj)
                    return std::make_shared<TF>(std::move(obj));
                else
                    return get_helper<TF, TSub...>();
            }
        }

        const locale& m_ref;
    };

public:
    /**
     * @lang{ZH}
     * @brief 以**程序启动时由环境变量解析得到**的各 `LC_*` locale 名称构造 locale。
     *
     * 每个 facet 的 locale 名称取自 `ori_facet_buf::locale_name(LC_*)`；这些名称在单例
     * 构造期已由 `resolve_locale` 校验过（无法实例化的名称已回退为 `"C"`）。因此本
     * 构造函数**不会**因"locale 名称非法"而抛异常——其名称来源始终可用。这与显式命名
     * 构造函数 `locale(const std::string&)` 形成对照：后者对调用方显式给定的名称**不**做
     * 回退，名称无法实例化时直接抛异常。
     * @endif
     *
     * @lang{EN}
     * @brief Construct a locale from the per-`LC_*` names resolved from the
     * environment at program startup.
     *
     * Each facet's locale name comes from `ori_facet_buf::locale_name(LC_*)`; those
     * names were already validated by `resolve_locale` at singleton construction (a
     * name that could not be instantiated has already fallen back to `"C"`). This
     * constructor therefore never throws because of an *invalid locale name* -- its
     * name source is always usable. Contrast the explicitly-named constructor
     * `locale(const std::string&)`, which does *not* fall back for a caller-supplied
     * name and throws instead when the name cannot be instantiated.
     * @endif
     */
    locale()
    {
        init<ctype_conf>(s_ori_facet_buf.locale_name(LC_CTYPE));
        init<collate_conf>(s_ori_facet_buf.locale_name(LC_COLLATE));
        init<monetary_conf>(s_ori_facet_buf.locale_name(LC_MONETARY));
        init<numeric_conf>(s_ori_facet_buf.locale_name(LC_NUMERIC));
        init<timeio_conf>(s_ori_facet_buf.locale_name(LC_TIME));
    }

    /**
     * @lang{ZH}
     * @brief 以调用方显式给定的 locale 名称构造 locale，其全部 facet 均使用该名称。
     *
     * 与默认构造函数不同，本构造函数**不**对 `name` 做"回退到 `"C"`"处理：既然调用方
     * 显式要求某个特定 locale，则当该名称无法被 C 库实例化时（如拼写错误、宿主未安装
     * 该 locale），应当**显式失败**而非静默降级到 `"C"`。校验在底层 facet 构造中完成
     * （`clocale_wrapper` → `newlocale`）；首个失败的 facet 即抛出异常，其余 facet 不再
     * 构造，半构造的 `locale` 随构造函数栈展开而销毁（不泄漏、不留下污染缓存）。
     *
     * @param name 一个在宿主上可被实例化的 locale 名称（语义同 `newlocale`）。
     * @throws cvt_error 当 `name` 无法被 C 库实例化时抛出（`cvt_error` 派生自 `io_error`，
     *         并最终派生自 `std::runtime_error`）。
     * @endif
     *
     * @lang{EN}
     * @brief Construct a locale from a caller-supplied locale name; every facet uses
     * that name.
     *
     * Unlike the default constructor, this one does *not* fall back to `"C"` for
     * `name`: since the caller explicitly asked for a specific locale, an unknown or
     * uninstantiable name (a typo, or a locale not installed on the host) fails
     * loudly rather than silently degrading to `"C"`. Validation happens in the
     * underlying facet construction (`clocale_wrapper` -> `newlocale`); the first
     * failing facet throws, the remaining facets are not constructed, and the
     * partially-built `locale` is destroyed as the constructor unwinds (no leak, no
     * poisoned cache entry).
     *
     * @param name A locale name instantiable on the host (as by `newlocale`).
     * @throws cvt_error if `name` cannot be instantiated by the C library
     *         (`cvt_error` derives from `io_error`, and ultimately from
     *         `std::runtime_error`).
     * @endif
     */
    explicit locale(const std::string& name)
    {
        init<ctype_conf>(name);
        init<collate_conf>(name);
        init<monetary_conf>(name);
        init<numeric_conf>(name);
        init<timeio_conf>(name);
    }

    locale(const locale& val)
    {
        std::shared_lock g(val.m_facet_mutex);
        m_facet_confs = val.m_facet_confs;
        m_facets = val.m_facets;
    }

    locale(locale&& val)
    {
        std::lock_guard g(val.m_facet_mutex);
        m_facet_confs = std::move(val.m_facet_confs);
        m_facets = std::move(val.m_facets);
    }

    // Assignment is the only mutating operation on a locale instance. Per the
    // class threading contract (see the class doc), the caller must ensure no
    // other thread accesses *this same instance* (read or write) during an
    // assignment. The locks below keep the source consistent and the maps free
    // of torn reads, but they do NOT make a concurrent get<TF>() on the
    // assigned-into object atomic against the assignment.
    locale& operator=(const locale& val)
    {
        if (this == &val) return *this;
        std::scoped_lock g(val.m_facet_mutex, m_facet_mutex);
        auto confs  = val.m_facet_confs;
        auto facets = val.m_facets;
        m_facet_confs = std::move(confs);
        m_facets      = std::move(facets);
        return *this;
    }

    locale& operator=(locale&& val)
    {
        if (this == &val) return *this;
        std::scoped_lock g(val.m_facet_mutex, m_facet_mutex);
        m_facet_confs = std::move(val.m_facet_confs);
        m_facets = std::move(val.m_facets);
        return *this;
    }

    locale involve(std::shared_ptr<abs_ft> ft) const
    {
        if (!ft) throw std::runtime_error("cannot add empty facet pointer into locale.");

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    locale involve_msg(const std::string& domain, const std::string& lang = "") const requires (!std::is_same_v<TChar, char>)
    {
        // messages_conf<TChar> is only specialized for char8_t, char32_t and
        // UTF-32 wchar_t (plus the char overload above); for char16_t or a
        // non-UTF-32 wchar_t it stays incomplete. Reject those here with a clear
        // message instead of an opaque "incomplete type" error from make_shared
        // below. Safe as a completeness check because messages_conf<TChar>'s
        // completeness is fixed program-wide (a type either has a specialization
        // everywhere or nowhere).
        static_assert(requires { sizeof(messages_conf<TChar>); },
                      "involve_msg: messages translation is unsupported for this character "
                      "type -- no messages_conf<TChar> specialization exists (e.g. char16_t, "
                      "or wchar_t on a non-UTF-32 platform).");
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        auto ft = s_ori_facet_buf.try_get_msg<TChar>(domain, filtered_lang);
        if (!ft)
        {
            ft = std::make_shared<messages_conf<TChar>>(domain, filtered_lang, false);
            ft = s_ori_facet_buf.put_msg<TChar>(ft, domain, filtered_lang);
        }

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    locale involve_msg(const std::string& domain, const std::string& lang = "",
                       const std::string& cvt = "") const requires (std::is_same_v<TChar, char>)
    {
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        // Resolve the encoding up front and use it consistently as both the cache
        // key and the construction argument. An empty cvt maps to the (stable,
        // env-derived) CTYPE name; keying by this effective value -- rather than by
        // the raw cvt -- ensures the cached facet is always looked up under the
        // exact encoding it was built with, and lets an explicit cvt equal to the
        // CTYPE name share the same cache entry as the empty-cvt case.
        const std::string effective_cvt = cvt.empty() ? s_ori_facet_buf.locale_name(LC_CTYPE) : cvt;
        auto ft = s_ori_facet_buf.try_get_msg<char>(domain, filtered_lang, effective_cvt);
        if (!ft)
        {
            ft = std::make_shared<messages_conf<char>>(domain, filtered_lang, effective_cvt, false);
            ft = s_ori_facet_buf.put_msg<char>(ft, domain, filtered_lang, effective_cvt);
        }

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    template <std::derived_from<abs_ft> TF>
    locale remove() const
    {
        locale res(*this);
        if (res.has<TF>())
        {
            res.m_facet_confs.erase(TF::id());
            res.m_facets.clear();
        }
        return res;
    }

    template <std::derived_from<abs_ft> TF>
    bool has() const
    {
        std::shared_lock g(m_facet_mutex);
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return false;

        return std::dynamic_pointer_cast<TF>(it->second) != nullptr;
    }

    // The `!std::derived_from` conjunct excludes the pathological "both a conf and a
    // composite facet" type, so that for such a type only the guard overload below is
    // viable (it would otherwise tie with this one and make the call ambiguous --
    // std::derived_from is a concept and subsumes, but the bare is_nonempty_... variable
    // template re-typed in two places forms distinct, non-subsuming atomic constraints).
    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>
                  && !std::derived_from<TF, abs_ft>)
    bool has() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>()); it != m_facets.end())
                return true;
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        return obj.has();
    }

    // Guard overload. A type that is *both* a facet conf (derived from abs_ft) and a
    // composite facet (with a non-empty create_rules) would satisfy the constraints of
    // both has() overloads above and make the call ambiguous. By design that never
    // happens -- confs derive from abs_ft while facets carry create_rules, and they are
    // distinct types. This overload's constraint is the conjunction of the other two's,
    // so it subsumes both and is selected unambiguously for such a pathological type,
    // turning an opaque "ambiguous call" into the precise static_assert below.
    template <typename TF>
        requires (std::derived_from<TF, abs_ft>
                  && is_nonempty_facet_create_rule<typename TF::create_rules>)
    bool has() const
    {
        static_assert(dependent_false_v<TF>,
                      "has<TF>(): TF is both a facet conf (derived from abs_ft) and a "
                      "composite facet (with a non-empty create_rules). These roles are "
                      "mutually exclusive by design -- confs derive from abs_ft, facets "
                      "carry create_rules -- so such a type is a definition error.");
        return false;
    }

    template <std::derived_from<abs_ft> TF>
    std::shared_ptr<TF> get() const
    {
        std::shared_lock g(m_facet_mutex);
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return nullptr;

        return std::dynamic_pointer_cast<TF>(it->second);
    }

    // See the has() composite overload above for why `!std::derived_from` is needed.
    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>
                  && !std::derived_from<TF, abs_ft>)
    std::shared_ptr<TF> get() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>()); it != m_facets.end())
                return std::static_pointer_cast<TF>(it->second);
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        auto res = obj.template get<TF>();
        if (res)
        {
            std::lock_guard g(m_facet_mutex);
            auto [it, inserted] = m_facets.insert({type_id_v<TF>(), res});
            if (!inserted)
                return std::static_pointer_cast<TF>(it->second);
        }
        return res;
    }

    // Guard overload; see the has() guard above for the rationale. Disambiguates (with
    // a precise diagnostic) a type that is both a facet conf and a composite facet.
    template <typename TF>
        requires (std::derived_from<TF, abs_ft>
                  && is_nonempty_facet_create_rule<typename TF::create_rules>)
    std::shared_ptr<TF> get() const
    {
        static_assert(dependent_false_v<TF>,
                      "get<TF>(): TF is both a facet conf (derived from abs_ft) and a "
                      "composite facet (with a non-empty create_rules). These roles are "
                      "mutually exclusive by design -- confs derive from abs_ft, facets "
                      "carry create_rules -- so such a type is a definition error.");
        return nullptr;
    }

    /**
     * @lang{ZH}
     * 返回某个 `LC_*` 类别在**程序启动时由环境变量解析得到**的初始 locale 名称。
     *
     * 这是对 `ori_facet_buf::locale_name` 的薄转发，使得仅依赖 `<locale/locale.h>`
     * 的代码（如 stdio 流对象）无需直接看到 `ori_facet_buf` 即可获取该初始名称。
     * 返回值与字符类型无关；之所以作为 `locale` 的静态成员暴露，是为了让调用方只
     * 看见 `locale`。
     * @endif
     *
     * @lang{EN}
     * Return the *initial* locale name for an `LC_*` category, as resolved from the
     * environment at program startup.
     *
     * A thin forwarder over `ori_facet_buf::locale_name` so that code depending
     * only on `<locale/locale.h>` (e.g. the stdio stream objects) can obtain this
     * initial name without seeing `ori_facet_buf` directly. The result is
     * independent of character type; it is exposed as a static member of `locale`
     * so that callers only need to see `locale`.
     * @endif
     */
    static const std::string& initial_locale_name(int category)
    {
        return s_ori_facet_buf.locale_name(category);
    }

private:
    template <template <typename> class T>
    void init(const std::string& ft_name)
    {
        std::size_t k = T<TChar>::id();
        auto v = s_ori_facet_buf.try_get<T<TChar>>(ft_name);
        m_facet_confs.insert({k, v});
    }

private:
    std::unordered_map<std::size_t, std::shared_ptr<abs_ft>> m_facet_confs;
    mutable std::unordered_map<std::size_t, std::shared_ptr<void>> m_facets;
    mutable std::shared_mutex m_facet_mutex;
};
}

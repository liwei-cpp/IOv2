#pragma once
#include <iterator>
#include <type_traits>
#include <io/io_base.h>
#include <io/fp_defs/base_fp.h>
#include <facet/numeric.h>
#include <locale/locale.h>

namespace IOv2
{
template <typename TChar, typename TValue>
    requires std::is_arithmetic_v<TValue>
struct writer<TChar, TValue>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, TValue value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->put(s, io, value);
    }
};

template <typename TChar, typename TValue>
    requires std::is_arithmetic_v<TValue>
struct reader<TChar, TValue>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, TValue& value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->get(s, s_end, io, value);
    }
};

template <typename TChar, typename TValue>
    requires (std::is_pointer_v<TValue>)
struct writer<TChar, TValue>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, TValue value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->put(s, io, value);
    }
};

/**
 * @lang{ZH}
 * @brief 从流中解析一个地址，写入 `void*`。
 * @note 与上面的 `writer` 不同，这里**只接受 `void*`**，不接受任意指针类型。插入侧放宽是安全
 *       的，也与标准一致——`std::ostream` 的 `operator<<(const void*)` 本来就会通过隐式转换
 *       接住任意对象指针。但提取侧没有这样的转换：`std::istream` 只有 `operator>>(void*&)`，
 *       写 `is >> ip`（`ip` 为 `int*`）在标准里是非良构的。
 * @note 这条约束是**有意收窄**的，不要改回 `requires std::is_pointer_v<TValue>`。放宽会带来两
 *       个后果：一是把文本里的地址 `reinterpret_cast` 进任意类型的指针，产生野指针而流仍为
 *       `good`；二是会静默接管 `char*`，使字符缓冲区的提取变成地址解析。收窄之后，
 *       `is >> charptr` / `is >> intptr` 没有任何 reader 匹配，由 `operator>>` 的兜底重载给出
 *       一条简短的 `static_assert`。
 * @endif
 *
 * @lang{EN}
 * @brief Parses an address from the stream into a `void*`.
 * @note Unlike the `writer` above, this accepts **`void*` only**, not an arbitrary pointer type.
 *       Being permissive on the insertion side is safe and matches the standard -- `std::ostream`'s
 *       `operator<<(const void*)` already accepts any object pointer through an implicit
 *       conversion. The extraction side has no such conversion: `std::istream` only provides
 *       `operator>>(void*&)`, and `is >> ip` (with `ip` an `int*`) is ill-formed by the standard.
 * @note This constraint is **deliberately narrow**; do not widen it back to
 *       `requires std::is_pointer_v<TValue>`. Widening has two consequences: it
 *       `reinterpret_cast`s a textual address into a pointer of arbitrary type, yielding a wild
 *       pointer while the stream stays `good`; and it silently captures `char*`, turning character
 *       buffer extraction into address parsing. As narrowed, `is >> charptr` / `is >> intptr` match
 *       no reader at all and are diagnosed by the fallback `operator>>` with one short
 *       `static_assert`.
 * @endif
 */
template <typename TChar>
struct reader<TChar, void*>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, void*& value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->get(s, s_end, io, value);
    }
};
}

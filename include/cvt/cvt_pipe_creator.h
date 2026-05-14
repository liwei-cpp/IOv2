/**
 * @file cvt_pipe_creator.h
 * @lang{ZH}
 * 管道式转换器工厂（Pipe Creator）定义文件。
 * 本文件提供了将多个转换器工厂链式组合为单一管道工厂的机制，
 * 以及通过 `operator|` 构造管道工厂时所需的辅助类型工具。
 * @endif
 *
 * @lang{EN}
 * Pipe-style converter creator definition file.
 * This file provides the mechanism for chaining multiple converter creators
 * into a single pipeline creator, along with the auxiliary type utilities
 * needed to construct pipeline creators via `operator|`.
 * @endif
 */
#pragma once

#include <cvt/cvt_concepts.h>

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * 管道式转换器工厂模板的前置声明。
 * 通过 `operator|` 将多个 `cvt_creator` 类型串联，可得到此类型的实例。
 * @endif
 *
 * @lang{EN}
 * Forward declaration of the pipe-style converter creator template.
 * Instances of this type are obtained by chaining multiple `cvt_creator`
 * types together via `operator|`.
 * @endif
 *
 * @tparam T 组成管道的各转换器工厂类型。
 *           / The converter creator types that form the pipeline.
 */
template <typename... T>
class cvt_pipe_creator;

/**
 * @lang{ZH}
 * `cpt_cvt_pipe_creator` 概念的辅助特征模板（主模板）。
 * 对于非 `cvt_pipe_creator` 特化的类型，`value` 为 `false`。
 * @endif
 *
 * @lang{EN}
 * Helper trait template (primary template) for the `cpt_cvt_pipe_creator` concept.
 * For types that are not specializations of `cvt_pipe_creator`, `value` is `false`.
 * @endif
 *
 * @tparam T 待检测的类型。 / The type to be inspected.
 */
template<typename T>
struct cpt_cvt_pipe_creator_helper
{
    constexpr static bool value = false;
};

/**
 * @lang{ZH}
 * `cpt_cvt_pipe_creator` 概念的辅助特征模板（`cvt_pipe_creator` 特化）。
 * 对于 `cvt_pipe_creator<T...>` 特化，`value` 为 `true`。
 * @endif
 *
 * @lang{EN}
 * Helper trait template specialization for `cvt_pipe_creator<T...>`.
 * For specializations of `cvt_pipe_creator`, `value` is `true`.
 * @endif
 *
 * @tparam T 组成管道的各转换器工厂类型。
 *           / The converter creator types that form the pipeline.
 */
template<typename... T>
struct cpt_cvt_pipe_creator_helper<cvt_pipe_creator<T...>>
{
    constexpr static bool value = true;
};

/**
 * @lang{ZH}
 * 检测类型 T 是否为 `cvt_pipe_creator` 特化实例的概念。
 * @endif
 *
 * @lang{EN}
 * Concept that detects whether type T is a specialization of `cvt_pipe_creator`.
 * @endif
 *
 * @tparam T 待检测的类型。 / The type to be checked.
 */
template<typename T>
concept cpt_cvt_pipe_creator = cpt_cvt_pipe_creator_helper<T>::value;

/**
 * @lang{ZH}
 * 根据两个转换器工厂类型推导合并后管道类型的类型生成器（主模板，故意未定义）。
 * 此主模板无定义，仅通过各特化提供 `type` 成员；
 * 若两个操作数的组合不满足任何特化的约束，将产生编译错误。
 * @endif
 *
 * @lang{EN}
 * Type generator that deduces the resulting pipeline type from two converter creator types
 * (primary template, intentionally left undefined).
 * This primary template provides no definition; only specializations expose the `type` member.
 * A combination of operands that satisfies no specialization results in a compile error.
 * @endif
 *
 * @tparam T1 左操作数的转换器工厂类型。 / Left-hand converter creator type.
 * @tparam T2 右操作数的转换器工厂类型。 / Right-hand converter creator type.
 */
template <typename T1, typename T2>
struct cvt_creator_type_gen;

/**
 * @lang{ZH}
 * `cvt_creator_type_gen` 特化：两个非管道的 `cvt_creator` 类型合并，
 * 生成 `cvt_pipe_creator<T1, T2>`。
 * @endif
 *
 * @lang{EN}
 * `cvt_creator_type_gen` specialization: two non-pipe `cvt_creator` types are combined,
 * yielding `cvt_pipe_creator<T1, T2>`.
 * @endif
 *
 * @tparam T1 左侧非管道转换器工厂类型。 / Left-hand non-pipe converter creator type.
 * @tparam T2 右侧非管道转换器工厂类型。 / Right-hand non-pipe converter creator type.
 */
template <typename T1, typename T2>
    requires (std::is_same_v<typename T1::category, CvtCreatorCategory> &&
              std::is_same_v<typename T2::category, CvtCreatorCategory> &&
              !cpt_cvt_pipe_creator<T1> && !cpt_cvt_pipe_creator<T2>)
struct cvt_creator_type_gen<T1, T2>
{
    using type = cvt_pipe_creator<T1, T2>;
};

/**
 * @lang{ZH}
 * `cvt_creator_type_gen` 特化：非管道工厂 T1 前置于管道工厂 `cvt_pipe_creator<T2...>` 之前，
 * 生成 `cvt_pipe_creator<T1, T2...>`。
 * @endif
 *
 * @lang{EN}
 * `cvt_creator_type_gen` specialization: a non-pipe creator T1 is prepended to an existing
 * pipeline `cvt_pipe_creator<T2...>`, yielding `cvt_pipe_creator<T1, T2...>`.
 * @endif
 *
 * @tparam T1 左侧非管道转换器工厂类型。 / Left-hand non-pipe converter creator type.
 * @tparam T2 右侧管道工厂所含的各转换器工厂类型。
 *           / The converter creator types contained in the right-hand pipeline.
 */
template <typename T1, typename... T2>
    requires (std::is_same_v<typename T1::category, CvtCreatorCategory> &&
              !cpt_cvt_pipe_creator<T1>)
struct cvt_creator_type_gen<T1, cvt_pipe_creator<T2...>>
{
    using type = cvt_pipe_creator<T1, T2...>;
};

/**
 * @lang{ZH}
 * `cvt_creator_type_gen` 特化：非管道工厂 T2 追加于管道工厂 `cvt_pipe_creator<T1...>` 之后，
 * 生成 `cvt_pipe_creator<T1..., T2>`。
 * @endif
 *
 * @lang{EN}
 * `cvt_creator_type_gen` specialization: a non-pipe creator T2 is appended to an existing
 * pipeline `cvt_pipe_creator<T1...>`, yielding `cvt_pipe_creator<T1..., T2>`.
 * @endif
 *
 * @tparam T1 左侧管道工厂所含的各转换器工厂类型。
 *           / The converter creator types contained in the left-hand pipeline.
 * @tparam T2 右侧非管道转换器工厂类型。 / Right-hand non-pipe converter creator type.
 */
template <typename... T1, typename T2>
    requires (std::is_same_v<typename T2::category, CvtCreatorCategory> &&
              !cpt_cvt_pipe_creator<T2>)
struct cvt_creator_type_gen<cvt_pipe_creator<T1...>, T2>
{
    using type = cvt_pipe_creator<T1..., T2>;
};

/**
 * @lang{ZH}
 * `cvt_creator_type_gen` 特化：两个管道工厂合并，将各自内部的工厂列表依序拼接，
 * 生成 `cvt_pipe_creator<T1..., T2...>`。
 * @endif
 *
 * @lang{EN}
 * `cvt_creator_type_gen` specialization: two pipeline creators are merged by concatenating
 * their internal creator lists in order, yielding `cvt_pipe_creator<T1..., T2...>`.
 * @endif
 *
 * @tparam T1 左侧管道工厂所含的各转换器工厂类型。
 *           / The converter creator types in the left-hand pipeline.
 * @tparam T2 右侧管道工厂所含的各转换器工厂类型。
 *           / The converter creator types in the right-hand pipeline.
 */
template <typename... T1, typename... T2>
struct cvt_creator_type_gen<cvt_pipe_creator<T1...>, cvt_pipe_creator<T2...>>
{
    using type = cvt_pipe_creator<T1..., T2...>;
};

/**
 * @lang{ZH}
 * 由两个转换器工厂组成的管道工厂（二元特化）。
 * 调用 `create(kernel)` 时，先以 T1 包装 `kernel`，再以 T2 包装上一步的结果，
 * 形成双层转换链路。向最终转换器写入数据时，数据依次流经 T2 和 T1 的处理，最终到达 `kernel`。
 * @endif
 *
 * @lang{EN}
 * Two-element pipeline creator (binary specialization).
 * When `create(kernel)` is called, it first wraps `kernel` with T1, then wraps the result
 * with T2, producing a two-layer conversion chain. When data is written into the resulting
 * converter, it passes through T2, then T1, and finally reaches `kernel`.
 * @endif
 *
 * @tparam T1 第一个（内层）转换器工厂类型。 / The first (inner) converter creator type.
 * @tparam T2 第二个（外层）转换器工厂类型。 / The second (outer) converter creator type.
 */
template <typename T1, typename T2>
class cvt_pipe_creator<T1, T2>
{
    template <typename... U>
    friend class cvt_pipe_creator;

public:
    using category = CvtCreatorCategory;

    /**
     * @lang{ZH}
     * 构造二元管道工厂，保存两个工厂实例的副本。
     * @endif
     *
     * @lang{EN}
     * Constructs a binary pipeline creator, storing copies of both creator instances.
     * @endif
     *
     * @param c1 第一个（内层）转换器工厂。 / The first (inner) converter creator.
     * @param c2 第二个（外层）转换器工厂。 / The second (outer) converter creator.
     */
    cvt_pipe_creator(const T1& c1, const T2& c2)
        : m_creators(c1, c2) {}

    /**
     * @lang{ZH}
     * 以此管道工厂创建转换器，将 `kernel` 依次包装进 T1 和 T2 中。
     * @endif
     *
     * @lang{EN}
     * Creates a converter using this pipeline creator by successively wrapping
     * `kernel` inside T1 and then inside T2.
     * @endif
     *
     * @tparam TKernel 满足 `io_converter` 的内核转换器类型。
     *                / The kernel converter type satisfying `io_converter`.
     * @param kernel  最内层的转换器内核。 / The innermost converter kernel.
     * @return 经过两层包装后的转换器。 / The converter after two layers of wrapping.
     */
    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        auto c1 = std::get<0>(m_creators).create(std::forward<TKernel>(kernel));
        return std::get<1>(m_creators).create(std::move(c1));
    }
private:
    std::tuple<T1, T2> m_creators;
};

/**
 * @lang{ZH}
 * 由三个或更多转换器工厂组成的管道工厂（多元特化）。
 * 内部将所有工厂展平存储于单一 `std::tuple` 中。
 * 调用 `create(kernel)` 时，按索引从小到大依次将 `kernel` 包装进每个工厂，
 * 最终得到多层嵌套的转换器：最外层为最后一个工厂（索引最大），
 * 最内层（直接包裹 `kernel`）为第一个工厂（索引 0）。
 * @endif
 *
 * @lang{EN}
 * Multi-element pipeline creator (variadic specialization, requires more than two elements).
 * Internally, all creators are stored flattened in a single `std::tuple`.
 * When `create(kernel)` is called, it wraps `kernel` with each creator in index order,
 * yielding a multi-layer nested converter. The outermost layer corresponds to the last
 * creator (highest index), and the innermost layer (directly wrapping `kernel`) corresponds
 * to the first creator (index 0).
 * @endif
 *
 * @tparam T 组成管道的各转换器工厂类型（至少三个）。
 *           / The converter creator types forming the pipeline (at least three).
 */
template <typename... T>
    requires (sizeof...(T) > 2)
class cvt_pipe_creator<T...>
{
    template <typename... U>
    friend class cvt_pipe_creator;

public:
    using category = CvtCreatorCategory;

    /**
     * @lang{ZH}
     * 从两个管道工厂合并构造，将两者内部的工厂列表拼接为一个扁平列表。
     * @endif
     *
     * @lang{EN}
     * Constructs by merging two pipeline creators, concatenating their internal
     * creator lists into a single flat list.
     * @endif
     *
     * @tparam T1 左侧管道工厂类型（须满足 `cpt_cvt_pipe_creator`）。
     *           / Left-hand pipeline creator type (must satisfy `cpt_cvt_pipe_creator`).
     * @tparam T2 右侧管道工厂类型（须满足 `cpt_cvt_pipe_creator`）。
     *           / Right-hand pipeline creator type (must satisfy `cpt_cvt_pipe_creator`).
     * @param t1 左侧管道工厂实例。 / Left-hand pipeline creator instance.
     * @param t2 右侧管道工厂实例。 / Right-hand pipeline creator instance.
     */
    template <cpt_cvt_pipe_creator T1, cpt_cvt_pipe_creator T2>
    cvt_pipe_creator(const T1& t1, const T2& t2)
        : m_creators(std::tuple_cat(t1.m_creators, t2.m_creators)) {}

    /**
     * @lang{ZH}
     * 从单个非管道工厂与一个管道工厂合并构造，将非管道工厂前置于管道工厂的工厂列表之前。
     * @endif
     *
     * @lang{EN}
     * Constructs by prepending a single non-pipe creator to an existing pipeline creator's
     * internal creator list.
     * @endif
     *
     * @tparam TH 左侧非管道转换器工厂类型。 / Left-hand non-pipe converter creator type.
     * @tparam TR 右侧管道工厂类型（须满足 `cpt_cvt_pipe_creator`）。
     *           / Right-hand pipeline creator type (must satisfy `cpt_cvt_pipe_creator`).
     * @param h  左侧非管道工厂实例。 / Left-hand non-pipe creator instance.
     * @param hr 右侧管道工厂实例。 / Right-hand pipeline creator instance.
     */
    template <typename TH, cpt_cvt_pipe_creator TR>
        requires (!cpt_cvt_pipe_creator<TH>)
    cvt_pipe_creator(const TH& h, const TR& hr)
        : m_creators(std::tuple_cat(std::make_tuple(h), hr.m_creators)) {}

    /**
     * @lang{ZH}
     * 从一个管道工厂与单个非管道工厂合并构造，将非管道工厂追加于管道工厂的工厂列表之后。
     * @endif
     *
     * @lang{EN}
     * Constructs by appending a single non-pipe creator to an existing pipeline creator's
     * internal creator list.
     * @endif
     *
     * @tparam TR 左侧管道工厂类型（须满足 `cpt_cvt_pipe_creator`）。
     *           / Left-hand pipeline creator type (must satisfy `cpt_cvt_pipe_creator`).
     * @tparam TT 右侧非管道转换器工厂类型。 / Right-hand non-pipe converter creator type.
     * @param tr 左侧管道工厂实例。 / Left-hand pipeline creator instance.
     * @param t  右侧非管道工厂实例。 / Right-hand non-pipe creator instance.
     */
    template <cpt_cvt_pipe_creator TR, typename TT>
        requires (!cpt_cvt_pipe_creator<TT>)
    cvt_pipe_creator(const TR& tr, const TT& t)
        : m_creators(std::tuple_cat(tr.m_creators, std::make_tuple(t))) {}

    /**
     * @lang{ZH}
     * 以此管道工厂创建转换器，将 `kernel` 依次包装进所有工厂中。
     * @endif
     *
     * @lang{EN}
     * Creates a converter using this pipeline creator by successively wrapping
     * `kernel` through all contained creators in index order.
     * @endif
     *
     * @tparam TKernel 满足 `io_converter` 的内核转换器类型。
     *                / The kernel converter type satisfying `io_converter`.
     * @param kernel  最内层的转换器内核。 / The innermost converter kernel.
     * @return 经过所有工厂逐层包装后的转换器。
     *         / The converter after being wrapped by all creators layer by layer.
     */
    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return create_helper<0>(std::forward<TKernel>(kernel));
    }

private:
    /**
     * @lang{ZH}
     * 递归辅助函数，将 `kernel` 依次包装进索引 N 及之后的所有工厂中。
     * 当 N 等于工厂总数时，直接返回 `kernel`，作为递归的终止条件。
     * @endif
     *
     * @lang{EN}
     * Recursive helper that wraps `kernel` through creators at index N and beyond.
     * When N equals the total number of creators, the recursion terminates by
     * returning `kernel` as-is.
     * @endif
     *
     * @tparam N       当前处理的工厂索引。 / Index of the creator currently being applied.
     * @tparam TKernel 当前内核类型。 / Type of the current kernel.
     * @param kernel   当前内核（可能已被若干工厂包装）。
     *                / The current kernel (possibly already wrapped by preceding creators).
     * @return 经过索引 N 起的所有工厂包装后的转换器。
     *         / The converter after applying all creators from index N onward.
     */
    template <size_t N, typename TKernel>
    auto create_helper(TKernel&& kernel) const
    {
        if constexpr (sizeof...(T) == N)
            return std::forward<TKernel>(kernel);
        else
        {
            auto x = std::get<N>(m_creators).create(std::forward<TKernel>(kernel));
            return create_helper<N+1>(std::move(x));
        }
    }
private:
    std::tuple<T...> m_creators;
};

/**
 * @lang{ZH}
 * 管道组合运算符，将两个 `cvt_creator` 类型组合为一个管道工厂。
 * 若两个操作数均为非管道工厂，则生成 `cvt_pipe_creator<T1, T2>`；
 * 若其中一个或两个已为 `cvt_pipe_creator`，则通过 `cvt_creator_type_gen`
 * 将内部工厂列表展平拼接，避免嵌套管道，始终保持线性扁平结构。
 * @endif
 *
 * @lang{EN}
 * Pipeline composition operator that combines two `cvt_creator` types into a pipeline creator.
 * If both operands are non-pipe creators, the result is `cvt_pipe_creator<T1, T2>`.
 * If one or both operands are already `cvt_pipe_creator` instances, `cvt_creator_type_gen`
 * flattens and concatenates their internal creator lists, preventing nested pipelines and
 * always maintaining a flat, linear structure.
 * @endif
 *
 * @tparam T1 左操作数类型，须满足 `cvt_creator` 且可拷贝构造。
 *           / Left operand type, must satisfy `cvt_creator` and be copy-constructible.
 * @tparam T2 右操作数类型，须满足 `cvt_creator` 且可拷贝构造。
 *           / Right operand type, must satisfy `cvt_creator` and be copy-constructible.
 * @param t1 左侧转换器工厂实例。 / Left-hand converter creator instance.
 * @param t2 右侧转换器工厂实例。 / Right-hand converter creator instance.
 * @return 由 T1 和 T2 组合而成的管道工厂。
 *         / A pipeline creator composed of T1 and T2.
 */
template <typename T1, typename T2>
    requires (cvt_creator<T1> && cvt_creator<T2> &&
              std::copy_constructible<T1> && std::copy_constructible<T2>)
auto operator | (const T1& t1, const T2& t2)
{
    using type = typename cvt_creator_type_gen<T1, T2>::type;
    return type{t1, t2};
}
}

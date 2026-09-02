// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file io_concepts.h
 * @lang{ZH}
 * I/O 层的概念与类型工具文件。
 * 本文件在转换器体系（见 cvt/cvt_concepts.h）之上提供 I/O 层使用的编译期类型工具：
 * - `ext_to_int`——用于从「根转换器 + 转换器工厂」推导出转换器管线最终暴露的内部字符类型。
 *   该别名主要服务于 streambuf 系列类型的类模板实参推导（CTAD）。
 * - `cvt_direction_capable` / `cvt_fits_direction`——判断一条转换管线的读写能力是否够得上
 *   某个方向的流缓冲区。这是 I/O 层的判据而非转换器层的：方向（`IsIn`/`IsOut`）是**流**的
 *   概念，转换器层只提供 `cvt_cpt::support_get` / `support_put` / `support_io_switch`
 *   这些能力原语，由本层组合。与设备一侧的分工一致——`dev_cpt` 的能力原语定义在
 *   `IOv2/device/device_concepts.h`，把它们组合成方向约束的是 `IOv2/io/iostream.h`。
 * @endif
 *
 * @lang{EN}
 * Concept and type-utility file for the I/O layer.
 * Building on the converter system (see cvt/cvt_concepts.h), this file provides the
 * compile-time type utilities used by the I/O layer:
 * - `ext_to_int`, which derives the internal character type ultimately exposed by a
 *   converter pipeline from a "root converter + converter creator" pair. This alias
 *   mainly serves the class-template argument deduction (CTAD) of the streambuf family.
 * - `cvt_direction_capable` / `cvt_fits_direction`, which decide whether a converter
 *   pipeline is capable enough for a stream buffer of a given direction. This belongs to
 *   the I/O layer rather than the converter layer: a direction (`IsIn`/`IsOut`) is a
 *   *stream* notion, while the converter layer only supplies the capability primitives
 *   `cvt_cpt::support_get` / `support_put` / `support_io_switch` for this layer to
 *   compose. The split mirrors the device side, where `dev_cpt`'s primitives live in
 *   `IOv2/device/device_concepts.h` and `IOv2/io/iostream.h` composes them into a direction
 *   constraint.
 * @endif
 */
#pragma once
#include <IOv2/cvt/cvt_concepts.h>

#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 推导转换器管线暴露的内部数据类型（`internal_type`）。
 *
 * 给定一个根转换器类型 @p TRootCvt 与一个转换器工厂类型 @p TCreator，本别名等价于
 * 用工厂在该根转换器之上构建出的完整转换器的 `internal_type`。其求值方式为：在**未求值
 * 语境**中形式化调用 `TCreator::create(TRootCvt)` 得到管线末端的转换器类型，再取其嵌套
 * 的 `internal_type`。
 *
 * @note 这是一个纯编译期类型计算，不会实际构造任何转换器或工厂对象
 *       （通过 `std::declval` 与 `decltype` 实现）。
 * @note 典型用途是 streambuf/istreambuf/ostreambuf 的 CTAD 推导指引：由「设备 + 工厂」
 *       反推出 streambuf 应采用的字符类型。
 *
 * @tparam TRootCvt 根转换器类型，须满足 `io_converter`（通常是包装了设备的根转换器）。
 * @tparam TCreator 转换器工厂类型，须满足 `cvt_creator`。
 * @endif
 *
 * @lang{EN}
 * @brief Derives the internal data type (`internal_type`) exposed by a converter
 * pipeline.
 *
 * Given a root converter type @p TRootCvt and a converter creator type @p TCreator,
 * this alias is equivalent to the `internal_type` of the complete converter that the
 * creator builds on top of that root converter. It is evaluated by formally invoking
 * `TCreator::create(TRootCvt)` in an **unevaluated context** to obtain the converter
 * type at the end of the pipeline, then taking its nested `internal_type`.
 *
 * @note This is a purely compile-time type computation; no converter or creator object
 *       is actually constructed (it uses `std::declval` and `decltype`).
 * @note A typical use is the CTAD deduction guides of streambuf/istreambuf/ostreambuf:
 *       deducing the character type a streambuf should use from a "device + creator" pair.
 *
 * @tparam TRootCvt The root converter type; must satisfy `io_converter` (usually a root
 *         converter wrapping a device).
 * @tparam TCreator The converter creator type; must satisfy `cvt_creator`.
 * @endif
 */
template <io_converter TRootCvt, cvt_creator TCreator>
using ext_to_int =
    typename decltype(std::declval<TCreator>().create(std::declval<TRootCvt>()))::internal_type;

/**
 * @lang{ZH}
 * @brief 转换器 @p TCvt 的读写能力是否够得上方向为 (@p IsIn, @p IsOut) 的流缓冲区。
 *
 * 三档判据，与 `base_streambuf` 的三种实例一一对应：
 * | 方向 | 实例 | 要求 |
 * |---|---|---|
 * | `IsIn && IsOut` | `streambuf` | `support_io_switch`（已蕴含 `support_get` 与 `support_put`） |
 * | 仅 `IsIn` | `istreambuf` | `support_get` |
 * | 仅 `IsOut` | `ostreambuf` | `support_put` |
 *
 * @warning **必须施加于具体的转换器类型，不能施加于 `runtime_cvt`。** `runtime_cvt` 是运行期
 *          类型擦除外覆类，它把全部接口都实现出来，能力不足时在**运行期**抛 `cvt_error`
 *          （如 "runtime_cvt fail: kernel does not support I/O switch"），因此本判据施加在
 *          它身上恒为真、毫无意义。能力信息只在 `TCreator::create()` 的返回类型上还在
 *          ——`abs_cvt` 正是用 `requires` 门控 `switch_to_get()`/`switch_to_put()` 的。
 *          `cvt_fits_direction` 就是为了把这件事封装好，避免调用方误用。
 *
 * @tparam TCvt 具体的转换器类型。
 * @tparam IsIn 流缓冲区是否具备输入方向。
 * @tparam IsOut 流缓冲区是否具备输出方向。
 * @endif
 *
 * @lang{EN}
 * @brief Whether converter @p TCvt is capable enough for a stream buffer whose direction is
 *        (@p IsIn, @p IsOut).
 *
 * Three cases, one per `base_streambuf` instantiation:
 * | Direction | Instantiation | Requirement |
 * |---|---|---|
 * | `IsIn && IsOut` | `streambuf` | `support_io_switch` (which already implies `support_get` and `support_put`) |
 * | `IsIn` only | `istreambuf` | `support_get` |
 * | `IsOut` only | `ostreambuf` | `support_put` |
 *
 * @warning **This must be applied to a concrete converter type, never to `runtime_cvt`.**
 *          `runtime_cvt` is a run-time type-erasing wrapper: it implements every interface and
 *          throws `cvt_error` at *run time* when the capability is missing (for instance
 *          "runtime_cvt fail: kernel does not support I/O switch"), so this predicate is
 *          vacuously true on it. The capability information survives only on the return type of
 *          `TCreator::create()` -- that is where `abs_cvt` gates `switch_to_get()` /
 *          `switch_to_put()` behind a `requires` clause. `cvt_fits_direction` exists to package
 *          this correctly so callers cannot get it wrong.
 *
 * @tparam TCvt The concrete converter type.
 * @tparam IsIn Whether the stream buffer has an input direction.
 * @tparam IsOut Whether the stream buffer has an output direction.
 * @endif
 */
template <typename TCvt, bool IsIn, bool IsOut>
concept cvt_direction_capable =
    ((IsIn && IsOut) ? cvt_cpt::support_io_switch<TCvt>
   :  IsIn           ? cvt_cpt::support_get<TCvt>
                     : cvt_cpt::support_put<TCvt>);

/**
 * @lang{ZH}
 * @brief 工厂 @p TCreator 在根转换器 @p TRootCvt 之上建出的管线，能力是否够得上方向为
 *        (@p IsIn, @p IsOut) 的流缓冲区。
 *
 * 求值方式与 `ext_to_int` 相同：在**未求值语境**中形式化调用 `TCreator::create(TRootCvt)`
 * 取得管线末端的转换器类型，再对该类型施加 `cvt_direction_capable`。纯编译期计算，
 * 不构造任何对象。
 *
 * @note 这是 `base_streambuf` 两个接收工厂的构造函数所用的判据。约束必须落在**构造函数**上
 *       才能得到声明层面的拒绝：这样 `io` 层无需重复检查——`istream`/`ostream`/`iostream`
 *       的构造函数约束里写有 `decltype(streambuf{...})`，本判据不满足时该表达式非良构，
 *       按 [temp.constr.atomic]/3 使那条原子约束判为不满足，对应的流构造函数随之被移除。
 * @note 输入方向的构造函数须施加**两次**（对 `rb_root_cvt` 与 `no_rb_root_cvt` 各一次）：
 *       它按运行期的 `has_in_buf` 在三元表达式里二选一，两支都会被实例化。
 *
 * @tparam TRootCvt 根转换器类型。
 * @tparam TCreator 转换器工厂类型。
 * @tparam IsIn 流缓冲区是否具备输入方向。
 * @tparam IsOut 流缓冲区是否具备输出方向。
 * @endif
 *
 * @lang{EN}
 * @brief Whether the pipeline that @p TCreator builds on top of root converter @p TRootCvt is
 *        capable enough for a stream buffer whose direction is (@p IsIn, @p IsOut).
 *
 * Evaluated the same way as `ext_to_int`: `TCreator::create(TRootCvt)` is formally invoked in an
 * **unevaluated context** to obtain the converter type at the end of the pipeline, and
 * `cvt_direction_capable` is applied to it. Purely a compile-time computation; no object is
 * constructed.
 *
 * @note This is the predicate used by `base_streambuf`'s two creator-taking constructors. The
 *       constraint has to sit on the *constructors* to get a declaration-level rejection, and
 *       that in turn means the I/O layer needs no check of its own: the constructors of
 *       `istream` / `ostream` / `iostream` mention `decltype(streambuf{...})` in their own
 *       constraints, so when this predicate fails that expression is ill-formed and, by
 *       [temp.constr.atomic]/3, the enclosing atomic constraint is not satisfied -- removing the
 *       corresponding stream constructor.
 * @note The input-direction constructor must apply this **twice**, once for `rb_root_cvt` and
 *       once for `no_rb_root_cvt`: it picks between them in a conditional expression on the
 *       run-time `has_in_buf` flag, so both branches are instantiated.
 *
 * @tparam TRootCvt The root converter type.
 * @tparam TCreator The converter creator type.
 * @tparam IsIn Whether the stream buffer has an input direction.
 * @tparam IsOut Whether the stream buffer has an output direction.
 * @endif
 */
template <typename TRootCvt, typename TCreator, bool IsIn, bool IsOut>
concept cvt_fits_direction =
    cvt_direction_capable<
        decltype(std::declval<const TCreator&>().create(std::declval<TRootCvt>())), IsIn, IsOut>;
}

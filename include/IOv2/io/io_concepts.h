// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file io_concepts.h
 * @lang{ZH}
 * I/O 层的概念与类型工具文件。
 * - `ext_to_int`——从「根转换器 + 转换器工厂」推导出转换器管线最终暴露的内部字符类型，
 *   主要服务于 streambuf 系列类型的 CTAD。
 * - `cvt_direction_capable` / `cvt_fits_direction`——判断一条转换管线的读写能力是否够得上
 *   某个方向的流缓冲区。
 * @endif
 *
 * @lang{EN}
 * Concept and type-utility file for the I/O layer.
 * - `ext_to_int`, which derives the internal character type ultimately exposed by a converter
 *   pipeline from a "root converter + converter creator" pair; it mainly serves the CTAD of the
 *   streambuf family.
 * - `cvt_direction_capable` / `cvt_fits_direction`, which decide whether a converter pipeline is
 *   capable enough for a stream buffer of a given direction.
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
 * 等价于用工厂 @p TCreator 在根转换器 @p TRootCvt 之上建出的完整转换器的 `internal_type`。
 *
 * @note 典型用途是 streambuf/istreambuf/ostreambuf 的 CTAD 推导指引：由「设备 + 工厂」
 *       反推出 streambuf 应采用的字符类型。
 *
 * @tparam TRootCvt 根转换器类型，须满足 `io_converter`（通常是包装了设备的根转换器）。
 * @tparam TCreator 转换器工厂类型，须满足 `cvt_creator`。
 * @endif
 *
 * @lang{EN}
 * @brief Derives the internal data type (`internal_type`) exposed by a converter pipeline.
 *
 * Equivalent to the `internal_type` of the complete converter that creator @p TCreator builds on
 * top of root converter @p TRootCvt.
 *
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
 * @warning **必须施加于具体的转换器类型，不能施加于 `runtime_cvt`。** `runtime_cvt` 把全部
 *          接口都实现出来，能力不足时在**运行期**抛 `cvt_error`，因此本判据施加在它身上
 *          恒为真、毫无意义。请改用 `cvt_fits_direction`，它施加于 `TCreator::create()` 的
 *          返回类型，能力信息只在那里还在。
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
 *          `runtime_cvt` implements every interface and throws `cvt_error` at *run time* when the
 *          capability is missing, so this predicate is vacuously true on it. Use
 *          `cvt_fits_direction` instead: it applies to the return type of `TCreator::create()`,
 *          which is where the capability information survives.
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
 * 即对 `TCreator::create(TRootCvt)` 的返回类型施加 `cvt_direction_capable`。
 *
 * @note 这是 `base_streambuf` 两个接收工厂的构造函数所用的判据。不满足时该构造函数被移除，
 *       连带 `istream`/`ostream`/`iostream` 对应的构造函数一并移除。
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
 * That is, `cvt_direction_capable` applied to the return type of `TCreator::create(TRootCvt)`.
 *
 * @note This is the predicate used by `base_streambuf`'s two creator-taking constructors. When it
 *       is not satisfied that constructor is removed, and with it the corresponding constructors
 *       of `istream` / `ostream` / `iostream`.
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

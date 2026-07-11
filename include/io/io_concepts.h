/**
 * @file io_concepts.h
 * @lang{ZH}
 * I/O 层的概念与类型工具文件。
 * 本文件在转换器体系（见 cvt/cvt_concepts.h）之上提供 I/O 层使用的编译期类型工具，
 * 目前包含 `ext_to_int`——用于从「根转换器 + 转换器工厂」推导出转换器管线最终暴露的
 * 内部字符类型。该别名主要服务于 streambuf 系列类型的类模板实参推导（CTAD）。
 * @endif
 *
 * @lang{EN}
 * Concept and type-utility file for the I/O layer.
 * Building on the converter system (see cvt/cvt_concepts.h), this file provides the
 * compile-time type utilities used by the I/O layer. It currently contains
 * `ext_to_int`, which derives the internal character type ultimately exposed by a
 * converter pipeline from a "root converter + converter creator" pair. This alias
 * mainly serves the class-template argument deduction (CTAD) of the streambuf family.
 * @endif
 */
#pragma once
#include <cvt/cvt_concepts.h>

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
}

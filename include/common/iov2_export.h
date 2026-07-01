/**
 * @file iov2_export.h
 * @lang{ZH}
 * 单例符号的链接/可见性宏 `IOV2_API`，用于在两种发布模式之间切换：
 * header-only（默认）与单独编译的共享库（DSO/DLL）。
 * @endif
 * @lang{EN}
 * Linkage/visibility macro `IOV2_API` for singleton symbols. Selects between the
 * two release modes: header-only (the default) and a separately-compiled shared
 * library (DSO/DLL).
 * @endif
 */

#pragma once

// ─── 发行模式开关 / Distribution-mode switch ─────────────────────────────────
// @lang{ZH}
//  header-only 发行（默认）：保持下面这行注释。
//  共享库发行：安装步骤会把它解开注释（将来迁到 CMake 后，改由
//  generate_export_header() 自动产出本文件并写好这行）。这样消费者只要
//  #include 就自动进入 shared 模式，无需在命令行上自己加 -DIOV2_SHARED，
//  从根上消除“漏宏 → 静默双实例”（方向 A）。构建 .so 时 iov2_objects.cpp 仍
//  自行 #define IOV2_SHARED/IOV2_EXPORTS；下面的 #ifndef 守卫保证两边不冲突。
// @lang{EN}
//  Header-only distribution (default): keep the line below commented out.
//  Shared-library distribution: the install step uncomments it (later, under
//  CMake, generate_export_header() emits this file with it enabled). Consumers
//  then get shared mode just by #include -- no need to pass -DIOV2_SHARED, which
//  removes the "forgot the macro -> silent duplicate instance" footgun. When
//  building the .so, iov2_objects.cpp still defines IOV2_SHARED/IOV2_EXPORTS
//  itself; the #ifndef guard below keeps the two from clashing.
#ifndef IOV2_SHARED
// #define IOV2_SHARED 1
#endif

/**
 * @lang{ZH}
 * 用法：
 *  - 默认（IOV2_SHARED、IOV2_EXPORTS 都不定义）：header-only。单例以 inline 变量
 *    在头文件中定义，`IOV2_API` 展开为空。
 *  - 消费共享库：定义 `IOV2_SHARED`。头文件只对单例做 `extern` 声明，定义来自库；
 *    Windows 上 `IOV2_API` 展开为 `dllimport`，ELF/Mach-O 上为 default 可见性。
 *  - 构建共享库：同时定义 `IOV2_SHARED` 与 `IOV2_EXPORTS`（仅在 iov2_objects.cpp）。
 *    Windows 上 `IOV2_API` 展开为 `dllexport`。
 *
 * `IOV2_EXPORTS` 仅在 Windows 上有意义——它区分“我正在构建这个库”（导出）与
 * “我在使用这个库”（导入）。ELF/Mach-O 两侧都是 default 可见性，与它无关。
 *
 * @warning `IOV2_SHARED` 必须在同一次链接的所有翻译单元中“要么都定义、要么都不
 *          定义”。混用会导致一边 inline 定义、一边 extern 声明，属于 ODR 违规。
 * @endif
 *
 * @lang{EN}
 * Usage:
 *  - Default (neither IOV2_SHARED nor IOV2_EXPORTS): header-only. Singletons are
 *    inline variables defined in the headers; `IOV2_API` expands to nothing.
 *  - Consuming the shared lib: define `IOV2_SHARED`. Headers only `extern`-declare
 *    the singletons; definitions come from the library. On Windows `IOV2_API`
 *    expands to `dllimport`, on ELF/Mach-O to default visibility.
 *  - Building the shared lib: define `IOV2_SHARED` and `IOV2_EXPORTS` (only in
 *    iov2_objects.cpp). On Windows `IOV2_API` expands to `dllexport`.
 *
 * `IOV2_EXPORTS` matters only on Windows: it distinguishes "I am building the
 * library" (export) from "I am using the library" (import). On ELF/Mach-O both
 * sides use default visibility, so it has no effect there.
 *
 * @warning `IOV2_SHARED` must be defined (or left undefined) consistently across
 *          every translation unit in a single link. Mixing the two would pair an
 *          inline definition with an extern declaration -- an ODR violation.
 * @endif
 */
#if defined(IOV2_SHARED)
#  if defined(_WIN32)
#    if defined(IOV2_EXPORTS)
#      define IOV2_API __declspec(dllexport)
#    else
#      define IOV2_API __declspec(dllimport)
#    endif
#  else
#    define IOV2_API __attribute__((visibility("default")))
#  endif
#else
#  define IOV2_API
#endif

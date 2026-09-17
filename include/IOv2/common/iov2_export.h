// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

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
 *
 * @warning **漏掉 `IOV2_SHARED` 却链接了 `libiov2.so`：能编译、能链接、通常也能跑，
 *          但进程里有两套单例**——头文件在可执行文件里定义了那些 inline 单例，.so
 *          对它们的引用被绑到那一份，随后可执行文件自己的初始化再构造一套覆盖掉。
 *          后果是两张 tie 图、两份 facet 缓存、退出时重复刷出，而链接期没有任何诊断。
 *          用安装版头文件（`make install-shared` 会打开上面那个开关）就不会遇到；
 *          直接用仓库内的头文件时请自行核对：
 *          @code
 *            # 链接了 libiov2.so 的二进制里不应有自己定义的 IOv2 单例符号
 *            readelf -d prog | grep -q libiov2.so && nm -D prog | grep ' u _ZN4IOv2'
 *          @endcode
 *          有输出即漏了宏：正确的消费者把单例符号带成 COPY 重定位（`nm` 显示 `B`），
 *          漏宏的消费者自己定义它们（`nm` 显示 `u`，STB_GNU_UNIQUE）。
 *
 * @warning **header-only 可执行文件 + 共享库插件共存**（两次独立链接，各自口径一致）
 *          只在宿主**不导出** IOv2 符号时成立。宿主若加了 `-rdynamic` /
 *          `--export-dynamic`，`libiov2.so` 对单例的引用会被宿主那一份插桩，.so 的
 *          初始化写进宿主的槽位，实测在 .so 初始化期间即崩溃。宿主请勿导出这些符号，
 *          或以 `-fvisibility=hidden` 编译。
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
 *
 * @warning **Forgetting `IOV2_SHARED` while linking `libiov2.so` compiles, links and
 *          usually runs, but leaves two sets of singletons in the process**: the headers
 *          define the inline singletons in the executable, the .so's references bind to
 *          those, and the executable's own initialization then constructs a second set
 *          over the top. The result is two tie graphs, two facet caches and a duplicated
 *          flush at exit, with no diagnostic at link time. Installed headers do not have
 *          this problem (`make install-shared` turns the switch above on); when consuming
 *          the headers straight from the source tree, check it yourself:
 *          @code
 *            # a binary linking libiov2.so must not define IOv2 singletons of its own
 *            readelf -d prog | grep -q libiov2.so && nm -D prog | grep ' u _ZN4IOv2'
 *          @endcode
 *          Any output means the macro was missed: a correct consumer carries the
 *          singleton as a COPY relocation (`nm` shows `B`), while one that missed the
 *          macro defines it itself (`nm` shows `u`, STB_GNU_UNIQUE).
 *
 * @warning **A header-only executable coexisting with a shared-library plugin** (two
 *          separate links, each internally consistent) works only while the host does
 *          **not** export IOv2 symbols. With `-rdynamic` / `--export-dynamic` on the
 *          host, `libiov2.so`'s references to the singletons are interposed by the
 *          host's copies, the .so's initialization writes into the host's slots, and it
 *          was measured to crash during that initialization. Do not export these symbols
 *          from the host, or compile it with `-fvisibility=hidden`.
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

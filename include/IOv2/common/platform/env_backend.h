// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file env_backend.h
 * @lang{ZH}
 * 读环境变量的平台后端选择入口。各后端在 `IOv2::detail` 中提供同名的 `get_env`：
 * 返回变量值的副本，变量未设置或为空时返回空串。
 *
 * 两种情况不加区分：库里读环境变量的地方本来都把它们同样处理，而 Windows 的 CRT 环境里
 * 没有"设为空"的变量（`_putenv_s` 设空串即删除），区分了也只在部分平台成立。
 *
 * 与其他线程并发的 `setenv`/`putenv`/`_putenv_s` 防不住：C++ 与 POSIX 都没有可移植的
 * 线程安全读法。库假定进程环境在启动配置之后不被并发修改；值读出后立即复制，读取窗口
 * 尽量小。
 * @endif
 * @lang{EN}
 * Platform-selection entry point for reading environment variables. Each backend provides,
 * in `IOv2::detail`, the same name, `get_env`: a copy of the variable's value, or the empty
 * string if it is unset or empty.
 *
 * The two cases are not told apart: every place in the library that reads the environment
 * treats them alike, and the Windows CRT's environment holds no variable set to the empty
 * string (`_putenv_s` with one removes the variable), so telling them apart would hold on
 * some platforms only.
 *
 * Nothing guards against a concurrent `setenv`/`putenv`/`_putenv_s` on another thread:
 * neither C++ nor POSIX offers a portable thread-safe read. The library assumes the process
 * environment is not modified concurrently once start-up configuration is done; a value is
 * copied out as soon as it is read, to keep the window small.
 * @endif
 */

#pragma once

#if defined(_WIN32)
#include <IOv2/common/platform/windows/env_backend.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <IOv2/common/platform/posix/env_backend.h>
#else
#error "IOv2 common: unsupported platform"
#endif

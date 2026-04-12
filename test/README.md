# IOv2Test: Build Instructions

[中文](#中文) | [English](#english)

---

## 中文

### 构建系统

本目录使用 Makefile 构建测试程序。支持 debug/release 两种模式，可单独构建或运行各模块测试。

### 快速开始

```bash
# 构建所有测试（debug 模式）
make

# 并行构建（推荐）
make -j8

# 构建并运行所有测试
make test
```

### 构建目标

| 目标 | 说明 |
|------|------|
| `all` | 构建所有测试程序（默认） |
| `test` | 构建并运行所有测试 |
| `valgrind` | 使用 valgrind 构建并运行所有测试 |
| `clean` | 清理当前模式的构建产物 |

### 单模块构建

| 目标 | 说明 |
|------|------|
| `device` | 仅构建 device 测试 |
| `cvt` | 仅构建 cvt 测试 |
| `facet` | 仅构建 facet 测试 |
| `locale` | 仅构建 locale 测试 |
| `io` | 仅构建 io 测试 |
| `util` | 仅构建 util 测试 |

### 单模块构建并运行

| 目标 | 说明 |
|------|------|
| `test-device` | 构建并运行 device 测试 |
| `test-cvt` | 构建并运行 cvt 测试 |
| `test-facet` | 构建并运行 facet 测试 |
| `test-locale` | 构建并运行 locale 测试 |
| `test-io` | 构建并运行 io 测试 |
| `test-util` | 构建并运行 util 测试 |

### 单模块内存检测 (Valgrind)

| 目标 | 说明 |
|------|------|
| `valgrind-device` | 使用 valgrind 构建并运行 device 测试 |
| `valgrind-cvt` | 使用 valgrind 构建并运行 cvt 测试 |
| `valgrind-facet` | 使用 valgrind 构建并运行 facet 测试 |
| `valgrind-locale` | 使用 valgrind 构建并运行 locale 测试 |
| `valgrind-io` | 使用 valgrind 构建并运行 io 测试 |
| `valgrind-util` | 使用 valgrind 构建并运行 util 测试 |

### 构建模式

通过 `MODE` 变量选择构建模式：

| 模式 | 编译选项 | 输出目录 |
|------|----------|----------|
| `debug`（默认） | `-g -O0` | `bin_debug/`, `obj_debug/` |
| `release` | `-O3 -DNDEBUG` | `bin_release/`, `obj_release/` |

### 示例

```bash
# Debug 模式构建所有
make

# Release 模式构建所有
make MODE=release

# 仅构建 device 模块
make device

# 构建并运行 device 测试
make test-device

# Release 模式构建并运行 device 测试
make MODE=release test-device

# 使用 valgrind 运行所有测试
make valgrind

# 使用 valgrind 运行 device 测试
make valgrind-device

# 清理 debug 构建产物
make clean

# 清理 release 构建产物
make MODE=release clean

# 查看帮助
make help
```

---

## English

### Build System

This directory uses a Makefile to build test programs. It supports debug/release modes and allows building or running individual module tests.

### Quick Start

```bash
# Build all tests (debug mode)
make

# Parallel build (recommended)
make -j8

# Build and run all tests
make test
```

### Build Targets

| Target | Description |
|--------|-------------|
| `all` | Build all test programs (default) |
| `test` | Build and run all tests |
| `valgrind` | Build and run all tests with valgrind |
| `clean` | Remove build artifacts for current mode |

### Single Module Build

| Target | Description |
|--------|-------------|
| `device` | Build device tests only |
| `cvt` | Build cvt tests only |
| `facet` | Build facet tests only |
| `locale` | Build locale tests only |
| `io` | Build io tests only |
| `util` | Build util tests only |

### Single Module Build and Run

| Target | Description |
|--------|-------------|
| `test-device` | Build and run device tests |
| `test-cvt` | Build and run cvt tests |
| `test-facet` | Build and run facet tests |
| `test-locale` | Build and run locale tests |
| `test-io` | Build and run io tests |
| `test-util` | Build and run util tests |

### Single Module Run with Valgrind

| Target | Description |
|--------|-------------|
| `valgrind-device` | Build and run device tests with valgrind |
| `valgrind-cvt` | Build and run cvt tests with valgrind |
| `valgrind-facet` | Build and run facet tests with valgrind |
| `valgrind-locale` | Build and run locale tests with valgrind |
| `valgrind-io` | Build and run io tests with valgrind |
| `valgrind-util` | Build and run util tests with valgrind |

### Build Modes

Use the `MODE` variable to select build mode:

| Mode | Compiler Flags | Output Directory |
|------|----------------|------------------|
| `debug` (default) | `-g -O0` | `bin_debug/`, `obj_debug/` |
| `release` | `-O3 -DNDEBUG` | `bin_release/`, `obj_release/` |

### Examples

```bash
# Build all in debug mode
make

# Build all in release mode
make MODE=release

# Build device module only
make device

# Build and run device tests
make test-device

# Build and run device tests in release mode
make MODE=release test-device

# Run all tests with valgrind
make valgrind

# Run device tests with valgrind
make valgrind-device

# Clean debug build artifacts
make clean

# Clean release build artifacts
make MODE=release clean

# Show help
make help
```

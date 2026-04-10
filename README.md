# IOv2: Refactoring C++ IO Streams

[中文](#中文) | [English](#english)

---

## 中文

### 项目简介
**IOv2** 是随书 **《重构C++IO流》** 推出的现代化 C++ 输入输出系统核心库。

不同于以往对标准库的局部修补，本项目从底层架构出发，对既有 I/O 框架进行了彻底的抽象拆解与分层重组。IOv2 致力于解决传统 C++ 标准 IO 中“结构不可组合”的核心痛点，将输入输出系统划分为三个正交且独立的维度：物理访问（设备层）、表示变换（转换器层）以及语义解释（facet 与 locale）。

**注意：本代码库的所有设计与实现方案均是为了支撑和演示《重构C++IO流》书中的理论范式而编写的实现。**

### 核心设计原则
- **职责分离 (Separation of Concerns)**：物理传输、表示转换与语义解释各司其职，禁止跨层承担职责。
- **组合优先 (Composition over Specialization)**：系统行为通过基础组件的链式组合生成，而非通过复杂的类继承体系。
- **能力边界 (Capability Boundary)**：利用 C++20 Concepts 形式化描述设备与转换器的原子能力（如读、写、定位等）。
- **零开销抽象 (Zero-overhead Abstraction)**：大量使用模板元编程与静态多态，确保架构灵活性不以牺牲性能为代价。

### 架构分层
1. **设备层 (Device)**：负责原始数据的物理传输（如文件、内存、标准流）。
2. **转换器层 (Converter)**：负责数据的表示变换（如编码转换、压缩、加密、哈希计算）。支持链式串连。
3. **流缓冲区 (Stream Buffer)**：协调数据流动并提供高效的读写辅助机制（如零拷贝优化）。
4. **语义层 (I18n)**：重构了 `facet` 与 `locale` 体系，为结构化数据提供文化规则支持。
5. **流对象 (Stream)**：最上层接口，实现块式 IO 与格式化逻辑。

### 开发环境
- **操作系统**：Fedora Linux (或其它 Linux 发行版)
- **标准**：现代 C++ (C++23 及以上)

### 已测试编译器

| 编译器 | 版本 | 标准库 | 状态 |
|--------|------|--------|------|
| GCC | 15.2.0 | libstdc++ | ✅ 通过 |
| Clang | 18.1.8 | libstdc++ (GCC 13) | ✅ 通过 |
| Clang | 22.1.3 | libstdc++ (GCC 13) | ⚠️ 编译通过，测试待验证 |

### 已知编译器问题

| 编译器 | 问题描述 | 状态 |
|--------|----------|------|
| GCC 13 | 在 Release 模式下，`-Warray-bounds` 对 `std::copy` 内联展开产生误报警告。当将单个 `wchar_t` 变量的地址传递给 `mem_device::dget()` 时，GCC 错误地认为 `__builtin_memcpy` 可能越界访问。这是 GCC 13 的已知缺陷，代码本身没有问题。 | GCC 15 已修复 |
| Clang + libc++ | libc++ 22.1.3 尚未完整实现 C++20 chrono 时区功能（`std::chrono::time_zone`、`zoned_time`、`get_tzdb` 等）。IOv2 的时间格式化功能依赖这些特性，因此暂不支持 libc++。请使用 libstdc++ 作为标准库。 | 等待 libc++ 支持 |

---

## English

### Introduction
**IOv2** is the core library of a modernized C++ Input/Output system, featured in the book **"Refactoring C++ IO Streams"**.

Instead of providing partial patches or wrappers for the existing standard library, IOv2 reconstructs the I/O framework from the ground up through architectural decomposition. It addresses the fundamental issue of "lack of composability" in traditional C++ standard I/O by isolating the system into three orthogonal dimensions: physical access (Device), representation transformation (Converter), and semantic interpretation (Facet/Locale).

**Note: This repository serves as the implementation specifically designed to support and demonstrate the theoretical paradigms discussed in the book "Refactoring C++ IO Streams".**

### Core Design Principles
- **Separation of Concerns**: Each layer is strictly limited to one semantic dimension, preventing cross-layer coupling.
- **Composition over Specialization**: System behaviors emerge from the composition of basic components rather than specialized inheritance hierarchies.
- **Capability Boundary**: Formalizes atomic capabilities (Read, Write, Seek, etc.) using C++20 Concepts.
- **Zero-overhead Abstraction**: Extensive use of template metaprogramming and static polymorphism to ensure flexibility without performance penalties.

### Architectural Layers
1. **Device Layer**: Handles raw data transmission (e.g., File, Memory, Standard Streams).
2. **Converter Layer**: Manages representation transformations (e.g., Encoding, Compression, Encryption, Hashing). Supports pipeline chaining.
3. **Stream Buffer**: Coordinates data flow and provides optimized R/W helpers (e.g., zero-copy optimizations).
4. **Semantic Layer (I18n)**: A reimagined `facet` and `locale` system for cultural and linguistic rules.
5. **Stream Layer**: Top-level interface for block I/O and formatting logic.

### Development Environment
- **OS**: Fedora Linux (Primary platform)
- **Standard**: Modern C++ (C++23 or later)

### Tested Compilers

| Compiler | Version | Standard Library | Status |
|----------|---------|------------------|--------|
| GCC | 15.2.0 | libstdc++ | ✅ Passed |
| Clang | 18.1.8 | libstdc++ (GCC 13) | ✅ Passed |
| Clang | 22.1.3 | libstdc++ (GCC 13) | ⚠️ Build passed, tests pending |

### Known Compiler Issues

| Compiler | Issue | Status |
|----------|-------|--------|
| GCC 13 | In Release mode, `-Warray-bounds` produces false positive warnings when `std::copy` is inlined. When passing the address of a single `wchar_t` variable to `mem_device::dget()`, GCC incorrectly reports that `__builtin_memcpy` may access out of bounds. This is a known GCC 13 bug; the code is correct. | Fixed in GCC 15 |
| Clang + libc++ | libc++ 22.1.3 has not fully implemented C++20 chrono time zone features (`std::chrono::time_zone`, `zoned_time`, `get_tzdb`, etc.). IOv2's time formatting functionality depends on these features, so libc++ is not currently supported. Please use libstdc++ as the standard library. | Waiting for libc++ support |


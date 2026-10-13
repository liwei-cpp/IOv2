# Changelog

[中文](#中文) | [English](#english)

---

## 中文

本文件记录 IOv2 主线（`main` 分支，tag 形如 `vX.Y.Z`）的版本变化，版本号遵循
[语义化版本](https://semver.org/lang/zh-CN/)，格式参照
[Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

`book-v1.*` 是《重构C++ IO流》（李伟 著，中文版第 1 版，人民邮电出版社，2026 年 6 月）
配套代码的独立发布线（GPL-3.0-or-later），代码冻结在出版时的状态，版本号与本库无关，
不在本文件记录。此后的书籍直接基于本库主线 1.x，不另设发布线。

### [1.0.0] - 2026-10-13

首个正式版本。自此公开接口遵循语义化版本：破坏性变更只在主版本号提升时发生。

#### 包含的组件

- **设备层**：`file_device`、`mem_device`、`std_device`，以 Concepts 描述读、写、定位等原子能力。
- **转换器层**：经 `operator|` 链式组合。
  - 编码转换：`code_cvt`、`code_cvt_stdio`。
  - 压缩：`zlib_cvt`（需 zlib）。
  - 密码学与哈希：`chacha20_cvt`、`hash_cvt`（需 Botan）、`vigenere_cvt`（仅限教学，位于 `IOv2::Crypt::Classic`）。
  - 运行时多态：`runtime_cvt`。
- **语义层**：重新设计的 `locale` 与 `collate`、`ctype`、`messages`、`monetary`、`numeric`、`timeio` 六类 facet。
- **流与通道**：`istream`、`ostream`、`iostream`、`iochannel`，操纵符，以及预定义标准流对象 `cin`/`cout`/`cerr`/`clog` 及其宽字符版本。
- **带填充的数值与货币可读回**：用相同的 locale、flags 与 `fill` 写出的字段都能原样读回；读不回的填充组合在写出侧被拒绝。
- **唯一的 I/O 扩展点** `io_traits<TChar, T>`，以及转发辅助 `compose_read`/`compose_write`。
- **流级线程安全**：单次 I/O 操作在流的 `io_mutex()` 下原子完成；`IOv2::sync` 使一组操作整体原子；`tie` 恒无环，且 tie 刷新不会死锁。
- **两种发布模式**：header-only（默认）与共享库 `libiov2.so`，经 `make install` / `make install-shared` 安装，附 pkg-config 文件。
#### 环境要求

- C++23；Linux + glibc + libstdc++。
- CI 验证的编译器：GCC 15.3、Clang 19.1（配 libstdc++）。
- 可选依赖：zlib（`zlib_cvt`）、Botan 2（`hash_cvt`）。

#### 许可证

MIT，适用于 `main` 分支 `b40a29d` 及之后的版本。更早的历史版本与 `book-v1` 线的许可
边界见 `README.md`、`NOTICE` 与 `PROVENANCE.md`。

#### 已知限制

- `chacha20_cvt` 尚未实现认证加密（AEAD），不应单独用于需要完整性保护的场景。
- 尚未适配 macOS 与 Windows。
- 读取返回 0 与 EOF 判定的解耦（#6）不在本版本范围内。

---

## English

This file records changes to the IOv2 mainline (the `main` branch, tagged
`vX.Y.Z`). Versions follow [Semantic Versioning](https://semver.org/), and the
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

`book-v1.*` is a separate release line holding the companion code of
the book 《重构C++ IO流》 (in Chinese, 1st edition; Posts & Telecom Press,
June 2026), under GPL-3.0-or-later and frozen as
published. Its version numbers are unrelated to this library's and are not
recorded here. Later books build directly on this library's 1.x mainline and
have no release line of their own.

### [1.0.0] - 2026-10-13

First stable release. From here on the public interface follows Semantic
Versioning: breaking changes come only with a major version bump.

#### Components

- **Device layer**: `file_device`, `mem_device`, `std_device`, with reading, writing and seeking described as Concepts.
- **Converter layer**, composed with `operator|`:
  - Encodings: `code_cvt`, `code_cvt_stdio`.
  - Compression: `zlib_cvt` (requires zlib).
  - Cryptography and hashing: `chacha20_cvt`, `hash_cvt` (requires Botan), `vigenere_cvt` (educational only, in `IOv2::Crypt::Classic`).
  - Runtime polymorphism: `runtime_cvt`.
- **Semantic layer**: a redesigned `locale` and six facet families: `collate`, `ctype`, `messages`, `monetary`, `numeric`, `timeio`.
- **Streams and channels**: `istream`, `ostream`, `iostream`, `iochannel`, manipulators, and the predefined `cin`/`cout`/`cerr`/`clog` with their wide-character counterparts.
- **Padded numbers and monetary values read back**: a field written under a given locale, flags and `fill` reads back as the same value under the same settings; fills that would not are refused when writing.
- **A single I/O extension point**, `io_traits<TChar, T>`, plus the forwarding helpers `compose_read`/`compose_write`.
- **Stream-level thread safety**: each I/O operation completes atomically under the stream's `io_mutex()`; `IOv2::sync` makes a group of operations atomic; `tie` can never form a cycle, and the tie flush cannot deadlock.
- **Two distribution modes**: header-only (the default) and the shared library `libiov2.so`, installed with `make install` / `make install-shared`, with pkg-config files.
#### Requirements

- C++23; Linux + glibc + libstdc++.
- Compilers verified in CI: GCC 15.3 and Clang 19.1 (on libstdc++).
- Optional dependencies: zlib (`zlib_cvt`), Botan 2 (`hash_cvt`).

#### License

MIT, for the `main` branch at `b40a29d` and later. The licence boundary for
earlier history and for the `book-v1` line is set out in `README.md`, `NOTICE`
and `PROVENANCE.md`.

#### Known limitations

- `chacha20_cvt` is not yet authenticated encryption (AEAD); do not rely on it alone where integrity matters.
- macOS and Windows are not yet supported.
- Decoupling "read returned 0" from EOF detection (#6) is outside this release.

[1.0.0]: https://github.com/liwei-cpp/IOv2/releases/tag/v1.0.0

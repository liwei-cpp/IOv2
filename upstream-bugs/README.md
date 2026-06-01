# Upstream Bugs

[中文](#中文) | [English](#english)

---

## 中文

### 简介

在 **IOv2** 的开发过程中,我们陆续在所依赖或与之交互的开源项目里发现了一些 bug,并向上游进行了报告。本目录用于集中追踪这些已上报的 bug,方便日后查阅、回溯与跟进上游进展。

### 内容组织

每一个上报的 bug 在本目录下对应一个独立的 `.md` 文件,文件命名建议采用 `<上游项目>-<简短主题>.md` 的形式(例如 `gcc-num_get-grouped-overflow.md`)。每个 `.md` 文件中通常包含:

- **上游链接 (Upstream tracker)**: 指向 GCC Bugzilla、LLVM Issue Tracker 等上游缺陷追踪系统的 URL,以及对应的 bug 编号。
- **关键时间点 (Timestamps)**: 创建时间、上游最近一次修改时间。
- **当前状态 (Current status)**: 上游标记的 Status / Resolution(如 `UNCONFIRMED` / `NEW` / `RESOLVED FIXED` 等),以及目前是否已经被 assignee 接手、是否已有补丁。
- **Bug 描述 (Description)**: 触发条件、根因分析、最小复现说明;必要时引用相关标准条款(如 LWG 缺陷编号、ISO C++ 段落)。
- **IOv2 的处理 (What IOv2 does)**: IOv2 内部是否已经做了对应修复或规避,如果没有则说明原因(例如这是编译器优化缺陷,IOv2 无法在库代码层规避)。

### 已上报 bug 列表

| 上游项目 | 编号 | 主题 | 状态 |
|---|---|---|---|
| GCC / 优化器 (IPA) | [PR 121814](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=121814) | [`-O2/-O3` 误编译指针 round-trip(IPA escape 分析丢失等价性)](gcc-pointer-roundtrip-escape.md) | `NEW` |
| GCC / libstdc++ | [PR 125499](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125499) | [`num_get` 在带分组的溢出输入下返回 0,违反 LWG 23](gcc-num_get-grouped-overflow.md) | `UNCONFIRMED` |
| GCC / libstdc++ | [PR 125500](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125500) | [`num_put::do_put(const void*)` / `num_get::do_get(void*&)` 在内部 iterator 抛异常时泄漏 fmtflags](gcc-num_put-num_get-void_ptr-flags-leak.md) | `UNCONFIRMED` |
| GCC / libstdc++ | [PR 125505](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125505) | [`num_put` 在 `std::fixed`/`std::scientific` + 近 `INT_MAX` 精度下崩溃(SIGSEGV),根因是 `_M_insert_float` 未检查 `__convert_from_v` 的负返回](gcc-num_put-float-precision-overflow.md) | `UNCONFIRMED` |
| GCC / libstdc++ | [PR 125554](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125554) | [`money_get` 把每组位数截断成 `char`,导致单组 256·k+g 位的畸形分组被误判为合法](gcc-money_get-group-count-truncation.md) | `UNCONFIRMED` |

> 表格中的"编号"指上游缺陷追踪系统分配的 ID(如 GCC PR 号、LLVM Issue 号)。状态字段建议使用 `Reported` / `Confirmed` / `Fixed` / `WontFix` 等简短标记。

---

## English

### Introduction

During the development of **IOv2**, we have occasionally found bugs in the open-source systems we depend on or interact with, and reported them upstream. This directory centralizes the tracking of those reported bugs so they remain easy to find, revisit, and follow up on as upstream progresses.

### Layout

Each reported bug is tracked in a single `.md` file under this directory. Suggested filename: `<upstream-project>-<short-topic>.md` (e.g. `gcc-num_get-grouped-overflow.md`). A typical bug `.md` file contains:

- **Upstream tracker**: the URL to the upstream issue (GCC Bugzilla, LLVM Issue Tracker, etc.) and the assigned bug ID.
- **Timestamps**: the creation time and the most recent upstream-modified time.
- **Current status**: the upstream `Status` / `Resolution` (e.g. `UNCONFIRMED`, `NEW`, `RESOLVED FIXED`), whether an assignee has picked it up, and whether a patch exists.
- **Description**: trigger conditions, root-cause analysis, and a minimal reproducer summary; relevant standard clauses (LWG defect numbers, ISO C++ paragraph references) where applicable.
- **What IOv2 does**: whether IOv2 has fixed or worked around the issue internally, and if not, why (e.g. the bug lives in the compiler optimizer and cannot be addressed at the library level).

### Reported bugs

| Upstream project | ID | Title | Status |
|---|---|---|---|
| GCC / optimizer (IPA) | [PR 121814](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=121814) | [`-O2/-O3` miscompiles pointer round-trip (IPA escape analysis loses the equality)](gcc-pointer-roundtrip-escape.md) | `NEW` |
| GCC / libstdc++ | [PR 125499](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125499) | [`num_get` returns 0 on grouped overflow, violating LWG 23](gcc-num_get-grouped-overflow.md) | `UNCONFIRMED` |
| GCC / libstdc++ | [PR 125500](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125500) | [`num_put::do_put(const void*)` / `num_get::do_get(void*&)` leak fmtflags when inner iterator throws](gcc-num_put-num_get-void_ptr-flags-leak.md) | `UNCONFIRMED` |
| GCC / libstdc++ | [PR 125505](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125505) | [`num_put` crashes (SIGSEGV) on `std::fixed`/`std::scientific` with a near-`INT_MAX` precision (unchecked negative `__convert_from_v` return in `_M_insert_float`)](gcc-num_put-float-precision-overflow.md) | `UNCONFIRMED` |
| GCC / libstdc++ | [PR 125554](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125554) | [`money_get` truncates each group's digit count to `char`, so a single 256·k+g-digit malformed group is accepted as valid](gcc-money_get-group-count-truncation.md) | `UNCONFIRMED` |

> The `ID` column refers to the identifier assigned by the upstream tracker (e.g. GCC PR number, LLVM Issue number). Recommended status values: `Reported` / `Confirmed` / `Fixed` / `WontFix`.

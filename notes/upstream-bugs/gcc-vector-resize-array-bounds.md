# GCC — bogus `-Warray-bounds` in `std::vector<char>::resize(1)` at `-O3` (reallocation branch of `_M_default_append`)

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=128679>
- **Upstream project / component**: GCC / filed as `c++`
- **Bug ID**: PR 128679
- **Reported**: 2026-11-12 19:39 UTC by liwei (Version: 16.2.1)
- **Last modified upstream**: 2026-11-12 19:39 UTC
- **Current status**: `UNCONFIRMED` — submitted, awaiting maintainer triage; `Assignee: unassigned`, no comments yet.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-11-12 19:39 UTC。同日在新机器(Fedora 45)上用 GCC 16.2.1 首次构建 IOv2 时发现,查明触发条件、与 GCC 15 对比、排除已知同类 bug 后提交。

### 当前状态

`UNCONFIRMED`(已提交,尚未被 GCC 维护者确认),无人接手,暂无评论。后续状态变更请以上游链接为准:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=128679>。

### 发现经过

`gcc-release`(`-O3 -Wall -Werror`)下,整个测试树 197 个源文件中只有 `test/cvt/comp/test_zlib_cvt_char.cpp` 编译失败,致使 `cvt.comp` 无法运行;其余 76 个测试全部通过。内联链落在 `zlib_cvt::get_main` 调用的 `reader.reset(1)`(`include/IOv2/cvt/comp/zlib_cvt.h`),即 `cvt_reader::reset`(`include/IOv2/cvt/abs_cvt.h`)里的 `if (m_buffer.size() < buf_size) m_buffer.resize(buf_size);`,最终是 `std::vector<char>::resize(1)`。CI 的 GCC 15 一直通过。

### Bug 描述

诊断落在 libstdc++ 16 `vector::_M_default_append` 的**重新分配分支**(本机头文件 `bits/vector.tcc:862`):该分支申请一块 `__len` 个元素的新缓冲区,再从 `__new_start + __size` 起默认初始化 `__n` 个新元素,对 `char` 即一次 `__builtin_memset`。

警告称 memset 的起点 offset 1 超出 [0, 1],即新缓冲区只有 1 个元素而写入从下标 1 开始——这要求 `__len == 1` 且 `__size == 1`。但 `__len` 由 `_M_check_len(__n, ...)` 算出,至少为 `__size + __n`(此处为 2),该路径不可达。看来 `__len` 与 `__size` 的关系在内联后丢失了。

触发条件(本机 g++ 16.2.1 实测):

- `-O3` 报,`-O2` 不报。起作用的是 `-O3` 调高的早期内联预算,而非 `-O3` 独有的优化 pass:`-O2 --param early-inlining-insns=8` 起即报(`-O3` 默认 14),`-O3 --param early-inlining-insns=6` 不报;`-O3` 比 `-O2` 多开的 13 个 `-f` 选项逐个开关都不影响结果。
- 只对字节大小的元素类型(`char`、`signed`/`unsigned char`、`char8_t`、`std::byte`)报;`bool`、`short`、`int` 不报。
- 只对常量 1 报;`resize(2)`、运行期 `n` 不报。
- `-std=c++11` 到 `c++26` 全部报。

与已知 bug 的区别:

- PR 114945(已修)与 PR 123850(GCC 15 回归)都在**原地追加分支**;114945 的修复已在本机头文件中,123850 的复现器在 16.2.1 上不报。
- PR 122197(推测性去虚化与中端警告,见 [gcc-devirt-speculative-array-bounds.md](gcc-devirt-speculative-array-bounds.md)):这里没有虚调用,`-fno-devirtualize-speculatively` 无效。
- PR 105746、107699、124945、94985 等 `-Warray-bounds` 误报涉及 `memcpy`、`std::sort`、`strncpy`,与此无关。

是 GCC 16 的回归:同机 `g++-15`(15.3.1 20260722,Red Hat 15.3.1-1)在 `-O2`、`-O3`、`-O2 --param early-inlining-insns=14` 下都不报,并能以 `-O3 -Werror` 编过 `test_zlib_cvt_char.cpp`。`g++-15` 用的是自带的 libstdc++ 15 头文件,因此这一对比不能区分问题在编译器还是在 libstdc++ 16 的 `vector` 实现。

### 最小复现

复现器为本项目自写,随上游 bug 一并提交,全文见英文部分。

### IOv2 的处理

已规避,见 PR #550:`cvt_reader::reset` 与 `cvt_writer::reset` 不再调用 `resize(buf_size)`,改为 `m_buffer.insert(m_buffer.end(), buf_size - m_buffer.size(), char_type{})`,在末尾追加同样的值初始化元素,语义不变。先试过的 `resize(buf_size, char_type{})` 在真实代码里只是把这个警告换成了一个 `-Wstringop-overflow` 误报("writing 8 bytes into a region of size 2"),因此没有采用。规避后 GCC 16.2.1 与 15.3.1 在 `-O3 -Werror` 下都能编过 `test_zlib_cvt_char.cpp`。上游修复后可改回 `resize`。

---

## English

### Creation time

2026-11-12 19:39 UTC. Found the same day during the first IOv2 build with GCC 16.2.1 on a new machine (Fedora 45), and submitted after narrowing down the trigger, comparing with GCC 15 and ruling out known reports of the same family.

### Current status

`UNCONFIRMED` (submitted, not yet confirmed by a GCC maintainer), unassigned, no comments yet. For later status changes, see <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=128679>.

### How it was found

Under `gcc-release` (`-O3 -Wall -Werror`), only `test/cvt/comp/test_zlib_cvt_char.cpp` out of the test tree's 197 sources failed to compile, so `cvt.comp` could not run; the other 76 tests all passed. The inline chain ends in `reader.reset(1)` called from `zlib_cvt::get_main` (`include/IOv2/cvt/comp/zlib_cvt.h`), that is, `if (m_buffer.size() < buf_size) m_buffer.resize(buf_size);` in `cvt_reader::reset` (`include/IOv2/cvt/abs_cvt.h`): a `std::vector<char>::resize(1)`. CI's GCC 15 has always passed.

### Description

The diagnostic lands in the **reallocation branch** of libstdc++ 16's `vector::_M_default_append` (`bits/vector.tcc:862` in the local header): that branch allocates a new buffer of `__len` elements and default-initializes the `__n` new elements starting at `__new_start + __size`, which for `char` is a single `__builtin_memset`.

The warning says the memset starts at offset 1, outside [0, 1]: a new buffer of one element written from index 1. That needs `__len == 1` with `__size == 1`. But `__len` comes from `_M_check_len(__n, ...)` and is at least `__size + __n` (2 here), so the path cannot be taken. The relation between `__len` and `__size` appears to be lost after inlining.

Trigger conditions (measured with g++ 16.2.1):

- Warns at `-O3`, not at `-O2`. What matters is the early-inlining budget `-O3` raises, not an `-O3`-only pass: `-O2 --param early-inlining-insns=8` and up warns (the `-O3` default is 14), `-O3 --param early-inlining-insns=6` does not, and toggling each of the 13 `-f` flags `-O3` adds over `-O2` one at a time changes nothing.
- Only byte-sized element types (`char`, `signed`/`unsigned char`, `char8_t`, `std::byte`); `bool`, `short` and `int` are silent.
- Only the constant 1; `resize(2)` and a run-time `n` are silent.
- Every `-std` from `c++11` to `c++26` warns.

How it differs from known reports:

- PR 114945 (fixed) and PR 123850 (a GCC 15 regression) are in the **in-place branch**; 114945's fix is in the local header, and 123850's testcase is silent with 16.2.1.
- PR 122197 (speculative devirtualization vs middle-end warnings, see [gcc-devirt-speculative-array-bounds.md](gcc-devirt-speculative-array-bounds.md)): there is no virtual call here, and `-fno-devirtualize-speculatively` does not help.
- PRs 105746, 107699, 124945 and 94985, other `-Warray-bounds` false positives, involve `memcpy`, `std::sort` and `strncpy` and are unrelated.

A GCC 16 regression: on the same machine, `g++-15` (15.3.1 20260722, Red Hat 15.3.1-1) is silent at `-O2`, at `-O3` and at `-O2 --param early-inlining-insns=14`, and compiles `test_zlib_cvt_char.cpp` at `-O3 -Werror`. `g++-15` uses its own libstdc++ 15 headers, so this comparison cannot tell whether the cause is in the compiler or in libstdc++ 16's `vector`.

### Minimal reproducer

Written for this report and submitted with it:

```cpp
// g++ -O3 -Wall -c pr.cpp
#include <vector>
void f(std::vector<char>& v) { v.resize(1); }
```

With g++ 16.2.1 20261029 (Red Hat 16.2.1-3):

```text
In function '... std::__fill_a1(_Up*, _Up*, const _Tp&) [with _Up = char; _Tp = char]',
    ...
    inlined from 'void std::vector<_Tp, _Alloc>::_M_default_append(size_type) [with _Tp = char; ...]' at /usr/include/c++/16/bits/vector.tcc:862:35,
    inlined from 'void std::vector<_Tp, _Alloc>::resize(size_type) [with _Tp = char; ...]' at /usr/include/c++/16/bits/stl_vector.h:1136:21,
    inlined from 'void f(std::vector<char>&)' at pr.cpp:3:40:
/usr/include/c++/16/bits/stl_algobase.h:949:25: warning: 'void* __builtin_memset(void*, int, long unsigned int)' offset 1 is out of the bounds [0, 1] [-Warray-bounds=]
```

### What IOv2 does

Worked around in PR #550: `cvt_reader::reset` and `cvt_writer::reset` no longer call `resize(buf_size)` but `m_buffer.insert(m_buffer.end(), buf_size - m_buffer.size(), char_type{})`, which appends the same value-initialized elements. `resize(buf_size, char_type{})` was tried first; in the real code it only traded this warning for a bogus `-Wstringop-overflow` ("writing 8 bytes into a region of size 2"), so it was not used. With the workaround, GCC 16.2.1 and 15.3.1 both build `test_zlib_cvt_char.cpp` at `-O3 -Werror`. Once upstream fixes the bug, `resize` can come back.

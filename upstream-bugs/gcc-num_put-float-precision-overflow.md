# GCC libstdc++ — `num_put` crashes (SIGSEGV) on `std::fixed`/`std::scientific` with a near-`INT_MAX` stream precision

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125505>
- **Upstream project / component**: GCC / `libstdc++`
- **Bug ID**: PR 125505
- **Reported**: 2026-05-29 20:38 UTC
- **Last modified upstream**: 2026-05-29 20:38 UTC
- **Current status**: `UNCONFIRMED` — submitted, awaiting maintainer triage; not yet assigned, `P3 / normal`, no maintainer comments.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-05-29 20:38 UTC(本地复现与最小化案例完成后,于同日提交至 GCC Bugzilla)。复现环境:GCC 15.1.0(Homebrew,arm64,libstdc++.6,Apple libc)。

### 当前状态

`UNCONFIRMED`(已提交,尚未被 GCC 维护者确认)。Assignee 字段仍为 *Not yet assigned to anyone*,Importance 为 `P3 / normal`,目前没有任何来自上游开发者的评论。后续状态变更请以上游链接为准:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125505>。

> 复现于 macOS / Apple libc;libstdc++ 的代码路径与平台无关,唯一与平台相关的前提——`vsnprintf` 在输出超过 `INT_MAX` 时返回负值——在 glibc 上同样成立。

### Bug 描述

`std::num_put` 在格式化浮点数时,会把 `str.precision()` 直接作为转换说明符里的精度字段传给底层 C 库的 `vsnprintf`。当用户开启 `std::fixed`(或 `std::scientific`)并把精度设到 `INT_MAX` 附近时,libstdc++ 会**崩溃(SIGSEGV)**,而不是产出对应的(巨大的)字符串或干净地失败。

`ios_base::precision(streamsize)` 接受任意 `streamsize`,所以 `os.precision(INT_MAX)` 是完全合法的标准用法。

根因位于 `libstdc++-v3/include/bits/locale_facets.tcc` 的 `num_put<>::_M_insert_float`(GCC 15.1.0 行号):

```cpp
// 1009: 只钳制了负值,没有钳制 > INT_MAX
const streamsize __prec = __io.precision() < 0 ? 6 : __io.precision();
// 1015:
int __len;
...
// 1027/1028: 初始缓冲很小(__max_digits*3,double 约 51),不含 __prec
int   __cs_size = __max_digits * 3;
char* __cs = static_cast<char*>(__builtin_alloca(__cs_size));
// 1030: 变参调用,"%.*f" 通过 va_arg 取 int 精度,最终返回 vsnprintf 的结果
__len = std::__convert_from_v(_S_get_c_locale(), __cs, __cs_size, __fbuf, __prec, __v);
// 1037: 唯一的判断 —— 捕捉不到负的 __len
if (__len >= __cs_size) { __cs_size = __len + 1; __cs = __builtin_alloca(__cs_size); ... }
...
// 1071-1073: 负的 __len 被当成长度复用
_CharT* __ws = static_cast<_CharT*>(__builtin_alloca(sizeof(_CharT) * __len));
__ctype.widen(__cs, __cs + __len, __ws);
```

关键链条:

1. `%.*f` 在精度 = `INT_MAX` 时,要产出约 21 亿位小数,总长度超过 `INT_MAX`。C 标准下 `vsnprintf` 的返回类型是 `int`,装不下这个长度;glibc 与 Apple libc 都会**返回 -1 并置 `errno = EOVERFLOW`**。
2. libstdc++ 把该返回值存进 `int __len`(= -1),而**唯一的检查是 `if (__len >= __cs_size)`**(第 1037 行),`-1 >= 51` 为假,于是把 -1 当成「转换成功、长度 -1」继续。
3. `-1` 随后被当成长度复用:`__builtin_alloca(sizeof(_CharT) * __len)`(第 1071 行)、`widen(__cs, __cs + __len, __ws)`(第 1073 行),以及随后的 `_M_pad`。在 `size_t` 语境下 `-1` 变成 `~SIZE_MAX`,触发一次越界 `memmove` → **SIGSEGV**。

存在两种崩溃模式(开启 `std::fixed` 时都崩):

| 精度(`std::fixed`) | 底层 `vsnprintf` | libstdc++ 结果 |
|---|---|---|
| `INT_MAX` | 返回 **-1**(EOVERFLOW) | `int __len = -1` 未检查 → `_M_pad` 中 `memmove(~SIZE_MAX)` → **SIGSEGV** |
| `INT_MAX - 100` | 返回 **~2.1×10⁹** | `__len >= __cs_size` 成立 → 重试时 `alloca(__len+1)` ≈ 2 GB 栈分配 → **栈溢出 SIGSEGV** |
| 默认 `%g`(未开 fixed) | 输出很短(去掉尾零) | 不崩 —— 因此普通 `std::cout << x` 测试发现不了 |

最小复现(`repro_gcc_num_put_precision_overflow.cpp`,见仓库根目录):

```cpp
#include <ios>
#include <limits>
#include <sstream>
int main() {
    std::ostringstream os;
    os << std::fixed;
    os.precision(std::numeric_limits<int>::max());   // INT_MAX,合法 streamsize
    os << 1.5;                                        // SIGSEGV
}
```

实测(GCC 15.1.0,arm64):程序在 `os << 1.5` 处收到 `EXC_BAD_ACCESS`;`lldb` 显示崩溃发生在 `_platform_memmove`,其调用者(`lr`)为 `libstdc++.6.dylib std::num_put<char, …>::_M_pad(...)`,`memmove` 的大小参数(`x2`)为 `0xffffffffff805f7f`(即被复用的负长度在 `size_t` 下的取值)。崩溃栈被巨型 `alloca`/`memmove` 破坏,故 `lldb` 只能展开到 `memmove` 这一帧;但 `lr` + 大小参数已足以定位到 `_M_insert_float` 的未检查负返回值。

直接验证前提(同机),展示负返回:

```
snprintf(buf, n, "%.*f", INT_MAX,     1.5) -> -1,         errno=EOVERFLOW
snprintf(buf, n, "%.*f", INT_MAX-100, 1.5) -> 2147483549, errno=0
```

#### 是否符合 C / C++ 标准

- **C 库一侧符合**:C 标准下 `snprintf` 返回 `int`,当应写出的字符数超过 `INT_MAX` 时返回负值并置 `EOVERFLOW` 是合规行为——C 从不承诺能产出 `> INT_MAX` 的输出。
- **C++(libstdc++)一侧不符合**:`[facet.num.put.virtuals]` 要求 `num_put` 按 `str.precision()` 格式化输出,并未给出上限,也未授权「崩溃」。崩溃既不是合规的输出,也不是「资源耗尽时干净抛 `bad_alloc`」那条豁免(`alloca` 栈溢出是未定义行为,不是 `bad_alloc`)。因此这是一处 libstdc++ 的实现缺陷 / 潜在 UB。

#### 修复建议

在 `__convert_from_v` 返回后、复用 `__len` 之前,检查负返回值并优雅处理,例如:

```cpp
__len = std::__convert_from_v(...);
if (__len < 0)
  {
    // 转换无法表示该长度(EOVERFLOW):置 badbit / 抛异常,绝不把负值当长度用
    __io.setstate(ios_base::badbit);   // 或者按既有约定处理
    return __s;
  }
if (__len >= __cs_size) { ... }
```

更彻底的做法是在进入转换前对 `__prec` 设上限(任何会让总输出超过 `INT_MAX` 的精度都不可能产出合法结果)。

### IOv2 的处理

IOv2 的 `numeric` facet 在 `insert_float` 末尾也走同一族 `snprintf` 调用(`include/facet/numeric.h:309`:`snprintf(buf, size, fbuf, static_cast<int>(prec), v)`),并据精度计算缓冲大小(`numeric.h:294`:`cs_size = max_exponent10 + prec + 32`)。在精度类型为 `streamsize` 时,IOv2 同样存在这一潜在缺陷。

我们已在 IOv2 内部从**根本上**规避:

- **文件**:`include/io/io_base.h`、`include/io/io_manip.h`、`include/facet/numeric.h`。
- **改动**:把流的精度类型从 `std::streamsize` 收窄为 `std::uint8_t`(`io_base.h` 的 `precision()` getter/setter 与成员 `m_precision`),`setprecision` 的接口同样只接受 `std::uint8_t`(`io_manip.h`)。于是精度被限定在 `0..255`。
- **结果**:`numeric.h:294` 的缓冲尺寸恒为小值(`max_exponent10 + 255 + 32`,double 下约 595),`numeric.h:309` 传给 `snprintf` 的精度恒 `≤ 255`——底层转换永远不会被要求产出超长输出,因而**不可能返回负值或超长长度**。能触发该崩溃的病态精度在 IOv2 的接口层就无法表达。
- **注释**:`numeric.h:278` 处注明精度现为有界 `uint8_t`,不再需要负值/越界的归一化处理。

代价(已知并接受):`setprecision(-1)` 会回绕成 `255`、`setprecision(256)` 回绕成 `0`,且失去了「负精度 = 用默认值」这一 sentinel——但换来的是该类崩溃在本库中结构性不可达。

---

## English

### Created at

2026-05-29 20:38 UTC. Submitted to GCC Bugzilla the same day the local reproducer and minimal test case were finalized. Reproduced on GCC 15.1.0 (Homebrew, arm64, libstdc++.6, Apple libc).

### Current status

`UNCONFIRMED` — submitted, awaiting upstream triage. The `Assignee` field still reads *Not yet assigned to anyone*; `Importance` is `P3 / normal`; there are no maintainer comments yet. For up-to-date status, see <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125505>.

> Reproduced on macOS / Apple libc; the libstdc++ code path is platform-independent, and the only platform-dependent precondition — `vsnprintf` returning a negative value when the output would exceed `INT_MAX` — also holds on glibc.

### Bug description

When formatting a floating-point value, `std::num_put` passes `str.precision()` straight through as the precision field of the conversion specifier handed to the C library's `vsnprintf`. With `std::fixed` (or `std::scientific`) and a precision set at/near `INT_MAX`, libstdc++ **crashes (SIGSEGV)** instead of producing the (enormous) string or failing cleanly.

`ios_base::precision(streamsize)` accepts any `streamsize`, so `os.precision(INT_MAX)` is perfectly well-formed standard usage.

The root cause is in `num_put<>::_M_insert_float`, `libstdc++-v3/include/bits/locale_facets.tcc` (GCC 15.1.0 line numbers):

```cpp
// 1009: clamps the negative side only, never > INT_MAX
const streamsize __prec = __io.precision() < 0 ? 6 : __io.precision();
// 1015:
int __len;
...
// 1027/1028: small initial buffer (__max_digits*3, ~51 for double); excludes __prec
int   __cs_size = __max_digits * 3;
char* __cs = static_cast<char*>(__builtin_alloca(__cs_size));
// 1030: variadic; "%.*f" consumes the precision via va_arg(int) and ultimately
//       returns vsnprintf's result
__len = std::__convert_from_v(_S_get_c_locale(), __cs, __cs_size, __fbuf, __prec, __v);
// 1037: the ONLY guard -- a negative __len slips through
if (__len >= __cs_size) { __cs_size = __len + 1; __cs = __builtin_alloca(__cs_size); ... }
...
// 1071-1073: the negative __len is reused as a length
_CharT* __ws = static_cast<_CharT*>(__builtin_alloca(sizeof(_CharT) * __len));
__ctype.widen(__cs, __cs + __len, __ws);
```

The chain:

1. `%.*f` at precision `INT_MAX` would emit ~2.1 billion fractional digits; the total length exceeds `INT_MAX`. C's `vsnprintf` returns `int`, which cannot represent that length, so both glibc and Apple libc **return -1 and set `errno = EOVERFLOW`**.
2. libstdc++ stores that in `int __len` (= -1), and the **only** check is `if (__len >= __cs_size)` (line 1037). `-1 >= 51` is false, so the -1 is mistaken for "conversion succeeded, length -1" and execution continues.
3. The `-1` is then reused as a length: `__builtin_alloca(sizeof(_CharT) * __len)` (line 1071), `widen(__cs, __cs + __len, __ws)` (line 1073), and later `_M_pad`. In a `size_t` context `-1` becomes `~SIZE_MAX`, driving an out-of-bounds `memmove` → **SIGSEGV**.

Two crash regimes (both with `std::fixed`):

| precision (`std::fixed`) | underlying `vsnprintf` | libstdc++ outcome |
|---|---|---|
| `INT_MAX` | returns **-1** (EOVERFLOW) | `int __len = -1` unchecked → `memmove(~SIZE_MAX)` in `_M_pad` → **SIGSEGV** |
| `INT_MAX - 100` | returns **~2.1×10⁹** | `__len >= __cs_size` → retry `alloca(__len+1)` ≈ 2 GB → **stack-overflow SIGSEGV** |
| default `%g` (no fixed) | short (trailing zeros stripped) | fine — so plain `std::cout << x` never hits it |

Minimal reproducer (`repro_gcc_num_put_precision_overflow.cpp`, repo root):

```cpp
#include <ios>
#include <limits>
#include <sstream>
int main() {
    std::ostringstream os;
    os << std::fixed;
    os.precision(std::numeric_limits<int>::max());   // INT_MAX, a legal streamsize
    os << 1.5;                                        // SIGSEGV
}
```

Observed (GCC 15.1.0, arm64): the process takes `EXC_BAD_ACCESS` at `os << 1.5`. `lldb` shows the fault in `_platform_memmove`, whose caller (`lr`) is `libstdc++.6.dylib std::num_put<char, …>::_M_pad(...)`, with the `memmove` size argument (`x2`) equal to `0xffffffffff805f7f` — the reused negative length as a `size_t`. The giant `alloca`/`memmove` corrupts the stack, so `lldb` can only unwind to the `memmove` frame, but the `lr` + size argument are enough to localize it to the unchecked negative return in `_M_insert_float`.

Direct precondition check (same machine), showing the negative return:

```
snprintf(buf, n, "%.*f", INT_MAX,     1.5) -> -1,         errno=EOVERFLOW
snprintf(buf, n, "%.*f", INT_MAX-100, 1.5) -> 2147483549, errno=0
```

#### Does it meet the C / C++ standard?

- **C library side: conforming.** Under C, `snprintf` returns `int` and returning a negative value with `EOVERFLOW` when the would-be output exceeds `INT_MAX` is legal — C never promises to produce `> INT_MAX` output.
- **C++ (libstdc++) side: non-conforming.** `[facet.num.put.virtuals]` requires `num_put` to format using `str.precision()`, with no upper bound and no license to crash. A SIGSEGV is neither the required output nor the "clean `bad_alloc` on resource exhaustion" escape (an `alloca` stack overflow is undefined behavior, not `bad_alloc`). This is an implementation defect / latent UB in libstdc++.

#### Suggested fix

After `__convert_from_v` and before reusing `__len`, check for a negative return and handle it gracefully:

```cpp
__len = std::__convert_from_v(...);
if (__len < 0)
  {
    // conversion cannot represent the length (EOVERFLOW): set badbit / throw;
    // never use the negative value as a length.
    __io.setstate(ios_base::badbit);   // or per the established convention
    return __s;
  }
if (__len >= __cs_size) { ... }
```

A stronger fix is to bound `__prec` before the conversion (any precision large enough to push total output past `INT_MAX` cannot yield a valid result anyway).

### What IOv2 does

IOv2's `numeric` facet ends `insert_float` on the same family of `snprintf` call (`include/facet/numeric.h:309`: `snprintf(buf, size, fbuf, static_cast<int>(prec), v)`) and sizes its buffer from the precision (`numeric.h:294`: `cs_size = max_exponent10 + prec + 32`). With a `streamsize` precision, IOv2 carried the same latent defect.

We have eliminated it at the **root**:

- **Files**: `include/io/io_base.h`, `include/io/io_manip.h`, `include/facet/numeric.h`.
- **Change**: narrowed the stream precision type from `std::streamsize` to `std::uint8_t` — the `precision()` getter/setter and the `m_precision` member in `io_base.h`, and `setprecision`'s interface in `io_manip.h` (now `std::uint8_t` only). Precision is thereby bounded to `0..255`.
- **Result**: `numeric.h:294`'s buffer size is always small (`max_exponent10 + 255 + 32`, ~595 for `double`), and the precision passed to `snprintf` at `numeric.h:309` is always `≤ 255` — the underlying conversion can never be asked for an oversized output, so it can never return a negative or overflowing length. The pathological precision that triggers this crash is **unrepresentable** at IOv2's interface.
- **Comment**: `numeric.h:278` documents that precision is now a bounded `uint8_t`, so the old negative / out-of-range normalization is no longer needed.

Known, accepted trade-off: `setprecision(-1)` wraps to `255` and `setprecision(256)` wraps to `0`, and the "negative precision = use default" sentinel is gone — in exchange, this class of crash is structurally unreachable in IOv2.

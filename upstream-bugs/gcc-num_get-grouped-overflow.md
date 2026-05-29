# GCC libstdc++ — `num_get` returns 0 on grouped overflow (violates LWG 23)

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125499>
- **Upstream project / component**: GCC / `libstdc++`
- **Bug ID**: PR 125499
- **Reported**: 2026-05-29 15:23 UTC
- **Last modified upstream**: 2026-05-29 15:23 UTC
- **Current status**: `UNCONFIRMED` — submitted, awaiting maintainer triage; not yet assigned, no comments from GCC maintainers.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-05-29 15:23 UTC(本地复现与最小化案例完成后,于同日提交至 GCC Bugzilla)。

### 当前状态

`UNCONFIRMED`(已提交,尚未被 GCC 维护者确认)。Assignee 字段仍为 *Not yet assigned to anyone*,Importance 为 `P3 / normal`,目前没有任何来自上游开发者的评论。后续状态变更请以上游链接为准:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125499>。

### Bug 描述

LWG 缺陷 23("Num_get overflow result")规定:当输入字段的数值超过目标类型的表示范围时,`std::num_get` 必须把目标变量置为 `numeric_limits<T>::max()`(或带符号负溢出时的 `::min()`)并设置 `failbit`。

libstdc++ 在**没有千位分隔符**的输入下能够正确遵守 LWG 23,但是当输入是**符合当前 locale 分组规则的、带千位分隔符的、且数值溢出**的字符串时,它会把目标变量置为 `0`,而不是 `numeric_limits::max()`。

根因位于 `libstdc++-v3/include/bits/locale_facets.tcc` 中 `_M_extract_int` 函数末尾的判定块(注释 `_GLIBCXX_RESOLVE_LIB_DEFECTS 23` 紧挨在上方):

- 数字累加循环里有一处短路:`if (__result > __smax) __testoverflow = true; else { ...; ++__sep_pos; }`。一旦溢出触发,`++__sep_pos` 就会被跳过,导致 `__sep_pos` 冻结在溢出发生时的旧值。
- 之后再来的、本身结构完全合法的 `thousands_sep` 会因为这个被冻结的 `__sep_pos` 而被分组校验判为不合法,从而把 `__testfail` 置位。
- 末尾的判定块**先检查 `__testfail`,再检查 `__testoverflow`**。在"溢出 + 带分组"这个重叠场景下两者同时为真,`__testfail` 优先生效,结果 `v = 0` 胜出,LWG 23 要求的 `max/min` 信号被掩盖。

也就是说:`__testfail` 在这种情况下并不是输入本身的结构错误,而是 `__testoverflow` 的下游副作用,但当前分支顺序却让这个"派生失败"压过了真正的"溢出"。

最小复现:同一个数值 `12345678901234567` 灌入 `uint32_t`:

| 场景 | 输入 | 结果(GCC 15.1.0) | 应得结果(LWG 23) |
|---|---|---|---|
| (a) classic locale,无逗号 | `"12345678901234567"` | `v = 4294967295`,failbit | `v = max`,failbit |
| (b) grouping locale,输入仍无逗号 | `"12345678901234567"` | `v = 4294967295`,failbit | `v = max`,failbit |
| (c) grouping locale,输入带逗号 | `"12,345,678,901,234,567"` | **`v = 0`,failbit ← bug** | `v = max`,failbit |

(c) 和 (a)/(b) 表达的是同一个数,目标类型相同,溢出程度相同,唯一差别只是书写方式是否使用了与 locale 分组规则一致的千位分隔符 —— 这种"是否使用分隔符"的纯排版差异不应该改变 `num_get` 的语义,但当前 libstdc++ 让它改变了。

修复建议是把判定块中两个分支的顺序交换,让 `__testoverflow` 先判:

```cpp
if (__testoverflow) { __v = ...max/min...; __err = failbit; }
else if (structural_failure || __testfail) { __v = 0; __err = failbit; }
else __v = ...;
```

这样一来,只要溢出信号触发,它就一定胜出,符合 LWG 23 的要求。唯一会发生行为变化的、"既结构错误又数值溢出"的边角输入(如 `",,99999999999"`),交换前后 `failbit` 都会被设置,只是 `v` 从 `0` 变为 `max`,而 LWG 23 并未直接仲裁该重叠区域,两种结果都合规。

### IOv2 的处理

IOv2 自身的 `numeric` facet 在数字解析末尾也有一段历史上与 libstdc++ 同源的判定逻辑,因此同样存在这个潜在缺陷。我们在向上游提交 bug 的同时,**已经在 IOv2 内部进行了修复**:

- 文件:`include/facet/numeric.h`
- 改动:交换"`testoverflow` 分支"与"`testfail` 分支"在末尾判定块中的先后顺序,使得 `testoverflow` 优先生效。
- 注释:在该分支上方加入了详细注释,说明这是一处刻意偏离 libstdc++ 顺序的修复(`We diverge from libstdc++'s ordering here ...`),并解释了 `sep_pos` 冻结所导致的 `testfail`-`testoverflow` 重叠机理,避免后续维护者把它误读为"无意中和 GCC 不一致"。
- 复现器:`repro_gcc_lwg23.cpp`(位于仓库根目录),原生 GCC 工具链上可直接编译运行,用于演示 libstdc++ 的行为以及 LWG 23 期望的行为。

因此,即使上游短期内不修复或维持 `WONTFIX`,IOv2 的用户在使用本库的 `numeric` facet 时也能得到符合 LWG 23 的结果。

---

## English

### Created at

2026-05-29 15:23 UTC. Submitted to GCC Bugzilla the same day the local reproducer and minimal test case were finalized.

### Current status

`UNCONFIRMED` — submitted, awaiting upstream triage. The `Assignee` field still reads *Not yet assigned to anyone*; `Importance` is `P3 / normal`; there are no maintainer comments yet. For up-to-date status, see <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125499>.

### Bug description

LWG defect 23 ("Num_get overflow result") requires `std::num_get` to store `numeric_limits<T>::max()` (or `::min()` for a signed negative overflow) into the destination value and set `failbit` whenever the input field overflows the target type.

libstdc++ honors LWG 23 when the input contains **no thousands separators**, but violates it when the input is a **structurally valid, grouped numeric literal under a locale whose grouping rules accept that spelling**: it stores `0` instead of `numeric_limits::max()`.

The root cause is in the post-loop resolution block of `_M_extract_int` in `libstdc++-v3/include/bits/locale_facets.tcc` (the block carrying the `_GLIBCXX_RESOLVE_LIB_DEFECTS 23` comment):

- The digit-accumulation loop short-circuits on overflow: `if (__result > __smax) __testoverflow = true; else { ...; ++__sep_pos; }`. Once `__testoverflow` fires, `++__sep_pos` is skipped for every subsequent digit, so `__sep_pos` freezes at whatever value it held at the moment overflow tripped.
- A later, otherwise structurally well-formed `thousands_sep` then fails the grouping check against the frozen `__sep_pos`, which sets `__testfail`.
- The post-loop block checks **`__testfail` before `__testoverflow`**. In the "overflow under grouping" overlap, both fire simultaneously and `__testfail` wins, so `v = 0` is returned — even though the LWG-23-mandated overflow signal is actually live.

In other words, the `__testfail` in this case is a *downstream consequence* of `__testoverflow`, not an independent structural error in the input — yet the current branch order lets that derived failure mask the overflow signal LWG 23 explicitly requires.

Minimal reproducer — the same numerical value `12345678901234567` parsed into `uint32_t`:

| Case | Input | Result (GCC 15.1.0) | Expected (LWG 23) |
|---|---|---|---|
| (a) classic locale, no commas | `"12345678901234567"` | `v = 4294967295`, failbit | `v = max`, failbit |
| (b) grouping locale, no commas in input | `"12345678901234567"` | `v = 4294967295`, failbit | `v = max`, failbit |
| (c) grouping locale, commas in input | `"12,345,678,901,234,567"` | **`v = 0`, failbit ← bug** | `v = max`, failbit |

Cases (a)/(b)/(c) represent the same number, the same target type, the same magnitude of overflow. The only difference is whether the spelling uses thousands separators that match the locale's grouping. A purely typographical difference should not change `num_get`'s semantics, yet under the current libstdc++ implementation it does.

The suggested upstream fix is to swap the two branches so `__testoverflow` is checked first:

```cpp
if (__testoverflow) { __v = ...max/min...; __err = failbit; }
else if (structural_failure || __testfail) { __v = 0; __err = failbit; }
else __v = ...;
```

Whenever the overflow signal fires it now wins, matching LWG 23. The only input class that changes behavior is the corner case "both structurally invalid AND numerically overflowing" (e.g. `",,99999999999"`); under the swap, that input yields `v = max` instead of `v = 0`. `failbit` is set either way, LWG 23 does not directly arbitrate this overlap, and the input was malformed regardless — both outcomes are conforming.

### What IOv2 does

IOv2's own `numeric` facet historically carries a resolution block parallel to libstdc++'s, so it inherits the same latent defect. We have **fixed the issue inside IOv2** at the same time as filing the upstream bug:

- **File**: `include/facet/numeric.h`.
- **Change**: in the post-loop resolution block, the `testoverflow` branch and the `testfail` branch are reordered so that `testoverflow` is checked first. Whenever the overflow signal is live, it dominates the derived `testfail` produced by the frozen `sep_pos`.
- **Comment**: a multi-line comment above the swapped branch documents this as a *deliberate* divergence from libstdc++'s ordering (`We diverge from libstdc++'s ordering here ...`), explains the `sep_pos`-freezing mechanism that makes `testfail` and `testoverflow` overlap, and prevents future maintainers from misreading the divergence as an accidental inconsistency with GCC.
- **Reproducer**: `repro_gcc_lwg23.cpp` (at the repository root) is a self-contained program that compiles on a native GCC toolchain and demonstrates both the libstdc++ behavior and the LWG-23-conforming expectation.

As a result, even if the upstream bug is not fixed in the short term — or is closed as `WONTFIX` — users of IOv2's `numeric` facet still observe LWG-23-conforming behavior on grouped, overflowing input.

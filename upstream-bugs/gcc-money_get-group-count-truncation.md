# GCC libstdc++ — `money_get` accepts malformed grouping when a single group has 256·k+g digits (inter-separator digit count truncated to `char`)

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125554>
- **Upstream project / component**: GCC / `libstdc++`
- **Bug ID**: PR 125554
- **Reported**: 2026-06-01 16:07 UTC by liwei
- **Last modified upstream**: 2026-06-01 16:07 UTC
- **Current status**: `UNCONFIRMED` — submitted, awaiting maintainer triage; `Assignee: Not yet assigned to anyone`, no maintainer comments yet.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-06-01 16:07 UTC(本地复现与最小化案例完成后,于同日提交至 GCC Bugzilla)。

### 当前状态

`UNCONFIRMED`(已提交,尚未被 GCC 维护者确认)。Assignee 字段仍为 *Not yet assigned to anyone*,目前没有任何来自上游开发者的评论。后续状态变更请以上游链接为准:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125554>。

### Bug 描述

`std::money_get<charT>::do_get`(经内部辅助函数 `money_get::_M_extract`)在解析带千位分隔符的货币串时,会把"两个相邻千位分隔符之间的数字个数"以 `char` 形式记录下来,用于事后的分组合法性校验:

```cpp
__grouping_tmp += static_cast<char>(__n);                              // 每遇到一个分隔符
...
__grouping_tmp += static_cast<char>(__testdecfound ? __last_pos : __n); // 最后一组
```

这里的计数 `__n` 是 `int`,却被截断进 `char`。于是当某一组的位数为 `256·k + g`(k ≥ 1)时,它会被记成 `g`。随后 `std::__verify_grouping` 拿这个被截断的值去和 `moneypunct::grouping()` 比较,导致**一个畸形的超长分组在位数 ≡ 合法组大小 (mod 256) 时被误判为合法**:`failbit` 不置位。

根因位于 `libstdc++-v3/include/bits/locale_facets_nonio.tcc` 的 `money_get::_M_extract` 中上述 `static_cast<char>` 处。

**性质说明(重要)**:这是纯粹的一致性/正确性缺陷,**不是内存安全问题**——被截断的计数值只在 `__verify_grouping` 里参与比较(`==` / `<=`),从不被当作下标或长度去访问缓冲区;解析出的数值数字串本身也完全正确(包含全部数字),受影响的仅仅是"分组是否合法"这个 `failbit` 判定。触发它需要"单个分组 ≥ 256 位连续数字"这种现实货币里不可能出现的输入。

最小复现:自定义一个 `moneypunct`,规定每组必须 3 位、分隔符为 `,`、无小数位。用标准 `money_get<char>` 解析三种输入:

| 场景 | 输入 | 结果(GCC 15.2.1) | 应得结果 |
|---|---|---|---|
| A) 合法分组 | `"12,345,678"` | 接受(failbit=0) | 接受 |
| B) 中间组 4 位 | `"12,3456,789"` | 拒绝(failbit=1) | 拒绝 |
| C) 中间组 259 位 | `"12,<259 个数字>,789"` | **接受(failbit=0)← bug** | 拒绝 |

B 与 C 唯一的差别只是中间组的位数(4 vs 259)。`259 = 256 + 3`,`static_cast<char>(259) == 3`,于是那一组畸形的 259 位被当成"合法的 3 位组"通过校验,而结构完全相同、只是位数为 4 的 B 却被正确拒绝 —— 两相对照即可证明截断的存在。

修复建议:当某一组的位数超过分组单元(`char`)可表示的上限(即 `> std::numeric_limits<unsigned char>::max()`)时,直接判定分组失败并置 `failbit`,而不是先截断再比较。

复现器:随上游 bug 一并提交的最小自包含程序 `money_group_bug.cpp`,在原生 GCC 工具链上 `g++ -std=c++20` 可直接编译运行,演示上述 A/B/C 三种行为。

### IOv2 的处理

IOv2 自身的 `monetary` facet 的解析逻辑与 libstdc++ 的 `_M_extract` 同源,因此同样存在这处截断。我们在向上游提交 bug 的同时,**已经在 IOv2 内部进行了修复**:

- **文件**:`include/facet/monetary.h`(`monetary::extract`)。
- **改动**:在两处把"组位数"写入 `grouping_tmp` 的地方——遇到分隔符时的中间组,以及末尾的最后一组——加入上限检查:一旦组位数超过 `std::numeric_limits<uint8_t>::max()`,直接判失败(中间组置 `testvalid = false` 并 `break`,末组置 `succ = false`),不再 `static_cast<uint8_t>` 截断。阈值刻意采用 `std::numeric_limits<uint8_t>::max()` 而非字面量 `255`,与 `grouping_tmp` 的元素类型保持一致。
- **效果**:任何"单组位数超过 `uint8_t` 上限"的输入都会被如实判为分组不合法,彻底消除截断回绕。这与给 GCC 提议的修复方向一致。

因此,即使上游短期内不修复或维持 `WONTFIX`,IOv2 用户在使用本库的 `monetary` facet 时也能得到正确的分组校验结果。

---

## English

### Created at

2026-06-01 16:07 UTC. Submitted to GCC Bugzilla the same day the local reproducer and minimal test case were finalized.

### Current status

`UNCONFIRMED` — submitted, awaiting upstream triage. The `Assignee` field still reads *Not yet assigned to anyone*; there are no maintainer comments yet. For up-to-date status, see <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125554>.

### Bug description

`std::money_get<charT>::do_get` (via the internal helper `money_get::_M_extract`) records the number of digits seen between two adjacent thousands-separators as a `char`, for the later grouping-validity check:

```cpp
__grouping_tmp += static_cast<char>(__n);                              // per separator
...
__grouping_tmp += static_cast<char>(__testdecfound ? __last_pos : __n); // last group
```

The count `__n` is an `int` but is truncated into a `char`. So a group of `256·k + g` digits (k ≥ 1) is recorded as `g`. `std::__verify_grouping` then compares that truncated value against `moneypunct::grouping()`, so **a malformed over-long group whose length is congruent to a valid group size mod 256 is accepted as valid**: `failbit` is not set.

The root cause is the `static_cast<char>` shown above, in `money_get::_M_extract` in `libstdc++-v3/include/bits/locale_facets_nonio.tcc`.

**Nature (important)**: this is a pure conformance/correctness defect, **not a memory-safety issue** — the truncated count is only ever *compared* inside `__verify_grouping` (`==` / `<=`), never used as an index or size, so there is no out-of-bounds access; the extracted digit string is also fully correct (it contains every digit). Only the grouping-validity verdict (`failbit`) is wrong. Triggering it requires a single group of ≥ 256 consecutive digits, which does not occur in real monetary input.

Minimal reproducer: a custom `moneypunct` requiring groups of exactly 3, separator `,`, no fractional digits. Parsing three inputs with the standard `money_get<char>`:

| Case | Input | Result (GCC 15.2.1) | Expected |
|---|---|---|---|
| A) valid grouping | `"12,345,678"` | accepted (failbit=0) | accepted |
| B) middle group = 4 | `"12,3456,789"` | rejected (failbit=1) | rejected |
| C) middle group = 259 | `"12,<259 digits>,789"` | **accepted (failbit=0) ← bug** | rejected |

The only difference between B and C is the length of the middle group (4 vs 259). `259 = 256 + 3`, `static_cast<char>(259) == 3`, so the malformed 259-digit group passes as a valid group of 3, whereas the structurally identical B with a 4-digit group is correctly rejected. The contrast demonstrates the truncation.

Suggested upstream fix: when a group's digit count exceeds the maximum value representable in the grouping `char` (i.e. `> std::numeric_limits<unsigned char>::max()`), fail the grouping check (set `failbit`) instead of truncating the count before appending it.

Reproducer: the minimal self-contained program `money_group_bug.cpp` filed with the upstream bug; it compiles and runs on a native GCC toolchain with `g++ -std=c++20` and demonstrates the A/B/C behavior above.

### What IOv2 does

IOv2's own `monetary` facet shares the same extraction logic as libstdc++'s `_M_extract`, so it inherits the same truncation. We have **fixed the issue inside IOv2** at the same time as filing the upstream bug:

- **File**: `include/facet/monetary.h` (`monetary::extract`).
- **Change**: at both sites where a group's digit count is appended to `grouping_tmp` — the middle groups (on hitting a separator) and the final group — an upper-bound check is added: once the count exceeds `std::numeric_limits<uint8_t>::max()`, the grouping is failed outright (middle group sets `testvalid = false` and `break`s; the final group sets `succ = false`) instead of being truncated via `static_cast<uint8_t>`. The threshold deliberately uses `std::numeric_limits<uint8_t>::max()` rather than the literal `255`, matching the element type of `grouping_tmp`.
- **Effect**: any input whose single group exceeds the `uint8_t` limit is faithfully reported as malformed grouping, eliminating the wrap-around. This matches the fix direction proposed to GCC.

As a result, even if the upstream bug is not fixed in the short term — or is closed as `WONTFIX` — users of IOv2's `monetary` facet still observe correct grouping verification.

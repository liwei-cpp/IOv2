# GCC libstdc++ — `time_zone::get_info` throws for an instant before `year::min()` when the zone's first period follows a rule set

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=127815>
- **Upstream project / component**: GCC / `libstdc++`
- **Bug ID**: PR 127815
- **Reported**: 2026-10-10 18:33 UTC by liwei (Version: 15.3.0)
- **Last modified upstream**: 2026-10-10 18:33 UTC
- **Current status**: `UNCONFIRMED` — submitted, awaiting maintainer triage; `Assignee: Not yet assigned to anyone`, no maintainer comments yet.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-10-10 18:33 UTC。同日在本机查明根因、写好不依赖系统 tzdata 的最小复现器后提交到 GCC Bugzilla。

### 当前状态

`UNCONFIRMED`(已提交,尚未被 GCC 维护者确认)。Assignee 字段为 *Not yet assigned to anyone*,目前没有上游开发者的评论。后续状态变更请以上游链接为准:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=127815>。

### 发现经过

本机的 `concur`(`FacetStaticsExit.ProcessWideFacetDataSurvivesExit`)、`facet.timeio`(25 例)与 `io.istream.extractors_time`(2 例)全部以 `timeio parse error` 失败,CI 却一直通过。差别在 tzdata:本机 2026-10-09 升到 2026e,CI 镜像(Debian 13)是 2026b。

tzdata 2026d 起,`CST6CDT`、`EST5EDT`、`MST7MDT`、`PST8PDT` 不再是指向 `America/Chicago` 等的 link(2026b:`L America/Chicago CST6CDT`),而是自时间起点就引用 `US` 规则集的单行 Zone(2026e:`Z CST6CDT -6 US C%sT`)。它们是 2026e 里仅有的「第一段就带规则」的区;其余区的第一段都是规则列为 `-` 的 LMT。

### Bug 描述

`std::chrono::time_zone::get_info(sys_time)` 的实现 `time_zone::_M_get_sys_info` 把每个区第一段的起点定为 `year::min()` 年 1 月 1 日。查询时刻 `tp` 早于这个起点、而第一段又引用规则集时,它要现场展开规则,并按 `tp` 与段起点相差的年数预留空间。实现可在
[`tzdb.cc`(master,commit `080bfd8da2e572dab240cce4a685b342fa7c070d`)](https://github.com/gcc-mirror/gcc/blob/080bfd8da2e572dab240cce4a685b342fa7c070d/libstdc%2B%2B-v3/src/c%2B%2B20/tzdb.cc#L1088-L1110)
与 [releases/gcc-15(commit `a3b110420ad422270974f2d42a019c88d389669e`)](https://github.com/gcc-mirror/gcc/blob/a3b110420ad422270974f2d42a019c88d389669e/libstdc%2B%2B-v3/src/c%2B%2B20/tzdb.cc#L816-L838)
核验;releases/gcc-16 分支同样如此。关键数据流用项目自拟的伪代码(并非 GCC 源码)概括如下:

```text
段起点 = year::min() 年 1 月 1 日                 (第一段)
n = 把 (tp - 段起点) 向上取整成年数,存进 int      (int64 被截成 int)
若 n 非零:预留 min(100, 2n) 个元素               (n 为负时,负数转成 size_t)
展开规则,直到找到覆盖 tp 的那一段                (tp 早于段起点时永远找不到)
找不到 -> 抛 runtime_error("time_zone::get_info")
```

由此有两种失败:

| `tp` | `n` | 结果 |
|---|---|---|
| 早于段起点一年以上(如 `sys_seconds::min()`、`min() + 1s`) | 负数(`min()` 时为 -292276991853,截成 int 为 -219215725) | `reserve` 收到巨大的 `size_t`,抛 `length_error("vector::reserve")` |
| 段起点前一年以内(如 `year::min()/January/1 - 1s`) | 0 | 跳过 `reserve`,展开后找不到覆盖 `tp` 的段,抛 `runtime_error("time_zone::get_info")` |

第一段不带规则(`-`)时,`get_info` 直接返回该段的固定偏移与缩写,同样的 `tp` 不出错(`America/Chicago` 在 `min()` 上返回 `LMT`)。标准没有给 `get_info` 设定义域下限,不带规则的区也能回答,带规则的区却抛异常——这是行为不一致。

**探针陷阱**:`get_info` 会把展开结果缓存在 zone 对象里。同一进程中先查晚的时刻、再查 `min() + 1s`,会命中缓存而「不抛」。复现时要先查会抛的时刻,或每个时刻用新进程查。

### 最小复现

不依赖系统 tzdata:经 libstdc++ 的 `__gnu_cxx::zoneinfo_dir_override` 钩子读一份只有两条规则、两个区的 `tzdata.zi`。复现器为本项目自写,随上游 bug 一并提交,全文见英文部分。GCC 15.3.0(Debian 13,`gcc:15` 镜像)与 GCC 15.3.1(Fedora 43)上结果相同:

```text
Test: threw vector::reserve        // 带规则,tp = sys_seconds::min()
Test: threw time_zone::get_info    // 带规则,tp = year::min()/January/1 - 1s
Plain: ok, abbrev=CST              // 不带规则,同样两个时刻
Plain: ok, abbrev=CST
```

### IOv2 的处理

`include/IOv2/facet/timeio_details.h` 建时区前缀树时原本从 `sys_seconds::min()` 起逐段遍历每个区,又把整个遍历包在一个 `catch (...)` 里:四个区一抛,整棵树只剩 `UNKNOWN`,所有 `%Z` 解析失败。IOv2 已在内部规避:

- 遍历起点改为 `sys_days{year::min() / January / 1}`,即 libstdc++ 自己的段起点;此前不会有任何转换,不漏缩写。本机 tzdata 2026e 下建出 769 个键,与 CI 的 2026b 相同。已合入 main(PR #497)。
- 每个区的遍历单独 `catch`:某个区出错只少收它剩下的缩写,区名与其余区照收。见 PR #498。

---

## English

### Creation time

2026-10-10 18:33 UTC, submitted to GCC Bugzilla the same day the root cause was found locally and a reproducer independent of the system tzdata was written.

### Current status

`UNCONFIRMED` (submitted, not yet confirmed by a GCC maintainer). The Assignee field reads *Not yet assigned to anyone*, and there are no comments from upstream developers yet. For later status changes, see <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=127815>.

### How it was found

Locally, `concur` (`FacetStaticsExit.ProcessWideFacetDataSurvivesExit`), `facet.timeio` (25 cases) and `io.istream.extractors_time` (2 cases) all failed with `timeio parse error`, while CI kept passing. The difference was tzdata: 2026e here since 2026-10-09, 2026b in the CI image (Debian 13).

Since tzdata 2026d, `CST6CDT`, `EST5EDT`, `MST7MDT` and `PST8PDT` are no longer links to `America/Chicago` and co. (2026b: `L America/Chicago CST6CDT`) but single-line Zones that follow the `US` rules from the beginning of time (2026e: `Z CST6CDT -6 US C%sT`). They are the only zones in 2026e whose first period has rules; every other zone starts with an LMT period whose rules column is `-`.

### Description

`time_zone::_M_get_sys_info`, which implements `std::chrono::time_zone::get_info(sys_time)`, starts every zone's first period on January 1 of `year::min()`. When the queried instant `tp` is earlier and that period follows a rule set, it expands the rules on the spot and reserves space by the number of years between the period's start and `tp`. The implementation can be checked in
[`tzdb.cc` (master, commit `080bfd8da2e572dab240cce4a685b342fa7c070d`)](https://github.com/gcc-mirror/gcc/blob/080bfd8da2e572dab240cce4a685b342fa7c070d/libstdc%2B%2B-v3/src/c%2B%2B20/tzdb.cc#L1088-L1110)
and [releases/gcc-15 (commit `a3b110420ad422270974f2d42a019c88d389669e`)](https://github.com/gcc-mirror/gcc/blob/a3b110420ad422270974f2d42a019c88d389669e/libstdc%2B%2B-v3/src/c%2B%2B20/tzdb.cc#L816-L838);
the releases/gcc-16 branch is the same. The data flow, in project-written pseudocode (not GCC source):

```text
period start = January 1 of year::min()            (first period)
n = (tp - period start) rounded up to years, in an int   (int64 truncated)
if n != 0: reserve min(100, 2n) elements           (negative n becomes a huge size_t)
expand the rules until a period containing tp is found   (never, if tp is earlier)
none found -> throw runtime_error("time_zone::get_info")
```

Hence two failures:

| `tp` | `n` | Result |
|---|---|---|
| More than a year before the period start (`sys_seconds::min()`, `min() + 1s`) | negative (-292276991853 for `min()`, -219215725 as an int) | `reserve` gets a huge `size_t` and throws `length_error("vector::reserve")` |
| Within a year before it (`year::min()/January/1 - 1s`) | 0 | `reserve` is skipped; no expanded period contains `tp`, so it throws `runtime_error("time_zone::get_info")` |

A first period without rules (`-`) is answered directly with its fixed offset and abbreviation, so the same `tp` works there (`America/Chicago` returns `LMT` at `min()`). The standard puts no lower bound on `get_info`; zones without rules answer such queries while rule-based ones throw, which is inconsistent.

**Probe pitfall**: `get_info` caches the expanded periods in the zone. Querying a later instant first and then `min() + 1s` in the same process hits the cache and appears to succeed. Query the failing instants first, or each one in a fresh process.

### Minimal reproducer

Independent of the system tzdata: it reads a `tzdata.zi` with two rules and two zones through libstdc++'s `__gnu_cxx::zoneinfo_dir_override` hook. Written for this report and submitted with it:

```cpp
// g++ -std=c++20 repro.cpp && ./a.out && ./a.out plain
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace __gnu_cxx
{
  // libstdc++ hook: read tzdata.zi from this directory instead of the system's.
  const char* zoneinfo_dir_override() { return "/tmp/pr-tzdb-repro"; }
}

int main(int argc, char**)
{
  std::filesystem::create_directories("/tmp/pr-tzdb-repro");
  std::ofstream("/tmp/pr-tzdb-repro/tzdata.zi")
    << "# version test\n"
       "R X 2000 ma - Mar lastSu 2 1 D\n"
       "R X 2000 ma - O lastSu 2 0 S\n"
       "Z Test -6 X C%sT\n"
       "Z Plain -6 - CST\n";

  using namespace std::chrono;
  // One zone per run, so no earlier call has expanded (and cached) it.
  const char* name = argc > 1 ? "Plain" : "Test";
  const sys_seconds ymin{sys_days{year::min() / January / 1}};
  for (sys_seconds t : {sys_seconds::min(), ymin - 1s})
    try
      {
        sys_info i = locate_zone(name)->get_info(t);
        std::printf("%s: ok, abbrev=%s\n", name, i.abbrev.c_str());
      }
    catch (const std::exception& e)
      {
        std::printf("%s: threw %s\n", name, e.what());
      }
}
```

Output with GCC 15.3.0 (Debian 13, `gcc:15` image) and GCC 15.3.1 (Fedora 43):

```text
Test: threw vector::reserve
Test: threw time_zone::get_info
Plain: ok, abbrev=CST
Plain: ok, abbrev=CST
```

### What IOv2 does

`include/IOv2/facet/timeio_details.h` walked every zone from `sys_seconds::min()` to build its time-zone trie, inside a single `catch (...)`: once the four zones threw, the trie held nothing but `UNKNOWN` and every `%Z` parse failed. IOv2 works around it:

- The walk starts at `sys_days{year::min() / January / 1}`, libstdc++'s own period start; no transition comes earlier, so no abbreviation is missed. Under tzdata 2026e it builds 769 keys, the same as under CI's 2026b. Merged to main (PR #497).
- Each zone's walk has its own `catch`: a zone that throws loses only its remaining abbreviations, while its name and every other zone still go in. See PR #498.

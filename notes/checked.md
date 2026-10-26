# 已核查清单

## 这份文件是做什么的

记录**已经审查过、结论为"当前代码在这一点上是正确的"**的检查项，供后续审查跳过，
避免重复劳动。

如果你是新接手审查的人：下面列出的每一条都已经用读码或探针实测确认过，**不必再复查，
也不要把它们当作新问题报告**。最后一节列的是本清单**没有**覆盖、仍值得看的范围。

行号是写作时的行号（第 1-163 条为 2026-08-08，第 164-198 条为 2026-08-10，
第 199-244 条为 2026-08-11，第 245-267 条为 2026-08-12，第 268-287 条为 2026-08-14，
第 288 条起为 2026-08-16，第 425 条起为 2026-09-05），文件改动后会漂移，
定位请以函数名或代码片段为准。

条目里出现的 `.review/...` 路径是当时探针源码的文件名，**这些文件不再保留**；
它们只用来说明结论是怎么得到的，不是可以打开的文件。

## 审查范围与方法

**被系统性审查过的文件**

- `include/io/io_manip.h`
- `include/io/istream.h`
- `include/io/ostream.h`
- `include/io/iostream.h`
- `include/IOv2/io/traits/` 五个文件：`traits_base.h`、`char_and_str.h`、`arithmetic.h`、
  `nullptr.h`、`tm.h`（第 425 条起）

其它文件（`io_base.h`、`streambuf.h`、`facet/timeio.h`、`facet/numeric.h`、`root_cvt.h`、
`utilities/*.h` 等）只在"验证上述文件的行为或契约"时被读到，**未被系统性审查**——
即便其中个别结论已被确认（如第 464-469 条之于 `facet/timeio.h`）。

**关注的问题类型**：内存安全（泄漏、越界、悬空指针）、未定义行为、边界条件错误、
明显逻辑 bug；外加一轮针对"未覆盖的边界条件 / 隐含假设 / 竞态与状态错误 / 头文件顺序与
完整性 / 文件卫生 / 拼写"的补充检查。风格与结构类意见不在范围内。

第 288 条起是对同样这四个文件的**第二轮**审查（2026-08-14 起），重点是未覆盖的边界条件、
隐含假设（契约没写、代码也没查）、潜在竞态与状态错误，外加文件卫生与拼写；机械可判定项
由主控全量核对，需要判断的三类分派四个 agent 并行、范围互不重叠，所有结论主控均已复跑
复核。这一轮从这四个文件出发追到了 `facet/numeric.h`、`facet/monetary.h`、`facet/timeio.h`、
`cvt/abs_cvt.h`、`cvt/code_cvt.h`、`io/streambuf.h` 等文件里的具体结论（第 306-317 条），
但那些文件**仍未被系统性审查**，只是这几个结论已经确认。

**方法**：先读每个类与函数的双语 Doxygen 注释以确定**契约**，再逐条用代码验证；
能构造出触发路径的疑点都写了独立探针程序实测。凡是注释已明确写清的前置条件
（工厂返回的临时对象持有引用/裸指针、`put_time` 要求 `std::tm` 整体自洽、构造函数可抛
`device_error` 等），不计为问题；只有**代码没有兑现自己写下的契约**时才计。

**实测环境**：clang++ 21 / g++ 15 / libstdc++ 15 / glibc，`-std=c++23`，
`-fsanitize=address,undefined`；并发部分用 clang 的 TSan（gcc 的 libtsan 在本机缺失）。
容器内的实验分两套：`ghcr.io/liwei-cpp/iov2-ci-gcc15` 是 amd64，在 arm64 机器上经 qemu
运行，此组合下 ASan 的 32 位分配器自检失败（`sanitizer_allocator_primary32.h`），
**与被测代码无关**——所以该镜像上跑的是 UBSan / 无 sanitizer 构建，ASan 结论来自原生的
`localhost/iov2-ci-arm64`。另注：macOS `tar` 生成的 AppleDouble `._*` 文件会被 Makefile 的
`*.cpp` 通配当成源码，打包时需 `COPYFILE_DISABLE=1 --exclude='._*'`。

---

## `io_manip.h`：格式化状态操纵符

1. **`setw(-1)`、`setw(total - str.size())` 的无符号回绕已被挡住。**
   `setw_t` 存 `std::ptrdiff_t`，`ios_base::width(std::ptrdiff_t)`（`io/io_base.h:474`）
   自己判负并抛 `stream_error`，运算符转成 `strfailbit`。实测 `os << setw(-1)` →
   `state=0x08`。形参取有符号类型正是为了让传参时的窄化把无符号回绕抵消回来。

2. **`setprecision` 的越界值不会被静默截断。**
   值域检查在 `ios_base::precision(std::ptrdiff_t)`（`io/io_base.h:426`）里，越界即抛
   `stream_error`，运算符转成 `strfailbit`；`io_manip.h` 的 `apply` 只是一行转调。
   形参取有符号的 `std::ptrdiff_t`，中间无窄化，符号一路保留到判断处。
   实测 `-1` / `256` / `300` / `PTRDIFF_MAX` / `PTRDIFF_MIN` / `(ptrdiff_t)SIZE_MAX`
   全部 `strfailbit`，且 precision 保持 6——抛出发生在 `exchange` 之前，不留半改状态。

3. **`setbase` 传入 8/10/16 以外的值不是错误。**
   清空 `basefield` 后按十进制输出，与 `std::setbase` 一致，注释已写明。
   实测 `setbase(7)` 输出 `255`。形参与 `m_base` 都是 `std::ptrdiff_t`（详见第 318 条），
   故宽于 `int` 的实参不会先被截断再撞上 8/10/16。

4. **六个"流形式"操纵符不加锁是正确的。**
   逐条核对 `io/io_base.h`：`setf(f, msk)` 是 CAS 循环、`setf(f)` 是 `fetch_or`、
   `fill` / `precision` / `width` 都是 `exchange`，全部是对单个原子成员的一次读-改-写，
   符合文件头 `@warning`（`io_manip.h:17-32`）给自己划的界限。

5. **`setw` 只能收紧提取端的边界，不会造成越界写。**
   `io/traits/char_and_str.h` 的 `istream_extract` 里是
   `const size_t width = io.width(); if (0 < width && width < num) num = width;`——
   注意 `width == 0` 被当作"无上限"，与标准一致。随后 `num <= 1` 时先写终止符、清 width
   再抛；循环条件是 `extracted < num - 1`；循环后无条件写终止符。实测
   `is >> setw(10) >> char[4]` 得 `"hel"`；`is >> setw(1) >> char[4]` 得空串 +
   `strfailbit`；`is >> setw(0) >> char[8]` 得 `"hello"`，与完全不写 `setw` 相同。
   ASan 无越界。

6. **`width` 的消费规则与文档一致。**
   实测：`setw` 之后接 `int` / `get_money` / `get_time` 提取，`width` 保持不变；
   接 `std::basic_string` 或字符数组提取才清零；插入侧 `put_money` 消费 `width`，
   而 `put_time` 既不消费也不应用（`os << setw(20) << put_time(...)` 不补齐到 20 列，
   width 原样留给下一次插入）。

7. **格式化状态操纵符的端到端效果与标准逐项吻合。**
   实测 `setw(8) << 42` → `"      42"`；`setfill('*')` → `"******42"`；
   `setiosflags(left)` → `"42      "`；`setbase(16) << 255` → `"ff"`；
   加 `showbase` → `"0xff"`；`setprecision(3) << 3.14159` → `"3.14"`；
   `setprecision(0)` → `"3"`；`setw(8) << "ab"` → `"      ab"`；
   同时置上 `oct` 与 `dec` 后输出 `"255"`；`setbase(16)` 后
   `resetiosflags(basefield)` 输出 `"255"`。每次插入后 `width` 都归零。

8. **`setw` / `setprecision` 抛出时的置位不在锁外。**
   这两条失败路径经 `operator<<` 的 `catch` 走 `handle_exception`，而
   `handle_exception`（`io/io_base.h:1074`）与 `setstate`（`io/io_base.h:899`）
   各自都取 `io_mutex()`，写入本身是同步的。（`endl` / `ws` 之所以要自己持锁再 `catch`，
   是为了让置位与**同一次操作**的其余步骤落在一把锁内，与这里的单点置位不是一回事。）

9. **所有载体类型的构造函数形态一致。**
   `resetiosflags_t` / `setiosflags_t` / `setbase_t` / `setfill_t` / `setprecision_t` /
   `setw_t` 六个都有 `explicit` 构造函数；`put_money_t` / `get_money_t` / `put_time_t` /
   `get_time_t` 四个是聚合体，由工厂用聚合初始化产生。

## `io_manip.h`：货币与时间操纵符

10. **`put_time` / `get_time` 的空指针在任何解引用之前就被拦下。**
    两处 `nullptr` 检查（`io_manip.h:822`、`991`）都位于解引用与
    `basic_string_view` 转换**之前**。实测四种空指针组合全部干净地得到 `strfailbit`，
    ASan/UBSan 无告警。

11. **`get_time` 解析失败时不会改坏目标 `std::tm`。**
    `timeio::get` 失败是抛异常（`if (!succ) throw stream_error("timeio parse error")`），
    `operator year_month_day()` 对不存在的日期也抛，两者都发生在回写之前。
    实测用 `"%H:%M %Z"` 读 `"01:02 UTC"` 得到 `strfailbit`，目标与调用前逐字节相同。

12. **两条提取路径都只写真正重建出来的九个字段，不碰 `std::tm` 的实现定义扩展成员。**
    解析上下文的转换接口是 `void convert_to(T&)`（**填进调用方给的对象**，而不是构造一个
    新值返回），`facet/timeio.h` 里的 `convert_to(std::tm&)` 只覆盖
    `tm_year / tm_mon / tm_mday / tm_hour / tm_min / tm_sec / tm_wday / tm_yday / tm_isdst`。
    `io_manip.h` 的 `get_time` 与通用提取运算符（`utilities/istream_operators.h`，
    连同 `extractable_with_ctx` 概念）都调它，所以两条路径共用同一份逻辑。
    实测（`TZ=America/Los_Angeles`，时间戳 1700000000）：

    ```
    is >> get_time(&t, "%H:%M")   gmtoff=-28800  zone=PST
    is >> t   (成功解析 %c)        gmtoff=-28800  zone=PST
    ```

    格式串填满六字段的 `"%Y-%m-%d %H:%M:%S"` 同样保留。这与 `std::get_time` 的行为一致
    （实测 std 也从不触碰这两个成员）。

    附带确认的两点，**不要当作问题报告**：
    - `time_parse_context` 用 `using` 把三个字段组 helper 的 `convert_to` 并进自己的重载集，
      三个 helper 的 `<false>` 主模板里各有一个零参且已删除的 `convert_to` 占位。这不是
      冗余代码：同名成员分散在多个基类会在名字查找阶段歧义，而派生类自己的 `convert_to`
      又会隐藏基类版本，`using` 与占位缺一不可（三种情形都单独写程序验证过）。
    - `convert_to(hh_mm_ss<TDuration>&)` 是受约束的函数模板，约束为
      `std::convertible_to<std::chrono::seconds, TDuration>`。`milliseconds` 可用
      （精确加宽），`minutes` 编译不过（会静默丢秒）——这是刻意的。

    附带确认（**不是缺陷，不要报告**）：此时 `strftime("%Z %z")` 仍只输出 `"PST "`，
    `%z` 为空。这是 glibc 的行为——`tm_isdst < 0` 时它不输出 `%z`——已用纯 C 程序单独
    验证（`isdst=0 -> "PST -0800"`，`isdst=-1 -> "PST "`）。而 `tm_isdst = -1` 是
    `get_time` 文档明列的三条例外之一，属既定语义。

13. **`put_time` 的 `%Z` / `%z` 退化与文档一致。**
    实测输出字面量 `%Z` / `%z`，流保持 `good()`。

14. **`put_time` 拒绝不自洽的 `std::tm`。**
    实测 `std::tm t{}` 配 `"%Y"`、以及 `tm_sec == 60`（闰秒）都得到 `strfailbit`
    且不输出任何内容，与文档一致。

15. **`get_time` 接受 `std::tm t{}`。**
    实测得到 1899-12-31（`tm_year = -1, tm_mon = 11, tm_mday = 31`），
    与 `std::get_time` + `mktime()` 的结果相同。提取侧比插入侧宽松是刻意的。

16. **`tm_wday` / `tm_yday` 由最终日期重算。**
    实测传入错误的 `tm_wday=3, tm_yday=99`，解析 `"%H:%M"` 后被纠正为 `2` / `317`。

17. **货币的整型目标过窄时不会静默截断。**
    实测 `get_money` 读 `"99999999999"` 到 `int` / `short` / `char` 都得到 `strfailbit`，
    且目标**完全不被改写**（保持调用前的值）；到 `long long` 正常解析。
    `put_money('A')` 虽被 `std::integral` 接纳（字符类型也是整型），但只是把 65 当作
    最小货币单位输出 `.65`，无越界。`put_money` 传入非法数字串（`"abc"` / `""` / `"-"`，
    以及含嵌入 `\0` 的 `std::basic_string`）**静默输出空串、流保持 `good()`**——数字扫描在
    第一个非数字字符处停止，因此 `"12\0" "34"` 输出 `.12`。这与 `std::put_money` 一致
    （`ostreambuf_iterator::failed()` 为假），不置失败位，也不越界。

18. **空格式串与空输入的行为正常。**
    实测 `put_time(&t, "")` 输出空串且 `good()`；`get_time(&t, "")` 不消费任何字符、
    字段保持回退值、`good()`；`get_money` 与 `get_time` 读空流都得到 `strfailbit`
    （哨兵先抛 "EOF with no value extracted"）。

19. **`setw` 与 `get_time` 组合正常。**
    实测 `is >> setw(3) >> get_time(&t, "%H:%M")` 正确解析出 1:02，
    且 `width` 保持 3 未被消费。

20. **`put_money` / `get_money` 的 `requires` 子句里 `remove_cv_t` 用得不对称是对的。**
    与 `io_manip.h:597` 的注释一致：`put` 的整型重载按值取参故 cv 被丢弃（只需在排除
    `bool` 时用 `remove_cv_t`），而其 string 重载取 `const&`，`volatile` 的 string
    绑不上，所以那一支不加 `remove_cv_t` 是正确的。

21. **`put_time` / `get_time` 是函数模板而非函数对象，不会有 ADL 二义。**
    `put_money` 之所以要做成函数对象，是因为 `std::basic_string` 的关联命名空间确实是
    `std`；而 `put_time` 的实参是 `const std::tm*` 与 `const TChar*`，实现上 `tm` 是全局
    命名空间的 `::tm`（`<ctime>` 只是 `using ::tm;`），关联命名空间是全局而非 `std`。
    实测：显式 `#include <iomanip>` 且 `using namespace IOv2;` 之后，
    `put_time(&t, "%H")` 仍稳定选中 `IOv2::put_time_t<char>`，无二义。
    同理，那五个非模板普通工厂（`resetiosflags` / `setiosflags` / `setbase` /
    `setprecision` / `setw`）的实参类型是 `ios_defs::fmtflags` / `size_t` / `int` /
    `ptrdiff_t`，关联命名空间里没有 `std`，也拖不进 `std::setw` 之流。

22. **工厂返回的临时对象持有引用或裸指针，是文件头 `@warning`（`io_manip.h:28-32`）
    明确写下的契约**，与 `std::put_money` 等同，不计为问题。

---

## `istream.h` / `ostream.h` / `iostream.h`：类型与构造

23. **五个构造函数都带 `char_type` 一致性约束，错误组合在声明层面就被拒。**
    三个类各有 5 处 `requires (std::is_same_v<...>)`：默认构造与单设备构造用
    `typename TDevice::char_type`，另外三个（设备+creator、设备+locale、
    设备+creator+locale）用对 `istreambuf` / `ostreambuf` / `streambuf` 做 CTAD 探测
    得到的管线 `char_type`。实测
    `istream<mem_device<char>, wchar_t>{mem_device<char>{}}`、
    `ostream<mem_device<char>, wchar_t>{}`、
    `iostream<std_device<1>, wchar_t>{std_device<1>{}}`、以及
    `ostream<mem_device<char>, wchar_t>` 配 `code_cvt_creator<char, char32_t>`
    四种组合，都得到 `no matching constructor for initialization of 'IOv2::xxx<...>'`
    加 `constraints not satisfied`，不再落进 `cvt/runtime_cvt.h` 内部。

24. **约束是必要且充分的：不带 creator 时转换管线不改变字符类型。**
    这一点单独验证过——`ostream<std_device<1>, wchar_t> os;`（char 设备 + 宽流 +
    无 creator）本来就无法实例化。另外，全局对象 `wcout`（`std_device<1>` + `wchar_t`）
    走的是 `io/objects/out_impl.h` 里独立的 `stdout_api` CRTP 基类，**不经过** `ostream`，
    不受这条约束影响。

25. **CTAD 与匹配的显式模板实参都照常工作。**
    实测：`istream a(mem_device<char>("hi"))`、`ostream b(mem_device<char>{})`、
    `iostream c(mem_device<char>{})`，以及三个类各自带 `locale<char>{}` 的重载，
    都推导出 `char_type == char`（取 locale 的两个重载靠自己的隐式推导指引推出 `TChar`，
    注释里的说法属实）。显式写 `ostream<mem_device<char>, char>` 亦正常。

26. **会改变字符类型的 creator 路径不受影响。**
    实测 `code_cvt_creator<char, char32_t>` 配 `mem_device<char>`，
    `ostream` / `istream` / `iostream` 三个类都正确推导出 `char_type == char32_t`；
    creator + locale 的显式形式也正常。

27. **哨兵的模板实参四处全对。**
    `istream` 用 `in_sentry<..., false>`、`ostream` 用 `out_sentry<..., false>`、
    `iostream` 用 `in_sentry<..., true>` 与 `out_sentry<..., true>`，与该参数
    "本流是否同时支持另一方向"的语义相符。

28. **基类没有重复继承，也没有成员歧义。**
    `istream_operators` 与 `ostream_operators`（`utilities/*_operators.h`）都不从
    `ios_state` 派生，所以 `iostream` 只继承 `ios_state<TChar>` 一次。两个 mixin 的成员
    也不重名：`get` / `peek` / `read` / `ignore` / `putback` / `i_iter` 对
    `put` / `write` / `flush` / `o_iter`；而 `tell` / `seek` / `rseek` / `device` /
    `detach` / `attach` / `locale` / `tie` 只在 `stream_common_operators` 里出现一次。
    三个类的基类列表与其私有拷贝构造函数的 mem-initializer 顺序完全一致，无 `-Wreorder`
    隐患。

## `istream.h` / `ostream.h` / `iostream.h`：拷贝、移动、赋值

29. **拷贝构造 `X(std::lock_guard{other.io_mutex()}, other)` 里临时锁的寿命是够的。**
    mem-initializer 本身就是一个 full-expression，其中的临时量在被委托的构造函数返回
    之后才析构，源流的锁覆盖了整个拷贝过程（被委托的那个私有构造函数函数体是空的）。
    `io_mutex() const` 返回 `mutable` 成员的非常量引用是标准的 lockable 惯用法；
    该锁是 `std::recursive_mutex`，sentry 与 `put()` 的重入没有问题。

30. **移动赋值的 `noexcept` 名副其实**（逐个编译验证），
    **拷贝赋值里的 `static_assert(is_nothrow_move_assignable_v<...>)` 不是自证**——
    它校验整个类（含所有基类与成员）的移动赋值确实 `noexcept`，一旦某个成员退化就报错。

31. **拷贝赋值的强异常保证成立。**
    唯一可抛的一步是拷贝进临时对象，此时目标尚未被触碰；提交步骤是全程 `noexcept`
    的移动赋值。

32. **不存在"持锁期间互斥量被赋值或销毁"。**
    `copyable_mutex`（`common/copyable_mutex.h`）的拷贝/移动赋值都是 no-op，
    不搬运锁状态，底层互斥量对象本身不被替换。

33. **`out_flusher<T>` 的 CRTP 状态不会在拷贝/移动后指错对象。**
    `out_flusher` **没有任何数据成员**，`try_flush()`
    （`utilities/ostream_operators.h:402`）用 `static_cast<T&>(*this)` 现场取回具体类型。

34. **tie 边不会被拷贝或移动带走。**
    `stream_common_operators` 的四个特殊成员都把目标的 tie 目标置为 `nullptr`，
    移动还额外清空源。注释给出了理由：tie 边绑定于对象身份而非值；且若被搬运就绕过了
    `tie()` 的环检测（`check_tie`，`utilities/stream_common_operators.h:962`），
    那是"tie 图无环"这一不变式的唯一把关处。
    **自赋值是例外**：`istream` / `ostream` / `iostream` 的拷贝赋值与移动赋值都在更外层以
    `if (this == &other) return *this;` 短路，根本到不了这个 mixin 的 `operator=`，
    因此 `a = a` 与 `a = std::move(a)` 之后 `a.tie()` 保持原值。见第 195 条。

35. **拷贝语义正确。**
    实测：`istream` 上先 `peek()` 出 `'h'` 再整体拷贝，随后原件与副本各自提取，
    都得到 `"hello"`，互不影响。

36. **移后（moved-from）流仍可安全使用，错误照常转成状态位。**
    与 `stream_common_operators` 注释里"其余操作不受影响"的承诺一致。实测
    `auto b = std::move(a);` 之后，`a << "x"` → `devfailbit`、`a.flush()`、`a << endl`、
    `a >> s` → `strfailbit`、`a >> ws` 全部不崩溃，ASan/UBSan 干净。移后立刻查
    `a.rdstate()` 仍为 0（状态位按值保留、源仍持有），第一次真正的 I/O 才置位。
    注意注释另有约定：移后窗口内**不得**调用 `device()` 与 `detach()`——那两个以
    "持有设备"为前置条件，属调用方责任。

## `istream.h` / `ostream.h` / `iostream.h`：加锁、状态与异常

37. **`ws` 与 `endl` 的加锁与 `catch` 纪律正确。**
    两者都自己取 `io_mutex()` 并在**同一把锁内** `catch` 后交给 `handle_exception`；
    运算符那层的 `catch` 在锁之外，只靠它的话置位会发生在解锁之后。掩码命中时异常仍会
    逃到运算符那层再处理一次，`handle_exception` 是幂等的，无害。

38. **`ends` 与 `flush` 不加锁也不 `try` 是正确的。**
    `ostream_operators::put()` 与 `flush()` 自己持有 `io_mutex()`，
    并已把异常交给 `handle_exception`。

39. **`is >> ws` 在全空白或已 EOF 的输入上只置 `eofbit`。**
    实测读 `"   \t\n  "` 后 `state=0x1`；在已 EOF 的流上再 `>> ws` 仍是 `0x1`，
    不会额外置失败位。

40. **`endl` 里 `os.locale().template get<ctype<TChar>>()` 不会悬垂。**
    `locale()` 的 getter 返回对成员的引用，`get<>()` 返回的是**拥有所有权**的
    `shared_ptr`；且整个操纵符在同一把 `io_mutex()` 之下完成，读 locale 与随后的
    `put()` 之间没有窗口。

41. **`iostream::switch_to_put()` / `switch_to_get()`（`iostream.h:343` / `373`）三点齐全**：
    加锁、失败态早返回、`catch` 后交 `handle_exception`。失败态下静默返回是刻意的，
    与 `tell()` / `flush()` 的做法一致，注释已写明。

42. **异常掩码打开时锁被干净释放，不会死锁。**
    实测：`os.exceptions(strfailbit)` 后 `os << setw(-1)` 抛出、状态为 `0x8`，
    随后 `clear()` 并关掉掩码，`os << "still usable"` 正常写出；提取侧同理
    （空流上提取 `std::string` 抛出、状态 `0x9`，`clear()` 后恢复 `0x0`），
    后续操作均不阻塞。

43. **tie 引发的隐藏锁边不会成为等待边。**
    `out_flusher::try_flush()` 用 `std::try_to_lock` 加至多三次
    `std::this_thread::yield()` 重试，取不到锁就跳过。这是既定的 best-effort 语义，
    不是缺陷。

44. **TSan 实测无竞争。**
    4 线程并发对同一个 `ostream` 施加插入、`setw` / `setfill` / `setbase`、
    `endl` / `flush` 与 `rdstate()` 读取，各 300 轮，ThreadSanitizer 告警数为 **0**。
    同时跑了一个**故意有竞争的对照组**（4 线程无保护自增同一个 `int`），
    确认 TSan 报出 1 条告警——以排除"检测器根本没生效"。

45. **构造失败路径不泄漏。**
    实测用默认构造的 `file_device<char>`（对应未打开的文件）建流，
    异常从成员初始化列表向外传播，ASan 报告无泄漏。这与文档一致：
    构造是本库唯一不把异常转成状态位的操作，调用方需自行 `try`。

---

## 文件卫生

46. **四个文件都以 `\n` 结尾**（末字节 `0x0a`），符合 POSIX 对文本文件的定义。
    仓库范围复查过：`git ls-files '*.h'` 里现在**没有**任何头文件缺结尾换行。

47. **四个文件都没有行尾空白、没有制表符、没有连续空行、没有重复词**（`the the` 一类）。

48. **`@lang{ZH}` / `@lang{EN}` / `@endif` 的配对数在四个文件里全部相等**：
    `io_manip.h` 27/27/54、`istream.h` 9/9/18、`ostream.h` 13/13/26、
    `iostream.h` 9/9/18。

49. **头文件按"本地组 / C++ 标准库组"两组划分，组内均已按字母序排列。**
    四个文件都不含 C 语言库头文件；`<ctime>` / `<cstddef>` / `<cstdint>` 按规则计入
    C++ 组。当前的本地组是：

    - `io_manip.h`：`common/defs.h`、`facet/monetary.h`、`facet/timeio.h`、
      `io/io_base.h`、`io/iostream.h`、`io/istream.h`、`io/ostream.h`、`io/traits/tm.h`、
      `io/traits/traits_base.h`、`io/utilities/istream_operators.h`、
      `io/utilities/ostream_operators.h`、`locale/locale.h`
    - `istream.h`：`cvt/cvt_concepts.h`、`device/device_concepts.h`、`io/io_base.h`、
      `io/streambuf.h`、`io/streambuf_iterator.h`、`io/traits/traits_base.h`、
      `io/utilities/istream_operators.h`、`io/utilities/stream_common_operators.h`、
      `locale/locale.h`
    - `ostream.h`：与 `istream.h` 同构（`istream_operators` 换成 `ostream_operators`），
      另有 `common/defs.h`（`stream_error`）与 `facet/ctype.h`（`endl` 用 `ctype<TChar>`）
    - `iostream.h`：`cvt/cvt_concepts.h`、`device/device_concepts.h`、`io/io_base.h`、
      `io/istream.h`、`io/ostream.h`、`io/streambuf.h`、`io/streambuf_iterator.h`、
      三个 `io/utilities/*.h`、`locale/locale.h`

50. **四个文件用到的每个名字都有对应的直接 include。** 逐名核对过
    `stream_error`、`ios_defs`、`ios_base`、`ios_state`、`io_traits`、`locale`、
    `ctype` / `monetary` / `timeio`、`istreambuf` / `ostreambuf` / `streambuf`、
    `istreambuf_iterator` / `ostreambuf_iterator`、`in_sentry` / `out_sentry` /
    `out_flusher`、`istream_type` / `ostream_type`、`io_device` / `cvt_creator`、
    `std::tm`，以及各 C++ 标准库设施。

51. **`io_manip.h` 里的 `<io/istream.h>` / `<io/ostream.h>` / `<io/iostream.h>`，
    以及 `iostream.h` 里的 `<io/istream.h>` / `<io/ostream.h>`，是刻意保留的伞头文件。**
    这几个名字确实没有在对应文件里被具名使用（只出现在 `#include` 行与注释里），
    但仓库里 73 个测试文件依赖 `#include <io/io_manip.h>` 一行就拿到三种流，
    删掉属于对外接口变更。**不要**把它们当作冗余 include 报告。

52. **英文注释无拼写错误。** 用 hunspell（`en_US`）扫描过四个文件的英文注释，
    剩余告警全部是标识符、术语与标准条款引用（如 `[basic.indet]`、
    `[locale.time.get.virtuals]`）。拼写风格统一为美式。

53. **四个头文件各自单独 `#include` 都能编译**（clang++ 21 与 g++ 15，`-std=c++23`）。
    连带验证过 `io/objects/{objects,in_impl,out_impl}.h` 与 `src/iov2_objects.cpp`。

---

## 迭代器分类、前缀树回退与纪元解析

54. **`stamp_input_iterator` 如实上报被包装迭代器的分类。** `iterator_category` 与
    `iterator_concept` 两个标签都显式声明，且都取自 `std::iterator_traits<TIter>`；对
    `stamp_input_iterator<istreambuf_iterator<...>>` 二者都是 `input_iterator_tag`，
    C++17 traits 与 C++20 概念口径一致。它**不是** forward / bidirectional 迭代器：流位置在
    共享的 streambuf 里（递增一个副本会改变原件看到的内容），且 `operator*` 返回纯右值而非
    `T&` / `const T&`。`operator--` 与 `rollback()` 是标准分类之外额外提供的原语，靠内部的
    回退日志经 `sputbackc` 实现。实测 `static_assert`：`iterator_category` 为
    `input_iterator_tag`，`std::input_iterator` 为真，`std::forward_iterator` 与
    `std::bidirectional_iterator` 均为假。

55. **"能后退"这一能力由 `steppable_back`（`common/metafunctions.h`）表达。**
    定义为 `std::input_iterator<I> && requires (I i) { { --i } -> std::same_as<I&>; }`。
    双向迭代器被完全包含——`std::bidirectional_iterator` 的定义中已含
    `{ --i } -> std::same_as<I&>` 且蕴含 `input_iterator`——因此约束里**不需要**再析取
    `std::bidirectional_iterator`。`prefix_tree::max_match` 与 `timeio` 的三处 `get` /
    `do_get` 都以它约束。实测：`steppable_back` 对 `stamp_input_iterator<...>` 与
    `std::string::iterator` 为真，对 `istreambuf_iterator<...>` 为假（该类型没有
    `operator--`，其回退走 `sputbackc`，由 `prefix_tree::max_match` 的另一个重载处理）。

56. **`prefix_tree::max_match` 用迭代器自己的 `--` 回退。** 随机访问迭代器走 `-=` 的 O(1)
    路径，其余走 `--` 循环。**不得改用 `std::advance` 或 `std::ranges::advance`**：前者按
    C++17 的 `iterator_category` 分派、后者要求 `bidirectional_iterator` 概念，负数实参在
    两边都是前置条件违反（libstdc++ 开启断言时直接 `abort`）。这条约束写在 `steppable_back`
    的 `@warning` 里。

57. **纪元（era）名被截断的输入只置失败位，不改写目标。** 实测 `ja_JP.UTF-8`：
    `is >> get_time(&t, "%EY")` 读入「平」（「平成」的真前缀）得到失败位，`*tmb` 逐字节保持
    调用前的值；读入「平成31年」正常解析出 `tm_year == 119`。半个 UTF-8 字符、「明」、
    「平31」等同理，完全不匹配的输入（如 `"zzz"`）也正常。`zh_TW.UTF-8` / `th_TH.UTF-8`、
    `%Ec` / `%Ex`、以及 `wchar_t` 流行为一致。回归测试见
    `test/common/test_prefix_tree.cpp::test_prefix_tree_stamp_input_iterator`
    与 `test/common/test_stamp_input_iterator.cpp` 里的分类一致性断言。

## 填充宽度的上限

58. **单次插入最多写出 `ios_defs::max_pad_count`（65536）个填充字符。**
    `width()` 本身仍不设上限（`ios_base::width` 只拒负值）；受限的是**一次插入实际写出多少个
    填充字符**。超出即抛 `stream_error`，经 `handle_exception` 转成 `strfailbit`，且该次插入
    **什么都不输出**。所有插入路径行为统一，实测 `setw(PTRDIFF_MAX)` 与 `setw(1<<30)` 配
    `const char*` / `std::basic_string` / 单个字符 / `int` / `double` / `put_money` /
    `boolalpha` 下的 `bool`，七条路径全部 `state=0x8`、输出长度 0，整组测试峰值 RSS 18 MB、
    总耗时 0.47 s。边界精确：填充量恰为 65536 时正常输出，65537 被拒。常规宽度
    （`setw(10)`、`setw(12)` 配 `internal` 等）不受影响。

## 状态位与 `width` 的消费时机

59. **`adjust()` / `retrieve()` 与其余操作一样把错误转成状态位。** 二者都持有 `io_mutex()`
    并把异常交给 `handle_exception`，因此"除构造之外，本库所有操作都把异常转为状态位"这条
    不变式**没有例外**。移后（moved-from）流上调用二者得到 `cvtfailbit`，不向调用方抛出。
    `retrieve` 取非常量的 `this TSelf&`——`handle_exception` 是非常量成员。

60. **`ostream_insert` / `istream_extract` 在任何可抛操作之前消费 `width`。**
    `io.width(0)` 在函数入口即执行，与 `numeric.h` / `monetary.h` 明文写下的规则一致：
    这两个函数内部的任何一条抛出路径都不会把 `width` 留给下一次操作。

61. **哨兵提前退出时 `width` 保留，与标准库一致。** 提取运算符的哨兵判到 EOF 后直接抛出，
    不进入 `istream_extract`，此时 `width` 不被消费。实测 IOv2 与 libstdc++ 逐项相同：
    空流上 `is >> setw(5) >> buf` 之后 `width == 5`；全空白输入同；失败态下
    `os << setw(20) << "abc"` 之后 `width == 20`；随后换设备再提取，两边都被残留宽度同样地
    截断。这是标准行为（sentry 转为 `false` 时 `width(0)` 那一步本就不执行），**不是缺陷**。

## 宽字符流（`wchar_t`）

62. **全套格式状态操纵符在 `wchar_t` 流上的行为与窄流逐项一致。**
    `setw(8) << 42` → `"      42"`；`setfill(L'*')` → `"******42"`；
    `setbase(16) << 255` → `"ff"`；`setprecision(3) << 3.14159` → `"3.14"`；
    `setiosflags(left) << setw(6) << 42` → `"42    "`。ASan/UBSan 干净。

63. **宽流上的失败路径同样只置位、不越界。** `os << setw(-1) << 42` → `state=0x8`
    且无任何输出；`os << setprecision(300) << 1.5` → `state=0x8` 且无输出。

64. **`put_time` / `put_money` / `get_time` / `ws` 在宽流上正常。**
    `put_time(&t, L"%Y-%m-%d %H:%M:%S")` → `"2024-01-15 01:02:03"`；
    `put_time(&t, (const wchar_t*)nullptr)` → `state=0x8` 且无输出（空指针检查生效）；
    `put_money(12345)` → `"123.45"`；`os << endl << ends` 输出换行加空字符；
    `is >> ws >> get_time(&t, L"%H:%M") >> setbase(16) >> v` 得 `h=1 m=2 hex=255`，
    `state=0x1`（仅 eofbit）。

65. **宽流上的 `setw` 提取边界与窄流一致。** `wchar_t buf[4]`，
    `is >> setw(10) >> buf` 读 `L"hello"` 得 `L"hel"`，ASan 无越界。

66. **字符类型不匹配的用法在宽流上照样在声明层面被拒**（预期行为，非缺陷）：
    `os << setfill('*')`（窄填充字符）、`os << put_time(&t, "%Y")`（窄格式串）、
    `os << put_money(std::string("12"))`（窄 string）、`os << ws`、`is >> endl`
    五种全部编译失败。

67. **宽字符流的拷贝 / 移动 / 赋值语义与窄流一致。** `istream<mem_device<wchar_t>>`
    先 `peek()` 得 `L'h'` 再整体拷贝，原件与副本各自提取都得 `L"hello"`，互不影响；
    移后的 `ostream` 上继续 `<< L"x"` / `flush()` / `<< endl` 不崩溃；
    宽 `iostream` 写入后 `switch_to_get()` 再读取行为与窄流相同；
    `a = b; a = std::move(b); a = a;` 三种赋值连做，`state=0x0`。ASan/UBSan 干净。

## 编码转换管线（char 设备 → char32_t 流，UTF-8）

68. **操纵符经过转换管线后的运行期行为正确。**
    `ostream os(mem_device<char>{}, code_cvt_creator<char, char32_t>{"C.UTF-8"})`：
    `setfill(U'*') << setw(8) << 42` → 字节流 `"******42"`；`setbase(16) << 255` → `"ff"`；
    `setprecision(3) << 3.14159` → `"3.14"`；`<< endl` 写出换行。`state=0x0`。

69. **转换管线上的失败路径同样只置位。** `os << setw(-1) << 42` → `state=0x8`。

70. **转换管线上的提取侧正常。** `istream is(mem_device<char>{"   ff  hello"}, cr)`：
    `>> ws >> setbase(16) >> v` 得 `v=255`；`char32_t buf[4]`，`>> setw(10) >> buf`
    得长度 3。`state=0x0`，ASan/UBSan 干净。

71. **`put_time` / `put_money` 经转换管线输出正确。**
    `put_time(&t, U"%Y-%m-%d") << put_money(12345)` → 字节流 `"2024-01-15123.45"`，
    `state=0x0`。

## 特殊成员函数、继承体系、生命周期（续）

72. **`operator=(X&&) noexcept` 名副其实。** 逐个 `static_assert` 实测，本机 libstdc++ 上
    每一环都真的是 `noexcept`：`std::unordered_map<size_t, shared_ptr<void>>`、
    `std::forward_list<pair<function<...>, size_t>>`、`copyable_mutex<recursive_mutex>`、
    `ios_base<char>`、`ios_state<char>`、`istream_operators` / `ostream_operators`、
    `out_flusher<ostream>` / `out_flusher<iostream>`、`stream_common_operators`、
    `istreambuf` / `ostreambuf` / `streambuf`、`locale<char>`，以及三个流类型本身。
    （`abs_flusher` 单独测显示 false，只是因为它的移动赋值是 `protected`、类外不可访问，
    并非退化。）注意拷贝赋值里的 `static_assert` 只在拷贝赋值被实例化时才检查，只用移动
    赋值的用户不受它保护——但当前每一环都确实是 `noexcept`，没有可被触发的漏洞。

73. **拷贝 / 移动赋值与 `try_flush()` 确实互斥。** `operator=(X&&)` 持有的是**目标**的
    `io_mutex()`，`try_flush()` 对**同一个** mutex 对象 `try_lock`——`copyable_mutex` 的
    赋值是 no-op，底层 mutex 对象在整个赋值过程中不被替换或重建，所以两者是同一把锁。
    TSan 实测：一线程 3000 轮 `P = std::move(q)` / `P = r`，另一线程持续对 tie 到 P 的流
    做 I/O，**0 告警**（同时跑了故意有竞争的对照组，确认 TSan 生效）。

74. **CRTP 的 `static_cast<T&>(*this)` 在赋值期间安全。** `out_flusher` 无数据成员，
    vptr 在赋值中不变。

75. **移后流上所有成员都把错误转成状态位。** `tell` / `seek` / `rseek` / `attach` /
    `locale()` / `locale(loc)` / `tie` / `adjust` / `retrieve` / `switch_to_put` /
    `switch_to_get` / `put` / `write` / `flush` / `<< endl` / `get` / `peek` / `read` /
    `ignore` / `putback` / `>>` / `>> ws` / `try_flush` 实测均不崩溃，`state` 落在
    `0x4` 或 `0xc`，ASan/UBSan 干净。仍由调用方负责的只有 `device()` 与 `detach()`
    （二者以"持有设备"为前置条件，见其文档）。

76. **从移后流拷贝构造不会空指针解引用。** `runtime_cvt` 的拷贝构造与拷贝赋值都判空；
    实测拷贝成功，后续读取正常转成 `strfailbit`。

77. **`tie(nullptr)` 在 tie 目标已析构之后调用不会解引用悬垂指针。** `check_tie`
    只从**新**目标 `str` 起步走链，旧值仅作返回值；实参为 `nullptr` 时链长为零，不走任何一步。
    **这只对 `nullptr` 实参成立，不要推广**：实参非空时 `check_tie` 会沿整条链
    `dynamic_cast`（即读每个节点的 vptr），链上任何一个已销毁的节点都会被解引用——哪怕发起
    调用的流与它素不相识（`b.tie(&c); delete c; a.tie(&b);` 崩在 `a.tie()` 上），且这次遍历
    持有进程级全局锁 `tie_graph_mutex()`。这属于调用方违反 tie 目标生命周期契约后的 UB，
    `tie()` 的 `@warning` 已把这条爆炸半径写明。

78. **tie 环检测有效，且失败的上报方式与库内其它操作一致。** 3 节点环与自绑定实测均被拒，
    原绑定保持不变。被拒经 `handle_exception` 置 `strfailbit`，**仅当该位在异常掩码中时才抛
    `stream_error`**；默认掩码为空，故默认不抛。因此返回值只表示"设置前保存的那个指针"，
    要判断本次设置是否生效须用 getter 比较（`self.tie() == str`）或查 `strfailbit`；被拒后
    流带 `strfailbit`、`operator bool` 为假，须先 `clear()` 才能继续用。
    `check_tie` 本身是 `noexcept`（只做 `dynamic_cast` 与原子读），上报发生在放掉
    `tie_graph_mutex()` **之后**，因为上报要取 `io_mutex()`，放在锁内会新增
    `tie_graph_mutex()` → `io_mutex()` 这条锁序边。

79. **私有的加锁委托构造函数不会劫持任何合法调用。** 它收的是具体类型
    `const std::lock_guard<copyable_mutex<std::recursive_mutex>>&`
    （`istream.h:250`、`ostream.h:256`、`iostream.h:262`），不是模板。
    三个类各 6 种构造形态 + CTAD + 拷贝/移动 CTAD + `is_constructible` 断言实测，
    全部落到预期的公开构造函数上。`istream<D,char> x(0, dev)` 这类"第二实参是设备"的两参
    调用连候选都进不去——实测诊断是"没有匹配的函数"，而非选中后因私有而报错。

80. **`operator bool` 对 eofbit 的处理使 `switch_to_*` 不会被错误跳过。**
    `operator bool` 在"仅 eofbit"时返回 `true`，与 `std` 的 `!fail()` 一致。实测：
    `state=0x1 bool=1` → `switch_to_put()` 生效 → 写入成功 → `switch_to_get()` 生效。
    失败态（`strfailbit`）下静默返回、状态不变，与 `tell()` / `flush()` 一致。

81. **`iostream` 的两个 mixin 无同名成员歧义，两个 sentry 不会在同一次操作里共存。**
    逐个列出 deducing-this 成员核对：`istream_operators` 侧
    `get` / `peek` / `read` / `ignore` / `putback` / `i_iter`，`ostream_operators` 侧
    `put` / `write` / `flush` / `o_iter`，`stream_common_operators` 侧
    `tell` / `seek` / `rseek` / `device` / `detach` / `attach` / `adjust` / `retrieve` /
    `locale` / `tie`，无交集。操纵符走 `io_traits` 的"流形式"分支，`operator<<` 那层不建
    哨兵；`endl` 的哨兵只在 `put()` 内部建一次。混合读写压力测试 ASan 全程干净。

82. **`iostream` 方向切换在压力下无问题。** 纯 `switch_to_put` / `switch_to_get` 交替
    100000 轮、混合读写 2000 轮、`>> ws` 与 `<< endl` 交替各 200 轮，ASan+UBSan 干净。
    写后 `switch_to_get()` 再读、`>> ws` 隐式切读、读后 `<< endl` 隐式切写，三种组合结果
    都与标准双向流语义一致（对 `"hello world"` 写 `"ABC"` 后读得 `"lo"`，设备为
    `"ABClo world"`）。`switch_to_get()` 之后立刻 `<< endl` 正常写出（由 `out_sentry`
    自行切回 put 方向）。

83. **TSan 追加组合无竞争。** 4 线程 × 300 轮在同一个 `iostream` 上并发施加
    `switch_to_get` / `switch_to_put` / `>> ws` / `<< endl` / `setw`+`setfill`+插入 /
    `rdstate()`+`clear()`，**0 告警**（第 44 条只覆盖了 `ostream`，本条补上 `iostream` 与
    `switch_to_*`）。另一组 4 线程做方向切换 + tie flush + 无锁 `rdstate()` / `tie()`
    读取，同样 **0 告警**。

## `setw` / `setprecision` / `setfill` 极值

84. **`setw` 负值在两个方向都被挡住。** `setw(PTRDIFF_MIN)`、
    `setw(static_cast<ptrdiff_t>(SIZE_MAX))`（= −1）：`ios_base::width` 判负抛出 →
    `strfailbit`，`width` 保持原值 0。

85. **提取侧的 `setw` 巨值无害。** `is >> setw(PTRDIFF_MAX) >> char[8]` → `"hello"`、
    `state=0x0`、`width` 归零；`>> std::string` 同；`is >> setw(PTRDIFF_MIN) >> char[8]`
    → `strfailbit` 且缓冲区一字未动。ASan/UBSan 干净。

86. **`setw` 与其它标志的组合正确。** `setw(3) << ""` → 三个空格；`setw(6) << "\0ab"`
    → `"   \0ab"`（长度 6，NUL 原样写出）；`setw(2)` 小于内容时不截断。
    `setw` 不被 `endl` / `ends` / `flush` 消费；`setw(4) << 1 << 2` 只有第一次补齐
    （`"   12"`）。

87. **`setprecision(255)` 在所有 floatfield 组合下缓冲区都够。** 配 `1e308` / `-1e308` /
    `denorm_min` / `inf` / `nan`，`default` / `fixed` / `scientific` / `hexfloat` 四种
    floatfield，`float` / `double` / `long double` 三种类型：最长输出 6833 字符
    （`fixed` + `long double::max()` + 千位分组），全部 `state=0x0`，ASan 无告警。
    `setprecision(0)` 配 `showpoint`：`default` / `fixed` 得 `"3."`，`scientific` 得
    `"3.e+00"`。`setprecision(256)` 与 `setprecision(SIZE_MAX)` → `strfailbit`。

88. **`setfill('\0')` 无越界。** 配 `setw(6)`：字符串得 `"\0\0\0\0ab"`、整数得
    `"\0\0\0\042"`、`internal` + `-42` 得 `"-\0\0\042"`，长度均为 6。
    `setfill(0)`（`TFill = int`）如文档所述在声明层面被 `same_as` 约束拒绝。

89. **`setfill` 的数组 / 指针推导不会造成怪异行为。** 按值形参使数组与函数退化：
    `setfill("*")` → `setfill_t<const char*>`，`setfill(char_arr)` → `setfill_t<char*>`。
    二者在 `io_traits` 的 `same_as<T::char_type, TFill>` 处不可行，**编译期即被拒**，
    不会静默转换或越界。

## `put_time` / `get_time` 与 `traits/tm.h`

90. **`io/traits/tm.h::make_parse_context` 的整数运算无溢出 / UB。** 六个字段各取
    `INT_MIN` / `INT_MAX` 及全极值组合共 18 组，UBSan 零告警。逐行核算：
    `total_mon` 最大约 2.6e10、`tod` 最大约 7.9e12、`day_carry * secs_per_day` 同量级，
    全部落在 `int64` 内；`norm_year` 先调月再 clamp 到 `year::min()/max()`，`norm_mon`
    恒在 `[0,11]`，`offset` clamp 到 ±4e6 后 `static_cast<int>` 无损；`first + days{offset}`
    约 ±1.7e7 天，远在 `int32` 内，且 `year_month_day{}` 只在 `std::clamp(...)` **之后**
    构造；`tod - day_carry*secs_per_day` 恒在 `[0, 86399]`。负数取余那条
    `-((86400-1-tod)/86400)` 在 `tod = -1 / -86400 / -86401` 三个边界上手算验证为正确的
    向下取整。`tm_sec == 60` → 59 的截断按文档执行。

91. **`put_time` 的 `std::tm` 单字段极值一律只置失败位。** `tm_year` / `tm_mon` /
    `tm_mday` / `tm_hour` / `tm_min` / `tm_sec` 取 `INT_MAX` / `INT_MIN` 都得 `strfailbit`、零输出，
    与"要求整体自洽"的契约一致。`tm_wday = INT_MAX` / `tm_yday = INT_MIN` 被忽略并由日期
    重算（`%a %j` → `三 319`），`good()`。

92. **`put_time` 格式串边界。** `"%%"`→`%`、`"%%%%"`→`%%`、`"abc%"`→`abc`、`"%"`→空、
    `"%E"`/`"%O"`→空、`"%Ez"`→字面量 `%Ez`、`"%Od"`→`15`、`"%EY|%Oy"`→`2023|23`、
    未知 `"%q"`→字面量 `%q`、含非 ASCII 字节正常透传。超长格式串：20000 个 `%Y-%m-%d`
    （输出 220000 字符）、100000 个字面量 `x`、50000 个连续 `%`（输出 25000 个 `%`）
    全部 `good()`，ASan 干净。

93. **`get_time` 格式串边界。** `"%%"` / `"%Ez"` / `"%Od"` / 非 ASCII 均按文档退化处理；
    `"%"` / `"%Y%"` / 50000 个 `%` 得 `strfailbit`（+`eofbit`），且 `*tmb` 六字段与
    `tm_wday`/`tm_yday`/`tm_isdst` **全部保持调用前的值**。20000 个 `%Y` 配 80000 字符
    输入正常解析。

94. **`get_time` 的数值溢出防线有效。** `%Y` 读 20 位与 300 位全 9 的输入只取 4 位
    （与 `std::chrono::parse` 一致）；`%m` / `%d` / `%H` / `%j` / `%U` / `%V` / `%S`
    读 12 位 → `strfailbit` 且目标不变；`%C` / `%y` / `%G` 取固定位数；
    `2023-02-30` → `strfailbit`。`timeio::extract_num` 的 `value = value*10 + digit`
    无溢出：全部 33 个调用点的 `max_val` 上限是 9999，`if (value > max_val) break`
    保证乘法前 `value ≤ 9999`。

95. **`mp->get` 抛出后跳过 `convert_to` 是正确行为，不是半写。** `io_manip.h` 的
    `get_time` 里 `tmp` 是局部拷贝，`*(f.tmb)` 只在 `convert_to` 一处被写；
    `timeio::get` 失败即 `throw`，`convert_to` 不执行，目标逐字节不变。

96. **`convert_to(std::tm&)` 自身也不会半写。** 它先完成 `convert_to(ymd)` 与
    `convert_to(hms)`（两者都可能抛），再逐字段赋值，所以抛出时目标 `std::tm` 一个字节都
    不被触碰。`tm_yday` 的查表计算（`days[12]` 与闰年修正）在 1 月 1 日、2 月 1 日、
    3 月 1 日（平/闰）、12 月 31 日（平/闰）六个边界上都正确，索引 `tm_mon ∈ [0,11]` 由
    `convert_to(ymd)` 的有效性保证，无越界。

## `put_money` / `get_money`

97. **`io_manip.h` 对 facet 的调用签名与实现吻合。**
    `monetary::put(TIter, bool intl, ios_base<char_type>&, TVal)`、
    `monetary::get(TIter, TSent, bool intl, ios_base<char_type>&, TVal&)`、
    `timeio::get(TIter, TSent, time_parse_context&, ...)`，实参顺序与类型全部对得上，
    重载选择与 `io_traits` 的 `requires` 子句一一对应、无歧义。

98. **`monetary::put` 的 `digits10 + 3` 栈缓冲对所有整型极值都够。** 布局 = 符号 1 +
    数字 `digits10+1` + `'\0'` 1。实测 `CHAR_MIN` / `SHRT_MIN` / `INT_MIN` / `LLONG_MIN` /
    `LLONG_MAX` / `ULLONG_MAX` / `char8_t` / `char32_t` / `wchar_t` 全部正确输出，
    ASan 无越界。`monetary::insert` 的 `2*paddec` 缓冲对最小分组大小 1 也够
    （`adjust_grouping` 保证 `grouping[0] != 0` ⇒ 分隔符最多 `paddec-1` 个）。

99. **`str_to_v` 的上下溢检查是紧的。** 两条防线（`value > (max-digit)/10` 与
    `value < (min+digit)/10`）在 `INT_MIN`、`LLONG_MIN`、`ULLONG_MAX` 边界上逐位算过；
    实测越界输入得 `strfailbit` 且目标不被改写。`get_money` 读 50000 位到
    `std::basic_string` 成功（`eofbit`）；读到 `long long` 时 `9223372036854775807` 正确、
    `...808` 与 50000 位一律 `strfailbit` 且目标完全不被改写。

100. **`put_money` 超长输入无越界。** `std::string(50000,'9')` 与 `"-" + 50000 位`：
     输出 66666 / 66667 字符，`good()`，ASan 干净；`intl=true` 同。

101. **`setw` 与 `put_money` 的宽度路径无越界。** `setw(1000) << put_money(1)` 输出恰好
     1000 字符；`setw(0)` 不补齐；`monetary::insert` 在任何可能抛出的操作之前就
     `io.width(0)`。

102. **多 locale 覆盖。** `C` / `POSIX` / `en_US.UTF-8` / `de_DE.UTF-8` / `ja_JP.UTF-8` /
     `zh_CN.UTF-8` / `ru_RU.UTF-8` / `ar_SA.UTF-8` 各跑了 `put_money` / `get_money`
     （含 30000 位）与 `put_time` / `get_time`（`%c %x %X %A %B %p %r`），
     ASan/UBSan 全程无告警。

## 载体类型、扩展点与迭代器契约

103. **四个载体类型的 `requires` 子句不会选中错误的特化，也没有同一完整表达式内的悬垂。**
     20 项 `requires` 探针实测全部符合预期：`put_money(bool/double/wstring)`、
     `get_money(volatile int&)` 被拒；`put_money(volatile int)`（cv 由按值形参吸收）被
     接受；四个载体的方向严格单向（`is >> put_money` / `os << get_time` /
     `os << put_time_t<wchar_t>` 配 `char` 流均不可行）。
     `put_money_fn::operator()` 的 `const TMoney&` 是**推导**形参、不发生转换 ⇒ 不产生需要
     延长寿命的临时量；`get_money_fn` 取 `TMoney&`，右值实参连编译都过不去。

104. **`put_money` / `get_money` 这两个 `inline constexpr` 函数对象无 constexpr UB、
     无 ODR 问题。** 双 TU 实测：`&IOv2::put_money` 在两个 TU 中地址相同（`get_money` 同）；
     `nm -C` 显示符号类型为 `u`（unique global，inline 变量的单一实体）。`put_money_fn` 是
     `sizeof == 1` 的聚合体，`{}` 为常量初始化。文件里"类型必须具名"的注释成立。

105. **迭代器契约一致，且 `operator>>` 丢弃 `sread` 的返回迭代器是安全的。**
     `io_traits` 侧写 `std::sentinel_for<TIter> TSent`，`monetary::get` / `timeio::get`
     侧一致，`operator>>` 传的是 `std::default_sentinel`。
     `istreambuf_iterator::operator*` 用 `sgetc()` 且**不写回 `m_c`**，`operator++` 用
     `sbumpc()`，位置状态全在 streambuf 里，只有后置 `++` 的旧副本才带缓存。

106. **`put_time` / `get_time` 的 `swrite` / `sread` 不使用 `ios_base& io` 形参是有意的，
     与文档一致。** 两者既不应用也不消耗 `width`，形参保留只为匹配扩展点签名。

107. **`endl` 里 `locale().get<ctype<TChar>>()` 返回的是拥有所有权的 `shared_ptr`。**
     `locale` 的三个 `get()` 重载都按值返回 `std::shared_ptr<TF>`，不会悬垂。

## `ws` / `endl` / `ends` / `flush` 的状态与异常路径

108. **失败态下四个操纵符都被哨兵拒掉。** `os << setw(-1)`（`strfailbit`）后
     `<< endl` / `<< ends` / `<< flush` 均不输出、不追加新状态位；
     `is >> setw(-1)` 后 `>> ws` 同样保持 `0x08`。

109. **EOF 态下 `is >> ws` 不追加失败位**（保持 `0x9`）。

110. **异常掩码下锁被干净释放、不死锁。** `exceptions(strfailbit)` 下 `<< setw(-1)` 与
     `<< endl` 各抛一次并被捕获，`clear()` + 关掉掩码后 `<< "still usable"` 正常写出；
     `exceptions(eofbit)` 下 `>> ws` 同理。

111. **移后 `iostream` 上调用 `switch_to_put()` / `switch_to_get()`** 得 `cvtfailbit`
     （`0x4`），不崩溃、不 `terminate`。

112. **`stream_common_operators` 的 8 处 `lock_guard` 都不会让异常逃逸。**
     `tell` / `seek` / `rseek` / `attach` / `adjust` / `retrieve` 各有 `try`；
     `detach` 设计上 `noexcept` 且被调方也是 `noexcept`；`locale(loc)` 锁外两步是
     noexcept 移动，会抛的 `access_callbacks` 在 `try` 内。

## 随机模糊测试

113. **io_manip 全操纵符随机组合模糊测试无告警。** 22 种操作（六个格式操纵符的两个方向、
     `put_money` / `get_money` 整型与字符串、`put_time` / `get_time` 配 16 种格式串含
     `%` / `%%` / `%Ez` / `%Od` / 空串、`endl` / `ends` / `flush` / `ws`、`switch_to_*`
     + 混合读写、随机 `clear()`），每轮 12 步、每种子 4000 轮、5 个种子，
     ASan+UBSan 零告警。

114. **货币与时间的随机扫描无告警。** 60000 轮，9 个 locale，随机格式串（含 `%E` / `%O`
     修饰符）× 随机字节输入（含任意字节 0-255、非法 UTF-8）× 随机 `std::tm` 回退值
     （含 `INT_MIN` / `INT_MAX`），每轮同时跑 `get_time`→`put_time` 回灌与
     `get_money`→`setw`+`put_money` 回灌。ASan + UBSan + LeakSanitizer 零告警。
     另有 30000 轮开启 `%EY` 的纪元路径扫描，同样零告警。

## 其它已确认

115. **流赋值不会丢弃未刷新的缓冲数据。** 实测 `ofile_device` 上
     `a << "LOST"; a = std::move(b);` 后 `A.txt == "LOST"`，与 `std::ofstream` 同样条件下
     的行为完全一致。数据是在移动赋值那一刻（旧设备析构）落盘的，不是提前写穿的。

116. **`ios_base` 的 pword 浅拷贝是既定语义。** 拷贝一个流之后两份 pword 条目共享同一批
     底层对象，这一点在 `io/io_base.h` 的注释里已明确写下。

## 解析路径抛异常时的流位置

117. **解析中途抛异常时，流停在抛出点，已消费的字符不退回。** 所有解析路径一致：纪元展开
     循环（`facet/timeio.h` 的 `%EY`）与普通的 `%Y` 表现相同。纪元循环里的
     `rp_wrapper.rollback()` 只服务于"换下一个候选格式串再试"，异常会一路展开出
     `timeio::get`、由流转成状态位，没有下一个候选要用这个位置，因此该路径上不调
     `rollback()` 不可观测。用"读到第 N 字节抛一次 `device_error`"的注入设备逐字节扫过
     `"平成12年@REST"`（`ja_JP.UTF-8`，`%EY`）验证：剩余输入恰好从第 N 字节开始，无重复、
     无丢失，ASan / LeakSanitizer 零告警。`stamp_input_iterator` 的析构函数不做回退，
     这是刻意的——异常来自设备或转换器，展开途中再往一个已经出问题的 `streambuf`
     `sputbackc()` 只会二次抛出。

## `detach()` 之后的流

118. **`detach()` 留下的是一个移后的设备对象，而不是"没有设备"。** 后续读写的结果取决于
     `device_type` 移后还剩什么，两种情形都不是未定义行为：
     - `mem_device` 这类移后仍是合法空设备的类型：流照常工作，`os << "second"` 得
       `state=0x0`，数据进入那个空设备，可由**下一次** `detach()` 原样取回；读则立即
       遇到 `eofbit`（`>> string` 得 `0x9`，目标变量不被写）。注意中间若调了 `attach()`，
       旧设备会按 `attach()` 的约定被静默丢弃——那是 `attach()` 的语义，不是数据丢失。
     - `file_device` 这类移后不可用的类型：按常规失败路径报错。缓冲写入本身仍成功，
       错误在数据真正落到设备时（`flush()` 或缓冲写满）以 `devfailbit` 报出。这比
       `std::ofstream`（`close()` 后第一次插入就置 `failbit|badbit`）晚，是写回缓冲的
       自然结果，错误并未被吞掉。

     默认构造、从未 `attach()` 过的 `ostream<mem_device<char>, char>` 同理：它是一个
     针对空内存设备的可用流，`<< "hello" << 42` 之后 `detach()` 得到 `"hello42"`。

## `put_money` / `get_money`（续）

119. **`intl = true` 在 `en_US` / `ja_JP` 下写出的内容读不回来，与 libstdc++ 逐字节一致。**
     两边都写出 `" 123.45"` / `" 12,345"`，也都以失败位读不回来。这是 glibc 的
     `int_curr_symbol` 数据本身造成的，不是本库的缺陷。13 个 locale × 两个 `intl` ×
     6 个数值共 156 组对照，只有下面两类与 libstdc++ 不同，且都是本库有意的策略。

120. **`C` / `POSIX` locale 下负号被保留。** 本库把该 locale 的正负号硬编码为 `"-"`
     （`facet/monetary_details.h`），`put_money("-12345")` 写出 `-12345` 并能原样读回；
     libstdc++ 按标准让 `negative_sign()` 返回空串，写出 `12345`、丢掉符号，且读不回
     `-12345`。本库的往返是自洽的。

121. **千位分隔符不是单个 `char` 时，分组整体关闭。** `fr_FR` / `ru_RU` 的
     `mon_thousands_sep` 是 U+202F（UTF-8 三字节），装不进一个 `char`，此时
     `m_grouping` 保持为空，输出不带分隔符（`9999999999,99`），解析侧也不施加分组校验。
     `facet/monetary_details.h` 与 `facet/numeric_details.h` 对这条规则各有明文注释，
     两边一致。作为对照，libstdc++ 在同一 locale 下吐出该字符的**首字节** `0xE2`，产生
     非法 UTF-8。回退用的 `','` 不会被误当成分隔符接受：这些 locale 下
     `get_money("1,234")` 与 libstdc++ 同样失败。

## `put_time`（续）

122. **格式串尾部的孤立 `%` 被丢弃，与 `std::put_time` 逐字节一致。** `"abc%"` → `"abc"`，
     `"%"` → `""`，`"%E"` / `"%O"` 同理输出空；而未知说明符按原样透传
     （`"%q"` → `"%q"`，`"abc%Eq"` → `"abc%Eq"`），`"%%"` → `"%"`。POSIX `strftime`
     在这些情形下会保留结尾的 `%` / `%E` / `%O`，但本库对齐的是
     `std::put_time` / `std::time_put`，不是 `strftime`。14 组格式实测全部一致。

## `io_traits` 扩展点的可探测性

123. **`io_traits<TChar, get_time_t<TChar>>::sread` 的约束与 `timeio::get` 一致。**
     两者都要求 `steppable_back<TIter> || is_istreambuf_iterator<TIter>`，因此一个只提供
     单趟输入、既无 `operator--` 也不是 `istreambuf_iterator` 的迭代器会在**约束**处被
     拒绝，`detail::extractable_with_iter` 能如实探测出"这个流不能提取 `get_time`"，
     而不是先判定可行、再在函数体里硬报错。`get_money_t` 一侧不需要这条约束——
     `monetary::get` 只要求 `std::sentinel_for`。

## `facet/monetary.h::extract` 与 `facet/timeio.h::do_get` 的逐行核对

124. **`extract` 的边界都成立。** `lit_zero = s_atoms.data() + s_zero` 配
     `std::find(lit_zero, lit_zero + 10, c)` 恰好扫到 11 元素数组的尾后位置（合法）；
     货币符号匹配循环先查 `j < len` 再索引，空符号串不会误判失败；去前导零对全零串走
     `find_first_not_of` 的 `npos` 分支、保留一位；`str_to_v` 的溢出判据在
     `INT_MIN` / `INT_MAX` 两端逐位验算正确（正向 `value > (max - digit) / 10`、
     负向 `value < (min + digit) / 10`，含"恰好等于极值"的临界位都不误拒）。

125. **`do_get` 的每处 `*rp` 都有守卫。** 外层循环条件保证进入 switch 时 `rp != rp_end`；
     `%z` 分支里每次 `++rp` 之后都补了 `rp == rp_end` 检查（含 `':'` 可选分隔那一步）；
     格式串尾部扫描的 `fmt + 1` / `fmt += 2` 都先查过 `cend()`；`extract_num` 的 `value`
     在 `max_val ≤ 9999`、`len ≤ 4` 下上界 99999，不会溢出；`%Ey` 的纪元筛选用 `int64_t`
     做 `(year − offset) × direction` 的运算，避免 `int` 溢出。

126. **两个 `prefix_tree::max_match` 重载的回退步数都有界。** 回退步数是
     `node_ptr->depth − found_depth`，不超过本次调用自己前进的步数，因此
     `stamp_input_iterator` 的 `m_rec` / `istreambuf_iterator` 重载的 `checking_chars`
     总是够用，`operator--` 不会抛 "cannot move backward"。`max_match` 按值收
     迭代器、由调用方接收返回值，回退发生在副本上、结果覆盖原迭代器，不存在双重回退。

127. **`extract` / `do_get` 的定向 + 随机扫描无告警。** 定向部分：59 个说明符
     （含 `%E*` / `%O*` / 未知 / 空）× 35 个输入（空串、恰好在说明符要读的位置结束、
     多读一个字符、非法 UTF-8、时区名前缀），带与不带字面尾巴各跑一遍。随机部分：
     36 万次解析，char 与 `wchar_t` 各一半，**连续迭代器**（`std::string::iterator`，
     越界在这里是真正的 UB，ASan 看得见）与流的 `istreambuf_iterator` 各一路，
     6 个 locale，随机格式串 × 随机字节输入（含高位字节与非法 UTF-8）。
     ASan + UBSan + LeakSanitizer 零告警。

## 三个流类：移动构造、读缓冲与方向约束

128. **三个流类的移动构造都是 `noexcept`，且名副其实。** `istream(istream&&)` /
     `ostream(ostream&&)` / `iostream(iostream&&)` 均写作 `noexcept = default`，每个基类与成员的
     移动构造都真的不抛。取证要**逐基类逐成员**做：对流类本身用
     `is_nothrow_move_constructible_v` 是同义反复——显式 `noexcept` 会覆盖隐式异常规格，
     该断言恒为真（第 180 条对移动赋值已指出同一点）。实测逐个断言 `ios_base`、`ios_state`、
     两个 operator mixin、`stream_common_operators`、`out_flusher`、`copyable_mutex`、
     `copyable_atomic`，以及三种 streambuf × 三种设备 × 两种字符类型与 `locale<>`，
     在 g++15 与 clang21 上全部真 nothrow。`file_device` 不可拷贝而
     `streambuf<file_device<char>,char>` 仍 nothrow-move，可证没有退化成拷贝。由此
     `std::vector<流>` 一类容器在扩容时经 `std::move_if_noexcept` 会选中**移动**构造，不会退化
     成拷贝构造——那条路在 move-only 内核（`file_device` 一族）上必然抛 `cvt_error`。实测
     `std::vector<istream<ifile_device<char>, char>>` 与
     `std::vector<ostream<ofile_device<char>, char>>` 扩容都正常。

129. **读缓冲区 `m_read_buf`（`io/streambuf.h`）是 `std::basic_string<char_type>`，语义上是一个
     栈顶在末尾的后进先出栈。** 它只在一端进出：`sputbackc()` 与 `sgetc()` 用 `push_back` 压入，
     `sbumpc()` 与 `sgetn()` 用 `pop_back` 弹出，末尾即为下一次读取将返回的字符。因此多次
     put-back 按**后进先出**取回。非输入模式下退化为 `std::monostate`，`[[no_unique_address]]`
     使其不占对象空间。两点连带性质：`std::basic_string` 的移动构造由标准明文保证 `noexcept`，
     这是第 128 条得以成立的一环；小字符串优化使常见的单字符 put-back 完全不分配。
     `char_traits<char_type>` 不是新增约束，`facet/messages.h` 早已依赖它。

130. **三个类的模板头各自约束设备方向**：`istream` 要求 `dev_cpt::support_get`，`ostream` 要求
     `dev_cpt::support_put`，`iostream` 两者都要求（`io_device` 本身只要求二者**之一**）。方向
     不匹配的组合在**声明**层面即被拒，CTAD 与显式写出模板实参两条路径同时被挡住，不会推进到
     转换器或哨兵内部才失败。实测 `ostream<ifile_device<char>, char>`、
     `istream<ofile_device<char>, char>`、`iostream<ifile_device<char>, char>` 都得到
     "constraints not satisfied"。

131. **三个类的移动构造未被隐式删除，各基类与成员都真有移动构造。** 探针 `is_move_constructible_v`
     对 `istream` / `ostream` / `iostream` / `ios_state` / `ios_base` / `istream_operators` /
     `ostream_operators` / `stream_common_operators` / `out_flusher<ostream>` / `istreambuf` /
     `ostreambuf` / `streambuf` / `locale<char>` / `mem_device<char>` 全为真。

132. **`ostream` / `iostream` 是多态类型**（`abs_flusher` 有虚析构），`istream` 不是；与各自是否
     派生 `out_flusher` 一致。`is_polymorphic_v` 实测。

133. **`stream_common_operators` 的 `m_tie_stream` 有默认成员初始化器 `{nullptr}`**
     （`utilities/stream_common_operators.h:939`），因此其拷贝/移动构造体内虽不显式初始化它，
     第 34 条"四个特殊成员都把目标的 tie 置空"仍然成立。

## `ws` / `endl` / `ends` / `flush`（补充）

134. **`is >> ws` 无视 `noskipws`，与 `std::ws` 一致。** `io_traits<TChar, ws_t>::sread` 传给
     `in_sentry` 的 `noskip = false` 是**硬编码常量**，不读 `skipws` 标志；而 `operator>>` 的
     迭代器分支（`utilities/istream_operators.h:1159-1160`）才是
     `bool skip = flags() & skipws; sentry_type cerb(obj, !skip);`。实测（ASan+UBSan）：
     `noskipws` 下 `is >> ws >> s` 得 `"hello"` / `state=0x1`；同样输入去掉 `ws` 得
     `""` / `0x8`。全空白输入、空流、`iostream >> ws` 均正常。

135. **`endl` 里的 `ctype<TChar>::widen('\n')` 不会失败也不会越界。** 实现是
     `m_widen[static_cast<unsigned char>(c)]`（`facet/ctype.h:614` 及多字节特化），256 元素数组，
     `'\n'` == 10 恒在界内；两个特化都只做数组读，不转发到别处。

136. **`ends` / `flush` 不自己加锁是正确的。** `ostream_operators::put()`
     （`utilities/ostream_operators.h:598`）与 `flush()`（同文件 `:701`）都在函数**第一行**
     取 `std::lock_guard guard(self.io_mutex())`；哨兵是 `try` 块内的局部量，构造与析构都在锁内，
     `catch` 也在锁内并调 `handle_exception`。不存在未加锁窗口。

137. **失败态下四个操纵符会追加 `strfailbit`，与库内其它插入/提取一致**（第 108 条只用
     `setw(-1)` 测过，而那本来就是 `strfailbit`，看不出是否**追加**）。用注入设备造出只有
     `devfailbit` 的流后实测：`<< endl` / `<< ends` / `<< "abc"` 都由 `out_sentry` 抛
     `stream_error`，`0x2` → `0xa`；`>> ws` 与 `>> std::string` 同样是 `0xa`。与
     `std::basic_ostream::sentry`（流非 good 时置 `failbit`）语义一致。

138. **字符类型矩阵正常。** `char` / `wchar_t` / `char8_t` / `char32_t` 四种流上 `<< endl << ends`
     都得到 `000a 0000`、`state=0x0`；`char` 设备 + `code_cvt_creator<char, char32_t>` 管线得到
     字节 `0a 00`。`char16_t` 流**根本无法构造**（`ctype_conf<char16_t>` 不存在，
     `locale<char16_t>` 的构造就编译不过）——这是全库限制，与这四个操纵符无关。

139. **TSan 追加组合无竞争。** 6 线程在同一个 `iostream` 上并发跑 `<< endl` / `<< ends` /
     `<< flush` / `>> ws`+`clear()` / **`locale(loc)` setter** / 无锁 `rdstate()`，各 400 轮，
     **0 告警**；同时跑了故意有竞争的对照组，确认检测器生效。第 44 / 83 条都不含
     `locale(loc)` setter，而 `endl` 的注释正是拿"锁外读 locale 即对其两张哈希表的数据竞争"
     来论证自己为何必须持锁——该论证成立。

140. **标准流对象走的是同一份加锁实现。** `__cout` / `__cerr` / `__clog` / `__wcout` … 经
     `stdout_api`（`io/objects/out_impl.h:26-30`），它同样派生 `ostream_operators<TChar>`，
     因此 `endl` / `ends` / `flush` 用的是同一份持锁的 `put()` / `flush()`。

## 六个格式状态操纵符（补充）

141. **`io_manip.h` 文件头"每个操纵符只是对某个原子成员的一次读-改-写"这一自我声明逐条成立**
     （`io/io_base.h`）：`setiosflags` → `m_flags.fetch_or`（284）；`resetiosflags` / `setbase`
     → 二参 `setf` 的 CAS 循环（308-315）；`setfill` → `m_fill.exchange`（416）；
     `setprecision` → `m_precision.exchange`（356）；`setw` → 判负**之后** `m_width.exchange`
     （392-394）。判负只读实参、不读共享状态，仍是单成员单次 RMW。

142. **`setf(fmtfl, msk)` 的 CAS 循环正确。** `desired` 表达式在 while 条件里，每次迭代随更新后的
     `old` 重新求值；`expected`（`old`）只在 `compare_exchange_weak` 内被改写，与实参求值不构成
     未定序修改，无 UB；弱 CAS 失败不写入，故不会发布撕裂的中间态；无丢失更新。`~msk` 的整型
     提升无害（`msk` 是 `uint16_t`，`old & ~msk` 只保留低 16 位再截回）。

143. **TSan 压力实测无竞争。** 5 线程 × 40000 轮并发施加 `setbase` / `set+resetiosflags` /
     `setw` / `setfill` / `setprecision` 与 `flags()` 读取，**0 告警**；掩码外的哨兵位
     （`showpos|uppercase`）全程未被 CAS 循环误清，`basefield` / `adjustfield` 从未被观察到多于
     一位。同时跑了故意有竞争的对照组，确认检测器生效。

144. **对 libstdc++ 的差分模糊测试零差异。** 48000 步随机操纵符序列（含 `setbase(0/2/7/-1/36)`、
     `setprecision(0..259)`、`setw(-20..179)`、随机组合掩码），每步比对 15 个共有标志位 +
     `precision` + `width` + `fill`，**mismatches = 0**，ASan+UBSan 无告警。

145. **私有的无约束 `apply`（`setbase_t` / `setprecision_t` / `setw_t`）不构成洞。** 它只被同类中
     受 `ostream_type` / `istream_type` 约束的 `swrite` / `sread` 调用；概念探测只看
     `swrite` / `sread` 两个名字，`apply` 不参与。非限定调用 `apply(s, f)` 也不会被 ADL 劫持
     （[basic.lookup.argdep]/1：普通查找找到类成员函数即抑制 ADL）。

146. **流形式与迭代器形式不会被误判。** 检测按**参数个数**：`insertable_with_iter` /
     `extractable_with_iter` / `extractable_with_ctx` 分别以 4 / 5 / 5 个实参探测，而六个操纵符的
     `swrite` / `sread` 只有 2 个形参，迭代器形式一律不可行；`iostream` 同时满足两个流概念也不
     影响，运算符里的 `static_assert(!(stream_v && iter_v))` 不会被触发。28 组 `requires` 矩阵
     （三种流 × 双向 × 三种值类别 × 字符类型错配）实测全部符合预期。

147. **`setfill_t` 的"抛出型拷贝构造"场景不可达。** `ios_base::m_fill` 是
     `copyable_atomic<TChar>`，`std::atomic<T>` 要求 `T` 平凡可复制，故任何能当 `char_type` 的
     类型其拷贝都不会抛；`same_as<T::char_type, TFill>` 又把别的 `TFill` 在声明层面挡掉。
     `TFill` 也不可能被推导为引用（工厂按值取参）。

148. **工厂形参的转换行为无意外。** `setprecision(3.0)` → 3、`setprecision('a')` → 97、
     `setprecision(255)` → ok、`setprecision(256)` / `(-1)` → `strfailbit`；`setw(3u)` /
     `(3ull)` / `('a')` / `(3.9)` 正常，`setw(SIZE_MAX)` → `strfailbit`；
     `setiosflags(fmtflags(-1))` → `flags=0xffff`、`resetiosflags(fmtflags(-1))` → `flags=0`；
     `setbase(0)` 只清 `basefield`（`0x1002` → `0x1000`，`skipws` 保留）。无窄化硬错误。

149. **`ios_defs::fmtflags` 是 `std::uint16_t`**（`io_base.h:71`），bit0..bit15 全部占满，
     `basefield = dec|oct|hex` 完整覆盖三个进制位。`setf` / `unsetf` / `width` / `precision` /
     `fill` 在 `ios_state`、三个 mixin 与三个流类里都没有同名成员，不存在名字隐藏导致调错重载
     （全仓 grep 确认）。`IOv2::sync` 确实存在（`io_base.h:1446`），文件头 `@warning` 的指引
     不是空引用。

## 货币与时间操纵符（补充）

150. **`io/traits/tm.h:137-139` 的约束比它调用的函数宽，但不可达。**
     `io_traits<TChar, time_parse_context<TChar,true,true,false>>::sread` 没有
     `steppable_back || is_istreambuf_iterator` 约束，而 `io_manip.h` 的 `get_time_t` 一侧有
     （第 123 条）。探针用自定义单趟迭代器实测：`get_time_t::sread` 约束 = 0、
     `time_parse_context::sread` 约束 = 1、`timeio::get` 实际可调用 = 0。但走不到：
     `istream_type<T>` 要求派生自 `istream_operators<TChar>`，而 `istream_operators::i_iter`
     （`istream_operators.h:1031`）函数体写死 `return istreambuf_iterator(self.m_streambuf,
     saw_eof);`，返回类型又写成 `TSelf::in_iter_type`。因此任何真能走到 `operator>>` 的流，其
     `in_iter_type` 只能是 `IOv2::istreambuf_iterator`；用户若把别名改成别的类型，`i_iter()`
     本身先编译不过。两种情形都只有编译期后果。

151. **`io_manip.h` 里 `get_time` 手写的中转路径与通用中转路径逐行等价。**
     `make_parse_context(*(f.tmb))` + `convert_to(*(f.tmb))` 对上
     `istream_operators.h:1170-1178`：同一个 `parse_context_type<TChar, std::tm>`、同一个
     `convert_to(std::tm&)`、写回目标同为 `std::tm&`。唯一差异是格式串——通用路径固定用 `'c'`。

152. **`get_time` 从空指针检查到回写之间无半写。** 可抛点是 `loc.get<timeio>()`、
     `make_parse_context`、`mp->get`、`convert_to`。前三者都不写 `*(f.tmb)`；
     `convert_to(std::tm&)`（`facet/timeio.h:1058`）先做 `convert_to(ymd)` / `convert_to(hms)`
     （两者都可能抛）再逐字段赋值，故抛出时目标一个字节都不被触碰。

153. **两个工厂的聚合初始化无生命周期问题。** `put_money_fn` / `get_money_fn` 的
     `return { mon, intl };` 是聚合初始化，引用成员直接绑到形参所引对象（不产生中间临时量），
     `bool` 无窄化；形参 `const TMoney&` 是**推导**形参，不发生用户定义转换。

154. **`swrite` 按值收 `put_money_t`、`sread` 按 `const&` 收 `get_money_t`，这个不对称无害。**
     `operator<<` 传的是 `const TValue&` 左值，拷贝聚合体只复制引用与 `bool`，被引用对象是调用方
     完整表达式里的对象，寿命覆盖整次调用。

155. **`f.fmt`（`const TChar*`）在 `mp->put` / `mp->get` 上不会误选单字符重载**：只有
     `basic_string_view<TChar>` 那条可行（`const TChar*` 无法转成 `char format`）。

156. **`get_time` 极值回退值大扫描零告警。** `tm_year` 取 11 个值（`INT_MIN` / `INT_MIN+1` /
     `-34667` / `-34668` / `-100000` / `0` / `30867` / `30868` / `32767` / `INT_MAX-1` /
     `INT_MAX`，含 `timeio::put` 年份边界两侧），其余六个字段各取 11 个极值（`INT_MIN` / `-1` /
     `0` / `1` / `11` / `12` / `31` / `32` / `366` / `367` / `INT_MAX`），配 33 个格式串（含
     `%a %A %j %U %W %V %G %u %w %c %x %X %D %F %T %R %e %n %t %%` 与空串）× 13 种输入，共
     **51909 次** `is >> get_time(&t, f)`：ASan + UBSan 零告警、无崩溃，17942 次成功解析。这覆盖
     了第 90 条之外的那段路径——极值 hint 经 `make_parse_context` 夹取后再进 `compute_ymd` 做
     周 / 星期 / 年内日推算；`year::max()/Dec/31` 附近的 `%a` / `%V` / `%j` 越界推算不会溢出，
     只以失败位收场。

157. **编码转换管线的提取侧正常**（第 68-71 条只测了插入侧，以及提取侧的 `ws` / `setbase` /
     `setw`）：`istream(mem_device<char>{...}, code_cvt_creator<char, char32_t>)` 上
     `>> get_time(&t, U"%H:%M")`、`>> get_money(long long&)`、`>> get_money(std::u32string&)`
     三条均正确，ASan/UBSan 干净。

158. **四个 `io_traits` 的 `requires` 与 facet 侧的实参约束逐项对齐。** `put_money_t` 的
     `(integral && !bool) || basic_string<TChar>` 对上 `monetary::put` 的
     `<std::integral TVal> requires(!same_as<TVal,bool>)` 按值形参（吸收 cv）与 string 重载，
     `volatile bool` / `const bool` 都被 `remove_cv_t` 正确排除；`get_money_t` 多的那条
     `same_as<TMoney, remove_cv_t<TMoney>>` 与 `monetary::get` 的 `TVal&` 形参（cv **不**被吸收）
     相符，`const int` / `volatile int` 目标被拒是正确的；`timeio::put` 无迭代器约束、
     `timeio::get` 有 `steppable_back || is_istreambuf_iterator`，两侧各自匹配。**不存在"本应
     可用却被拒"的类型**：`std::pmr::string`、自定义 allocator 的 `basic_string`、枚举、浮点在
     facet 侧同样不可用，约束与实现一致。

159. **`monetary::put` / `insert` 的缓冲与填充上界自洽。** 整型重载把 `char_type* p` 隐式转成临时
     `std::basic_string<char_type>` 再传给 `insert`，`p` 已由 `*--p = '\0'` 保证 NUL 结尾，
     `char_traits::length` 无越界；`insert` 开头 `const char_type* beg = digits.data();
     if (!(*beg == s_atoms[s_minus]))` 在 `digits` 为空时读的是 `data()[0]`（结尾 NUL），
     C++11 起良定义。填充上界：`throw` 判据用的 `len`（= value + sign + 可选 symbol）与
     `internal` 分支 `res.append(width - len, fill)` 是同一个 `len`，收尾补齐用
     `width - res.size()` 而 `res.size() >= len`，故两处填充量都 ≤ `max_pad_count`。

160. **`saw_eof` 不悬垂。** `operator>>` 的局部 `bool saw_eof` 地址交给 `i_iter`，`sread` 按值收
     迭代器，`mp->get` 内部的 `stamp_input_iterator` 包装与返回的 `res` 都在同一完整表达式内销毁。

161. **提取运算符的步骤顺序与 `get_time` 文档一致**：先 `i_iter` → 建 `in_sentry`（按 `skipws`
     跳空白）→ `eof()` 检查 → 才进 `sread` 做空指针校验。故文档里"已跳过的前导空白不退回"属实。

## 其它（补充）

162. **两个流迭代器的 `value_type` 都是 `TStreamBuf::char_type`**（`streambuf_iterator.h:82`
     与 `:402`），所以四个货币/时间 `io_traits` 里 `std::is_same_v<TChar, typename
     TIter::value_type>` 这条约束在插入与提取两个方向上都成立，不存在"输出迭代器 `value_type`
     为 `void` 而静默失配"的隐患。

163. **第 49 条列出的 `io_manip.h` 本地 include 组现已包含 `common/metafunctions.h` 与
     `common/streambuf_defs.h`**——`steppable_back` 与 `is_istreambuf_iterator` 由它们提供，
     被 `io_manip.h` 的 `get_time_t::sread` 约束使用（第 123 条）。组内字母序成立，二者都不是
     冗余 include。

---

## 载体类型的引用 / 指针语义与自别名（补充）

164. **`get_time_t::sread` 的 `make_parse_context(*tmb)` → `convert_to(*tmb)` 不构成自别名。**
     `make_parse_context` 把 `std::tm` 折成 `year_month_day` / `hh_mm_ss` 两个纯值后**按值返回**；
     `set_date_hint` / `set_time_hint` 只存标量；`time_parse_context` 的全部成员
     （含 `std::list<era_entry>`）都是自有值，无 view、无指针。`convert_to(std::tm&)` 先算局部
     `ymd` / `hms` 再逐字段写 `out`，写后不再读，所以"边写边读回退值"的顺序不存在。
     `fmt` 与输入迭代器都没有被存进 context。

165. **`swrite` 按值收 `put_money_t` 不会让 `m_mon` 绑到已死临时量。** 带引用成员的聚合体，
     其复制构造是复制引用本身、不物化临时量；`TDecay` 回退分支对这四个非数组非函数类型不可达。
     堆上被引用对象 + 表达式后立即 `delete` 的 ASan 探针干净。（补上了第 154 条未覆盖的一层。）

166. **找不到能让工厂的 `mon` 在完整表达式内提前失效的调用形式。** 穷举了纯右值、位域、
     `.operator()<T>()` 触发用户定义转换三类能物化临时量的形式，寿命都到完整表达式末尾，
     ASan 全干净。只有"存起来跨表达式用"才悬垂，那是文件头 `@warning` 的既定契约。

167. **`requires (std::is_same_v<TChar, typename TIter::value_type>)` 是 SFINAE 软失败**，
     不是硬错误；`steppable_back<TIter> || is_istreambuf_iterator<TIter>` 同样。裸指针与仅前向
     迭代器上 clang 21 与 g++ 15 结果一致。顺带确认 `get_money_t::sread` 不加 `steppable_back`
     是对的——`monetary::get` 本就不要求回退。

168. **`put_time(&t, nullptr)` 不编译**（`nullptr_t` 推不出 `TChar`）；显式写 `put_time<char>`
     可编译并干净落到 `io_manip.h:825` 的空指针检查，六种空指针组合全部得 `strfailbit`。

169. **`get_money_t::sread` 的写回是覆盖而非追加**，失败时目标零改动。

170. **输入在格式串走完之前耗尽时，`get_time` 置失败位且不写目标。** 截断输入不会带
     `succ == true` 走到 `convert_to` 写出半成品；实测比 `std::get_time` 更严格：整次失败且
     目标逐字节不变。

171. **`io_manip.h` 自足**：`parse_context_type<TChar, std::tm>` 特化不依赖调用方的包含顺序。

172. **`get_time` 的"回退值来源 == 写回目标"整条链路模糊测试干净。** 40000 轮、4 个 locale
     （含纪元数据）的 `make_parse_context → get → convert_to`，无 ASan / UBSan 报告，
     9652 次成功输出全部自洽。

173. **同形状的第二处"自别名"（`is >> std::tm`，走通用提取路径）同样安全。**

## 三个流类的构造、friend 与实例化（补充）

174. **私有的加锁委托构造函数未被任何合法表达式劫持。**
     （`istream.h:250`、`ostream.h:256`、`iostream.h:262`）它的第一形参是具体类型
     `const std::lock_guard<copyable_mutex<std::recursive_mutex>>&`，所以只有真正持有锁守卫的
     调用才可能选中它——"无约束两参模板会让友元里的 `is_constructible_v<S, X, Y>` 恒为真"
     这类担忧在当前签名下不成立。另经逐个通读三个类 friend 列表
     （`in_sentry` / `out_sentry` / `out_flusher` / 三个 mixin）的全部实现体，加上全仓 grep：
     库内**没有任何一处**用两个实参构造流对象，也没有 `is_constructible` /
     `construct_at` / `emplace` / `return {a,b}` 作用于流类型。

175. **friend 列表完整、实例化无死角。** grep 出 `m_streambuf` / `m_locale` 的每个类外访问点并
     逐个核对访问者，再对 16 个合法的 `流 × 设备 × 字符` 组合做**显式实例化**并把全部成员 /
     运算符 / 操纵符调一遍（含 `>> ws`、`<< endl/ends/flush`、`setw` / `put_money` / `get_time`、
     `tell` / `seek` / `attach` / `detach` / `tie` / `switch_to_*`、`i_iter` / `o_iter`、五个特殊
     成员、三个模板构造函数），g++ 与 clang++ 各跑一遍：0 error、0 `-Wreorder`，ASan/UBSan 干净。

176. **推导指引 × 受约束构造函数的组合矩阵：不匹配组合无一能编译通过。** 23 个用例逐个单编。
     所有不匹配组合（方向不匹配 + CTAD、显式写错 `TChar`、locale 字符类型与设备 / 管线不一致的
     二参与三参、`TChar ≠ 设备 char_type` 的默认构造）全部被拒，**首错都落在用户调用点，没有
     钻进 `cvt/`**；合法组合（既 get 又 put 的设备给 `istream`、creator 改变字符类型的二参 /
     三参）全部通过且 `char_type` 正确。私有构造函数**确实**参与 CTAD，但它推出的类型与拷贝候选
     相同，被选中即因私有而硬错误，不会静默推错。

177. **基类初始化顺序、`*this = std::move(tmp)`、以及派生类切片：无 UB。** 切片探针
     （ASan/UBSan）显示经基类引用赋值只是普通切片：派生成员保留、vptr 不变、`tie` 按文档清空、
     两条 `delete` 路径安全。一条通用风险（**非本库缺陷**）：`ostream` / `iostream` 的赋值运算符
     是 public，而 `std::basic_ostream` 的是 protected。

178. **`iostream` 的两个 mixin 不共享任何非虚公共基类，状态无重复副本。** 用 `-fdump-lang-class`
     （g++）与 `-Xclang -fdump-record-layouts`（clang）**实测布局**、而非只读代码，两编译器一致：
     `ios_base` / `ios_state` / `abs_flusher` / `stream_common_operators` **各一份**；两个 operator
     mixin 是无基类、无成员的空类，一个字节状态都不带，**不存在读写方向状态的双副本**。

179. **派生类路径下的全量实例化：`TSelf` = 派生类时同样无访问错误。**（deducing-this 的 `TSelf`
     换成用户派生类后重跑第 175 条的全量扫描，g++ 与 clang++ 均干净。）

180. **三个类移动赋值上的 `noexcept` 名副其实，且不是靠自身声明撑起来的。** 类内那条
     `static_assert(is_nothrow_move_assignable_v<...>)` 近乎同义反复（它只能挡住"有人把
     `noexcept` 关键字删掉"），因此另行逐一验证了移动赋值**函数体内每一步**的真实异常规格
     （`ios_base` / `base_streambuf` 都是 `= default`，故类型特征算的是真值），四组设备 / 字符
     全部通过——不存在"移动赋值抛出 → `terminate`"的隐患。

## `ws` / `endl` / `ends` / `flush` 的异常与析构路径（再补充）

181. **`out_sentry` 一律在 `try` 块**内**析构，析构抛出被同一层 `catch` 接住。** 三处构造点
     （`ostream_operators.h` 的 `put`、`write`、`operator<<` 迭代器形式）哨兵都是 try 块内部的
     局部量，不是函数级局部量。`flush()` 不建哨兵，无此问题。六条路径实测均以异常正常传播到
     调用者、无 `terminate`、ASan/UBSan 干净。

182. **掩码命中重抛的整条链路上没有锁泄漏、没有状态不一致，流仍可 `clear()` 后复用。** 8 条路径
     实测：抛出后从**另一个线程** `try_to_lock` 取 `io_mutex()` 均成功，本线程连续两次
     `try_to_lock` 也成功（递归锁计数已归零，说明 `endl` 的外层 `lock_guard` 与 `put()` 的内层
     `lock_guard` 都解开了）。`handle_exception` 被调两次确实幂等，不存在"两个异常同时在飞"的窗口。

183. **`ws` 里立即析构的 `in_sentry` 既不回退字符、也不可能在析构中抛。**
     `~in_sentry() = default;`（`istream_operators.h:158`），只有一个 `TStream&` 成员；跳空白由
     `sgetc()` / `snextc()` 推进取指针完成，析构里没有任何 `sputbackc`。连续三次 `>> ws` 幂等，
     不多吞也不回退。

184. **`endl` 的递归重入（endl 锁 → `put()` 锁 → 哨兵的 tie `try_flush()`）不会自死锁。**
     `try_flush()` 取的是**对方**流的锁且只用 `try_to_lock`，不形成等待边；自环与 3 节点环在
     `tie()` 设置时就被拒。实测 `a.tie(&b)`、`b` 的 `dflush` 注入抛出、两流全掩码，`a << endl`
     不抛、`a.rdstate()==0`、`b.rdstate()==2`，两把锁事后都空闲。

## 三个流类的对象生命周期与 tie（补充）

185. **tie 目标先于 tying 流析构 → 悬垂虚调用：是真实 UB，但文档已明文列为调用方责任。**
     `ostream` / `istream` / `iostream` 三者都能用 ASan 复现 `stack-use-after-scope`。
     `stream_common_operators.h:714` 起的 `@warning` 写的是**无条件**的生命周期契约
     （"让 `str` 所指的流存活于所有绑定到它的流之后……否则任何 I/O、以及任何一次实参非
     `nullptr` 的 `tie()` setter 调用，都会解引用悬空指针"），不是并发契约——
     **"文档只覆盖并发场景"的假设不成立。**
     ~~附带确认：链式 `A→B→C` 中 `C` 先死时 **`A << x` 是安全的**——刷新明确不沿 tie 链传播
     （`ostream_operators.h:667`），A 只刷 B、不碰 C。~~
     **1.0.1 起作废**：tie 改为链式（`out_tie_target::try_flush()` 先递归刷下游），`A << x`
     会依次刷 C、B，C 先死即为 UB。生命周期契约仍覆盖它（C 须活过 B），`tie()` 文档已改写
     为"整条链都会被走到"。

186. **`std::vector<ostream>` 扩容后 tie 悬垂：真实 `heap-use-after-free`，同属第 185 条的契约。**
     `stream_common_operators.h:727` 起已明确"**销毁之外，被移动同样会让一个 tie 目标失效**"。

187. **通过 `abs_flusher*` `delete` 一个 `ostream` / `iostream` 是良定义的。** `abs_flusher` 的
     虚析构是 public，`~ostream()` / `~iostream()` 亦 public 且隐式为虚，指针调整由
     deleting-destructor 完成，多重继承的子对象偏移不成问题。ASan/UBSan 0 告警，无
     `new-delete-type-mismatch`、无泄漏。`istream` 不派生 `abs_flusher`，既不能做 tie 目标也
     没有这条路径。

188. **`abs_flusher*` 在移动赋值后仍指向同一对象；派生自 `ostream` 的用户类型同样成立。**
     `out_flusher` 的模板实参**永远是 `ostream<TDevice,TChar>` 本身**（`ostream.h:24`），不是
     用户的派生类，所以 `static_cast<T&>(*this)` 只是一次静态下转型，用户再派生多少层都不影响。
     实测 `struct MyOs : ostream<...>`，`a = std::move(other)` 后 tie 指针仍等于 `&a`，经该指针
     虚派发 `try_flush()` 0 告警。（把第 74 条推广到了派生类。）

189. **tie 的"反向边"在目标被赋值后指向内容已换掉的流——是既定语义，不是 bug。**
     `b.tie(&a); a = std::move(other);` 之后 `b.tie() == &a` 仍为真，`b << "y"` 作用在 `a` 的
     **新**内容上，不崩不报错。文档已写明 tie 边"绑定于对象身份"；`a` 的身份没变。清空 tie 只
     发生在"**自己**被拷贝 / 移动"时，不会也不应该去清别人指向自己的入边。

190. **`A.tie(&B); B << "data"（未刷新）; B.detach();` 之后经 tie 触发的 `try_flush()` 既不丢
     数据也不 UB。** `detach()` 取回的设备里已含 `"data"`（detach 自己完成了刷新）；之后
     `a << "x"` 经 sentry 调 `b->try_flush()`，在移后的 `mem_device`（合法空设备）上成功返回。
     `file_device` 一侧查了源码而非推断：`dflush()` 以 `if (!is_open()) return;` 开头
     （`device/file_device.h:508-514`），`dput()` 以 `if (!is_open()) throw device_error(...)` 开头
     （`:468-480`），移后的 `unique_ptr<FILE>` 为空时不会把 `nullptr` 交给 `fflush` / `fwrite`，
     走的是常规 `devfailbit` 路径。（第 118 条的结论可推广到 tie 路径。）

191. **三个流类的特殊成员在基类 / 成员上没有遗漏。** 私有的加锁拷贝构造与移动赋值都覆盖了全部
     基类与全部数据成员，顺序与声明顺序一致，没漏掉 `out_flusher`（无状态但显式写出）。析构：
     成员先于基类销毁，`m_streambuf` 的析构负责把缓冲刷到设备，且它不引用任何基类子对象，这个
     顺序无隐患。

## 栈展开期间的错误上报

192. **栈展开期间 `handle_exception` 只置位、绝不抛出，因此库内不存在"析构里做 I/O 失败 →
     `std::terminate`"这条路。** `ignore_exception_mask == false` 时它先测
     `std::uncaught_exceptions() != 0`（`io_base.h:1103`，**在取 `io_mutex()` 之前**），成立就
     降为 `ignore_exception_mask == true` 的行为并吞掉一切抛出，加锁失败也一并吞掉。该判据在
     `catch` 处理器内是精确的：被处理的那个异常已不计入，外层仍在飞的异常才计入，所以在
     `catch` 块里做 I/O 依旧遵守掩码。
     判据集中在 `handle_exception` 一处，因此覆盖**所有**上报点，而不只是哨兵那一次刷新：
     实测在栈展开中的用户析构里，掩码全开且设备已失效时，`os << flush`、`os << setw(-1) << 42`、
     缺 ctype facet 的 `os << endl`、`is >> ws`（掩码含 eofbit、流已耗尽）、以及哨兵**构造**
     抛 `cvt_error` 的 appmode + UTF-8 组合，全部只置位、原始异常照常传播到外层 `catch`。
     真实 `IOv2::cout` + `::close(STDOUT_FILENO)` 亦可复现，clang++ 与 g++ 结果一致。

193. **该判据**不**覆盖 `clear(bits)` / `setstate()` / `unset_state()` / `exceptions()`。**
     它们是公开入口，抛出正是调用方索要的效果（与 `std::basic_ios` 一致），在展开期调用仍可能
     `std::terminate`。这是有意的边界，文档已写明：析构函数中请只经 `handle_exception` 报错。

194. **`~out_sentry()` 声明为 `noexcept(false)` 是必要的，且不需要自己判断是否在展开中。**
     正常路径上刷新失败要经 `handle_exception` 按掩码抛出，由发起本次输出的操作自身的
     try/catch 接住并传给调用者；展开路径上的抑制已由第 192 条统一完成，析构里重复判断只会
     引入第二处需要同步维护的策略。

195. **自赋值保持 tie 边不变。** `istream.h:241` / `ostream.h:246` / `iostream.h:252` 三处移动
     赋值与拷贝赋值都以 `if (this == &other) return *this;` 短路（移动赋值为了自赋值安全，拷贝
     赋值另有理由：move-only 内核上的拷贝必然抛出），故 `a = a` 与 `a = std::move(a)` 到不了
     `stream_common_operators::operator=`，`a.tie()` 保持原值——自赋值是恒等操作。这与第 34 条
     "四个特殊成员都置空"并不矛盾，后者说的是**非**自赋值的情形。新增流类型时须照此办理：要么
     同样短路自赋值，要么明确接受"自移动会解掉自己的 tie"这一差异。

## `put_money` / `get_money` 的全 locale 往返（补充）

196. **本机全部 316 个 UTF-8 locale 上的货币往返扫描无内存问题、无构造失败。**
     316 locale × `intl` 两态 × 8 个数值（`0` / `1` / `123` / `12345` / `-1` / `-12345` /
     18 位 / 30 位）共 5056 组 `put_money` → `get_money`，`-fsanitize=address,undefined`：
     **0 条 ASan / UBSan 报告**，**0 次 put 失败**，316 个 locale 全部可用
     `IOv2::locale<char>(name)` 构造（`std::locale` 亦然）。

197. **不存在"libstdc++ 读得回、本库读不回"的情形。** 上述扫描里 1334 组写出后读不回来
     （`intl=false` 640、`intl=true` 694），逐组把同一段文本交给 imbue 了同名 locale 的
     `std::istringstream` + `std::get_money` 复核，**1334 组无一例外也在 libstdc++ 上失败**。
     成因是 glibc 的 `int_curr_symbol` / 货币数据本身（第 119 条），不是本库的解析缺陷。

198. **put 侧与 libstdc++ 的差异全部落在两个已知类别里。** 5056 组里有 280 组字节不同，
     涉及 64 个 locale：其中 **264 组**是第 121 条那条规则——`mon_thousands_sep` 不是单个
     `char` 时本库整体关闭分组，而 libstdc++ 吐出该字符的首字节（产生非法 UTF-8）——
     两边数字序列完全相同，只有分隔符不同；其余 **16 组**全部来自 `C.utf8`，那里 libstdc++
     写出**空串**而本库写出数字。没有第三个类别，也没有任何一组的数字序列本身对不上。

---

## 六个格式状态操纵符（补充）

199. **`const` 载体对象在两个方向都走正确通道。** `const setw_t` / `const setfill_t<char>`
     左值插入时 `operator<<(T&, const TValue&)` 推导出 `TValue = setw_t`（cv 不进入推导），
     命中 `io_traits<char, setw_t>::swrite`；提取侧 `operator>>(T&, TValue&&)` 用
     `std::remove_cvref_t` 归一。全库 39 个 `io_traits` 特化之间无偏序歧义：唯二的无约束形状
     偏特化在 `traits/arithmetic.h` 的 `is_arithmetic_v` 与 `is_pointer_v` 两处，其约束都排除
     了六个载体类型。`setfill_t` 的字符类型匹配矩阵（`<OS, setfill_t<wchar_t>>`、
     `<OS, setfill_t<char8_t>>`、`<OS, setfill_t<const char*>>`、`<OS, setfill_t<int>>`、
     `<OS, setfill_t<signed char>>`、`<OS, setfill_t<char&>>` 等）全部在**声明**层面不可行
     （`same_as` 约束），无静默转换、无硬错误。

200. **移后（moved-from）流与已 `detach()` 的流上施加六个操纵符是安全的。** 它们只触碰
     `ios_base` 的原子成员，不建哨兵、不取 `o_iter()`、不触碰 streambuf / device，因此不存在
     空 streambuf 解引用的路径。实测两种流上六个操纵符均 `state=0x0`、`width` 正常写入，
     ASan + UBSan 无告警。

201. **`IOv2::sync` 作用域内操纵符抛出不会自锁死。** `sync` 构造时已持有 `io_mutex()`，
     失败路径上 `operator<<` 的 `catch` 调 `handle_exception` → `setstate` 会再取一次同一把锁；
     `io_base::m_io_mutex` 是 `copyable_mutex<std::recursive_mutex>`，递归重入合法。实测
     `sync` 作用域内 `os << setw(-1)`（掩码开、异常抛出）与 `os << setprecision(9999)`
     （掩码关、`state=0x8`）都正常返回、作用域正常退出。

202. **`iostream` 上双向操纵符不会同时命中两条路径。** 六个特化的 `swrite` / `sread` 都存在，
     但一条 `ios << setw(4) << setfill('_')` 与一条 `ios >> setw(3) >> setbase(8) >> …` 各自
     只生效一次，`operator<<` / `operator>>` 里的 `static_assert(!(stream_v && iter_v))` 未触发。
     已置失败位的流仍接受格式状态操纵符（流形式不建哨兵），与 libstdc++ 一致。

203. **抛出时被改的状态保持原值。** `ios_base::width` 在 `exchange` **之前**判负，
     `setprecision` 的 `apply` 在 `s.precision(...)` **之前**判界，故
     `os << setw(11)` 后 `os << setw(PTRDIFF_MIN)` 仍为 11、
     `os << setprecision(255)` 后 `os << setprecision(256)` 仍为 255，
     两条失败路径都不会留下半改的状态。

204. **`copyable_atomic` 确为真原子。** `load` / `store` / `exchange` / `fetch_or` /
     `fetch_and` / `compare_exchange_weak` 直接转发给 `std::atomic<T>`（默认 seq_cst），
     不是"用互斥量假装的原子"。因此 `io_manip.h` 文件头"每个操纵符只是对单个原子成员的
     一次读-改-写"这条自我声明成立：六个操纵符依次落在 `fetch_or`、二参 `setf` 的 CAS 循环、
     `m_fill.exchange`、`m_precision.exchange`、`m_width.exchange` 上，不加锁是自洽的。

205. **`basic_string` 提取端的 `setw` 没有 off-by-one。** `traits/char_and_str.h` 用
     `const size_t n = w > 0 ? w : str.max_size();` 配 `while (extracted < n ...)`，即**恰好**
     `width` 个字符；字符数组那条才是 `min(width, N)` 再取 `n-1`（给终止符留位）。
     （**【第十五轮订正】写法不精确**：`width == 0` 是「不设宽度」而非「宽度为零」，代进 `min` 得无意义结果；正确形式是 `width > 0 ? min(width, N) : N`，与第 437 条所写一致。实现本身正确，错的只是这处复述。）这与标准
     [istream.extractors] 对 `basic_string` 与 `charT(&)[N]` 的两套规则分别吻合。
     `io.width(0)` 在函数入口、任何可能抛出的步骤**之前**执行，因此该函数的每一条失败路径
     都消费 `width`；两个下游函数之外的抛出点各自显式清 width，判据见第 427 条。

## 货币与时间操纵符（补充）

206. **类型接受矩阵两侧一致。** 29 种类型 × put/get × 工厂 / `io_traits` / facet 三层编译期
     探测，**不存在"过了 `io_traits` 约束却在 facet 侧硬错误"的类型**。要点：`monetary::get`
     的签名其实能收 `const bool&` / `volatile bool&`（`TVal` 推成 `const bool`，
     `!same_as<TVal,bool>` 反而为真），函数体里 `units = tmp;` 才硬错误——`io_manip.h` 中
     `get_money_t` 那条 `same_as<TMoney, remove_cv_t<TMoney>>` 正好把这个坑挡在约束层。
     `char8_t` / `char16_t` / `char32_t` 的 `digits10+3` 栈缓冲手算均够；`__int128` 在
     `-std=c++23` 下 `is_integral_v` 为 false 故被拒，`-std=gnu++23` 下 `buf_size=41` 对
     39 位 + 符号 + NUL **恰好够**。`std::pmr::string` / 自定义 traits / 自定义 allocator /
     `string_view` / 枚举 / `float` / `long double` 两侧都拒，一致。
     这 29 型**不含**五个 C++23 扩展浮点类型；那五个类型的接受判据与承载类型转换见第 449 条，
     常驻矩阵已在 `test_io_traits.cpp` 里补齐。

207. **`put_time` / `get_time` 的 `TChar` 只从 `fmt` 推导。** `volatile` 与字符类型不匹配的
     实参都退化成别的 `put_time_t<其它>`，对应的 `io_traits` 特化不存在，**只有编译期后果**。
     非 const 数组与非 const 指针格式串运行期正确。

208. **`put_money` 的宽度路径无越界、无宽度错算。** 8 locale × 4 种 adjustfield ×
     `showbase` × `intl` × 8 个数值 × 7 个宽度 = **7168 次**，输出长度恒等于
     `max(width, 自然长度)`，流恒 `good()`，ASan / UBSan 零告警。特别核过 `monetary::insert`
     的 `internal` 分支在 `part::space` 与 `part::none` 两处都会填充的隐患——无 pattern 同时含
     这两个部件，不会二次填充。

209. **`get_money` 的"解析成功但零位数字"路径不可达。** 14 种畸形输入 × 2 locale × 2 种目标，
     **全部得失败位、目标一字节不变**，不存在"`good()` 却不写目标"。

210. **`iostream` 上货币与时间的写→`seek(0)`→读回逐项一致。** 异常掩码打开时三条抛出路径
     （`put_time` 空指针、`get_time` 空指针、`get_money` 溢出）都不死锁，`clear()` 后流可复用，
     溢出那次目标保持原值。

211. **同一完整表达式里多次调用工厂、实参为纯右值是安全的。** `os << put_money(f()) <<
     put_money(g())`：C++17 起 `<<` 的左操作数先于右操作数求值，每个临时量在被用到时都还活着。

212. **长词回退不撞缓冲边界。** 读缓冲 `m_read_buf`（`io/streambuf.h`）是按需增长的
     `std::basic_string`，回退量无固定上限（印证第 126 条）。

213. **全 locale 货币往返无静默改值。** 315 个 `*.utf8` × 3780 次 `put_money` → `get_money`：
     **0 次静默改值、0 次 put 失败、0 次抛出**；771 次 GETFAIL 全部是显式 `strfailbit`，
     且逐例定位为 locale 数据本身（pattern 以 `space` 部件开头）所致，与第 119、197 条一致。

## 三个流类本身（补充）

214. **构造失败不泄漏（带 creator 的路径）。** 用必抛的自定义 creator，把抛出点落在设备
     **已被移入** root kernel 之后，配**已打开**的 `ofile_device` / `ifile_device` /
     `file_device`，覆盖 4 个带 creator 的构造函数 + 5 组对照：`/proc/self/fd` 前后计数相等，
     ASan / LSan / UBSan 零告警。（第 45 条只用未打开的设备测过，够不着这条路径。）

215. **拷贝一个带脏缓冲的流不会把数据写两次。** 拷贝赋值里的 `tmp` 在
     `*this = std::move(tmp)` 之后才析构，那时它已被移空；且库内三种设备要么深拷贝
     （`mem_device`）要么禁止拷贝（`std_device`、`file_device`），"副本与原件写同一 sink"
     这一情形不可达。（第 115 条只测了移动赋值。）

216. **刷缓冲失败不 `terminate`。** 用让 `dflush()` 抛的自定义设备，在析构 / `noexcept`
     移动赋值 / 拷贝赋值三条路径上均正常返回。

217. **`switch_to_get()` 不丢未刷数据。** 改用**不直通**的设备（第 82 条全部用 `mem_device`，
     写直通，测不到 cvt 写缓冲）复测，结果与对照组逐字节相同。

218. **`switch_to_put()` 的两条抛出路径都不留半切换状态**（方向标志与读缓冲不会一边翻一边
     没清）；异常掩码打开时无锁泄漏，另一线程 `try_lock` 成功。

219. **45000 次随机特殊成员 / 方向切换 / tie 组合**，ASan + UBSan + LSan 零报告。

## `ws` / `endl` / `ends` / `flush` 与头文件卫生（补充）

220. **`iostream` 上 `>> ws` 与写入交替不丢字符。** 7 组读写交替实测，
     `in_sentry<..., true>` 的隐式切向不会造成"切了方向却没真跳空白"或"跳完又被切回"。

221. **`ends` 经编码转换管线正确。** `code_cvt_creator<char, char32_t>` 与 `<char, wchar_t>` 下
     U+0000 编码为单字节 `0x00`，不被当终止符截断，多字节字符前后不受影响，往返读回码点齐全。

222. **状态位 × 异常掩码全矩阵 448 组**（8 种前置状态 × 7 种掩码 × 4 个操纵符 × 3 种流）：
     不崩溃、不 `terminate`；每组结束后由**另一线程 `try_lock`** 验证锁已释放，448 组全部
     成功；`clear()` 后流可复用。补齐了第 108 / 109 / 137 条未覆盖的 `devfail`-only /
     `cvtfail`-only / `otherfail`-only 以及掩码含这三位的各档。

223. **栈展开期间 `os << ends` 安全**（第 192 条漏了这一个）。四操纵符 × 三流 × 四种状态在
     `noexcept(false)` 析构里执行，无一逃逸，原始异常完好；对照组确认掩码在非展开时确实会抛。

224. **四个头的包含顺序无关。** `io_manip.h` / `istream.h` / `ostream.h` / `iostream.h`
     的 **60 种全排列 × 2 编译器 = 120 次编译，0 失败**；全仓 include 图**环数 0**；
     与 `io/objects/*.h` 的 3 种顺序组合通过；三个流头**功能自足**（只包含 `ostream.h`
     即可 `os << endl << ends << flush`）。（第 53 条只测了"各自单独 include 能编译"。）

225. **四个操纵符对象双 TU 取址相同、`nm` 显示为 `u`**，无 ODR 隐患；`width` 不被四者消费
     （补上 `ws`），`flags()` 前后逐位不变。

## 跨文件：`get_time` 的纪元解析、C locale 名称识别、流类成员布局

这一组的实现落在 `facet/timeio.h`、`facet/timeio_details.h`、`facet/monetary_details.h`、
`facet/numeric_details.h`、`facet/facet_helper.h`，但可观察面在 `io_manip.h` 的
`get_time` / `get_money` 与三个流类上，故一并记在这里。

226. **`get_time` 只填格式串提到的字段；纪元推导只由 `%E` 说明符驱动。** 解析上下文的纪元
     条目表是**按需**填充的：只有解析到带 `E` 修饰符的 `%C` / `%y` / `%Y` 时才从 locale 的
     纪元表拷入，不含 `%E` 的格式串下它始终为空。年份推导链末端那条"取纪元起始年"的分支
     要求 `%EC` 真的匹配到了纪元名（`m_have_era`）。因此在带纪元数据的 locale
     （`zh_TW` / `ja_JP` / `th_TH` / `lo_LA` 及其 24 个编码别名）上，`%H:%M`、`%m-%d`、`%A`、
     `%d`、`%S`、`%U`、`%V`、`%X`、`%r`、`%G` 这些定不出年份的格式串**不会**改动 `*tmb`
     已有的年份，与 `std::get_time` 一致，也与 `io_manip.h` 中 `get_time` 的契约
     （"格式串中未出现的字段保留 `*tmb` 原有的取值"）一致。通用提取路径
     `is >> std::tm` 与 `is >> year_month_day` 走同一个 `compute_ymd`，同样一致。

227. **修饰符只作用于它所在的那一个转换说明，不沿格式串向后粘连。** `%EC %y` / `%EC %C` /
     `%EC %Y` / `%EC%Ey %C %y` 中不带修饰符的 `%y` / `%C` / `%Y` 按普通公历形式解析
     （分别得 1988 / 1900 / 1988 / 1988）；`%Oy` 走"替代数字符号"一支而非纪元一支。
     locale 无纪元数据时（`C`）`%EC` / `%Ey` / `%EY` 分别退化为 `%C` / `%y` / `%Y`。
     这两条正是 POSIX 的规定："E/O 修饰 field descriptor"，以及"替代格式在当前 locale
     不存在时，使用未修饰的 field descriptor"。用例在
     `test/facet/timeio/test_timeio_char.cpp` 的 `test_timeio_char_get_18`。

228. **`%EC` 与 `%EY` 的年份取自匹配到的那条纪元条目。** `ja_JP` 下 `%EC` 单独出现：
     令和→2020、平成→1990、昭和→1927；`%EY`：令和元年→2019、平成元年→1989、令和2年→2020、
     平成3年→1991。元年形式对应的纪元条目其格式串里没有 `%Ey`，年份只能来自纪元条目本身，
     因此 `%EY` 匹配成功后会把条目表收窄到"格式串与本次匹配到的那条相同"的条目。
     `%Ex` 展开到 locale 的纪元日期格式后同样正确（令和元年06月15日→2019-06-15）。
     纪元名前缀树与 `%Ey` 的 glibc 校验公式见第 57、117 条。

229. **C/POSIX locale 按**语言段**识别，`C.utf8` 一类拼法不再丢负号。**
     `FacetHelper::is_c_locale_name()` 取 `.` 与 `@` 之前的语言段比较，故 `C` / `POSIX` /
     `C.UTF-8` / `C.utf8` / `C.UTF8` / `C.UTF-8@euro` 得到同一份硬编码 facet 数据
     （正负号为 `"-"`，因此 `put_money("-12345")` 写出 `-12345` 并能原样读回）。
     语言段匹配但 `newlocale()` 不接受的名称（`C.BOGUS` / `C@euro` / `C.` / `C@` /
     `POSIX.utf8` / `POSIX@x`）仍抛 `cvt_error`；语言段不匹配的名称一律不归它认领。
     `monetary_conf` / `numeric_conf` / `timeio_conf` 的 7 个构造函数全部走这一判据。
     另有一道独立兜底：`lconv` 的货币字段整体不可用（`frac_digits` / `int_frac_digits` /
     `n_sign_posn` / `int_n_sign_posn` 均为 `CHAR_MAX`，且 `negative_sign` /
     `currency_symbol` / `int_curr_symbol` 均为空）时改用硬编码的 C 默认值——本机 890 个
     locale 中只有 `C` / `POSIX` / `C.utf8` 命中该谓词，无误判。这条取代了第 120 条里
     "只认精确的 `C` / `POSIX`"这一描述。

230. **三个流类的成员声明顺序是有意的分层顺序，且已写进类级注释。** `m_streambuf` 在前、
     `m_locale` 在后，故销毁时 `m_locale` 先、`m_streambuf` 后，`~root_cvt` 把残留缓冲冲刷
     进设备这一步始终作用在一个仍然完整的下层上。由此确定的依赖方向是**下层不得访问上层**：
     流缓冲区及其以下（转换器、设备）不得引用 locale。新增或改动这一层时须维持该顺序。

231. **私有的加锁拷贝构造函数不是模板。** 三个流类的 `X(const std::lock_guard<
     copyable_mutex<std::recursive_mutex>>&, const X&)` 形参类型是具名的——`io_mutex()` 返回
     `copyable_mutex<std::recursive_mutex>&`，调用点 `std::lock_guard{other.io_mutex()}`
     的 CTAD 结果正好是它。因此它不会参与任何 `is_constructible` / `constructible_from` 探测，
     也劫持不了任何合法调用（第 79、174 条）。

## 回归

232. **七个套件都已在真机上链接并跑通过。** `test_common` / `test_device` / `test_cvt` /
     `test_facet` / `test_locale` / `test_io` / `test_concur` 全部 rc=0，环境是原生
     aarch64 容器 `localhost/iov2-ci-arm64`（gcc 15.3.0）；同一轮里 `make all MODE=debug`
     exit 0、30 个公共头伞形 TU 在 `-Wall -Wextra` 下 0 error / 0 warning、37 个用到
     `size_t` 的头各自单独作 TU 全部通过。更早一轮 `make facet io locale MODE=sanitizer`
     也全部通过，ASan + UBSan + LSan 零告警。
     （**注意**：这次全套运行覆盖的是第 262、264 条所记的那部分代码。第 255、263、265-267 条
     所记的那部分只有编译与独立探针验证，七个套件未在其上跑过；第 233-244 条同样只有
     编译期探针与逐行对拍。**验证边界见末节。**）

## 位掩码类型、扩展点的可探测性与指针插入

这一组的可观察面在 `io_manip.h` 与三个流类上，实现落在 `io/io_base.h`、
`io/utilities/ostream_operators.h`、`io/traits/arithmetic.h`、`facet/numeric.h`。

233. **`ios_defs::fmtflags` 与 `ios_defs::iostate` 是两个互不相通的位掩码类型。**
     二者都是带固定底层类型的**非限定作用域**枚举（分别为 `std::uint16_t` /
     `std::uint8_t`），各自配齐 `| & ^ ~ |= &= ^=`，运算结果仍是自身类型，因此两族封闭：
     `iostate` 的值到不了收 `fmtflags` 的接口，反之亦然，整数也进不来——
     `setiosflags(ios_defs::strfailbit)`、`setf(0)`、`clear(fmtflags)` 一律编译不过。
     非限定使得 `if (state & eofbit)` 这类到 `bool` 的隐式转换、以及 `goodbit == 0`
     的写法与整数 typedef 时代完全一致。`~` 的结果按固定底层类型截断，是良定义的；
     `setf(fmtfl, msk)` 的 CAS 循环、`unsetf`、`clear` / `setstate` / `unset_state`
     的位运算均在底层整数上进行，出入口显式转换。掩码常量的数值与位序未变
     （`skipws | dec == 0x1002`，`adjustfield == left|right|internal`）。
     枚举量没有 `constexpr static` 命名空间变量的内部链接问题，头文件里不再是每个 TU 一份。
     断言在 `test/io/ios_state/test_ios_state.cpp`（`test_ios_state_bitmask_types_1`
     与其上的匿名命名空间）。`ios_defs` 中没有其它位掩码 typedef 待转换。

234. **`detail::insertable` 的答案与 `operator<<` 实际能否编译永远一致，cv 限定的载体也不例外。**
     `insertable_with_stream` / `insertable_with_iter` 各带两个类型参数：一个用于查
     `io_traits` 的**键**，一个用于构造调用实参的**实参类型**；衰退档传的是
     `<std::decay_t<const TValue&>, TValue>`，于是概念求值所用的实参表达式与运算符
     函数体真正传进去的是同一个。因此 `volatile setw_t`、`volatile setfill_t<char>`、
     `const volatile endl_t` 这些载体在 `requires { os << v; }` 下都答 `false`，
     写出来得到的是「无可行重载」而不是函数体内部的硬报错。提取侧 `operator>>`
     以 `std::remove_cvref_t` 归一并对真实实参求值，与插入侧同构（第 200 条）。

235. **流形式的 `io_traits` 把键的 `TChar` 绑到流的 `char_type`。** 全部 10 个流形式特化
     ——`io_manip.h` 的 `resetiosflags` / `setiosflags` / `setbase` / `setfill` /
     `setprecision` / `setw`，以及 `endl` / `ends` / `flush` / `ws`——都带
     `requires std::same_as<typename T::char_type, TChar>`（`setfill_t<TFill>` 上是
     `char_type == TFill && char_type == TChar` 三者一致）。于是
     `io_traits<wchar_t, endl_t>::swrite(narrow_os, endl)` 这类显式限定的错配调用是
     编译错误，而不是运行期的 `strfailbit`。其余特化（`char_and_str.h`、`arithmetic.h`、
     `nullptr.h`、`tm.h`、`put_money` / `get_money`）都是迭代器形式，本就以
     `TIter::value_type == TChar` 绑住；`put_time_t<TChar>` / `get_time_t<TChar>`
     连错配的模板实参都拼不出来。断言在 `test/io/traits/test_io_traits.cpp` 第 8 节。

236. **函数指针不进算术特化的指针一支。** `io/traits/arithmetic.h` 的指针特化要求
     `!std::is_function_v<std::remove_pointer_t<TValue>>`，因此漏写实参列表的
     `os << IOv2::setw`（函数名衰退成函数指针后只有 `numeric::put(bool)` 可行、
     会打出 `1`）是编译错误。作用于 `ios_base<char_type>&` 的用户操纵符走
     `operator<<(T&, void (*)(ios_base<typename T::char_type>&))` 这个专用重载
     （`io/utilities/ostream_operators.h:955`），不经过本特化，函数指针与函数左值两种
     写法都仍然可用。对象指针的取址路径不受影响（含指向 `volatile` 的，见第 239 条）。
     这条排除项还兼着挡住 `swrite` 里的 `static_cast<const volatile void*>`：函数指针与
     对象指针之间没有这个转换，少了排除项，漏写实参列表的 `os << IOv2::setw` 会变成**函数体
     里的硬错误**，而不是 SFINAE 看得见的失败，`operator<<` 的约束就挡不住它。
     断言在 `test/io/traits/test_io_traits.cpp`。

237. **插入侧的衰退键保留指向类型的 const。** 键取 `std::decay_t<const TValue&>` 而非
     `std::decay_t<TValue>`，故 `char buf[10]` 的键是 `const char*`——与实参在运算符
     形参 `const TValue&` 上实际衰退成的类型一致。`char_and_str.h` 里每个 `X*` 特化都有
     配对的 `const X*` 特化（`TChar*`、`char*`、`char,char*`、`unsigned char*`、
     `signed char*` 五对），所以换键没有丢失任何路径：`os_c << char[4]`、
     `os_w << char[4]`、`os_w << char*` 仍可插入，`os_c << wchar_t[4]` /
     `os_c << const wchar_t*` 仍然编译不过。

238. **指针的写出与读回同构于 `std::num_put` / `std::num_get`，并且往返无损。**
     `numeric::put(const void*)` 临时置 `hex | showbase`、清 `basefield | uppercase`，
     再按平台位宽选 `unsigned long` / `unsigned long long` 走通用整数写出路径，
     因而**会施加 locale 的数字分组**。这不是 IOv2 的自选动作：libstdc++ 的
     `num_put::do_put(..., const void*)`（`bits/locale_facets.tcc`）逐行同构——
     同样的 `~(basefield | uppercase)` 掩码、同样的 `hex | showbase`、同样的
     `sizeof(const void*) <= sizeof(unsigned long)` 条件类型、同样交给内部的整数写出。
     实测把 `std::ostream` imbue 成带分组的 locale 后，`std::cout << (const void*)p`
     给出同样形式的 `0x7ff,e78,ae0,dfc`。
     若比较时看到「IOv2 分组而 `std::cout` 不分组」，那只是**默认 locale 不同**：
     IOv2 的流按 `LC_ALL` > `LC_<category>` > `LANG` > `"C"` 从环境推导默认 locale
     （`locale/ori_facet_buf.h`，有意为之并已写进注释），而 `std::cout` 默认用经典
     locale（`numpunct<char>::do_grouping()` 返回空串，不分组）。`LC_ALL=C` 下两者
     逐字符相同。带分组的地址还能被 `numeric::get(void*&)` 原样读回（实测 `q == p`），
     往返不变量成立。**不要把这条当作缺陷重新报告。**

## cv 限定的键

这一组的实现落在 `io/traits/arithmetic.h`、`io/traits/traits_base.h`，可观察面在
`io/utilities/{ostream,istream}_operators.h` 的泛型运算符上。全部结论都与 `std::ostream` /
`std::istream` 在同一 locale（`LC_ALL=C`，两侧都 imbue 经典 locale）下逐字节对拍过，
编译器为 g++ 15 / libstdc++ 15；标量那一档的**标准侧**行为另在 clang 21 / libc++ 21 上
复核，两套库结论相同（该行为出自 [conv.lval] 的左值到右值转换，本就与库实现无关）。

239. **指向 `volatile` 的对象指针打地址。** 算术特化的指针一支在 `swrite` 里把实参显式转成
     `const void*`——`const_cast<const void*>(static_cast<const volatile void*>(value))`——
     再交给 `numeric::put`；两步缺一不可，`static_cast` 只能**添加** cv 限定，去掉
     `volatile` 只有 `const_cast` 做得到。于是 `volatile int*`、`volatile int[3]`、
     `volatile void*`、`const volatile void*` 都写出地址，而不是因为够不到
     `put(const void*)` 而经布尔转换写出 `1`。这与 C++23 P1147R1 给 `basic_ostream`
     添加的**非删除**重载 `operator<<(const volatile void*)` 是同一裁决。丢弃 `volatile`
     在这里是安全的：地址只被格式化，`numeric::put` 从不解引用它。这一步同时让所有指针类型的
     重载决议变成确定的精确匹配，不再依赖隐式转换序列。`setw` / `right` 在地址路径上照常生效，
     写出的地址能被 `is >> (void*&)` 原样读回（第 238 条）。

240. **指针一支的排除名单按 `remove_const_t<pointee>` 判，不是 `remove_cv_t`。**
     const 不改变一个指针算不算字符串指针，volatile 改变。因此 `volatile wchar_t*`、
     `const volatile wchar_t*`、`volatile char*`、`volatile char8_t*` 之类不被当作字符串
     指针，走第 239 条的地址路径；无限定与 const 限定的拼法不受影响，`wchar_t*` /
     `const wchar_t*` 在宽流上仍写字符串，窄流上的 `const wchar_t*` 仍然编译不过。
     标准到的是同一个地方：被删除的重载是 `const wchar_t*`，**不是**
     `const volatile wchar_t*`，后者落到 `operator<<(const volatile void*)`。
     撇开标准这条也成立——打字符串至少要两趟（先找终止符，再逐字符搬出），两趟之间 volatile
     内存可以变，先前量到的长度不再作数，于是截断或越界；`setw` 又要求先知道宽度再输出，
     两趟压不成一趟。指针值本身不带 `volatile`，打它一次 volatile 读都不需要，是唯一安全的
     答案。标量只读一次，不受此限：`volatile wchar_t` 在宽流上仍按字符走，在窄流上仍被
     那条 deleted 重载挡住。（本条判的是**指向类型**的 cv，第 241 条判的是**键本身**的顶层
     cv，两根轴互不相干。）

241. **带顶层 cv 的键与它的无限定拼法路由完全一致。** `arithmetic.h` 的算术特化与指针特化
     都带 `std::is_same_v<TValue, std::remove_cv_t<TValue>>`：`is_arithmetic_v` 与
     `is_pointer_v` 对顶层 cv 不敏感，而 `char_and_str.h` 里的字符与字符串特化敏感，不拒绝
     就会由算术特化抢答。拒绝之后 `operator<<` 用衰退档（`std::decay_t<const TValue&>`，
     第 234、237 条）重跑一遍偏序，这是唯一能复现无限定拼法所给答案的做法——那些特化是靠
     偏序赢的，本就不在任何排除名单上，往名单里加条目复现不出来。于是 `volatile char` 写字符、
     `volatile signed char` / `volatile unsigned char` 在窄流上写字符、在宽流上写数字、
     `char* volatile` 与 `const char* volatile` 写内容、`int* volatile` 与 `void* volatile`
     写地址，逐条与 `std::ostream` 相同。只有 `volatile` 会以这种方式到达键：`operator<<`
     的形参是 `const TValue&`，顶层 `const` 到不了键上。

242. **`volatile` 载体与 `volatile` 目标的答案与标准一致。** 类类型的 `volatile` 载体一律
     不可插——`volatile setw_t`、`volatile setfill_t<char>`、`const volatile endl_t`、
     `volatile std::string`、`volatile std::tm`：衰退档找得到特化，但把 `volatile` 类左值
     绑到成员的 `const T&` 会丢掉限定，非良构。标量逃得过这一条，是因为左值到右值转换把它
     读进一个临时量；类对象得逐成员读，正是第 240 条那个多趟问题。`const volatile` 的拼法
     不是另一条路径：`operator<<` 从 `const TValue&` 推导，它们到达时已经是普通的
     `volatile`。提取侧一律不接受 `volatile` 目标（`volatile int`、`volatile void*`），
     `std::istream` 同样没有这种重载。编译期断言在
     `test/io/traits/test_io_traits.cpp` 第 9 节，运行期那半在
     `test_ostream_inserters_arithmetic_{char,wchar_t}` 第 12 例与
     `test_ostream_inserters_character_{char,wchar_t}` 第 9 例——哪个特化答的，
     `detail::insertable` 看不出来，只有输出内容看得出来。

243. **`sread` 只收 `void*`，这处收窄由两处共同表达。** requires 子句里的
     `std::is_same_v<TValue, void*>` 钉住**键**，形参 `void*&` 钉住**目标**。只走
     `operator>>` 的话任一处都够——键是从目标类型推出来的，两者不可能不一致；但显式限定调用
     能把它们拆开（`io_traits<char, int*>::sread(...)` 配一个 `void*` 目标），那时只有两处
     齐备才挡得住。所以删掉任何一处都不会改变 `operator>>` 的行为，**不要据此以为它是冗余的**。
     `is >> charptr` / `is >> intptr` 没有 `sread` 可匹配，编译不过：放宽会把文本里的地址
     `reinterpret_cast` 进任意类型的指针（野指针而流仍 `good`），并静默接管 `char*`，
     把字符缓冲区的提取变成地址解析。插入侧放宽是安全的，`std::ostream` 的
     `operator<<(const void*)` 本来也通过隐式转换接住任意对象指针；提取侧没有这样的转换，
     `std::istream` 只有 `operator>>(void*&)`。

244. **`traits_base.h` 给用户的扩展点配方与库内实现同步。** 中英文两份配方的流形式都带着
     `requires std::same_as<typename T::char_type, TChar>`，并说明了为什么不能省：流类型是
     模板形参，不写它就与键的 `TChar` 无关，显式限定调用便能把宽键配窄流，本该编译期报错的事
     落到运行期的 `strfailbit`（第 235 条）。迭代器形式靠同一条值类型约束达到
     同一效果（写侧现在查的是 `iter_value_t`，见第 285 条）。照配方扩展自己类型的用户不会重新打开这个口子。

---

## 哨兵、方向切换与异常路径

这一组的可观察面在 `io_manip.h`（`ws`）与 `iostream.h` 上，实现落在
`io/utilities/istream_operators.h` 的 `in_sentry`、`io/streambuf.h` 与 `cvt/runtime_cvt.h`。

245. **`ws` 跳不跳空白与 `skipws` 位无关。** `in_sentry(TStream&, bool noskip)` 的第二个
     形参是 `noskip` 而不是 `skipws`，`ws` 传 `false` 即"跳"；哨兵构造函数从不读 `skipws` 位。
     所以 `is >> noskipws >> ws` 仍然跳空白，与 `std::ws` 一致（第 134 条）。

246. **`ws` 里那个 `catch(...)` 是承重的，不是冗余。** `in_sentry` 构造函数内部只把
     **跳空白那一段**包在 try 里；`if constexpr (involve_output) is.m_streambuf.switch_to_get();`
     **不在任何 try 内**，它抛的 `cvt_error` 直接从构造函数逃出、不经 `handle_exception`。
     `iostream` 的 `in_sentry_type` 正是 `involve_output = true`，所以 `ios >> ws` 在内核
     不支持 io_switch 时会走到这条路径。`io_traits<TChar, ws_t>::sread` 的 catch-all 把它
     接住并转成 `strfailbit`。删掉这个 catch 会让异常穿透到 `operator>>` 之外。
     这条路径与第 183 条不是同一条。

247. **`ws` 在 EOF 上只置 `eofbit`、不置 `failbit`，与 `std::ws` 一致。** 空掩码下的链条：
     跳空白块耗尽输入时 `throw eof_error{}` → 被同函数的 catch 接住 → `handle_exception`
     只置位、不抛 → 构造函数末尾的复查用 `operator bool`，而它对"只含 `eofbit`"返回 true
     → **哨兵构造成功**，随即在 `ws` 里立刻析构（`~in_sentry() = default`，不回退 `width`、
     不动状态位）。掩码打开时：`handle_exception` 重抛 → 逃出构造函数 → 被 `ws` 的 catch
     接住 → 再 `handle_exception`（幂等，同一 `ex` 同一 `at_eof`）→ 重抛给用户。
     所以 `in_sentry` 注释里的 `@throw eof_error` 只在 `eofbit` 落在异常掩码里时成立。

248. **EOF 之后 `iostream::switch_to_*` 不会被早返回悄悄跳过。** `operator bool` 对
     "只含 `eofbit`"返回 true，早返回不触发。

249. **`streambuf::switch_to_put()` 的抛出路径共四类，留下的状态都是自洽的。** 该函数按序做
     四件事：① `tell()` ② `m_cvt.seek(pos)` ③ `m_read_buf.clear()` ④ `m_cvt.switch_to_put()`。
     四类是：① `tell()` 自身抛（`dtell()` 失败）、② `seek()` 抛（`dseek()` 失败，含超量
     `sputbackc` 的组合）、④ 抛 `cvt_error`，以及**反向的 `switch_to_get()` 抛出**
     （`root_cvt::switch_to_get` → `flush()` → `dput` 失败）。
     ①② 抛出什么都没改（第 218 条）。**④ 也会抛**——`runtime_cvt` 在内核支持定位但不支持
     io_switch 时就在这一步抛 `cvt_error`——此时 ③ 已清空读缓冲、② 已回退物理位置，
     而方向仍是 input。但这不是"半切换"：`tell() = m_cvt.tell() - m_read_buf.size()`，
     调用前逻辑位置 = `P − N`，② 令 `P := P − N`，③ 令 `N := 0`，`tell()` **前后不变**；
     `runtime_cvt::switch_to_put()` 是先 `m_kernel.switch_to_put()` **成功之后**才写
     `m_io_status`，那一层也不会半切换；缓冲里那些字符来自设备、物理游标此刻正指着它们，
     后续读会重新取到。唯一取不回的是"超量 `sputbackc()` 压回的、从未真正读过的字符"，
     而这些在**成功路径**上同样丢（put-back 从不写穿），且已是明文声明的既定语义。
     所以除了状态位，④ 抛出与"这次调用从未发生"之间没有可观察差异。

250. **③ 排在 ② 之后这个顺序是承重的。** 正因为先回退物理位置再清读缓冲，失败时留下的
     才是一致状态；反过来（先 `clear()` 再 `seek()`）一旦 `seek` 失败，逻辑读游标就会凭空
     前移 `N` 个字符。第 249 条的推理依赖这个顺序，而源码里没有注释或断言锁住它——
     若日后要调整 ③④ 的先后，这条推理需要重做。

251. **`endl` 里取到的 `ctype` facet 不会悬垂。** `stream_common_operators.h` 的 locale
     getter 返回 `m_locale` 的 const 引用，但 `locale<TChar>::get<TF>()` **按值返回持有型
     `std::shared_ptr`**。即使 `m_locale` 被并发换掉，facet 对象仍被这个 shared_ptr 持活。

252. **委托拷贝构造 `X(std::lock_guard{other.io_mutex()}, other)` 抛出时不泄漏锁。**
     实测：抛出后**从另一线程**对源流与目标流的 `io_mutex()` 做 `try_lock` 均成功
     （递归锁在本线程 `try_lock` 会假阳性，必须换线程）；源流仍可 `put` / `flush`，
     `static_cast<bool>` 为真。这补上了第 29 条只做语言规则论证、未实测的那一半。

---

## 操纵符方向、取值检查与 `width` 归属

253. **六个格式状态操纵符的 `swrite` / `sread` 双向齐全；三个只有一向的是设计意图。**
     `setw` / `setprecision` / `setfill` / `setbase` 一类六对齐全，而 `endl` / `ends` /
     `flush` 只有 `swrite`、`ws` 只有 `sread`，所以 `ios << ws` 与 `ios >> endl` 是编译错——
     这是有意的方向约束，不是漏写。

254. **`setf(fmtfl, msk)` 是单条 CAS 循环，不是两次非原子 RMW。** desired 表达式写在
     `while` 条件里，每轮用更新后的 `old` 重算。

255. **精度的范围检查在 `ios_base::precision` 自己身上。** setter 取有符号的
     `std::ptrdiff_t`（与标准的 `std::streamsize` 一致），越界即抛 `stream_error`，
     而 getter 与存储仍是 `std::uint8_t`。形参非放宽不可：窄化发生在**调用点**，
     形参若就是 `std::uint8_t`，300 到达函数体时已经是 44，体内加检查根本看不到越界值。
     `ptrdiff_t` 覆盖完整、没有"绕回合法区间"的洞：`size_t → ptrdiff_t` 在 mod 2^64 下是
     单射（C++20 起有定义），`[0, PTRDIFF_MAX]` 原样保留，`(PTRDIFF_MAX, SIZE_MAX]` 全部落到
     `[PTRDIFF_MIN, -1]`，于是 `< 0 || > 255` 两端全收。`setprecision` 只保存原值、不查范围，
     因而与同文件的 `setw` 完全同构（形参 `ptrdiff_t`、字段 `ptrdiff_t`、`apply` 直接调 setter、
     检查归 `ios_base` 的 setter）。抛出的 `stream_error` 由 `ostream_operators.h` 的同一个
     `catch (...)` 接住交给 `handle_exception`，转成 `strfailbit` 并遵守流的异常掩码。

     代价已写进 `io_base.h` 的中英文注释：形参为 `std::uint8_t` 时，**常量**实参
     `precision(300)` 由 `-Woverflow` 报 "changes value from '300' to '44'"，配上本项目的
     `-Werror` 就是编译失败；加宽后 300 落在 `ptrdiff_t` 内，编译期无话可说，改由运行期抛出。
     即以常量那一路的编译期诊断，换运行期变量那一路不再被静默窄化——后者才是会漏进产品的那种。

256. **`width(std::ptrdiff_t)` 判负在 `m_width.exchange` 之前抛出，存储未被改动。**
     `setw(-1)` 不会回绕成巨大的 `size_t`（第 1 条），且失败后 `width()` 保持旧值。

257. **"`setw` 只能收紧提取边界"这条内存安全断言的承重前提是 `is >> char*` 编译不过。**
     `io_traits<TChar, TChar*>` **只有 `swrite`，没有 `sread`**。`std::istream >> char*`
     那个经典的缓冲区溢出脚枪，在本库是被类型系统关掉的，而不是靠 `setw` 兜。提取侧的界由
     `char_and_str.h` 保证：`num = min(width, N)`（**【第十五轮订正】写法不精确**：`width == 0` 是「不设宽度」而非「宽度为零」，代进 `min` 得无意义结果；正确形式是 `width > 0 ? min(width, N) : N`，与第 437 条所写一致。实现本身正确，错的只是这处复述。），循环条件 `extracted < num - 1`，
     终止符落在下标 ≤ `N-1`。

258. **`width` 的消耗按值类型分，不按方向分。** 全库 `width(0)` 共 7 处：`char_and_str.h`
     三处（`:20` `ostream_insert` 插入、`:46` `istream_extract` **提取**、`:524`
     `basic_string::sread` **提取**）、`numeric.h` 三处（`put(bool)`、`insert_float`、
     `insert_int`，均插入）、`monetary.h` 一处（`insert`）。即：**字符数组与字符串的提取确实
     消耗 `width`**（V12 九组实测确认，与 `io_manip.h` 里 `setw` 的注释一致），而算术提取、
     `get_money`、`get_time` 之后 `width` 仍然保留。
     （本条原写作"全部在插入侧"，方向写反了，已更正。）`put_time` 既不补齐也不消耗 `width`、
     把它原样留给下一次插入，与 `std::put_time` 一致（`time_put` 同样不看 width）。

259. **`get_money_t::sread` 与 `get_time_t::sread` 的约束差异是对的，不是漏写。**
     两者各自**精确镜像自己的 callee**：`timeio::get` 带
     `steppable_back || is_istreambuf_iterator` 这条约束，`monetary::get` 不带。

260. **`put_money_t` / `get_money_t` 的 cv 判据写法不同但等价。** `get_money_t` 的 requires
     首项就是 `same_as<TMoney, remove_cv_t<TMoney>>`，此后 `TMoney` 已无 cv，两种写法一样。
     `put_money_t<const int>` 可用而 `get_money_t<const int>` 被拒是对的——后者会持有
     `const int&`，拒掉才是更好的诊断。

261. **`timeio::put` 的字段校验里没有有符号溢出。** `||` 链先查
     `t.tm_year > year::max() - 1900`，再算 `t.tm_year + 1900`。即使 `tm_year == INT_MIN`，
     `INT_MIN + 1900` 是朝零走，仍在 `int` 范围内。

---

## 头文件卫生、默认构造约束与不抛保证

262. **`include/` 里 `size_t` 一律写作 `std::size_t`。** 标准只保证 `<cstddef>` 提供
     `std::size_t`，`::size_t` 由实现附赠。libstdc++ 下两者是同一类型
     （`static_assert(std::is_same_v<::size_t, std::size_t>)` 通过），所以重载决议、
     模板推导、ABI 全不变——这是拼写。注释与字符串字面量按原样保留，`facet_common.h`
     那条 static_assert 消息里的 `std::bit_cast<size_t>` 是有意留的。`test/` 未纳入，
     且与此零耦合（没删任何 include，`::size_t` 在测试里照样可见）。

263. **`include/` 里每个头都自包含。** 62 个头各自单独作 TU 编译通过、零告警；用到
     `std::size_t` 的头都直接 `#include <cstddef>`，靠传递包含拿声明的已经没有。
     `io_manip.h` 不再 include `<limits>`——它的唯一用户随第 255 条的检查一起移走了。

264. **流类不会谎称自己可默认构造。** `istream` / `ostream` / `iostream` 的默认构造函数
     除 `char_type` 匹配外还要求 `std::is_default_constructible_v<TDevice>`，不可默认构造的
     设备被**约束排除**，而不是在函数体里硬错。其余四个构造函数都按值收 `TDevice`、
     无默认实参，没有一个能零实参调用，所以 `std::is_default_constructible_v<istream<D, char>>`
     答的是实话。`ifile_device` / `ofile_device` / `file_device<char>` 与 `std_device`
     （有 `= default`）支撑的流仍可默认构造，`cout` / `cin` 不受影响。

265. **16 个可证明不抛的访问器带 `noexcept`。** 三组：免锁 getter（`flags`、`precision`、
     `width`、`rdstate`）；建在 `rdstate` 上的谓词（`good`、`dev_fail`、`cvt_fail`、
     `str_fail`、`other_fail`、`eof`、`operator bool`）；全身只有一次原子操作的 mutator
     （`flags(fmtfl)`、`setf(fmtfl)`、`setf(fmtfl, msk)`、`unsetf`、`fill(TChar)`）。
     这些函数体里出现的每个操作本身就是 `noexcept`：`copyable_atomic` 的
     `load` / `exchange` / `fetch_or` / `fetch_and` / `compare_exchange_weak`，以及 `ios_defs`
     的位运算符（全是 `constexpr ... noexcept`）。`fill(TChar)` 按值收参，但
     `copyable_atomic<TChar>` 内含 `std::atomic<TChar>`、要求 `TChar` 可平凡复制——
     凡是能实例化的 `TChar`，其复制就不会抛。

     **有意不标的**：`precision(std::ptrdiff_t)` / `width(std::ptrdiff_t)` 会抛
     `stream_error`；`exceptions() const`、`clear()`、`exceptions(e)`、`setstate()` 都要取
     `io_mutex()`，`lock()` 本身可抛 `std::system_error`，且后三者的重抛正是设计意图。
     `io_base.h` 无 `virtual`，故无 override 的 noexcept 兼容问题；全仓无对这些成员取
     成员函数指针（`noexcept` 会改函数类型）；`ios_base<void>` 不含这些成员。

     **代价**：`noexcept` 是对未来的承诺——日后若给某个 mutator 加值域校验（正如
     `precision` / `width` 各自带的那种），就会撞上 `std::terminate` 而不是抛出。
     五个 mutator 里 `fill` 无可校验的值域，另四个收的是位掩码，风险最低但不为零。

266. **`switch_to_*()` 在失败态下静默返回，与 `tell()` 一致、与 `flush()` 相反。**
     `tell()` 失败静默返回 `nullopt`；`flush()` 失败**抛** `stream_error`，经
     `handle_exception` 加 failbit。`iostream.h` 的中英文注释都只援引 `tell()`。

267. **`precision` / `width` 的 getter 与 setter 各有自己的文档块。** `@throw` 只挂在会抛的
     那个重载上，getter 是 `noexcept`。`io_base.h` 里其余带 `@throw` 的块（`xalloc`、
     `clear` / `setstate`）都不是 `noexcept`，无同类问题。

---

## 货币与时间操纵符的失败路径

268. **`get_time::sread` 在 `mp->get` 之后无条件 `convert_to` 是安全的。**
     `facet/timeio.h:1806-1810`：`timeio::get` 的唯一失败通道是 `do_get` 的 out 参数 `succ`，
     `get` 把它转成 `throw stream_error`，不存在"正常返回但解析失败"的路径，所以
     `io_manip.h:1016` 只在成功后到达。探针 14 组（中途字面量不匹配、字段内提前 EOF、
     字面量前 EOF、字段越界、日期不存在、闰秒、`%Z`）加 40000 轮模糊测试
     （`parse_fail=32447`，`mutated_on_fail=0`）：失败路径上 `*tmb` **逐字节不变**，
     与 `io_manip.h:887-889` 的承诺一致。
     `convert_to` 本身也不会半写：`timeio.h:1059-1088` 先把 `ymd` / `hms` 算进局部量
     （唯一抛出点 `ymd.ok()` 在任何 `out.tm_*` 写入之前），再逐字段落盘。

269. **`put_money` / `get_money` 载体的引用成员不悬垂。** `const TMoney&` 是推导形参，
     无转换、无需生命周期延长，纯右值临时量活到完整表达式结束。ASan（含
     `detect_stack_use_after_return=1`）干净。`get_money(std::string{})` 按设计编译不过。
     两者 `requires` 的不对称是必需的：`const TMoney&` 推导会吸收 `const` 但保留 `volatile`，
     故只需 `remove_cv_t` 做 bool 排除；`TMoney&` 两个 cv 都保留，故两条都要。
     全部拒绝均 SFINAE 友好（落到主模板 → 无 `sread` → 无可行重载），不是硬错误。

## 格式状态操纵符的取值边界与无锁前提

270. **巨大 `width` 不会导致 `bad_alloc`、死循环或越界。** 插入端五条填充路径
     （`char_and_str.h:24`、`numeric.h:289/851/970`、`monetary.h:735`）**在分配之前**
     都检查 `max_pad_count`（65536），无遗漏。`setw(1e9)` 与 `setw(PTRDIFF_MAX)` 各配
     7 条插入路径共 14 组实测：全部 `strfailbit`、零输出、耗时 ≤3ms。

271. **`setiosflags` 造出的非法标志组合不会 UB 或死循环。** `numeric.h` 的四个分派点
     全是穷尽 if/else，**无一处按标志值查表索引**；`m_out_atoms` 的索引由数值本身算出，
     且有 `static_assert` 钉死。12 组含全置位的实测得到 `state=0x0`，ASan+UBSan 干净。
     `setf(fmtflags(0), mask)` 的两种读法恒等（CAS 算的是 `(old & ~msk) | (0 & msk)`），
     `resetiosflags` 与 `setbase` 共用它，没有一方被写错。

272. **四个 setter"不加锁"的前提成立。** 逐行核对：`setf(f,msk)` 是 CAS 循环、
     `setf(f)` 是 `fetch_or`、`fill` / `precision` / `width` 是 `exchange`，全是单次原子 RMW；
     值域检查只读**实参**、不读原子成员，不构成"先读再写"。

## 三个流类的特殊成员与生命周期（再补充）

273. **移动赋值在持锁期间把锁所在的基类整个赋值掉，不是 UB。** `io_mutex()` 是
     `ios_base<TChar>::m_io_mutex`（`io_base.h:778`，getter `:591`），`ios_state::operator=`
     确实会走到它；但 `copyable_mutex` 的拷贝/移动赋值都是**空函数体**
     （`common/copyable_mutex.h:79/81`），底层 `TMutex` 不被重构造、覆盖或 swap，
     递归计数不动。实测：外层持锁把递归计数抬到 1，跨移动赋值与拷贝赋值后另一线程
     `try_lock` 仍失败、出作用域后成功，互斥量地址不变；叠加另一线程抢锁做 20000 轮，
     ASan/UBSan 零告警。

274. **委托拷贝构造的锁覆盖全程。** `io_mutex() const` 因成员 `mutable` 返回非 const 引用；
     依 [class.base.init]/11 与 [class.temporary]/4，`lock_guard` 临时量在被委托的构造函数
     返回之后才析构。运行期钩子实测三个类均"成员拷贝期间持锁=1、结束后=0"。自赋值短路
     六处齐全（`istream.h:290/342`、`ostream.h:297/351`、`iostream.h:304/359`）。

275. **`iostream` 没有菱形继承，读写两侧是同一把锁。** `istream_operators`（`:474`）与
     `ostream_operators`（`:557`）**无基类、无成员**（`is_empty_v` 双双通过）。实测 `sync`
     持锁时另一线程的 `put()`（out_sentry）与 `get()`（in_sentry）**都被阻塞**，不持锁的
     对照组立即完成——两个哨兵确实互斥。

276. **`~ostream()` / `~iostream()` 期间没有回调派生类的路径。** `stream_common_operators`
     只有一个成员 `copyable_atomic<abs_flusher*> m_tie_stream{nullptr}`
     （`stream_common_operators.h:900`），析构 `= default`；它访问 `m_streambuf` 的十处
     **全部**是 deducing-this 的 `self.m_streambuf`，构造/析构里一次都没有，不持有任何指针。
     层次依赖亦已核对：`cvt/`、`device/`、`streambuf*.h` 无一持有 `IOv2::locale<TChar>`，
     所以 `m_locale` 先于 `m_streambuf` 析构不会悬垂。

277. **三个文件的 `requires` 与推导指引归一化后逐字符 `diff` 完全一致。** 每个类探测的管线
     类型（`istreambuf` / `ostreambuf` / `streambuf`）与自己的方向严格对应，无复制粘贴错位。

## `ws` / `endl` 的锁与方向（再补充）

278. **`endl` 里的 `os.locale()` 不悬垂。** getter 返回 `const auto&` 绑定到成员
     （`stream_common_operators.h:598-602`），不产生临时 locale；`ctype::widen` 是自有的
     256 元素查表（`facet/ctype.h:614`），不回指 locale。moved-from 流上 `get<>()` 返回空
     `shared_ptr`，走 `ostream.h:474-475` 的显式抛出，不解引用空指针。

279. **`ws` 不产生多余的方向切换，也不丢缓冲输入。** `~in_sentry() = default`
     （`istream_operators.h:158`），析构无切回动作；`runtime_cvt::switch_to_get()` 首行
     `if (m_io_status == io_status::input) return;`（`runtime_cvt.h:413`），已是读方向时为
     空操作。实测（含先 `peek()` 灌满读缓冲再 `>> ws`）位置逐字节正确。EOF 上 `ws` 只置
     `eofbit`（`state=0x1`、`bool=1`），与 `std::ws` 一致，`is >> ws >> x` 不会静默失效。

280. **哨兵存活期间同线程切方向的路径存在，但无损。** 迭代器形式 `swrite` 的第二个实参传的
     是整个流（`ostream_operators.h:886-887`），用户把形参写成模板 `TIo&` 即可拿到
     `iostream&`（库自带特化都写 `ios_base<TChar>&`，碰不到）。实测自愈：`streambuf` 每个
     读写入口按需切向，`runtime_cvt` 两个 `switch_to_*` 都有 `m_io_status` 幂等短路，
     `~out_sentry()` 的刷新内含 `switch_to_put()`。

## `switch_to_put()` / `switch_to_get()` 抛出路径的运行验证

281. **第 249 条列出的四类抛出都能实际注入出来，且六项断言逐项实测成立。**
     四类的造法：① 注入 `dtell()` 失败；② 注入 `dseek()` 失败（含超量 `sputbackc` 的组合）；
     ④ 用 `no_switch_cvt`（`support_positioning` 真、`support_io_switch` 假，`static_assert`
     锁定）抛 `cvt_error`，以及现成内核 `code_cvt<char,char32_t>`（变长编码未到 EOF 时
     `code_cvt.h:1005-1019` 抛）——后者是多字节内容上 ②③ 已执行、④ 才抛的真实场景；
     `switch_to_get()` 则经 `root_cvt::switch_to_get` → `flush()` → `dput` 失败。
     实测结论：`tell()` 不变式成立（1→1、1→1、0→0，并用"切换期间 `dseek` 调用次数"计数
     证明 ② 确实执行过，不是在 ② 之前就失败的假阳性）；状态位恰好一位，无多置漏置；
     `clear()` 后读回的内容与"从未调用过"的孪生流逐字节相同；方向仍是 input，二次调用
     幂等地再抛；`clear()` 后可正常读写（失败后写入落点 `"aZcdef"` 与无失败对照组一致）；
     异常掩码打开时跨线程 `try_lock` 成功，无锁泄漏。
     20000 轮随机注入中 `switch_to_put` 失败 6087 轮，`tell()` 不变式**每轮都成立**，
     6087 个失败轮次的读回内容全部与未受扰动的孪生流比对通过；g++15 与 clang++21 各跑一遍，
     ASan/UBSan/LSan 零报告。

282. **两类失败在当前结构下构造不出来，原因是结构性的。**
     `switch_to_get()` 抛 `cvt_error`：`bos()` 把状态同步为 input，而离开 input 只能靠必然
     先抛的 `switch_to_put()`——input 是吸收态，故永远走幂等早返回。
     `root_cvt::switch_to_put` 内部 `seek(tell())` 失败：`iostream` 的根转换器
     `HasInBuffer == false`，input 方向恒有 `m_buf_cur == m_buf_end`，该分支恒不进入。

283. **第 250 条依赖的 ③ 排在 ② 之后这个顺序当前成立**（`streambuf.h:684` 的 `seek` 在
     `693` 的 `clear` 之前）。历史上 `1c57a715` 那次调换发生在 `seek()` / `rseek()` 两个函数里，
     方向与第 250 条的要求一致。源码里仍无注释或断言锁住这个顺序。

## 跨文件：状态位重抛、写侧迭代器约束与货币缓冲区

284. **`clear()` 的重抛优先级是写进契约的行为。** 它按 设备 → 转换 → 流 → 其他 → EOF 的固定
     顺序，在"该位既已置位、又落在异常掩码中"时重抛该类别保存的原始异常
     （`io_base.h:871` 中文、`:891-898` 英文明文规定，实现在 `:929-953`）。因此当更高优先级
     的失败位已满足这两个条件时，`handle_exception` 传播出去的是那个类别的异常而不是本次
     传入的 `ex`——流上已有 `devfailbit` 时把哨兵的 `stream_error` 交进去，抛出的是先前那条
     `device_error`（实测 `state=0xa`，两位都对，无泄漏无 UB）。这是刻意的：先报因果链上游
     那个更根本的故障（哨兵的 `stream_error` 本身是"流已坏"的后果）。被让位的
     `exception_ptr` 不会丢失——`clear()` 先释放被清除位的指针（`:921-923`）再决定重抛，
     上例中 `m_exp_dev` 被 `std::exchange` 消费而 `m_exp_str` 原封不动，待 `devfailbit` 被
     `unset_state` 清掉时 `stream_error` 照样抛出。`handle_exception` 的文档块已就"传播出去
     的异常未必是传入的 `ex`"单列 `@note`。

285. **写侧的迭代器约束是 `char_sink_for<TIter, TChar>`（`io/traits/traits_base.h`），
     读侧是 `std::is_same_v<TChar, typename TIter::value_type>`。** 两侧不对称是有意的：
     输出迭代器不必有 `value_type`——C++20 起
     `std::back_insert_iterator`、`front_insert_iterator`、`insert_iterator`、
     `ostream_iterator`、`std::ostreambuf_iterator` 的 `value_type` 一律是 `void`，
     `std::output_iterator` 概念也不要求该 typedef 存在。`char_sink_for` 因此照搬 `<format>`
     的 `std::output_iterator<TIter, const TChar&>`（`/usr/include/c++/15/format:782` 等处
     即此写法），再加一道本库特有的守卫：`std::iter_value_t<TIter>` **若能求值**就必须等于
     `TChar`。全部 22 个写侧约束点用的都是它（`char_and_str.h` 16 处含 `ostream_insert`、
     `io_manip.h` 2、`arithmetic.h` 2、`tm.h` 1、`nullptr.h` 1）。
     实测行为矩阵：`IOv2::ostreambuf_iterator` 宽窄错配被拒、`wchar_t*` 配 `TChar == char`
     被拒（此处比 `std::format_to` 严）、写不进去的汇被拒；`back_insert_iterator<std::string>`、
     `std::ostreambuf_iterator<char>`、`char*` 放行。
     **已知边界**：对报不出字符类型的汇（`iter_value_t` ill-formed 或为 `void`）只剩可写性
     一关，宽窄错配会经隐式转换静默通过——窄 facet 写进 `std::wstring` 得到"把字节当字符"的
     伪宽串，宽 facet 写进 `std::string` 逐码元截断。两者都不是 UB、不越界、不崩溃，只是
     字符损坏，且 ASCII 部分看着正常。这与标准一致（`std::format_to`、`std::copy` 到
     `back_inserter` 皆然），也与本库 facet 层一致——`facet/` 下的 `put` / `get` 模板完全
     没有迭代器约束（如 `facet/monetary.h:390` 是裸的 `template <typename TIter, ...>`），
     所以这条守卫拦的是 `io_traits` 这一层，不是整个系统。
     **措辞更正**：读侧那条不能说成"输入迭代器按标准必须有 `value_type`"。`std::input_iterator`
     要求的是 `iter_value_t` 可求值，不是有嵌套 `value_type`；**裸指针**就是反例
     （`std::input_iterator<char*>` 为真、`iter_value_t<char*>` 为 `char`，却没有
     `char*::value_type`）。读侧查的正是嵌套成员（`io_manip.h`、`arithmetic.h`、
     `char_and_str.h`、`tm.h` 各处），故 `char*` 在读侧不可行而在写侧可行。后果只在编译期且
     不可达（`operator>>` 永远只传 `istreambuf_iterator`，第 150 条已证明这是结构性的），
     不是缺陷；`traits_base.h` 里同样写法的四处说明已一并改正。

286. **`monetary::put` 的 `digits10 + 3` 是精确正确的缓冲区大小，不是"恰好够用"的隐患。**
     有符号类型所需 = 1 个符号 + (digits10+1) 位 + 1 个 NUL = digits10+3，实测每个类型都
     严丝合缝（`short` 4→7 需 7、`int` 9→12 需 12、`long long` 18→21 需 21、
     `__int128` 38→41 需 41）。这是构造上的恒等式而非巧合：`|min|` 与 `max` 的十进制位数
     必然相同，因为 `2^k−1` 不可能是全 9 的十进制数。无符号类型省掉符号位，反有 1 字节余量。
     另：`-std=c++23` 下 `put_money(__int128)` 编译失败，机制是 `std::integral<__int128>`
     为假（实测 `is_integral_v<__int128> = 0`）使 `io_traits<TChar, put_money_t<TMoney>>` 的
     `requires` 不满足，与缓冲区无关——同一模式下 `numeric_limits<__int128>::is_specialized`
     仍为 1、`digits10` 仍为 38。`-std=gnu++23` 下 `is_integral_v` 为 1，可正常使用。

## 回归（当前工作树）

287. **七个套件在当前工作树上全部跑通。** `make MODE=sanitizer clean` 后全量重建
     （g++15，`-Wall -Werror -Wshadow -std=c++23 -g -O1 -fsanitize=address,undefined`，
     174 个 TU，**零告警**），七个可执行文件依次运行：

     | 套件 | 通过 | 退出码 | sanitizer 报告 |
     |---|---|---|---|
     | `common` | 54 | 0 | 0 |
     | `cvt` | 257 | 0 | 0 |
     | `device` | 100 | 0 | 0 |
     | `facet` | 527 | 0 | 0 |
     | `locale` | 45 | 0 | 0 |
     | `io` | 353 | 0 | 0 |
     | `concur` | 18 | 0 | 0 |

     合计 **1354 项全部 Done**，ASan / UBSan / LSan 零报告、零泄漏。
     该数字是 2026-08-14 的快照，此后套件有增补，见第 319 条。

---

## 四个文件的卫生与退化实参（第二轮全量核对）

288. **四个文件的空白字符与文件末尾换行全部干净。** 行尾空白 0、连续空行 0、尾随空行 0，
     四个文件均以单个 `\n` 结束。（第 46、47 条的复核，结论一致。）

289. **include 分组与排序符合规则，且无冗余。** 四个文件都是「本地头 → C++ 库头」两组、
     组内按完整路径字典序；`<cstddef>` / `<cstdint>` / `<ctime>` 归入 C++ 组参与排序，
     没有被误当成 C 头单列。逐个核对「包含了但没用到」：`io_manip.h` 的 `<concepts>`
     （`std::integral` 2 处、`std::same_as` 19 处）、`<cstddef>`（`std::ptrdiff_t` 8 处）、
     `<cstdint>`（`std::uint*` 6、`std::int*` 2）、`<ctime>`（`std::tm` 25）、
     `<iterator>`（`std::sentinel_for` 2）、`<string>`（`std::basic_string` 16）、
     `<type_traits>`（`std::is_same_v` 2）全部有使用者；三个流头的 `<exception>` /
     `<mutex>` / `<type_traits>` / `<utility>` 同理；`cvt/cvt_concepts.h` 的 `cvt_creator`
     与 `device/device_concepts.h` 的 `io_device` 在三个流头里各 3 / 5 处真的用到。
     `ostream.h` 疑似缺 `<iterator>` 是误报——`:387` / `:402` 的 `std::end` 匹配来自注释里的
     `std::endl`。

290. **四个文件未发现拼写错误。** 英文注释 858 行过 hunspell（剔除标识符与 Doxygen 命令）
     零误拼；中文注释通读一遍加 28 处易错词定点核对（的/地/得、其他/其它、做/作，及
     「刷出」「压回」「哨兵」等术语）零错别字；Doxygen 命令与 `@lang{...}` / `@endif`、
     `@{` / `@}` 配对全部正确。仅两处「一个概念两种写法」——`转换管线`/`转换器管线`、
     `格式化状态`/`格式状态`——属信息性记录，不作为问题。

291. **非法标志组合配 `setw` 不会产生越界或错长输出。** `setiosflags` 把 `adjustfield`
     的两位/三位同时置上（`left|right`、`left|internal`、`right|internal`、三者全置）
     以及 `setiosflags(fmtflags(0xFFFF))` 全置位，再配 `setw(10)` 走 7 条插入路径
     （`int` / 负 `int` / `double` / `const char*` / `std::string` / `char` / `bool` /
     `put_money` 正负）：**输出长度恒为 10、`state=0x0`、ASan+UBSan 无告警**。
     原因是三处对齐判据全部用**整字段相等**比较而非按位测试
     （`io/traits/char_and_str.h:26`、`facet/numeric.h:292`、`:1132`/`:1141`、
     `facet/monetary.h:741` 与 `:786`），任何多位组合都落到「非 left、非 internal」这一支
     即右对齐，填充只发生一次。`monetary::insert` 里 `part::space` / `part::none` 两处
     `testipad` 填充也不会与收尾填充叠加（`testipad` 要求 `f == internal` 精确相等）。
     （第 271 条只核了 `numeric.h` 且只看 `state`，没看输出长度。）

292. **退化实参是良定义的恒等操作。** `setw(0)` 在插入侧对 `int` / `const char*` /
     `put_money` 三条路径都等价于「不设宽度」（输出 2/2/5 字节、无填充），因为三处都是
     严格大于比较；提取侧见第 5 条。`setiosflags(fmtflags(0))` → `fetch_or(0)`；
     `resetiosflags(fmtflags(0))` → `setf(0, 0)` 的 CAS 算 `(old & ~0) | 0` = `old`。
     实测 `flags` 前后恒为 `0x1002`、`state=0x0`，不会误清标志。

293. **`setbase` 与 `setiosflags(basefield)` 的先后交互无隐藏状态，提取侧的自动判定与
     libstdc++ 逐组一致。** `setbase(16)` 后 `setiosflags(basefield)` → `basefield=0x4a`，
     `<< 255` 得 `"255"`；反序 → `basefield=0x8`，得 `"ff"`；`setiosflags(basefield)` 后
     `setbase(7)` → `basefield=0x0`，得 `"255"`；`oct|hex + showbase << 255` 得 `"255"`
     无前缀。最后一次写入胜出。提取侧 `base ∈ {0, 7, -1, 2, 36, INT_MIN, 8, 10, 16}` ×
     10 种输入共 **90 组**，逐组与 libstdc++ 的 `std::setbase(同值)` 对比
     **90/90 完全一致**，ASan/UBSan 干净。（第 3 条只实测了输出那一半。）

294. **货币操纵符与 libstdc++ 的差异只有两类，两类本库都是更好的一方。**
     覆盖口径是**本机 316 个 `.utf8` locale 按 `LC_MONETARY` 形状去重后的全部 24 个
     pattern 桶**，每桶取一个代表（`p_cs_precedes` / `p_sep_by_space` / `p_sign_posn`
     及其 `n_` / `int_` 对应项构成桶键；分桶数据见探针目录的 `all_money.txt`）。
     `put_money` 取 `""` / `"-"` / `"+"` / `"0"` / `"-0"` / `"00"` / `"12.34"` / `"1 2"` /
     `"12abc"` / `"abc"` / 含嵌入 `\0` / 40 位全 9 及其负数，整型取 `0` / `±1` / `±9` /
     `INT_MIN` / `INT_MAX` / `LLONG_MIN` / `LLONG_MAX`；`intl` 两种取值。合计
     **1056 次 put 对拍 + 480 次往返**，ASan/UBSan 干净。差异只有：
     - **`C` locale 的负号**（12 处）。该 locale 下 libstdc++ 的 `negative_sign` 为空串，
       `put_money(-12)` 写出 `"12"`（符号被吞）、`get_money("-12")` 直接 FAIL，本库写出
       `"-12"` 也读得回 `"-12"`，是**两侧对称且可往返**的那一方。
     - **多字节 `mon_thousands_sep`**（48 处），见第 353 条。

     其余 22 个桶（含括号表示负数的 `sign_posn=0`、`sign_posn=2/3/4`、`sep_by_space=2`、
     以及正负 pattern 形状不同的那几个）**逐字节一致**。负零语义与标准一致，超长与整型
     极值两侧完全一致。

295. **`put_time` 在 `std::tm` 合法极值上全部正常。** 15 个时刻 × 27 个格式串 = 405 次
     `put_time`，**全部 `state=0x0`**、ASan/UBSan 零告警，覆盖 1970-01-01、
     1969-12-31 23:59:59、1900-02-28、2000-02-29、2024-02-29 23:59:59、2021-01-01
     （ISO 第 53 周）、0000-01-01、9999-12-31、10000-01-01。年份的接受/拒绝边界**恰好**
     落在 `std::chrono::year` 上、两侧各差一都正确：`tm_year=-34668`（年 −32768）→
     `strfailbit` 零输出，`-34667` → 成功，`30867`（年 32767）→ 成功，`30868` →
     `strfailbit` 零输出。与 glibc `strftime` 的差异只有两类且都是契约：`%Y` / `%F` /
     `%C` 对 0-3 位年份补齐到 4 位（`std::format` 的 chrono 规则）；`%a` / `%j` / `%U` /
     `%W` / `%V` / `%G` 由日期重算而非读 `tm_wday` / `tm_yday`（`io_manip.h:716-717`
     写明的契约）。（第 91 条的补集。）

## 隐含假设与竞态（第二轮）

296. **扩展点函数体所需的基类由概念钉住，不靠约定。** `ostream_type` / `istream_type`
     （`ostream_operators.h:457-468`、`istream_operators.h:312-323`）保证了六个格式操纵符
     用的 `s.setf/fill/precision/width`、`endl` 用的
     `os.locale()/put()/handle_exception()/io_mutex()`、`ws` 用的
     `is.io_mutex()/handle_exception()/T::in_sentry_type` 全部存在。

297. **`copyable_atomic` 的拷贝/移动赋值是原子 store，不是裸内存复制**
     （`common/copyable_atomic.h:66-77`）。因此三个流类的移动赋值整体替换 `ios_state` /
     `ios_base` 时，`m_stream_state` / `m_flags` / `m_width` / `m_precision` / `m_fill`
     的写入仍是原子的，与 `rdstate()` / `flags()` / `width()` 这些无锁读不构成数据竞争。
     （第 204 条只核了 `load/store/exchange/fetch_or/fetch_and/CAS`，没核赋值运算符。）

298. **默认构造的 `m_locale` 不会因环境变量非法而抛。** 空 `name` 走
     `ori_facet_buf::locale_name(LC_*)`，这些名称在单例构造期已由 `resolve_locale` 校验，
     不可实例化者回退到 `"C"`（`locale/locale.h:355` 的注释明文写着不会抛）。故五个构造
     函数的 `@throw device_error` 没有漏掉 `cvt_error` 来源。

299. **迭代器形式的 `operator<<` 在持有 `io_mutex()` 之后才读 `obj.locale()`**
     （`ostream_operators.h:876-889`），故 `put_money` / `put_time` 与 `locale(loc)` setter
     并发时不会读到被移走的 locale。（第 139 条的 TSan 只覆盖了 `endl`/`ends`/`flush`/`ws`。）

300. **`istreambuf<TDevice,TChar>` 与 `locale<TChar>` 的字符类型一致由构造函数的 `requires`
     钉住**，不是靠约定：5 个构造函数都带 `is_same_v<...::char_type, TChar>`。

301. **移后流与 `detach()` 后的流上施加货币与时间四个操纵符是安全的。** 实测：

     ```
     A moved-from os << put_money / put_time      state=0x8  不抛，*tmb 逐字节不变
     B moved-from is >> get_money / get_time      state=0x8  不抛，目标不变
     C detached   os << put_money / put_time      state=0x0  正常写入移后的空 mem_device
     D moved-from ios 三条                        state=0x4  不抛
     E 掩码全开的移后流                            抛 "cannot get monetary facet" /
                                                  "cannot get timeio facet"
     ```
     空 `shared_ptr` 由 `io_manip.h` 的三处 `if (!mp) throw` 拦下，无空指针解引用；
     ASan + UBSan 全程零报告。（第 200 条只覆盖了六个格式状态操纵符，第 75 条也没列这四条。）

302. **`switch_to_put()` / `switch_to_get()` 的 `@throw` 缺失不算契约漏洞。** 实测移后的
     `iostream` 上掩码含 `cvtfailbit` 时两者都会把 `cvt_error` 抛给调用方，但「按异常掩码
     重抛」是写在 `handle_exception` / `clear()` 一处的全局属性；
     `ostream_operators::put`/`flush`、`stream_common_operators::tell`/`seek`/`attach`
     同样只在低层校验处写 `@throw`——是全库惯例，不是本处的疏漏。

303. **四个 TSan 探针 + 阳性对照，全部 0 告警。**
     `clang++ -std=c++23 -fsanitize=thread -g -O1`（clang 21；gcc 的 libtsan 本机缺失），
     每个探针跑 6 遍。阳性对照 `p0_control.cpp`（同样的 include 与线程形态 + 一个 4 线程
     无保护 `++g_racy`）稳定报 1 条 `data race`，证明检测器在该编译选项下确实生效。

     | 探针 | 覆盖面 | 6 轮结果 |
     |---|---|---|
     | `p1_ws` | 4 线程 `is >> ws` + 无锁状态读（`rdstate`/`good`/`eof`/`operator bool`）+ `clear`/`setstate`/`exceptions` + `seek` + `locale(loc)` setter | **0 告警**，无挂起 |
     | `p2_dir` | `iostream` 方向抖动：2 写 + 2 读 + 1 线程反复 `switch_to_put()`/`switch_to_get()` + 1 线程 `endl`/`ws` + 1 线程无锁状态读/`clear` | **0 告警**，无挂起 |
     | `p3_assign` | 特殊成员并发：`a = b` 与 `b = a` 反向配对 + 两个拷贝构造线程 + 两操作数各自的 I/O + 无锁状态读 | **0 告警**，无挂起、无死锁 |
     | `p4_tie` | tie 链 `A→B→C`（`A` 是 `iostream`）：三处读写制造 `try_flush` 竞争 + 反复跑 `tie()` setter + 直接 `flush()` | **0 告警**，无挂起 |

     `p3` 是最值得记的一条：第 44 条的 TSan 只跑了「单个 `ostream` 上 4 线程做插入 /
     格式操纵符 / `endl` / `rdstate`」，**没有**跑特殊成员；第 273、274 条的运行期钩子是
     「持锁计数」的观测，不是竞态检测。

304. **四类竞态假说均被排除，理由是结构性的。**
     - `ws` / `endl` 的临界区无 check-then-act 窗口：`istream.h:436-445` 的 `lock_guard`
       在 `try` **之外**，`in_sentry` 的构造与析构、`catch` 里的 `handle_exception` 全在锁内；
       `ostream.h:470-483` 同形，且 `os.locale()`（不加锁的 getter）与随后的 `os.put()`
       之间没有解锁点，而 `locale(loc)` setter 持同一把锁，两者互斥。
     - `switch_to_put()` / `switch_to_get()` 的 `if (!static_cast<bool>(*this)) return;`
       这次无锁原子读发生在**已持锁之后**（`lock_guard` 在第一行），而状态位的**写**一律在
       `io_mutex()` 下（`io_base.h:919`、`988`、`1215`），故「读状态 → 据此动作」整体原子。
     - tie 的隐藏锁边不会成为等待边：`try_flush()`（`ostream_operators.h:378-394`）一律
       `std::try_to_lock` + 至多 3 次 `yield`，哨兵在持有**自己流的锁**时发起它，故任一线程
       最多「一把阻塞锁 + 一把 try 锁」，构不成 AB-BA。（`sync(P); X << 1;` 反序配对的
       p11 型属用户责任、**故意不修**，见 `memory/tie-deadlock-fix-plan.md`。）
     - 拷贝构造/拷贝赋值的两把锁先后获取、不重叠：`T tmp(other);` 持**源**锁，
       `*this = std::move(tmp);` 持**目标**锁，故 `a = b` 与 `b = a` 并发不会 AB-BA。
       移动赋值只锁目标、读写源确属无保护的跨对象访问，但已写进 `@warning`
       （`istream.h:275-277`），属契约。

305. **`width` 的消费面与 `setw` / `setprecision` 的存储类型描述均属实。** 九组实测：

     ```
     extract char[N]: width→0   extract string: width→0   extract int: width 保留 3
     get_money: 保留 3          get_time: 保留 3
     insert char*: →0           insert int: →0            put_money: →0    put_time: 保留 8
     ```
     存储类型：`io_base.h:752` `copyable_atomic<std::size_t> m_width{0}`、`:751`
     `copyable_atomic<std::uint8_t> m_precision{6}`。注释里写作裸 `size_t` 而非
     `std::size_t` **不是**不一致——全 `include/` 的注释里 `std::size_t` 出现 0 次，
     裸 `size_t` 是注释层一贯写法，第 262 条约束的是代码不是注释。
     另：「move-only 内核（如 `file_device`）上的拷贝必然抛出，故自赋值也要先挡掉」属实，
     且确实是**运行期**抛而非编译期失败——`ostream<ofile_device<char>,char>` 等三个流对
     `std::copy_constructible` 全为 1（`streambuf` 走多态的 `runtime_cvt`，可拷贝性不再由
     设备静态类型决定），真开文件实测拷贝构造抛
     `runtime_cvt fail: kernel does not support copy construction`，而 `if (this != &other)`
     短路让 `a = a` 安全返回。四个文件里没有 `` `xxx.h:123` `` 形式的行号交叉引用
     （全量正则零命中，4 处 `:数字` 是时间字面量 `01:02`），6 处文件名形式的引用逐个属实。

## 填充字符的可读性判据（跨文件：`facet/numeric.h` / `facet/monetary.h`）

306. **插入与提取两侧都拒绝会改变人类读数的填充字符。** 判据同时看**是哪个字符**与
     **落在哪里**，三种落点：紧贴数字之前（`leads_digits`）、数值之后（`trails_value`）、
     二者皆非（隔着符号 / 进制前缀 / 货币符号）。

     - **数字**：仅当是 `'0'` 且 `leads_digits` 时安全。`'1'`–`'9'` 处处危险；`'0'` 落在
       数值之后（`"42000000"`）或落在符号 / `0x` 之前（`"00000-42"`）同样危险。
     - **小数点**：向右与数字结合，仅 `trails_value` 时安全（`"42......"` 仍读作 42，
       `"......42"` 读作 0.42）。
     - **符号首字符**（数值 `'-'`/`'+'`；货币 `negative_sign[0]`/`positive_sign[0]`）：
       落在数值之前会被读成该数值的符号，故**仅当与数值本身的符号一致时才安全**；
       `trails_value` 时一律安全。
     - **其余字符一律安全**，**包括千位分隔符**：它必须左右都有数字才成立，而填充段靠数字
       的那一侧之外只可能是符号、进制前缀、货币符号或字段边界，故永远无法与数值结合。

     后两条是被实测推翻后定下来的，记下以免回退：「符号一律危险」错——现有 `facet` 测试
     大量使用 `ios.fill('+')` 期望 `"++++++++2,22507e-308"`，那读出来仍是同一个数；
     「千位分隔符也危险」错——会让以空格为千位分隔符的 locale（fr_FR 等）连默认填充都用不了。

307. **命中判据即 `throw stream_error`，与 `max_pad_count` 的抛出同构**——经
     `handle_exception` 落成 `strfailbit`，仅当该位在异常掩码中时才抛给调用方。
     **检查只在真正写出（或消费）填充字符的那一次发生**：`setw` 为 0 或不大于内容长度时
     根本不填充，此时不检查、不抛。`fill` 是粘性流状态，不能因为流上挂着 `setfill('1')`
     就让所有插入都失败。

308. **四个落点各自的判定信息都是就地取得的，不靠推算。**
     1. 数值插入侧：`facet/numeric.h` 的私有谓词 `fill_alters_reading_` 在 `pad()` 里调用
        ——`pad` 只在 `w > len` 时被调用，正是「真要填充」那一刻，且已拿到 `adjust`、
        `startSign`、`start0x`、`cs[0]`。`insert_float` 与 `insert_int` 两个调用点因此
        一并覆盖。`put(bool)` 的 boolalpha 分支**不检查**：补的是
        `truename()`/`falsename()` 不是数，任何填充都不会让它读成另一个值，检查反而会打掉
        `setfill('0') << setw(n) << boolalpha` 这种正常写法（代码里有注释说明）。
     2. 货币插入侧：`insert` 在写出每段填充时记下它在 `res` 中的 `{pos, len}`，收尾 pad 后
        统一判定——**位置从成品字段上读**而不是从 pattern 上推：某段填充是否紧贴金额，取决于
        中间那些 part 实际是不是空的（正号为空、`showbase` 未置位等），只看 pattern 判不出来。
     3. 货币提取侧：`extract` 的 `space` / `none` 分支里**只有真的吃掉了填充**才判定
        （`ate_fill`），因此 `is >> setfill('1') >> get_money(v)` 读 `"$123.45"` 一切照旧，
        读 `"112345"` 才响亮失败；是否紧贴数字**从输入本身读**——看 run 停在哪个字符上。
     4. 解析逻辑本身（`extract` 里消费 fill 的那个 run）一行未动。
     `facet/numeric.h` 的提取侧**不读 `fill`**（全文 4 处 `io.fill()` 都在 `put`/`insert_*`
     里），故数值解析不存在被 fill 吃掉内容的问题，无需改动。

309. **本库在这一点上与标准有意分歧，代价已知。** 输出侧本库与 std 同构——libstdc++ 15
     `money_put::_M_insert`（`bits/locale_facets_nonio.tcc:542-554`）对 `space` 是
     `__res += __fill;`、对 `none` 是 `if (__testipad) __res.append(__width - __len, __fill);`，
     与 `facet/monetary.h:766-772` 逐字符一致。输入侧 std 用空白
     （`__ctype.is(ctype_base::space, …)`，[locale.money.get.virtuals]/2 亦如此规定），
     本库改用 `io.fill()`：往返实测（3 locale × 4 种格式状态共 12 组）
     **IOv2 10/12、libstdc++ 4/12**——只要 `fill` 不是空格，libstdc++ 就读不回自己刚写出的
     货币值。**不应**把判据改回 `ctype::space`：`extract` 只拿到 `ios_base<char_type>&`，
     全文没有 ctype/locale 引用，`ios_base` 也没有 `locale()`，根本取不到 ctype facet。
     分歧点是 std 对上述所有情形一律静默产出、静默解析，本库会拒绝。
     **已知代价（保留）**：`fill` 为默认的 `' '` 时，`'\t'` / `'\n'` 在 `space` 位置不被接受，
     而标准要求接受；只影响读入别处产生的、用 TAB 作分隔的货币文本。

310. **填充判据没有牵连宽容解析。** 宽容度矩阵（`q3_sepcount` vs `baseline.txt`）逐字节相符；
     往返矩阵 12 组 IOv2 过 10 组，那两组失败在写侧 / 读侧都是响亮的失败，不是静默的错值。
     `test_io_base_manipulators_basefield.cpp` 里 `setfill('0')` 下的 hex `left` / `right`
     两行用的是空格填充：`"|0x12 345 6780000|"`（会被读作 0x123456780000）与
     `"|00000x12 345 678|"`（读不成任何数）正是这条判据要拒绝的输出；`internal` 那行用零填充，
     那才是十六进制补零的正确写法。

## tie 边对双向流读侧的影响

311. **`flush` 会丢掉双向流已压回的读侧内容，这是既定语义，三处文档都写明了。**
     调用链：`writer << x` → `out_sentry` 构造（`ostream_operators.h:135` `tied->try_flush();`）
     → `out_flusher<iostream<...>>::try_flush()` → `target.flush()` →
     `streambuf::flush()`（`streambuf.h:500`）→ `streambuf::switch_to_put()`（`:677`）
     → `m_read_buf.clear()`（`:693`）。切换前会把底层转换器回退到 `tell()`，但**回退恢复的是
     位置而不是内容**：`sputbackc()` **替换**过的字符与**过量压回**的字符在切换后一律丢失，
     再读回来的是底层原始数据。实测矩阵（remote kick 来自另一线程）：

     ```
     (1) 只 peek，未替换      : before 'a' tell=0 | after 'a' tell=0   state=0x0   ← 无损
     (2) 替换式 putback('?')  : before '?' tell=0 | after 'a' tell=0   state=0x0   ← 内容丢失
     (3) 过量压回 'X','Y'     : before 'Y' tell=0 | after 'a' tell=0   state=0x0   ← 两字符全丢
     (4) EOF 后 putback('Z')  : before 'Z' tell=1 | after 'b' tell=1   state=0x0   ← 读到别的字符
     ```
     边界恰好是「压回内容 ≠ 底层内容」。**行为是对的**：`flush` 是一次输出操作，输出必然
     要求设备停在写位置，读缓冲区里放的是尚未生效的读侧推测状态，刷新把它丢掉是输出语义的
     必然结果。发起者可以是**另一个流**——`iostream` 是合法 tie 目标，tie 目标在绑定方每次
     I/O 前都会被 `try_flush()`，于是一次与本流无关的 `writer << "x"` 就会销毁本流的压回
     内容且不置任何状态位。这一点现由三处文档覆盖：`streambuf.h` 的 `switch_to_put()`
     （回退恢复位置而非内容、要保留就先消费掉）、`iostream.h` 的类文档与 `switch_to_put()`
     两条 `@warning`（本类可作 tie 目标、发起者可以是绑定方）、
     `stream_common_operators.h` 的 `tie()` setter `@warning`（别把正在用
     `putback()`/`peek()` 读的 `iostream` 设为 tie 目标）。

## 契约完整性（第二轮补齐后的当前状态）

312. **四处契约与代码现已逐项对齐，可据以判断异常路径。**
     - `io_traits<TChar, get_time_t<TChar>>::sread` 的 `@throw` 列全了四条抛出通道：
       两个空指针、缺 timeio facet、**解析失败**（格式不匹配 / 字段越界 / 中途 EOF，由
       `timeio::get` 抛 `stream_error("timeio parse error")`——这是本函数最常见的失败方式）、
       以及 `convert_to` 在结果不是有效日历日时的同类抛出。四者一样由流转成 `strfailbit`，
       仅当该位在掩码中时才传播到调用方。粒度与插入侧 `put_time_t::swrite` 一致。
     - `put_money` / `get_money` 的 `@param mon` 写明取值域是「**除 `bool` 外**的整型，或
       `std::basic_string<char_type>`（**须为默认 `char_traits` 与默认分配器**）」。排除
       `bool` 与 `monetary::put`/`get` 的 `requires` 一致；默认 traits/分配器的要求**比本库
       自己的字符串插入更严**——`io_traits<TChar, std::basic_string<TChar, TTraits, TAlloc>>`
       （`char_and_str.h:485-486`）对二者开放，故 `os << str` 能写的串未必能
       `os << put_money(str)`。三类实参失败时诊断都是通用的「没有匹配的 `operator<<`」，
       看不出原因，所以必须在文档里列全。实测可行性：
       `long long/int/char/string = 1`，`bool/double/odd_str = 0`（两侧一致）。
     - `istream.h` 里 `ws` 的注释用的是哨兵形参真名 `noskip`（`istream_operators.h:116`），
       并注明 `in_sentry` **不读** `skipws` 标志、只看构造实参；读标志的是提取运算符
       （`istream_operators.h:1159`），而 `ws` 这条路径传的是字面量 `false`，故
       **`is >> noskipws >> ws` 照样跳空白**。实测：`skipws` 清除与置位两种情况下
       `is >> ws` 都是 `pos 0 -> 3, state=0`，只有普通提取才受标志影响。
     - `io_manip.h` 文件级 `@warning` 里「为什么可以不加锁」的原语清单是
       `exchange` / `fetch_or` / CAS 循环**三项**，与第 4 条一致。`fetch_and` 在
       `io_base.h` 里只有 `unsetf(msk)`（`:374`）一处，而本文件没有任何操纵符调用 `unsetf`
       （`resetiosflags` 走的是 `setf(fmtflags(0), mask)`）。

313. **三个流类头文件都直接包含 `<common/copyable_mutex.h>`。** 它们的私有加锁委托构造函数
     在**签名**里直接写出 `copyable_mutex<std::recursive_mutex>`（`common/copyable_mutex.h:72`），
     此前是靠 `io/io_base.h` 传递取得声明。现已各加一行，按既有分组规则排在本地头一组的
     字典序首位。第 263 条「`include/` 里每个头都自包含、靠传递包含拿声明的已经没有」
     在这三个文件上现在成立。

## `cvt` 写出侧的预留与回滚（跨文件：`cvt/abs_cvt.h` / `code_cvt.h` / `comp` / `crypt`）

314. **预留了输出槽位的转换器都用 RAII 守卫回滚，异常路径不会漏出字节。**
     `abs_cvt.h` 的 `put_buf_guard` 构造时记下预留量，`used(n)` 扣减已用量，析构回滚剩余
     （`rollback` 在析构里抛出被吞掉）。三处使用：`code_cvt::put_main`、
     `chacha20_cvt::put_main`（`m_cipher->cipher()` 可能抛）、`zlib_cvt::put_main`
     （`deflate` 返回后立刻记 `used`，成功与失败两条路径的回滚量同为 `avail_out`）。
     `vigenere_cvt` 循环体是纯算术不抛、`hash_cvt` 不用 writer，二者不需要守卫。

315. **三种 `cvt_writer` 特化里有两种不在自己的暂存缓冲里预留**，这是守卫必须存在的原因：

     | 特化 | `put_buf` | `commit` | 未回滚的预留量会怎样 |
     |---|---|---|---|
     | mem_device（`root_cvt.h:2200`） | 直接转调 `mem_device::put_buf`，当场 `m_str.resize()` 并推进写位置 | **空操作** | 字节**立刻**在设备里 |
     | 非 mem_device（`root_cvt.h:1850`） | 推进 root_cvt 内部缓冲游标 | 刷出 | 下次 flush 时落到设备 |
     | 基础模板（`abs_cvt.h`） | 写进自有 `vector` | 刷出 | 无（析构即丢弃） |

     `abs_cvt.h` 里「未 commit 的内容在 writer 析构时丢弃……编码错误丢弃整批而不是产生
     partial commit」那句**只对基础模板成立**。gdb 实测链路
     `sputc → runtime_cvt::put → abs_cvt::put → code_cvt::put_main → cvt_writer<no_rb_root_cvt<mem_device>>::put_buf → mem_device::put_buf`
     中两处 `writer.commit()` 断点均未命中——字节在 `put_buf` 返回那一刻就已进设备。
     `zlib_cvt` 的注释里早已记下这一点。

316. **`codecvt_kernel<char, TInt>` 不做码点校验，合法性完全由 glibc 决定。**
     其 `out_helper` 的唯一动作是 `std::wcrtomb`。直接探 glibc（zh_CN.UTF-8）：

     | 码点 | `wcrtomb` | 字节 |
     |---|---|---|
     | `0xD800` / `0xDFFF` | −1 (EILSEQ) | — |
     | `0x10FFFF` | 4 | `f4 8f bf bf` |
     | `0x110000` | 4 | `f4 90 80 80` |
     | `0x200000` | **5** | `f8 88 80 80 80` |
     | `0x7FFFFFFF` | **6** | `fd bf bf bf bf bf` |
     | `0xFFFFFFFF` | −1 (EILSEQ) | — |

     glibc 的 UTF-8 转换器实现的是**旧的 RFC 2279**（最长 6 字节、上限 `0x7FFFFFFF`）
     而非 RFC 3629 的 `0x10FFFF`，故 `os << (char32_t)0x110000` 静默写出 4 字节的
     `f4 90 80 80` 而 `good()`；代理被拒也是 glibc 拒的。两侧一致（`mbrtowc` 读得回这些
     序列），且**无内存安全问题**——`out_helper` 先保证有 `epc() == MB_CUR_MAX` 字节可写，
     该 locale 下实测 `MB_CUR_MAX == 6`，正好覆盖最长输出。本库自己的
     `codecvt_kernel<char8_t, TInt>::out_helper` 判据则是严格的（显式拒代理、显式
     `else if (c <= 0x10FFFFU)`），所以这是**两个内核判据不一致**，不是「库没查」。
     要严格的 USV 语义就用 `code_cvt_creator<char8_t, char32_t>`。四处文档写明了这一点：
     `codecvt_kernel<char, TInt>` 类 `@warning`、`out_helper` 的 `@note`（「无法编码」只等于
     `wcrtomb` 返回 −1）、`code_cvt_creator<char, TInt>` 的 `@warning`、
     `codecvt_kernel<char8_t, TInt>` 的反向对照 `@note`。

## 年份往返与 `setbase` 的形参宽度

317. **年份是 `put_time` / `get_time` 往返对称性的唯一例外，方向与全篇相反。**
     `put_time` 接受的年份范围比 `get_time` 能读回的**宽**：`%Y-%m-%d` 往返在年 0..9999
     全部成功，而年 10000 / 32767 / −32767 **put 成功、get 得 `strfailbit`** 且 `*tmb`
     完全不变。这是有意的——`facet/timeio.h` 的 `do_get` 里 `%Y` 分支上有整段注释说明这是与
     `std::chrono::from_stream("%Y")` 和 POSIX `strptime` 对齐的取舍并标注 "NOT a bug"，
     第 94 条也已确认。（libstdc++ 在同样输入上也 FAIL，但会留下**写了一半**的 `std::tm`
     ——`tm_year` 被写成 3276，本库一字节不改，这一点本库更好。）现由三处文档覆盖：
     `facet/timeio.h` 类总览把「总能读回」限定到「**因退化而写出的字面量**」这一语境并新增
     年份例外的 `@warning`；`io_manip.h` 的 `put_time` 与 `get_time` 各新增一条 `@warning`，
     分别写「年份是唯一反过来的一处，这里比 `get_time` 宽」与「年份上不对称的方向是反的，
     这里比 `put_time` 严」，后者点明这是全篇「提取侧更宽松」的唯一例外。

318. **`setbase` 的形参与 `m_base` 是 `std::ptrdiff_t`，宽于 `std::setbase` 的 `int`。**
     取值只与 8、10、16 比较，形参一旦窄于实参，越界值会先被静默截断再撞上其中某个常量，
     于是**看起来**设置成功——`int` 形参下 `setbase(8LL + (1LL << 32))` 会选中八进制。
     这比 `setw` / `setprecision` 的同类问题更隐蔽：越界 precision 至少能被 `-Woverflow`
     或 `io_base::precision` 的范围检查抓住，而截断后的 base 是个**完全合法**的取值，
     哪一层都不报警。加宽后该实参落进「其它取值」一档，清空 `basefield`。
     `io_traits<TChar, setbase_t>::apply` **不做**值域校验、不抛 `stream_error`，这是刻意的：
     那个条件表达式逐字就是 [std.manip] 对 `std::setbase` 的规定；`basefield == 0` 不是
     「存不下」的非法状态而是 `resetiosflags(ios_defs::basefield)` 同样可达的合法状态；
     `setw` / `setprecision` 抛出的理由是实参放不进目标存储（`io_base.h:426-430`，
     `std::uint8_t` 存不下 300），进制没有这种情形。本文件立的「越界实参显式拒绝而不是静默
     窄化」这条规矩管的是**窄化**，不是值域校验。

## 回归（当前工作树）

319. **`cvt` 与 `io` 两个套件在当前工作树上跑通，其余五个套件自 2026-08-14 起未重跑。**
     `cvt` **260** 项（较第 287 条的 257 增加 3 项：`test_code_cvt_mem_char_put_err_2`、
     `test_code_cvt_mem_char8_t_put_err_2`、`test_put_buf_guard_1`，覆盖第 314、315 条的
     守卫）；`io` 在 debug 模式下全绿，较第 287 条的 353 增加 6 项
     （`ios_base<char> fill case 2` 覆盖第 306 条，`setbase` 5 项覆盖第 318 条）；
     `facet` **530**（较 527 增加 3 项，覆盖第 306-308 条）。**注意**：这三个数字来自各自
     改动当时的单套件运行，**没有**做过一次覆盖全部七个套件的重跑，
     sanitizer 版更未重跑——见末节。

## tie 刷新失败的归属与诊断（跨文件：`io/utilities/{ostream,istream}_operators.h` / `io/io_base.h`）

320. **tie 目标刷新失败时，失败只记在**目标**身上，且 `try_flush()` 是 `noexcept` 的。**
     `out_flusher<T>::try_flush()`（`ostream_operators.h:432`）包住 `obj.flush()`，catch 中调
     `obj.handle_exception<true>(std::current_exception())`（`:450`）：置目标的失败位、把原始
     异常存进目标对应的 `m_exp_*_fail` 槽位，然后返回。发起方一位不动、也收不到异常。
     纯虚声明 `abs_flusher::try_flush() noexcept`（`:344`）把这条归属规则变成编译期约束，
     两处哨兵因此只写 `tied->try_flush();`（`ostream_operators.h:134`、
     `istream_operators.h:123`），不再需要 `catch (...)`。

     **`noexcept` 为什么必需**：哨兵构造在**发起方**流的 `try` 块内
     （插入运算符 `ostream_operators.h:881-889` 一带），异常一旦逸出就落进发起方的
     `handle_exception`，而后者按**异常类型**分派（`io_base.h:1222-1227`）——发起方会被置上
     自己管线从未发生过的 `cvtfailbit`，且它的 `m_exp_cvt_fail` 里躺着一条描述**别的流**的
     streambuf 的异常。靠每个调用点自觉包 catch 来维持归属是脆弱的，`noexcept` 让这条误归属
     路径**结构上不可达**。

321. **必须用 `handle_exception<true>`（忽略掩码）而不是默认版本，差别不只是「抛不抛」。**
     掩码版最终走到 `clear()` 的重抛分支，那一行是
     `std::rethrow_exception(std::exchange(m_exp_cvt_fail, nullptr))`（`io_base.h:938-939`）
     ——**重抛会顺手把刚存进去的原始异常清空**。在这条路径上重抛出来的异常无处可去，
     于是净效果会是「恰恰对那些显式把该位放进 `exceptions()` 的使用者，连异常对象都不留」。
     `<true>` 使 `setstate<true>` → `clear<true>` 的 `if constexpr(!ignore_exception_mask)`
     整块被跳过，暂存槽得以保留，诊断可事后从目标取回。

     实测（`.review/tmp3/ios/m3_stash_consumed.cpp`，不可定位管线 + tie，ASan/UBSan 零报告）：
     掩码开与关两种情形，`target.clear(cvtfailbit)` 取回的都是完整原文
     `"base_streambuf::switch_to_put fails: cannot reposition 1 ..."`。若暂存被消费，取回的会是
     `clear()` 在槽位为空时抛的通用兜底 `"converter failure bit has been set"`（`io_base.h:941`）
     ——该字符串的出现即为暂存已空的判据。

322. **tie 目标的异常掩码在这条路径上不生效，这是有意的，且与标准库不同。**
     唯一在场的调用栈属于一个与目标无关的流，异常送不出去；失败通过目标的状态位与
     `m_exp_*_fail` 上报。标准库不是这样——实测（`.review/tmp3/m2_std_tie_throw.cpp`，
     `streambuf::sync()` 返回 −1 使 `flush()` 失败）：

     | 场景 | libstdc++ | libc++ |
     |---|---|---|
     | 插入，只有 target 有掩码 | 异常**逸出** `writer << 1`；writer 状态 `0` | 不逸出；**writer 置 badbit** |
     | 插入，两边都无掩码 | 不逸出；target 置 badbit；写入照常 | 同左 |
     | 插入，两边都有掩码 | 异常逸出；writer 状态 `0` | 异常逸出；writer 置 badbit |
     | 提取，只有 target 有掩码 | 不逸出；**reader 置 badbit\|failbit** | **异常逸出**；reader 状态 `0` |

     两家在插入侧与提取侧**正好互为镜像**，说明 `[ostream.sentry]`/`[istream.sentry]` 只规定了
     「调用 `tie()->flush()`」而未规定异常处理，分歧在于 sentry 构造算不算
     `[ostream.formatted.reqmts]` 所说的「输出期间」。但两家有两点一致：**target 每次都拿到
     自己的失败位**，且**失败从不被静默吞掉**。IOv2 取前者、不取后者，理由即第 320 条的误归属。

323. **`iostream` 作 tie 目标时的后果按转换器**是否支持定位**分两支，文档已按此写明。**
     `iostream.h` 的类文档 `@warning`（`:40` 中文 / `:81` 英文）与 `switch_to_put()` 的
     `@warning`（`:410`+`:417` 中文 / `:443`+`:452` 英文）、以及
     `stream_common_operators.h` 的 `tie()` setter `@warning`（`:723` 中文 / `:820` 英文）
     一致给出：

     | | 可定位（本库自带三种设备都在此档） | 不可定位（双向但不可 seek，如套接字一类） |
     |---|---|---|
     | 压回内容 | 被丢弃（`clear()` 执行了） | **完整保留**（抛出在 `clear()` 之前） |
     | 状态位 | **不置任何位**（刷新成功了） | 置**目标**的 `cvtfailbit`，原始异常进目标的 `m_exp_cvt_fail` |
     | 发起方 | 不受影响 | 不受影响，也拿不到异常 |

     **触发门槛比看上去低**：不必显式 `peek()`/`putback()`——任何一次成功的提取都会在定界符处
     把一个字符留在 `m_read_buf` 里。实测（`.review/tmp3/ios/p1_tie_switch_throw.cpp` +
     `noseek.h`）第 3 组只做了一次普通的 `ios >> std::string`，随后的 `switch_to_put()` 一样失败。
     又因抛出先于清空，读缓冲区仍非空，目标 `clear()` 之后绑定方再写一次就会再次置位；
     要脱离该状态须先把目标的读缓冲区消费干净，或解开 tie。

324. **`switch_to_put()` 里「seek 在 clear 之前」是一条对外契约的支撑点，已加注释锁住。**
     `streambuf.h:698-719`：`tell()` 在 `:704`、`m_cvt.seek(pos)` 在 `:705`、
     `m_read_buf.clear()` 在 `:716`、`m_cvt.switch_to_put()` 在 `:718`。两处结论压在这个顺序上：
     (a) `tell()`（`:546-557`）用 `m_cvt.tell() - m_read_buf.size()` 算逻辑位置，先 `clear()`
     会让它算出设备的**超前**位置、seek 到错误偏移且不报错；(b) 第 323 条「压回内容反而保留」
     成立的原因正是 `seek()` 抛出发生在 `clear()` 之前。源码在 `:714-715` 已有注释说明它必须
     留在 seek 之后。（`clear()` 挪进 `try` 块内语义等价——`m_read_buf` 是
     `std::basic_string`，`clear()` 不抛——目前未挪。）

## 转换管线的方向能力（跨文件：`io/io_concepts.h` / `io/streambuf.h` / `cvt/*`）

325. **流缓冲区的方向与转换管线的读写能力在**声明层面**匹配，判据施加于具体转换器类型。**
     `io_concepts.h` 的两个 concept：`cvt_direction_capable<TCvt, IsIn, IsOut>`（`:130`）给出
     三档要求——双向要 `cvt_cpt::support_io_switch`、只读要 `support_get`、只写要
     `support_put`；`cvt_fits_direction<TRootCvt, TCreator, IsIn, IsOut>`（`:184`）按
     `ext_to_int`（`:78`）的同一套路在未求值语境里形式化调用 `TCreator::create(TRootCvt)`
     取得管线末端类型，再施加前者。

     施加位置是 `base_streambuf` 的两个接收 creator 的构造函数：输出/双向那个（`:146`）一次，
     对 `no_rb_root_cvt<TDevice>`；输入那个（`:198-199`）**两次**，对 `rb_root_cvt<TDevice>`
     与 `no_rb_root_cvt<TDevice>`——它按运行期 `has_in_buf` 在三元表达式里二选一，两支都实例化。

     判据放在 I/O 层而非 cvt 层，是因为方向（`IsIn`/`IsOut`）是**流**的概念；与设备一侧分工
     一致——`dev_cpt` 的能力原语在 `device/device_concepts.h`，组合成方向约束的是
     `io/iostream.h`。

326. **`runtime_cvt` 上做不了这个判断，能力信息只在 `create()` 的返回类型上还在。**
     实测（`.review/tmp3/m4_p2_feasibility.cpp`）：

     | 类型 | put | get | io_switch | positioning |
     |---|---|---|---|---|
     | `no_rb_root_cvt` / `rb_root_cvt` | 1 | 1 | 1 | 1 |
     | `zlib_cvt` | 1 | 1 | **0** | 0 |
     | `hash_cvt` | 1 | **0** | **0** | 0 |
     | `vigenere_cvt` / `code_cvt` | 1 | 1 | 1 | 1 |
     | **`runtime_cvt`（`m_cvt` 的实际类型）** | 1 | 1 | **1** | 1 |

     `runtime_cvt` 是运行期类型擦除外覆类，接口全都实现出来、能力不足时在运行期抛
     （`runtime_cvt.h:413` 的 `if constexpr (!support_io_switch<KernelType>) throw`），故 concept
     施加其上恒为真。而 `abs_cvt` 用 `requires (enable_io_switch && support_io_switch<KernelType>)`
     门控 `switch_to_get`/`switch_to_put`（`abs_cvt.h:1786`、`:1825`），所以具体类型上的判据是准的。

327. **`io` 层无需自己检查，约束从 streambuf 自动传播。**
     `istream`/`ostream`/`iostream` 的构造函数约束里写有
     `decltype(streambuf{declval<TDevice>(), declval<const TCreator&>()})::char_type`；
     `streambuf` 的 ctor 被约束掉后该表达式非良构，按 [temp.constr.atomic]/3 使那条原子约束判为
     **不满足**（软拒绝，非硬错误），对应的流构造函数随之被移除。用不依赖本库的最小样例验证过
     这条语言规则（`.review/tmp3/m6_propagate.cpp`）。

     **不能改放在接收 cvt 的私有委托构造函数上**（`.review/tmp3/m7_private_ctor.cpp` 实测）：
     约束只在私有 ctor 上时公开 ctor 仍可行，`is_constructible_v` 照样为真，失败退化成实例化时的
     **硬错误**，上面这条传播链失效；且输入侧 ctor 的三元表达式已把两支显式转成 `runtime_cvt`，
     私有 ctor 拿到的正是被擦除后的类型，根本判不了。

328. **当前的接受/拒绝矩阵**（实测 `.review/tmp3/m5_p2_scope.cpp`，常驻用例
     `test_streambuf_char_io_switch_4`）：

     | | zlib（get+put，不可切） | hash（只 put） | vigenere / code（可切） |
     |---|---|---|---|
     | `streambuf` / `iostream` | 拒 | 拒 | 收 |
     | `istreambuf` / `istream` | 收 | **拒** | 收 |
     | `ostreambuf` / `ostream` | 收 | 收 | 收 |

     诊断是 `constraints not satisfied`。仓库自带的 `zlib_cvt`（`cvt/comp/zlib_cvt.h:161`）、
     `hash_cvt`（`cvt/crypt/hash_cvt.h:183`）、`chacha20_cvt`（`cvt/crypt/chacha20_cvt.h:155`）
     都是 `abs_cvt<..., positioning=false, io_switch=false>`，故都不能用于 `iostream`；
     `hash_cvt` 连 `istream` 也不能用。

     **注意**：`test/io/iostream/switch_to_{get,put}/` 里那四个用例此前从未真正在 zlib 管线上
     切过方向——`runtime_cvt::switch_to_get()` 开头是
     `if (m_io_status == io_status::input) return;`（`runtime_cvt.h:413`），新构造的流本就在
     input 方向，空操作。现在它们是 `static_assert(!is_constructible_v<...>)`。

## `put_time` / `get_time` 对无法解释的格式内容

329. **格式串末尾落单的 `%`（或落单的 `%E`/`%O` 修饰符）两侧都按字面透传，往返成立。**
     put 侧 `do_put` 在 `++f == format.cend()` 时写出 `'%'`（`timeio.h:2983`；修饰符那支写
     `'%'` + 修饰符，`:2993`）；get 侧 `do_get` 在 `++fmt == _fmt.cend()` 时按字面 `%` 匹配
     （`:1998`；修饰符那支 `:2013`），与现成的 `%%` 分支同一逻辑。

     这走的是本 facet 对**无法解释的格式内容**既有的策略——`bad_format` 标号
     （`timeio.h:3412` 一带）的注释写着 "Unknown format; output the format, including the '%',
     since this is most likely the right thing to do if a multibyte string has been misparsed"，
     即 `%Q` → `"%Q"` 的来历。由此本 facet 的策略分界是：**值**有问题（`tm` 字段越界等）严格
     拒绝，**格式文本**不认识则原样透传——坏值会静默产出错误输出必须拦，不认识的格式文本会
     可见地出现在输出里。

     实测（`.review/tmp3/p2_trailpct.cpp` / `m10_roundtrip.cpp`，常驻用例
     `test_timeio_char_put_19` / `test_timeio_wchar_t_put_18`）：

     | fmt | put 输出 | get 读回自身输出 | `strftime` |
     |---|---|---|---|
     | `%Y%` | `"2024%"` | ✓ | `"2024%"` |
     | `%` | `"%"` | ✓ | `"%"` |
     | `a%` | `"a%"` | ✓ | `"a%"` |
     | `%E` / `%O` | `"%E"` / `"%O"` | ✓ | 同 |
     | `%%` | `"%"` | ✓ | `"%"` |
     | `%Q` | `"%Q"` | ✓ | `"%Q"` |

     put 与 `strftime` **逐行一致**。get 仍要求输入里真有那个 `%`（`get("2024")` 配 `"%Y%"`
     失败），故"往返成立"是真的字面匹配而非把尾随 `%` 忽略掉；常驻用例里有这条反向断言。

330. **标准没有规定这件事，两个实现的答案在 put 侧相反。**
     `std::put_time` → `time_put::put`，标准把 `do_put` 定义为「as if by calling `strftime`」，
     而 C 标准说 `%` 后跟非法说明符行为未定义（末尾 `%` 即 `%` 后跟 `\0`，属此列）；
     `std::get_time` → `time_get::get`，标准未为「格式串以 `%` 结尾」规定行为。实测
     （`.review/tmp3/m9_std_trailpct.cpp`）：libstdc++ 的 put 丢弃（`"%Y%"` → `"2024"`），
     libc++ 的 put 原样输出（→ `"2024%"`，与 `strftime` 一致），两家都 `good`、都不置位。
     get 侧两家一致，但那更像 `%Y` 撞 EOF 后循环提前退出的副产品。往返上：libstdc++ 成立、
     **libc++ 不成立**（put 输出 `%` 但它自己的 get 拒绝）。IOv2 现在与 `strftime` 对齐且自洽。

## 设备的刷新契约

331. **自带内部缓冲的设备必须在自己的析构函数里 `dflush()`，已写成 `dev_cpt::support_put`
     的契约**（`device/device_concepts.h:63` 一带，中英各一条 `@note`）。
     销毁一个流只把转换器缓冲经 `dput()` 推给设备——`root_cvt::~root_cvt()`
     （`root_cvt.h:341-352`）调的是 `flush()`——**不会**调用设备的 `dflush()`。
     全仓 `dflush()` 的调用点只有三处：unitbuf 哨兵析构（`ostream_operators.h:213`）、
     `flush(true)`（`:773`）、以及设备自己的析构。自带设备均已遵守：`file_device.h:301`、
     `std_device.h:113-118` 都在析构里 `dflush()`，`mem_device::dflush()` 是空实现
     （`mem_device.h:306`，无内部缓冲）。不遵守则设备缓冲里的数据在流销毁时丢失。

## 三个流类与操纵符（第三轮的其余核对）

332. **`iostream` 的三个特殊成员在自赋值与自拷贝上齐全**（探针 `ios/p3_special_members.cpp`）。
     `a = a` 与 `a = std::move(a)` 连做之后：tie 目标保持、状态 `0x0`、读缓冲区里的 `peek`
     结果不变、后续提取与设备内容与未自赋值的对照逐字节相同。拷贝构造产出的两条流互不影响；
     拷贝赋值整体替换设备是类文档 `@warning` 明写的语义，**不是**数据丢失。以上在
     **非可定位管线**上重跑结论相同。补上第 34/274 条只做语言规则论证的那一半。

333. **`iostream` 的成员声明顺序与析构期冲刷**：`m_streambuf` 在前、`m_locale` 在后，
     与类文档一致，故析构是 `m_locale` 先、`m_streambuf` 后，`~root_cvt` 的那次冲刷跑在一个
     仍完整的下层上。移动赋值里成员赋值顺序同向。与 `ostream.h` 的同一段话逐字对应。

334. **默认构造的流不是「没有设备」**：`iostream()` / `istream()` / `ostream()` 构造的是
     `TDevice()`。默认构造后 `tell()` 得 0、插入与提取正常，`flush`/`ends`/`endl` 三个操纵符
     全 `state=0x0`。真正接近「无设备」的是 `detach()` 之后：`tell`/`switch_to_put`/
     `switch_to_get`/`flush`/`peek`/`<< "zz"` 六项逐个调用得
     `0x0/0x0/0x0/0x0/0x1(eof)/0x0`，无崩溃、ASan/UBSan 零报告；随后 `attach()` 正常读回。

335. **`switch_to_put()` / `switch_to_get()` 的 `handle_exception` 路径在不可定位管线上同样成立**
     （探针 `ios/p5_switch_handle_exception.cpp`，即第 281 条当时构造不出来的那一支）：
     状态位恰好一位、失败态早返回、`clear()` 后可复用、掩码打开时异常抛给调用方且
     **锁被干净释放**（跨线程 `try_lock` OK，无泄漏）。

336. **析构期冲刷不需要也没有碰 locale**（探针 `s1_dtor_order.cpp`）。用 `weak_ptr` 观察流自己
     `m_locale` 持有的 ctype facet：四组（`ostream` / 外部持有对照 / `iostream` / 经
     `abs_flusher*` delete）实测 `m_locale` 确实先于 `m_streambuf` 销毁（冲刷时 facet 已 DEAD，
     对照组证明探测手段灵敏），而数据一字不少落盘，ASan/LSan 零告警。即 `ostream.h` 那句
     「destructor-time flush runs after `m_locale`」是对现状的准确描述，不是顺序错误。

337. **析构期冲刷抛出不会 `terminate`。** 收尾真正发生在 `~ostreambuf` → `~root_cvt`
     （`root_cvt.h:341-352`）的 catch-all 里，故 `~ostream()` / `~istream()` 的 `= default`
     不需要 `noexcept` 修饰或 try/catch。五种情形（含 `iostream`、移动赋值、拷贝赋值）实测均不
     `terminate`。**取证要点**：必须用「让 `dput()` 抛」的设备才压得到这条路径——
     第 216 条用的是「让 `dflush()` 抛」的设备，而析构期**根本不调用设备的 `dflush()`**
     （见第 331 条），那个探针没触到析构里的写出路径。结论一致，取证已更正
     （探针 `s2_dtor_throw_and_move.cpp`）。

338. **移动之后源对象再析构不二次冲刷、不解空缓冲**：用会记账的设备核对，`dput` 恰好一次，
     移后对象析构时没有任何设备调用。自赋值被 `istream`/`ostream` 的移动与拷贝赋值里的
     `this == &other` 短路挡成恒等操作，脏缓冲既没被丢也没被写两遍。

339. **`endl` 的 "cannot get ctype facet" 抛出路径**（探针 `s3_manip_paths.cpp`）：用一个
     **活的、正常构造的**流（传入 moved-from 的 `locale<char>`）走到该处——此前只在移后流上
     走到过（第 278 条）。空 `shared_ptr` 在 `if (!mp)` 处被拦下，**未解空指针**；转成
     `strfailbit`、**一个字节都没写出**、`clear()` 后可复用。`ostream.h` 那条注释担心的
     「异常路径把 `unitbuf` 永久留下、此后每次写降级为逐字符刷新」**确实不会发生**：失败的
     `endl` 前后 `flags()` 逐位相同。`ends` / `flush` 不需要 ctype facet。

340. **`ws` 的边界**（14 种「前置状态 × 异常掩码」组合实跑）：在已失败的流上只追加
     `strfailbit`；缺 ctype facet 时**不吞字符**（取 facet 在动 `sgetc()` 之前，读游标未动）；
     EOF 上连续三次 `>> ws` 幂等；`eofbit` 已置位但设备仍有数据时照常跳空白且不抛。

341. **六个格式状态操纵符的极值实参**（探针 `p1_bounds.cpp`）：`setw` 负值与 `PTRDIFF_MIN`
     → `strfailbit` 且 `width` **保持旧值**（无半改）；`setprecision` 专测了「若被 mod 256 就会
     落回合法值」的 1024 / −256 / 262，全部被拒，证明范围检查是在**未截断的 `ptrdiff_t`** 上做的、
     不存在绕回洞；`setbase` 的 `8+(1<<32)` 等全部落进「其它取值」一档而没有被截断成 8/10/16
     （第 318 条形参加宽的**运行**验证，该条此前只做了编译期与推理）。填充上限：
     `setw(65538)` 成功、`setw(65539)` → `strfailbit` 且**输出 0 字节**。

342. **整条操纵符链路零静默窄化**（探针 `p2_implicit.cpp`）。**这是前两轮没用过的检测面**：
     本清单其余各条的实测环境是 `-fsanitize=address,undefined`，而 `undefined` **不含**
     `implicit-integer-truncation` / `implicit-integer-sign-change`，二者须显式打开
     `-fsanitize=implicit-conversion`。用 clang 覆盖六个操纵符的全部边界取值（含 `setfill` 的
     全 256 个 `char` 取值），**告警数 0**。逐点对上的代码事实：`operator~(fmtflags)` 自己已把
     结果截回 `uint16_t`，故 `setf` 里 `(old & ~msk) | (fmtfl & msk)` 恒在 `[0,65535]`；
     六个载体的字段与其 setter 形参同宽，唯二的收窄都在值域检查通过之后且是显式转换。

343. **六个格式状态操纵符的载体不可能悬垂**（探针 `p3_lifetime.cpp`）：六者全部
     `is_trivially_copyable_v` 且 `sizeof` 等于其参数本身（无多余引用/指针成员）；`setfill`
     工厂**按值取参**，对 `char&`/`const char&`/`char&&`/`volatile char&` 四种实参 `TFill`
     **一律推导为 `char`**，不可能被推导成引用类型。运行期把载体跨作用域、跨 `vector` 搬家、
     跨线程使用，ASan use-after-scope 零报告。`io_manip.h` 文件级 `@warning` 说的
     「只应在同一完整表达式内使用」**只针对四个 locale 操纵符**，对这六个不适用，且是结构性的。

344. **`setfill` 的字符类型匹配全在声明层面被拒**：`char` 流拒绝
     `wchar_t`/`signed char`/`unsigned char`/`int`/`char8_t`/`char16_t`/`char32_t`/指针/`bool`；
     宽流同构。故 `setfill(0x10000)` 给 `char` 流、`setfill('\xff')` 给 `signed char` 流
     **根本不存在运行期转换路径**，谈不上窄化 UB。约束是 `io_traits` 里的
     `std::same_as<typename T::char_type, TFill>`。

345. **非法位模式打不出非法流状态**：`ios_defs::fmtflags` 是固定底层类型 `std::uint16_t` 的
     枚举且 bit0..bit15 **全部被定义**（`io_base.h:73-88`），故「未定义的高位」这个类别在本库里
     是空的——超出 16 位的整数按模运算落回 16 位（良定义）。bit0..bit15 逐位置位、
     加六种位模式 × 四条插入路径，全部 `state=0x00`，ASan/UBSan 零告警。
     `resetiosflags(0xFFFF)` 只清 `m_flags`，相邻的 `width`/`precision`/`fill` 三个原子成员不变。
     两个位有真实副作用但都是既定契约：`appmode`（bit15）在变长编码管线上导致 `cvtfailbit`
     （可恢复，与 `io_base.h:1413-1419` 的 `@warning` 相符）、`unitbuf`（bit13）只影响刷新时机。

346. **`setfill` 的 `@warning` 与实现逐条相符**：把该 `@warning` 的每一条断言（连同它自己举的
     例子）翻成用例，**30/30 全部相符**（探针 `p6_fillwarn.cpp`）。失败路径一律 `""` +
     `strfailbit`（**零字节输出，不留半截字段**）。

347. **`put_time` 对越界 `std::tm` 字段不做数组索引**（探针 `p3_tm_fields.cpp` +
     控制组 `m1_tm_control.cpp`）。8 种格式串 × 31 组越界 `tm`（`tm_mon=12/-1/±INT_MAX`、
     `tm_mday=32/0/±INT`、`tm_hour=24`、`tm_sec=60`、`tm_year=±INT_MAX`、全字段极值……）
     **全部 `strfailbit` + 零字节输出**，ASan/UBSan 零报告。
     **控制组是必需的**：没有合法 `tm` 的对照时，「全是 `0x8`」本可能只说明那些格式串不受支持；
     补测证实合法 `tm` 在同样 8 个格式串上全部 `state=0x0`，故「越界即拒」是真的字段校验。
     逐字段定位：`tm_mon`/`tm_mday`/`tm_hour`/`tm_sec` 越界时**即使格式串不读该字段**也被拒，
     与 `io_manip.h` put_time 文档「`*tmb` 必须描述一个完整且真实存在的时刻」逐字相符。

348. **`tm_wday` / `tm_yday` 不被读取，故不构成越界索引**：`tm_wday=99` 或 `-1` 配 `"%a"`
     照样输出 `"Mon"`（由 y/m/d 重算，`facet/timeio.h:1083`），**不拿它索引星期名表**。
     这是 `io_manip.h` put_time 文档 `@note` 明写的契约，与 `strftime` 的分歧也已在同一处写明。

349. **`put_money` / `get_money` 的取值域是编译期约束，不是运行期 UB**（探针 `p4_money.cpp`）：
     `bool`/`double`/`long double`/`char_type` 不符的串/非默认 traits 的串/非默认分配器的串
     **全部不可行**（编译期拒绝）；`const int`/`volatile int` 可 put 不可 get，与第 312 条写明的
     取值域一致。极值往返（`INT64_MIN`、`UINT64_MAX`）逐个相同，无有符号溢出。读回越域文本
     （`u64` 读 `-1`、`i64` 读 `9223372036854775808`、`schar` 读 `-129`、`uchar` 读 `256`）
     一律失败且**目标变量保持旧值不变**，边界值正确写入。

350. **四个 locale 操纵符「只在同一完整表达式内使用」的契约属实，安全形式是真安全**
     （探针 `p5_dangle.cpp`，同一份源码编译两个二进制）。安全侧 7/7 通过、ASan（含
     `detect_stack_use_after_return=1`）零报告：`os << put_money(std::string("12345"))`
     （prvalue 实参）、`put_money(make_str())`（函数返回 prvalue）、prvalue 整型、
     `put_time(&t, temp.c_str())`（临时串的 `c_str()`，同一完整表达式）、四者串联、
     `get_money` + `get_time`、以及具名载体配存活实参。即临时实参在完整表达式内被引用是安全的
     ——其生存期延续到完整表达式结束，晚于 `operator<<`。不安全侧按契约翻车：
     `auto man = put_money(make_str()); os << man;`（临时量已死）触发 ASan
     **stack-use-after-scope**，读发生在 `facet/monetary.h:771` 经 `io_manip.h` 的 `swrite`。
     这给出了该 `@warning` 此前缺失的**运行期证据**（此前只有文字契约）。

## 回归与 CI 的分工

351. **回归类验证（sanitizer / valgrind / release / coverage / 安装形态 / clang-tidy）自本轮起
     移交 CI，本地不再重跑。** 已核对：`ci.yml:32-38` 的 "Run tests (Sanitizers)" 就是
     `make test MODE=sanitizer`，而 `test/Makefile:172-173` 的 `test` 目标遍历**全部七个套件**
     ——本地跑一轮 ASan+UBSan 全量是 CI 的**真子集**，且 CI 更强：`g++` + `clang++` 两个编译器
     矩阵、`halt_on_error=1` + `print_stacktrace=1`，另加本地从未跑过的 Valgrind 全量
     （`--leak-check=full`）、coverage 全量、shared 与 header-only 两种安装形态的全量（-O2）、
     clang-tidy。`MODE=debug`（-O0）不在 CI 里，但 coverage job 是 `-O0 --coverage`，
     `-O0` 这一档实质上覆盖到了。
     **约定**：提交成功即视为这些报告已跑，CI 报红时再介入。
     **clang-tidy 这一项本地复现不了，只能取差集**：本机 clang-tidy 22.1 连自己的
     `stddef.h` 都找不到，同一条 CI 命令在 `origin/main` 上就报 32 类 error，而 CI 是绿的
     ——绝对数字全是版本与环境噪声。可信的做法是另开一个 `origin/main` 的 worktree，
     用**同一条命令**跑两边、按「去掉行号后的 error 文本」取差集，只看自己这一改新增了什么。
     第 406 条那个签名就是这么定位到的。
     **两条边界**：(a) 自动触发只在 push 到 `main`/`master` 或 PR **指向**它们时发生，
     在未开 PR 的 feature 分支上提交不等于验证过——但已加 `workflow_dispatch`，可手动补跑
     （第 387 条）；(b) **TSan 仍是 CI 覆盖不到的一项**，属本地职责（见末节）。
     locale 一项已移交 CI，见第 384 条。

352. **基线用例数（`LC_ALL=zh_CN.UTF-8 make test MODE=debug`，七套件全绿 1375/1375，
     退出码 0）**：device 100 / cvt 261 / facet 533 / locale 45 / io 363 / common 54 /
     concur 19。相对 1369 那一版增加的 6 项，是此后几轮修复带的常驻用例：
     `test_iostream_switch_to_put_char_4` / `_5` 与 `test_streambuf_char_direction_1`
     （io +3，提交 `4c9ee676`）、`test_zlib_cvt_bos_4`（cvt +1，第 401 条）、
     `test_concur_iostream_crossed_1`（concur +1，第 398 条）、
     `test_timeio_char_put_20`（facet +1，第 406 条）。
     `MODE=debug`（-O0 不带 coverage）这一档 CI 不建，但 coverage job 是 `-O0 --coverage`，
     `-O0` 实质上覆盖到了；而 locale 一侧自第 384 条起也已进 CI。故本机再跑全量回归的
     剩余价值主要在**迭代速度**，不在覆盖面。

## 货币 locale 的 pattern 全覆盖（第四轮）

353. **`char` 流下装不进一个 `char` 的 `mon_thousands_sep` 会让分组整体消失，这是刻意的，
     且本库是不产生非法 UTF-8 的一方。** 本机有四个 locale 的 `mon_thousands_sep` 是
     U+202F（NARROW NO-BREAK SPACE，字节 `E2 80 AF`）：`nn_NO` / `nb_NO` / `kk_KZ` /
     `fr_CA`。它们在 `char` 流下的输出与 libstdc++ 不同：

     ```
     libstdc++:  99\xe2999\xe2999\xe2999,99     只写出首字节，结果不是合法 UTF-8
     IOv2:       99999999999999,99              整个分组丢弃
     ```

     机制在 `facet_helper.h:176-197` 与 `monetary_details.h:549-582`：分隔符按单个
     `CharT` 存放，`string_to_char_convert` 对装不下的多字节输入返回 `'\0'`；
     `monetary_details.h:572` 的 `if (m_thousands_sep == '\0') m_thousands_sep = ',';`
     成立后，**`:573` 的 `else if (!grouping_raw.empty())` 被跳过，`m_grouping` 始终为空**，
     于是不分组——那个回退用的 `','` 实际上从来没有被用到。两个库各自都读得回自己的输出
     （第 354 条的往返测量），所以这只是字节差异，不是缺陷。

354. **往返测量必须带 libstdc++ 自往返的对照组，否则整张表会被误读。** 只测「本库 `put_money`
     写出、本库 `get_money` 读回」时，24 个桶里出现 **130 处失败**；加上「libstdc++ 写出、
     libstdc++ 读回」这一列之后降到 **8 处**，且 8 处全部是第 294 条那个 `C` locale 负号。
     共同原因：`showbase` 关闭时货币符号不输出，但 pattern 里的 `space` 单元照样输出，
     于是写出的是 `" .00"` 这类前导空格串，**两个库都读不回自己写的东西**。这是标准行为的
     共有后果，不是本库的问题。新覆盖的 21 个桶（括号负数 `en_HK` / `fr_CA`、
     `sign_posn=2/3/4`、`sep_by_space=2`）往返行为与 libstdc++ 完全一致。

355. **第 353 条的行为只发生在「装不进 `char`」这一种情形，不是更广的分组缺陷。** 同样四个
     locale 在 `wchar_t` 流下分组正常（`1 234 567,89`，U+202F 单个 `wchar_t`
     放得下）；单字节分隔符的对照 locale（`en_US` / `de_DE`）宽窄两侧一致。

356. **两个回退的不对称已写进 `monetary_details.h` 的注释**（`char` 特化与宽字符特化各一处）：
     `.` 那个真的会替换字符，而 `,` 那个走的是 `if` 分支、于是跳过了填 `m_grouping` 的
     `else if`，分组被整个关掉、那个 `,` 存了但永远写不出去（第 353 条）。即多字节情形是
     **省略**分隔符而非替换成错的。宽字符那份另注明该分支比 `char` 侧难触发得多
     （U+202F 装得进一个 `wchar_t` / `char32_t`，原样存下、分组照常工作，第 355 条）。

357. **同一机制在 `numeric` 一侧同样成立，且暴露面大一个量级**（探针 `m3_numeric_sep.cpp`）。
     `facet_helper.h:176` 是货币与数值共用的。本机**有 47 个 locale 的数值分隔符是多字节**
     （货币侧只有 4 个），分三种字节序列：U+202F `e2 80 af`（41 个，代表 `br_FR` / `ce_RU` /
     `crh_RU`）、U+2019 `e2 80 99`（5 个，代表 `de_CH` / `de_LI` / `fr_CH`）、
     U+066C `d9 ac`（1 个，`ps_AF`）。行为与第 353 条一致：`char` 流不分组，
     `123456789` 原样写出。**`wchar_t` 侧与 libstdc++ 逐字节相同**——十个 locale
     × 四种取值的对拍里，宽字符一列 **0 处差异**，差异全部落在 `char` 一列（16 处）。
     单字节分隔符的对照 `en_US` / `de_DE` / `C` 两侧全等。

358. **`ps_AF` 暴露出 `decimal_point` 也会走同一条回退，且后果比分隔符更重。** 该 locale 的
     数值 `decimal_point` 是多字节（U+066B），本库回退成 `'.'`，写出 `0.5`；libstdc++ 写出
     `0\xd95`（截断首字节，非法 UTF-8）。分隔符不可表示时的后果是「少写一些东西」，
     小数点不可表示时的后果是「**写出另一个字符**」——但本库两侧用的是同一个 facet，写出的
     `0.5` 也读得回 `0.5`，自洽且可往返，不构成四类缺陷。

359. **「与 libstdc++ 一致」在这条轴上不是良定义的目标。** 同一个 U+202F 分隔符，
     libstdc++ 自己的两个 facet 给出三种结果：`numpunct<char>` narrow 成 ASCII 空格
     （第 357 条 `br_FR` 一行）、`moneypunct<char,false>` 写出原始首字节 `\xe2`
     （第 353 条）、`moneypunct<char,true>` 又写出 ASCII 空格（`m1.out` 的 `kk_KZ intl=1`
     与 `intl=0` 两行对比）。所以第 353、357、358 三条记的是**测得的差异**，不是
     「本库偏离了某个统一基准」。

360. **单字节编码 locale 里的非 ASCII 分隔符不产生 UB，`char` 侧与 libstdc++ 逐字节相同**
     （探针 `m4_nonascii_sep.cpp`，ASan/UBSan + `detect_stack_use_after_return=1`）。
     `facet_helper.h:180` 的 `if (narrow.size() == 1) return narrow[0];` 不做任何转换，
     把原始字节直接存进 `char`。本机有 **156 个 (locale, 字段) 对**走这条分支且字节非 ASCII：
     0xA0 见于 ISO-8859-1/2/5/13/15、CP1251、PT154，0x9A 见于 KOI8-R/U。
     `char` 有符号，所以存进去的是**负值**（−96 / −102）。
     **原本怀疑它会进 `isspace()` 或 ctype 表下标造成越界，实测没有**：九个 charmap 代表
     locale 的整数与货币往返共 30 次解析全部正确，另外喂入纯高字节串
     `"\xa0\x9a\xff\x80"` 也只是正常拒绝，全程零告警。`char` 侧输出与 libstdc++ **0 处差异**。

361. **同一批 locale 的 `wchar_t` 侧有 7 处差异，且本库是自洽的一方。** 以 `cs_CZ`
     （ISO-8859-2）为例，本库宽侧给 **U+00A0**，libstdc++ 给 **U+202F**：
     ```
     IOv2{123 456 789}   std{123 456 789}
     ```
     0xA0 在 ISO-8859-2 里就是 U+00A0，所以本库的宽侧是该字节在**该编码下**的忠实加宽，
     与自己的窄侧（第 360 条测得两库同为 0xA0）互为逆运算。libstdc++ 的宽侧给的是这个
     locale **UTF-8 变体**里的字符，而 U+202F 在 ISO-8859-2 中根本不可表示——即
     **libstdc++ 自己的窄、宽两个 facet 对「分隔符是哪个字符」给出了互相矛盾的答案**。
     `aa_DJ`（ISO-8859-1）与 `be_BY`（CP1251）无差异；`en_US.UTF-8` 对照两侧全等。

## 操纵符与三个流类（第五轮）

覆盖 `io_manip.h` / `istream.h` / `ostream.h` / `iostream.h` 四个文件全文，分五路并行核对。
本节全部结论都有可编译探针 + 阳性对照，ASan/UBSan 零报告；TSan 那条用 clang（本机 gcc 缺
libtsan）。

362. **`IOv2::sync` 不能把格式状态操纵符纳入互斥；文档已写明这一点。** `sync` 的全部
     作用是对流的 `io_mutex()` 加 RAII 锁，而**六个格式状态操纵符的流形式是本库唯一一组
     不取这把锁的操作**——`ostream_operators.h` 的 `stream_v` 分支只有 `try/catch`，迭代器
     分支才有 `lock_guard`；`hex` / `left` / `boolalpha` 那类函数指针操纵符同理。于是持
     `sync(A)` 的线程挡不住另一线程的 `A << setw(n)`，后者径直插进临界区中间。对本库其余
     任何操作（插入、提取、`flush`/`endl`/`ws`、`seek`、`locale(loc)`）而言，单侧 `sync`
     就足以互斥，因为对方必然去取同一把锁。实测：`setw` / `setfill` / `setbase` / 函数指针
     操纵符四种干扰全部改坏受保护的输出，而换成会取锁的普通插入作对照时 `sync` 确实挡住了。
     **这不是数据竞争**（涉及的成员都是原子量），TSan 按定义不会报，只能靠约定。
     `io_manip.h` 文件头的 `@warning` 与 `sync` 的类文档都写明：需要整体原子时，
     **访问同一条流的每个线程**都要用 `sync`，只有一侧加是不够的。

363. **第 341-346 条的无锁前提已由 TSan 证实，0 告警。** 6 线程 × 20000 轮**连跑 6 遍**全部
     零告警；同一份源码加 `-DPOSITIVE_CONTROL`（4 线程无保护 `++g_racy`）稳定报 1 条
     `data race`，证明检测器在同一组编译选项下生效。覆盖的是第 44、83、139、143 条**没有**
     跑过的组合：全 16 位 `setiosflags(0xFFFF)` / `resetiosflags(0xFFFF)` 双向施加；在猛砸
     `m_flags` 的同时读写**相邻的** `width` / `precision` / `fill`（第 345 条此前只有单线程
     证据）；两条**失败**路径 `setw(-1)` / `setprecision(256)` 走 `handle_exception →
     setstate` 在 `io_mutex()` 下写状态位，与无锁 `rdstate()` 读并发；**跨线程共享同一个
     载体对象**（`const setw_t&` / `setfill_t<char>` / `setbase_t`，补上第 343 条只做了
     ASan use-after-scope 的那一半）；真正写出填充的插入（`numeric.h::pad` 在锁内读
     `fill()` / `width()`）与另一线程的 `setfill` / `setw` 并发。结束态 `state=0x0`，
     写出 43718 字节，无挂起、无死锁。

364. **六个格式状态操纵符在全局流对象上工作正常**（第 140 条只覆盖了 `endl` / `ends` /
     `flush`）。`IOv2::cout` / `wcout` / `cin` 走 `io/objects/out_impl.h` 的 `stdout_api` 与
     `in_impl.h` 的 `stdin_api`，**不经过** `ostream` / `istream`，但同样派生
     `ios_state<TChar>` → `ios_base<TChar>`，故六个特化全部可用。实测
     `cout << setfill('*') << setw(8) << setbase(16) << 255` → `******ff`；
     `resetiosflags(basefield) << setiosflags(showpos) << setw(6) << 42` → `***+42`；
     `wcout` 侧同形。失败路径同样只置位：`cout << setw(-1)` → `state=0x8` 且 `width` 保持 0。
     提取侧 `cin >> setw(5) >> setbase(8)` 正常。

365. **`os << flush` 在只有 `devfailbit` 的流上会追加 `strfailbit`，与 `endl` / `ends`
     一致。** 第 137 条测了 `endl` / `ends` / `"abc"` 但漏了 `flush`，第 108 条的流本来就带
     `strfailbit` 看不出是否追加。`ostream_operators::flush()` 第一步就是
     `if (!static_cast<bool>(self)) throw stream_error(...)`，不是「失败态静默返回」。
     用注入 `dput` 抛 `device_error` 的设备实测：`0x2` → 三者**都**得到 `0xa`。

366. **两条失败路径都不留 `width` 残渣，也不留半截输出。**（第 60 条只论证了函数入口的
     `io.width(0)`，第 346 条只测了「零字节输出」。）`os << setw(70000) << 42`（超
     `max_pad_count`）→ `state=0x8`、`width` 归 0、输出 0 字节；`os << setfill('1') <<
     setw(6) << 42`（填充改变读数）→ 同样 `0x8` / `width` 归 0 / 0 字节，**但 `fill` 仍是
     `'1'`**——粘性流状态，与 `setfill` 文档一致；`clear()` 后 `os << 7`（不触发补齐）
     正常输出。成功路径对照 `setw(6) << 42` → `"    42"`、`width` 归 0。

367. **`put_time` / `get_time` 的空指针四种组合全部干净，第 312 条的 `@throw` 清单复验通过。**
     两处检查都在任何解引用与任何 `basic_string_view` 隐式转换之前。`char` 与 `wchar_t`
     两种字符类型 × `(tmb, fmt)` 的四种空/非空组合，三项断言逐例成立：`state == strfailbit`、
     `put_time` 一字节不输出、目标 `std::tm` 与调用前 `memcmp` 逐字节相同。第四种组合
     （两者均非空）由阳性对照走正常路径。

368. **`fmt` 必须是 NTBS，这条契约写在文档里；`%` 后跟多字节字符不越界。** put 侧与 get 侧都把
     `const TChar* fmt` 隐式转成 `basic_string_view`（`timeio::put` / `get` 的形参类型），
     转换走 `char_traits<TChar>::length`，**未以 `\0` 结尾的数组因此被读越界**，ASan 在
     两侧都精确抓到。**这不是缺陷**——`const TChar*` 形式的格式串按 C++ 惯例即 NTBS，
     `std::put_time` / `strftime` 完全同构。两侧的空指针 `@note` 之后各有一条说明，
     并明确「只有空指针会被挡下置位，非空但未终止不在其列」。
     `%` 后跟多字节字符（含单字节负值 `char` 如 `0xff` / `0x80`）不产生任何越界，输出按
     `bad_format` 策略原样透传，往返成立；阴性对照（输入与格式串不匹配）确实 `strfailbit`，
     说明往返是真的字面匹配而非忽略尾随内容。

369. **第 329 条的往返在 `char8_t` / `char32_t` 上同样成立。** 四种字符类型同一矩阵
     （`%Y%` / `%` / `a%` / `%E` / `%O` / `%%` / `%Q` / 完整格式串），put 侧输出与
     `char` / `wchar_t` 逐行相同、`state=0x0`，get 侧读回自身输出全部成功，failures = 0。
     阴性对照（`%Y%` 配输入 `"2024"`）在四种字符类型上都 `strfailbit`。

370. **`convert_to(std::tm&)` 不会留下半写的目标，`days[12]` 的索引不可能越界。**
     前两步 `convert_to(ymd)` 与 `convert_to(hms)` **都写进局部变量**，对 `out` 的第一次
     写入排在其后；唯一的抛出点在 `convert_to(year_month_day&)`，它先 `compute_ymd()`、
     再判 `ok()`、最后才 `out = ymd`。这从代码结构上坐实了第 11 条（此前只有实测）。
     `out.tm_mon = int(unsigned(ymd.month())) - 1` 能走到说明 `ymd.ok()` 为真，而
     `year_month_day::ok()` 蕴含 `month().ok()` 即 `[1,12]`，故索引恒在 `[0,11]`。

371. **`get_time` 用越界的 `std::tm` 回退值播种解析上下文时无有符号溢出、无越界结果。**
     这是第 347 条（查的是 `put_time`）与第 15 条（只用 `std::tm t{}`）之外的一面：
     `get_time` 的契约只要求 `*tmb` **已初始化**，不要求字段在范围内——越界字段「会先归一化」
     是写进文档的行为，故 `INT_MAX` / `INT_MIN` 属契约之内，必须不产生 UB。21 组实测
     （六个字段各自取极值、全部取极值 × 六种格式串）UBSan 与 ASan 全程 0 告警。
     逐行核对算术：`(int64(tm_year)+1900)*12 + tm_mon` 峰值约 2.8e10、`tod` 峰值约 7.9e12、
     `tm_mday-1+day_carry` 峰值约 2.2e9，全部先提升到 `std::int64_t` 再运算；年被夹进
     `[year::min(), year::max()]`、日偏移夹进 `[-4e6, 4e6]`（故 `static_cast<int>` 不截断）、
     结果再夹进 `[cal_min, cal_max]`。探针另断言成功时回写的九个字段全部落在合法区间，
     0 例违反——这同时是第 370 条 `days[tm_mon]` 索引安全的运行期佐证。

372. **`switch_to_put()` 的「seek 在 clear 之前」不变量在 `iostream` 这一层可观测地成立**
     （已有常驻用例，见第 393 条）
     （第 324 条此前只有一句注释）。用可注入 `dtell`/`dseek`/`drseek`/`dput`/`dflush` 失败
     并计数的外覆设备实测：注入 `dseek` 失败时 `dseek` **确实被调用过**（计数 +1，排除
     「在 seek 之前就失败」的假阳性），失败后 `tell()` 在 `clear()` 后仍与调用前逐字节相同，
     读缓冲区里那个定界符**完整保留**；替换式 `putback('Z')` 后同理；注入 `dtell` 失败时
     `dseek` 调用数增量为 **0**（证明顺序是先 `tell` 后 `seek`）；失败态早返回时两者增量
     均为 0、状态位不变。
     **状态位归属分两支，诊断详略不同**：第 321/323 条与 `iostream.h` 的 `@warning` 描述的是
     **转换器**不支持定位那一支，落 `cvtfailbit`；而**设备**级 `dseek` / `dtell` 抛
     `device_error` 时，`base_streambuf::switch_to_put()` 的 `catch (const cvt_error&)`
     **不匹配**，原始异常直接穿出，落 `devfailbit`，那条带上下文的 "cannot reposition N
     buffered/put-back character(s)" 包装消息**不会**生成。两支的状态位归属都正确
     （设备坏了就是 `devfailbit`），第 281 条的「状态位恰好一位」与此一致。

373. **不可定位（non-positionable）双向管线的造法：不需要写假 cvt 内核**（该设备已进
     `test/support/`，见第 393 条）**。** `root_cvt` 把
     `tell`/`seek`/`rseek` 用 `requires (dev_cpt::support_positioning<device_type>)` 门控，
     而 `switch_to_get`/`switch_to_put` **不**门控，所以**一个不提供 `dtell`/`dsize`/
     `dseek`/`drseek` 的双向设备**就直接得到「可切方向、不可定位」的管线；`iostream` 的
     类模板约束（只要 get+put）与 `streambuf` 的 `cvt_direction_capable`（只要
     `support_io_switch`）都放行。据此实测，`iostream.h` 那两条 `@warning` 的**每一句**都成立：
     不支持定位 → 抛 `cvt_error`、落 `cvtfailbit` 且是**唯一**一位；抛出早于 `clear()`，
     故压回内容**反而完整保留**（读回的是定界符而非底层数据）；读缓冲区为空时不需要定位、
     切向成功；经 tie 刷新时只置**本流**的位，发起方一位不动、也拿不到异常；原始异常存进
     本流的 `m_exp_cvt_fail`，取回的是带上下文的原文而非 `clear()` 的兜底消息（第 321 条的
     判据）；`clear()` 之后绑定方再写一次就会再次置位；tie 目标的异常掩码在这条路径上不生效，
     **且暂存槽仍在**——`clear()` 的重抛分支会 `std::exchange` 掉槽位，但重抛出来的异常随即
     被 `try_flush` 的 `catch(...)` 接住交给 `handle_exception<true>`，后者在槽位为空时把它
     **重新存回去**，净效果与第 321 条的期望一致；门槛比看上去低，一次普通 `>> std::string`
     就够，不需要显式 `peek()`/`putback()`。
     此管线上的 `detach()` 不置任何位（`streambuf::detach()` 有意吞掉重定位失败），随后
     `switch_to_put`/`switch_to_get`/`flush`/`<<` 六项均 `state=0`，`attach()` 后正常读回
     ——把第 334 条从 `mem_device` 推广到了不可定位设备。
     注意此时**先抛的是 `tell()` 不是 `seek()`**：`switch_to_put()` 的 `try` 块同时罩住两者，
     而 `runtime_cvt::tell()` 在内核不可定位时就已抛出。`iostream.h` 的主句因此写的是中性的
     「抛 `cvt_error`」，另有括注点明真正的抛出点。

374. **构造 / 移动 / 交换 / `detach()` 的边界，8 组全部按契约成立**（`istream` + `iostream` +
     `ostream`，`ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1`，
     ASan+UBSan+LSan 零报告）：
     - `istream` 自移动 + 自拷贝赋值：`if (this != &other)` / `if (this == &other)` 短路生效，
       tie 边保持、读缓冲区里 `peek()` 出的字符没丢。
     - `iostream` 带**脏写缓冲**自赋值：期间 `dput` 调用次数增量为 **0**（既没被丢也没被写
       两遍），随后 `flush()` 落盘正确。补上第 338 条只用记账设备核过 `istream`/`ostream`
       的那一半。
     - **`std::swap`**（两个类都没有成员 `swap()`，走「移动构造 + 两次移动赋值」）：内容正确
       互换，**两端的 tie 边都被清空**——这是第 34 条的既定语义（移动清 tie），但**调用方
       在 swap 之后必须重设 tie**。自 swap 也是恒等。
     - 移后流上 22 个操作全跑一遍（含 `switch_to_put`/`switch_to_get`/`>> ws`/`<< endl`/
       `seek`/`rseek`/`attach`/`tie`）不崩溃、`state=0x4`，随后赋值复活成功。与第 75 条一致，
       并把 `switch_to_*` 与 `>> ws` 补进该清单。
     - `detach()` 之后 `switch_to_put`/`switch_to_get`/`flush`/`peek`/`<<`/`tell` 全部安全，
       **且对一个已 detach 的流做拷贝构造、移动构造、拷贝赋值也都安全**（第 334 条没测这三样）。
     - **拷贝赋值的强异常保证有了运行期证据**：让源设备的拷贝构造抛出，`istream tmp(other);`
       抛出后 `*this` 的状态位与设备内容**逐字节未变**，流仍可用。此前第 31/305 条只有语言
       规则论证 + 一次 `file_device` 观察。
     - 自拷贝赋值 + 「拷贝必抛」的设备：**不抛**，证明短路发生在任何拷贝之前。
     - 构造中途抛出（`dtell` 失败使转换器初始化失败）：异常从成员初始化列表向外传播，
       LSan 零泄漏。

375. **`out_flusher<T>::try_flush()` 三次 `try_lock` 全失败后静默返回，是刻意的且已文档化，
     可观测后果已实测。** 三处文档措辞一致（`out_flusher::try_flush()` 的 brief、
     `abs_flusher::try_flush()` 的 `@warning`、`tie()` setter 的 `@warning`）：放弃时不留任何
     pending，那次 tie 刷新被整个跳过；提高重试次数买不到安全，真正的 AB-BA 里对方正阻塞在
     本线程持有的锁上，每次尝试都注定失败。实测（target 用**真文件设备**——`mem_device`
     在这里不能用，它给转换器的是直写的 put 缓冲，字节根本不经过 flush 就已在设备里）：
     无竞争的阳性对照下 6 字节在 `writer << "DATA"` 这一步落盘；竞争线程占死锁时
     `on_disk=0`、**两条流都仍报 `good()`、状态位全 0**，竞争线程离开后**不做任何补偿**，
     最终兜底只有 target 自己的析构。即**调用方没有任何编程手段能观测到「这次 tie 刷新被
     跳过了」**。这是刻意的取舍，换来「用户看不见的 tie 锁边永不成为等待边」（第 43 条）。

376. **`~ostream()` 对设备恰好 `dput` 一次、`dflush` 零次；析构中 flush 失败静默丢数据。**
     用只实现 `dput`/`dflush` 且计数器放在设备外面（能活过设备析构）的记账设备实测：
     `dtor only : dput=1 dflush=0`，而显式 `flush()` 是 `dput=1 dflush=1`。这是第 331 条那条
     契约的直接实测（此前只有读码结论 + `iostream` 侧探针）。析构中 `dput` 抛出时：
     **不 `terminate`、不抛、不置位、数据静默丢失**——抛出被 `~root_cvt` 的 catch-all 吞掉，
     故 `~ostream() = default` 既不需要 `noexcept` 也不需要 try/catch（与第 337 条一致，
     取证独立重做）。这一段写在 `~ostream()` 的文档里。

377. **`detach()` 的边界全部安全**（第 0 组为阳性对照：普通 detach 必须取回字节）：
     **连续两次 `detach()`** 在 `mem_device` 上第二次得到空 payload、不崩不 UB，在
     `ofile_device` 上第二次拿到的是移后设备、随即析构也安全；`detach()` 后让流析构，
     `~root_cvt` 的那次 flush 作用在空缓冲上、无副作用；`detach()` → `attach()` → 再写 →
     再 `detach()` 两次的字节互不串台；**detach 之后继续写**再 `detach()`，字节进了内部那个
     默认构造的设备、可由下一次 `detach()` 取回，与 `detach()` 的 `@note` 逐字相符。
     `root_cvt::detach()` 无论清理是否抛出都无条件复位 `m_bos_len`/`m_buf_cur`/`m_buf_end`/
     `m_io_status`，因此**不存在「残留字节在析构时被写第二遍」**这条路。第 334 条只覆盖到
     `detach()` 之后的六个操作，双重 `detach()` 与 `detach()` 之后析构由这里补齐。

378. **两个 `noexcept` 名副其实，不是掩盖 `terminate` 的空头支票。** `ostream(ostream&&)
     noexcept = default` 与手写的 `operator=(ostream&&) noexcept` 都是「声明上 noexcept」，
     body 若真能抛就是 `std::terminate`；而拷贝赋值里那句
     `static_assert(std::is_nothrow_move_assignable_v<...>)` 只能挡住「有人把 `noexcept`
     关键字删掉」，挡不住成员的移动本身会抛。逐成员编译期核对：`mem_device` / `file_device` /
     `locale<char>` / `ostreambuf<...>` / 三个流类的移动构造与移动赋值**全部**为 `noexcept`
     （`io_device` 概念本身就要求设备的移动 `noexcept`）。
     运行期（`ofile_device` 是 move-only 内核）：`ostream` 的拷贝构造**没有约束**，
     `std::is_copy_constructible_v` 对 move-only 内核也报 `true`——但这**不是**「编译不过却
     谎称能拷」，实例化能编过，失败发生在**运行期**、抛 `cvt_error`，与
     `operator=(const ostream&)` 文档那句 "A copy always throws on a move-only kernel" 对得上。
     拷贝赋值的强异常保证成立：抛出发生在 `ostream tmp(other)` 这一步，`*this` 一字未改、
     两侧状态位都还是 `0x0`。自拷贝赋值与自移动赋值都被短路成恒等操作，move-only 内核上
     自拷贝也不抛。

379. **移动赋值不会丢掉目标流的待刷字节。** `m_streambuf = std::move(other.m_streambuf)`
     最终走到 `root_cvt::operator=(root_cvt&&) noexcept`，该函数**在覆盖前先 `flush()`
     目标自己的缓冲**（异常被吞），写进目标**原来**的设备；拷贝赋值一侧同理（先 `flush()`
     再 copy-and-move）。所以「赋值整体替换设备」不伴随目标待刷数据的静默丢弃——丢的只有
     flush 本身失败那一种，且与第 376 条的析构路径同构：静默、无位。

380. **生命周期收尾的冲刷不受状态位约束；这与标准库同构，且已文档化。** 四条路径都是
     **流**这一层的接口（设备没有 `detach()`），冲刷动作都发生在下一层的 `root_cvt` 上、
     作用于**转换器的 put 缓冲**：

     | 流这一层的入口 | 看状态位？ | 调设备 `dflush()`？ |
     |---|---|---|
     | `ostream::flush()` | **是** | 是 |
     | `~ostream()` → `~root_cvt()` | 否 | 否 |
     | `ostream::detach()` → `root_cvt::detach()` | 否 | 否 |
     | `ostream::operator=` → `root_cvt::operator=` | 否 | 否 |

     即 `flush()` 里那句 `if (!static_cast<bool>(self)) throw stream_error(...)` 是**唯一**
     一道守卫，另外三条绕过了它；`root_cvt::flush()` 的全部内容只是 `m_device.dput(...)`，
     **从不调 `dflush()`**。实测：失败态下 `flush()` 后 `dput=0`、盘上 0 字节，
     而 `~ostream` 后 `dput=1`、盘上 3 字节；失败态 `detach()` 同样取回那批字节。
     **与 libstdc++ 对拍（GCC 15）**：`std::ofstream` 上写入后人为置失败位，`flush()` 盘上
     0 字节、`~ofstream()` 3 字节、显式 `close()` 3 字节、移动赋值 3 字节；用计数
     `streambuf` 直接看，`good()` 时 `sync` 调用数为 1（阳性对照），failbit / badbit 下
     增量为 **0**——**标准的 `flush()` 在失败态下根本不到达 streambuf**。机制同源：
     `iostate` 挂在 `basic_ios` 上、`basic_filebuf` 看不到它，析构走的是 streambuf 那条线
     （`~basic_ofstream()` → `~basic_filebuf()` → `close()` → 冲刷 put 区），链上无处可查
     状态位；本库的分层一样，`root_cvt` 拿不到 `ios_state`。**两处差异本库都不落下风**：
     (a) 标准还多一条更宽的路径——显式 `basic_ofstream::close()` 也不看状态位；本库没有
     `close()`，最接近的 `detach()` 把清理阶段的首个异常以 `exception_ptr` 交还调用方，
     而标准这边析构吞异常、`close()` 只在 `rdbuf()->close()` 返回空指针时置 `failbit`。
     (b) 判据不同：标准用 `good()`，故**单独的 `eofbit` 也会让 `flush()` 什么都不做**；
     本库的 `operator bool` 是 `(s == 0) || (s == eofbit)`，`eofbit` 单独不算失败、`flush()`
     照做。文档正文在 `ostream_operators::flush` 的 `@warning` + `@note`，`detach()`、
     两个析构函数、赋值那组文档均交叉引用它。

381. **四个文件的机械项与 include 全部干净。** 无行尾空白、无连续空行、均以 `\n` 结尾、
     无 tab、无 CRLF。include 都是「本地组 → C++ 组」两组、组间空行分隔、**组内字母序**，
     无 C 语言库组与其他组（按本项目约定 `cstddef` / `cstdint` / `ctime` 计入 C++ 组）。
     逐个核过是否被用到（**以剥离注释后的代码为准**，避免被 Doxygen 散文误导）：三个流类
     文件的四个 C++ 头（`<exception>` / `<mutex>` / `<type_traits>` / `<utility>`）均有实际
     使用；`io_manip.h` 的 `<concepts>` / `<cstddef>`（`std::ptrdiff_t`）/ `<ctime>` /
     `<iterator>`（**`std::sentinel_for`**）/ `<string>` / `<type_traits>` 均必需，
     而 `<cstdint>` 零命中（`std::uint8_t` 只出现在文档散文里，说的是 `ios_base::precision`
     在**另一个文件**里的存储宽度；本文件的精度形参与成员用的是 `std::ptrdiff_t`），
     故不在 include 列表中。

382. **四个文件的英文拼写与双语标记全部干净。** 拼写：按 `@lang{}` / `@endif` 边界切出英文
     区段（中文整段丢弃），只留注释行，剥掉反引号里的代码、Doxygen 命令与 URL，
     `hunspell -d en_US` 取未识别词表（1146 个不同词），逐个回原文核对——51 个未识别词
     **全部**是技术术语、本库标识符、刻意造词（`erroring` / `unprobeable` /
     `unsubstituted`）或标准条款标签（`indet` / `argdep` / `virtuals`，均与 N4950 实际节名
     一致）。另做三轮：连字符复合词逐段拆开单查；20 余个高频误拼（`seperat*` / `recieve` /
     `occured` / `lenght` 等）与英式拼法（`behaviour` / `initialise` 等，该库统一美式）
     零命中；代码行里的标识符按 `_` 与 camelCase 拆成子词（168 个）再查，未识别的 68 个
     全是标准库或本库既有名字（含 `cerb`——标准 `[ostream.sentry]` 示例里哨兵变量的惯用名）。
     双语标记：四个文件都满足 `#@endif == #@lang{ZH} + #@lang{EN}`
     （io_manip 29/29/58、istream 10/10/20、ostream 14/14/28、iostream 10/10/20）。
     **光看总数相等挡不住「多一个、少一个、位置错开」这种互相抵消的排布**，故另用深度计数器
     按出现顺序走一遍（`@lang{X}` 入栈、`@endif` 出栈），检查从不出现负深度（孤立 `@endif`）、
     从不出现嵌套、文件末尾深度归零——四个文件全部通过，即配对不仅数目相等、**顺序也严格是
     `@lang{X} … @endif` 交替**。

383. **`ostream.h` 其余逐点核对（读码）无新问题。** `out_sentry` 的模板实参
     （`involve_input=false`、`is_std=false`）与 `istream.h`、`iostream.h`、
     `objects/out_impl.h` 的四处取值构成自洽的一组（第 27 条）；成员声明顺序
     `m_streambuf` 在 `m_locale` 之前，与类文档那段「分层顺序」的论证一致，移动赋值的赋值
     顺序同向；私有加锁委托构造的形参类型与调用处 `std::lock_guard{other.io_mutex()}` 的
     CTAD 结果逐字匹配；`copyable_mutex` 的赋值是空操作，故移动赋值全程持有的那把
     `this->io_mutex()` 不会在 `ios_state::operator=` 那一步被换掉或销毁（第 32 条）；
     **`abs_flusher` 有虚析构**，经 tie 指针 `delete` 不是 UB；
     `std::this_thread::yield()` 用到的 `<thread>` 已在 `ostream_operators.h` 直接包含，
     不靠传递包含。

## locale 覆盖已移交 CI（第五轮）

384. **CI 现在覆盖 12 个 locale，做法是复用已编译的二进制、只换 `LC_ALL`，一行编译都不多做。**
     `build` job 在 release 与 sanitizer 两个编译步骤之后各加一步重跑：release 跑全部 12 个，
     sanitizer 只跑 3 个（单次成本高好几倍，只挑 sanitizer 真能说上话的：单字节高位分隔符
     `cs_CZ.ISO-8859-2`、非 UTF-8 多字节 charmap `zh_CN.GBK`、多字节小数点 `ps_AF.UTF-8`）。
     清单是**每条轴一个代表**，不是从镜像里那约 300 个 locale 抽样——同一形状的第二个 locale
     走的是同一条代码路径，只费时间不报新东西。每一项在**至少一个 facet 会读的字段**上与其余
     全部不同：
     - `C.UTF-8` UTF-8 charmap 配 C 惯例（无千分位、符号位置未指定）
     - `en_US.UTF-8` `.` 小数点 `,` 千分位；`de_DE.UTF-8` 两者互换（抓写死 `.` 的地方）
     - `de_CH.UTF-8` `.` 小数点但千分位是 U+2019，多字节，走**数值**侧的「装不进一个 `char`」
       回退（第 357 条）；`fr_CA.UTF-8` 同一回退但走**货币**侧、分隔符是 U+202F（第 353 条）
     - `ps_AF.UTF-8` 多字节**小数点**——该回退唯一会**替换字符**而不是省略的情形，且窄宽两侧
       行为不同（第 358 条）
     - `tr_TR.UTF-8` 无点小写 i，`toupper('i') != 'I'`，打破 ctype 底下是 ASCII 的假定
     - `en_HK.UTF-8` `n_sign_posn=0`，负数货币加括号（第 354 条新覆盖的桶）
     - `zh_CN.GBK` 非 UTF-8 多字节 charmap
     - `cs_CZ.ISO-8859-2`（0xA0）与 `ru_RU.KOI8-R`（0x9A）分隔符是单个高位字节，存进有符号
       `char` 即为负值（第 360 条）
     - `zh_CN.UTF-8` 形状与 `en_US` 同族但符号位置不同，且是本机默认——PR #292 那次把 locale
       差异误判成代码缺陷的正是这一档
     纯 `C` 不在清单里：容器不设 `LANG`，workflow 其余各 job 本来就跑在那里。
     **每个 locale 用前先验可用性**：glibc 对不存在的 locale **不返回非零**，只往 stderr 写
     警告然后静默退回 `C`，所以护栏判的是 **stderr 非空**而不是退出码——否则镜像哪天掉了
     `locales-all`，整个矩阵会静默退化成 N 遍 `C`，全绿且什么都没测。另注意 `locale -a` 打印的
     是规范化名（`de_DE.iso88591`），拿它去精确匹配 `de_DE.ISO-8859-1` 会得出相反结论。

385. **测试此前不是 locale 无关的，现已显式钉住 `"C"`。** 16 个 locale 的横扫里**曾有 8 个是
     红的**，全部栽在同一个假定上，且**库在每一例都是对的**：本库的默认 locale **跟环境走**
     （`LC_ALL` → 类别变量 → `LANG` → `"C"`，见 `ori_facet_buf.h::resolve_locale`），
     **这与标准不同**——标准里流的默认 locale 是全局 locale，启动时为 classic `"C"`，不看环境。
     所以 `"2.456e3"` 在小数点不是 `.` 的 locale 下读成 `2` 是正确行为。错的是测试：
     构造流时不指定 locale，却拿 C 形状的字面量去比对，**读写两个方向都有**
     （`test/io/istream/extractors_arithmetic` 与 `test/io/ostream/inserters_arithmetic`
     四个文件）。等于在赌开发者的运行环境，而 `C` 与 `zh_CN` 下小数点恰好都是 `.`，故一直没暴露。
     现在这些流构造都传显式的 `IOv2::locale<charT>("C")`（`test/io` 下已有的写法）。
     三条边界值得记：
     - **只有构造需要钉**。`attach()` 只收设备、不动流的 locale，那些点本来就不需要改。
     - 流构造有**圆括号与花括号两种写法**，只改一种会让失败一层层往后挪——`VERIFY` 抛异常即
       中止，后面的用例根本跑不到。
     - `ps_AF` 红在**宽字符**那一份：它的多字节小数点在 `char` 侧回退成 `.`（窄字符测试因此
       本来就过），宽侧不回退才暴露。与第 358 条的机制严丝合缝。

386. **全量横扫的当前结果：16 个 locale × 7 个套件全绿，零 FAIL**（`bin_debug`，只换 `LC_ALL`
     不重编）。覆盖 `C` / `C.UTF-8` / `en_US` / `zh_CN` / `ja_JP` / `de_DE` / `fr_CA` / `nb_NO` /
     `de_CH` / `ps_AF` / `tr_TR` / `en_HK` / `de_DE.ISO-8859-1` / `cs_CZ.ISO-8859-2` /
     `zh_CN.GBK` / `ru_RU.KOI8-R`。CI 清单的 12 个是其中按轴去重的子集
     （`ja_JP` 与 `zh_CN` 逐字段同形、`nb_NO` 与 `fr_CA` 同为 U+202F，各留一个）。
     单个 locale 走完七套件约 37 秒（`-O0`），其中 `test_facet` 一个就占 26 秒。

387. **`ci.yml` 已加 `workflow_dispatch` 触发器**，第 351 条边界 (a) 因此收窄：未开 PR 的
     feature 分支可以手动触发验证，不必等到开 PR 才第一次跑 CI。

388. **`io/utilities/ostream_operators.h` 不包含 `<utility>`，其本地头组按字母序排列。**
     `std::exchange` 与 `std::move_only_function` 只出现在 Doxygen 散文里（前者还是在描述
     **另一个文件**里 `clear()` 的实现），与第 381 条 `<cstdint>` 是同一个陷阱：判据是
     **剥离注释后的代码**，不是散文。

---

## 设备方向约束在流缓冲区层的补齐（第六轮）

389. **`base_streambuf` 不带工厂的两个构造函数现在也查设备方向，判据与流层同为 `dev_cpt` 原语。**
     此前设备方向只在 `istream`/`ostream`/`iostream` 的**类级**约束上查，流缓冲区一侧不查——
     带工厂的路径因第 390 条的传递性顺带查到了，不带工厂的那两个则完全没查。流缓冲区是公开
     API，直接使用它的调用方不经过流的类级约束，于是 `istreambuf<只写设备, char>` 能编译通过，
     失败推迟到**构造时**由 `runtime_cvt` 抛 `cvt_error`（单向缓冲区在 `init_cvt()` 里发现初始
     方向不符，转而请求方向切换）。三个流的类级约束一字未动。

390. **管线的读写能力永远不超过它下面的设备，实测九格逐格相同。** 用真实工厂
     `code_cvt_creator<char, char32_t>` 建管线，`cvt_fits_direction` 与「设备原语按方向组合」
     在 {get-only, put-only, 双向} × {istreambuf, ostreambuf, streambuf} 九格上取值完全一致。
     这解释了为什么带工厂那条路径本来就没有洞。

391. **不带工厂的输出构造函数同时服务 `ostreambuf` 与双向 `streambuf`，那里只查 `support_put`
     是错的。** 两个不带工厂的构造函数调用签名相同（输入那个的 `has_in_buf` 有默认实参），
     靠互斥的 `requires` 划分：`IsIn && !IsOut` 归输入那个（它多一个 `has_in_buf` 开关，可建
     带回读缓冲的 `rb_root_cvt`），`IsOut` 归输出那个（一律 `no_rb_root_cvt`），
     类上的 `requires (IsIn || IsOut)` 排除 (false,false)，三种合法组合各归一处。
     于是 (true,true) 走的是输出那个，其约束必须写成
     `IsOut && support_put && (!IsIn || support_get)`。**实测**：漏掉最后一项时
     `streambuf<只写设备, char>` 变为可构造，而 `iostream` 仍在类级拒绝——两层重新对不上。

392. **判据不具名，直接用 `dev_cpt` 原语；两条被否掉的写法记在此以免重来。**
     五个施加点里三个是方向固定的流类，`support_get` / `support_put` 直接写最清楚；只有流缓冲区
     那两个构造函数是泛型的，其中一个的表达式也只有一行——为两个点起名不划算。
     - **不要把判据定义成「把 `cvt_direction_capable` 施加在根转换器上」**（理由通常是防止与
       `root_cvt` 漂移）：九格真值表与直接用 `dev_cpt` 原语**完全相同**，那层间接没有任何可观察
       收益，代价却是为回答「设备有没有 `dget`」而实例化 `root_cvt<D,true>` 与 `root_cvt<D,false>`
       两个重模板、诊断多穿几层。
     - **不要把约束上移到类**：类级还要求三个方向别名的基类列表、以及
       `common/streambuf_defs.h` 里它们的前向声明与偏特化统统逐字重复（重声明的约束必须一致，
       否则 "redeclaration ... with different constraints"），并把那个只依赖 `device_concepts.h`
       的轻量头文件的包含面推大。落在构造函数上的代价是 `istreambuf<坏设备, char>` 仍可被具名，
       只是没有可用的构造函数。
     - **不能**把三个方向别名改写成 `using istreambuf = base_streambuf<...>`——推导指引不能
       给别名模板写（[temp.deduct.guide]/1 要求是类模板），而 `TChar` 没有任何构造函数推得出来，
       全靠文件末尾那组显式指引；`istream.h`/`ostream.h` 六处构造函数约束里的
       `decltype(istreambuf{...})` 也会随之失效。GCC 15 支持别名 CTAD，但它转换的是
       `base_streambuf` 自己的指引，而后者定不出 `TChar`。

     **常驻用例 `test_streambuf_char_direction_1`**：{get-only, put-only, 双向} ×
     {三个流缓冲区, 三个流} 共 18 格，含正对照，两层逐格一致。写法上有一处约束——**流那六格
     不能用 `std::is_constructible_v`**：流的约束在类上，`istream<只写设备, char>` 根本不是
     类型，写进 `is_constructible_v` 是硬错误而不是 `false`，必须让设备保持模板形参、用
     template-template 的 `requires` 概念在替换语境里查。流缓冲区那六格两种写法都可以。

393. **`switch_to_put()` 的两类失败路径已有常驻用例**，第 372/373 条的探针结论不再只存在于
     探针里：`test_iostream_switch_to_put_char_4`（设备级，落 `devfailbit`）与 `_5`
     （转换器级，落 `cvtfailbit`）。所需的两个设备已进 `test/support/`：
     - **`injectable_device<CharT>`**：可运行期注入 `dget`/`dput`/`dflush`/`dtell`/`dseek`/
       `drseek` 失败，**并对每个原语计数**。计数是顺序断言可观测的前提——只注入 `dseek` 失败
       无法与「根本没走到 `dseek` 就失败了」区分开。开关与计数放在**共享块**里（`shared_ptr`），
       因为流按值取走设备，测试得对流内那份副本保有句柄。
     - **`no_seek_device<CharT>`**：双向但**不提供**四个定位原语，直接得到「可切方向、不可定位」
       的管线，不需要假 cvt 内核（第 373 条的造法）。
     实测复现：注入 `dseek` 失败时 `dseek` 计数 +1、`devfailbit` 恰一位、`clear()` 后 `tell()`
     与调用前相同且读缓冲里的定界符完整保留；`putback('Z')` 替换后同理，读回 `'Z'`；改注入
     `dtell` 失败则 `dseek` 增量为 **0**（钉住顺序是先 tell 后 seek）；已失败流二次调用两个
     增量均为 0、状态位不变；不可定位管线上 `cvtfailbit` 恰一位、抛出早于 `clear()` 故缓冲内容
     保留、读缓冲为空时切换成功、`clear()` 后再切会再次置位、重抛消息是带上下文的
     "cannot reposition N buffered/put-back character(s)…" 而非 `clear()` 的兜底文案。
     **只做了 `char` 一侧**：这几条失败路径与字符类型无关，宽字符侧未镜像。
     `tell()` 返回的是 `std::optional<std::size_t>` 而非 `std::size_t`，写探针时按后者传进
     `printf` 的可变参数会连带错位后续实参。

---

## locale 继承与 `ostream` 的半构造（第七轮）

394. **`test/` 里默默继承进程 locale 的流构造点已逐一查清：502 处构造中 246 处继承，但没有一处
     真的依赖它。** 判据按原定的「流构造处有没有显式 locale 实参」执行，机械化时补了两处它本身
     没说、但不补就会误判的东西：
     - 要算上**构造之后**的 `obj.locale(...)` / `imbue(...)`。`setw`/`setbase`/`setprecision`/
       `basefield` 四个文件按构造实参查全是误报，它们是构造后设的。
     - 要算上**流类型是模板形参**的构造（`T ofstrm{TDevice{...}}`）。只匹配 `IOv2::*stream`
       会漏掉 `seek`/`attach` 等文件里成对跑 `istream`/`iostream` 的那种写法，实际漏掉一大半。

     246 处集中在 47 个文件，**全部是字符级 I/O**：`get`/`getline`/`ignore`/`peek`/`putback`/
     `read`/`seek`/`tell`/`ws`/`flush`/`inserters_character`/`extractors_character`/
     `switch_to_*`/`sync`。这些路径不读 numeric / monetary / timeio 的任何字段，只在跳空白时
     碰 `ctype`，而测试数据全是 ASCII，`isspace` 在所有被测 locale 上一致。真让算术值过 facet 的
     只有 `test_io_base_fill`（`42` / `-42`）、两个 `ostream/seek`（循环下标 0–9）、两个
     `attach`（`>> v` 读 `"42"`）——位数够不着分组，也不含小数点与符号。**所以这条不是「跑得过
     但仍是假定」，是判据本身比要查的东西宽**：没有显式 locale 实参 ≠ 依赖 locale。
     实测背书：CI 那 12 个 locale 加纯 C，共 13 × 7 个二进制全量重跑，`fail=0`。

395. **本库的整数分组不按进制门控**（查上一条时的副产品，与 std 一致）：`insert_int` 里
     `if (!m_grouping.empty())` 之前没有 `basefield` 判断，八进制与十六进制同样插分隔符。
     `test_io_base_manipulators_basefield` 用自带的 `MyNP`（grouping `{3}`、分隔符 `' '`）
     把这件事钉成了常驻断言：`0726 746 425`、`0x12 345 678`。

396. **`ostream` 不带工厂的构造只查询设备一件事，失败路径不泄漏。** 用
     `injectable_device<char>` 逐一注入六个原语的失败：构造期间只有 `dtell` 被调用（两次），
     注入它则异常从成员初始化列表向外传播，ASan+LSan 零报告；其余五个原语构造根本不碰，注入
     后构造照常成功。补上了第 374 条只在 `istream`/`iostream` 上做过、第 45 条只用未打开的
     `file_device` 做过的这一格。带工厂的那条路径由第 400 条的契约保证。
     探针带正对照：干净构造必须成功，末尾故意漏 4096 字节必须被报告，否则「零报告」不说明
     LSan 是开着的。

397. **`ws` 扩展点在 `iostream` 上的方向与锁语义成立，八格实测。** 这是只有双向流能证伪的
     两条声明：
     - **方向靠成员的有无表达，不靠约束。** `io_traits<TChar, ws_t>` 只有 `sread`，
       `iostream << ws` 因此无可行重载——`iostream` 同时满足两个流概念，若靠约束区分，
       两个方向都会调成功。六个 `static_assert`（三个流类 × 两个方向）成立，
       其中 `!requires { iost{} << IOv2::ws; }` 是关键那格。
     - **锁由 `sread` 自己取，且 `catch` 在同一把锁内。** `io_mutex()` 是
       `copyable_mutex<std::recursive_mutex>`，故 `IOv2::sync(ios)` 持锁时再 `>> ws` 不死锁
       （实测跑完并读到期望字符）。失败置位发生在解锁之前：空设备上 `>> ws` 之后，另一个线程
       只能在 `sread` 返回后拿到锁，它读到的 `rdstate` 与主线程一致（`0x1`）。
     其余四格：`is >> noskipws >> ws` 照样跳空白（哨兵不读 `skipws` 标志，这条路径传字面量
     `false`）；处于**输出**模式、写缓冲脏的 `iostream` 上 `>> ws` 会切方向且已写内容不丢，
     `rdstate` 保持 `0x0`；掩码命中时**逃出去的是原异常**，两次 `handle_exception` 之后的状态位
     与只处理一次时相同（`0x1 == 0x1`）。**逃出的类型是 `eof_error` 而非 `stream_error`**
     ——EOF 类别不存原异常指针（`io_base.h` 明文），`clear()` 现造一个抛出，所以在
     `is >> ws` 外面只写 `catch (const stream_error&)` 接不住 eofbit 掩码命中。
     并发那一格（两线程各 400 次 `>> ws`，结果一致、ASan 零报告）**只说明没崩、没算错**，
     竞态判定见第 398 条。

398. **三个 mutator 同时冲一条 `iostream` 不产生竞态，TSan 干净。** 新增常驻用例
     `test_concur_iostream_crossed_1`：赋值（整体替换 `m_streambuf`）× 方向切换（重定位转换器、
     清空读缓冲那个 deque）× tie 刷新（由另一条流的哨兵发起、走同一个转换器），四个线程同时压
     一条 `iostream`。此前 `test_concur_assign_tie_target_1` 只有「赋值 × tie 刷新」且是
     `ostream`，`test_concur_switch_1` 只有方向抖动，**只在三者交错时才打开的窗口两边都看不到**，
     而三样能同时落在一条流上只有双向流做得到。设备带 4096 个同字符加一个定界符，保证提取后读
     缓冲非空——`switch_to_put()` 只有在那个状态下才做 tell → seek → 清 deque 这段真正的活。
     **实测**：clang 21 `-fsanitize=thread`（gcc 的 libtsan 本机缺），整套 `concur` 19 个用例
     零报告；把一个无同步计数器塞进该用例的四个线程体做阳性对照，TSan 报出 3 条竞态且落点都在
     该用例内（`test_concur_streams.cpp:595` / `:600`），去掉后回到零——所以这个「零」是有分辨力的零。

399. **`try_flush()` 放弃那条分支在 TSan 下也干净，第 375 条的结论补上了竞态维度。**
     照第 375 条的原样搭（target 用**真文件设备**，`mem_device` 在这里不能用），一个线程全程
     park 在 `sync(target)` 上使三次 `try_lock` 必然全失败，三个写线程各 2000 次 `put` 持续触发
     tie 刷新；再跑一遍中途放行 holder 的版本，让「放弃」与「拿到锁」两条分支在同一进程里都走到。
     两格 TSan 零报告，两条流全程 `good()`。同一进程末尾的无同步计数器被报出**恰一条**竞态，
     即阳性对照成立。这条只做探针不进常驻套件：常驻的 `test_concur_tie_nonblocking_1` 已经覆盖
     「不阻塞」这件事，再加一个只为压同一条分支的用例是重复。

400. **`abs_cvt` 在把方向抹掉之前先让派生层释放资源，这条顺序由基类保证，五个转换器都不必
     各自记住。** `bos_impl()` 可以按 `m_io_status` 分配只属于某一方向的资源（`zlib_cvt` 就是
     按方向二选一调 `deflateInit` / `inflateInit`），而释放它同样要知道方向；`bos()` 与
     `main_cont_beg()` 失败时都会把 `m_io_status` 复位成 `neutral`，复位之后那份信息就没了。
     两处的 `catch` 因此都在复位**之前**调 `release_derived_state()`。
     - **复用已有的 `detach_impl()` 钩子，不另设一个。** 五个转换器
       （`code_cvt` / `zlib_cvt` / `chacha20_cvt` / `hash_cvt` / `vigenere_cvt`）**都已经有**这个
       钩子，它已被 `static_assert` 强制 `noexcept`、已被文档规定为「派生层清理」、且本来就要求
       幂等——正常生命周期里 `detach()` 与析构会各调一次。新设钩子等于要求每个未来的转换器都
       记得实现它，那正是要避免的重复。返回的 `exception_ptr` 在这两处被丢弃：调用点正在传播一个
       更靠近病因的异常。
     - **另一种写法（在基类另存一份 `m_bos_status` 供 `close_stream()` 读）被否掉**：那只是把
       约定换个地方，每个转换器仍要记得读它而不是 `m_io_status`，负担没减。
     - **判断层级选对了的标准是 `zlib_cvt` 一行不用改**——事实如此。在 `main_cont_beg` 失败点上
       `m_strm` 非空、`m_io_status` 仍是 `output`/`input`、`m_is_bos_done` 为 `false`，
       `close_stream()` 正好走「不跑 `Z_FINISH` 循环、直接 End + 置空」那一支，它自己的
       `state_guard` 还顺手完成复位。
     - `bos()` 那一处对现有转换器是冗余的（`bos_impl` 里的 `inflate_guard`/`deflate_guard` 已经
       盖住了它自己的失败），保留是为了让规则一致、让未来的 `bos_impl` 不必自己写守卫。
     - 与失败语义无冲突：这一步只加了一次释放，`neutral` + 污染标志 + 重抛三件事一件没动。
       派生层状态被提前重置（如 `chacha20_cvt` 的 `m_leftover`、`vigenere_cvt` 的 `m_pos`）不可观测
       ——此后 `m_is_tainted` 为真，一切操作都被 `assert_not_tainted()` 挡住，只剩析构、
       `detach()`、`attach()` 三条路，而这三条本来就会重置。

401. **上一条有常驻用例咬得住：`test_zlib_cvt_bos_4`。** 四格，用 `injectable_device<char>`
     直接驱动转换器的 `bos()` + `main_cont_beg()`（不经流，因为契约就在这一对 API 上）：
     输出侧注入 `dput` 或 `dtell` 失败、输入侧注入 `dtell` 失败，三格都是 `bos()` 成功、
     `main_cont_beg()` 抛出——正是资源还活着而方向即将被抹掉的那个窗口；第四格是阴性对照，
     输入侧注入 `dget` 失败会让 `bos()` 自己抛，那是 `bos_impl` 的守卫本来就盖住的路径，
     若哪天它不再抛，前三格就已失去窗口。**普通跑法看不见泄漏，判卷的是 `MODE=sanitizer`**
     （与 `test_zlib_cvt_bos_3` 同一写法）；普通跑法验的是「这三格确实还够得着那个窗口」。
     实测咬合：把 `release_derived_state()` 的两个调用点删掉后，ASan/LSan 报
     742 784 字节 / 3 处泄漏，落点全在本用例内（`test_zlib_cvt_char.cpp:845` / `:863`），
     退出码 1；加回去后 `cvt` 套件 sanitizer 零报告。

402. **其余四个转换器的启动失败路径也逐个原语跑过了，都不泄漏。** `code_cvt<char, char32_t>` /
     `chacha20_cvt` / `hash_cvt`（MD5）/ `vigenere_cvt` 各建在 `injectable_device<char>` 之上，
     六个设备原语逐一注入失败，驱动 `bos()` 而后 `main_cont_beg()`，ASan+LSan 只报出末尾故意漏的
     4096 字节。**四个转换器各有一格真的走到了第 400 条那个窗口**（`bos()` 成功、
     `main_cont_beg()` 抛出），因此这不是「没跑到所以没报」。
     - **走到窗口的那一格四个都是 `dtell`**：`main_cont_beg()` 背后的重定位查询是这几个转换器
       在启动第二步唯一会碰设备的地方。`code_cvt`/`hash_cvt`/`vigenere_cvt` 在 `bos()` 阶段
       连设备都不读写（`dget=dput=0`）。
     - **`chacha20_cvt` 必须喂真实密文才够得着**：拿一段普通字节当输入，`bos()` 在解析头部时
       就抛了，六格全落在 "bos threw"，看着干净其实什么也没测。改成先用同一把密钥
       (`"liwei"`) 加密出一段再注入，才有一格走到窗口。这条坑记下来，免得重做时被假阴性骗过。
     - 与第 400 条的判断一致：这四个的 `detach_impl()` 只重置成员或调 Botan 的 `clear()`，
       没有 zlib 那种「必须显式调 End 才释放」的 C 资源，所以即便顺序错了也漏不出东西
       ——现在这是实测而不再是读码推论。只做探针不进常驻套件：没有可断言的可观测量，
       而基类那条契约已由 `test_zlib_cvt_bos_4` 咬住。

403. **`ctype<char>` 与 libstdc++ 的差分对拍：43 264 格逐格相同，零差异。** 13 个 locale
     （CI 那 12 个加纯 C）× 256 个字节 × 13 个格：九个分类原语位（`upper`/`lower`/`alpha`/
     `digit`/`xdigit`/`space`/`print`/`cntrl`/`punct`，`alnum`/`graph` 是派生的不单测）
     加 `toupper`/`tolower`/`widen`/`narrow`——后四项是 `ctype<char>` 从同一个 conf 快照下来的，
     顺手一起比。
     - **带鉴别力对照**，因为「处处零差异」也可能是两边都没真的用上 locale：另取三格
       （`tr_TR` 的 `'i'`、`cs_CZ.ISO-8859-2` 的 `0xA0`、`ru_RU.KOI8-R` 的 `0xC1`）确认各自与
       C locale 下的取值不同——IOv2 三格都变，libstdc++ 两格变（`0xA0` 那格只是我那个对照
       只查了 `alpha` 与 `toupper`，而 ISO-8859-2 的 `0xA0` 是不换大小写的 NBSP；全量扫描里
       `cs_CZ` 同样零差异）。
     - **只做了 `char` 一侧。** 宽字符侧的 `ctype_conf<CharT>` 只对 `[0, s_len)` 预建快照表、
       区间外每次直接转发，输入空间不可穷举，逐格对拍要另设抽样判据，未做。
     - 写探针时踩到的坑记一下：`std::use_facet<F>(std::locale(name))` 取到的引用**会悬垂**
       ——临时 `std::locale` 在整个表达式结束时析构，facet 引用计数随之归零。必须先把
       `std::locale` 存进具名变量。C locale 的 facet 是静态存储所以不崩，正好掩盖问题，
       换成别的 locale 才段错误。

404. **`put_time` 与 libstdc++ 的差分对拍：2912 格里 2829 格逐字节相同，其余 83 格来自
     两个成因，都已定性。** 13 个 locale × 14 个时刻 × 16 个说明符。
     最初一轮是 182 格差异、三个成因；其中两个已修（第 405、406、407 条），剩下的 83 格是
     **56 格 `%Z` 退化**（`en_US`/`de_DE`/`en_HK`）与
     **27 格 `cs_CZ` 的补零差**（`%-d` 归一化成 `%d` 的代价，见第 406 条，刻意如此）。
     时刻取每月各一个加两个下午的，保证十二个月名、七个星期名与 12 小时制两半都被产出到。
     - **逐字节相同的部分**：五个带名字的说明符 `%a`/`%A`/`%b`/`%B`/`%h` 在**全部 13 个
       locale** 上无一格不同（这是本条最初要查的东西）；`%p` 同；固定形状的
       `%D`/`%F`/`%T`/`%R`/`%e`/`%j` 同；`%c` 在 `zh_CN.UTF-8`/`ps_AF`/`tr_TR`/`zh_CN.GBK`
       上同；`%x`/`%X` 除各一个 locale 外全同。
     - **差异基本是结构性的，不随数据变**：出问题的「locale × 说明符」几乎都在 14 个时刻上
       全部不同。唯一的例外是 `cs_CZ` 的 `%c`，14 个时刻里差 13 个——补零差只在日 < 10 时
       才显形，这一格反过来是归一化确实只动了补位、没动别的的旁证。
     - **带鉴别力对照**：`%A` 在 C 与 `de_DE` 下两边都变（`Thursday`/`Donnerstag`），
       否则「大面积相同」也可能只是两边都没用上 locale。
     - **只做了 put 侧的 `char`**。get 侧另用往返探针查（见第 406 条，那一侧扫的是本机全部
       888 个 locale），未做与 libstdc++ 的全量差分；宽字符侧未做。
     - 写探针时的一个自查坑：`%x` 只承载日期、`%X`/`%r` 只承载时间，往返比对若一律比六个
       字段，会把 `%X` 判成「日期读错」——那是探针的错不是库的错。

405. **`%r` 在 `T_FMT_AMPM` 为空的 locale 上与 libstdc++ 逐字节相同。** `timeio_conf` 的
     构造函数里四个复合格式串现在都带空值兜底，`m_am_pm_format` 取到空串时回退到
     `"%I:%M:%S %p"`——与同一构造函数里另外三个（`m_era_date_format` / `m_era_time_format` /
     `m_era_date_time_format`）一致，与 glibc `strftime` 的回退一致，也与 C locale 那条分支
     早就硬编码的值一致。宽字符与 `char8_t` 两个特化是从 `char` 那份转换来的，一处改动全覆盖。
     **实测**：13 个 locale 的 `%r` 与 libstdc++ 全部逐字节相同——包括 `de_DE`/`de_CH`/
     `fr_CA`/`cs_CZ`/`ru_RU` 这五个 `T_FMT_AMPM` 为空的；`de_CH`/`fr_CA`/`ru_RU` 三个 locale
     在第 404 条的差分里整体归零。
     **`%r` 在这些 locale 上仍读不回，这是刻意的，且比 std 好。** 这些 locale 的 `AM_STR`/
     `PM_STR` 也是空的，`%p` 因此既写不出也读不出内容，`get_time(&tm, "%r")` 置 `strfailbit`
     且不改目标对象。libstdc++ 在同一格上**报成功却读出 `0:0:0`**（实测 de_DE 与 ru_RU 皆
     如此）——静默丢数据。响亮失败是更好的一侧，故不改。这一格也不在 `timeio.h` 那条往返
     不变量的覆盖范围内：该不变量只管「说明符与值不匹配时退化为字面量」这条规则，
     文档里已有 `@warning` 说明它不等于「两侧取值域处处相同」。

406. **locale 数据里的 glibc 扩展在进库时被归一化，不再字面回显、也不再静默读回错值。**
     `nl_langinfo()` 取回的格式串是 **locale 数据**，不是调用方写的；glibc 在里面用了本库
     `timeio` 不实现的扩展语法（`%-d` 不补位、`%l`、`%k`、`%P`）。而 `timeio` 的 put 侧对
     不认识的说明符是**连 `%` 一起原样写出**（第 329 条钉住的规则，对用户自己写的 `%Q`
     是对的、与 `strftime` 一致），于是「只写了 `%c`」的调用方拿到含 `%` 的输出，get 再把
     它当字面量吃掉——`cs_CZ` 上 `put_time(&tm,"%x")` 写出 `%-d.%-m.2009`，同一格式串的
     `get_time` **不置任何错误位**地读回 `2009-12-31`（原值 `2009-02-13`）。
     - **修在 locale 数据入库的那一处**：`timeio_conf<char>` 的构造函数里新增私有静态
       `normalize_time_format()`，对七个复合格式串各跑一遍。这是 locale 数据进入本库的唯一
       入口，在这里校验一次，put 与 get 自动对称——若只改单侧会直接破掉往返不变量。put/get
       两个 switch 一行未动。宽字符与 `char8_t` 两个特化是从 `char` 那份转换来的，一处覆盖全部。
     - **归一化规则**：丢弃补位标志（`-` `_` `0` `^` `#`）与字段宽度；`%l`→`%I`、`%k`→`%H`、
       `%P`→`%p`；`E`/`O` 修饰符保留；末尾落单的 `%` 原样保留（不打掉第 329 条）。**代价只有
       补位与大小写**，字段顺序、分隔符、语言全部保留——这是选它而不选「整串回退到 C」的理由：
       890 个 locale 的扫描里 132 个用了 `-` 标志、79 个用了 `%l`/`%P`/`%k`，整串回退会把这
       200 多个 locale 的日期顺序一律变成美式。只有归一化后仍不在支持集内时才整串回退。
     - **支持集是 37 个字符 `%ABCDFGHIMRSTUVWXYZabcdeghjmnprtuwxyz`**，从 put 与 get 两个
       switch 的 `case` 分支里抽出来比对过，**两侧完全相同**。
     - **实测（本机全部 888 个可用 locale × `%c`/`%x`/`%X`/`%r` × 4 个时刻 = 14 208 格往返）**：
       失败的「locale × 格式」对 905 → 519，**新增 0 个、修好 386 个**。单看归一化这一项
       （拿掉它重编一份做对照）：**去掉 969 条失败、新增 0 条**。
     - **残留的 519 对不是本库的缺陷**：绝大多数是 locale 自己的 `T_FMT_AMPM` 与
       `AM_STR`/`PM_STR` 皆空，`%r` 写出 `11:31:30 ` 而读不回——glibc `strftime` 逐字节相同
       （见第 405 条）；其次是 locale 的 `%c`/`%X` 本就不含秒或不含年（`bem_ZM` 的 `D_T_FMT`
       用 `%R`），往返对这些字段本就无从谈起；`br_FR` 一类是 `AM_STR` 与 `PM_STR` 相同，
       被构造函数既有的校验直接拒掉（抛 "AM and PM designators are identical"）。
     - **签名是单参数返回 `std::optional<std::string>`，调用点用 `.value_or(<C locale 取值>)`
       ——这是被 CI 强制的形状，不能"简化"回两个参数。** 写成
       `normalize_time_format(fmt, fallback)` 会让两个相邻的 `const std::string&` 撞上
       `bugprone-easily-swappable-parameters`，而 `ci.yml` 的 clang-tidy job 带
       `--warnings-as-errors='bugprone-*'`，直接红。这条检查在这里**是对的**：两个实参调换
       位置照样编译，然后静默地把兜底值当格式串去归一化。因此是去掉隐患而不是 NOLINT 屏蔽
       （库里两种做法都有先例：16 处 NOLINT，`monetary_details.h` / `messages_details.h`
       则为这条检查重构过签名）。
     - **常驻用例**：`test_timeio_char_put_20`。它不复述支持集，而是**从 facet 自己的行为反推**
       ——不实现的说明符会被原样回显（第 329 条），所以「输出等于格式串」即「不支持」。
       A 部分逐个断言 37 个字符都不回显（`%Z`/`%z` 用 `zoned_time` 而非 `std::tm`，否则会撞上
       退化规则被误判成不支持），并用 `QLfikloqsv` 十个字符作反向对照，保证「回显即不支持」
       这条判据本身不是空的；B 部分再断言各 locale 的十一个复合格式串里不含支持集之外的序列，
       并报出实际测到几个 locale（只剩 C 的机器上这半是弱的，报数让它可见）。
       put/get 的 switch 与归一化表若哪天脱节，A 部分先坏。

407. **C locale 的 `%c` 是标准规定的 `"%a %b %e %H:%M:%S %Y"`，`m_era_date_time_format` 同此。**
     C 标准 7.27.3.5 对 `"C"` locale 下的 `%c` 规定的就是这一串，POSIX 同，glibc 的 `D_T_FMT`
     亦同。**不能**把它拼成 `m_date_format + ' ' + m_time_format`，那是 `%x %X`
     （`02/13/09 23:31:30`）：`%x`（`%m/%d/%y`）与 `%X`（`%H:%M:%S`）各自独立时都对，
     拼起来当 `%c` 这一步才是错的。
     - **往返不变量不依赖这一条**（put 与 get 用的是同一个字符串），它管的是与外界的互操作：
       `%c` 拼错时本库写出的 C locale 时间戳，`strptime("%c")` 或任何按标准实现的读取方都
       解析不了。
     - **这条格式串被 9 个测试文件的期望值钉住**：`timeio` 的四份
       （`char`/`wchar_t`/`char8_t`/`char32_t`，各 4 处）、`istream`/`ostream` 的时间读写各两份、
       `io_base` 的操作符一份。期望值全部是**跑出来核对的**，不是手算的。

---

## `%z` / `%Z` 的三档时区模型（第八轮）

408. **`tz_level` 说的是「哪个说明符会被解析」，不是「存多少字段」。**
     `facet/timeio.h:718` 的 `enum class tz_level : unsigned char { none, offset, zone }` 是
     `time_parse_context<CharT, HaveDate, HaveTime, TzLevel>` 的第四个模板实参。每一档回答两个
     互相独立的问题——`%z` 解不解析、`%Z` 解不解析——**不解析的那个按字面量匹配**，与 put 对
     同一类值的退化正好对上：

     | 档 | `%z` | `%Z` | `time_value_fields` | 值类型 |
     |---|---|---|---|---|
     | `none` | 字面量 | 字面量 | `has_offset` 假、`has_zone` 假 | `year_month_day`、`hh_mm_ss` |
     | `offset` | 解析 | 字面量 | `has_offset` 真、`has_zone` 假 | `local_time` |
     | `zone` | 解析 | 解析 | 两者皆真 | `std::tm`、`sys_time`、`zoned_time` |

     第四种组合（`%z` 字面量、`%Z` 解析）不存在也不需要：**能说出区名的值必然说得出偏移**。
     全部 put 重载逐一核对过——`zoned_time`、`sys_time`、`std::tm` 是 (真, 真)，`local_time`
     是 (真, 假)，`year_month_day` / `hh_mm_ss` 是 (假, 假)，没有 (假, 真)。所以三档仍是嵌套的，
     `zone` 的存储包含 `offset`，有序枚举成立。

     **偏移与区名同处一档，不像 `HaveDate` / `HaveTime` 那样各占独立开关**，因为它们是同一件
     事的两种说法，转换的工作恰恰是把两者对上：`convert_to(sys_time&)` 的取值阶梯一条链上同时
     读偏移与区名，`convert_to(zoned_time&)` 还要拿解析到的偏移去校验解析到的区。日期与时间
     没有这种纠缠，那两个才是独立的。

     **档位由 `time_value_fields` 推出，两侧同源。** `io/traits/tm.h:46-49`（第十七轮订正，原记 `:60`）：
     ```cpp
     static constexpr tz_level tm_parse_tz_level =
         time_value_fields<std::tm>::has_zone   ? tz_level::zone
       : time_value_fields<std::tm>::has_offset ? tz_level::offset
                                                : tz_level::none;
     ```
     `std::tm` 的 `tm_gmtoff` / `tm_zone` 用 `requires` 表达式探测，不用平台宏。这两个成员
     **总是同进同出**——glibc 与 musl 把它们放在同一个特性宏下（本机
     `/usr/include/bits/types/struct_tm.h:19-25` 的 `__USE_MISC`），BSD / macOS 无条件都有，
     POSIX Issue 8（2024）是把两个一起加进标准的，MSVC 的 UCRT 两个都没有。所以上面那个三元
     实际只在 `zone` 与 `none` 之间取值，`offset` 那一支留给将来可能只有其中一个的平台；真取到
     了也是对的（那种平台上 put 退化 `%Z`，`offset` 档正好按字面量收，仍然对称）。
     - 钉死某一档会把往返弄断：成员缺失的平台上 put 写出字面 `%z`，而 `offset` / `zone` 档的
       `do_get` 要求读到真的 `+0800`；何况解析出的值没有成员可写回。
     - 相应地 `io_traits` 对 `TzLevel` 偏特化（`io/traits/tm.h`），只特化其中一档会让别的平台
       **找不到** `io_traits`。三个档在那里行为一致，档位只影响 `do_get` 内部两个说明符是真解析
       还是按字面量匹配。

     **要认的跨平台代价**：同一 locale、同一程序，Linux 出 `+0800 CST` 而 MSVC 出字面
     `%z %Z`。往返不变量在**每个平台内部**仍成立（put 与 get 查的是同一张表），不成立的是跨平台
     的文本一致性。这是取舍。

409. **`%Z` 那棵前缀树收三类键，值是 `zone_ref`。** `facet/timeio_details.h:334` 的
     `prefix_tree<char, zone_ref> s_timezone_tree`，程序启动时经 `std::chrono::get_tzdb()`
     构建，收：
     - `tzdb.zones` 里的区名（本机 341 条）；
     - `tzdb.links` 里的别名（257 条）——`US/Pacific`、`Asia/Calcutta`、`Japan` 这些
       `locate_zone()` 一样认的名字；
     - 每个时区走过的全部缩写（179 条）。

     值 `zone_ref{text, is_name, is_abbrev}`（`timeio_details.h:101`）里的两个标志位是**互相
     独立的谓词，不是三值枚举**：`is_name` 是「在 `zones ∪ links` 里」，`is_abbrev` 是「是某个
     时区某时刻的 `sys_info::abbrev`」。两者同真的条目有 8 条——`CET`、`EET`、`EST`、`GMT`、
     `HST`、`MST`、`UTC`、`WET`，它们既是 link 名又是缩写。两者同假的那一格留给 `UNKNOWN` 记号
     （见第 414 条）。`text` 是键的原文，**不做规范化**：解析 `EST` 得到 `EST` 而不是
     `America/Panama`，解析 `US/Pacific` 得到 `US/Pacific` 而不是 `America/Los_Angeles`。原文是
     `tm_zone` 往返所必需的，而规范化由 `locate_zone` 自己完成，没有理由在树里再存一份。

     **缩写的采集范围是每个时区从纪元下限走到「今天 + 10 年」的全部转换**：
     ```cpp
     const sys_seconds projected = time_point_cast<seconds>(system_clock::now()) + years{10};
     const sys_seconds floor{sys_days{2038y / January / 1}};
     const sys_seconds horizon = (projected > floor) ? projected : floor;
     ```
     - **没有下界**，因为 put 侧的 `%Z` 原样写出 `tm_zone`，而 `localtime()` 能表示任意时刻：
       一个 1943 年的 `std::tm` 打出 `EWT`，树里没有它就读不回来。「树必须收全 put 能写出的
       一切」是这里的判据。
     - **上界是必须的**，因为有夏令时的时区在 libstdc++ 的 tzdb 里**永远外推**：没有一个时区
       报 `end == sys_seconds::max()`，「不再有转换」的哨兵是 32767-12-31，走到底不会停。
       上界取什么值几乎不影响结果（2026–2100 各值给出的缩写集合相同），取「当前时间 + 10 年」
       是让它随时钟移动；2038 的地板是给系统时钟异常时兜底。
     - **靠收敛判停行不通**，已否：`America/Anchorage` 在 1983 年换成 AKST/AKDT 之前，
       AHST/AHDT 一对要循环 26 次转换，任何 K ≤ 26 的「连续 K 次无新缩写就停」都停早了。
     - **只在某一时刻采样一次不够**：341 个时区里有 89 个的 7 月缩写会读不回来。

     **分类先在 `std::map` 里做完，再一趟灌进树**，所以同一个键不可能 `add` 两次——
     `prefix_tree::add` 的重复冲突在这里**结构上不可能发生**。这一点不能靠巧合：`add` 遇到同键
     异值会抛，而那个异常会被建树 lambda 外层的 `catch (...)` 吞掉、静默截断整棵树。
     - 因此建树里**不再调用 `locate_zone`**：`is_name` 就是「在 `zones ∪ links` 里」，纯集合
       运算，连 `try/catch` 一起省掉。

     **条目互为前缀是常态，但不产生歧义**：`Etc/GMT` < `Etc/GMT+1` < `Etc/GMT+10`、
     `WIT` < `WITA`、`America/Bahia` < `America/Bahia_Banderas`，历史缩写里还有 `AT`、`CT`
     这类两字母条目。`prefix_tree::max_match` 取最长匹配、失配完整回退，每一组都按整体解析。
     唯一能观察到的差别：不合法的输入若以某个合法缩写开头（`ATLANTIC` 之于 `AT`），`%Z` 读掉
     那个缩写就停，剩下的留在流里——与 `%d` 读 `12abc` 只取 `12` 同理，不是缺陷。

     **树以 `char` 为键装字节，全程序只有一棵**，挂在不带字符类型的中间基类 `base_ft<timeio>`
     上（`base_ft<TFacet>` 本就是各字符特化的共同基类，messages / monetary / ctype 同此）。
     tzdb 的名字全在 ASCII 内，宽字符流查找靠 `prefix_tree::max_match` 的往返检查命中同一棵树。
     - **实测规模与开销**（`-O2`）：770 个键、4095 个节点；仅 `#include <facet/timeio.h>` 且
       什么都不做的程序，相对空进程基线（0.00 s / 3728 KB）为 **0.02 s / 6800 KB**。
     - **代价是它不按需构造**：类模板的静态成员只在 odr-use 时实例化，而非模板类的
       `inline static` 无条件初始化，所以**任何包含 timeio.h 的程序都在启动时付这笔钱，哪怕
       从不解析 `%Z`**。改成函数局部静态可两头兼得，评估后按现状保留：这个量级不值得把
       `s_timezone_tree` 从变量改成访问器。
     - 覆盖测试：`test_timeio_char_get_22` 除逐条断言 `PDT` / `EDT` / `CEST` / `ACDT` /
       `NZST` / `EWT` 之外，还有一条数据库级不变量——遍历 `get_tzdb().zones` × {2024-01-15,
       2024-07-15} 的全部缩写逐一解析（`VERIFY(checked > 500)`）。另外三个字符类型各有一条
       `get_13`，测的是「宽字符输入经往返检查命中同一棵字节树」。

410. **`%Z` 解不解析是编译期由档位决定的，与树里恰好有什么无关。**
     `do_get` 的 `case 'Z'`（`timeio.h:3690`）开头就是：
     ```cpp
     if constexpr (TzLevel != tz_level::zone) goto bad_parse_format;
     ```
     `zone` 档之下这个说明符**按字面量匹配**，即消费 `%` + 修饰符 + `Z` 三个字符，与 put 侧
     `bad_format` 写出同样三个字符对称。`zone` 档查树，匹配不到即 `succ = false`。

     走 put 侧那条退化路的是**没有区域身份的值**：`local_time`（`put(out, t, offset, fmt)` 内部
     构造 `zone_info zi{offset, {}}`，`abbrev` 恒空）、`year_month_day` / `hh_mm_ss`
     （`zi == nullptr`），以及两个扩展成员都没有的平台上的 `std::tm`。实测：
     ```
     put(local_time, "%F %T %z %Z") = [2024-09-04 13:33:18 +0800 %Z]
       get at offset -> OK          get at zone -> throw
     ```
     - **没有运行期回退。** 档位不会「先查树、匹配不到再当字面量」——那会让一个档的行为取决于
       数据库内容。`offset` 档对 `UTC`、`PDT` 这类真实记号一律拒绝，因为那一档的格式串要的就是
       `%Z` 两个字符，而 put 对落在该档的值从不写出区域记号。
     - 覆盖测试：`test_timeio_char_get_23` 把两档钉开——`off_ok("%Z", "%Z")` 真、
       `zone_ok("%Z", "%Z")` 假、`zone_ok("UTC", "%Z")` 真、`off_ok("UTC", "%Z")` 假，
       `XYZ` 两档都拒（一边无树条目、一边无字面匹配）。三个宽字符文件各有一组同样的断言。

411. **`%Z` 与 `std::get_time` 的差别是「本库认 770 个记号（179 条缩写、341 个区名、
     257 个 link 名，8 条重合），libstdc++ 只认一张硬编码 14 条的表」，不是「本库严、
     libstdc++ 全放行」。** libstdc++ 的 `%Z` 走
     `_M_extract_name` 比对 `__timepunct_cache::_S_timezones`
     （`bits/locale_facets_nonio.tcc:983-1003`，表声明在 `locale_facets_nonio.h:64`）。
     实测放行 `GMT MST PST CST EST AST HST AKST CET EET JST NST IST`，**拒绝**
     `UTC CEST PDT EDT CDT MDT ADT` 与 `America/Los_Angeles`——DST 缩写一个都不认。
     - 于是 `get_time(&t, "%H:%M %Z")` 读 `01:02 UTC` **本库成功、`std` 失败**；
       两边一致的只有 `01:02 PST` 都成功、`01:02 XYZ` 都失败。
     - `US/Pacific`、`Asia/Calcutta`、`Japan` 这类 link 名本库也认（`locate_zone()` 认它们），
       `std` 同样不认。

---

## 格式串扩展、`%Z` 的供出与 `std::tm` 的流格式（第九轮）

412. **locale 的时间格式串按原样使用：每个复合说明符只有一个变体，facet 从不往里追加
     `%Z`。** `%c` 取 `m_[era_]date_time_format`、`%X` 取 `m_[era_]time_format`、`%r` 取
     `m_am_pm_format`，`do_put` / `do_get` 不按值有没有时区在多个变体间选。时区在格式串里的
     位置与个数完全由 locale 决定。
     - **判据是 locale 数据本身就可能带时区**：本机 888 个 locale 里 **339 个（38%）**的时间
       格式含 `%Z` / `%z`，共 500 处（`d_fmt` 一处没有）；位置分布为末尾 399 处、开头 99 处
       （`%Z %H:%M:%S`）、中间 2 处（`%T (%Z)`）。`T_FMT_AMPM` 含 `%Z` 的另有 150 个。
       任何「在末尾拼一个 ` %Z`」的做法都会让这 339 个 locale 打出两遍时区。
     - 需要时区而 locale 的 `%c` 里没有的调用方，用第 413 条的 `expand_format` 自己补，
       不由 facet 猜。

413. **`expand_format<TVal>` 是公开接口：展开 locale 复合说明符，并摘掉该值类型供不出的
     说明符。** 三块支撑：
     - `time_value_fields<TVal>`：名字空间作用域的表，四个标志位
       `has_date` / `has_time` / `has_offset` / `has_zone` 对应 `put` 交给 `do_put` 的那几个
       指针。**主模板不定义**，不在支持范围内的类型是硬错误，而不是悄悄按「什么都供不出」
       处理。星期不单独占位——供得出 `ymd` 就一并供得出 `wd`。
     - `expand_and_filter`：`do_put` 那个 switch 的编译期镜像。那里问指针空不空，这里问模板
       标志；那里 `goto bad_format`，这里摘掉。两边必须一一对应，
       `test_timeio_char_expand_1` 对 6 种值类型 × 53 个说明符 × 3 个修饰符（除去 `%E` / `%O`
       两个截断组合，共 942 对）逐对与 `put` 对拍，防止二者悄悄漂移。
     - `contains_specifier(fmt, format, modifier = 0)`：静态函数。手写 `find("%Z")` 会把
       `%%Z` 判错——那是转义的 `%` 后跟字面 `Z`，不是说明符。
     摘的规则：连一侧分隔符一起摘（否则留下孤立的逗号或斜杠），前面有东西就摘前面的、只在串首
     才摘后面的；分隔符是空白加至多一个标点，标点经 ctype facet 判断（装了 ctype 就用它）。
     括号组整体处理：递归过滤其内容，非空的组过滤后变空则连括号一起摘。`%EY` 不展开——纪元
     取决于年份，是值相关的。供不出的复合说明符整个摘掉而不展开，否则只剩一串孤立标点。
     - **locale 数据当作可信输入**，与 `put` / `get` 的假定一致：自引用的复合格式串
       （`D_T_FMT` 里含 `%c`）会让展开无限递归，行为未定义。

414. **带 `tm_zone` 成员的平台上，`%Z` 对任何 `std::tm` 都写得出内容**：有名字写名字，
     `tm_zone` 为空指针**或空串**时写 `base_ft<timeio>::s_unknown_zone`（`UNKNOWN`）。
     该记号也注册在 `s_timezone_tree` 中，映射到 `zone_ref{"", false, true}`——两个标志位都假、
     文本为空串——所以写得出就读得回。
     - **解析到它时 `m_zone_abbrev` 指向空串，而不是留 `nullptr`。** 这两者含义不同：`nullptr`
       是「`%Z` 压根没解析到」，空串是「解析到了，而且它明说没有时区」。于是有两个不同的判据，
       不能互相顶替——问「有没有时区」用 `p && *p`（与 put 侧 `t.tm_zone && *t.tm_zone` 对称），
       问「`%Z` 有没有解析到东西」只看 `p`。`convert_to(std::tm&)` 回写 `tm_zone` 用后者，
       **空串正是要写进去的值**。
     - **写空串而不是跳过，是往返闭合所必需的。** 跳过的话，调用方 `tm` 里上一次留下的区名会
       残留，下一次 put 就写出那个陈旧的名字，与文本不符。四种取值的往返都闭合：`nullptr` 与
       `""` 都经 `UNKNOWN` 回到 `""`，真实缩写与区名原样回来。
     - 它与 tzdb 不冲突：对全部区名、缩写与 link（本机 769 条）逐一比对过，无相等项，也无任一
       方向的前缀关系。**这一条不只是观察结论，建树时会实际检查**：一旦哪天的 tzdata 破坏它，
       建树在往树里灌任何 tzdb 条目**之前**抛异常，被外层 `catch` 接住，树里只剩这个记号，于是
       每一次真实的 `%Z` 都报可捕获的 `stream_error`——响亮地全错，而不是悄悄错一条。抛在
       `try` **内部**是刻意的：抛到外面就是静态初始化器里逃逸的异常，等于 `std::terminate`，
       会让每个包含本头文件的程序在启动时死掉，哪怕它一辈子不碰 `%Z`。
     - 记号本身的注册放在 tzdb 遍历**之前**，数据库不可用时它仍在。
     - **编译期表与运行期 switch 之间没有缝。** `time_value_fields<std::tm>::has_zone` 是
       `requires { t.tm_zone; }`，在这类平台上恒真，而 put 对这类平台的任何 `std::tm` 也确实
       总写得出内容——表说的是事实，`expand_format` 保留下来的说明符 put 一定供得出。这是
       该表可信的前提：若 `%Z` 能在运行期退化，表就会声称供得出而实际供不出。
     - **不回退到进程本地时区**（glibc 的 `strftime` 在 `tm_zone` 空且 `tm_isdst >= 0` 时查
       `tzname[tm_isdst != 0]`）。实测该回退恰好只在手工构造的 `std::tm` 上触发——`localtime()`
       给 `PDT`、`gmtime()` 给 `GMT`，本库 `convert_to` 的产物 `tm_isdst == -1` 也不触发——
       即只在「对时区一无所知」时才生效，而它安上去的是进程的时区：`tm_gmtoff` 为 `+0800` 的值
       在美国西岸会写出 `PST`。它还要读 `tzname[]` 这一全局可变状态，与本 facet 拒绝
       `mktime()` / `tzset()` 的既有立场冲突。

415. **`os << tm` 与 `is >> tm` 共用 `detail::tm_stream_format`：展开后的 `%c`，缺什么补
     什么——`%z` 与 `(%Z)`。**
     - **两个说明符各测各的，因为各还原一个成员。** `std::tm` 的时区是两个成员，一个说明符
       还原一个：`%z` 还原 `tm_gmtoff`，`%Z` 还原 `tm_zone`，谁都替不了谁。
       - `%Z` 供不出偏移：`do_get` 的 `case 'Z'` 只写 `m_zone_name` / `m_zone_abbrev`，从不置
         `m_have_offset`，而 `convert_to(std::tm&)` 仅在该标志为真时写 `tm_gmtoff`。缩写本身
         也定不出偏移（`CST` 同时是美中 −6、中国 +8、古巴 −5）；就算是无歧义的 IANA 全名，
         从名字算偏移要查 DST 规则、还可能在折返/空洞上抛异常，不是一个 `tm` 转换该做的事。
       - `%z` 供不出区名。
       - 实测 `tm_gmtoff = 28800, tm_zone = "CST"`：只走 `%Z` 读回来 `tm_gmtoff` 是 `0`，
         只走 `%z` 读回来 `tm_zone` 是空指针，两个都走才都对。所以 en_US 的 `%c` 自带 `%Z`，
         照样要补 `%z`。
     - **补出来的 `%Z` 带括号，`%z` 不带**，形如 `... +0800 (CST)`——与 RFC 5322 的日期一致，
       也与本机两个 locale 自带的 `%a %Y %b %d %H:%M:%S (%Z)` 一致。已经带 `%Z` 的 locale
       （本机 165 个，其中 163 个裸写在末尾）不再补，于是它们仍是 `... %Z %z`。**不追求跨
       locale 的形状统一**：各 locale 的 `%c` 本就千差万别，这里只要求写得出的读得回来、
       且看着像样。
     - **先展开 `%c` 不是为了过滤**（`std::tm` 什么都供得出，一个说明符都不会被摘），而是为了
       让藏在 `%r` / `%X` 里的 `%z` / `%Z` 对上面那两个判据可见。
     - **格式串与取值无关，两侧必须同一个。** 值级门控做不到：`sread` 手上只有
       `time_parse_context`，没有 `std::tm`。put 门控而 get 不门控，带时区的值写出的文本会残留
       在流里污染下一次提取；两边都门控，无时区的值写出的文本又满足不了要求这些字段的格式串。
     - 平台的 `std::tm` 两个成员都没有时两个都不补：那里 `%z` / `%Z` 都会退化成字面量，档位
       也已降到 `tz_level::none`（第 408 条），补了只会在输出里留下那几个字符。
     - **实测往返**（`os << tm` → `is >> tm` → `os << tm`，四种情形全部逐字节闭合、两个成员
       都还原）：
       ```
       C            [Wed Sep  4 13:33:18 2024 +0800 (CST)]      gmtoff=28800 zone=CST
       C（无时区）   [Wed Sep  4 13:33:18 2024 +0000 (UNKNOWN)]  gmtoff=0     zone=空串
       en_US        [Wed 04 Sep 2024 01:33:18 PM CST +0800]     gmtoff=28800 zone=CST
       ja_JP        [2024年09月04日 13時33分18秒 +0900 (JST)]     gmtoff=32400 zone=JST
       ```
       `(UNKNOWN)` 那行读回来 `tm_zone` 是**空串而不是空指针**，正是这一点让它再写出去仍是
       `(UNKNOWN)`（第 414 条）。
     - **`os << tm` 的默认输出带尾部的偏移与区域记号。** 这是刻意的取舍：代价是与
       `strftime("%c")` 不逐字节相同，换来的是 `tm` 携带的 `tm_gmtoff` 与 `tm_zone` 都不被
       静默丢弃。
     - `tm_stream_format` 每次插入 / 提取都重新展开一遍 `%c`，未做缓存。

---

## 第十轮审查修掉的三处（`io_manip.h` / `istream.h` / `ostream.h` / `iostream.h`）

416. **`setfill` 的可读性判据按 `basefield` 选数字集合：`hex` 十六个，其余十个。**
     - 判据在 `facet/numeric.h::fill_alters_reading`，唯一调用点是同文件的 `pad`；`pad` 现在
       多收一个 `basefield` 形参，由两个调用点（`insert_float` / `insert_int` 的补齐分支）
       传 `io.flags() & ios_defs::basefield`。
     - **`hex` 档同时查小写段 `[s_odigits, +16)` 与大写段 `[s_oudigits, +16)`**，与 `uppercase`
       无关——提取侧接受混合大小写（实测 `"FFFFFFab"` 读得回），只查其中一段挡不住另一半。
     - **`oct` 仍用十位表，不是八位。** 判据问的是"人读起来像不像数字"，不是"该进制收不收
       这个字符"：`setfill('8') << oct << setw(8) << 0777` 写出的 `"88888777"`，人会读成
       八千八百多万。缩到八位反而会放松一道原本成立的检查。同理 `'0'` 落在符号前
       （`"00000-42"`）虽然读不成任何数，也照样拒绝。
     - 修前的行为（实测）：`hex` + `setfill('f')` + `setw(8)` 写 `0xab` 得到 `"ffffffab"`，
       本库自己读回是 4294967211，不置任何状态位；而同一档下 `setfill('1')` 被拒——判据自相
       矛盾。`setbase(16)` 这条入口同样命中。
     - 修后实测：`'a'`–`'f'` / `'A'`–`'F'` × 三种 `adjustfield` × `uppercase` 开关全部
       `strfailbit`；`'0'` 的 right/internal 仍成功、left 仍失败，加 `showbase` 后只有
       `internal` 成功；`dec`/`oct` 下 `'f'` 仍放行。
     - **回归**：`test_io_base_char_fill_3` 与 `test_io_base_wchar_t_fill_2`
       （`test/io/io_base/test_io_base_fill.cpp`）。原有的 `test_io_base_char_fill_2` 只覆盖
       十进制的 `'0'`/`'9'`，hex 一档此前无用例——这是该洞能活下来的原因。
     - **`hexfloat` 下的字母填充仍放行**（`"ffffffff0x1.8p+0"`）。按判据字面算命中，但它读不成
       任何数，危害远轻于整数一路；判据只看 `basefield`，不看 `floatfield`。未处理，见未覆盖一节。

417. **`std::basic_string` 的提取无论成功还是抛出都消费 `width`。**
     - `io/traits/char_and_str.h::sread` 的 `io.width(0)` 已从循环之后移到读完 `w` 之后、
       任何可抛动作之前，与同文件另外两个消费点（`ostream_insert`、`istream_extract`）
       一致——那两处一直是"先取后清"，并各带一行注释说明原因。
     - **这推翻了第 205 条那句"失败路径也会消费 `width`"**：修前它只对 `extracted == 0`
       那一个抛出点成立（那个 `throw` 在旧的 `width(0)` 之后）。循环体里的 `*iter` / `++iter`
       是流缓冲区迭代器，缓冲区耗尽回设备取数失败就在那里抛，抛在 `width(0)` 之前。
     - 修前实测（`mem_device` 外覆注入失败的设备，载荷 200000 个 `'A'`）：抛出后 `width()`
       仍是 5000；`clear()` 之后一句**完全没有 `setw`** 的 `in >> s2` 被截断到 5000 字符，
       且因 `extracted != 0` 不置任何状态位，调用方看到的是 `good()`。字符数组一路在同一注入
       下正确归 0——两条提取路径当时行为相反。
     - `io.width(0)` 自身不会抛：`io_base.h` 的 `width(std::ptrdiff_t)` 只在实参为负时抛，
       字面量 `0` 永远安全。
     - **回归**：`test_istream_extractors_character_char_10`
       （`test/io/istream/extractors_character/test_istream_extractors_character_char.cpp`），
       用 `support/injectable_device.h` 让缓冲区补给失败，并把 `char[N]` 路径并排放着作对照。

418. **`%z` 的取值域是 `(-24h, +24h)`，超出者钳位；钳位发生在窄化之前。**
     - `facet/timeio.h` 的 `do_put` `%z` 分支：先取 64 位的 `zi->offset.count()`，钳到
       `±86399`（= `23:59:59`），再窄化成 `int`。
     - **边界为什么是 86399**：get 侧 `%z` 判 `val / 100 >= 24 || val % 100 >= 60` 即失败。
       `86399` → `/60` → `1439` → `23*100+59` = `2359`，get 接受；`86400` → `2400`，get 拒绝。
       `86400` 恰是第一个被拒的值，所以 `|offset| < 86400` 是紧的，且保证写出的每个 `%z`
       都能被自己的 get 读回去。
     - **顺序是承重的，不是风格。** `:2510` 的 `put(out, local_time, seconds offset, fmt)`
       重载里 `offset` 是调用方直接给的 64 位 `seconds`；先窄化会让 `seconds{2147483648}`
       回绕成 `INT_MIN`，再钳只能得到 `-86399` → 写出 `"-2359"`，**正偏移印成负的**。
       实测把钳位挪到 `static_cast<int>` 之后：`+2147483648` → `"-2359"`（符号翻转）、
       `+4294967296` → `"+0000"`。
     - 修前的行为：`tm_gmtoff` 完全不被 `timeio::put` 的校验块（`:2664-2671`）检查，
       `val = -val` 在 `INT_MIN` 上是**有符号溢出 UB**（UBSan 实测命中
       `negation of -2147483648`）；越界值不置失败位就静默写出，`+400h` 因 `put_dec<4>`
       只写低四位而变成 `"+0000"`，`INT_MAX` 变成 `"+2314"`——后者还能被 `get_time` 的 `%z`
       当成 23:14 收回去。
     - **秒被向零截掉，这是 `%z` 本身的有损性，不拒绝。** `tm_gmtoff = 1172`
       （Europe/Amsterdam 的 LMT `+00:19:32`）写出 `"+0019"`；get 侧存的是
       `std::chrono::minutes`，分辨率天生是分钟。tzdb 里真有带秒的 LMT 值，拒绝会挡掉合法输入。
       于是**往返闭合只在域内且整分钟时成立**。
     - **`tm_gmtoff` 走钳位而不是校验**，与其余字段（`tm_mon`/`tm_mday`/`tm_hour`/`tm_min`/
       `tm_sec`/年份，任一越界即抛）不同。四处 `zone_info` 构造点中只有 `:2510` 与 `:2693`
       的偏移可控，另两处（`zoned_time` 取自 tzdb、`sys_time` 硬编码 0）天然安全。
     - **回归**：`test_timeio_char_offset_clamp_1`（`test/facet/timeio/test_timeio_char.cpp`），
       含边界、`INT_MIN`/`LONG_MIN`、put→get 逐值闭合、sub-minute，以及 `local_time` 重载的
       符号那一条（实测能抓到顺序写反）。

419. **随上述三处一并订正的文档**（都属"契约写反了"，不是"写得不够细"）：
     - `io_manip.h` 的 `get_time` `%Z` 警告整块此前说"只有偏移写得回去""`%Z` 解析出的名字
       在任何一档都被丢弃——`std::tm` 没有释放接口，所有权无处安放"。**已被 `e4ca8466` /
       `c1bb32c9` 作废**：`convert_to` 现在写 `out.tm_zone`，存的是指向时区前缀树内静态存储期
       文本的指针，不悬垂也不需要所有权。`put_time` 侧那句"回写 `tm_zone` 会留下悬垂指针"同理。
     - 同块还把档位说成"带 `tm_gmtoff` 时处在只带偏移与缩写的那一档"。实际由
       `io/traits/tm.h:46-49`（第十七轮订正，原记 `:61`） 决定：`has_zone`（两个成员都在）→ `tz_level::zone`，glibc 上就是
       zone 档。`timeio.h` 类文档退化表里 `std::tm` 两行的"时区档"也一并从"偏移"改成"区域"，
       并注明该档随平台的两个扩展成员而定。
     - `io_manip.h` 的 `put_time` 说 `tm_zone` 为空时"退化为原样写出 `%Z`"——实际写
       `s_unknown_zone`（`UNKNOWN`），见 `timeio.h:2689-2692`。
     - `io_manip.h` 与 `timeio.h` 都说 `timeio::put` "校验全部字段 / validates every field"，
       而 `tm_gmtoff` 不在其列。已改成"日期与时刻字段"并点明 `tm_gmtoff` 走钳位。
     - `timeio.h` 的 `put_dec` 说 "Never truncates / 永不截断"——只对 `n == 0` 分支成立；
       `n > 0` 分支写恰好 `n` 位、高位丢弃。已改，并写明 `n > 0` 时调用方须自行保证位数。
     - `io_manip.h` 的 `setfill` 说"其余字符（`'*'`、`' '`、千位分隔符、字母等）不受影响"
       ——`hex` 下字母是数字。已改。
     - **命名**：`facet/numeric.h` / `facet/monetary.h` 的 `fill_alters_reading_`、`pad_impl_`、
       `format_float_` 三个尾下划线函数改为无后缀。全库带尾下划线的函数原本只有这三个，
       都在这两个文件里，是孤例。

## 第十一轮（`io_manip.h` / `istream.h` / `ostream.h` / `iostream.h`：边界条件、隐含假设、竞态、形式检查）

本轮的缺陷点全部落在四个目标文件**之外**——它们本体只有构造函数、拷贝/移动、`ws`、
`endl`/`ends`/`flush`，实现都在 mixin 里。收成：改代码 1 条（第 420 条）、撤回误报 1 条
（第 421 条）、改判为文档并订正 1 条（第 422 条）、形式项 3 条（第 423 条）、
并发轴四轴全清（第 424 条）。

420. **`read()` 与 `get()` 的字符计数形参是有符号的 `std::ptrdiff_t`，负值在进入任何写操作
     之前被拒。**（`io/utilities/istream_operators.h`）
     - 三个入口：`read(TChar* s, std::ptrdiff_t n)`、
       `get<DelimPolicy, CStrPolicy>(TOut s, std::ptrdiff_t n, TChar delim)`、以及它的
       两参数转发版。判负紧挨着既有的空指针 / `n == 0` 校验，抛 `stream_error` 走
       `handle_exception` 置 `strfailbit`；随后 `static_cast<std::size_t>` 交给下层。
     - **为什么必须是有符号**：与 `io_base.h` 的 `width(std::ptrdiff_t)` /
       `precision(std::ptrdiff_t)` 同一条判据（`io_base.h:456-462`）——读取长度常由含
       `size_t` 的算式得出（`end - cur`、`cap - used`），越界时回绕成接近 2^64 的巨值；
       有符号形参让传参时的窄化把回绕抵消回来、还原成负数并在此拒掉。
       无符号形参下这个转换不是窄化，编译器默认不报，负值静默变成 `SIZE_MAX`。
     - **修前的行为是内存破坏，不是状态位不对**：`read(buf8, -1)` 一路 `sgetn` 到输入耗尽，
       ASan 实测 `heap-buffer-overflow, WRITE of size 4096`（栈顶 `istream_operators.h` →
       `streambuf.h:380` → `mem_device.h:186` 的 `memmove`）；
       `get<keep_sep, app_zt>(buf8, -1, '\n')` 一路填到遇分隔符或 EOF，
       ASan 实测 `WRITE of size 1`。写入量与调用方缓冲区大小完全无关。
     - **迁移风险是静默且单向的**：标准库同名接口收的是有符号的 `std::streamsize`，
       `std::istringstream::read(s, -1)` 是无害的 no-op（`gcount=0`、`failbit`、不越界）。
       从 `std::istream` 搬过来的同一个表达式在修前的本库上会变成堆破坏。
     - **`app_zt` 的补空字符判据同步改成 `n > 0`**（原为 `n != 0`），否则负的 `n` 会让异常
       路径往 `s[0]` 写一个 `TChar{}`。两参数转发版的同一处判据一并改。
     - **刻意不改的两处**（不是疏漏）：`ignore()` 的 `n` 仍是 `std::size_t`——它只丢弃、
       不写调用方缓冲，负值退化成「丢到 EOF 为止」，没有内存安全问题；
       `ostream_operators::write` 方向相反（读调用方缓冲），`n = SIZE_MAX` 实测被
       `mem_device` 以 `device_error` 挡下。两者是否随之改成有符号是一致性取舍，不是缺陷。
     - **`n == 0` 仍是与负值分开的一条拒绝**，行为不变，未被折叠进负值分支。
     - **回归**：`test_istream_read_char_7`（`test/io/istream/read/test_istream_read_char.cpp`）
       与 `test_istream_get_char_4`（`test/io/istream/get/test_istream_get_char.cpp`）。
       两者都覆盖 `-1` 与 `PTRDIFF_MIN`、`istream` 与 `iostream` 两种流、掩码开与关，
       且**断言调用方缓冲区一个字节都没被改写**（哨兵填充 `'#'` 逐字节核对）——
       这才是这条的要害，只断言状态位抓不到它。实测在修前的头文件上：
       ASan 构建报 `stack-buffer-overflow, WRITE of size 4096` 正打在用例自己的 `buf[8]` 上；
       无 sanitizer 的 debug 构建先 VERIFY 失败再段错误。

421. **`eofbit` 的判据是「本次操作还需要一个字符而没有」，不是「流恰好空了」；
     循环已被满足之后 `snextc()` 的那次预取不算。**（`ba3e4b77` 确立）
     - 落点：`io/utilities/istream_operators.h` 的
       `get<DelimPolicy, CStrPolicy>(s, n, delim)` 与 `ignore(n, delim)`，两处判据同构：
       `at_eof = (gcount + is_cstr < cap) && (!c.has_value())` /
       `at_eof = (gcount < n) && (!c.has_value())`。
       **前一半是 guard，不是冗余**：循环因「缓冲区满 / 已丢弃满 n 个」而退出时，
       本次操作并没有「还需要一个字符」，循环体末尾那次 `snextc()` 只是预取，不置 `eofbit`。
     - 因此 `get(b,3,'+')` 读 `"ab"`、`ignore(2,'+')` 读 `"  "`、`read(b,3)` 读 `"abc"`
       三者一致地停在 `goodbit`。
     - **这是与 libstdc++ 有意的分歧，不是疏漏**：libstdc++ 只在 `ignore()` 里 guard、
       `get()` 里不 guard，本库两边都 guard（`ba3e4b77` 提交信息原话：
       "libstdc++ guards this in ignore() but not in get(); IOv2 guards both"）。
       EOF 的探测也落在各操作自身而**不**在 `in_sentry` 的析构函数里：放在析构里的探测
       在管道/终端上会阻塞等待下一个事件（`cin.read(buf,3)` 读 `"42\n"` 要等到写端关闭才返回），
       而且会在从未越过末尾的操作后置 `eofbit`。
     - **`get`/`getline` 的三种收尾清清楚楚可区分**，不存在「无法区分」的问题
       （探针 `.review/tmp/r11io_eof_contract.cpp`，`n=4`、`app_zt`、`cons_sep`）：

       | 输入 | 退出原因 | 状态 |
       |---|---|---|
       | `"abc"`（恰好填满，无分隔符） | 缓冲区满，操作被满足 | `goodbit`，`buf="abc"` |
       | `"abcdefgh"`（行比缓冲区长） | 缓冲区满，且下一个字符不是分隔符 | `strfailbit`（"delimiter not found within buffer capacity"） |
       | `"ab"`（`n=8`，真的读过头） | 还需要一个字符而没有 | `eofbit`，`buf="ab"` |

     - **第十一轮曾把这条误报成缺陷**，提议改成 `at_eof = !c.has_value();`。误报的两个成因
       值得记住：(a) 拿 libstdc++ 当判据，把有意的分歧当成缺陷；(b) 注意到 `ignore` 与 `get`
       写法相同却判定「`ignore` 那边语义上成立、不要一起改」——两处本就是同一条规则的一致
       实现，这个内部一致性是最强的反证，却被读成了巧合。
       另注：函数文档里 `@note 到达 EOF 时置位 eofbit` / `@note Sets eofbit on reaching EOF`
       这句话字面上不够紧（"reaching EOF" 未点明是「本次操作还需要一个字符而没有」），
       正是它把审查者引偏的；契约本身没错。

422. **换向可以被转换器拒绝；拒绝之后流是自洽的，但压回内容不一定还在。**
     （`ba3e4b77` 之后的语义，第十一轮订正文档）
     - **`base_streambuf::switch_to_put()`（`streambuf.h`）有两个抛出源**，只有第一个在 `try` 里：
       ① 读缓冲区非空时 `tell()` + `m_cvt.seek(pos)` 被拒 —— 抛在 `m_read_buf.clear()` **之前**，
       压回内容保留；② `m_cvt.switch_to_put()` 被拒 —— 抛在 `clear()` **之后**，压回内容丢失。
       ②**与读缓冲区是否为空无关**：缓冲区为空时整个 `if` 块被跳过，② 照样抛
       （实测 `.review/tmp/r11ios_emptybuf.cpp`：无 `peek()`/`putback()`，`tell()=2` 时切向即抛）。
     - **`clear()` 的位置是承重的，不是疏漏。** 成功的 seek 已经把物理位置退到
       `pos = 物理位置 - m_read_buf.size()`，而 `tell()` 的定义就是物理位置减去 `m_read_buf.size()`；
       此时保留缓冲区会让 `tell()` **重复扣减**。所以「把 `clear()` 挪到换向之后」是错的修法，
       要挪就得在换向的 catch 里 seek 回原值，而那次 seek 自身又是新的失败源。
     - **拒绝之后流仍自洽**：位置正确、方向仍是读、可继续使用（`clear()` 后 `get()` 取到正确的
       下一个字符、`tell()` 正确）。转换器一侧也不变——`abs_cvt::switch_to_put` 先调派生层钩子、
       再动 kernel 与 `m_io_status`，钩子抛出时转换器原封不动。
     - **判据不是「设备是否支持定位」。** 订正前 `iostream.h` 把结果按此枚举成恰好两种并注明
       「本库自带的设备都是这一档（切向成功、不置任何状态位）」——这句话不成立：`mem_device`
       完全支持定位，而 `iostream<mem_device<char>, char32_t>` 未到 EOF 时必然落在 ② 上。
       真正的判据是**转换器**在当前状态下是否同意换向，那是转换器自己的策略，不是流层的契约。
     - **可观察后果**（探针 `.review/tmp/r11ios_which_throws.cpp`，两条异常信息分别指认两个抛出点）：

       | 切向时 `tell()` | 抛出源 | 状态位 | 压回内容 |
       |---|---|---|---|
       | `> 0` | ① 重定位被拒 | `cvtfailbit` | **保留** |
       | `== 0` | ② 离开读方向被拒 | `cvtfailbit` | **丢失** |
       | —（两者都通过） | 无 | **不置位** | 丢失 |

       因此**不能用 `cvtfailbit` 反推压回内容是否还在**；需要保留就在切向之前消费掉。
     - **tie 目标的后果比原文档写的重**：原文只说「丢失压回内容」。实测（`r11ios_tietarget.cpp`）
       只要转换器拒绝的条件持续成立，**每一次** tie 刷新都会在 target 上重新置 `cvtfailbit`，
       与有没有 putback 无关，而发起方一位不动（`writer` 保持 `0x0`）、也拿不到异常。
     - **订正的文档**（六处，均只改注释、无行为改动）：`iostream.h` 类文档的 tie `@warning`
       与新增的「双向不等于可以自由地读写交替」一条；`iostream::switch_to_put()` 的第二条
       `@warning`（换轴为「转换器可拒绝的两件事 → 三种结果」）；`streambuf.h` 里
       `m_read_buf.clear()` 上方那句已不成立的注释；`base_streambuf::switch_to_put()` 的
       `@throws` 子句（原文把抛出与「读缓冲区非空」绑定）；`abs_cvt::switch_to_put()` 补一条
       「换向可以被拒绝，被拒时本对象不变」。
     - **泛型层不写具体转换器的规则**：`code_cvt` 那条「变长/状态依赖编码要求 `is_eof()`」
       只是众多转换器之一的策略，留在 `code_cvt::switch_to_put_impl` 自己的文档里；
       `iostream.h` / `streambuf.h` / `abs_cvt.h` 只说「可以被拒绝」及拒绝后的可观察状态。

423. **第十一轮的三条形式项修改。**
     - **`io/iostream.h` 删掉 `#include <io/istream.h>` 与 `#include <io/ostream.h>`。**
       类名 `istream`/`ostream` 在该文件代码区出现 0 次——`iostream` 的基类是 `ios_state` /
       `istream_operators` / `out_flusher` / `ostream_operators` / `stream_common_operators`，
       缓冲区来自 `io/streambuf.h`。删后单独 `-fsyntax-only` 仍通过。
       **注意：删之前必须补三个测试文件。** 它们用 `IOv2::istream` 却只 include 了
       `io/iostream.h`，靠传递包含拿到声明：`test/io/ostream/seek/test_ostream_seek_char.cpp`、
       同目录的 `_wchar_t` 版、以及
       `test/io/ostream/inserters_arithmetic/test_ostream_inserters_atithmetic_wchar_t.cpp`。
       三者已各补一行 `#include <io/istream.h>`。
       （审查阶段曾断言「没有任何文件依赖这个传递包含」，那是**错的**；扫描时把
       「包含了 `ostream.h` 且只用 `ostream`」的文件与真正的违规混在了一起，配对判据写松了。）
     - **`over-pushback` 统一改为 `over-putback`**（`iostream.h`、`streambuf.h` 四处、
       `stream_common_operators.h`）。API 名是 `putback()` / `sputbackc()`，中文侧一直写的是
       「过量压回」，英文侧混用了 push。纯措辞。
     - **`get()` 的 `eofbit` 注释收紧**（`istream_operators.h`）。原文 `@note 到达 EOF 时置位
       eofbit` / `Sets eofbit on reaching EOF` 字面无条件，与第 421 条的契约不符，
       正是它把审查者引偏成误报。已改为写明「指本次提取还需要一个字符而没有；缓冲区填满后
       循环末尾的那次预取不算」。同文件 `read()`（「未能读满时置位」）与 `ignore()`
       （「若在丢弃 n 个字符前到达 EOF」）的注释本来就把 guard 写进去了，未改。
     - 四个目标文件其余形式项全绿：尾部空白 0、连续空行 0、均以 `\n` 结尾且末行非空、
       均可单独 `-fsyntax-only`、include 分组与组内字母序正确（无 C 头文件）、无拼写错误。
       （既有的三处尾部空白与两个缺 EOF 换行在 `test/io/istream/{read,get}` 下，本轮未动。）

424. **第十一轮并发轴：四条攻击轴全部跑完，未发现新问题。**
     每条 TSan 结论都配了阳性对照（同源码 `-DPOSITIVE_CONTROL` 加一个无同步自增），
     证明探针有分辨率而不是"跑了没报所以没问题"。

     - **`locale()` 设置器与格式化并发下 facet 无悬垂、无竞态**（探针
       `.review/tmp/r11c_locale_facet.cpp`）。攻击的是 `stream_common_operators.h:690-697` 的
       setter——它 move 出旧 locale、move 赋值新的，而格式化读的 `obj.locale()` 是**绑定到
       `m_locale` 的引用**，随后交给 `io_traits::swrite/sread` 去掏 facet。setter 第三条 `@note`
       声称二者永不交错（哨兵持同一把锁），**此前只有读码结论**，现在有了运行时证据：
       三个场景（`put_time %Z %z`/`put_money`/`<< double`/`setw+setfill` vs 不停打 `locale()`；
       `get_time %Z` 解析 vs locale setter；8 线程打进程级静态时区 trie），
       ASan+UBSan 零报告，TSan **连跑 3 遍 0 告警**，`-DPOSITIVE_CONTROL` 稳定报 5 条竞态。
       **`%Z` 这一支值得单独记**：它是 `e4ca8466` / `c1bb32c9` 的新代码，解析结果是**静态存储期
       指针**经 `std::tm::tm_zone` 交还调用方，**活得比 locale 长**。探针刻意在解析结束很久之后、
       且在别的线程正换 locale 时读 `t.tm_zone`：`parsed == zone_ok == 1600`，一次都没悬垂。
     - **操纵符自身没有跨线程共享的可变状态**（构造性论证，不需要压测）：`io_manip.h` 里所有
       `static` 都是静态成员**函数**（`swrite`/`sread`/`apply`），无静态数据成员、无 `thread_local`、
       无 `mutable`；唯二的命名空间级对象 `put_money_fn`（`:701`）与 `get_money_fn`（`:818`）
       都是 `inline constexpr`，不可变；全部载体类型都是按值构造的局部临时量。
     - **状态位与格式状态的并发读写**（第 2066-2068、2124 条）本轮顺带得到 TSan 复核：
       上面 O1 场景的第 6 个线程在其余 5 个线程做格式化与换 locale 的同时全程**无锁**读
       `rdstate`/`flags`/`fill`/`width`/`precision`，三遍零告警。原结论成立，
       且从「读码 + 设计论证」升级为「有 TSan 证据」。
     - **`try_flush` / `tie` 的深链与 fan-in** 见「未覆盖的范围」里那条已关闭的 TODO：
       四场景各 4000 轮，ASan 零报告、TSan 三遍零告警、阳性对照 4 条，
       24000 次成环尝试全部被 `check_tie` 拒绝（`accepted=0`）。

---

## `io/traits/*.h` 扩展点五个文件（第十二轮专项 + 收尾）

被审查的文件：`traits_base.h`、`char_and_str.h`、`arithmetic.h`、`nullptr.h`、`tm.h`。
追出去确认到的结论涉及 `facet/timeio.h`、`facet/numeric.h`、`io_manip.h`、
`utilities/*_operators.h`，那几个文件**仍未被系统性审查**。

### `char_and_str.h`

425. **定长字符数组提取无 off-by-one。** `num = min(width, N)`（**【第十五轮订正】写法不精确**：`width == 0` 是「不设宽度」而非「宽度为零」，代进 `min` 得无意义结果；正确形式是 `width > 0 ? min(width, N) : N`，与第 437 条所写一致。实现本身正确，错的只是这处复述。）（判据是双侧的
     `0 < width && width < num`）、循环条件 `extracted < num - 1`、循环后无条件
     `*s = TChar{}`，终止符下标恒 ≤ `N-1`。矩阵
     `width ∈ {未设,0,1,2,N-1,N,N+1,1e6,PTRDIFF_MAX}` × 输入 `{<N-1, N-1, N, N+1, 空, 全空白}`
     × `N ∈ {2,4,8}`，ASan 栈红区零触发。
426. **`basic_string` 提取端的 `setw` 无 off-by-one**（第 205 条原为读码推理，此处为运行实证）：
     4000 组随机 + 112 组分块边界（长度 0…1000 × `setw` 0…1000）与 libstdc++ **逐字节相同**。
     128 字节暂存与换块逻辑正确。
427. **`width` 在两个下游函数的入口就被消费。** `ostream_insert` 与 `istream_extract`
     都在做任何可能抛出的事之前 `io.width(0)`，因此这两条路径上的每一个抛出点都已消费 width。
     两个函数之外、调用它们之前的抛出点（取 facet 失败、空指针、加宽缓冲分配失败等）
     由 `ios_base::width_guard()` 兜住（见第 477 条）。判据不是「有没有 `width(0)`」，而是**成功路径消不消费 width**：
     无格式化函数（`put`/`write`/`get`/`getline`/`read`）按 [ostream.unformatted] /
     [istream.unformatted] 本就不碰 width；哨兵失败时格式化函数「什么都不做」，
     width 按标准不该被重置；`get_money` / `put_time` / `get_time` 的成功路径本来就不消费，
     给它们补 `width(0)` 反而制造新的不一致。六个场景与 libstdc++ 逐项相同。
     全文件普查 121 格，其中 6 格「不消费」经查 libstdc++ 后确认是正确行为
     （`is >> c` 单字符提取本就不消费 width）。
428. **写侧填充与对齐与 libstdc++ 逐字节一致**：`std::string` / `const char*` / 单字符三条路径，
     `setw(0..11)` × `left/right/internal` × `setfill('*'|'0'|' ')`，4000 组零差异。
     `internal` 对字符串等同右对齐，与 std 相同。
429. **`max_pad_count` 边界**：判据是 `pad > 65536` 且在写出任何字符之前；
     `setw(65538) << "ab"`（pad 恰为 65536）放行，`setw(65539)` 抛且**零输出**。
     `width = PTRDIFF_MAX` 走拒绝路径，不会试写 2^63 个填充字符；
     负宽度在 `ios_base::width(ptrdiff_t)` 设值时就被拒，不存在「负宽度转 `SIZE_MAX`」的整数陷阱。
430. **加宽路径（`io_traits<TChar,char*>`）的 `n == 0` 无 UB**：`std::vector<TChar> buf(0)` 的
     `data()` 是 `nullptr`，但 `widen_seq` 是显式循环、`std::copy` 对 `ostreambuf_iterator`
     走循环特化、`nullptr + 0` 语言层良定义。`char`/`wchar_t`/`char8_t`/`char32_t` 四种流 × 7 组
     零 sanitizer 报告；连会退化成 `memmove(dst,nullptr,0)` 的裸指针汇路径，
     gcc15 与 clang21 两套 ASan+UBSan 也都零报告。
431. **`signed char` / `unsigned char` 全家**：256 个字节值写出、255 个字节值读回全部正确；
     `reinterpret_cast` 合法（窄字符类型间经 `char*` 访问）；高位字节经 `ctype` 表时索引已强制转
     `unsigned char`，**无负索引越界**。
432. **自定义 `TTraits` / `TAlloc` 不构成问题**（第 2254 条的顾虑不成立）：`basic_string` 特化
     只用 `erase()` / `append(ptr,len)` / `data()` / `size()` / `max_size()`，
     从不对非终止缓冲区调 `TTraits::length`，空白判定走 `ctype` facet（与标准一致）。
     改写了 `eq/lt/compare/length` 的 `odd_traits` 两个方向均正确。
433. **特化路由无二义**：`<char,char*>` / `<char,const char*>` 两个全特化压过 `<TChar,TChar*>`
     与 `<TChar,char*>`；数组名插入靠运算符的衰退重试绕过只有 `sread` 的
     `io_traits<TChar,TChar[N]>`，四种数组类型实测正确。
434. **`width` 的读-改-写不是竞态**：`ostream_operators.h` 与提取侧同形，
     在调用 `swrite`/`sread` 之前已取 `io_mutex()`。
435. **`io_traits<TChar,TChar>::sread` 不消费 `width` 是正确的** —— 与标准
     `operator>>(basic_istream&, charT&)` 一致（第 258 条）。
436. **终止符在每一条放得下的出口都被写入。** `istream_extract` 的五条出口——正常结束、
     `num == 1`（缓冲区只放得下终止符）、缺 `ctype` facet、一个字符都没提取到，
     以及**异常从提取循环里逃逸**（循环包在 `try` 内，`catch (...)` 补写终止符再重抛）——
     行为一致，对应 [istream.extractors]/8「若一个字符都没提取到则存在第一个位置」。
     异常出口的写入下标恒在界内：循环每次写入前都被 `extracted < num - 1` 门住，
     写了 k 个字符后指针停在下标 k ≤ num-1。libstdc++ 的 `__istream_extract` 在这一条上
     **没有**兜住（它的 `*__s = char_type()` 同样在循环之后，且 `__catch(...)` 是吞掉异常），
     本库在此比 std 严。把失败放到 token 中途需要「短读 + 第 N 次 `dget()` 抛」的设备，
     `test/support/throwing_get_device.h` 提供它，现有四个设备夹具都做不到这个组合。
     `num == 0` 不可达：三个调用点传入的都是数组类型的 `N`（恒 ≥ 1），`width` 只会把它收窄到 `> 0`。
     这条保证是「失败后仍可安全 `strlen`」的依据；缺了它，未初始化的 `char buf[N]` 会让调用方越界读。
     默认（skipws）下哨兵先失败、什么都不写，与 libstdc++ 平价；`noskipws` 各格也逐格相同。
437. **`N == 1` 由 `istream_extract` 的 `num <= 1` 分支统一处理**：先清 width、
     再把下标 0 写成终止符、再抛。`io_traits<TChar,TChar[N]>::sread` 里没有更强的早退把它挡掉。
     [istream.extractors]/8 的三步（`n = width>0 ? min(width,N) : N`；提取到 `n-1` / EOF / 空白为止；
     随后存终止符；`width(0)`；未提取到则置失败位）逐条对上。
438. **`str.append` 抛 `length_error` / `bad_alloc` 时 `basic_string::sread` 的中间状态可用**：
     抛出后 `str` 恒为输入的**干净前缀**（长度必为 128 的倍数或 0），`c_str()` 的 NUL 不变式成立。
     `max_size()` 边界 127/128/129 三格无 off-by-one；超长输入静默截断且流保持 `good()`；
     失败后的读取位置与 libstdc++ 逐格相同。失败路径的 `width` 因入口前置消费恒为 0，
     libstdc++ 的 `width(0)` 在 try 末尾会残留——本库方向更安全。
     （用全同字符作输入的探针只能验长度、验不出内容被打乱；须用位置可辨的 pattern 逐字节校验。）
439. **宽字符流上的定长数组提取有运行证据**：每种字符类型 432 格（`width` 8 档 × 输入 6 类 ×
     `N ∈ {1,2,4,8}` × skipws 两态），`wchar_t` 侧与 libstdc++ `std::wistream` 逐格对照 diffs=0，
     `char / wchar_t / char8_t / char32_t` 四类型交叉对照零差异，合计 1296 格。
     目标缓冲做成堆上恰好 N 个元素的 `struct Box`（红区紧贴 `a[N-1]`），全程零 ASan / UBSan 报告。
440. **`char16_t` 流在公开 API 上不可达**，因此三个头在该字符类型上不存在用户会撞上的缺陷：
     五个 facet conf 都没有 `char16_t` 特化（`ctype_conf` 最先挡住，`collate_conf` 的
     `static_assert` 排在后面），而 `locale.h` 已把 `char16_t` 明确声明为不支持的字符类型。
     补 5 个 conf 桩绕过流层后，三个头的 `char16_t` 路径全部真正实例化并跑通，
     行为与 `wchar_t` 逐行一致，ASan 零触发。
441. **`io_traits<TChar,char*>` 的加宽缓冲无上界不是缺陷**：`std::vector` RAII，ASan+LSan 全程干净，
     无半构造对象、无半截输出，`bad_alloc` 归入 `otherfailbit` 且保存原始异常；
     libstdc++ 同名操作（`operator<<(basic_ostream<charT>&, const char*)` 里的
     `new _CharT[strlen(s)]`）同样无上界。三种手法（注入式 `operator new`、libstdc++ 对照、
     `setrlimit(RLIMIT_AS)` + 200 MB 窄串真实规模）结论一致。
     该函数体入口即取 `io.width_guard()`，守卫析构里的 `width(0)` 不抛，
     无栈展开中二次抛出导致 `terminate` 的风险。

### `arithmetic.h` / `nullptr.h`

442. **`nullptr.h` 的长度 / 终止符 / 区间三者自洽**：`"nullptr"` 恰 7 字符，
     `[c_buf, c_buf+7)` 半开区间合法，`TChar buf[7]` 恰好写满，`ostream_insert(s, io, buf, 7)`
     只读这 7 个。**无越界、无未初始化读、不依赖终止符**。四种流实跑全绿，
     `width`/`fill`/`adjustfield` 均正确，插入后 `width()==0`。
443. **指针特化 `swrite` 的 cv / 数组衰退 / 空指针**：`int*` / `const int*` / `volatile int*` /
     `const volatile int*` / `void*` / `const void*` / `int(*)[3]` / `Incomplete*` 全部正确打地址；
     空指针打 `"0"`（`showbase` 对 0 值不加 `0x`，与 libstdc++ 同）。
444. **指针特化 `sread` 的 `void*` 收窄成立**：`ExtC<void*> = 1`，而 `int*` / `char*` /
     `const void*` 均不可提取——`requires` 里的 `is_same_v<TValue, void*>` 与 `void*&` 形参
     两处收窄都真的生效。`void*` 往返正确。
445. **`long double` 极值 × 四种 floatfield × `precision(255)`**：40 组，ASan/UBSan 零报告。
     `fixed` + `long double max` + `prec 255` 实测输出 **6833 字符**，真的走了 `insert_float` 里
     「2048 上限 → `snprintf` 返回所需长度 → `resize` 重试」这条路径；后续三段缓冲均够用。
446. **算术读侧的退化输入**：26 组、目标对象前后各 16 字节哨兵，哨兵全部完好、
     ASan/UBSan 零报告；溢出饱和 + `strfailbit` 与 LWG 23 / libstdc++ 一致。
447. **`__int128` / `__float128` 两种方言下都安全。** 严格 `-std=c++23` 下
     `is_arithmetic_v` 为 0，两者都进不来（第 206 / 286 条复现）。`-std=gnu++23` 下
     `__int128` / `unsigned __int128` 被算术特化接纳，而 `insert_int` / `extract_int` 完全泛型
     （`make_unsigned_t` + `numeric_limits`）：无截断、无 UB，min/max 往返闭合，
     溢出按 LWG 23 钳位（同一表达式在 libstdc++ 上反而是歧义硬错误）；
     `__float128` 被第 449 条那个 `numeric_limits` 合取项挡在编译期。
448. **`bool` 的 `boolalpha` 往返**在 `LC_ALL=C` 下闭合。
449. **C++23 扩展浮点类型不会落进 varargs，越界判定也落在目标类型上。** 算术特化的约束在
     `is_arithmetic_v<TValue>` 之后还有一个 `numeric_limits` 合取项：浮点类型须满足
     `long double` 的 `digits` / `max_exponent` / `min_exponent` 三项都不窄于它，故 x86-64 上
     `float128_t`（113 位尾数 > x87 80 位的 64 位尾数）落选，退回「`io_traits` 特化不存在」，
     `requires { os << x; }` 如实答 0。能进来的 `float16_t` / `bfloat16_t` / `float32_t` /
     `float64_t` **由 `numeric` facet 自己承载**：私有的 `float_carrier_t` 选出能精确表示
     `TValue` 的**最窄**标准浮点类型（四者依次落到 `float` / `float` / `float` / `double`），
     `put` 先加宽到载体再交 `snprintf`、`snprintf` 的长度修饰符随载体而非 `TValue`，
     `convert_to_v` 在载体上 `strto*` 解析。io 层两侧都只把值原样交给 facet。
     两个后果：(a) 不存在「不参与默认实参提升的类型被 `%g` 按 `double` 读」的 UB；
     (b) 越界判据是 `isinf(parsed) || |parsed| > max<TValue>`，判定发生在收窄**之前**——
     `[conv.double]` 规定源值越界时的收窄本身是 UB，所以这个顺序是必须的——于是四个扩展类型
     与 `float` 走同一条 LWG 23 契约：饱和到 `±max<TValue>` **并且**置 `strfailbit`，
     且饱和值在 `get` 抛出前就已写进调用方的变量。标准三类型上 `float_carrier_t` 是它们自己，
     新增的两个比较除 inf 外恒假，那三条腿行为不变。`float128_t` 若被直接调到 facet，
     `float_carrier_t` 的 `static_assert` 具名拒绝。
     这个结构与 [ostream.inserters.arithmetic] 对扩展浮点「条件存在的重载 + 先转换到标准类型」
     的规定一致。2700 组（五个类型 × 值矩阵 × floatfield × precision）与 libstdc++ 零差异。
     承载逻辑放在 facet 的私有成员而非命名空间级 trait，是为了不给公开头文件添可能撞名的名字。
450. **`os << u8"..."` / `os << (const char16_t*)` 是编译期拒绝，不打印地址。**
     37 格矩阵（char / wchar_t 流 × 单字符 / 数组 / 指针 / 非 const 被指类型 / cv 被指类型）
     与 libstdc++ 37/37 一致。`arithmetic.h` 的排除名单已完整表达 P1423R3 的 `= delete` 效果，
     且与 `test_ostream_inserters_character_char.cpp` 现有的 `static_assert` 一致。
451. **提取失败时目标对象是否被改写，与 libstdc++ 零差异。** 7 种类型 × 11 种输入 = 77 格，
     状态位语义归一后无差异。`int` 收 `""` 保值而收 `"abc"` 清零的区别来自**哨兵失败时
     facet 根本没被调用**——标准与 libstdc++ 是同样的行为。
     valgrind memcheck 另证明失败路径不读取目标的旧值。
452. **`hexfloat` × `grouping()[0] == 1` 的 locale 下 `0,x,1.fffep+3` 是平价行为。**
     确实会写出这种串、往返断裂，但 libstdc++ 逐字节相同（2520 格全矩阵差分为空）——
     `numeric_text` 是 `locale_facets.tcc:1088` 的逐行移植。越界读与缓冲区不足两项均已排除。

### `tm.h`

453. **`make_parse_context` 的整数运算无 UB、无不变量违反**（第 90 条原为 18 组手算，
     此处为 327 万组）：
     ```
     exhaustive done=262144  bad=0 threw=0
     edges      done=275398  bad=0 threw=0
     TOTAL      done=3275398 bad=0 threw=0
     ```
     ASan + UBSan 零报告。出口不变量（`mon∈[0,11]`、`mday∈[1,31]`、`hour∈[0,23]`、
     `min∈[0,59]`、`sec∈[0,59]`、`year∈[year::min(), year::max()]`）逐项断言通过。
     第 90 条的承重前提仍成立：`year_month_day{}` **只在** `std::clamp(first + days{offset},
     cal_min, cal_max)` **之后**构造，极端 `offset` 不会先造出越界的 `year_month_day` 再夹取。
454. **`tm.h` 三处 `std::clamp` 无悬垂**：返回的 `const T&` 绑到**同一完整表达式内**的临时量，
     结果在该完整表达式内即被拷贝/使用。
455. **`sread_admits<fwd_only> == 1` 而真调用是硬错误，维持非缺陷判定**（第 150 条）：
     用显式限定调用可以拼出来，编译通过、能跑，但**没有任何越界 / UB**。
     可选改法是对齐 `io_manip.h` 里那处已带完整约束的写法。
456. **`%Z` 读回写进 `tm_zone` 的指针寿命无悬垂**：用指针相等证明指向程序生命期的
     `s_timezone_tree`，locale 与流析构后仍有效。
457. **`detail::tm_stream_format` 的 `%z` / `(%Z)` 补齐判据与往返闭合无问题。**
     唯一不闭合的情形不在这个函数而在 locale 数据本身：`hy_AM` / `su_ID` 的 `%c` 是 12 小时制
     配一个空 designator，半天信息在写侧就已丢失（见第 469 条）。
     **【第十四轮订正】「唯一不闭合的情形」的名单已过期**：全 locale 往返扫描 6520 格
     （326 locale × 4 字符类型 × 5 时刻）里 6324 格闭合，不闭合的是 **9 个** locale，
     原因分类不变（locale 的 `%c` 装不下该字段），非代码缺陷。
458. **【第十五轮补注：本条结论正确，只需补证据等级——「跑通」仅指编译期实例化，
     真正驱动输入时显式指定的离平台档位会失配；但格式串与档位同源，
     每个平台上两者恒一致，故公开 API 上不可达。见第 519 条。】**
     **`tm_parse_tz_level` 的三档推档两侧同源**：put 侧与 get 侧读同一个
     `time_value_fields<std::tm>`，三档 × `%z`/`%Z` 六组合逐一落在预期分支；
     `offset` 档在 glibc 上逻辑不可达但代码可达（三档均已显式实例化跑通），是有意设计。
459. **`io_traits<TChar,std::tm>::swrite` 按 `width()` / `fill()` / `adjustfield` 补齐并消费 width。**
     标准划线的轴不是「处理的是不是时间」，而是**格式串从哪来**：`std::put_time` 由调用方给格式串，
     `time_put` 直通，既不补齐也不消费（libstdc++ 与 libc++ 同样"泄漏"，且这是
     [ostream.formatted.reqmts]/3 允许的）；而 `std::chrono` 的流插入器格式串由库定，补齐并消费。
     `os << tm` 用的是 `detail::tm_stream_format` 库自己拼的格式串，属后者。
     补齐需要预先知道总宽度，而展开后的 `%c` 没有上界，故 `width() != 0` 时先收进
     `std::basic_string<TChar>` 再交 `ostream_insert`；`width() == 0`（默认）时保持直写、不建缓冲。
     缓冲路径上 `tm_stream_format` 与 `mp->put` 都排在唯一的 width 消费点 `ostream_insert` 之前
     且都可抛（后者对任何字段越界的 `std::tm` 确定性抛出），故函数入口即取 `io.width_guard()`。
460. **`make_parse_context` 对越界字段的归一化与 POSIX 一致，是 documented behavior。**
     `tm_mday` 按「自当月 1 日起的天数偏移」处理，故 `0` 表示上月最后一天，
     于是 `std::tm t{}` 播种成 1899-12-31；实测 `mktime(std::tm{})` 逐字段相同
     （`ret=-2209046400`）。归一化只用 `std::chrono` 完成而不经 `mktime()`，
     是为了避开后者的 `TZ` 依赖、DST 平移、`tzset()` 全局状态与 pre-1970 失败。
     文档已把「未解析到的字段保留 `tmb` 的取值」限定为「除非其它字段的归一化进位波及到它」——
     `tm_mday` 的进位会一并改掉取值合法且未被解析的 `tm_year` / `tm_mon`。
     `std::tm t{}` 是 `get_time` 的规范写法而非病态输入；标准侧 `time_get::get` 从不归一化，
     未解析字段原样留着，故 `strftime("%F")` 给 `1900-01-00` 而本库给 `1899-12-31`，
     差异可见但两者都在各自文档的约定之内。

### `traits_base.h`

461. **`char_sink_for` 的行为矩阵**、**三个析取项的可达性与短路**、
     **形式判别 / 双形式 `static_assert` / 衰退档**、
     **「取了 `io_mutex()` 的流形式成员必须自己 catch」的名单**、
     **`@warning` 的两条断言**、**自包含性与库内真实迭代器** —— 逐项探针核过，契约与实现一致。
462. **扩展点配方描述的中转过程与实现一致。** 文档写的是「取得一个上下文临时量（来自
     `make_parse_context`，没有则默认构造），用 `io_traits<TChar, 上下文类型>::sread` 解析，
     再调它的 `convert_to(T&)` 写回目标」——`convert_to` 正是
     `istream_operators.h` 里真正调用的成员。按配方（提供 `operator T`、不提供 `convert_to`）
     写出来的用户类型 `extractable_with_ctx` 答 0、`is >> t` 不可用，而本库刻意放弃定制诊断，
     所以规格文件里必须点名这个成员，否则用户只看得到一句泛泛的 "no match for operator>>"。
463. **`extractable_with_ctx` 的判据与分派体一一对应。** 判据里有「上下文可获得」这一合取项
     （`make_parse_context(v)` 可调用 **或** `std::default_initializable<in_ctx_t<...>>`），
     与分派体的两条分支对齐，因此 `requires { is >> x; }` 对「特化了 `parse_context_type`、
     上下文不可默认构造、又没有 `make_parse_context`」的类型如实答 0，泛型代码能正确回退。
     分派体第三条分支是一句 `static_assert`，直接说明该补默认构造函数还是补
     `make_parse_context`，让直接写 `is >> x` 的人一眼看见缺什么，而不是掉进模板实例化深处。
     **【第十四轮订正】这句不成立**：该 `static_assert` 对它所针对的标准场景**不可达**——概念的
     第 3 个合取项本身就是「有 maker **或** 上下文可默认构造」，两者都不成立时概念先答 0、
     `operator>>` 直接退出重载决议，用户拿到的是泛泛的 "no match for `operator>>`"。
     唯一可达的形状下它的消息还是错的（说「has no make_parse_context()」而成员其实存在）。
     根因与第十四轮问题 2 同源：探测用的是非 const 左值。见第 502、509 条。
     这两者互补：判据管 SFINAE，断言管诊断。
     **返回类型是同一条不变量的另一半。** 分派体先问有没有 `make_parse_context`，有则以
     `static_assert` 检查其返回类型能否构造出上下文，**不**退回默认构造——默认构造只兜底
     「压根没有这个成员」。漏写 `return` 会让它返回 `void`，那种特化是写坏了而不是没提供，
     无声换成默认构造等于悄悄抽掉它本该播种的回退值（`std::tm` 恰好可默认构造，最容易中招）。
     判据 `extractable_with_ctx` 不管这一层：它问的是「拿不拿得到」，写坏的特化仍答 1
     并撞上具名断言。
     **【2026-09-06 修订，本条末句原先是错的】** 原文写「要求的是『能构造出 `type`』而非
     『就是 `type`』，返回一个可隐式转换的中间类型是合法特化」——第十三轮问题 2 证明那正是
     悬垂的来源（中间量在 `return` 那条完整表达式结束时即析构，上下文若持有指向它的引用，
     到 `convert_to` 时已经悬垂，ASan 报 stack-use-after-return）。契约已于 `b52394f` 收紧：
     `make_parse_context()` 必须**恰好返回 `type`**、且形参必须是 `const T&`，两者都是具名
     `static_assert`，不再退回默认构造。中间类型现在**不是**合法特化。判据侧未动，
     `extractable_with_ctx` 仍只问「拿不拿得到」。

### `facet/timeio.h`（随 `tm.h` 追进去确认的部分）

464. **写侧 4224 次 `put`**（44 说明符 × 3 修饰 × 8 locale × 4 字符类型）+ 17 个病态格式串，
     无抛出、无 sanitizer 报告；`bad_format` 出口**一定推进**（无死循环），
     `put_dec<0>` 的 10 字节缓冲恰好装下 `INT_MAX`。
465. **字段守卫完整**：`||` 短路正是阻止 `INT_MAX + 1900` 有符号溢出的那一环；
     `tm_gmtoff` 极值钳位，`tm_zone == nullptr` 不被解引用。
466. **456,768 次 `put` × 59 说明符 × 8 locale**（含 alt-digit 与 era locale），`'?'` 逃生口一次未触发；
     `%OC` 是唯一可能让下标超过 99 去索引 `m_alt_digits[100]` 的路径，被 `goto bad_format` 挡死。
467. **无 off-by-one、无截断、无游离 NUL。**
468. **与 libstdc++ `std::put_time` 在全部 312 个可用 UTF-8 locale 上逐字节相同。**
     余下的往返失败全部是 locale 的 `%c` 本身就缺 `%S` / `%p`，非缺陷。
469. **designator 为空的 locale 上 `%p` 两侧对称。** 写侧写出零字节（与 glibc `strftime` 一致），
     读侧 `%p` 匹配空串：不消费字符、不设上下午标志、也不算失败。
     读侧两处都带这个例外——主循环的 `case 'p'`，以及格式串尾部的跳过循环，
     因为 `%p` 落在格式串末尾时主循环会因输入耗尽而根本到不了 `case 'p'`。
     326 个 locale 全量往返扫描，负控制（缺 designator、伪 designator）在 en_US 上仍然失败。
     `hy_AM` / `su_ID` 读回的是 `h=1` 而非 `h=13`：这两个 locale 的 `%c` 是 12 小时制配空 designator，
     半天信息在写的时候就没了，标准也不保证 `put_time` / `get_time` 往返
     （libstdc++ 读回 `0:00:00`、libc++ 读回 `1:33:18`，都取不回 13）。本库与 libc++ 一致。

---

## `try_flush` 的重试预算、tie 图并发、方向切换与填充判据的运行验证

470. **`try_flush()` 的「短暂竞争由有限次重试覆盖」不是概率性主张，是可确定性验证的。**
     钩住 `pthread_mutex_trylock`，在第 N 次失败时放行 holder 且**在 holder 真的解锁之前
     不把失败结果交还**，「锁在第 N 与 N+1 次尝试之间释放」就与调度无关。三档实测
     （必然失败 / 第 1-2 次之间放行 / 第 2-3 次之间放行）证实三次预算的第 2、3 次都真能兜住。
     另有一个单核 + `yield` 的概率性探针给出与 `attempts == 3` 逐档吻合的阶梯，作为旁证。

471. **第 416–418 条的填充判据在四种字符类型上逐格一致。** `char` / `wchar_t` / `char8_t` /
     `char32_t`：`hex` + `setfill('f'|'F'|'a')` 一律置 `strfailbit` 且不输出，
     `setfill('0')` 在 `internal` 与 `right` 下都放行，`setfill('*')` 放行。ASan+UBSan 干净。
     `char16_t` 跑不了，原因见第 440 条（流本身实例化不出来，不是 `io_manip` 的问题）。

472. **`try_flush` 在三个以上 tie 目标 / 深链下的并发无问题。** 四个场景各 4000 轮，
     全部经 `ostream.h` 的公开入口驱动（`operator<<`、`endl`、`flush`、`tie()`、`rdstate()`）：
     S1 六节点深链（6 写 + 1 无锁读 + 1 尾节点 `<< flush`）、S2 六源 fan-in 到同一 target、
     S3 六线程分别试图把尾节点 tie 回链上每个位置的成环风暴、S4 五节点链上反复改绑。
     ASan+UBSan 零报告；TSan 连跑 3 遍 0 告警、无挂起，`-DPOSITIVE_CONTROL` 稳定报 4 条竞态
     证明探针有分辨率。S3 的 **24000 次成环尝试全部被 `check_tie` 拒绝，`accepted=0`**——
     并发下没有一次漏网，也没有一次让 Floyd 判圈在持有进程级 `tie_graph_mutex()` 时不终止
     （那种失败会瘫痪整个进程的 `tie()`）。S1/S2 的输出字节数三次运行逐字节一致。
     **结构性理由**（非仅实测）：`check_tie` 走图只在 `tie_graph_mutex()` 下进行，而唯一能
     写出非空 `m_tie_stream` 的路径就是 `tie()` 自己；四个特殊成员虽不取图锁，
     但**只写 `nullptr`**。故走图期间每条出边只可能是「原值」或「`nullptr`」，
     路径是固定的无环路径，Floyd 必然终止。第 185 条「链不传播刷新」在并发下依然成立。

473. **`iostream` 配 `cvt_creator` 时，方向切换在契约定义域内可观察中性。**
     孪生流差分：同一条随机读写脚本跑两遍（基准不注入 / 对照在每步前以 1/2 概率注入
     `switch_to_put()`|`switch_to_get()`），比对每步读出值、每步后的 `tell()`、以及最终设备字节。
     两个实例：无转换器的 `<mem_device<char>, char>`，与 `<mem_device<char>, char32_t>` +
     `code_cvt_creator`（UTF-8 变长，源串含 1/2/3/4 字节四种长度）。结果
     `rounds=5000 injected_switch_ok=56536 injected_switch_failed=16342 failures=0`，
     ASan/UBSan 零报告。另有独立模型对拍探针（300 轮全过）。
     **定义域边界**：脚本若允许「GET → WRITE → PUTBACK」这种压回**不匹配**字符的序列，
     注入组与基准组**会**分叉（注入的 `switch_to_put()` 丢弃替换内容），200 轮里出现 13 次；
     这正是 `streambuf.h:656-664` 与 `iostream.h:426-432` 明文写的语义，**不是缺陷**。

474. **`iostream` 配真实 `file_device`（读写模式）与 `mem_device` 落盘逐字节一致。**
     「读 4 再写 2」「写 3 再读 2」「r,r,w,w,r,r,w,r」三种脚本下，最终字节与读出序列逐字节一致，
     `tell()` 全程与模型位置相符；10 万字符的大文件写入落点正确。`failures=0`。

475. **`%z` / `%Z` 的宽字符一侧往返闭合。** `tm_gmtoff = ±400h` 在 `char` / `wchar_t` /
     `char8_t` / `char32_t` 四种字符类型下都写出 `+2359` / `-2359` 且不置失败位；
     `±08:00` / `-05:30` put→get 逐值闭合；`tm_zone` 为 `nullptr` 与 `""` 都写出 `UNKNOWN`、
     读回空串。`char16_t` 的情况见第 440 条。

---

## 字符插入的加宽边界与 `width` 守卫

476. **`TChar` 流上插入一个 `TChar` 不加宽，也不查 locale。** 字符已经是流的字符类型，
     加宽只会是恒等变换，所以 `io_traits<TChar,TChar>::swrite` 直接写出。这与同文件
     `io_traits<char,char*>` 与 `io_traits<char, basic_string<char>>` 的窄流路径一致
     （两者都按字节原样写出），也与标准一致——std 对这两侧都不加宽。
     可观察后果：char 流上 `os << 'x'` 在 locale 缺 `ctype` facet 时**照常成功**并补齐，
     而 `os << nullptr` 仍然失败（`nullptr.h` 要 `widen_seq` 把 7 个字节搬成 `TChar`）。
     需要加宽的是 `io_traits<TChar,char>`：那里源字符类型与流的字符类型本就不同。

477. **`ios_base::width_guard()` 是纯防御式清零的统一手法，与「取值后清零」是两回事。**
     全库 16 处 `width(0)`：7 处属「先把 width 读进局部量再清零、随后拿它做补齐或收窄」
     （`ostream_insert`、`istream_extract`、`basic_string::sread`、`monetary` 一处、
     `numeric` 三处），这类写成显式的读-清两步；其余 9 处只是为了「因异常等原因也要清掉」，
     用的是守卫（`char_and_str.h` 五处、`nullptr.h`、`io_manip.h` 的 `put_money`、
     `arithmetic.h` 两处），`tm.h` 的缓冲路径同此。
     守卫不可拷贝不可移动、靠保证的复制消除返回，析构调用的是 noexcept 的原子 `exchange`，
     故栈展开中安全；`[[nodiscard]]`，返回值必须绑定。
     语义与 `fmtflags_guard` 相反：后者存旧值再还原，它一律清零——因为 width 是一次性状态，
     格式化输出函数的契约是**消费**它而不是保管它。
     判据仍是第 427 条那条：看**成功路径消不消费 width**，而不是看有没有 `width(0)`。
     **【修订】** 上面「`char_and_str.h` 五处」应为**四处**（`:231`/`:293`/`:364`/`:400`）——
     `be1ae6b` 删掉 `io_traits<TChar,TChar>::swrite` 的加宽路径后少了一处，全库总数
     16 → **15**。
     **【第十四轮再订正】应为 16**：原枚举把 `tm.h:256` 那处写成「同此」而未计入守卫的 9 处。
     当前实际＝消费型单次交换 7 处（`monetary.h:767`；`numeric.h:316`/`:822`/`:990`；
     `char_and_str.h:67`/`:141`/`:617`）＋守卫型 9 处（`arithmetic.h:76`/`:189`；`tm.h:256`；
     `char_and_str.h:228`/`:290`/`:354`/`:390`；`nullptr.h:48`；`io_manip.h:856`）。
     四处 `width_guard` 的行号也已漂移：`:231`/`:293`/`:364`/`:400` → **`:228`/`:290`/`:354`/`:390`**。

---

## 第十三轮：`io/traits/*.h` 五个文件的逐项验证

以下均为第十三轮**实际跑过**（探针一律 ASan+UBSan）或逐行核对的结论。
三条确认问题本身见「未覆盖的范围」里的第十三轮小节与提交 `b52394f`。

### `traits_base.h`

478. **`char_sink_for` 不拒绝库内任何真实迭代器。** 五种字符类型的库内真实汇
     （`IOv2::ostreambuf_iterator`）全为 1、跨宽度组合全为 0。判据确实会接纳一个
     「赋值后不真写字符」的黑洞代理（`std::output_iterator` 的语义要求标准明文不可检测，
     `std::format_to` / `std::copy` 同样接纳），第二合取项查的又是迭代器**自报的**
     `value_type` typedef、可被代理的 `operator=` 绕过——两条都**够不着**，因为
     `operator<<` 永远只传 `o_iter()` 写死的返回类型。**若将来把汇开放给用户传入，
     这两条立刻够得着**，届时需重新判定。
     **【第十四轮订正】这个「够不着」的前提不成立，而且从来就不成立**：
     `io_traits<TChar,T>::swrite` 的迭代器形式是**公开类模板的公开静态成员模板**，
     `traits_base.h:22-36` 把这个签名当扩展点配方逐字列出，`:51-55` 更以「显式限定的手写调用
     能把宽键配窄汇」作为 `char_sink_for` 存在的**理由**——用户传自己的迭代器是文件自己
     文档化的**既有用法**。两条口子实测都够得着（`.review/tmp/r14tb_sink.cpp`：黑洞代理静默丢字符；
     自报 `value_type = char` 的代理让窄 facet 写进 `wchar_t` 数组）。**但仍不算缺陷**：
     前者是 `std::output_iterator` 的语义要求、标准明文不可检测；后者的后果与
     `traits_base.h:283-287` 已有 `@warning` 描述的同类——字符损坏，非 UB、不越界、不崩溃。
     下一轮不要因为「够不着」这个措辞而降低优先级。

479. **`is_void_v` 那个析取项不是死代码。** 唯一让它生效的形状是「显式特化
     `std::iterator_traits<I>` 且其 `value_type` 为 `void`」，实测可达。
     与它并列的 `!requires` 那一项也不冗余：`std::back_insert_iterator` 的
     `iter_value_t` 是 **ill-formed 而非 `void`**，只有 `!requires` 兜得住。

480. **退化实参一律答 0 而非硬错。** cv / 引用 / 数组 / 函数 / `void` / 不完整类型下，
     `parse_context_type` 主模板、`in_ctx_t`、`in_target_t`、`extractable` 全部答 0。

481. **两个主模板不存在「实例化不完整类型」的库内路径。** `io_traits` 只声明不定义，
     含「显式特化但没有定义体」这一格在内都不可达；`type` 被特化成不完整 / 引用 /
     `void` / 数组时概念全部挡下。

482. **`be1ae6b` 新增的三句契约与实现一一对应**（const 左值探测、`convertible_to` 而非
     `constructible_from`、「返回类型不对时不退回默认构造」的三分支结构）。
     **后两条各自引出了第十三轮的问题 2、问题 3，契约已于 `b52394f` 收紧**：
     `make_parse_context()` 现在必须恰好返回 `type`、形参必须是 `const T&`。

### `char_and_str.h`

483. **四处 `width_guard` 都是函数体第一条语句**，其前无可抛点；19 格运行验证
     （写侧 13 格 + 读侧 6 格）证实 `width` 在所有可达抛出路径上都不残留。

484. **抛异常的 `TTraits::copy` 造成的 1799 字节泄漏不是本库的。** 对照探针完全不含 IOv2，
     只在 `main` 里对 `std::basic_string<char, boom_traits>` 直接 `append`，同样泄漏、
     栈顶完全相同——泄漏发生在 libstdc++ 的 `_M_mutate`（先 `_M_create` 再调
     `traits::copy`，`copy` 抛出时新缓冲既未接管也未释放）。且抛异常的 `char_traits::copy`
     本就在 [char.traits] 契约外。

485. **抛异常的 allocator 下两侧都干净**：`sread` / `swrite` 零泄漏、无半构造对象，
     `str` 仍完好可用，`width` 已清零。

486. **`str.erase()` 之后抛出导致调用方看到空串，与标准一致。** [string.io] 规定
     「sentry 为真后**调用 `str.erase()`**，然后提取」，清空发生在 sentry 成功之后；
     与 libstdc++ 逐格对拍，sentry 先失败的两格两边都保留旧值。
     **【第十四轮补正】本条只对拍了「`erase()` 之后立刻抛」（缺 ctype / 哨兵先失败）两格，
     没有对拍「循环中途抛」**，因此没看见第十四轮问题 3 那个分叉。另需记下：**libstdc++ 的
     `std::string` 走的是 `.so` 里的全特化、与泛型模板行为不同**（全特化从 streambuf 的 get 区
     成块 append，一个已消费字符都不丢；`std::wstring` 与自定义 allocator 走泛型模板，
     与本库改动前逐格相同）——以后做对拍时不要用 `std::string` 一条路去代表 libstdc++。

487. **迭代器 / 哨位契约全部正确。** `ostream_insert` 的前导填充 / 正文 / 尾随填充三段
     各自抛出时 `width` 都已消费；真实 `ostreambuf_iterator` + 设备写满时异常正确转成
     状态位且 `width` 归零；读侧「提前撞哨位 / 缓冲区先满 / 空输入」三条路径的终止符与
     返回位置都正确（`extracted < num - 1` 是循环第一个条件，缓冲区先满时根本不碰迭代器，
     分隔符不被消费）。

488. **`unsigned char*` / `signed char*` 四个特化 + 两个窄字符数组读特化**：六条空指针
     路径、高位字节、`reinterpret_cast` 均正确。循环内部抛出时 `catch(...)` 补终止符的
     路径在宽字符与 `basic_string` 两侧实测，与 libstdc++ 平价。

489. **`const volatile wchar_t*` 在 `char` 流上的不对称是刻意的**，路由在 `arithmetic.h`
     侧，`char_and_str.h` 不提供竞争特化，无二义（见第 492 条）。

### `arithmetic.h` / `nullptr.h`

490. **`be1ae6b` 新增的 `numeric_limits` 合取项不误伤任何整型。** 19 个标准算术类型逐个
     实例化，`bool` / `char` 家族的取值全部落在文档写死的契约上。但**正确性完全依赖
     `!is_floating_point_v` 左支短路**——右支若被求值，`__int128`（digits 127）会被挡掉。

491. **五个扩展浮点类型真实实例化并运行。** `float16_t`/`bfloat16_t`/`float32_t`/`float64_t`
     编译通过，`float128_t` 因 digits 113 > 64 在编译期落选；越界饱和判定发生在收窄
     **之前**，且用的是**目标类型**的极值（`float16_t` 收 `1e300` 得 65504 而非
     `max<float>`）。第 449 条由「推理 + 编译期探针」升级为实测。

492. **`const volatile wchar_t*` 不是 P1423R3 漏网。** 16 种 cv 组合与 libstdc++ **零差异**。
     排除项键在 `remove_const_t` 而非 `remove_cv_t` 是**正确的**——P1423R3 只删
     `const wchar_t*`，`const volatile wchar_t*` 落到 P1147R1 未删除的
     `operator<<(const volatile void*)`。

493. **四个 `swrite` / `sread` 函数体逐行审查**（此前从未被审）：四者都是无状态转发器
     （取 facet → 空指针检查 → 交给 facet），无分配、无算术、不写调用方对象、不吞异常。
     `arithmetic.h` 那处两步 cast 不是 UB——去掉 `volatile` 只在**访问**时才 UB，
     而 `numeric::put` 从不解引用。

494. **LWG 23 的写入 / 置位 / 抛出顺序无问题。** `strfailbit` 与 `otherfailbit` 是同一个
     `handle_exception` 按异常类型分派的**互斥分支**，不存在顺序问题；饱和值在抛出前
     就已写进调用方变量，与异常掩码无关。

495. **`nullptr.h` 全文验证。** `widen_seq` 严格 1:1，`buf[7]` 恰好填满、无未初始化读；
     `ostream_insert` 用显式长度、不找终止符；`width_guard` 在全部四类可抛点之前。
     四种字符类型 × width 边界（7/8 交界无 off-by-one）× 缺 ctype 全部正确。

496. **`max_pad_count` 边界在算术 / 指针 / nullptr 三条路径上都成立**（第 429 条只测了
     字符串路径，而 `arithmetic.h` 不走那条）：pad == 65536 放行且长度精确，
     pad == 65537 零输出 + `strfailbit` + width 清零，`PTRDIFF_MAX` 在写出任何填充前即被拒。

### `tm.h`

497. **`make_parse_context` 的极值字段无有符号溢出。** 31842 组单字段极值与多字段同时越界
     （含 `tm_year=INT_MAX && tm_mon=INT_MAX && tm_mday=INT_MIN`），出口不变量逐项断言
     通过，UBSan 阳性对照证明该编译命令确实能抓到溢出。所有算术都**先转型再算**。

498. **三处 `std::clamp` 的分支对 `std::tm` 一侧都真的可达**（此前第 454 条只有「无悬垂」
     结论，可达性没有断言锁住）：六组输入分别触发年份钳位、日偏移 `±4'000'000` 钳位、
     日历上下界钳位；文档「stops about ±10,951 years」实测精确成立。

499. **`tm_stream_format` 的两个 `TChar[]` 字面量逐元素核对无 off-by-one。**
     326 个 UTF-8 locale × 4 种字符类型 = 1288 格，`fails=0`；`grew` 直方图
     3→252 / 5→20 / 8→1016，**三种分支组合全部真实可达**（`grew=5` 只出现在
     `nb_NO`/`nn_NO`/`ro_RO`/`si_LK`/`uk_UA` 五个 locale 上）。`contains_specifier`
     大小写敏感，故两个 `if constexpr` 块的顺序依赖安全，且不把 `%%z` 误判为 `%z`。

500. **`swrite` 缓冲路径正确。** `ostream_insert` 拿到的是**已写出字符的真实计数**而非
     预估；`max_pad_count` 比较的是 **pad** 而非总长（`len=65572 = 36 + 65536` 放行直接
     证明），判据是 `>` 不是 `>=`；`width_guard` 是函数体第一条语句。超限时零输出、
     `put` 抛出时零半截输出、width 均不泄漏。

501. **`sread` 的形参/实参匹配与哨位传递正确。** 与 `char format` 重载无二义，`fmt` 是
     具名局部量故 `string_view` 不悬垂。36 个截断前缀全部抛出而非越界，机制是**两道独立
     的闸**：哨位只防越界（退出时 `succ == true`），抛出来自 `facet/timeio.h` 那个
     「格式串未消费完而输入已耗尽」的独立检查。`tm.h` 原样转发 `iter_end`，
     自身没有读过头的窗口。

---

## 第十四轮的四条确认问题：处置与验证（提交 `50aea5c`，分支 `fix/io-parse-context-detection`）

四条问题全部落地，`include/` 七个文件、+206/−70 行。**全量 57/57 通过**（`build/gcc-debug`），
gcc 15 与 clang 21 在 `-Wall -Wextra -Wcomment -Werror` 下均干净，探针一律 ASan+UBSan。
问题的完整描述与证据见 `.review/r14-*.md`（`issues.md` 是每轮重写的暂存件，内容已并入本清单后删除）。

502. **`make_parse_context` 的判据换了**种类**，不是又一次收紧——这是三轮返工的根因。**
     前三轮（`be1ae6b` / `b52394f` / `3f87b88`）检查的都是「**调用**能不能成立」，而满足某个
     调用性质的声明集合是**开放**的，所以每轮都有新形状漏过去：第十三轮堵了返回类型，
     第十四轮就从形参侧漏（`const U&` 中间类型 → ASan stack-use-after-scope）；同时
     存在性探测用非 const 左值，`T&&` / `const T&&` / 非静态成员被当成「没有这个成员」而
     静默默认构造。八种非 `const T&` 形参形状里只有一种真报错。
     现在改成比较**声明本身**：`decltype(&TPct::make_parse_context)` 与要求的函数指针类型
     比对，是**封闭**比较。这类 bug 到此为止。

503. **存在性与形状分开，且存在性判据里绝不能混入形状。**
     `detail::declares_maker` 用**继承歧义**只问名字在不在：派生自待测特化并同时继承一个带
     同名成员的标签，查找歧义即「存在」。与形状无关，故静态成员、非静态成员、重载集、
     成员模板，以及误写成数据成员或类型别名的情形一概查得出。
     一旦把形状混进存在性判据，写坏的特化就会被判成「没有这个成员」而静默退回默认构造
     ——那正是要根除的失效模式。已写成 `@warning`。

504. **`final` 特化当不了基类，由第二个析取项（即原来那条调用探测）兜住。**
     曾考虑改用「五条调用形式的并集」替代歧义探测，**实测被否**：并集是**枚举**，凡形参列表
     不是「一个能由目标转换来的实参」的形状一律漏——`f()` 忘写形参、`f(int)` 形参写错、
     `f(const T&, int)` 多一个、数据成员、类型别名、`f(T*, int)` 七种全漏（`r14x_union_gap.cpp`），
     而这些恰是写扩展点时最常见的手误。也曾考虑「拒绝所有 `final` 特化 + 具名断言」，
     被否是因为会打掉今天可用且正确的 `final` 特化。

505. **`noexcept` 必须单列一档。** 自 C++17 起 `noexcept` 是函数类型的一部分，裸
     `same_as<decltype(&f), TCtx(*)(const TV&)>` 会把一个写法完全正确的 `noexcept` 成员误判成
     形状不符。`maker_signature_v` 用一对偏特化同时收两种。
     **这条是本轮在自己刚写的代码里查出来的**，也是「换了判据种类就要把边界重扫一遍」的例证。

506. **形状矩阵 15 种 × {`final`, 非 `final`}，gcc 15 与 clang 21 逐格一致。**
     非 `final`：正确形状可用，十一种写坏的全部拿到具名断言，只有「压根没有」退回默认构造。
     `final`：正确形状**仍可用**（不破坏今天能用的），`const U&` 悬垂那格仍被抓住；
     `T&&` / `const T&&` / 非静态 / 同名数据成员四格静默——**那是改动前就有的行为**，
     语言层面关不掉（歧义探测必须把特化当基类），已在 `traits_base.h` 写明「特化不要写 `final`」。

507. **`maker_probe` 继承十三种敌意类形状全部安全**：有虚函数、抽象类、虚基类、
     析构 `= delete`、默认构造 `= delete`、不可拷贝、含 `const` 成员、遮蔽基类同名成员、
     含同名嵌套类型——无硬错误、无误判静默；maker 落在 `private` / `protected` 基类的两种
     正确诊断为断言。**继承来的 maker 可用**。

508. **普通类型不受影响，`maker_probe` 根本不会被实例化。**
     `int` / `double` / `std::string` / `char` / `void*` / `bool` 的上下文都是恒等，概念第二个
     合取项就短路了；全库只有 `std::tm` 走这条路。

509. **概念现在对「声明了但写坏了」的特化答真，用它时才撞断言——这是有意的，已写进契约。**
     不写下来，下一轮会把它当成「概念即分派」不变量的破口报回来。取舍是：写坏的特化应当
     响亮失败，而不是被泛型代码静默绕开；而「压根没有 maker 且上下文不可默认构造」仍答 0，
     泛型回退不受影响（第 463 条的设计意图保住了）。

510. **不可拷贝、不可移动且持有目标引用的上下文可用**，靠 C++17 的保证复制消除，种子经引用
     正确读到。这是「上下文持引用」这个合法用法的最严格形态。

511. **`basic_string::sread` 的抛出语义改成「保住已消费字符的完整前缀」**（方案 B）：
     提取循环包进 `try`，暂存缓冲区在异常路径上照样追加。原先每满 128 字符冲刷一次、
     循环后才追加尾段，中途抛出会丢掉最多 127 个**已从输入上消费掉**的字符，
     留下的既非空串也非原值——而它自己的 `@note` 两种都承诺了。
     **收尾的 `append` 保留在 `try` 内**：append 是强异常安全的，失败的那次没有副作用，
     重试是保住暂存字符的唯一机会。曾把它挪出 `try`，用计数迭代器精确测量后**撤销**
     ——挪出去时「收尾 append 抛一次」那格从 300/300 退化成 256/300（`r14y_exact.cpp`）。
     文档同步改准：**保证只覆盖输入侧抛出**（迭代器、`ctype` facet、设备）；
     **追加本身失败**（`bad_alloc` / `length_error`）时暂存区最多 128 个字符无处可放，
     `str` 会短于已消费量。原文后半句「定长数组形式原内容至少还在」是**错的**，已删——
     `istream_extract` 在循环里 `*s++ = *iter`，抛出前已覆盖 K 个字节，只补终止符
     （实测 `array K=1000 -> strlen=1000`）。

512. **五处从不使用的形参去掉名字**（`char_and_str.h:210`、`tm.h:298`、`io_manip.h:1303`/`:1592`、
     `numeric.h:1768`），逐个核对过「确实用不上」而非「漏了该用」：单字符提取按标准本就不消费
     width；`timeio::get` / `put` 的签名**根本不收 `ios_base`**，无处可传；`extract_float` 所需的
     `m_grouping` / `m_in` 都是 facet 自己的成员。其中 **`numeric.h` 那句 `@param io 提供格式标志的
     流对象` 是错的**（函数体一个标志都没读），已随形参名删除；`io_manip.h:1303` 那条带真信息的
     转成了 `@note`。四种字符类型全路径重扫后 `-Wunused-parameter` 归零。
     （顺带记录一条既有设计，非缺陷：**`put_money` 的 `TMoney` 只接受整型（非 `bool`）或
     `std::basic_string<TChar>`，不接受 `long double`**，与 `std::put_money` 不同，文档已写明。）

513. **`traits_base.h` 一侧本轮核过、但不属于上面各条的六项**（`.review/r14-traits_base.md`
     有逐项证据，此处只留结论供去重）：
     (a) `iter_v` / `iter_c` 两个析取项**互斥**，无重叠误判；
     (b) 概念探测所用的迭代器类型与分派体实际使用的类型**无漂移**；
     (c) 概念 `extractable_with_ctx` 的**五个合取项与分派体分支逐条对齐**——第十四轮问题 2
         是唯一破口，其余合取项与分支一一对应（第 463 条只说了「判据管 SFINAE、断言管诊断」，
         没做逐条对齐，这里补上）；
     (d) 库内 `swrite` 调用点的**实参来源唯一**；
     (e) `cb6d007` / `cc59627` 那轮 TU 局部性的文档改动**已落到位**，
         `traits_base.h:97-100` / `:231-236` 附近无旧的「泛型代码可以直接用它分支」残留；
     (f) **一处纯文档遗漏（未修）**：`ostream_operators.h:522-525` 的 `insertable` `@note`
         缺 TU 局部性回指，而对称位置 `istream_operators.h:413-414` 有。不属四类问题，
         留待 `*_operators.h` 那一轮顺手补——同 `cc59627` 的教训：同一条结论复述在多处，
         改一处要 grep 全库。

---

## 第十五轮：`io/traits/*.h` 的边界、隐含假设与头文件卫生（提交 `98e9cc5` + `30547ff`）

分支 `fix/io-traits-round15`。本轮主题是「只找前十四轮没提到过的问题」，重点为
未覆盖的边界条件 / 隐含假设 / 竞态 / 头文件卫生 / 拼写。
确认问题 6 条（原报 7 条，其中一条复核后降级，见第 519 条），全部已落地。
中间记录在 `.review/r15-*.md`，探针在 `.review/tmp/r15*`。

**执行说明**：四个 agent 中三个撞会话配额被中断，但因要求边分析边落盘，
保住 935 行分析与全部探针；剩余部分由协调者补完。**这条经验值得保留：
简报里"每确认一条立刻追加"不是形式要求。**

514. **`arithmetic.h` 单独包含时 `os << "hello"` 静默打地址——本轮唯一的行为缺陷，已修。**
     两处特化把正确性**显式寄托**给 `char_and_str.h`（正文提及 10 次、6 处明确委托），
     而它**不包含**那个头，`ostream.h` 也刻意不包含任何 traits 头（开集策略）。
     于是只含 `arithmetic.h` 的 TU 里 `os << "hello"` → `0x4c4aa0`、
     `os << (signed char)'A'` → `65`、`os << (const signed char*)s` → 地址，**流全程 `good()`**。
     对照组 `os << 'C'` 是硬编译错误——因为 `char` 真在排除名单里，
     这证明「靠排除名单」与「靠别的头的特化压过来」**强度完全不同**。
     **修法**：排除项由「缺席」改为「条件排除」，正是本文件自己的设计注记所规定的
     （类模板没有 `= delete`，排除名单就是让组合「不存在」的手段）。
     `signed char`/`unsigned char` 是**二维**的（`char` 流上是字符、宽流上是数值），
     故条件挂 `TChar == char`；`char` 被指类型**无条件**排除；三处键都用 `remove_const_t`，
     `volatile char*` 仍打地址。带两个头时行为逐格不变，缺头时变编译错误。
     **未采纳「让 arithmetic.h 直接包含 char_and_str.h」**：它用不到那个头的任何名字，
     那会是「为副作用而包含」，与本目录其余部分的 include 卫生相悖。
     （`nullptr.h`/`tm.h` 包含它是因为真的调用 `ostream_insert`。）

515. **本条推翻了「TU 局部性」一节的结论。** 那节写着「缺头在**使用**路径上是硬编译错误，
     只有**探测**路径静默」——第 514 条是使用路径上的**静默错误输出**，为该结论的反例。

516. **`tm.h` 的 `make_parse_context` 无条件读目标 `std::tm`，未初始化即 UB——已补文档。**
     `std::tm t;`（不写 `{}`）后 `is >> t` 或 `get_time(&t, fmt)` 读六个不确定值播种回退，
     **即使格式串填满六项也照样触发**（播种在解析之前）。valgrind 46 errors / 28 contexts，
     垃圾真流进结果。**与 `std::get_time` 相反**（`time_get::get` 只写不读），是移植陷阱。
     **注意**：`io_manip.h:1453` 起的 `get_time` 文档**中英双侧本来就写全了**，
     缺的只是 `is >> t` 这条入口——`io_traits<TChar, std::tm>` 只有 `swrite`，
     提取全部经 `parse_context_type` 中转，故警告加在 `make_parse_context` 上即覆盖两条入口。
     **教训：报告问题前先 grep 全库的文档，本条大半是重复发现。**

517. **`char_and_str.h` 的「终止符必写」契约在哨兵失败时不成立——已改文档，行为不动。**
     该保证只覆盖 `istream_extract` **函数内部**；哨兵失败时按
     [istream.formatted.reqmts]「不尝试获取任何输入」直接返回，**目标数组一个字节都不被触碰**。
     边界在「哨兵失败」与「哨兵成功但 0 字符」之间——只有后者写终止符。
     **IOv2 与 libstdc++ 三格逐项相同**，故是注释 over-promise 而非行为缺陷，
     改行为会偏离标准。原注释把「可放心 `strlen()`」当卖点，正是病根。

518. **`min(width, N) - 1` 这条安全公式在 `width == 0`（默认值）下是错的——已改文档。**
     `width == 0` 是「不设宽度」而非「宽度为零」，代进 `min` 得 `-1`／按 `size_t` 得 `SIZE_MAX`
     ——**恰是这条注释想排除的「无上界」**。正确形式 `width > 0 ? min(width, N) : N`，
     与第 437 条所写一致。实现行为正确（2376 格实测），错的只是复述；
     第 205、257、425 条抄了同一条错公式，均已就地订正。

519. **【降级】`tm.h` 的三档 `TzLevel` —— 非缺陷，仅文档过度承诺，已加 `requires` 收紧。**
     初报为「`offset`/`none` 两档运行必失败、MSVC 上 `is >> tm` 恒定失败」，**那是错的**：
     `tm_stream_format` 与 `tm_parse_tz_level` 读的是**同一对布尔量**
     （`time_value_fields<std::tm>::has_offset`/`has_zone`），故**每个平台上两者按构造必然一致**
     ——MSVC 两个布尔都为假 ⇒ 档位 `none` **且**格式串两个说明符都不补，**不会坏**。
     探针 `r15tm_tier.cpp` **显式构造**离平台档位，测的是库永远不会产生的形状。
     与第 455 条同族（特化比库实际产生的更泛，显式实例化多出来的泛性会失灵，但无 UB、公开
     API 不可达）。已加 `requires (TzLevel == tm_parse_tz_level)` 把误用变编译错误。
     **第 458 条的结论正确、不要推翻**，只需补一句：「跑通」仅指编译期实例化。

520. **`char_sink_for` 的 `is_void_v` 注记说反了覆盖对象——已改文档。**
     实测五个标准输出适配器（`back_insert_iterator`/`front_insert_iterator`/`insert_iterator`/
     `ostream_iterator`/`ostreambuf_iterator`）**都声明成员 `value_type = void`，
     但 `iter_value_t` 全部 ill-formed**，故**全部**由 `!requires` 那项兜住；
     `is_void_v` **够不到任何标准适配器**，只覆盖显式特化 `std::iterator_traits<I>` 且
     `value_type = void` 的类型（可达，非死代码）。这正是「查 `iter_value_t` 而不查成员」
     的用处所在，原注释把这个设计要点讲丢了。顺带订正「两个析取项」→「三个里的前两个」
     （与第 461 条一致）。

521. **英文块整段缺失中文块有的正文——已补。**
     `char_sink_for` 的中文块有「本文件头部说明了这条约束为何不能省……」一段，英文块**没有**。
     缺的不是修饰文字，是**这条概念存在的理由**，也正是第 478 条订正所依据的关键。
     **躲过 `cc59627` 机械核对的原因**：那轮按结构标签逐个比对，而丢的是一段**不带任何标签
     的正文**，两边标签集完全相同。**下次做中英一致性核对，要比对无标签正文的段落数。**

522. **机械项与 include 审计全绿。** 尾部空白 0、连续空行 0、EOF 换行 5/5、末尾无多余空行、
     分组（本地→C++ 标准库）与组内字母序全对、**无真实拼写错误**
     （hunspell 报的 `ing`/`tio`/`tod`/`pre`/`mis` 逐个查上下文均为误报）。
     **include 缺失 0 条、多余 0 条**：剥注释后逐名核对 32 处 `std::` 用法 → 定义头、
     `<utility>` 一族零命中、库内名字 20 项全部由直接包含的头供出、
     **十次自包含编译**（gcc + clang × 五头，只 `#include` 它自己的 TU）全部静默通过；
     13 个标准库 include 与 18 个库内 include 逐个都能指出用点。
     **注意**：`traits_base.h` 的 `<type_traits>` **只被概念定义用到**、删掉也能编过
     （`<iterator>` 传递供出），但「用到了」是事实，**保留是对的**。
     **【第十七轮订正】本条的「库内名字 20 项全部由直接包含的头供出」不成立，见第 553 条**：
     `arithmetic.h` / `char_and_str.h` / `nullptr.h` / `tm.h` 四个文件用 `stream_error`
     却**不**直接包含 `<IOv2/common/defs.h>`，靠三层传递到手。
     本条其余部分（尾部空白、连续空行、EOF 换行、分组与字母序、拼写、自包含性、
     `<type_traits>` 的保留判断）第十七轮均已复核，**仍成立**。

523. **本轮排除方向**（记录，免得下轮重查）：
     - 格式状态竞态**不存在**：格式化插入在 `ostream_operators.h:894` **整段持有 `io_mutex()`**。
       流形式操纵符的竞态属 `io_manip.h`，已有文件级 `@warning`。
     - `obj.locale()` 引用在 `imbue` 下的寿命**已是契约**：`stream_common_operators.h:547-556`
       双语 `@warning`「不要把引用带出临界区」。
     - **`nullptr.h` 的 `buf[7]` 不可能被自定义 ctype facet 写溢出——结构上保证**：
       `widen_seq`（`ctype.h:351`）是 `detail::ctype_ops` 的**非虚**成员，
       实现是 `while (beg != end) *dst++ = self().widen(*beg++);`，严格 1:1，
       可定制点只有单字符 `widen()`。比第 495 条的「实测 1:1」更强。
     - 三个调用者的 `@throw` 不列 `max_pad_count` **不是缺陷**：本库一贯只列本函数自己抛的
       （`basic_string::sread` 同样不列 `bad_alloc`）。
     - `ctype` 表的高位字节、`size_t` 回绕、`buf[len++] = *iter` 的求值顺序、
       窄字符 `reinterpret_cast`、偏特化二义性——逐条排除，见 `.review/r15-char_and_str.md`。

524. **新记录的库行为：本库主动拒绝数据有歧义的 locale。**（批 6 的 locale 扫描发现，
     此前任何一轮都没记过。）`br_FR`（AM 与 PM designator 相同）、
     `fy_DE` / `fy_NL` / `lo_LA`（重复的星期名）——timeio facet **拒绝在其上建立**，
     理由是那样的数据写出去读不回来。glibc 的 `strftime` 对它们照写不误
     （`br_FR` 输出 39 字节），**本库选了更严的一侧**。
     与第 457 条那 9 个「`%c` 装不下字段」的往返有损 locale **是两回事**。

525. **十五轮的欠账首次开始偿还：六批常驻回归用例已落地**（提交 `30547ff`，
     `io` 标签由 38 个套件增至 40）。逐批锁住的内容：
     - **批 1**（`test_io_traits.cpp` §11-13）：`make_parse_context` 8 形状 × 存在性/签名
       两个判据；**`noexcept` maker 单列一行**（最容易在下次改判据时被误杀）；
       `tz_level` 平台档（**写成相对 `tm_parse_tz_level` 的等式，不硬编码 `zone`**，
       故在 `std::tm` 不带那两个字段的平台上照样成立）；`char_sink_for` 三个析取项各自覆盖谁。
       **已做阴性对照**：四条关键断言取反后均编译失败，证明有分辨力。
     - **批 2**：哨兵失败 vs 0 字符提取的终止符边界；抛出时的前缀不变式（K=2…300，
       **129/130/255 是关键格**——改动前 boom=130 会给出 128 而非 129）；
       负 `signed char` 的 hex/oct **现状锁**；`max_pad_count` 三条路径。
     - **批 3**（`include_combinations/`，7 个 TU）：**唯一能看见第 514 条那类缺陷的东西**
       ——单 TU 测试按构造自洽。不变式是**四种结局里只有一种不被允许**：
       编译期拒绝 ✓、运行期失败并置状态位 ✓、正确文本 + `good()` ✓、
       **错误文本 + `good()` ✗**。不是 31 个 TU：`nullptr.h`/`tm.h` 都传递包含
       `char_and_str.h`，**有效包含集**远少于命名它们的子集，7 个覆盖全部有效集。
     - **批 4**：真实例化每种被接纳的浮点类型（§10 只查概念，**从不实例化运算符体**，
       故触及不到 facet 的 `float_carrier_t` 断言）；`float16_t ← 70000` 饱和。
     - **批 5**：`width==0` 的两分支上界；两条早退**不消费输入**；宽流写空窄串的
       `nullptr + 0` 路径。
     - **批 6**（`locale_dependent/`）：326 个 locale 上的格式串增长分支与写出成功。
       CI 镜像装了 `locales-all`，**可进常规 CI**。
     - **`__int128` 刻意未做**：`IOv2TestOptions.cmake:46-49` 明写 `CXX_EXTENSIONS OFF`，
       严格 `-std=c++23` 下 `__int128` 不是算术类型、根本进不到 traits
       （第 447 条已记）。为它单开 `gnu++23` target 会违背项目既定选择。

526. **写这批测试时它们抓到了六处我自己的错误假设**（记录下来，因为这正是回归用例的
     即时价值，也说明"探针跑过"与"断言锁住"不是一回事）：
     默认构造的流用**环境 locale**（带数字分组，不是 `"C"`）；
     `boom = 1` 是**哨兵**抛出而非 token 中途抛出；
     「正确或被拒绝」**漏了第三种合法结局**（运行期报告失败）；
     `char8_t` 流的 collate facet **拒绝非 UTF-8 locale**（`"C"` 对其他字符类型都行）；
     `str_fail()` **不是全部状态**（拒绝可能以别的位到达，要查 `good()`）；
     重构后遗留的旧断言。

---

## 第十六轮：`io/traits/*.h` 的第五轮复查（提交 `3c53d5b`，分支 `fix/sma`）

本轮主题：**把历轮只有推理／存疑的结论用运行验证打掉**，而不是找新方向。
确认问题 **1 条**（`char_and_str.h`），其余四个文件零缺陷。
八个 agent 里**六个撞会话配额被中断**，因简报硬性要求「每确认一条立刻落盘」，
分析与探针一条没丢，接手的 agent 靠读前任的 `.md` 直接续跑。
探针索引见文末附录（`r16*`）。

527. **【唯一缺陷，已修】catch 内的救援 `append` 会顶掉在飞的原异常
     （`char_and_str.h:669-679`）。**
     `basic_string::sread` 在异常路径上重做一次 `str.append(buf, len)` 以保住暂存字符，
     但**那一句自己会抛**（`length_error` / `bad_alloc`），一旦抛出，其后的 `throw;`
     根本不执行，原异常被整个替换。
     **危害不在异常类型变了**：`handle_exception` 按异常类型分派状态位，于是设备中途故障
     以 `otherfailbit` 而非 `devfailbit` 到达流——调用方的重试／换设备分支不触发，
     `m_exp_dev_fail` 收不到，诊断信息也从设备名换成 `basic_string::append`。
     不崩溃、无 sanitizer 报告，只有差分读码才找得到。
     **修法**：把救援 `append` 包进自己的 `try/catch` 吞掉其异常。**得到的行为恰好是
     `:611-615` 的 `@note` 已经承诺的那条**（「最多 128 个暂存字符无处可放，`str` 会短于
     已消费的量」）——它预见了字符丢失，只是没预见异常丢失，故**文档不必改**。
     否决的两个替代：预 `reserve`（无 `width` 时上界就是 `max_size()`）、
     append 前判尺寸（只挡 `length_error` 挡不住 `bad_alloc`）。
     **同形位置已核**：`istream_extract` 的 catch（`:183-187`）里 `*s = TChar{}`
     对字符类型不可能抛，安全，不动。
     边界实测：`str.size() + len` 是否越过 `max_size()`（`249` 塌、`250` 不塌）。
     回归锁见第 547 条。

528. **定长数组上界公式与实现一致，64 格 ASan 实测坐实**（第 518 条改的注释）。
     `width == 0 ? N - 1 : min(width, N) - 1`，`N∈{1,2,3,8}` × `width∈{0,1,2,3,7,8,9,PTRDIFF_MAX}`
     × 两种输入，目标数组放在**恰好 N 字节的堆块**上让 ASan 红区贴边，`failures=0`。
     **阳性对照**：把 8 元数组的真实存储缩到 7 字节，ASan 立刻报 `heap-buffer-overflow WRITE`，
     栈顶 `:181` ← `:557` ← `istream_operators.h:1267`。
     关键格：`N=1`（存 0 字符 + 终止符 + `str_fail()`）、`width=1`、`width=N`、`width=PTRDIFF_MAX`
     （上界退回 `N`，`0<width && width<num` 为假）。

529. **`istream_extract` 的退化 `num` 与 `TChar[0]` 都不可达，可达也无写。**
     库内三个调用点（`:557`/`:569`/`:581`）的 `num` 恒为 `N`，`:152` 的 `0 < width` 挡住压零。
     `io_traits<TChar, TChar[0]>` **不匹配偏特化**、是不完整类型：gcc/clang × `c++23`/`gnu++23`
     四组合全部验证，`is >> zero` 在 gnu++23 下亦是硬编译错误。
     `istream_extract` 是公开函数，用户手写可传 `num == 0`——那条路径**一个字节都不写**就抛。

530. **三个数组特化的 `reinterpret_cast<char*>`（`:569`/`:581`）在写入方向也合法。**
     对齐（三类窄字符 `alignof`/`sizeof` 均为 1，编译期锁住）；别名——[basic.lval]/11 的例外项
     覆盖的是 **access**（读**和**写），故通过 `char` 左值**修改** `unsigned char` 对象是允许的，
     实现比 `:455-457` 只提「读取」的措辞更宽但仍合法；值往返逐字节正确。
     10 格实测于 `-O2 -fstrict-aliasing -Wstrict-aliasing=2 -fsanitize=address,undefined`，
     含 `\xFF` / `\xC3\xA9` 高位字节（`\xFF` 作 `signed char` 是 −1、作 `unsigned char` 是 255，
     两侧都还原），`bad=0`、无警告。

531. **`io_traits<TChar,TChar>::swrite`（`:210`）缺 `width_guard` 是无害且结构性的。**
     `:212` 的读与 `ostream_insert` `:67` 的 `io.width(0)` 之间**没有任何可抛语句**；
     而 `<TChar,char>`（`:238`）那支在读之前有 `loc.get<ctype>()` 与 `mp->widen(c)` 两个可抛点，
     守卫在那里是必需的。用第 k 次 `operator=` 抛出的 `bomb_iter` 逐格实测 boom@0..5，
     **无守卫支与有守卫支在每一个抛出点都给出 `width_after=0`**。

532. **【第十四轮存疑关闭】`:212`/`:244` 的「load + exchange」两步残留：既非 UB，亦无逻辑后果。**
     **【第十七轮补】本条的枚举漏了第三个同形点 `tm.h:275`**（`width_guard` 之后
     `if (io.width() == 0)` 再读一次）。结论一样：非数据竞争，最坏后果是补齐丢失、
     不会损坏数据。全库同形点共**三**处，不是两处。
     （第十四轮原话「未能造出确定性复现」，行号今为 `:212`/`:244`。）
     **非数据竞争**：`m_width` 是 `copyable_atomic<std::size_t>`（内含 `std::atomic`，seq_cst），
     clang TSan 5 线程 × 20000 轮**连跑 3 遍 0 告警**，阳性对照稳定报 race。
     **逻辑层造出了确定性复现**：用户自定义汇 `hook_iter` 在 `operator=` 里把线程 A 停在
     `ostream_insert` **内部**（`:67` 的 exchange 之后、`swrite` 返回之前），再放线程 B 跑
     `width(9)`。结论分两半——
     `:212` 穷举全部交错**都可线性化**到某个串行序（读到 0 ⇒ 不做 exchange ≡「A 先 B 后」；
     读到 W1≠0 ⇒ exchange 拿到 B 写的 W2、按 W2 补齐 ≡「B 先 A 后」），
     「先读一次再 exchange 一次」与「只 exchange 一次」的**可观察结果集合相同**；
     `:244` 可观察到 B 的 `setw` 被整个吞掉，但那是 `width_guard` 的固有语义
     （`io_base.h:511-519`「不保存不还原，一律置 0」），**与两步残留无关**
     （改成单次 exchange 也一样吞），且 `io_manip.h:20-33`/`:68-86` 的文件级 `@warning`
     逐字写了这一条并给出解法（每个线程都用 `IOv2::sync`）。**两处都不必改。**

533. **【订正第 523 条】其第一点措辞过强，且与第 362 条矛盾——以 362 为准。**
     第 523 条写「格式状态竞态**不存在**：格式化插入在 `ostream_operators.h:894` 整段持有
     `io_mutex()`」。插入这一侧对（`os << 'X'` 走迭代器形式，全程持锁），
     但 **`setw` 走流形式**（`io_manip.h:666-682` → `ostream_operators.h:880` 的 `stream_v` 分支），
     那里**只有 `try/catch`、没有 `lock_guard`**，`apply()` 直接 `s.width(f.m_n)`。
     故「对方也必须持同一把锁才够得着」**不成立**。第 362 条其实已经写对了。

534. **裸指针作汇 + 空窄串不会走到 `memmove(NULL, 0)`。**
     链路 `io_traits<TChar,char*>::swrite`（`:375-378`，`n==0` ⇒ `vector(0).data()` 可能为空）
     → `ostream_insert` → `:83` `std::copy(s, s+n, iter)`。
     裸指针**确是**合法汇（`static_assert(char_sink_for<char*, char>)` 通过，gcc/clang 皆然），
     故某用例文件注释里「copy 是空区间循环而非 memmove from null」那句**不覆盖裸指针汇**；
     但 libstdc++ 15 的 `__copy_move_a2`（`stl_algobase.h:410-457`）两条 `__memcpyable`
     快路径都有 `__n == 0` 早退，**挡住了**。结论不变、理由该换。

535. **`basic_string` 读侧：`max_size()` 上界与双 `append` 都无缺陷。**
     `:646` `n = w > 0 ? w : str.max_size()`（第十四轮记为「未构造 GB 级耗尽用例」）——
     改用 `max_size()` 被压小的分配器实测：循环**不会越过** `max_size()`；
     `w == 0` 时超长 token 被**静默截断且不置失败位**，[string.io] 明文如此，
     与 libstdc++ 逐格相同（第 438 条已记录该行为）。
     `:667`/`:673` 的双 `append` **不产生重复字符**——依据是 [string.require] 对
     `basic_string` **全体成员**的强异常保证通则（不是本库的假设），
     throwing allocator 把 boom 从第 1 次 `allocate` 扫到干净跑完所需次数 +1，**12 格前缀全完整**；
     `:671-672` 的注释是**契约**而非想当然。顺带：`:611-615`「a failing append … `str` ends up
     shorter」是**偏悲观**的描述——重试成功时前缀是完整的。

536. **`basic_string::sread` 的 `width` 契约属实，五条出口实测。**
     `:596-597`/`:616-618` 称「`width` 被消费并清零，成功与失败都是如此」——
     成功 / `extracted==0` / 缺 facet / 设备中途抛 / `append` 抛，**五条出口全部 `width_after=0`**。
     **【第十七轮订正】「缺 facet」那一条要加限定：它按 `skipws` 分岔。**
     默认 `skipws` 下**哨兵自己就要 `ctype` 来跳前导空白**，先失败，`sread` 根本进不去
     ——那时 `width` **不被消费**（保持原值）、目标字符串**也不被清空**。
     只有 `noskipws` 时哨兵不需要 `ctype`，`sread` 才跑到 `:649-651` 抛出并消费 `width`。
     原条目那句「五条出口」说的是 **`sread` 函数内部**的五条，成立；但「缺 facet」经
     `is >> str` 这条入口在默认标志下**够不着**。两半都已固化，见第 549 条。
     `:641` 的 `str.erase()` 排在 `:645` 的 `width(0)` 之前**不构成泄漏**：`erase()` 默认实参下
     不可抛（标准 Throws 子句只列 `out_of_range if pos > size()` 而 `pos == 0`；
     libstdc++ `_M_check(0)`/`_M_set_length(0)` 逐行核对不分配）。
     哨兵失败时 `width` **不被任何人消费**、`str` 保持原值（提取侧全库无 `width_guard`），与 libstdc++ 同形。
     `:643` 未初始化 `buf` 在 `len == 0` 的两处 `append(buf, 0)` 下 **valgrind 0 errors**，
     阳性对照**在同一函数内**报 128 errors。

537. **`basic_string` 特化对恶意 `TTraits` 与自引用都安全。**
     把 `TTraits::length()` 谎报 1000000 的恶意 traits：**调用计数恒为 0**——本库从不调
     `TTraits::length`（独立佐证第 432 条），四条路径 ASan/UBSan 零报告，
     300 字符跨双 128 边界逐字节正确。
     自引用（`str` 与输入流同一块内存）经本库 API **不可达**：`mem_device` 按值持有字符串
     （`mem_device.h:89-90`）；手搓的持引用设备与缓存裸指针设备两种最坏形状 ASan 亦干净，
     且有**结构性理由**：`capacity(目标) ≥ size(源) ≥ token 长度` ⇒ 那串 `append` 永不重新分配。

538. **`tm.h` 的整数运算链干净；「加法在钳位之前」不构成溢出。**
     65760 组极值 UBSan 零报告（32 极值 × 6 字段单项 + 六字段同值 + 两组三重笛卡儿积），
     每组回读 `year_month_day` / `hh_mm_ss` 并断言 `ymd.ok()`、`sod ∈ [0,86400)`；
     阳性对照 `INT_MAX + 1` 被抓到。手工取值域另核：`/12`、`%12` 的 `INT64_MIN/-1` 够不着；
     `clamp` **确在** `static_cast<int>` 之前；`lo<hi` 是字面量无 UB。
     **「余量为零」第 5 条的推理坐实且可放心**：`norm_year` 已在 `:135-137` 先被钳到日历范围，
     故 `:154` 的 `first` 本身有界（±1.27e7 天），再叠 ±4e6 **在 25 位实现上也不溢出**
     （负侧余量 89'788 天，正侧 1'528'508 天，与该条推理逐位吻合）。
     **新事实**：`days::rep` 在 libstdc++ 是 **63 位**、在 **libc++ 只有 31 位**——见第 5 条的补注。

539. **`tm.h` 其余五项全部无问题。**
     (a) 第 519 条加在 `:312` 的 `requires` **无硬错误、无递归实例化、SFINAE 友好、与声明顺序无关**
     （**先用后定义**探针 + `-ftemplate-depth=64`，g++/clang++ 双跑；离平台档正确地回答为不完整类型）。
     **第 458 条结论成立**，只需注明「跑通」仅指编译期实例化。
     (b) `:223-224`/`:233-235` 两个 `tail[]` 终止符齐备，五种字符类型 `char_traits::length` 均可用，
     真实 `tm_stream_format` 输出无内嵌 `TChar()`（「余量为零」第 4 条现状核实）。
     (c) `:274`/`:323` 的 `fmt` **不悬垂**——`put`/`get` 不存留那个 `string_view`
     （上下文仅有的两个指针成员指向程序寿命的静态前缀树），ASan 往返在**销毁两个 locale 之后**仍存活。
     (d) `:279-281` 写侧输出**有界**：**879 个 locale** 实测上限 189 字节
     ⇒ 与第 535 条的读侧无上限**不同族**。
     (e) `:269`/`:275` 的两步窗口无数据竞争（同第 532 条），交错语义已在 `io_manip.h:20-21` 的契约里。

540. **【第 523 条升级】`nullptr.h:56-57` 的 `TChar buf[7]` 由「结构上保证」升级为实测。**
     `buf` 放在**恰好 7 元素的堆块**上 + 恶意 `ctype_conf` 重写（定制点确实触发，值变成
     `0x7f`/`0xabcd`），仍**恰好写 7 个**，ASan 干净；阳性对照（写 8 个）在 `ctype.h:354` 触发。
     `n == 7` 与 `"nullptr"` 一致、三处同一常量、`buf` 不依赖终止符（`ostream_insert` 用显式长度）。
     宽度双重消费**幂等**；facet 缺失抛出时宽度已消费，与 `:20-21`/`:31-34` 的承诺一致。

541. **【第 514 条修复的残余口子扫查：零违规】四结局不变式扫成 1872 格矩阵。**
     不变式（第十五轮批 3）：编译期拒绝 ✓ / 运行期失败并置位 ✓ / 正确文本 + `good()` ✓ /
     **错误文本 + `good()` ✗**。维度：`TChar` 4 种 × `TValue` 39 格（15 算术/cv +
     24 指针/数组/成员指针/函数指针/顶层 volatile 指针）× **12 个包含集**。
     地址归一化后逐格与全包含参照对拍，**无一格落到被禁的那种结局**。
     顺带订正：`test/io/traits/include_combinations/combination_body.h:27-31` 的
     「7 = 全部有效包含集」措辞不准，实测**有效集 10、可观察行为类 4**。

542. **`char_sink_for` 第一合取项可被内存不安全的汇满足，但判为调用方契约。**
     （第 478 条那两条口子——黑洞代理、自报错 `value_type`——已判非缺陷；本条是没查过的第三种。）
     `operator++` 让上次 `operator*` 失效的悬垂代理汇**能**满足 `std::output_iterator`，
     按 `traits_base.h:22-36`/`:51-55` 文档化的手写调用会 ASan SEGV（栈顶 `char_and_str.h:83`）。
     **但对照组** `std::copy` / `std::format_to` 用同一个汇**逐字同样炸**（崩点都在
     `stl_algobase.h:407`），而 `ostream_insert` 只是直筒转发、逐次接返回值、不复用旧迭代器
     ⇒ **本库暴露面与标准逐字相同，未加大**。`std::output_iterator` 的语义要求标准明文不可检测。

543. **`make_parse_context` 契约与 `4b993bb` 后的实现逐条对得上；两个退化写法不算缺陷。**
     七项（单一／非模板／非重载／公开／未删除／`static`／返回 `type`／形参 `const T&`／可 `noexcept`／
     可继承）全对。`test_io_traits.cpp` §11 只锁了 8 种形状，本轮补测**它没锁的 9 种 + 继承格，全部合约**
     （探针用 `99005 = 真被播种` / `−995 = 静默默认构造` 两个哨兵值区分）。
     两个退化写法：特化 `parse_context_type` 但**不给 `type`** ⇒ 静默不可提取，是四结局里
     **合法的「编译期拒绝」**——且**不是硬错误**，`in_ctx_t` 只出现在概念的原子约束里，
     运算符体在概念为假时根本不实例化（读代码容易误判成硬错误，值得记）；
     `type = T` 恒等 + 形状正确的 maker ⇒ maker 静默失效，见「未覆盖」一节的存疑 1。

544. **`arithmetic.h:231` 两步 cast 条款级干净；扩展浮点判据方向正确、维度不缺。**
     UB 的判据词是 **access**（N4950 [defns.access] = 读或改对象的值），而
     `static_cast<const volatile void*>` + `const_cast<const void*>` 只作用于**指针值**、
     不构造指向被指对象的泛左值 ⇒ [dcl.type.cv]/5 的前提不成立；另有一层保护：
     `:208` 的顶层 cv 排除保证 `value` 这个指针对象自身**从不是** volatile。
     三格实测：空指针写 `0`+`good()`（与 libstdc++ 逐字相同）、函数指针编译错误、
     成员指针不完整且无别的特化接走。
     判据（`:82-87`）：三项合取对 radix-2 类型**恰好等价**于「值集 ⊆ `long double`」，
     也就恰好等价于 C++23 `[ostream.inserters.arithmetic]` 的 conversion-rank 判据；
     **次正规不是漏掉的维度，是被 `digits` ∧ 规格化下端蕴含的**；只比 `long double` 一个也够
     （[basic.fundamental]/12）。八类型全表逐格对拍 5/5 一致，`bfloat16_t`（`digits=8` 但
     `max_exponent=128`）正确接纳。缺 `radix` 维度那条见「未覆盖」一节的存疑 2。

545. **`decay_t` 重试绕不过排除名单；双 requires 是 SFINAE；不完整主模板双编译器零分歧。**
     cv 排除 + `decay_t` 重试 **28 格与 libstdc++ 零字节差异**，
     `L"hi"` / `u8"hi"` / `wchar_t* volatile` / `char8_t* volatile` 四条最危险路径实测都是 `<rejected>`
     ——机理是排除名单键在**被指类型**上，而 `decay_t` 只动**顶层** cv。
     `arithmetic.h:131-135` 的模板级 + 函数级双 requires：`typename TIter::value_type` 缺席是
     **SFINAE 不是硬错误**（[temp.constr.atomic]/3），g++/clang++ 12 格一致，连裸 `const char*`
     也是干净拒绝，`traits_base.h:52-55` 的前提兜得住。
     `traits_base.h:375-376`「不完整主模板上取成员是 SFINAE」这条**整个设计的承重假设**：
     g++ 15.3.1 与 clang++ 21.1.8 **40 格零分歧**。

546. **本轮记录的三处「本库比标准更严」。**
     (a) `width(0)` 提到 `char_and_str.h:645` 最前，而 libstdc++ 的 `operator>>(istream&, string&)`
     放在 try 块末尾 ⇒ **libstdc++ 在任何抛出路径上都把宽度漏给下一次提取**；
     (b) 成员指针一格 libstdc++ 打 `1`（成员指针→bool），本库编译期拒绝；
     (c) 写侧越界 `std::tm` 本库抛 `stream_error` 且**一个字节都不输出**（`state=0x8`），
     `strftime` 照着打印垃圾。
     另：写侧未初始化 `std::tm` **是 UB**（valgrind 3 处告警），但与 `std::put_time` / `strftime` /
     `std::time_put::put` **同构**（「写出一个 tm 要先读它」是默认预期，不存在读侧那种反差），
     故不列为四类问题——**读侧**的警告（第 516 条）之所以必要，正是因为它违反直觉。

547. **【教训，写测试时必看】测试里凡是「标准库某个量与另一个量的关系」，一律运行时读，不要写死。**
     第 527 条的回归锁 `AFailingRescueAppendKeepsTheOriginalException`
     （`test/io/istream/extractors_character/test_istream_extractors_character_failure_shapes.cpp`）
     **第一版在 CI 上失败、本机全绿**，原因与被修的缺陷无关：它把
     `std::basic_string::max_size()` 当成了 `allocator::max_size() - 1`。实际那是**实现定义的函数**，
     而且**两台机器的 `_GLIBCXX_RELEASE` 同为 15 却公式不同**：

     | | `capped_alloc<char,200>` | `capped_alloc<char,250>` | 公式 |
     |---|---|---|---|
     | 本机 gcc 15.3.1 | 199 | 249 | `Max - 1` |
     | CI 镜像 `iov2-ci-gcc15` | **99** | **124** | `(Max - 1) / 2` |

     CI 上 cap=99 < 128 的暂存缓冲 ⇒ **第一次 flush 就在 `try` 内抛**，设备根本没机会抛，
     用例前提整个不成立（`dev_fail()` 为 false、`size` 为 0），而失败现象看上去像修复有问题。
     **靠 `_GLIBCXX_RELEASE` 判断不出来，只有 CI 会暴露。**
     改法两条：(1) 运行时读出 cap 再据此推导投放点；(2) 把 cap 压到 128 以下，让整个 token 装进
     **一个**暂存缓冲，于是唯一的 `append` 就是救援那次——前一版依赖「恰好一次 flush 已落地」，
     那是 cap 与 128 的**模关系**，本质上更脆（`cap % 128 == 127` 时根本无解）。
     两条前提现在写成 `ASSERT` 并注明该改什么。
     验证：`cap ∈ {2, 19, 49, 99, 126}`（**49 正是 CI 的值**）**有修复全过、无修复全败**。
     同类风险还有：容器 `capacity()` 的增长因子、SSO 阈值、`std::hash` 的取值。

548. **方法论：「边分析边落盘」不是形式要求，本轮第二次救命。**
     八个 agent 有**六个**撞会话配额被中断（第十五轮是四个里三个）。因简报硬性要求
     「每确认一条立刻 `>>` 追加到自己的 `.md`」，**分析与探针一条没丢**，
     接手的 agent 读前任的 `.md` 就能直接续跑，不必重跑任何已封闭的格。
     **下一轮的简报必须继续写这一条。**

---

549. **第十六轮欠账的五格已固化（提交 `a1c7c27`，分支 `test/io-traits-pinning`）。**
     纯测试改动，`include/` 零改动；全量 **60/60**（`io` 由 40 增至 41 个套件），
     九个受影响套件在 clang 侧亦全过。**下一轮复核时按下面的用例名直接查，不必重跑探针。**

     | 欠账 | 用例 | 关键点 |
     |---|---|---|
     | `make_parse_context` §11 未锁的 9 形状 + 继承格（第 543 条） | `test/io/traits/test_io_traits.cpp` §11 的 `declares_maker` / `maker_shape_ok` 两组断言（新增 `templated` / `overloaded` / `priv` / `deleted` / `byvalue` / `crvalue` / `datamember` / `nestedtype` / `inherited` 九个 fixture） | 检测**全部为真**（含非函数的数据成员与嵌套类；`private` 也挡不住，访问检查在名字查找之后），形状检查**只有 `inherited` 为真**。gcc 15 / clang 21 十八格一致；4 条取反后编译失败 |
     | `nullptr.h` 的 `buf[7]` 在恶意 `ctype_conf` 下（第 540 条） | `OstreamInsertCharacterChar.AHostileWidenStillYieldsExactlySevenCharacters` | `widen()` 对任何字节都答 `'#'`，输出仍是恰好 7 个 `#`——**输出是 `#######` 而不是 `nullptr`**，这才证明定制点真跑了而非断言碰巧通过 |
     | `basic_string::sread` 失败出口的 `width`（第 536 条） | `IstreamExtractCharacterFailureShapes.EveryExitOfTheStringFormSpendsTheWidth`、`IstreamExtractCharacterFailureShapes.AMissingCtypeStopsAtTheSentryWhileSkippingWhitespace`、`IstreamExtractCharacterFailureShapes.AFailedSentryDoesNotSpendTheWidth` | 固化时**测出第 536 条需要订正**（`skipws` 分岔，见该条的【第十七轮订正】）。设备抛那条早已由 `IstreamExtractCharacterChar.TheWidthIsSpentEvenWhenTheExtractionThrows` 覆盖 |
     | `file_device` 与 `mem_device` 落盘对拍（第 474 条） | **新套件** `io.iostream.device_parity`：`IostreamDeviceParity.TheThreeMixedScriptsLandTheSameBytes`、`IostreamDeviceParity.ALargeWriteLandsAtTheSameOffsetOnBothDevices` | 第十二轮那三个脚本（读 4 写 2 / 写 3 读 2 / r,r,w,w,r,r,w,r）+ 10 万字符写入，逐字节 + **每步 `tell()`** 对拍；带**非空转断言**（两个都不干活的设备也会「一致」） |
     | 填充判据与操纵符的 `char8_t` / `char32_t`（第 471 条） | `IosBaseFill.TheSameCriterionHoldsOnUtf8AndUtf32Streams`；新文件 `test_ostream_endl_utf.cpp`（`OstreamEndlUtf.*` 5 条）、`test_ostream_ends_utf.cpp`（3 条）、`test_ostream_flush_utf.cpp`（3 条）、`test_istream_ws_utf.cpp`（`IstreamWsUtf.*` 5 条） | 此前 `test/io` 下 86 个源文件里 26 个 `wchar_t`、**这两种字符类型一个都没有**。操纵符只重复**与字符类型相关**的那部分（dispatch + 「一个码元」），理由沿用 `wchar_t` 那几个文件自己的说法 |

     **写这批用例时测出的两条新事实**（都不是缺陷，但会咬人）：
     1. **第 536 条的 `skipws` 分岔**（见该条订正）。
     2. **`char8_t` 用例不能用默认 locale。** 实测 `LC_ALL=C` / `POSIX` 下
        **默认构造的 `char8_t` 流直接抛** `collate_conf<char8_t>: inter locale is not UTF-8`，
        只有 UTF-8 环境才活；`locale<char8_t>("C")` 一样抛。故本批全部显式写
        `locale<char8_t>("C.UTF-8")`（`char32_t` 用 `"C"` 无碍），并在
        C / POSIX / en_US.UTF-8 / zh_CN.UTF-8 **四种 `LC_ALL` 下各跑一遍**确认。
        这是第 526 条那个坑的实际后果，**与第 547 条硬编码 `max_size()` 是同一类错误**：
        本机（UTF-8 环境）过、C-locale 的 CI 炸。
        → **给写 `char8_t` 用例的下一个人：locale 必须显式给 UTF-8 的，别用默认。**

     **仍未固化的欠账**（第十二至十六轮清单的剩余部分）：
     - 那 9 个 `%c` 装不下字段的往返有损 locale 的 xfail 清单（第 457 条 F 节 / 第十四轮）
       ——**判定不做**：会把测试绑死在特定 glibc 上，与第十五轮对 `locale_dependent`
       直方图和「被拒绝的 locale 清单」的判断同理。
     - `__int128` 一路——第 525 条末已判定不做。
     - 四种字符类型的**读侧**全量对拍（第十四轮）：`char32_t` / `char8_t` 只做过写侧。（**已做，第 645 条。**）

550. **本轮（固化轮）没有新增确认缺陷。** 五格全部落地，两处把**已记录的结论订正得更准**
     （第 536 条的 `skipws` 分岔、`char8_t` 默认 locale 会抛）。
     方法上值得留一句：**这两处都是在"把探针改写成常驻用例"时才暴露的**——
     探针当时怎么跑通的就怎么记了，写成用例要面对默认标志、默认 locale、别的构建环境，
     于是前提被迫显式化。**「探针跑过」与「断言锁住」不是一回事**（第 526 条已说过一次，
     本轮是第二个实例）。

---

## 第十七轮：`io/traits/*.h` 的边界、隐含假设、竞态与头文件卫生（第 551-554 条均已处置）

本轮命题是「**只找之前没提到过的**」。四个 agent 互斥分工（A `char_and_str.h`、
B `tm.h`+`nullptr.h`、C `traits_base.h`+`arithmetic.h`、D 五文件的 include 与拼写），
四个**全部跑完没撞配额**（前两轮分别是 3/4 和 6/8 被中断），增量落盘仍照做。
**确认问题 4 条，全部经协调者独立复现**；`tm.h` / `nullptr.h` 零缺陷零存疑。
完整报告曾在 `issues.md`（每轮重写的暂存件，内容并入本清单后已删除）。

551. **【已修复】两个提取点对每个字符求值 `*iter` 两次，而约束并不保证 `*i` 稳定。**
     **类型是逻辑 bug（测一个值、存另一个值），不是文档问题。**

     **根因**：提取侧的全部约束是 `std::sentinel_for<TSent, TIter>` +
     `is_same_v<TChar, typename TIter::value_type>`。`sentinel_for` 只蕴含
     **`input_or_output_iterator`**，它只要求 `{ *i } -> can-reference` 与 `weakly_incrementable`
     ——**`indirectly_readable` 不在其中**。「`*i` 必须 equality-preserving / stable」
     那条保证来自 `indirectly_readable`，属于 `std::input_iterator`，
     **本库从来没有要求过 `input_iterator`，因此无权依赖它。**
     `traits_base.h:52-55` 自己就写着提取侧「查 `value_type` **最省事**」。

     **真正被坑到的不是「`*i` 会推进」那种迭代器**（那种任何写法都救不了，
     `*i` 读、`++i` 进是迭代器模型的基本盘），而是**合法但不稳定**的 `*i`：
     满足 `input_or_output_iterator`、`*i` 不推进、但两次求值给不同值
     （可变源 / 生成器 / 硬件寄存器）。实测：判空白读到 `a`、**存进去的是 `A`**。

     **修法**：两处循环改单次解引用（先 `const TChar c = *iter;`，再用 `c` 判、用 `c` 存）。
     行为等价，解引用次数 `2n+1 → n+1`。
     回归锁（新文件 `test/io/istream/extractors_character/test_istream_extractors_character_iterator_contract.cpp`）：
     `IstreamExtractCharacterIteratorContract.TheDelimiterTestAndTheStoreSeeTheSameCharacter`、
     `IstreamExtractCharacterIteratorContract.EachPositionIsDereferencedExactlyOnce`。
     另补 `IstreamExtractCharacterBounds.TheStringFormLeavesTheDelimiterToo`
     ——把 `is_any` 从条件挪进循环体后，`break` 放错就会吃掉分隔符，
     而「分隔符不被消费」此前**只有数组形式有 `peek()` 断言**；
     阴性对照（`break` 前多一个 `++iter`）证明该笔误在没有这条用例时**完全静默**
     （两次连续提取照样成功，因为第二次的哨兵会跳空白）。

     **【定性教训，本轮最值得记的一条】协调者把这条判错了两次**：先判「隐含假设，改文档」，
     再判「撤回，标准保证 stable」。两次都是**凭被调类型的惯常身份**
     （「传进来的都是 `istreambuf_iterator`」）推断契约，而没有去读
     **该函数模板实际写了哪些约束**。是用户坚持才查到 `sentinel_for` 的蕴含链。
     → **判定「这是标准保证的」之前，先读 `requires` 子句本身。**

     （以下为初次报告的原文，保留供追溯。）
551a. **（原文）两个提取点对每个字符求值 `*iter` 两次，这条前置条件没有任何地方写明。**
     （`char_and_str.h:172-178` 的 `istream_extract` 与 `:654-666` 的 `basic_string::sread`）
     循环形状是「`!(ct->is_any(space, *iter))` 判一次、`*s++ = *iter` 取一次」，
     实测 **4 个字符 → 9 次 `operator*`**（`2n+1`）。这等于**要求提取侧迭代器有 peek 语义**
     （解引用不消费、连续两次给同一字符），但 `@param iter` 只写「输入迭代器」，
     而 `traits_base.h:54-55` 把这个假设写成**事实陈述**（「实际传进来的只有
     `istreambuf_iterator` 一族」）而非**前置条件**——同时 `:22-36` 把迭代器形式的 `sread`
     **当配方列出**，第十四轮「更正 1」也已认定用户传自己的迭代器是**文档化用法**。
     消耗型 `operator*` 的迭代器**完全满足 `std::input_iterator`**
     （「解引用保持相等性」是 [concepts.equality] 的语义要求，**标准明文不可检测**），
     实测 `abcdefgh` → token `bdfh`，**静默、无失败位、流仍 `good()`**。
     **收紧 `requires` 没有用**，只有文档能防。本库自己的迭代器安全：
     `streambuf_iterator.h:176-194` 把 `sgetc()` 结果留在**局部变量**里，从不写 `m_c`。
     方案：把 peek 语义写进 `@param iter`，并把 `traits_base.h:52-55` 那句事实陈述改成前置条件。

552. **【已修复】「定长数组形式没有前缀保证」这句话与实现相反。**
     修法：中英各一句改成「前缀保证**更强**」。**并补了回归用例**——那句话改完就从「描述」
     变成了「断言」，断言就该有东西锁住：
     `IstreamExtractCharacterFailureShapes.TheArrayFormKeepsTheSamePrefixAndHasNoStagingToLose`
     在同样的 boom 点把数组形式与串形式**并排对拍**。
     （`char_and_str.h:593-594` 中 / `:613-615` 英）
     注释的**机制描述是对的、结论是反的**：恰恰因为字符**直接写进调用方数组**、没有暂存缓冲，
     数组形式抛出时留下的是**完整的、带终止符的前缀**，而且**比 `basic_string` 形式更强**
     ——后者有 128 字符暂存区，`append` 失败时会丢掉最多 128 个已消费字符（`:591-593`
     自己承认了这个例外），数组形式**没有这个失败模式**。
     实测：`boom=3 → "ab"`、`boom=5 → "abcd"`，恰好 `boom-1` 个字符。
     危害是双向的：照字面理解会**丢弃本可用的数据**，反过来读会**信任唯一真有丢失情形的那个形式**。
     `grep -rn` 确认该说法没写在别处。方案：只改文档，把那半句改成「前缀保证更强」。

553. **【已修复，但无自动防线】四个文件用 `stream_error` 却不直接包含 `<IOv2/common/defs.h>`。**
     修法：四个文件各加一行，放本地组首位。
     **⚠️ 这条回归不了、也测不出来**：实测把新加的 include 删掉，**自包含编译照样通过**
     （`defs.h` 从 `facet/numeric.h` → `clocale_wrapper.h` 传递过来）。
     自包含性测试只能发现「谁都不供出」的缺失，发现不了「靠别人供出」。
     CI 五个 job（build / thread-safety / coverage / install-paths / clang-tidy）里也没有
     include 卫生检查。要防住只能加 lint（判据是集合差
     `comm -23 <(提到 stream_error 的头) <(直接包含 defs.h 的头)`），
     **用户 2026-09-10 决定不做**。→ 下一轮若发现同类回归，属预期之内，不必惊讶。
     `arithmetic.h`（4 处 throw）、`char_and_str.h`（22 处）、`nullptr.h`（3 处）、
     `tm.h`（2 处）；`traits_base.h` 只在注释里提到，**无需包含，是对的**。
     定义在 `common/defs.h:107`，目前**三层传递**才到手
     （`arithmetic.h` → `facet/numeric.h` → `common/clocale_wrapper.h` → `common/defs.h`）。
     **这是本项目自己的标准**：`io_base.h`（20 处用它）、`ostream.h`、`streambuf.h`
     **都直接包含**；checked.md 第 49/50 条当年审 `io/ostream.h` 用的正是这条判据，
     明确把 `common/defs.h (stream_error)` 列为必需的直接包含；`3724b2f` 的提交说明
     也明确拒绝「为副作用而包含」。判据是集合差：
     `comm -23 <(提到 stream_error 的头) <(直接包含 defs.h 的头)` 只剩七个文件——
     `defs.h` 自己、两个纯注释提及、以及这四个。
     方案：四个文件各加一行，放本地组首位（`common` < `facet` < `io` < `locale`），零行为变化。
     **本条推翻第 522 条的「include 缺失 0 条」，已在该条就地标注。**

554. **【降级为调用方契约 + 补注释】插入侧默认「汇是无界的」。**
     **先查清了一件事：拿不到长度。** 插入侧全链路无哨位——20 个 `swrite` 重载全是裸 `TIter`，
     `ostream_insert` 与 facet 的 `put` 亦然；裸指针不带长度。要拿到得给整条链加哨位参数，
     是对文档化扩展点的破坏性 API 变更。（提取侧从一开始就带 `TSent iter_end`，
     **这才是两侧真正的结构差别**。）
     **标准也是这么做的**，IOv2 与之逐字一致：`num_put::put`、`format_to`、`copy`、
     `ctype::widen` 全都不带上界；`<format>` 另给 `format_to_n` 作为有界形式。
     **原先「提取侧写了长注释、插入侧沉默 ⇒ 可论证的疏漏」这个理由站不住**，
     因为两侧危险不是镜像的：`is >> ptr` 的长度来自**输入数据**（攻击者可控，
     标准 P0487R1 也是为此删的），`swrite(buf, ...)` 的长度来自 **`width()`**（调用方自己设的）。
     ⇒ **与第 542 条同判，先例张力消失。**
     已在 `char_sink_for` 中英各补一条 `@warning`，并在
     `test_io_traits.cpp` §13 补 `char_sink_for<char*, char>` 等三条 `static_assert` 作**绊线**
     ——将来谁收紧概念，断言会失败并逼他同步改注释。
     （以下为初次报告的原文，其「确认问题」定性已被上面推翻。）
554a. **（原文）插入侧默认「汇是无界的」，而提取侧为同一危险专门做过设计并写了长注释。**
     （`traits_base.h:358-363` 的 `char_sink_for`；对照面 `char_and_str.h:499-503`/`:522-528`）
     第一个合取项 `std::output_iterator<TIter, const TChar&>` **不要求容量、不要求哨位**，
     故裸指针是合法汇（实测 `char_sink_for<char*, char> == 1`），
     而**写出量是 `max(io.width(), 内容长度)`——上界只由流状态给（`max_pad_count == 65536`），
     与汇的容量无关**。按 `:22-36` 文档化的手写配方，`char buf[8]` + `width(100)` →
     ASan `stack-buffer-overflow`，**WRITE of size 98**，栈顶 `numeric.h:1100` ← `arithmetic.h:106`。
     **不对称在于**：提取侧为**同一危险**写了整段说明（为什么不提供向裸指针提取的 `sread`：
     「库无从得知缓冲区容量……可利用的缓冲区溢出」），实测提取侧确实**拒绝**裸指针（0），
     插入侧**接受**（1）且只字未提。
     方案：**只改文档**（`char_sink_for` 加一条 `@warning`，回指提取侧那段，点明是刻意的不对称）。
     **不要收紧概念**——加容量要求会把所有合法的可增长汇一并拒掉。
     - **协调者对 agent 论证的一处更正**：agent 称本条推翻了 `:305-309`/`:345-352` 那条
       「不是 UB、不越界、不崩溃」的 `@warning`——**误读**。那条警告的适用范围是
       「对**报不出**字符类型的汇（`iter_value_t` ill-formed 或为 `void`）」，
       而 `char*` 的 `iter_value_t` 就是 `char`，走第三个析取项，**不在该警告覆盖范围内**。
       警告没有被推翻，只是**对这一格保持沉默**。
     - **与第 542 条的张力（待定夺）**：第 542 条对**同一形状**（概念可被不安全的汇满足、
       std 对照组同样崩）判的是「暴露面未加大 ⇒ 调用方契约，非缺陷」。
       本条仍列为确认问题，理由是有一处实质区别：第 542 条里用户写的是**本身就坏掉的**
       迭代器（`operator++` 使上次 `operator*` 失效），本条里 `char buf[8]` **汇完全正常**，
       越界来自汇看不见的流状态；加上提取侧为镜像危险专门做过设计，这个沉默是
       **可论证的疏漏**而非中立选择。**若采纳第 542 条的先例，把本条降级到「已知但刻意不处理」即可。**
       std 同形已实测：`std::format_to(buf, "{:100}", 42)` 同样越界（WRITE of size 32）。

555. **`tm.h` / `nullptr.h` 连续第五轮（13–17）零缺陷。** 本轮新关闭的几格：
     - **`make_parse_context` 的两次 `set_hint`（`:160`/`:162`）互不干扰**——是两个重载
       转发到**不同基类**、写互不相交的成员（`m_year/m_month/m_mday` vs
       `m_hour/m_minute/m_second`），与顺序无关且幂等；facet 侧的模 24h 归约在此处是 no-op，
       因为传入值可证在 `[0, 86400)`。
     - **879 个 locale** 扫 `tm_stream_format` 的格式串拼接：零空 `%c`、零悬空 `%`、
       零转义 `%%z`；并证明**即使够得着也无害**（融合出的 `"% "` 仍能往返，
       timeio 把落单的 `%` 当字面量、空格单独处理）。
     - **「写读之间 `imbue` 导致格式串不同源」这个假设不成立**：全库**没有 `imbue`**，
       唯一的 locale setter 持 `io_mutex`，单次插入跨不过 locale 切换。
     - TSan 5 线程 × 4000 轮共享 locale 干净（阳性对照有效）。

556. **算术一路解析失败时目标对象的状态，与 libstdc++ 逐格相同**（含 `void*` 那条）。
     此前只对 `basic_string` 与定长数组查过失败态，算术一路是**首次运行验证**：
     种子值不会被留下。同批还封闭：`parse_context_type::type` 的五种退化形状、
     浮点下溢、`decay_t` 重试、**`width_guard`-then-facet 次序在四个写侧头的六个落点完全一致**。
     另有三条简报给出的前提被探针**推翻**：按值 `swrite` vs 按 const 引用 `operator<<`
     的不对称**无可观察后果**；`is_arithmetic_v` 对不完整类型**不是 UB**
     （完整性要求在 [meta.unary.prop]，不在 [meta.unary.cat]）；`width_guard` 次序无状态错误。

557. **头文件卫生（除第 553 条外）全绿，并关掉了第 521 条留下的 lint 缺口。**
     自包含性 **10/10 静默**（gcc 15.3.1 + clang 21.1.8 × 5 头，`-Wall -Wextra -Wcomment`）；
     20 个标准库 include **零多余**——做了 **38 组合 × 2 编译器**的逐个删除实验，
     并用**阳性对照**说明「能编过 ≠ 多余」（从 `nullptr.h` 删掉 `char_and_str.h` 头文件级能编过，
     一旦实例化 `io_traits<char,nullptr_t>::swrite` 就报 `'ostream_insert' was not declared`，
     因为两阶段查找推迟了依赖调用）；第 522 条关于 `<type_traits>` 保留的判断**复核后不推翻**。
     **拼写 0 条**（67 处 hunspell 命中 + 47 处标识符命中逐个查上下文均为误报；
     `char_and_str.h:131` 的 `licence` 是作者一贯的英式拼法，与 README/PROVENANCE 一致）。
     **中英块无标签正文段落数 21/21 全等**——这正是第 521 条说「按结构标签比对查不出整段缺失」
     所要求的那个 lint，脚本 `.review/tmp/r17d_langdiff.py`，**值得做成常驻 lint**。

558. **方法论：用流级探针测「抛出时写了几个字节」是假阴性。**
     失败的流不会 flush，`device().str()` 无论如何都是空的——无论 `put` 有没有写出半条记录。
     必须用**裸计数汇**。agent B 在收尾核对时才抓到（那个探针压根没编译成功过）。
     与第 526 条、第 550 条同族：**「探针跑过」与「探针跑对」也不是一回事。**

---

## 从「未覆盖的范围」提出来的既有结论（第十七轮整理，2026-09-10）

「未覆盖的范围」一章长期是 append-only 的流水账，把**已完成的记录**与**真待办**混在一起。
本次整理把前者提成编号条目、后者留在原章节。下面三条是整理时发现**只存在于那一章、
正文一次都没有**的内容。

559. **【订正连续四轮的错误说法】`io/utilities/*_operators.h` 不是「未被系统审查」，
     而是「按轴部分覆盖」。**
     第十四至十七轮的「未覆盖」一节都写着这三个文件「仍未被系统审查」，
     **这个说法过重**。实测引用深度与已审六轮的文件同一量级：
     `ostream_operators.h` **18 处不同行号 / 32 次提及**、
     `istream_operators.h` **12 处 / 31 次**、`stream_common_operators.h` **8 处 / 14 次**
     （对照：`traits_base.h` 12 处 / 40 次，已审六轮）。

     **已覆盖的轴**（正文已有结论）：
     - 加锁与异常逃逸：第 112 条（`stream_common_operators` 8 处 `lock_guard` 全不逃逸）、
       第 136 条（`put`/`flush` 第一行取锁，无未加锁窗口）、
       第 275 条（两个哨兵互斥，`is_empty_v` 双双通过）、第 434 条（`width` 读-改-写不是竞态）
     - 特殊成员与寿命：第 133 条（`m_tie_stream` 的默认成员初始化器）、
       第 276 条（析构期间无回调派生类的路径）、第 251 条（`endl` 取到的 facet 不悬垂）
     - include 卫生：第 388 条（不含 `<utility>`，本地组字母序）
     - tie 刷新失败的归属与诊断：正文有**整整一节**

     **未覆盖的轴**（这才是真待办，见「未覆盖的范围」）：
     **提取与插入的逻辑本身**——`get`/`peek`/`read`/`ignore`/`putback`/`operator>>` 与
     `put`/`write`/`operator<<` 的边界条件，以及 `extractable_with_ctx`（`:411-425`）与
     分派体（`:1199-1260`）之间的「概念即分派」不变量。
     第十三轮在那里找到过两处破口（问题 2、3），其余合取项与分支未逐一对齐核对。
     → **下一轮换靶时按这条轴排，不要再写「未被系统审查」。**

560. **注释精简的删除/保留判据**（原属「文档精简轮」小节，2026-09-06 提交 `cc59627`，
     16 个文件净删 597 行注释）。**写新注释时按这个对齐：**
     - **可删**：标准条款推导链；层级归属论证（「为什么这个判据归 I/O 层而不归转换器层」）；
       签名已经说清的复述（「纯编译期计算、不构造对象」）；构建模式内幕；取舍长文；
       跨文件重复的同一条注记。
     - **必留**：全部 `@brief`/`@param`/`@tparam`/`@return`/`@throw`；**全部 `@warning`**
       （行为陷阱都在那儿）；陈述**可观察行为**而非实现理由的 `@note`。
     - **机械核对法**（可复用）：结构标签逐个按名比对；`@warning` 零删除；
       剥掉注释后代码逐字节相同。47 条被删的 `@note` 逐条读过，**5 条**带走了别处没有的
       事实、已补回。
     - **该轮的教训**：同一条结论在多个文件里复述时，**改一处要 grep 全库**
       （`ostream_operators.h`/`istream_operators.h` 两处漏改过）。

561. **探针写法陷阱汇总**（历轮踩过的，下一轮直接复用）：
     - 失败的 requires-expression 直接写在**非模板上下文**里，gcc 下是硬错误而非 SFINAE。
     - 读侧测 `width` 泄漏若用空输入，`width` 保持原值是**哨兵**先失败、`sread` 没被调用，
       不是泄漏；须用非空输入 + `noskipws`。
     - 测「设备写满」时 `width` 必须大于 cvt 内部缓冲，否则 `dput` 在插入过程中根本不被调用，
       异常推迟到流析构，插入看起来是成功的（假阴性）。
     - 注入抛出的 `operator new` 必须替换全套（`new/new[]/nothrow/aligned` + 对应 `delete`），
       否则 ASan 的 nothrow-new 与自家 free 会 alloc-dealloc-mismatch。
     - 用**流级**探针测「抛出时写了几个字节」是**假阴性**：失败的流不会 flush，
       `device().str()` 无论如何都是空的。必须用**裸计数汇**（第 558 条）。
     - `traits/*.h` 这种写法出现在块注释里会构成 `*/` 提前闭合。
     - `char8_t` 探针必须显式 `locale<char8_t>("C.UTF-8")`：`("C")` 会抛，
       默认构造的流在 `LC_ALL=C` 下也抛（第 549 条）。
     - 别把标准库两个量之间的关系写死（`basic_string::max_size()` 本机与 CI 公式不同，第 547 条）。

---

562. **第十一轮已实跑、结论成立的去重参照**（原属「未覆盖的范围」，均**不构成缺陷**，
     下一轮遇到同题直接引用，不必重跑）：
     - **格式串退化输入不越界**：`""`、`"%"`、`"abc%"`、`"%%"`、`"%q"`、`"%5"`、`"%E"`、`"%O"`、
       `"abc%E"`、`"%Oq"`、`"a\0b"`、`"%\xff"`、`"% "` 十三种，put/get 两侧都不越界不崩溃；
       put 侧一律按字面量原样写出且**不置失败位**，get 侧对不认识的说明符置 `strfailbit`。
     - **操纵符参数取值域都被挡住**：`setw(-1)`/`setw(PTRDIFF_MIN)` → `strfailbit` 且 `width` 不变；
       `setw(PTRDIFF_MAX)` 存得下、随后的插入由 `max_pad_count` 挡住并清零；
       `setprecision(256)`/`(-1)`/`(PTRDIFF_MIN)` → `strfailbit` 且 `precision` 保持旧值；
       `setbase(8LL + (1LL<<32))` 未被截断成 8；`setiosflags(fmtflags(0xFFFF))` 不越界
       （`fmtflags` 底层固定 `uint16_t`）；`setw(65538) << "ab"` 恰好等于 `max_pad_count`，放行。
     - **`detach()`/`attach()` 与方向切换的组合无悬垂**；变长编码下未经替换的 `peek()`
       在切向失败后重读得同一字符，与 `streambuf.h` 的承诺一致。
     - **三种 `iostream` 组合的 `streambuf`/`locale`/`mem_device`/`file_device` 移动构造与
       移动赋值实测均为 `noexcept`**，`iostream.h` 的 `noexcept` 与 `static_assert` 前提成立。
     - **审阅后驳回、不要再查的两条**：(i) `write(buf8, 4096)` 从 8 字节缓冲越界读——调用方
       声称有 4096 个字符却只给 8 个，任何 iostreams 实现都会越界，`std::ostringstream::write`
       同样（与 `read(buf, -1)` 那条**不同**，后者在 libstdc++ 里是合法 no-op）；
       (ii) 过量压回后 `tell()` 钳位到 0、未读先 `putback()` 时 IOv2 是 `good` 而 std 是 `bad`
       ——`streambuf.h:706-711` 明文写「这是既定语义，非错误」。

---

## 第十八轮：`io/objects/*.h` 全局流对象（`fix/io-objects-lifetime-and-locks` 已并入 main：`101330d`…`2f8bb5d`；续作分支 `fix/io-objects-suspects`）

基线 `main` = `6e63fed`（2026-09-11）。三个 agent 分工：`stdin_api` / `stdout_api` 与六个输出单例 /
全局对象生命周期与 shared 模式；主控对三条最重的确认问题都亲手重跑过探针。五条确认问题全部修掉并
带常驻用例（2026-09-12），存疑 S2-S5 全部处置（第 582-585 条，2026-09-13）。探针索引见附录「第十八轮」。

### 五条确认问题的处置

563. **【已修 `101330d`】进程退出时八个流单例被真的析构 → 与仍在使用它们的线程 heap-use-after-free。**
     `~cout_t()` 的 `flush()` 在 `io_mutex()` 之下完成，随后基类在锁外析构 `m_locale`（facet map）、
     `m_streambuf`、`recursive_mutex` 本身；另一线程在两次插入之间重新拿到锁后 `locale<char>::get`
     （locale.h:985）读已释放节点。两种形态都实测：(a) 主线程 `cout <<` 循环中另一线程 `std::exit()`；
     (b) **更常见**——detached 线程打日志、`main()` 正常 `return`。TSan 每次 26-27 条报告（17 条 UAF）；
     同一探针换 `std::cout` 对照组 0 条——[iostream.objects.overview]/5 规定标准流对象不析构，libstdc++
     `ios_base::Init` 退出只 flush。同一根因让「晚于 `_cout_init` 析构的代码」（其它 TU 更早构造的静态对象的
     析构、更早登记的 `atexit`）用 `cout` 从「std 下安全」变成「IOv2 下 use-after-destroy」（`life_1c_12`）。
     `sync_with_stdio(false)` 模式下 TSan 0 报告但对象已析构、主线程又插入 21 万次——存储是静态缓冲所以
     sanitizer 看不见，**不要把 0 报告当作 nosync 模式安全的证据**。
     **处置**：`sing_temp` 增加 `exit_hook`（`void (*)(T*) noexcept` 静态成员，派生类经 `sing_temp(exit_hook)`
     构造函数登记；`~init()` 有钩子调钩子、无钩子析构——同 `unique_ptr` 的 Deleter 思路）。六个输出流登记
     `[](X_t* p) noexcept { try { p->flush(); } catch (...) {} }`，`cin_t`/`wcin_t`/`ori_facet_buf` 登记空钩子
     （后者必须一起不析构：流的 `locale::get` 退出阶段仍可能回到它构造未命中的 facet）；六个 `~X_t()` 删除。
     回归：两个 TSan 探针各 3 次 0 报告；`life_1c_12` 转绿；shared 模式消费者 ASan/UBSan 干净；
     `test_sing_temp` +2（`ExitHookReplacesDestruction` / `ExitHookSurvivesAThrowingFlush`）。

564. **「不析构」为什么不是泄漏（第 563 条的前提，实测）。** 泄漏的判定是**不可达**而非「未 free」：对象本体在
     `sing_buf` 静态缓冲里，其成员指针（vector `data()`、`unique_ptr`、`unordered_map` 桶、`shared_ptr` 控制块）
     都在静态存储中，LSan 保守扫描 .data/.bss 把它们当根、一路标记可达；Valgrind 归为 still reachable。
     实测：(1) `std::cout` 在 `sync_with_stdio(false)` + `imbue` + `iword` + `register_callback` 之后拥有一堆堆块、
     从不析构，ASan/LSan exit=0；(2) 用 IOv2 自己的 `ostream<std_device<1>, char/wchar_t>` placement-new 进静态
     缓冲、做格式化输出后不析构，LSan 0 报告；对照组同样对象 `new` 在堆上、退出前把唯一指针置 null：
     `29227 byte(s) leaked in 60 allocation(s)`。`locale_t` 不 `freelocale` 也不被任何检测器报。
     前提只有两条：对象本体在静态存储；`instance()::p` 与本体退出时不清空——`exit_hook` 路径正是如此。

565. **lambda 不能做 `sing_temp` 的类型参数（第 563 条方案取舍的实测依据）。** `class cout_t : public
     sing_temp<cout_t, P>` 与 `friend sing_temp<cout_t, P>;` 两处的 `P` 必须是同一类型（`init()` 调私有构造函数
     要靠这个 friend）。每个 lambda **表达式**都是新闭包类型，两处各写一遍就 `P1 ≠ P2`；别名模板
     `template<class T> using S = sing_temp<T, decltype([]{…})>` 在 gcc 15 上两处展开同型、**clang 21 上不同型**
     （诊断写成 `probe_t::(lambda at …)`，报 `calling a private constructor`）。运行期函数指针没有这个问题，
     且无捕获 `noexcept` lambda 可直接转换（两家 `-pedantic` 通过）。`std::function` 被否：非平凡析构的静态量
     会引入新的 atexit 顺序边，且作为类模板的 inline 静态在 .so/exe 各一份。三种形态的对比：类型参数 9 处 /
     定义点传参 18 处（头 + .cpp）/ 构造函数登记 9 处；遗漏时三者都是静默析构，只有不一致能被编译期抓住。

566. **【已修 `53b2f66`】`stdin_api` / `stdout_api` 自写的 `sync_with_stdio` / `reset()` / `code()` /
     `switch_code()` 全部绕过 `io_mutex()`。** 锁契约在 `stream_common_operators.h:89-91`（除 `device`/`detach`/
     `attach` 外均持锁），那里的 `attach()`/`adjust()`/`retrieve()` 包装都 `lock_guard`；两个 api 类把
     `detach`/`attach` `= delete` 后自己写的替代品一个都不取锁——`io_mutex()` 是在它们写成之后才引入的。
     `wcout.switch_code` 与并发 `<<`：ASan 3/3 次 SEGV 于 `__wcrtomb_internal`（`freelocale` 后的 `locale_t`
     被 `uselocale`）；输入侧 TSan 报 `runtime_cvt::m_ptr`、`m_read_buf` 长度、`std_device::m_c`、`operator delete`。
     **处置**：七处函数首行 `std::lock_guard guard(this->io_mutex());`（递归锁，`switch_code`→`code()`、
     `reset`→`clear()/exceptions()` 重入无碍）。常驻用例 `test/concur/test_std_objects_sync.cpp`（三个用例，
     进 gcc-tsan job）：反向验证去锁 48 条 TSan 报告、加锁 0 条。三个探针（`in_03` mode 0/1、`in_04` mode 0/1、
     `out_04`）全部 0 报告，`in_03` mode 9 裸 `int` 正控仍报 1 条。

567. **【已修 `a31e690`】`stdin_api::sync_with_stdio` 先翻标志再 detach 再重建；重建抛出后 streambuf 停在
     detached，同值重试因早退是 no-op。** `in_02`（替换 `operator new` 让第 N 次分配失败，只开 UBSan）实测：
     `fail_at=2` 后 `>>` 置 `cvtfailbit|strfailbit`（非 UB），`sync_with_stdio(false)` 再调无效，只有反向调用才修好。
     附带：`:62` 的 `if (err) rethrow` 对 `std_device<0>` 是死路径——`root_cvt::detach` 对既不可定位也不可写的
     设备既不 seek 也不 flush，`streambuf::detach` 自己的 `tell()` 抛出被吞，`err` 恒空。
     **处置**：定为**不会失败**——`noexcept`，重建放进 `try`，`catch (...) { std::abort(); }`，`err` 非空同样
     `abort()`，标志成功后再翻；`stdout_api::sync_with_stdio`（原子交换）与全局 `IOv2::sync_with_stdio` 同标
     `noexcept`（要么八个全切、要么进程终止，原存疑 S1 随之关闭）。理由：能抛的只有 2 KB 分配失败与运行期
     locale 数据库消失，都是环境已坏；`sing_temp::init` 对同一批对象构造失败已是 `abort`。回归：`in_02`
     `fail_at=2` → `exit=134`（SIGABRT）。三处补了 Doxygen（含「须在任何 stdin 读取之前调用」）。

568. **标准库的 `sync_with_stdio` 抛出行为（第 567 条的去重参照）。** 标准 [ios.members.static] 无 Throws 子句、
     非 `noexcept`；「调用前已有 I/O 则效果实现定义」。**libstdc++**（`src/c++98/ios_init.cc`）：只处理
     true→false 且只此一次（之后 `sync_with_stdio(true)` 返回 false、什么都不做）；顺序是先翻
     `_S_synced_with_stdio`、再显式析构六个 `stdio_sync_filebuf`、再 `new (&buf_cout) stdio_filebuf`
     （内部 `new char[BUFSIZ]`，可抛 `bad_alloc`），抛出后 `cout.rdbuf()` 指着已析构存储——悬垂 UB，同值重试同样
     no-op；即事实上致命、未声明。**libc++**：`static bool previous_state` 纯翻转，`cout` 底下是无缓冲的
     `__stdoutbuf`（逐次 `fwrite` 进 stdio），永远同步，`sync_with_stdio(false)` 零效果（竞赛圈常识的根源）；
     标志只为 *Returns* 正确。**MSVC** 同类（`_Sync` 静态标志，把握略低于 libc++）。标准只说「allows」不说
     「requires」，所以后两家合规。

569. **【已修 `2f8bb5d`】转换器 tainted 时 `switch_code()` 抛出但编码已被切换。** 根因 `cvt/code_cvt_stdio.h::adjust`
     只检查 `is_init_state()`，提交 `m_cvt_kernel`/`m_code` 后才 `return BT::adjust(acc)` → `abs_cvt::adjust` 首行
     `assert_not_tainted()` 抛——注释里的强保证被破坏；且 `switch_code` 裸抛不经 `handle_exception`，状态位
     不记录。现实顺序：用户看到 `cvtfailbit` 猜编码不对于是换编码。**处置**：`code_cvt_switch` 分支首行
     `this->assert_not_tainted()`（abs_cvt.h:876-878 明许），`@throws` 补 tainted 情形并写明两种拒绝都不切换。
     常驻用例 `test_code_cvt_stdio_char.cpp::ASwitchRefusedByTaintLeavesTheEncodingAlone`；反向验证去掉该行用例
     失败（`Which is: "C"`）。

570. **【已修 `a31e690`】`out_impl.h` `m_sync_with_stdio` 的注释「随析构与 stdio 同步刷新」说反。** 唯一读点是
     `out_sentry<T,false,true>`（ostream_operators.h:134-135、:197）：为 true 时**每次插入结束**把 IOv2 缓冲推进
     stdio 缓冲；六个 `~X_t()`（现已删除）无条件 flush、不读它。`out_02` 实测 `false` 模式退出照样刷出。已改正。

### 已知 / 用户责任（与 libstdc++ 有行为差异，记录备查）

571. **不包含 `objects.h` 的 TU 里的静态对象经函数间接用 `cout`，构造期为空引用，取决于链接顺序。**
     [basic.start.dynamic]/3.1（第二十四轮订正：原写 /3.2）不成立（`_cout_init` 未在该 TU 定义）→ indeterminately sequenced = `.o` 顺序。
     TU1 在前：UBSan `ostream_operators.h:894: member access within null pointer of type 'struct cout_t'`；
     TU2 在前干净。libstdc++（GCC 13+）两种顺序都好：`std::cout` 在 `libstdc++.so` 自身的 init_priority
     构造器里初始化、先于可执行文件全部初始化器。标准不保证；
     **析构侧那一半已随第 563 条消失**（`life_1c_12` 现在打印正常），只剩构造侧（无解，shared 模式靠 DSO 依赖序
     已解决）。
     **【第二十二轮订正】**原文写的「`sing_temp.h` 的 `@warning` 已写明」**不成立**：`sing_temp.h:180-184`/`:195-200`
     那两条讲的是 init 必须 inline、派生类静态成员先于 init，不面向用户，也没说哪些静态对象可以用 `cout`。
     同轮还测出第三种形态：**同一 TU 里定义在 `#include <objects.h>` 之前**的静态对象（经后面定义的函数用 `cout`）
     在 gcc 15 上同样是空引用 SEGV，clang 21 正常（clang 把 inline 变量的初始化器各放进独立 comdat `.init_array`
     条目并排在 TU 的 `_GLOBAL__sub_I` 之前）——两家都合规。三种形态的可用范围已按
     [iostream.objects.overview]/3 的口径写进 `objects.h` 的 `@note`。

572. **忘记 `-DIOV2_SHARED` 却链接了 libiov2.so：能链接能跑、ASan/UBSan 无报告、输出完整，但两套单例。**
     `LD_DEBUG=bindings` 显示 .so 内对 `_ZN4IOv24coutE` 等的引用被插桩到 exe 的 `u`（STB_GNU_UNIQUE）符号：
     .so 初始化写进 exe 的引用槽、随后 exe 的 inline `_cout_init` 重新构造一套覆盖。两份 `ori_facet_buf` 缓存、
     两张各自自洽的 tie 图、退出 `fflush(stdout)` 各一遍（幂等）。ODR 违规但 ASan ODR 检查不报。唯一防线是
     `make install-shared` 翻转安装副本的 `iov2_export.h:36`（CI `ci.yml:323` 有 grep 断言）；用仓库内头配 .so
     的用户无提示。属配置错误，不列问题。

### 验证后确认无问题

573. **header-only 模式同 TU 内 8 个流 + `s_ori_facet_buf` 的 partially-ordered 初始化顺序成立。**
     [basic.start.dynamic]/3.1（第二十四轮订正：原写 /3.2）要求 V 在每个定义 W 的 TU 里先于 W 定义：`ori_facet_buf.h:566-567`（经
     `out_impl.h` → `locale.h:32` 包含）在 `_cout_init` 之前；`_cout_init` 先于 `_cerr_init`（`tie(&cout)`）；
     `_wcout_init` 先于 `_wcerr_init`；`in_impl.h` 包含 `out_impl.h`，故 `_cin_init`/`_wcin_init` 都在其后。
     多 TU 探针（TU1 含 objects.h、其 ordered-init 静态对象在构造与析构里都用 `cout`/`cin.tie()`）两种链接
     顺序 ASan/UBSan 干净、`cin.tie()==&cout`、用户静态析构时 `cout`/`cin` 仍存活。

574. **`sing_temp` 三处静态量的性质**：`instance()::p` 与 `storage()::sing_buf` 都是常量初始化（`nm` 无 `_ZGV`
     guard、无 atexit），`std::array<byte>` 平凡析构——「`sing_buf` 比 `_cout_init` 晚初始化从而先析构」不成立；
     17 个 inline `_x_init` 都有 guard，确是动态初始化、未被提前为静态初始化、全程序只跑一次；
     `tie_graph_mutex()::m` 同样常量初始化。当前 9 个单例的构造函数都不访问自己的 `ptr()`。

575. **析构顺序（gdb 断点）**：`~_wcin_init → ~_cin_init → ~wcerr_t → … → ~cout_t → ~_ori_facet_buf_init`，
     构造完成的逆序；`cin`/`cerr` 的 `m_tie_stream` 在各自析构期间指向存活的 `cout`，且 `~cerr_t` 只调 `flush()`
     （不经 sentry、不碰 tie）。（第 563 条修后这些析构已不再执行，结论保留作历史。）

576. **fork / `exit` / `_exit` / `quick_exit` 12 种组合与 `std::cout` 逐字节一致**：`std_device::dput` 是
     `fwrite(…, stdout)`，stdio 缓冲 fork 时复制，`exit` 下双写 `A A B` 两个实现相同；`_exit`/`quick_exit` 不 flush
     也相同。

577. **shared 模式**：头 `extern IOV2_API X_t& x` ↔ `.cpp` 定义 8 个流 + `s_ori_facet_buf` + `tie_graph_mutex()`
     逐个同名同类型；`nm -DC` 恰好这 9 个 `B` + 1 个 `T` 是 IOv2 导出的非模板符号；`IOV2_API` 非 Windows 下只随
     `IOV2_SHARED` 展开为 `visibility("default")`；`.cpp` 里的 `static X_t::init` 与 sing_temp.h「必须 inline」的
     `@warning` 不矛盾（警告针对头文件里的 static；shared 模式头不定义任何 init）。正常消费者静态对象构造与
     析构里都用 `cout`：输出完整，exe 的 fini 先于 .so 的 fini。

578. **`IOv2::sync_with_stdio()` 的顺序与 tie 无依赖**：tie 指针只在两个 sentry 里读；两侧的 `sync_with_stdio`
     都不构造 sentry、不读 `m_tie_stream`。`objects.h:6-8` 的三个 include 都已被 in_impl/out_impl 自行包含，
     缺失无编译错误、无 ODR 风险（第 53、224 条），纯卫生不报。`init()` 的 `catch(...){abort();}`：
     `cerr_t()`/`wcerr_t()` 的 `tie(&cout)` 默认掩码下不抛；宽流的 creator 名字来自 `resolve_locale` 校验后的值
     （第 298 条），`LC_ALL=C`/`POSIX`/`zh_CN.GBK`/`xx_YY.FOO`/`LANG=xx_YY`/`LC_ALL=`/`LC_CTYPE=ja_JP.eucJP`
     七种环境启动不 abort。

579. **`stdin_api` 逐项**：(1) `sync_with_stdio` 的 wchar_t 分支在 `detach()` 之后调 `code()`→`retrieve` 合法
     （`runtime_cvt::retrieve` 只查 `m_ptr` 非空，`detach()` 不清它；`code_cvt_stdio::retrieve` 只拷 `m_code`）；
     (2) 被 detach 出来的 `std_device<0>` 局部析构无副作用（析构只对 STDOUT/STDERR `dflush`）；(3) detached
     streambuf 上 `>>` 不是 UB（`root_cvt::get` 抛 `cvt_error` → 置位）；(4) `reset()` 在未 detach 的 streambuf 上
     `attach()`：`root_cvt::attach` 先 `detach()` 旧设备（静默析构）再装 `std_device<0>{}`（同 fd 0，丢 `m_c` 与
     `m_buffer`），kernel 类型不变——`sync_with_stdio(false)` 后 `reset()` 仍是缓冲模式（`seq 1 100` 读一个 int
     后管道剩余 0 vs 默认模式 290 字节）；(5) `switch_code("FOO")` 强保证（`newlocale` 失败在提交前，rdstate 与
     `code()` 不变），且直接抛不走 `handle_exception`——设计选择；(6) `switch_code` 时已缓冲原始字节按新编码
     重解释、已解码进 `m_read_buf` 的 wchar_t 不变（管道不可定位，唯一不丢数据的选择；测试
     `TheInputEncodingCanBeSwitchedMidStream` 依赖）；(7) `wcin_t` 静态初始化期造 creator：`initial_locale_name`
     → `s_ori_facet_buf.locale_name()` 只读已构造成员，六种环境编码解析符合 POSIX（`LC_ALL` 优先且不回落 `LANG`）；
     (8) `tie(&cout)` 在静态初始化期安全（`tie_graph_mutex()` 函数局部 static；输入流不派生 `abs_flusher`，
     `check_tie` 不做 `dynamic_cast`）。

580. **`stdout_api` 逐项**：(1) 析构链三层刷新（`flush()` → `~root_cvt` → `~std_device::dflush`）在 `>/dev/full`
     （含掩码全开）、缓冲有数据后 `close(1)`、`setstate(strfailbit)` 后退出三种情况 rc=0 不逃逸；failbit 时缓冲
     仍经 `~root_cvt` 一层 `dput` 到达（ostream_operators.h:676-684 `@warning` 的契约，非静默丢数据）；glibc 在
     `fflush` 失败后丢弃缓冲（`out_01b_glibc_retain.c`），三层各自 `dflush()` 不送两次。**【第十九轮订正】**这段是
     修前的析构链；第 563 条修后对象不析构、退出只走钩子（现为 `try_flush()`），它经 `flush()` 受状态位约束——
     **失败态的流退出时缓冲不再写出**，与 libstdc++（`sync_with_stdio(false)` + `setstate(failbit)`）一致（第 591 条实测）。
     「failbit 时缓冲仍到达」只对显式 `detach()`/析构成立。`std_device` 的 `fflush` 在 `exit()` 里由 stdio 自己完成。
     (2) `sync_with_stdio` = 「IOv2 层无缓冲、stdio 层照常缓冲」：`dput` 是 `fwrite`、`dflush` 是 `fflush`；
     sync=true `A1P1A2P23P3`，false `P1P2P3A1A23`，false→true 切换不丢字节。(3) `switch_code` 输出侧：
     `code_cvt::put_main` 在 `put` 时就逐字符编码成外部字节，`wcout << L"中"`（UTF-8）→ 切 GB18030 → `<< L"文"`
     得 `e4 b8 ad ce c4`，边界干净；非法名强保证。(4) `reset()`：`clear(goodbit)` 再 `exceptions(goodbit)` 不抛；
     `attach()` 内部 `detach()` 把缓冲 `dput` 给旧设备，pending 字节两半都到。(5) `cerr_t`/`clog_t` 两个
     `std_device<2>` 包同一个 `FILE* stderr`：不持 fd、不 close，析构幂等 `fflush(stderr)`；交错 `E1L1E2P1L2`，
     tie 路径 `C1E1L1C2C3`；`close(2)` 后 `cerr` 立刻 `devfailbit`，`clog` 的 `L2` 在析构里失败被吞。
     (6) 宽流三兄弟各 locale 字节正确（UTF-8 `e4 b8 ad e6 96 87`、GB18030/GBK `d6 d0 ce c4`）；`LC_ALL=C` 下
     `中文` 置 0x4；`0xD800` 三种 locale 都拒；`0x110000`/`0x7fffffff` 在 UTF-8 下 glibc 照编（code_cvt.h:82-95
     `@warning` 已声明由 C 库决定）；无越界。(7) 析构体内经 CRTP 调 `flush()` 本身安全：不构造 `out_sentry`
     （ostream_operators.h:660-664）、不碰 tie、不读 `m_sync_with_stdio`。

### 探针写法陷阱（补第 561 条）

581. - 替换 `operator new` 注入分配失败的探针不能开 ASan（与 ASan 自家的 new/delete 冲突），只开 UBSan。
     - TSan 探针**必须**带一个裸 `int` 竞争的正控（`in_03` mode 9），否则「0 报告」没有证据力——nosync 模式的
       退出竞争就是这样被识破的（第 563 条末）。
     - **提取在 `read()` 里阻塞时仍持 `io_mutex()`**，写 `concur` 用例时读线程停在空管道上会把另一线程饿死成
       死锁；喂数据的一方必须在切换线程完成之后才关管道（`pipe_iguard::feed()` 为此而加）。
     - 反向验证（去掉修复看用例是否失败）必须让 `Building … .o` 那行可见或直接看二进制 mtime：第 569 条第一次
       「去掉修复用例仍通过」是陈旧构建。
     - 本机 gcc 缺 libtsan，TSan 用 clang 手工编，直接链 `build/clang-debug/lib/libgtest.a`（未插桩的 gtest
       不产生误报）。
     - lambda 做类型参数在两家编译器上行为不同（第 565 条），别拿 gcc 通过当证据。
     - **并发用例里不要经数值格式化插入整数**：CI 的 12 个 locale 里 `fr_CA`/`de_DE` 等有千分位分组，`wcout << 1000`
       会插入 U+202F/U+00A0，GBK 一档装不下 → tainted → 下一次 `switch_code` 抛 `tainted`，与锁无关的失败
       （`fr_CA.UTF-8` 下 concur 曾因此挂掉）。要 ASCII 就用字符算术 `L'0' + i % 10`，本机跑一遍 12 个 locale 再交。

### 存疑项的处置（续作分支 `fix/io-objects-suspects`）

582. **【已处置 `0500406`，文档】`cin.sync_with_stdio(true)` 从缓冲模式切回时静默丢掉 `root_cvt` 缓冲里最多 2048
     字节，而注释只说「一个前瞻字符」。** 丢弃发生在两层：streambuf.h `detach()` 注释说的是它自己的 `m_read_buf`；
     整块缓冲丢在 `root_cvt::detach`（root_cvt.h:402-430），那里只写「需设备支持定位」，没说不支持时静默丢弃。
     技术上无法避免（管道/终端不能 unread，`sync_with_stdio` 整个换 kernel、没有搬缓冲的 API）；libstdc++ 根本
     不支持 false→true（第 568 条），标准把 I/O 之后的调用定为实现定义。**实测**：任何读取之前来回切换三次
     `11 22 33` 一个不少（三层缓冲此时都空，`bos()` 对只读设备不做 I/O）；缓冲模式读过之后切回同步才丢 ` 44\n`。
     处置：`root_cvt::detach` 注释补「不支持定位时未消费数据静默丢弃（最多 `s_buffer_length` 字节）」；
     `streambuf::detach` 加 `@warning` 指明根转换器缓冲一并丢失、换缓冲操作应在任何读取之前；函数体内注释同步。
     不做「缓冲非空时拒绝切换」：`sync_with_stdio` 已 `noexcept` 且返回值是旧状态，没有表达拒绝的通道。

583. **【已处置 `5aeed7c`】shared 模式下消费者侧 `X_t::ptr()` 恒返回 nullptr。** 根因：`instance()` 的函数内静态
     是类模板的隐式实例化，.so 以 `-fvisibility=hidden` 编译后它是 hidden 局部符号，消费者 TU 再实例化一份从未
     被写入的 `p`。**可见性方案实测**：GCC 规则是模板实例可见性 = 模板与全部实参可见性的最小值——只给 `sing_temp`
     加 `IOV2_API`、或只给 `instance()` 加，都无效（`cout_t` 是 hidden）；`sing_temp` + 9 个单例类全加才有效
     （`ptr()==&cout`），代价是导出符号 10 → 267（流类全部成员模板实例，libstdc++ 的做法），与「只导出 9 个引用
     + 1 个函数」的设计相悖。`#if !IOV2_SHARED || IOV2_EXPORTS` 守卫也被否：`sing_temp` 是通用组件，消费者在 exe 里
     自定义的单例（`test_sing_temp` 在 `gcc-installed-shared` 下）其 `ptr()` 本来正确。
     **处置**：把 `ptr()` 整个删掉，`init` 增加 `T& get() const noexcept`，18 个定义点改成 `_x_init.get()`——引用
     只能经 init 对象取得，而 init 对象存在即单例已构造，不再有任何返回 nullptr 的入口（连带消掉了 header-only
     下 `ptr()` 在 init 之前 / 析构之后返回 nullptr 的两种状态）。shared 消费者调 `cout_t::ptr()` 变成编译错误
     `'ptr' is not a member of 'IOv2::cout_t'`；`_cout_init` 是 .so 里的 static 看不见；导出符号仍 10 个。
     继承方式未改：`ptr()` 删掉后 `sing_temp` 的公开面只剩 `init`/`exit_hook`，私有继承没有再要藏的东西。
     `test_sing_temp` 改经 `lifetime.get()` 与计数观察；`common`/`io.objects`/`locale` 套件、shared 消费者探针
     （ASan/UBSan）、CI clang-tidy 规则集全过。

584. **【已处置 `79bbfa9`】S3：解码失败后 `wcin` 的 kernel 留着「半个字符」，后续字节错位；`switch_code` 被
     `is_init_state()` 拒绝。** 根因只在 `codecvt_kernel::in_helper`（code_cvt.h:373）：`mbrtowc` 返回 `-1` 时不碰
     `m_state`，而此前的 `-2` 已把 lead byte 存进去；C11 7.29.6.3.2 说 EILSEQ 之后状态未指定。**处置**：`-1` 分支
     `init_state()`（4 行含注释），坏字节仍按 get 契约被消耗，之后对齐正确、`switch_code` 可用。**基线**：libstdc++ 的
     `wcin` 两种 sync 模式都把首个坏序列变成永久 `eofbit|failbit`（伪装成 EOF，`clear()` 无效，后面的字节永远读不到）；
     `wifstream`/libc++ 同样卡死。**放弃的方案**（三轮讨论）：「跳过坏单元、返回值上报、`abs_cvt::put` 区外抛出」
     ——叠在 code_cvt 之上的转换器（`vigenere_cvt`/`hash_cvt` 只要求下层 internal 是整型，可以叠）会把下层的可恢复
     异常当作中途失败、批次被展开，而 zlib 这类叠在上面时跳过一个单元本来就解不出东西，「可恢复」只在
     code_cvt 直接接 streambuf 时有意义；「`cvt_error` 加 recoverable 标志 + 边界吸收重试」同理过重。
     用例：`test_code_cvt_mem_char_char32_t.cpp` +3（rb/no_rb 对齐、错误后可切写），去掉那行三个都失败
     （`{ U+7487, U+6CB8, U+604D }`）。
     **【第二十七轮订正】** 这里加的 `init_state()` 在有状态编码下会丢移位态，已由第 656 条换成只丢收着的字节（`ffc6b658`）。

585. **【已处置 `79bbfa9`】S4：无法编码的宽字符让 `wcout` 永久失效，只能 `reset()`。** 处置不改 taint 语义：
     `abs_cvt` 加 public `is_tainted()`；`code_cvt_stdio` 约束底层设备只能是三种 `std_device`（并 `static_assert`
     可缺省构造），`put`/`get`/`flush`/`adjust` 入口 `if (is_tainted()) recover()`，`recover()` =
     `attach(device_type{})` + `bos()` + `main_cont_beg()`——标准流的设备是固定 fd，缺省构造再 attach 就是就地重新
     初始化；`mem_device` 会被换成空设备，所以必须挡掉（现有 5 个 mem_device 用例改用 `std_device`）。已提交到根
     转换器的前缀在 attach 内部的 `detach()` 里刷出；fd 仍坏时 `attach` 抛出、taint 保持，下一次入口再试（那时
     `detach` 已丢弃未刷字节，成功）——这条行为由 `IsTaintedFollowsTheFlagAndIsClearedByAttach` 锁住。
     `reset()` 保持公开成员，注释改为正面的用途描述（在同一 fd 上从头开始、丢弃残余输入/未提交输出；不是出错后的
     必经之路，也不是测试后门）。曾考虑「友元 + 宏门控只在测试里定义」：非虚成员本不占对象布局，宏门控在
     `gcc-installed-shared` 下让 .so 与 exe 的类定义不同（IFNDR），且与 C++20 模块不兼容（导入方的宏不影响模块导出）。**基线**：libstdc++ `wcout` sync 模式把坏字符打成 `?`
     写出且状态位不置；非 sync 模式到 flush 才 `badbit`、丢掉此前缓冲的前缀、`clear()` 后卡死。为什么仍派生自
     `code_cvt`：自己的东西只有换编码/报编码名/`m_code`，转换全靠 `code_cvt`；CRTP `CurrentType` 是 `code_cvt`，
     `abs_cvt` 内部互调不经守卫，但守卫在入口已消掉 taint，与既有 `adjust`/`retrieve` 遮蔽式写法同一机制。
     用例：`test_code_cvt_stdio_char.cpp` put/flush/switch 三条恢复 + `is_tainted` 三条；`test_io_objects_wchar_t.cpp` +4
     （`wcout`/`wcin` 坏单元后 `clear()` 继续、随后可换编码）；全部反向验证失败。60 套件、12 locale、clang-tidy、.so 全过。
     **顺带发现（未处置）**：截断的多字节序列停在 EOF（`a e4 b8` 后无更多字节）被静默丢弃，只得 `a`、EOF，不报错——
     现状与修后相同，见「未覆盖的范围」§6。

---

## 第十九轮：`io/objects/*.h` 专项复查——只找第十八轮没提过的（分支 `fix/io-objects-round19`，提交 `4ba1b60`）

基线 `origin/main` = `59e1790`。三个不同类型的 agent（general-purpose / claude / Explore）分工：`in_impl.h` 语义、
`out_impl.h`+`objects.h` 语义、三文件的头文件卫生 / 空白 / EOF / 拼写；主控对三条行为问题都亲手重跑过探针。
用户按 `issues.md` 自行修改，主控复核 diff 并补两处后提交。探针索引见附录「第十九轮」。

### 三条行为问题（已修 `4ba1b60`）

586. **【已修】六个退出钩子的 `flush()` 阻塞式取 `io_mutex()`：另一线程持锁不放时 `exit()` 永不返回。** 持锁不必是死锁——
     任何 `operator<<` 都在锁内跑用户的 `io_traits::swrite`，用户代码在里面等网络 / 等一个本该由正在 `exit()` 的主线程唤醒的
     条件变量就够。glibc `exit()` 的 `_IO_cleanup` 用 `_IO_flush_all_lockp(0)`（不取 FILE 锁；**订正，第二十九轮**：只对 glibc ≤ 2.37 成立，2.38 起改为逐 FILE 取锁的 `_IO_flush_all()`，见第 685 条），libstdc++ `~Init` 也不取流锁，
     正是为此；「持 IOv2 流锁、不持 FILE 锁」这一大类只有 IOv2 挂。实测：持锁 3 s → IOv2 `exit()` 3.14 s / std 0.35 s；持锁
     600 s + `timeout 5` → IOv2 rc=124。第 563 条只提了一句「取舍」，没有实测。**处置**：六个钩子改 `p->try_flush()`
     （`out_flusher::try_flush`，`noexcept`，`try_to_lock` + 3 次 `yield`，异常经 `handle_exception<true>` 吞）——取不到锁
     说明别的线程正在写，退出时那点缓冲丢掉与 std 的竞争结果一致。常驻用例 `test/concur/test_std_objects_exit.cpp`：
     fork+exec 子进程、持锁线程 + `exit(0)`、15 s 期限；带「锁空闲时必须刷出」的对照腿，防止 `try_flush` 退化成永远放弃；
     第三条腿是第 563 条的形态 b（detached 线程持续插入时退出），clang TSan 下三种模式各 0 报告——它进 gcc-tsan job 后，
     退出期 UAF 类回归会被常驻用例抓住。第十九轮顺手：删掉 `facet/timeio_details.h` 重复的 `#include <string_view>`。

587. **【已修】`reset()` 的失败路径：异常裸抛而 `rdstate()==0`，根转换器停在 `neutral`，之后每次插入失败、`clear()` 救不回。**
     链：`streambuf::attach` = `m_cvt.attach(dev)` 然后 `init_cvt()`；`root_cvt::attach` 先 `detach()`（output 下 `flush()`），
     装好新设备后**重抛** detach 捕获的错误 → `init_cvt()` 没跑。通用 `stream_common_operators::attach()` 有 try/`handle_exception`，
     `stdout_api` 自写的 `reset()` 丢了这层。**只有三个 char 流卡死**；宽流经第 585 条的 `recover()` 自愈（`attach` 抛出时
     `abs_cvt` 已 taint）。实测（fd 1 → `/dev/full`）：`reset()` 抛 `partial write`、`good()==true`，随后状态 12、`clear()` 无效、
     第二次 `reset()` 才好。**处置**：`reset()` 先 `detach()`（noexcept、捕获错误），再 `attach()`（此时无 pending，`init_cvt`
     一定跑），两者的异常都经 `handle_exception` 落成 `devfailbit`。用例 `ResetReportsAFailedFlushOfTheDroppedBytesAndStaysUsable`。
     通用 `attach()` 保持行为，`stream_common_operators.h` 的 `@note`/`@warning` 改成写明「抛的是**前一步**（旧设备收尾），
     之后停在 neutral，只能再 `attach()`」——原注释说抛的是后一步，写错了。

588. **【已修】`switch_code("")` 被接受，`code()` 返回 `""`，`stdin_api::sync_with_stdio` 据此重建内核时随调用时刻的环境而变，
     可把「不会失败」变成 `abort()`。** POSIX `newlocale(…, "", …)` 的空名 = 按环境解析，`setlocale(LC_ALL, "")` 的直觉会让人
     这么写；`resolve_locale` 启动时把别名解析成具体名，`switch_code` 这条路径没做。实测：`switch_code("")` +
     `setenv(LC_ALL, xx_YY.NOPE)` + `sync_with_stdio(false)` → rc=134；具名编码对照 rc=0。含 NUL 的名字也被接受（内核用截断名、
     `code()` 报全名）。**处置**（比「拒绝空名」更根本）：`code_cvt_stdio` 的 `m_code` 缓存整个删掉；`code_cvt_access` 下沉到
     `code_cvt`（`retrieve_impl` 钩子），由内核自报**已解析**的 `LC_CTYPE` 名——`clocale_wrapper::name()` =
     `nl_langinfo_l(_NL_LOCALE_NAME(LC_CTYPE), loc)`（**glibc 扩展**，POSIX 无等价物，已在 Doxygen 注明）；`char8_t` 内核报
     `"(builtin)UTF-8"`（故意让 `newlocale` 拒绝的伪名）。空名、`"POSIX"` 别名、含 NUL 三类不一致一次消掉。用例：
     `ClocaleWrapper.NameReportsTheResolvedLocale`/`NameSurvivesCopyAndMove`、`CodeCvtMemChar8.RetrieveReportsTheBuiltInEncoding`、
     `IoObjectsWchar.SwitchCodeResolvesTheEmptyNameAtTheCallAndKeepsTheConcreteOne`（保存并恢复 `LC_ALL`：CI 的 12 轮循环里它是
     外部设好的）。

### 文档与卫生（已修 `4ba1b60`）

589. **`reset()` 注释两处与代码不符（第 585 条新写的）。** (1) `out_impl.h` 说「丢弃本流尚未提交的输出」——写反了，`attach` 内部
     `detach()` 先 `flush()`，`PENDING` 照样写出（第 580(4) 条早测过）；(2) 两处「回到刚构造时的样子」承诺过头：格式标志、
     `width`/`precision`/`fill`、tie、`unitbuf`、`sync_with_stdio` 模式、`switch_code` 选的编码全部保留，普通文件 stdin 不 rewind；
     注释自己举的「测试复位」场景恰恰会踩（上个用例的 `noskipws` 不会被清）。两处改成枚举式（清什么、不清什么）。
     `objects.h` 的「应在任何标准流 I/O 之前」改为只约束 stdin 读取。

590. **头文件卫生（全库 56/56 有本地包含的头都是「本地 → C++ → C」，只有这两个反着；`in_impl.h` 那两处是第十八轮加锁时引入的）。**
     缺失按「直接使用的名字须直接包含其定义头」逐名补齐（`in_impl.h` 6 个标准头 + 7 个本地头，`out_impl.h` 5 + 5）；多余的删
     （`copyable_mutex.h`、`root_cvt.h`、`runtime_cvt.h`、实现头里的 `istream.h`/`ostream.h`、`out_impl.h` 的 `char_and_str.h`）；
     `objects.h` 只留 `istream.h`/`ostream.h` 作**有意的 re-export**（`ws`/`endl`/`ends`/`flush`，`stream_common_operators.h` →
     `ostream_operators.h` 这条链不包含它们）并写了文件级 Doxygen。`char_and_str.h` 移除后 objects 系列与 `iostream.h` 的
     「按值类型自己选配 traits」`@note` 一致：单包 objects.h 时 `cout << "abc"` 与 `cout << 42` 都不编译、`cout << endl` 可以
     （实测；原先是字符串能写、整数不能写，两头不靠）。另：8 个无用前置声明、两处连续空行、`/// cout_t`、`io_device` 约束与
     `attach = delete` 形式不一致，全部修掉。`<unistd.h>` 单独成 C 组放 C++ 组之后（先例 `device/std_device.h`）。

### 存疑（未处置，见「未覆盖的范围」§6）

591. - **`wcin`/`wcout` 上「换编码」有两个公开入口、两种错误约定**：`stream_common_operators::adjust(cvt_behavior)`/`retrieve()`
       仍是公开成员（只 `= delete` 了 `detach`/`attach`）。实测 `wcin.adjust(code_cvt_switch("FOO"))` 不抛、置 `cvtfailbit`；
       `wcin.switch_code("FOO")` 抛、不置位；`wcin.adjust(code_cvt_switch("C"))` 绕过 `switch_code` 的同值早退；
       `cin.adjust(code_cvt_switch("GBK"))` 静默 no-op（`root_cvt::adjust` 是 no-op）。第 579(5) 条把裸抛定为设计选择但没写进注释；
       `code()`/`switch_code()` 与两处 `= delete` 仍无 Doxygen。
     - **`sync_with_stdio(false→true)` 后不再插入、紧跟 `printf`**：先前缓冲的字节排在 printf 之后（`BA`）；注释「每次插入结束推进」
       字面没错，目的语「保持与 printf 的交错顺序」在这个形态下不成立。
     - **第十八轮修后退出走 `flush()`，失败态的流退出时缓冲不再写出**——与 libstdc++（`sync_with_stdio(false)` + `setstate(failbit)`）
       一致；第 580(1) 条已按此订正。
     - `sync_with_stdio` 在一次提取**中途**（用户 `sread` 里）重入：递归锁放行、缓冲被丢。已在 `in_impl.h` 的 Doxygen 写成
       「不得重入」（同一提交）。
     - `reset()` 不改编码（`switch_code` 之后仍是切换后的编码）——合理行为，第 589 条改注释时已写明。

### 验证后确认无问题（摘要，细节见探针索引）

592. - **静态初始化期 `tie(&cout)` 与另一线程持有 `tie_graph_mutex()`**：按 ELF 规则（partially-ordered init、`DT_NEEDED` 拓扑序、
       `_dl_init` 期间的 `dl_load_lock` 与懒绑定 `add_dependency` 条件）逐条推理，不构造病态依赖图就不可达，可达也只是一次等待
       （持锁方临界区只做原子读 + `dynamic_cast`）。未探针。
     - **`cerr_t()` 构造体内 `tie(&cout)` 与 `setf(unitbuf)` 之间的窗口**：对象此时不可达——`instance()` 在 placement-new 返回之后
       才赋值；shared 模式下能引用 `IOv2::cerr` 的模块都对 libiov2.so 有依赖边、尚未初始化。
     - **能过 `resolve_locale` 却过不了 `codecvt_kernel` 的名字：没有**——两者都是 `newlocale(LC_ALL_MASK, name, nullptr)`；
       扫 `locale -a` 全部 890 个名字 + 15 个合成名，mismatch=0。
     - `m_streambuf(device_type{}, false)` 的 `false`（无读缓冲）与 `m_sync_with_stdio = true` 一致，`reset()` 经 `runtime_cvt::attach`
       转发不换 kernel 类型；stdin 侧裸 `bool m_sync_with_stdio` 没有锁外读点；`in_sentry` 没有 `is_std` 参数——两侧不对称是
       结构性的（读侧 sync 语义在构造时定 kernel 类型，sentry 无事可做）。
     - **fd 0/1/2 关闭或只写着启动**：构造零 I/O，首次 I/O 得 `devfailbit`（+`strfailbit`），退出 rc=0，无 abort、无 sanitizer 报告。
     - `sync_with_stdio` 的 `if (err) abort()` 对 `std_device<0>` 是死路径；`reset()` 的 `clear()`/`exceptions(goodbit)` 顺序不抛；
       `attach()` 对 stdin 实际不可能抛；`code()` 在 tainted 时不触发 `recover()`（输入侧 taint 不可达）；超长（70000 字节）编码名
       被 `newlocale` 拒绝、强保证成立。
     - **`switch_code` 在输出侧被 `is_init_state()` 拒绝：glibc 下不可达**——状态非初始只在有状态编码（ISO-2022-*）写出非 ASCII 后
       出现，glibc 官方 SUPPORTED 无此类 LC_CTYPE，本机 locale 源未装无法 `localedef`。若将来有：`code_cvt` 输出侧从不写 unshift 序列。
     - `m_sync_with_stdio.exchange`/`load()` 用 `seq_cst` 多一条 fence 不是问题（`relaxed` 已足够）；`objects.h::sync_with_stdio`
       八次调用各取各锁、并发时最坏是各流先后切换；`wcout`/`wcerr`/`wclog` 三次 `newlocale` 同名不是问题；`cerr.reset()` 不触发
       tie 刷新；`reset()` 后 `m_eof_hit` 随旧设备丢弃（Ctrl-D 之后可再读）。
     - 三文件 `@lang` 配对闭合；三处 `sync_with_stdio` Doxygen 互不矛盾；尾部空白、tab、CR 均无，以单个 `\n` 结尾。
     **【第二十七轮补测】** 「静态初始化期 `tie(&cout)`」一项已在 header-only、`IOV2_SHARED`、dlopen 三种形态实测，见第 663 条。

### 本轮方法记录

593. - 三个 agent 用了三种类型（general-purpose / claude / Explore）；Explore 是只读 agent，但能用 Bash 写结果文件与跑探针，
       做卫生/拼写这类不改代码的任务正合适。
     - 用 `grep -c 'error'` 数编译错误会漏掉中文环境的 `错误`——主控一度把「`cout << "abc"` 单包 objects.h 能编译」判成 0 错误，
       实际 1 个；数错误要 `grep -c '错误\|error:'` 或用 `LC_ALL=C` 跑编译器。
     - 反向验证时 zsh 里 `3>&1 >&-` 的重定向顺序不能反（`>&- 3>&1` 会让 shell 自己报 EBADF、程序根本没跑）。
     - 全库 include 顺序惯例的判定方法：脚本扫 62 个头，凡有本地包含的 56 个是否一致——一致才能把规则当惯例来判。

---

## 第二十轮：`io/objects/*.h` 第三次专项（分支 `fix/io-objects-round20`，提交 `ea4b0dc`）

基线 `main` = `5ab0d37`（2026-09-14；第十九轮的 `4ba1b60` 已 squash 成它）。五个 agent：`in_impl.h` 语义 /
`out_impl.h` 语义 / `objects.h` 与进程级生命周期 / 生命周期剩余项 / **独立复核**（对报出的每一条从零重写探针，
允许给出相反结论——它推翻了一条）。主控对两条确认问题都亲手重跑过探针，并单独做了一条纯 C 的归因实验。
**结果：确认问题 2 条（同一根因的两条路径，已修 `ea4b0dc`），存疑 1 条（接口取舍，已处置 `9fa886c`），其余全部实测无问题。**
用户按 `issues.md` 自行修改并顺带做掉两条 §6 待办，主控复核 diff、补一处测试守卫后提交。探针索引见附录「第二十轮」。

### 两条确认问题（已修 `ea4b0dc`）

594. **【已修】缺省缓冲的 stdout 上，`reset()` 丢掉字节却报告 `goodbit`。** `out_impl.h` `reset()` 的唯一报告通道是
     `detached.second`，只装得下 `root_cvt::detach()` → `flush()` → `std_device::dput()` = `fwrite` 的失败。stdout 非 tty 时
     glibc 缺省**全缓冲**，`fwrite` 只是把字节搬进 stdio 缓冲、必定成功，于是不置任何位；真正的 `write(2)` 发生在函数末尾
     `detached.first` 析构 → `~std_device` → `dflush()` → `fflush` 失败 → **glibc 丢弃整个缓冲** → `clearerr` → 异常被析构函数的
     `catch (...)` 吞掉。注释原话「状态位是唯一能报告这件事的通道」在缺省配置下完全静音，`ferror(stdout)` 也被清成 0，两层都查不到。
     （**订正，第二十九轮**：`07925312` 删掉了 `std_device` 的 `clearerr`，现在 `ferror(stdout)` 保持置位，见第 684 条。）
     `cerr`/`clog` 不受影响（`stderr` 默认不全缓冲）。**不是「本来就活不了」**——纯 C 归因（`r20z_glibc.c`）：不做那次 `fflush`
     则 fd 恢复后字节正常落盘（`PENDING|AFTER`），做了则 `|AFTER`；是 `reset()` 尾部那次注定失败的 `fflush` 亲手毁掉的。
     常驻用例 `ResetReportsAFailedFlushOfTheDroppedBytesAndStaysUsable` 之所以绿，是 `oguard<true>` 在 `stdio_guard.h:106`
     `setbuf(stdout, NULL)`，跑在无缓冲上。主控独立复现（`r20z_reset.cpp`，全程不碰 `setbuf`）：`reset()` 得 `0x0 good=1`、
     `flush()` 得 `0x2`，两者 `PENDING` 都丢。
     **处置**：(1) `root_cvt::attach()` 在 `detach()` 之前快照 `was_output`，旧设备曾输出则在销毁前对它 `dflush()`，失败并入
     `detach()` 本来就返回的 `exception_ptr`（first-failure-wins，装好新设备后再重抛；`if constexpr (support_put)` 守住无
     `dflush` 的设备；`mem_device` 特化不改）。(2) `stdout_api::reset()` 单独再补一次 `detached.first.dflush()`——它是先
     `streambuf::detach()` 再 `attach()`，到 `attach()` 时 root 已 neutral、设备已移出，`was_output` 恒 false，只修 (1) 覆盖不到。
     `stdin_api::reset()` 不改（输入设备无 `dflush`）。注释补「旧设备的 `dflush()` 在析构前显式调用」与「冲刷的是整个 `FILE`，
     可能含调用方 `printf` 的字节」；`device_concepts.h` 的 `@note` 同步。**方案不新增任何一次 `fflush`**（那次今天就在
     `~std_device` 里发生），所以不改变与 `printf` 的交错。回归：`r20z_reset` 修后 `reset()` 得 `0x2`；新用例
     `ResetReportsAFlushFailureFromAFullBufferedStdout`（`setvbuf(_IOFBF)` 锁住缺省缓冲这条路）；`test_root_cvt_device_flush.cpp`
     四条钉住 attach 冲刷 / bos 前不冲刷 / detach 不代冲刷 / 错误优先级。

595. **【已修】同一根因的第二条路径：tainted 的宽流上 `switch_code()` 静默丢弃已提交字节，`@throws` 漏了 `device_error`。**
     第 585 条加的 taint 自愈让 `switch_code()` 在 tainted 时先走 `code_cvt_stdio::adjust` 的 `recover()` = `attach(device_type{})`，
     内部 `detach()` 把已提交字节刷给旧设备。两种形态都与注释不符：无缓冲 fd（`cerr`/`clog` 天然如此）下 `dput` 当场失败 →
     `root_cvt::attach` 在重置缓冲指针**之后**重抛 → 抛的是 **`device_error`** 而 `@throws` 只列 `cvt_error`，且「已缓冲的字节都
     没有改变（强保证）」不成立；缺省全缓冲下 `fwrite` 成功 → `recover()` 顺利返回 → **不抛、编码真的切了、pending 随后被
     `~std_device` 那次失败的 `fflush` 丢掉、无任何报告**——`code_cvt_stdio.h:274-282` 写的「`attach()` 原样抛出」那一支根本
     不触发。主控独立复现（`r20z_switch.cpp`）：`threw=no code=C`、`PENDING` 丢。
     **处置**：数据丢失那一半随第 594 条 (1) 修好（`recover()` 的 `attach` 现在真的会把冲刷失败带出来，taint 保持、下一次入口重试，
     `recover()` 的注释从此成立）；文档那一半：`@throws` 补 `device_error`（tainted 且预先恢复时旧设备冲刷失败；编码不切换，
     待冲刷的字节可能已丢），强保证限定为「进入时未 tainted」。回归：`r20z_switch` 修后 `threw=device_error code=zh_CN.UTF-8`；
     新用例 `SwitchCodeReportsAFullBufferedStdoutFailureBeforeSwitching`。

### 顺带做掉的两条 §6 待办（同一提交）

596. **`code_cvt` 关流时写出 unshift 序列。** `codecvt_kernel<char,·>::unshift()` = `wcrtomb(to, L'\0', &m_state)` 减去终止 NUL；
     `char8_t` 内核恒 0。`code_cvt::close_stream()` 在输出态经 `state_guard` 复位状态之前把它 `put` 给下层；`detach_impl`、
     移动赋值（先终结自己的旧流）、析构（新增 `~code_cvt() noexcept`，异常吞掉）三处都调。moved-from 对象 `m_io_status` 已是
     neutral，不会在坏 locale 上调 `wcrtomb`。**两点记录**：(a) 八个标准流退出时走 `try_flush()` 不析构，有状态编码在**退出**时
     仍无 unshift（glibc 无此类 LC_CTYPE，不可达）；(b) taint 路径 `release_derived_state()` 现在会在状态仍为 output 时先试
     `unshift`+`put` 到下层——被 `detach_impl` 的 try 包住、无状态编码是 0 字节，无害但行为变了。用例：两个内核的 `unshift` 各一条。
597. **`zlib_cvt` 只 `bos()` 过的流在 detach 时被 `Z_FINISH` 成合法空流**，不再留下只有头部的截断流；用例三条
     （静态 / runtime_cvt / bos 前析构不碰设备）。

### 存疑（已处置）

598. **【已处置 `9fa886c`】`wcin`/`wcout` 上「换编码」的两套入口、两种错误约定**（第 591 条的延续）。本轮先验证了两个入口
     失败之后转换器状态**一致**（`code()` 仍报旧编码、后续解码正确），同值重建 1000 次 + 别名交替 2000 次 + `C`/`POSIX` 交替
     2000 次 ASan+LSan **无泄漏**、RSS 恒定（首轮 +64 KB 是 glibc 的 locale 文件缓存）——所以只剩接口取舍。
     调研过三个候选：(a) 补文档；(b) `switch_code` 也走状态位；(c) 把 `adjust`/`retrieve` 在两个 api 类里 `= delete`
     （依据：全库五个 `cvt_behavior`/`cvt_status` 派生里标准流的固定管线只认 `code_cvt_switch`/`code_cvt_access`，不认识的
     行为是静默 no-op，概念与库内模板都不要求这两个成员）。**用户拍板 (b) 的更彻底形态**：`code()`/`switch_code()` 改成
     `retrieve(code_cvt_access)`/`adjust(code_cvt_switch)` 的薄包装——不再自己取锁、不再直接调 `m_streambuf`，锁与
     `handle_exception` 都走通用入口，两套约定合一。后果：非法名 / 非初始状态 → `cvtfailbit`，tainted 预先恢复的冲刷失败 →
     `devfailbit`（第 595 条的 `device_error` 也统一进来），只有 `exceptions()` 掩码开了才抛；返回值仍是调用前的名字，失败时
     它就是当前名字；`code()` 失去 `const`（`handle_exception` 非 const）。`switch_code` 的「读 → 比较 → adjust」不再是一个
     临界区，但并发两次切换的终态仍是其中之一、同名并发只多重建一次，无害。
     **一个连带的必改点**：`stdin_api::sync_with_stdio` 里原来用 `code()` 取名字重建——经包装后（不可达的）失败会被掩码挡成
     状态位 + 空串，`code_cvt_stdio_creator("")` 就按环境重建、静默换了编码；改成在已持锁、已在 `try{…}catch(...){abort();}`
     里直接 `m_streambuf.retrieve(acc)`，「不会失败」契约保持精确。用例：`SwitchCodeReportsARejectedNameThroughTheStateBits`
     （`cvtfailbit`、编码不变、掩码下抛 `cvt_error`）；tainted 用例改期望 `devfailbit`；两处 `EXPECT_NO_THROW` 补 `good()`/`code()`
     断言（失败不再抛，原断言会静默通过）。`test_io_objects` 23/23、`test_concur` 的 `StdObjectsSync`/`StdObjectsExit` 4/4，
     ASan/UBSan 0 报告。第 579(5) 条「裸抛是设计选择」自此作废。

### 被复核推翻的一条

599. **sync 模式下 `cin` 的一字节前瞻不回 C stdio——现象属实，不升格。** 默认 sync 下 `IOv2::cin >> n; getchar();` 拿到的是下一行
     **首字符**（`'h'`），`std::cin` 同形态拿到分隔符（`'\n'`）；机制是每次格式化提取用 `sgetc()` 探分隔符，它经 `read(0)` 取走
     后住在本流私有的 `streambuf::m_read_buf`（`streambuf.h:259-270`），C 侧看不到。两条反证：(a) **已被写成契约**——
     `device/std_device.h:45-48`/`:62-66` 的类级 `@warning` 逐字写着「请勿在 stdin 上将本类与 C stdio 函数混合使用……可能导致
     数据丢失或意外行为」，`cin_t` 的 `device_type` 正是它；报出这条的 agent 只查了 `in_impl.h` 与 `objects.h`。(b) **字节没丢**
     ——`IOv2::cin.get()` 能拿回那个 `'\n'`，是「对 C 侧不可见」而非「少读一个字符」。其余形态（`getchar(); cin >> a`、sync=0
     块缓冲）与 `std::cin` 的 sync=0 相同；`cin >> a; wcin >> b` 在 IOv2 上**不丢字节**，libstdc++ 那边 `wcin` 直接失败。
     **「改 `std_device<0>` 走 `getc`/`ungetc`」经复核不可行**：前瞻在设备**之上**，换底层没人去 `ungetc` 它，症状照旧；
     另有 `do_get` 的 EINTR/EAGAIN+poll 无法在 `getc` 上重建、`deof()`/`dget` 的「抛出时不丢前瞻」要重做、`m_eof_hit` 从每设备
     变进程级、切换模式时 `ungetc` 的字节不随设备移动四项隐患。剩下的只是把那段 `@warning` 复制进 `stdin_api::sync_with_stdio`
     的可发现性问题。

### 验证后确认无问题（摘要，细节见探针索引）

600. - **§6 待办**：截断的多字节序列停在 EOF——libstdc++ **行为相同**（同样只得 `a`+eof、不报错，§6 原记「未测」）；残留
       `m_state` 不会拼出假字符（`clear()` 后 `m_eof_hit` 粘住走不通；`reset()` 后孤立续字节 `ad` 判非法；`sync_with_stdio`
       切换 kernel 整个重建）。`sync_with_stdio(false→true)` 后 `printf` 得 `BA`——TSan 确认只是顺序、无 UB。三个 inline 静态量
       （`s_timezone_tree`/`s_domain_dirs`/`s_top`）在 .so/exe 各一份——逐个运行验证，每份只被自己那一侧的代码触及，`s_top` 甚至
       只有 exe 一份（库内无 `xalloc()` 调用点；`m_pwords` 是 `unordered_map` 不是数组，「越界」不成立于结构）。`sing_temp<X_t>`
       三类静态在 shared 消费者侧零实例化（`nm` 为空；无 odr-use 点）。`dlopen`/`dlclose`：库只有带 dso handle 的 `__cxa_atexit`，
       无 `atexit`/`at_quick_exit`；.so 含 libstdc++ 的 STB_GNU_UNIQUE 符号所以 glibc 拒绝真卸载，这一支没跑到。
     - **第 592 条的两条「未探针」项补上实测**：静态初始化期 `tie(&cout)` 撞另 4 条线程持 `tie_graph_mutex()`——TU2 静态在构造函数
       里起线程并自旋确认已跑起来，计数器证明 `_cin_init` 构造时它们已进出 **219 轮**；clang TSan 0 报告、裸 `int` 正控每次都报；
       `tie_graph_mutex()::m` 是 constexpr 构造的 `std::mutex`，`nm` 确认**无 guard variable**，不存在 guard 半途窗口。
       `cerr_t()` 构造体内 `tie(&cout)`——`-fsanitize=vptr` 0 报告（`check_tie` 只对目标 `dynamic_cast`，对 `this` 只比指针）。
     - **⭐`std::exit()` 落在单例构造期间**：`s_exit_hook` 由基类子对象在 T 的初始化列表里写入、早于构造函数体，而 `instance()` 要等
       placement-new 返回才赋值——若此刻 `exit()`，钩子非空而指针为空。探针不碰 `include/`，自定义两个 `sing_temp` 用户单例在
       构造函数体内 `exit()`：`[basic.start.term]/1` 严格成立，`~init` **不登记、不执行**，钩子一次都没被调；exit 落在两个 init 之间
       时前一个的钩子拿到有效指针、后一个一动不动。**`sing_temp.h` 无需修改。**八个流对象的构造函数没有任何回调到用户代码的点。
     - **退出钩子在同一线程重入**（`std::exit()` 从用户 `swrite` 中途发起）：`recursive_mutex` 的 `try_lock` 对持有者成功，钩子确实
       在插入中途重入 `flush()`，但插入粒度是单字符（`ostreambuf_iterator::operator=` 直接 `sputc`，`cvt_writer` 每次 `put` 前
       `commit()`），用户代码只能在两次 `sputc` 之间运行，没有可被访问的中间状态。12 种组合 0 报告、字节数精确。第 586 条只测了
       另一线程持锁，这一半本轮补上。
     - `atexit` 回调 / 更早静态的析构里用 `cout`、`cin.tie()`（两种链接序）、detached 线程跨 `main` 返回（ASan + TSan）、
       `reset()` 期间两个同 fd `std_device` 并存（输入侧全部状态只有 `m_eof_hit`/`m_c`，输出侧无状态；`handle_exception` 两次调用
       保留**第一次**的 `exception_ptr`）、`sync_with_stdio` 的 detach → 移动赋值 → `code()` 链（`detach()` 不碰 kernel，`locale_t`
       仍活；往返 3 次数值与汉字正确）、`locale_t` 无双重释放（`switch_code` 成功时旧的在 `clocale_wrapper::operator=(&&)` 里释放，
       `recover()` 不重建 kernel）、`src/iov2_objects.cpp` 定义顺序（每个 tie 目标先于使用者）、`objects.h::sync_with_stdio` 与
       「要么全切要么终止」字面一致、`copyable_atomic` 拷贝不可达——全部 ASan/UBSan 干净。
     - **EOF 截断后 `clear()` + `switch_code()` 抛 `invalid state`——不属四类。** 对照腿：真解码失败（`61 ff 62`）→ `cvtfailbit` →
       `clear()` → `switch_code` **成功**；截断（`61 e4 b8`）→ `eofbit|strfailbit` → `clear()` → 抛。`in_impl.h:193-194`（现为 `:403-406`/`:432-436`）对它描述的
       情形字面成立，截断是没覆盖的另一种；`reset()` 是有效出路。只欠一句文档。
     **【第二十七轮订正】** 截断序列在 EOF 现报 cvtfailbit（第 654 条）；`sync_with_stdio` 重建时解码器状态随之带过去（第 655 条）。

### 本轮方法记录

601. - 三个首轮 agent 因配额中断，但都按「边做边写」把结果落在 `.review/` 里，接手的 agent 只需读结果文件续做——这条规矩这次
       真的救了一轮工作。
     - **独立复核 agent 值得常设**：它推翻了一条「确认问题」（第 599 条，漏查了设备层注释）、给另一条补了归因实验、还测出报出者
       漏掉的第二形态（第 595 条的缺省缓冲支）。复核的要求是「自己从零写探针、允许相反结论」。
     - 「注释是否已写成契约」要查到**设备层**：`cin` 的行为契约有一半写在 `std_device.h` 的类注释里，只看 `in_impl.h` 会误报。
     - 归因要做「去掉嫌疑操作对照」：第 594 条若只看「`/dev/full` 上字节丢了」会被辩解成「本来就写不出去」，纯 C 的
       「不 `fflush` 则字节活下来」一腿才把责任钉在那次 `fflush` 上。
     - 常驻用例的 fixture 会**改变被测环境**：`oguard` 的 `setbuf(stdout, NULL)` 让「缺省缓冲」这条路在整个套件里零覆盖。
       用 `setvbuf(_IOFBF)` 的用例要把缓冲放进 RAII 对象（`stdout_full_buffer`），否则 ASSERT 提前返回会让 `stdout` 指向已死的
       栈数组。
     - 主控简报里的行号要在发出前核一遍：给 agent 的 `in_impl.h` 定义点 `:462-493` 已失效（当前 390 行），`tie()` 的路径写成了
       `io/streams/`（实为 `io/utilities/`），agent 各自做了勘误。

---

## 第二十一轮：`io/objects/*.h` 的边界、隐含假设、竞态与卫生（提交 `e60b8cb` + `5394910`）

基线 `9fa886c`。三个**不同类型**的 agent，范围互不重叠：Explore（头文件卫生 / 空白 / EOF / 拼写 / 中英块一致性）、
Plan（隐含假设与边界条件）、claude（竞态与状态错误）。机械项（尾部空白、tab、CR、连续空行、EOF `\n`、include 分组与
组内字母序）主控先扫过，三文件全部干净，agent 只做它查不了的部分。**五条确认问题全部已修**；三条存疑随后也已处置（第 608 条，`bf9b5b2`）。

### 确认问题（已修）

602. **【已修 `5394910`】首字节为 NUL 的编码名被静默当成 `""`，切换到环境编码而不报错。** `std::string` 可以携带内嵌 NUL，
     而 `clocale_wrapper` 收的是 `const char*`——13 个调用点全是 `xxx(name.c_str())`，于是 `newlocale` 校验的**不是**调用方
     给的那个字符串。首字节 NUL 截断成 `""`，POSIX 语义是「按环境解析」：`wcout.switch_code(std::string("\0zh_CN.GBK", 10))`
     切到 `LC_ALL` 说了算的编码、`good()` 为真、`code()` 报环境解析名，没有任何通道告诉调用方它给的名字被扔了。
     实测（`r21z_nul`）：`LC_ALL=C` 下 `before=zh_CN.UTF-8 after=C rdstate=0x0`。这正是 `locale.h:318-320` 契约里
     「`name` 非空时**不**回退到 `"C"`，应当**显式失败**而非静默降级」所排除的形态；第 588 条只记了**中间** NUL
     （内核用截断名），没看首字节这一格。**处置**（用户拍板放在最根部）：`clocale_wrapper` 构造函数改收
     `const std::string&`，`name.find('\0') != npos` 即抛 `cvt_error`；13 处调用方去掉 `.c_str()`。`const char*` 重载随之删除，
     `nullptr` 从运行期检查变成编译错误（C++23 的 `std::string(nullptr)` 是 `= delete`）。**这是本类唯一自己做的判定**——
     名字是否合法仍 100% 由 C 库说了算（第 592 条已证 `resolve_locale` 与内核的判据一致），加这一行只是保证
     「`newlocale` 校验的就是调用方给的那个名字」。两个 `switch_code` 的 `@note` 补了这条规则。
     用例：`ClocaleWrapper.ANameWithAnEmbeddedNulIsRejected`（首/中/尾三种）替换原 `NullNameThrows`；
     `IoObjectsWchar.SwitchCodeRejectsANameWithAnEmbeddedNul`（`cvtfailbit`、编码不变、流仍可用）。
     签名改动波及所有 facet TU，60 个套件全部重编 0 错误 0 警告，10 个相关套件跑过、sanitizer 干净。

603. **【已修 `e60b8cb`】`switch_code()` 的 `@return` 让调用方「查 `fail()`」，而流上没有 `fail()`。** `9fa886c` 新写的四处
     （`in_impl.h:310`/`:332`、`out_impl.h:209`/`:233`；现为 `in_impl.h:400-402`/`:428-431`、`out_impl.h:342-344`/`:372-376`）。`ios_state` 只有 `good()`/`dev_fail()`/`cvt_fail()`/`str_fail()`/
     `other_fail()`/`eof()`/`operator bool`（`io_base.h:1046-1060`），按注释写 `wcout.fail()` 直接编译错误。
     **处置**：改成 `cvt_fail()`/`dev_fail()`，并把存疑 S-2 的前提一并写进去——`adjust()` 没有 `good()` 门槛，
     状态位已置时 `switch_code` 照常成功、位不变，所以「失败看状态位」只在**进入时 `good()`** 的前提下成立；
     另给一条不依赖状态位的判据：比较 `code()` 与目标。

604. **【已修 `e60b8cb`】`code_cvt_access` 直接使用却未直接包含其定义头。** 定义在 `cvt/code_cvt.h:815`，两个实现头只包含
     `cvt/code_cvt_stdio.h`；`g++ -H` 显示本 TU 里它实际先经 `locale.h → collate.h → collate_details.h → cvt_facilities.h`
     到达，完全靠传递包含。违反第 590 条的判据。其余逐名核对（in 14+7+1、out 13+6+1 个头）**缺失 0、多余 0**；
     `<mutex>` 在 `9fa886c` 之后仍有 `reset()`/`sync_with_stdio` 的 `std::lock_guard` 使用者，保留正确。

605. **【已修 `e60b8cb`】两处注释格式与一处过时描述。** (1) 两个 `reset()` 块的 `@endif` 与 `@lang{EN}` 之间缺 ` *` 空行
     （`in_impl.h:213`、`out_impl.h:301`；现为 `in_impl.h:293-295`、`out_impl.h:451-453`）——全库 1222 处隔行 vs 69 处不隔，三文件 15 个块里仅这 2 处，awk 复核确认。
     (2) `stdin_api::reset()` 的「形状与 `stdout_api::reset()` 保持一致」写于 `5ab0d37`，`ea4b0dc` 给输出侧加了
     `detached.first.dflush()` 一步之后不再精确，改成「除输出侧多一步旧设备 `dflush()` 外一致」。

### 验证后确认无问题（摘要，细节见探针索引）

606. - **竞态（11 条，clang TSan 全部带裸 `int` 正控）**：`9fa886c` 把 `switch_code` 拆成 `code()`→比较→`adjust()` 三段、
       中间无锁，是本轮最想打的形状——两切换线程（A/B 与 A/`""`）+ 插入线程 + 循环 `reset()`，3+3 次 **0 报告**、
       16000 字节整、全 ASCII、终态 ∈ {A,B}；输入侧同形状（含自缓冲模式）12 次 0 报告；退出钩子 `try_flush()` 恰落在
       无锁窗口（detached 线程死循环 `switch_code`，主线程 `return`）三模式 ×3 次 0 报告、rc=0，最多多刷一次。
       `adjust`/`reset`/`exceptions()` setter 都取 `io_base.h:634` 的同一把 `m_io_mutex`，`reset()` 的 `attach()`→`init_cvt()`
       之间插不进 `adjust`；`in_impl.h` 在 `9fa886c` 之后仍无锁外读 `m_streambuf`/`m_sync_with_stdio` 的路径（唯一锁外的是
       继承的 `device()`，其 `@warning` 已是契约）。常驻 `test_std_objects_{sync,exit}` 手工 clang TSan 4/4 0 报告。
     - **边界与隐含假设（13 条）**：`retrieve` 失败仍不可达（逐行到 `clocale_wrapper::name()`，只在 moved-from 上抛，
       内核从未被移出）；`code_cvt_access::code` 缺省空串（「失败返回空串」的说法成立）；`code()` 无 const 调用点。
       `stdin_api::sync_with_stdio` 在 `detach()` 之后 `retrieve`：`close_stream()`（`ea4b0dc` 新加）只复位 `mbstate_t`、
       `locale_t` 不动；`std_device<0>` 无 `dseek`/`dput`，`err` 恒空、`abort` 不可达（第 567 条仍成立），5 种状态 +
       200 次来回实测。`reset()` 的 `clear()` → `exceptions(goodbit)` 顺序在掩码全开下不抛；tainted 上 `reset()` 干净；
       tie 并发 2000 次干净。构造期 **22 种** `LC_CTYPE`/`LC_ALL`（`C.UTF-8`/`C.utf8`/`@euro`/`@valencia`/`POSIX`/路径/
       5000 字节…）全不 abort（`resolve_locale` 先探后回退 `C`）。`main` 返回、八个钩子跑过之后 detached 线程
       `IOv2::sync_with_stdio(false)` 干净。掩码下 `switch_code` 的抛出点在 `handle_exception`，`lock_guard` 已解锁、
       `code()` 仍可查。
     - **tty（pty 实测）**：Ctrl-D 之后 `clear()` 读不回来，与 glibc ≥ 2.28 的 `std::cin` 相同；`reset()` 相当于
       `clearerr`——补上了 §6「tty 没测」的 EOF 语义那一半，缓冲不共享的 tty 形态仍未测。
     - **卫生（11 项）**：英文 hunspell 过筛无真拼错、冠词正确；中文无错别字、无半角/全角混用；EN 块纯 ASCII。
       15 个 `@lang{ZH}`/`@lang{EN}` 块配对闭合、顺序 ZH→EN、**行首锚定**的标签计数两侧全等，`code()`/`switch_code()`
       无 `@throws` 残留。注释引用的 14 处符号/文件/契约全部存在且说同一件事；两文件头注释列的基类、`= delete`、
       退出钩子、tie/unitbuf、fd 分配逐条与代码相符；`objects.h` 本地头字母序、无环包含。

### 三条存疑的处置（提交 `bf9b5b2`）

608. - **【已处置】`sync_with_stdio` 没有只读 getter。** 无参调用 = `sync_with_stdio(true)`，所以「查询」只能设一次再设回去；
       输入流上那是**两次重建 streambuf**，已缓冲的输入随之丢失（实测自缓冲模式下丢 ` 2 3\n`，`rdstate=0x9`）。
       标准确实没有这个 getter（`[ios.members.static]` 只有 `static bool sync_with_stdio(bool)`，libstdc++/libc++ 也不提供），
       但两条理由压过了「照抄标准」：(1) 本库这个函数**已经不是**标准那个——它是每流一个的成员而非 `ios_base` 的 static，
       可反复切换；(2) 标准那边这个惯用法**无害**（libstdc++ 只处理一次 true→false，之后什么也不做；libc++ 纯翻标志，
       第 568 条实测），IOv2 是唯一会因此丢数据的实现。**处置**：两个 api 类各加
       `[[nodiscard]] bool synced_with_stdio() const noexcept`，并在 `sync_with_stdio` 的 Doxygen 写明「本函数不是 getter」。
       **连带把成员统一了**：`stdin_api::m_sync_with_stdio` 原是锁内的裸 `bool`（因为除自己的 setter 外无人读它——读侧的
       sync 语义在构造 streambuf 时就固化进 kernel 类型，`in_sentry` 从不查它，见第 592 条），而输出侧必须是原子量
       （setter 是不取锁的裸 `exchange`，读点 `out_sentry` 却持锁）。getter 正是那个缺失的读点，且全库查询函数
       （`flags()`/`width()`/`precision()`/`fill()`，`io_base.h:299/396/447/542`）一律是 `const noexcept` 的无锁 `load()`——
       故输入侧也改成 `copyable_atomic<bool>`，写入仍在原来的锁内。用例
       `IoObjectsChar.SyncWithStdioCanBeQueriedWithoutSwitching`（查询不改状态、不丢已缓冲输入、只切一个流）。
     - **【已处置，文档】`reset()` 的 `dflush()` 会冲掉同一 `FILE` 上兄弟流的字节。** `cout`/`wcout` 共用 `stdout`、
       `cerr`/`clog` 共用 `stderr`；`cout.reset()` 的 `fflush(stdout)` 会把 `wcout` 刚交出去、尚未落盘的字节一并冲刷，
       失败只记在 `cout` 的状态位上，`wcout` 仍 `good()`、`ferror` 也被 `clearerr` 清成 0（第二十九轮订正：`07925312` 后 `ferror` 保持置位，第 684 条）。一个 `FILE` 一份缓冲，
       **没有代码修法**（不冲刷就退回第 594 条那个洞），`std::cout`/`std::wcout` 同样如此。`@note` 已补这一句。
     - **【已处置，文档】「与 `code()` 相同时什么也不做」以解析名逐字比较。** `""`（环境解析结果恰是当前编码）与别名
       （`zh_CN.utf8` vs `zh_CN.UTF-8`、`POSIX` vs `C`）都不命中早退，会真的重建一次——结果相同，代价是一次 `newlocale`，
       第 598 条已证不泄漏。不改成「解析后再比较」：那要先 `newlocale` 才知道解析结果，早退就没有意义了。注释已写明。

### 本轮方法记录

607. - **退出形态的 TSan 正控 `for(;;) ++racy;` 会被 clang `-O1` 整个删掉**——「正控报了」是假象，正控要写成 `noinline`
       函数里的存储。
     - 输入侧并发探针只等一个切换线程就停喂管道 → 提取线程持锁阻塞在 `read()`、切换线程等锁，是第 581 条陷阱的又一形态
       （gdb 三栈可证）；喂数据方必须等**全部**取锁线程结束。
     - Plan 类型的 agent 没有 Write 工具，靠 `cat >> … <<'EOF'` 照样能「边做边写」，且它前置的整体读码让 13 条「无问题」
       每条都带调用链。三种类型（Explore/Plan/claude）分卫生/边界/竞态三条轴，互不重叠、无一条重复。
     - **「注释是否已写成契约」要查到调用链的每一层**：第 599 条（上一轮）漏在设备层，本轮第 602 条则相反——
       `locale.h:318-320` 的「非空名不得静默降级」正是判定它是缺陷的依据。
     - 范围外顺手记（未处置）：`stream_common_operators.h:463-469` `attach()` 的 `@warning` 引用 `root_cvt.h:476-486`
       已漂移（函数现从 `:484` 起）；同文件 `:463-480`、`:145-168` 的 `@note`/`@warning` 行顶格无缩进。

---

## 第二十二轮：`io/objects/*.h` 第五次专项——§6 待办清零与两条确认问题（提交 `24be767` + `119fa14`，续作 `013d3f6`…`1b7605f`）

基线 `main` = `e555147`（2026-09-16）。三个**不同类型**的 agent，范围互不重叠：general-purpose（`in_impl.h`）、
engineering-code-reviewer（`out_impl.h`）、claude（`objects.h` + 进程级生命周期 / shared / dlclose）。主控对两条确认问题都用
**自写探针**独立复现（B-1 另加 gdb 栈证明因果；C-1 复现两种形态），并亲手推翻了一条 agent 的推荐方案（第 612 条）。
**结果**：确认问题 2 条（已修并入 main）；§6 里落在本范围的 11 条待办**全部处置**（关闭 9、评估后落地 2）；
处置过程中用户又顺手收紧了三处契约（第 614-616 条）。剩余 2 条低优先级存疑留在 `issues.md`。探针索引见附录「第二十二轮」。

### 两条确认问题（已修）

609. **【已修 `24be767`】宽流 `reset()` 在 stdio 缓冲冲刷失败时丢字节却报告 `goodbit`——第 594 条的第三条路径，藏在
     `abs_cvt::detach()` 的结构化绑定临时量里。** 触发点 `out_impl.h` `reset()` 的 `m_streambuf.detach()` + `detached.first.dflush()`；
     根因 `abs_cvt.h:1137-1141` `auto [dev, inner_err] = m_kernel.detach(); … return { std::move(dev), … };`——隐藏的 pair 临时量持有
     一个 `std_device<1>`，而 `std_device<1>/<2>` 的移动构造对源对象是空操作、moved-from 仍是完整设备，临时量析构时 `~std_device`
     先做了那次注定失败的 `fflush(stdout)`：失败被吞、glibc 丢弃缓冲、`clearerr`（`07925312` 后不再 `clearerr`，第 684 条）；回到 `reset()` 的显式 `dflush()` 已无事可报。
     `cout` 的 `runtime_cvt_imp` 直接包 `root_cvt`、没有 `abs_cvt` 层，所以第 594 条修后正确报 `0x2`。实测（`> /dev/full`，ASan/UBSan
     0 报告）：`cout.reset()` `0x2` / `wcout.reset()` `0x0`，sync 两态都中；gdb 在 `out_impl.h:402`（现为 `:510`）后断 `fflush`，第一次的栈是
     `~std_device ← std::pair<std_device<1>,exception_ptr>::~pair ← abs_cvt::detach ← runtime_cvt_imp::detach ← … ← stdout_api::reset`。
     第二十轮「已全库搜过销毁点」漏掉了**隐藏临时量**这种形态；第 606 条「tainted 上 `reset()` 干净」是在可写 fd 上测的。
     **处置**：`std_device` 加 `bool m_active = true;`——「本对象是否仍持有该流的收尾冲刷责任」：移动构造 `m_active(std::exchange(other.m_active, false))`，
     移动赋值 `m_active = std::exchange(other.m_active, false)`（**纯转交、零 I/O**，与 `m_eof_hit`/`m_c` 同一规则；曾考虑 `m_active || exchange` 保住目标
     已有的责任，被否——`||` 会短路、且不符合「目标状态整体来自源」的移动语义），析构先 `if (!m_active) return;`。显式 `dflush()`（`root_cvt::attach`、
     `reset()`）是对有效对象调的，行为不变；拷贝仍 `= delete`（责任必须唯一）。对象大小：fd 1/2 仍 1 字节，fd 0 从 3 变 4。
     用例：`StdDevice.MoveHandsOverTheClosingFlush` / `MoveAssignmentHandsOverTheDutyWithoutFlushing` / `AMovedFromDeviceDoesNotConsumeTheFlushFailure`
     （设备层的直接形态）；`IoObjectsWchar.ResetReportsAFlushFailureFromAFullBufferedStdout` + `…WhenUnsynchronized`。修前 4 条失败。
     60 套件 ASan/UBSan 全过。

610. **【已修 `119fa14`，范围外】`base_ft<timeio>::s_timezone_tree`（`timeio_details.h:336`）与 `base_ft<messages>::s_domain_dirs`
     （`messages_details.h:748`）退出时被真的析构，退出阶段的使用者 heap-use-after-free——第 563 条同根因、当时漏掉的两处。**
     两者都是带非平凡析构的 inline 静态量；流对象自己已「退出不析构」，所以 `cin >> get_time(…, "%Z")`、`locale.involve_msg(...)` 在退出阶段
     能走到这两处——外层安全、内层已死。三种形态（agent 实测，主控复现 1 与 3）：(1) detached 线程循环解析 `%Z` + `main` 返回，3/3 UAF；
     (2) detached 线程 `bind_text_domain`/`get_dirname` + `main` 返回，UAF（探针陷阱：ASan 打印报告时在 `dl_iterate_phdr` 上与正在
     `__cxa_finalize` 的主线程互锁，进程挂死，要 gdb 断 `__asan_report_load8` 才拿到栈）；(3) **单线程只换链接序**：不含 IOv2 头的 TU1
     静态对象析构里调 TU2 的 `%Z` 解析，TU1 在前 UAF、在后正常；同形态换 `std::chrono::locate_zone` rc=0。
     主控排查全库：带非平凡析构、退出时会销毁的静态对象共 **5 处**——上述两处 + `messages_details.h:156` 函数局部 `def_dir` +
     `messages.h:124/263` 函数局部 `empty_res`/`empty_msgid`（后两者**按引用返回**，退出后是悬垂引用）。其余（`s_exit_hook`、`s_id`、`s_top`、
     `instance()::p`、两个 `std::mutex`）平凡析构。
     **libstdc++ 的做法（查源码 + 实测）**：`std::cout`（`ios_init.cc:123-136` `~Init` 只 flush）与 `locale::classic()`
     （`locale_init.cc:85-95/333` placement-new 进静态字节数组、facet `refs=1` 永不销毁）都是**永不析构**；只有 tzdb 会析构但尽量晚
     （`tzdb.cc:221-228` `_S_head_owner` 是 `shared_ptr` + `init_priority(98)`，且住在更晚终结的 `libstdc++.so` 里）——detached 线程形态
     3/3 干净只因它的析构到 `_exit` 只有微秒级窗口，IOv2 的 `prefix_tree` 4095 节点在 ASan 下析构要 100 ms 以上。header-only 库既不能靠
     DSO 终结序也没有跨 TU 的 `init_priority`，所以取 libstdc++ 对 classic locale 的策略。
     **处置**（用户否决了 `shared_ptr` 方案——静态 `shared_ptr` 退出时照样析构，退出后新构造的 facet 去拷贝它是 UB；也否决了只补文档）：
     统一惯用法「**静态引用绑定到 `new` 出来、永不 `delete` 的对象**」——引用本身平凡析构，对象经静态引用可达、LSan 不报（第 564 条原理），
     用法处一字不改。`s_timezone_tree` → `const prefix_tree&` 绑定 `*new prefix_tree(lambda())`（仍启动时构建），`timeio.h:3730` 的 `decltype`
     套 `std::remove_cvref_t`；`s_domain_dirs` → `unordered_map&` 绑定 `*new`；`def_dir` → `static constexpr std::string_view`；`translate("")` 直接
     `return ori;`；`head_entry()` 的 `empty_msgid` → 函数局部静态引用绑定 `*new`。`in_impl.h`/`out_impl.h` 文件级 `@note` 写明「退出阶段仍可用」
     只覆盖库自己的流/locale/facet/facet 进程级数据，经 `locale(loc)` 装入的用户 facet 若持有会析构的全局量不在此列（与 `std::cout` 相同）。
     用例 `test/concur/test_facet_statics_exit.cpp`：fork+exec 子进程，detached 线程解析 `%Z` / 操作文本域表 + `main` 返回，两条腿修前 UAF
     （30 s 超时），修后 286 ms。**链接序形态没做成常驻用例**：gcc 把每个 TU 的动态初始化放进 `_sub_I_00099_1` 按链接序执行，concur 套件里
     按字母序排在前面的 TU 先初始化了树，late-static 的检出力取决于 glob 出来的链接序，不可靠，去掉。

### §6 待办的处置（11 条全部关闭；只列有实质内容的）

611. **`sync_with_stdio(false→true)` 后不插入、紧跟 `printf` 得 `BA`（§6(a)）→ 已修 `013d3f6`，后续在 `1b7605f` 改成掩码模式。**
     机制：`m_sync_with_stdio` 的唯一读点是 `out_sentry` 构造时的快照（`ostream_operators.h:135`），析构按快照决定是否冲刷，只影响**之后开始**
     的插入；切换本身是一行 `exchange`，不做任何 I/O，积压的字节没有人负责。文档目的语「保持与 `printf` 的交错顺序」正是这个开关存在的全部意义，
     故判为逻辑 bug 而非取舍。**处置**：`false→true` 且旧值为 `false` 时先 `exchange` 再取锁 `flush()` 一次（顺序不能反，否则两步之间开始的插入
     读到 `false` 不冲刷，窗口仍在）；已处于失败态的流不冲刷（流级 `flush()` 会抛 `stream_error` 额外叠 `strfailbit`）。
     错误处置经历了三步演进：先照 `try_flush` 写成 `handle_exception<true>` + `noexcept`；用户质疑「`<true>` 到底会不会抛」引出第 614 条；
     再质疑「输入侧 `abort` 是不是特殊化」引出第 615 条，最终两侧都改成默认（按掩码）形式，`noexcept` 去掉——更贴近的先例是 `reset()`
     （用户调用、冲刷后继续、按掩码抛）而非 `try_flush`（退出钩子，没有调用栈可报告）。四处注释同步改写（`out_impl.h` 函数与成员、`objects.h`）。
     **未覆盖的一格**：翻转落在本次插入内部（用户 `swrite` 里翻）时该次插入仍按快照走，到下一次插入结束才进入 stdio——写进 `@note`；
     若要堵上得让 `~out_sentry` 现读原子量，见「未覆盖的范围」。
     用例 `IoObjectsChar.SwitchingBackToSyncPushesWhatWasAlreadyBuffered` / `SwitchingBackToSyncOnAFailedStreamAddsNoState`，修前都失败。

612. **「忘记 `-DIOV2_SHARED`」的防线（§6(d)）→ agent 推荐的运行期 `dlsym`+`dladdr` 自检被主控探针推翻，改为符号表判据 + CI 双对照（`e30d0c4`）。**
     推翻的证据：`.so` 导出的**数据**符号被 exe 引用时链接器做 **COPY 重定位**，存储搬进 exe 的 `.bss`，`.so` 自己的引用也绑到那里——于是
     `dlsym(RTLD_DEFAULT, "IOv2::cout")` 在**正确**消费者里同样解析到可执行文件，与漏宏的形态无法区分（探针 `d/` 两种消费者都报 INTERPOSED）。
     讽刺的是 COPY 重定位正是同一个 agent 在 A6 里记录的事实，两者没有交叉核对——它只做了读码推理没写探针。
     同一份 `nm -D` 输出给出了**可用的判据**：正确消费者把单例符号带成 `B` + COPY 重定位、自己不定义任何 `u _ZN4IOv2…`；漏宏的消费者自己定义
     全部 9 个（`u`，STB_GNU_UNIQUE）、零 COPY。真库上实测：正确 0 个 / 漏宏 9 个。**这是构建后一行 `nm -D` 就能查的事，不需要库里加任何代码**
     ——用户最初的直觉「是不是在 pipeline 里加一条」反而是对的落点。合法共存形态（header-only 宿主 + shared 插件）不误杀：那时宿主不链 libiov2.so，
     判据前提不成立。**处置**：`iov2_export.h` 的 `@warning` 给出诊断配方（`readelf -d prog | grep -q libiov2.so && nm -D prog | grep ' u _ZN4IOv2'`）；
     `install-paths` 作业加一步，用**仓库内**头（安装版开关已开、漏不了）编一个带宏、一个不带宏的消费者链 `build/make/libiov2.so`，断言前者 0 个、
     后者非 0 个——两条对照互为控制，配方与它依赖的性质都不会静默失效。同一 `@warning` 块写进 S2 的限定：共存要求宿主不导出 IOv2 符号
     （不加 `-rdynamic`，或 `-fvisibility=hidden`）；`-rdynamic` 宿主下 .so 的 744 处引用被插桩、初始化即崩（UBSan invalid vptr，实测）。

613. **四处纯文档（`2148032` + `e30d0c4`）**：(1) `in_impl.h` `sync_with_stdio` 的 `@warning`：「同步」= 逐字节 `read(0)`，不与 C stdio 共享缓冲，
     三种形态（分隔符对 `getchar` 不可见但 `cin.get()` 可取回 / tty 上 `getchar` 吞整行 / `cin` 与 `wcin` 各有读缓冲）都**不丢字节**（pty 实测，
     与第 599 条管道结论一致）；(2) `out_impl.h` 文件级 `@note`：「退出时」只指 `exit()`，`quick_exit`/`_exit`/信号/`abort` 一概不跑钩子，
     与 `std::cout` 逐字节相同（不 fork 的直接形态实测，四种路径都 0 字节）；(3) `objects.h` 按 [iostream.objects.overview]/3 口径写明哪些静态对象
     的构造/析构里可用这八个流（同 TU 且定义在包含之后；定义在包含**之前**的在 gcc 15 上是空引用、clang 21 正常——clang 把 inline 变量初始化器
     独立成 comdat `.init_array` 条目排在 TU 的 `_GLOBAL__sub_I` 之前；两家都合规），并**订正第 571 条**「`sing_temp.h` 已写明」（那两条不面向用户）；
     (4) S2 见第 612 条。
     其余关闭项（均实测，细节在附录探针索引）：tty 三形态、`reset()` 的 `attach()` 失败支在 fd 0 上**实证为死代码**（替换 `operator new` 全部失败 +
     LD_PRELOAD 计数 `read/poll/newlocale/freelocale`：零分配、零系统调用、零 `newlocale`；`reset()` 里唯一一次分配是 `streambuf::detach()` 内
     `tell()` 抛给自己吞的 `cvt_error`），截断序列的 `@note` 与现状逐字一致，有状态编码退出路径确认不走 `close_stream()`，第九个单例 `ori_facet_buf`
     跨 TU 顺序（两模式 × 两链接序 × ASan 干净，[basic.start.dynamic]/3.1（第二十四轮订正：原写 /3.2）覆盖 TU1 在前），shared 分支 `tie_graph_mutex()` TSan（10 线程 × 3000 轮
     0 报告，noinline 正控 1 条），`dlclose` 真卸载（`-fno-gnu-unique` 下 `RTLD_NOLOAD` 证实卸载、二次加载正常、无 UAF；**每轮 LSan 泄漏 47475 B /
     150 块**——「不析构」策略在卸载时的必然结果，已由 `iov2_objects.cpp:91-98` 契约覆盖）。

### 处置过程中顺手收紧的契约（用户提出，`7fbcff1` + `1b7605f`）

614. **`clear` / `setstate` / `handle_exception` 的 `ignore_exception_mask == true` 实例标 `noexcept(ignore_exception_mask)`。**
     逐行核过 `<true>` 分支：按掩码重抛那一整块被 `if constexpr` 编译期丢弃，`m_stream_state.store` 与四个 `exception_ptr` 赋空是 `noexcept`，
     `rethrow_exception` 在 try 内被 `catch(...)` 兜住；只剩递归锁——`recursive_mutex::lock()` 抛 `system_error` 实际只可能是递归计数溢出或 OS 资源耗尽，
     那时终止进程才是诚实的结果，而且三个调用点（`io_base.h:1241` 内部、`try_flush`、`sync_with_stdio`）本来就是 `noexcept` 或吞异常的——
     今天的行为已经是 terminate 或静默丢弃，加 `noexcept` 只是把既成事实写进类型。`io_base.h:1241` 的 `try{}catch(...){}` 随之成死代码删除。
     主控一度把「调用方已持锁所以取锁抛点不可达」说过头：`lock_guard` 放在 try 内时到 catch 已经解锁，`handle_exception` 是重新取锁；
     用户追问后改成嵌套形状，再随第 615 条整体简化。

615. **`stdin_api::sync_with_stdio` 的两个 `abort()` 去掉；两侧 `sync_with_stdio` 与自由函数都不再 `noexcept`，失败按掩码报告；自由函数抛聚合异常 `sync_error`。**
     用户指出「流有很多操作会让它不可用，都不 abort，唯独这里 abort 是特殊化」。核查：一个 `abort`（`if (err)`）**本来就不可达**（第 567/606 条：
     `std_device<0>` 无 `dseek`/`dput`，`err` 恒空）；另一个的前提「留下半换的流」不成立——赋值抛出时 `m_streambuf` 仍是旧的、已 detach 的对象，
     第 579(3) 条实测在它上面 `>>` 只是置 `cvtfailbit`，`reset()` 能救回；`sing_temp::init` 的类比偏弱（它没有调用方可报告，这里有）。
     主控先建议「保留 `noexcept`、只换掉 `abort`」，用户追问理由后自己收回：理由之一是循环论证（「它不会抛」正因为选了 `<true>`），
     之二（自由函数好写）比说的弱（默认掩码下 `<false>` 同样一个异常都不抛）；库里更贴近的先例（第 598 条 `switch_code`、`reset()`）都是按掩码。
     **自由函数**：八个流彼此独立没有先后，「只保留第一个异常」是任意选择（用户指出）；C++ 没有现成的兄弟异常汇总（`nested_exception` 是因果链，
     `exception_list` 只在 Parallelism TS），每个流保存的原始异常又是私有的、只能靠掩码重抛且会被消费——所以定义 `sync_error : io_error`，
     嵌套 `failures` 结构体八个**具名**成员（`cout_err`…`wcin_err`，用户否决了数组+下标：名字是编译期检查，下标靠文档）；收集阶段用栈上局部
     结构体、`exception_ptr` 赋值 `noexcept`（标准要求，`exception_ptr.h:116`），**零分配、真 `noexcept`**，只在有失败时才构造异常对象
     （用户问「构造 vector 抛异常怎么办」，答案是不用 vector）；`what()` 靠 `rethrow_exception` + `catch` 逐个取 `what()` 拼出失败流名与原因
     （**不消费** `exception_ptr`，`failed()` 之后照样能取到全部），只 `log(e.what())` 的调用方也能看到全貌（用户要求）。八个流一律都试，
     默认掩码下不抛、全切。文档：输入侧写明失败后流停在**未附接**状态、`clear()` 不够须 `reset()`、标志保持原值故「报告的模式」与「实际怎么读」
     一致；输出侧写明**模式已切换成功**、失败的只是交出积压字节这一步。
     用例 `IoObjectsChar.SyncWithStdioReportsEveryStreamThatFailed`：武装 `cout` 掩码 + `/dev/full`，断言抛 `sync_error`、失败表恰好只有 `cout`、
     `what()` 带流名与 `std_device` 的原因（**不**断言是 `dput` 还是 `dflush`——`oguard` 把 stdout 设成无缓冲，失败在 `fwrite` 而非 `fflush`；
     探针在全缓冲下则相反）、其余流仍切换成功、默认掩码下两个方向都不抛。输入侧的失败路径**没有常驻用例**：注入分配失败要替换全局 `operator new`，
     与 ASan 冲突（第 581 条），CI 套件都在 ASan 下跑，只有探针。（**第 696 条后输入侧切换只翻一个标志，这条失败路径已不存在。**）

### 验证后确认无问题（摘要，细节见附录探针索引）

616. - **`in_impl.h`（agent A，7 个探针，0 条问题）**：`sync_with_stdio` 中途切换的对齐——char/wchar_t 双向符合 `detach()` 契约（sync→unsync
       只丢 1 个分隔符，unsync→sync 丢 rb 缓冲），`std_device<0>::m_eof_hit`/`m_c` 随移动构造搬进新 streambuf（EOF 后切换再 `clear()` 立即 eof、
       不阻塞），宽流前瞻已解码成整字符一并丢、无半个序列残留；`m_streambuf = istreambuf(...)` 是 `unique_ptr` 换指针，第 596 条担心的 kernel
       移动赋值 unshift/put **不在这条路径上**且输入内核 `support_put` 为假。`newlocale`/`freelocale` 每次重建 +1/+1 配平；19 个 locale 名
       （别名/`@修饰`/路径名）经 `switch_code→code()→sync 切换` 往返 0 abort。`code()`/`switch_code()`：`retrieve` 失败不可达，掩码含 `cvtfailbit`
       时 `code()` 在 `code_cvt_switch` 构造前抛出、无已获取资源。构造：`initial_locale_name` 返回单例成员的 `const std::string&`，被
       `code_cvt_stdio_creator(std::string)` 拷走，无悬垂。
     - **`out_impl.h`（agent B）**：`noexcept` 标注与可抛性一致（六个钩子 lambda 转 `void(*)(X_t*) noexcept` 成立）；`detach()`/`attach()` 的
       `= delete` 四种拼法均编译失败；`reset()` 掩码全开 + `/dev/full` 不抛、连续两次、在 `neutral` 上再调（`detach()` 返回缺省构造设备，`dflush()`
       无害）；`switch_code()` 切换编码保留已编码字节（`e4 b8 ad` 后切 GB18030 再写 `ce c4`），掩码下抛出时 `adjust` 的 `lock_guard` 已解锁、taint
       保持、下一次插入自愈；翻转 `m_sync_with_stdio` 落在 sentry 构造与析构之间按快照、无 UB；`cerr`/`clog` 共用 `stderr`：`~std_device<2>` 只
       `fflush` 不 `close`。
     - **生命周期 / shared（agent C）**：静态初始化期从另一线程 `std::exit()`——插桩 `newlocale`，九个单例构造共调用 177 次，第 k 次时另一线程
       `exit(0)`，k = 1…177 **全部** rc=0、0 报告（与第 600 条推理一致）；gcc ASan 下 20–40 ms 档偶发 SEGV 在 libstdc++ 自己的
       `time_zone::_M_get_sys_info`（exit 线程正析构 libstdc++ 的 tzdb），非 IOv2。`objects.h::sync_with_stdio` 与并发 `cout << x; cin >> y`
       交错只有顺序差异，翻转 300 次 TSan 0 报告。shared：`iov2_objects.cpp:143-168` 每个 tie 目标先于使用者；三种 .so 都恰好导出 9 个 `B` + 1 个 `T`；
       消费者 `-fvisibility=hidden` 下 `extern` 声明自带 default 可见性，exe 里是 COPY 重定位（`B`，非 `u`）；exe ASan + .so plain / 两边 ASan /
       两边 TSan 都跑过。
     - **`std_device` 改动的回归**：`m_active` 方案 60 套件 ASan/UBSan 全过；`test_root_cvt_device_flush.cpp` 四条既有期望不受影响。

### 两条存疑的处置（续作分支 `test/reset-full-buffered-stderr`，`45c9ac4` + `9ffacde`）

618. - **【已处置，文档】`reset()` 的 `@note` 举的例子「宽流重建编码转换状态失败」不可达。** 两个实现头都有这句。主控探针
       `z_attach`：五个标准流（`cout`/`wcout`/`wclog`/`cin`/`wcin`）在**全部分配失败**（替换 `operator new`，只开 UBSan）且 fd 指向
       `/dev/full` 的条件下 `reset()`——0 次分配、不抛、状态位全 0、之后可用。`attach()` 装的是同一 fd 的缺省设备，`root_cvt::attach`
       的 `m_buffer.resize(2048)` 同尺寸不分配，`init_cvt()` 的 `bos()` 对 put-only/get-only 设备只置状态，`main_cont_beg()` 对 `code_cvt`
       是 `init_state()`（`mbstate_t` 复位），不重建 locale。两处 `@note` 改成写明「这一步在标准流上没有可失败的操作，`handle_exception`
       只是兜底」，保留「若真抛了须再次 `reset()`、`clear()` 不够」的恢复规则。
     - **【不是问题】`switch_code("")` 在 `code()` 失败时早退。** 到达条件是 `retrieve` 失败 = `clocale_wrapper::name()` 在 moved-from
       上抛（`clocale_wrapper.h`：`c_locale` 为空才抛）；`c_locale` 只在 `code_cvt` 被移动构造/赋值时移出（`code_cvt.h:943-961`），而标准流
       的转换器从不被移动：`sync_with_stdio` 换的是 `unique_ptr`（新建对象，旧的销毁），`reset()` 走 `attach` 不动内核，`switch_code` 是移动
       **进** `m_cvt_kernel`，`recover()` 不重建内核（第 600 条）。即便到达，行为也无害：`code()` 已置 `cvtfailbit`，返回 `""` 是当前
       名字的诚实报告，没有任何东西被切换。不改。
     - **顺手补的常驻用例**（§6 欠账之一）：`IoObjectsWchar.ResetReportsAFlushFailureFromAFullBufferedStderr`——`wclog` 在
       `setvbuf(stderr, _IOFBF)` + `/dev/full` 下 `reset()` 报 `devfailbit`（第 609 条的 fd 2 形态；`wcerr` 是 unitbuf，每次插入已当场
       `fflush`，`reset()` 时不会有积压）。`stdio_guard.h` 的 `stdout_full_buffer` 泛化为 `full_buffer<isOut>`，加 `stderr_full_buffer`
       别名。反向验证：去掉 `m_active` 守卫的头副本上 `wclog.reset()` 得 `0x0`，修后 `0x2`。

### 本轮方法记录

617. - **agent 的「推荐方案」没有探针就不能信**（第 612 条）：agent C 自己在 A6 记录了 COPY 重定位，却在 D 里推荐了一个被 COPY 重定位打穿的方案；
       主控一个 30 行探针就推翻了。以后凡是 agent 标「推荐」而非「实测」的方案，落地前主控必须自己写探针。
     - **`oguard` 会把 gtest 的失败信息吞掉**（gtest 写 stdout，`oguard` 把 stdout 重定向到文件）：用例在 `oguard` 作用域内失败时 `--output-on-failure`
       什么都看不到，要在探针里复现或临时把断言挪到作用域外。同一个 `oguard` 还把 stdout 设成**无缓冲**，`/dev/full` 上失败的是 `fwrite` 不是
       `fflush`——断言别写死哪一层的消息。
     - **`||` 会短路**：`m_active = m_active || std::exchange(other.m_active, false)` 在目标已 `true` 时右边不执行、源对象没被清成惰性——
       `MoveHandsOverTheClosingFlush` 抓到的。最后按用户意见改成纯转交，问题随之消失。
     - **zsh 不对 `$var` 分词**（第二次踩，本机记忆里已有）：循环里 `./probe $args` 把 `"c nosync"` 当一个参数，四组只跑了第一组；要用 `${=var}`
       或逐个手工传参。
     - **agent 卡死的识别**：`ps` 看到探针进程 `futex_do_wait` 3 分钟 + 结果文件 mtime 停在十几分钟前 ⇒ agent 的 Bash 被挂死的子进程拖住；
       主控 kill 掉那个进程，agent 立即恢复。这次是 ASan 报告与 `__cxa_finalize` 互锁。
     - **多 TU 下 gcc 的初始化顺序**：每个 TU 的动态初始化在 `_sub_I_00099_1`（priority 99）里、按链接序执行；glob 出来的套件里「谁先初始化」
       取决于文件名字母序——依赖它的用例检出力不可靠。单 TU 探针能复现的链接序问题，进套件后可能就消失。
     - **两家编译器对 inline 变量初始化器的安排不同**（S1 的根源）：gcc 按 TU 文本序，clang 独立成 comdat `.init_array` 条目排在前面。
       「定义在包含之前的静态对象用 `cout`」gcc 崩、clang 好，两家都合规。
     - checked.md 引用漂移（本轮核出）：§6 与第 600 条引用的 `in_impl.h:193-194` 现为 `:354-357`/`:383-387`（第二十四轮再核：`:403-406`/`:432-436`）；第 599 条的 `streambuf.h:259-270`
       现为 `:259-268` 附近（`sgetc` 在 `:283-294`；第二十四轮再核：`sgetc` `:259-270`、`sbumpc` `:290-306`、`is_eof` `:409-414`）；第 571 条已订正。

---

## 第二十三轮：`io/objects/*.h` 只找「刚才没有提到」的问题（分支 `fix/io-objects-round23`：`b0e7451`、`22eebd1`、`fe107fc`、`f0ff02c`；已 rebase 进 main 为 `5b644b88`…`7e09356a`）

基线 `main` = `3d93094`（2026-09-17）。三个**不同类型**的 agent，范围互不重叠：Explore（卫生/拼写/注释一致性）、Plan（单线程边界与隐含假设）、
claude（竞态与锁序）。机械项（尾部空白/tab/CR/连续空行/EOF/include 分组与字母序）主控先扫过，三文件全部干净（**订正，第二十九轮**：`in_impl.h` 的本地组当时已不按字母序，见第 688 条）。
**结果**：行为级确认问题 4 条——**全部出自第二十二轮新写的代码**，三条集中在 `stdout_api::sync_with_stdio` 的十几行里；卫生/文档 6 条；
存疑 2 条随后处置（第 626 条）。主控对 B-1、R-1 亲手复现，R-2、B-2 读码确认。探针索引见附录「第二十三轮」。

### 四条行为级确认问题（已修）

619. **【已修 `b0e7451`】切回同步时那次冲刷做多了一步：调的是流级 `flush()`（`m_streambuf.flush()` + `dflush()`），而「同步」的定义只是哨兵的
     `m_streambuf.flush()`（`ostream_operators.h:197`）。** `dflush()` = `fflush(stdout)` 把 stdio 缓冲整个压到 fd，里面可能只有 `printf` 的字节；
     压失败记在 `cout` 头上，glibc 还把那些字节丢掉、`ferror` 清零。主控复现（`z_b1`）：全缓冲 stdout → `/dev/full`，`cout` 自身 0 字节，
     `printf("P")` 后 `cout.sync_with_stdio(true)` → `cout.rdstate=0x2 ferror(stdout)=0`；同样场景下一次同步模式的插入根本不碰 stdio 缓冲。
     与第 594/608 条同族：在「报告落在错误的流上」的位置多做一次 `fflush`。第 611 条的注释「失败的只是把先前缓冲的字节交给 stdio 这一步」
     「不会因此多一个位」在这个形态下都不成立。**处置**：改为 `m_streambuf.flush()`（与哨兵同一操作；`good()` 门槛保留，免得 tainted 宽流经
     `code_cvt_stdio::flush` 的 `recover()` 多出一个位）。语义变化写进注释：全缓冲 stdout 上设备写失败要到 stdio 自己冲刷才暴露，与同步模式的
     插入一样。用例 `IoObjectsChar.SwitchingBackToSyncDoesNotFlushStdioForOthers`（退回 main 头反向验证失败）。

620. **【已修 `b0e7451`，与 619 同一处改动】同一次冲刷失败登记两次，两次之间锁已释放。** 流级 `flush()` 自己 `catch(...)` → `handle_exception`
     （登记 + 按掩码抛，`ostream_operators.h:746-749`），第二十二轮加的外层 `catch` 在 `lock_guard` 释放后又 `handle_exception` 一次；
     另一线程在其间 `clear()` 的结果被第二次登记盖掉（幻影 `devfailbit`）。agent 实测（`R_07`，UBSan，`/dev/full`）：1–4 轮即命中、终态 `0x2`；
     对照腿单次登记 2000 轮终态恒 `0x0`。单线程下两次登记终态相同，第二十二轮的用例抓不到。改成 `m_streambuf.flush()` 后 streambuf 层直接抛、
     不经 `handle_exception`，外层 `catch` 成为**唯一**登记点——所以 try/catch 不但要留而且必需（agent 建议的「去掉 try/catch」只对旧写法成立）。

621. **【已修 `b0e7451`】「先 `exchange` 再取锁」让第二个切换者提前返回。** T1 从 `false` 切 `true`：`exchange` 完成、等锁（被一次插入持有）；
     T2 再调 `sync_with_stdio(true)`：`exchange` 返回 `true` → 立即返回不搬运；T2 紧跟 `printf("B")` 排在 T1 尚未交出的 `P` 之前。
     实测 `BP`（clang TSan 0 报告、正控 1 报告；主控重跑 3/3）——第 611 条修掉的 `BA` 形态在两个切换者之间重现。**处置**：切到 `true` 时先取锁、
     锁内 `exchange`；哨兵的快照本来就在锁内取（`ostream_operators.h:601/:642` 先 `lock_guard`，`:604/:645` 才构造 sentry），所以任何在我们之后拿到
     锁的插入都看到 `true`，「反向」格随之关闭。切到 `false` 保持无锁无 I/O。代价：`true→true` 的空操作也取一次锁（非热路径，文档已说会等插入）。
     用例 `StdObjectsSync.ASecondSwitcherCannotReturnBeforeTheHandOver`（主线程持 `cout.io_mutex()` 冒充插入，两个线程切换，第二个的 `printf`
     必须落在 `P` 之后；两处 50 ms sleep 只为让切换者到位，正确实现在任何调度下都过；退回 main 头反向验证失败）。
     **改前逐条核过的「会不会引入新问题」**：锁内 `handle_exception` 递归再进一层、掩码抛出时 `lock_guard` 沿栈释放；`good()` 门槛挡住失败态/neutral；
     unitbuf 的 `cerr`/`wcerr` 搬运是空操作；两条既有用例在 `oguard` 无缓冲 stdout 下断言不变；同线程插入内翻转那一格行为不变。

622. **【已修 `22eebd1`】`stdin_api::sync_with_stdio` 里 `if (err) handle_exception(err)` 排在 `m_sync_with_stdio.store(sync)` 之前。** 走到这两行时
     streambuf 已按新模式重建成功；`err` 非空且掩码命中则抛出、`store` 不执行——内核新模式、标志旧值，之后同值再调早退，永远对不齐。
     `std_device<0>` 上 `err` 恒空（第 567/606 条），`cin`/`wcin` 不可达；但 `stdin_api` 是公开模板、`TDevice` 只约束到 `io_device`（`in_impl.h:88`），
     agent 用自定义可定位设备实测「kernel is SYNC, flag says unsync」。**处置**：两行对调——模式确实切换了就先落标志，旧设备收尾的错误之后再报。

### 卫生 / 文档（已修 `fe107fc`；两处随 `b0e7451` 进入）

623. - `in_impl.h` 的 `<cstdlib>`：`1b7605f`（第 615 条）删掉两个 `std::abort()` 时漏删，已无任何直接使用者——删。其余 in 16+6+1、out 14+5+1、
       objects 4+3 个 include 逐名核对缺失 0、多余 0（第二十二轮新加的 `defs.h`/`<exception>`/`<string>`/`<utility>` 各有直接使用者，
       `std::exception` 来自 `<exception>`）。**第 604 条「多余 0」的结论至此被订正一次。**
     - 两个实现头「与 `istream`/`ostream` 的差别**都**来自…」是穷举句式，写于 `5ab0d37`，`e555147` 加的 `synced_with_stdio()` 没补进去——四处补齐。
     - `objects.h` 文件头只说「八个流对象 + 自由函数 + 操纵符 re-export」，漏了本文件自己定义的唯一公开类型 `sync_error`——补。
     - `in_impl.h:145`（现 `:146`）一对 `“”` 与三文件其余 18 对 `「」` 混用（第二十二轮新写）；`:119`/`:154`（现 `:120`/`:155`）裸写 `streambuf.h`（其它文件引用都是反引号+目录前缀）；
       `:154` EN 缺 ZH `**不得**` 的强调（22 对块中唯一的 `**` 不对称）——都改。

### 验证后确认无问题（摘要，细节见探针索引）

624. - **卫生（Explore）**：22 对 ZH/EN 块配对、顺序、行首锚定标签计数、段落数、`@endif` 隔行全部一致；hunspell + 常见错拼 + 非 ASCII 全干净；
       注释引用的 `reset()`/`out_flusher::try_flush()`（`attempts=3`）/`handle_exception`/`code_cvt_stdio::adjust`/`std_device.h` `@warning`/
       `sing_temp.h`/`streambuf::detach` 吞异常/`stream_common_operators::attach` `@warning`/`io_status::neutral` 逐个存在且说同一件事；
       `[basic.start.dynamic]/3.3`、`[iostream.objects.overview]/3` 引用正确；`objects.h` `@note`「`cout<<42`/`"abc"` 不编译、`endl` 可以」
       探针 2/1/0 错误复核成立；「默认掩码下自由函数不抛任何异常」**严格成立**（`slot` 只在成员真抛时赋值，默认掩码下 `setstate<false>`
       不重抛 → `any()` 恒 false → `sync_error`/`describe` 根本不构造）。
     - **边界（Plan，7 组探针）**：`reset()` 的 `clear()` 在状态位已置 + 掩码武装下**不抛**——无参 `clear()` 即 `clear(goodbit)`，掩码检查的是
       **新**状态 `s`，`mask & 0 == 0`（第 592 条在更苛刻前提下仍成立）；`sync_error`：`current_exception()` 在处理器内不会为空，多流失败
       （`cout`/`clog`/`wcout`、两种缓冲）`what()` 格式正确、`failed()` 的指针未被 `describe` 消费，"unknown exception" 只经先前保存的用户
       非 `std::exception` 异常可达，`describe` 的 `bad_alloc` 以 `bad_alloc` 顶替 `sync_error` 传播而各流状态位完整（分配序号扫描），库内无
       `noexcept` 上下文调用自由函数；输入侧失败路径：`dev` 按值形参必被移空、失败后同值再调一次也能恢复、`retrieve` 直走 `m_streambuf`
       无静默空串；输出侧：掩码抛出点锁已全部释放、`eofbit` 放行、fresh/unitbuf/tainted+`clear()` 各 `0x0`；`initial_locale_name` 永不空串。
     - **竞态（claude，9 条 clang TSan + noinline 正控，0 报告）**：三线程交接无丢字节、「反向」格结构上不可达（快照在锁内）；掩码抛出时 5 层递归锁
       配平、抛后他线程可取锁/插入、各拿各的异常；退出钩子 `try_flush()` vs 进行中的切换冲刷 3 模式 × 3 次无丢失；输入侧阻塞在 `read()` 后的切换
       （第 581 条陷阱）与注入分配失败路径（`otherfailbit`、`reset()` 恢复、锁不泄漏；**TSan 也与替换 `operator new` 冲突**，只能 UBSan）；八流自由函数
       并发相反方向终态可混合（强制 7/8 MIXED）但文档无任何一致性承诺；**锁矩阵**：库内唯一跨流取锁点 `try_flush` 永远 `try_lock`，无阻塞式
       `io_mutex(X)→io_mutex(Y)` 边，写满 pipe 让 `flush` 真阻塞的探针下 `cerr<<`/`cin>>`/`tie()` 都完成；内存序：锁的 release/acquire 已足够，
       `seq_cst` 多余无害；常驻 `concur` 两套件在新代码上 TSan 3/3 过。
     - 只记不评：`IOv2::sync_with_stdio()` 现在可抛，静态对象构造函数 / `atexit` 回调里调用（掩码武装时）→ `terminate`；库内无此用法。

### 两条存疑的处置（同一分支，`f0ff02c`）

626. - **【已处置】`stdin_api` / `stdout_api` 的 `TDevice` 只约束到 `io_device`。** 调研：全库对两个模板的实例化只有八个流对象自己，设备全是
       `std_device<0/1/2>`；两个实现头的全部「不会失败」论证（`detach` 的 `err` 恒空、`attach()` 无可失败步骤、`reset()` 的 `dflush()`、
       `recover()` 装缺省设备）都以固定 fd 为前提，第 622 条是这个缺口已经露出的第一个后果。处置：照 `code_cvt_stdio.h:127-130` 的先例加
       `requires`——`stdin_api` 限 `std_device<STDIN_FILENO>`，`stdout_api` 限 `std_device<STDOUT_FILENO>` 或 `<STDERR_FILENO>`，无额外注释。
       `sync_with_stdio` 里 `if (err) handle_exception(err)` 随之成为可证明不可达的分支，**用户决定保留作防御**。
       用例：`test_io_objects_char.cpp` 六条 `static_assert`，用 `requires { typename stdin_api<probe, Dev, char>; }` 概念探测模板头
       （命名合法设备满足、其它 `std_device` 不满足；退回无 `requires` 的头时三条否定断言各自编译失败）——`typename` 要求只检查模板头的约束、
       不实例化类体，所以 CRTP 参数用一个不完整类型即可。
     - **【已处置，文档】`in_impl.h` `sync_with_stdio` 的 `@note`「此后每次操作都按状态位失败（`cvtfailbit`）」缺限定。** 判据：重建失败记的是
       那次异常自己的类别（分配失败是 `otherfailbit`）；之后的提取在 `clear()` **之前**被 `in_sentry` 门槛挡下（`istream_operators.h:117/:149`
       对 `!good()` 抛 `stream_error` → `strfailbit`），到不了 detached 的 streambuf；`clear()` 之后才到达它，`root_cvt::get` 抛 `cvt_error` →
       `cvtfailbit`。两处改成「`clear()` 之后是 `cvtfailbit`」。

### 本轮方法记录

625. - **新写的代码就是下一轮的靶子**：四条行为级问题全在第二十二轮的新代码里，其中三条挤在同一个函数的十几行内。上一轮的用例（单线程、`oguard`
       无缓冲 stdout）一条都抓不到——双重登记单线程终态相同、多余的 `fflush` 在无缓冲下不可见、两个切换者要并发。
     - **「同步」有精确定义就照定义做**：第 611 条把「切回同步时冲刷一次」直觉地写成流级 `flush()`，比哨兵多做的那一步 `fflush` 正是第 594/608 条
       警告过的东西。库里同名操作有多层时，先确认要的是哪一层。
     - **只改十行也要先做「会不会引入新问题」的清单**：这次逐条核了锁的递归层数、`good()` 门槛、unitbuf 流、既有用例的前提（`oguard` 无缓冲）、
       同线程翻转那一格，然后才动手。
     - `oguard` 把 stdout 设成无缓冲这一点反复影响用例设计：它让「全缓冲下才暴露」的行为在整个套件里零覆盖（第 601 条已记），本轮的
       `SwitchingBackToSyncDoesNotFlushStdioForOthers` 必须在 `oguard` 之上再套 `stdout_full_buffer`。
     - 三种 agent 类型（Explore / Plan / claude）第二次分卫生/边界/竞态三条轴，互不重叠、无一条重复，且每条都自带探针——延续第 607 条的分工。
     - checked.md 引用漂移（本轮核出）：§6 的 `ostream_operators.h:196` → `:197`（已改）；第 606 条引用的两个实现头行号已随第二十二轮改动漂移，
       当前 `in_impl.h` `sync_with_stdio` 体 `:194-227`、`reset()` `:327-339`，`out_impl.h` `sync_with_stdio` 体 `:222-243`、`reset()` `:489-508`，
       `objects.h` `sync_error` `:113-179`、自由函数 `:224-249`（`d85f304a` 后的值见第 632 条）；第 613 条 `[basic.start.dynamic]/3.2` 按 N4950 编号疑应为 `/3.1`——第二十四轮查证成立，第 571/573/613 条已订正。

---

## 第二十四轮：`io/objects/*.h` 从 §6 待办出发（`fix/io-objects-round24` 已并入 main：`6f477e03` + `53229c74`；
续作分支 `test/io-objects-round24-pinning` 已并入 main 为 `0b76cbae`）

基线 `main` = `7e09356a`（2026-09-18；第二十三轮四个提交已 rebase 进 main）。三个**不同类型**的 agent，范围互不重叠：`claude`（输入侧运行探针）、
`engineering-code-reviewer`（输出侧运行探针）、`Explore`（文档契约 / include 卫生 / 全库 `noexcept` 扫描 / checked.md 引用漂移）。主控开工前先跑了
`io.objects` 与 `concur` 两个常驻套件（clang-debug，全过），并亲手复现了两条行为级问题（`B_02` 三态、`A_02` 五态）。**本轮优先目标是 §6 的待办**：
落在本范围的 6 项全部实跑并关闭（第 630 条）；顺藤摸出两条行为级确认问题——**都不在第二十三轮改过的行里**，一条在门槛条件上，一条根因在设备层——
与一条措辞问题，三条都已修（一个提交 `d85f304a`）；七条措辞级存疑随后由主控逐条调研（读码 + 重跑探针），全部落为文档改动（第 633 条，`eb365726`），
`issues.md` 清空。所有探针 ASan+UBSan 0 报告；探针索引见附录「第二十四轮」。

### 两条行为级确认问题（已修 `d85f304a`）

627. **【已修】`out_impl.h` `sync_with_stdio(true)` 的失败态门槛（原 `:238` `if (static_cast<bool>(*this)) m_streambuf.flush();`）让「返回即已在 stdio 手里」
     （`:168-170`）的承诺失效，第 611 条修掉的 `BA` 失序在失败态流上原样重现。** 状态位非零（`eofbit` 除外）时标志翻成 `true` 但什么也不搬，本流缓冲里的
     字节要等 `clear()` 后**下一次插入**的哨兵才进 stdio：`sync(false); <<"A"; /* 任一失败位 */ sync(true); clear(); printf("P"); <<"B"` 得 **`PAB`**，
     good 流与只置 `eofbit` 的流都得 `APB`。主控复现（`D_issue1_demo`，三态，`__fpending` 逐步）：人工 `setstate(devfailbit)` 与真实失败（stdio `_IONBF`、
     fd 1→`/dev/full`、`cout.flush()`——`root_cvt::flush` `:836-844` 在 `dput` 抛出时**特意保留** `m_buf_cur` 以便重试，所以失败后缓冲里有字节是常态）
     都是 `PAB`。门槛的两个历史理由都已站不住：第 611 条的理由（流级 `flush()` 对 `!good()` 抛 `stream_error` 叠 `strfailbit`）随第 619 条改成
     `m_streambuf.flush()` 而消失；第 619 条保留门槛的理由（tainted 宽流经 `recover()` 多出一位）**门槛本来就挡不住**——taint 与 rdstate 独立
     （`abs_cvt.h:1089` 只由 `attach/detach` 清），`clear()` 之后门槛放行、`recover()` 照跑（`B_03 2/3`）。**处置**：去掉门槛，失败态也搬（当前 `:251`），
     ZH/EN 契约同步改写（`:172-179` / `:208-218`：成功即顺序正确、位不变；再失败只是把已置的那一位再报一次，掩码武装时再抛一次；tainted 宽流的 `recover()`
     会在这里而不是下一次插入时报设备失败）。**改前逐条核过的「会不会引入新问题」**（`D_issue1_retry`，修前/修后各跑）：失败态 + fd 仍在 `/dev/full`
     重试再失败 → 同一位 `0x2` 不多位；同上 + 掩码武装 → 再抛一次 `device_error`、`synced=1`（模式已切换，与 `:173` 已写的语义一致）；`wcout` tainted
     未 `clear()` + `/dev/full` → `0x4` 变 `0x6`——正是第 619 条的顾虑，但那个 `devfailbit` 不是假的（`recover()` 把本流留着的 `A` 写到 `/dev/full` 失败，
     修前同样发生、只是推迟到 `clear()` 后的下一次插入）。用例：`SwitchingBackToSyncOnAFailedStreamAddsNoState` 头注释改掉「failed state has nothing to
     flush」（断言不变：人工置位 + 好设备，重试成功不多位）；新增 `SwitchingBackToSyncOnAFailedStreamStillHandsOverItsBytes`（`stdout_full_buffer` 下断言
     `APB`，退回旧头得 `PAB` 反向验证失败）。`io.objects` 34 + `concur` 在修后全过。

628. **【已修】`device/std_device.h` `dget()`（原 `:270-283`）在 `m_c` 有前瞻字节时无条件补读 `do_get(s+1, n-1)`，设备暂时无数据就阻塞：非同步模式下
     `cin.ignore()` / `cin >> x` 比 `std::cin` 多等一个输入事件，真 pty 上敲空行 `ignore()` 不返回。** 触发链：`istream_operators.h:977-982`（`ignore(n)`
     每轮先 `is_eof()` 再 `sbumpc()`）与 `streambuf_iterator.h:342-350`（facet 的 `++beg != end` → `at_end` → `is_eof()`）→ `streambuf.h:409-414` →
     `root_cvt.h:675-692`（缓冲空时 `m_device.deof()` 读 1 字节进 `m_c`）→ 取字时 `root_cvt.h:743` / `:1806` 以 `dget(buf, 2048)` 整块请求 → 补读阻塞。
     主控复现（`A_02`：写端写 `"\n"` 后保持打开 2 s）：非同步 `ignore()` 2001 ms vs 同步 / `std::cin` / `cin.get()` 1 ms；agent `A_03`：`>> int` / `>> string`
     非同步 4004 ms vs 2002 ms；`wcin` 同（`A_08`）；真 pty（`A_09` + `pty.fork`）非同步敲空行不返回、再敲一行才返回。§6 的 `m_c` 待办本身是关闭的（字节不丢
     不乱），这是同一条链上顺藤摸出的**活性**问题。**处置分两步，第一步走错了**：先落的是「只交出前瞻字节、返回 1」的短读版——活性对了，但 `ignore(1000)` 读
     3000 字节文件的 `read()` 从 5 次变 **3001 次**（探针 1 字节 → 短读 1 字节 → 缓冲又空 → 再探针，凡「先探针再取字」的消费者都退化成逐字节 `read()`，
     `F_ign` 用 `LD_PRELOAD` 计数）。**用户提出**交出前瞻字节后 `poll(fd, 0)` 就绪才补读——既不阻塞又保住整块读：`readable_now` 做成 `dget()` 内的 lambda
     （零超时 `poll`，`POLLIN | POLLHUP`，`EINTR` 重试，出错当「没有」交给随后的阻塞 `read()` 报），`n > 1 && readable_now()` 才 `do_get(s+1, n-1)`，补读
     抛出时 `m_c` 仍保留（旧代码那句顾虑回来了；短读版把它丢了）。`F_ign` 回到 5 次 `read()` + 2 次 `poll`；`A_02`/`A_03`/`A_08`/pty 全部不再多等；设备层三形态
     （`F_dev`）：对端已发 `"abc\n"` → 一次到手 3；只发 `"a"` → 1，下次 2；发 `"a"` 后挂断 → 1，EOF 紧随。`deof()` / `dget()` ZH/EN 注释改成「首先返回该字节，
     连同此刻已就绪的数据，但不为凑数等待」。用例：`AProbedByteStaysFirstInALongerRead`（文件上一次得 `"abc"`，钉「就绪即整块」，短读版曾把它改掉、现恢复）、
     新增 `AProbedByteDoesNotWaitForMoreInput`（管道只有 `"a"`，写端 300 ms 后才写 `"bc"`，`dget(buf,3)` 必须得 1——断言返回值不断言时间；退回旧头得 3 随后
     卡在 `read()`）、`AProbedByteBeforeAHangupComesBackAndEofFollows`（对端挂断）；`test_io_objects_char.cpp` 新增
     `UnsyncedIgnoreReturnsOnTheByteItFoundWithoutWaitingForMore`（非同步 `cin`，管道只写 `"\n"`，写端 500 ms 后才写 `"x\n"` 并置标志，`ignore()` 返回时标志
     须仍 false，随后 `get()` 得 `'x'`）。涉及 `std_device` / 标准流的 7 个套件（`device.std_device`、`io.objects`、`concur`、`io.istream.read`、
     `io.streambuf_iterator`、`cvt.root_cvt`、`cvt.code_cvt`）全过。

### 文档（已修 `d85f304a`）

629. `objects.h` 自由函数 `sync_with_stdio` 的 ZH/EN 注释（原 `:186` / `:204-206`）仍写「切回同步时那次冲刷失败 / the flush performed」，第二十三轮
     `b0e7451`（第 619 条）已把输出侧那一步统一改称「搬进 stdio 缓冲 / hand-over」（`out_impl.h` 与 `objects.h:192/:214` 都改了），这两句是同一段里的漏网之鱼
     ——读者会以为切回同步时会 `fflush`。改成「把本流缓冲搬进 stdio 缓冲那一步失败 / the hand-over of its buffer to stdio」，段落重新折行。

### 验证后确认无问题

630. **§6 待办处置表（本轮的优先目标，六项全部关闭）：**
     - **`std_device<0>::m_c` 非空时切 `sync_with_stdio` / `reset()`**：公开可达的「`is_eof()` 之后不取字」序列只有 `cin.device().deof()`
       （`stream_common_operators.h:338-341` 的 `device()` 在 `stdin_api` 上未 delete）；`ignore(n)` 每轮探后立即 `sbumpc`，facet 的 `++beg != end`
       之后总跟 `*beg`。用 `deof()` 构造（`A_01` 五态）：两方向翻转字节**保留、不重复、不乱序**（`std_device.h:103-113` 随移动搬走）；`reset()` 丢 1 字节
       在 `in_impl.h:274-275` 契约内。
     - **`stdin_api::sync_with_stdio` 失败路径**（第 615/622 条）：`A_04`/`A_05`（`cin`/`wcin`，替换 `operator new` + 插桩 `read()`，只开 UBSan）重跑通过：
       `otherfailbit`(0x10)、标志不变、位未清时提取只多 `strfail`(0x18)、`clear()` 后提取 `cvtfail+strfail`(0x0c)、`reset()` 后 0x00；**`reset()` 之后
       kernel 实际模式与标志一致**（true→false 失败后 `read()` n=1，false→true 失败后 n=2048），`wcin` 的 `code()` 也保持——抛出发生在右值 `istreambuf`
       构造期，`m_streambuf` 本体的 kernel 类型（`streambuf.h:182-186` / `:211-217` 按 `has_in_buf` 选定）未变，`attach()` 复用之，与第 579(4) 条互相印证。
       另：`clear()` + **重试** `sync_with_stdio` 也能恢复（第二次 `detach()` 交出惰性设备，stdin 上 `m_active` 只是标记），`:143-145`「须 `reset()`」作为
       指引成立但不唯一。第 622 条的 `err` 分支本探针未触发（`err` 恒空）。仍无常驻用例（注入点仍是 `operator new`）。（**第 696 条后此路径已不存在。**）
     - **截断的多字节序列停在 EOF**（`in_impl.h:403-406`「须 `reset()`」）：翻 `sync_with_stdio` 确实**绕过** `code_cvt_stdio.h:258-259` 的
       `is_init_state()` 守卫（翻转后 `switch_code` 可用），但无害（`A_06`）——`retrieve_impl`（`code_cvt.h:1051-1055`）只拷 `code()` 不查状态，
       `acc.code` 非空（`:208-210` 的前提成立）；半个序列留在旧 streambuf 的 rollback 区随 `:202` `detach()` 丢，不拼进新流；`eofbit` 与设备粘性 EOF
       随移动保留，`reset()` 换新设备能继续读。tainted 后翻转不会因 `retrieve` 抛（`abs_cvt.h:1265-1270` 不查 taint、`:1127-1141` `detach()` 已清 taint）。
     - **第 611 条未覆盖的一格（翻转落在本次插入内部）**（`B_01` a-e）：五种形态无丢失、无重复、锁层数配平、状态位正确。false→true：`exchange` 后
       `m_streambuf.flush()` 把本次插入已写的前半段推进 stdio，尾部按哨兵快照留在本流缓冲，无下一次插入时由退出钩子（`out_flusher::try_flush()` → 流级
       `obj.flush()`，`ostream_operators.h:403-428`）交出（`X1Q2`）；中间夹 `printf` 得 `X1P2Y`（一次插入被拆两半，第 633 条）；true→false：哨兵
       快照仍 flush（`12QBC`）；翻转 + swrite 抛出（掩码武装）：`otherfailbit`、另一线程 `try_lock` 成功、`synced()` 正确；栈展开中哨兵先析构（`m_os` 仍
       good）快照 true 照 flush。
     - **第 609 条同族（用户自建 `ostreambuf<std_device<1>,…>` 被移动）**（`B_04` 六形态）：**纠正前提**——`ostreambuf` 的移动到不了 `root_cvt.h:230-330`：
       `base_streambuf` 的移动 `= default`（`streambuf.h:222-224`）→ `runtime_cvt` `= default`（`runtime_cvt.h:572-573`）→ 只是 `unique_ptr<runtime_cvt_imp>`
       换手，内核原地不动。两层都测：`ostreambuf` 层移动构造 / 赋值 / 自移动 / `/dev/full` / wchar_t（源析构不 `fflush`、目标旧字节经 `~root_cvt` 写出、
       失败只记一次、自移动按 `unique_ptr` 语义安全）；`no_rb_root_cvt<dev1>` 直接移动（真正的 `:230-250` / `:301-331`）：moved-from 根不写不冲、
       `c = move(*b)` 先 `flush()` 自己（`:305-309`）再接管、自移动 `:303` 守卫。
     - **`noexcept(ignore_exception_mask)` 的推广范围**：全库 24 处 bool 模板形参逐个读，与 `clear`/`setstate`/`handle_exception` 同型（bool NTTP +
       `if constexpr` 丢弃唯一抛点）的只有 **`device/mem_device.h:328-347` `mem_device::get_buf<Saturate>`**（`false` 支无任何可抛语句）→ 值得标
       `noexcept(!Saturate)`，连带 `root_cvt.h:2132-2136` 的转发；其余不值得：`root_cvt.h:1780-1830` / `abs_cvt.h:204-250` 的 `get_buf<Saturate>` 两支
       都有无条件 `throw`，`zlib_cvt::zerr<IgnoreBufError>` 只免一种 case，sentry / streambuf / file_device / timeio / monetary 的 bool 都不决定可抛性。
       `<true>` 的显式调用点全库只有 `try_flush`（已 noexcept）与 `io_base.h:1251` 内部。顺手记：`io_base.h:1042-1046` `unset_state()` 是该家族唯一没有
       `ignore_exception_mask` 形参的，无 Doxygen；今日无 noexcept 上下文需要它。是否落地待用户定（见「未覆盖的范围」§6）。
     **【第二十七轮订正】** 「`sync_with_stdio` 绕过 `is_init_state()` 门但无害」对有状态编码不成立，已由第 655 条改为把解码器带过去。

631. **其它验证（第二十三轮新代码与只有推理的旧结论）：**
     - `eofbit` 单独置位时 `sync_with_stdio(true)` 照常搬运（第 624 条「eofbit 放行」封闭）：`B_02 e` 得 `APB`。
     - tainted `wcout` + `clear()` 后先插入再切（`B_03 1`）：`recover()` 把旧字节写出，终态 0x0，不多位、不丢字节。
     - `reset()` 在全缓冲 stdout + `/dev/full` + stdio 里有 `printf` 字节（`B_05` 三态）：失败只记本流、共用 `FILE` 的兄弟流 `good()`；glibc 在 `fflush`
       失败后丢缓冲并（经 `std_device::dflush` 的 `clearerr`）清错，第二次无事可报，「一次失败记两流」不会发生（第二十九轮订正：第二次无事可报是因为 glibc 已丢弃缓冲，与 `clearerr` 无关；`clearerr` 已由 `07925312` 删除，第 684 条）；`wcout` 自己有字节时记的是它自己的失败。
       与 `out_impl.h` `reset()` 的 `@note` 完全一致。
     - `objects.h` 自由函数（读码 + `B_06` control 腿）：八个流的顺序无意义（`stdin_api::sync_with_stdio` 体内无 `tie()`/`try_flush()`、不建 `in_sentry`）；
       `describe` 里的 err 是 `sync_error` 自己不可达（成员抛的只能是 `handle_exception` 重抛的原始异常或它新造的 `device_error`，`m_streambuf.flush()`
       在 `std_device<1/2>` 上不进入用户代码）；掩码武装下抛的是**原始异常类型**（`io_base.h:982` `rethrow_exception(exchange(…, nullptr))`，实测
       `device_error("std_device::dput fail: partial write")`），流名由 `describe` 以 `"cout: "` 前缀补；`:982` 的 `exchange` 意味着抛出后流内不再保存
       该异常，`sync_error::failed()` 是唯一持有者。
     - 栈展开中 `sync_with_stdio(true)` 搬运失败且掩码武装（`B_06`，第 614 条 `handle_exception<true>` 转入路径）：不 terminate、返回旧值 false、
       `rdstate=0x2`、`synced()==1`；对照腿抛 `device_error`；恢复 fd 后 `clear()` + `<<"B"<<flush` 得 `AB`——`root_cvt::flush` 保留的 `A` 未丢。
     - 从用户 `sread` 里重入 `cin.reset()` / `sync_with_stdio`（`A_07`，ASan 干净）：`istream_operators.h:1259` 的迭代器只存 `&obj.m_streambuf`
       （`streambuf_iterator.h:156-157`）与一个 `optional<char>`，`reset()` / 移动赋值只改成员内容、地址不变，`m_read_buf` 是 `vector` 且迭代器不持
       元素指针，`in_sentry` 也不持内部指针——`in_impl.h:120-122` 对 `sync_with_stdio` 的重入警告是**数据**警告不是**安全**警告。同步起步无损，
       非同步起步 rb 缓冲整块丢（契约内）。
     - 第二十三轮改动的注释与代码逐句对（`git diff 3d930946..7e09356a -- include/IOv2/io/objects/`）：`out_impl.h` `sync_with_stdio` 全部吻合（哨兵
       `ostream_operators.h:197-198` 同为 `m_streambuf.flush()`；`operator bool` = good 或仅 eofbit）；`objects.h` 自由函数调用顺序 = `failures` 成员序
       = `describe` 序；`in_impl.h` catch 分支不 `store`，`:225-226` 顺序与第 622 条一致。除第 629 条与第 633 条的前两项外无差异。
     - include 清单按「直接使用」逐名核：三文件缺失 0 / 多余 0（含第二十三轮删的 `<cstdlib>`）。影子头 `-fsyntax-only` 探针（`C_inc_probe.sh`）只作参考：
       50 个 include 里只有 4 个硬依赖，其余传递可得——正是判据必须是直接使用的理由。
     - 机械项：尾部空白 / tab / CR / 连续空行 0，EOF 换行齐，include 三组字母序。中英块：22 对块标签序列 / `**` / 段落数全等，「」20 对、“” 0，
       与第 624 条无差异（`out_impl.h` 新增第 20 对同风格）；本轮改动后复核仍对称（`sync_with_stdio` 块 8/8、4/4）。
     - `objects.h:24-30` / `:58-68` 三种静态对象形态与第 571 / 613 条措辞一致；**查证 N4950**（`~/n4950.pdf`）：`[iostream.objects.overview]/3`（脚注 269）
       与 `[basic.start.dynamic]/3.3`（indeterminately sequenced）两处引用都正确。
     - 三文件 `@note`/`@warning` 交叉引用的 14 个外部位置逐个打开：全部存在且同义（`streambuf.h:759-786` detach 契约、`std_device.h:46-48/:63-65` 类级
       `@warning`、`code_cvt_stdio.h:253-270` adjust、`:179/195/211/255/297` recover、`ostream_operators.h:403-430` attempts=3、`sing_temp.h:223-228`、
       `timeio_details.h:350` / `messages_details.h:752` 静态引用绑定 `*new`、`stream_common_operators.h:430-433/:467-470`、`iov2_export.h:52-54/:89-91`）。

### 七条存疑的处置（主控逐条调研，已修 `eb365726`）

633. 全部是措辞级，无一条行为缺陷；每条都先读码再重跑对应探针（基线 `d85f304a`）。
     - **`in_impl.h` `sync_with_stdio` `@return`「失败时同步状态未改变」**：`err` 在 `cin`/`wcin` 上**可证明恒空**——`streambuf::detach`
       （`streambuf.h:786-816`）只在读缓冲非空时 `seek(tell())` 且吞掉异常；`root_cvt::detach`（`root_cvt.h:408-437`）的 try 体只在 `support_put`
       或 `support_positioning` 时有事可做，`std_device<0>` 两者皆无；`wcin` 多的 `abs_cvt::detach`（`abs_cvt.h:1127-1141`）→ `code_cvt::detach_impl`
       （`code_cvt.h:1005`）→ `close_stream()` 的 `unshift` 支受 `cvt_cpt::support_put<KernelType>` 守卫，输入侧退化为只复位状态。所以 `if (err)`
       是死分支（第 626 条已知、保留作防御），`@return` 对每种可达失败都成立；「失败时」→「重建失败时」与 `@note` 首句同词。
     - **`objects.h` 自由函数「切回同步时它会取该流的锁」**：`out_impl.h` 对任何 `sync == true` 一律取锁（`true→true` 空操作也等插入，第 621 条）；
       改成「切到同步时（已同步再调也一样）取锁，若此前自行缓冲则搬运」，ZH/EN 同。
     - **`out_impl.h`「**不** `fflush`」在 tainted 宽流上不成立**（`G_s3_tainted_fflush`，`LC_ALL=C`，stdout 全缓冲、`printf("P")` 先留在 stdio）：
       未 tainted 搬运 pending 1→2（只搬不 `fflush`）；tainted + `clear()` 后搬运 pending 1→**0**——链 `out_impl.h` `m_streambuf.flush()` →
       `code_cvt_stdio::flush`（`code_cvt_stdio.h:208-213` `if (is_tainted()) recover()`）→ `recover()`（`:297-302`）→ `root_cvt::attach`
       （`root_cvt.h:484-516`）`was_output` 分支 `dflush()` = `fflush(stdout)`；fd 1→`/dev/full` 时 `P` 与本流的 `A` 都被 glibc 丢弃、`devfailbit`
       记在 `wcout`——第 619 条修掉的形态经 `recover()` 在 tainted 宽流上又回来了，且不是 `sync_with_stdio` 特有（任何 `put`/`flush` 都这样恢复）。
       处置：文档加「宽流转换器已 tainted 时例外」限定；代码层面（一个不 `dflush` 的重附接入口）属 `cvt/` 轮，见「未覆盖的范围」§6。
     - **翻转落在插入内部会把一次插入拆两半**（`B_01` b 重跑 `X1P2Y`）：契约内（`exchange` 后 `m_streambuf.flush()` 把本次插入已写的前半段推进 stdio，
       尾部按哨兵快照留到下一次插入），但 `@note` 只写了尾部去向；补半句「前半段随本次搬运立刻进入 stdio，一次插入可能被拆两段、中间可夹别的字节」。
     - **失败态流退出时不刷**（`B_07` 重跑：非同步 `<<"A"; setstate(devfailbit); return` → 0 字节，`std::cout` 同）：钩子 `try_flush()`
       （`ostream_operators.h:403-428`）调**流级** `obj.flush()`（`:730-750`），首句 `if (!static_cast<bool>(self)) throw stream_error` 把失败态挡在
       `m_streambuf.flush()` 之前，异常经 `handle_exception<true>` 只登记。与第 627 条不矛盾：`sync_with_stdio(true)` 做的是哨兵那一步、没门槛；
       退出钩子走流级 `flush()`、门槛在那里——各自定义决定。处置：文件头 `@warning` 补一句「失败态退出不刷（`std::cout` 相同）；先 `clear()` 再 `flush()`」。
     - **「同步 = 逐字节 `read(0)`」**（`A_10` 重跑，插桩 `read()`）：`cin.read(buf,5)` 是一次 `read(0,…,5)`；`get<…>(buf,6)` / `get(buf,16,'\n')` /
       `>> string` 才是 `1 1 1…`。真正的不变量是**不预读超过本次操作所需**（`no_rb_root_cvt` 无缓冲，`streambuf::sgetn` → `root_cvt::get` `no_rb`
       支直转 `dget(to, to_max)`），与 C stdio 的可见性不受影响。处置：只改 `@warning` 的定义句（「不带读缓冲、每次 `read(0)` 只要本次操作所需的字节」，
       括注两种形态）；`@brief` 与成员注释的「逐字节」留作口语概括。
     - **`reset()` 缺与 `sync_with_stdio` 对称的重入警告**（`A_07` 四腿重跑，ASan 干净）：迭代器只存 `&obj.m_streambuf`（`streambuf_iterator.h:156-157`）
       与 `m_saw_eof` 指针，`reset()` / 移动赋值只改成员内容、地址不变，`m_read_buf` 是 `vector` 且迭代器不持元素指针——重入无 UAF；同步起步无损，
       非同步起步 rb 缓冲整块丢（契约内）。处置：`reset()` 加一句「不要在提取进行中重入：不会崩，但本次提取之后已缓冲的输入随之丢弃」。
     - 改后复核：三文件中英块标签 / `**` / 段落数仍对称，尾部空白 0。按用户指示未重新编译（纯注释改动）。

### 四条契约的常驻用例（`test/io-objects-round24-pinning`，已并入 main 为 `0b76cbae`）

634. 本轮的结论此前只有探针与注释，挑四条**行为不变、只是没有防线**的做成用例（无一条改 `include/`）：
     - **`IoObjectsChar.SynchronizedReadsNoFurtherThanTheOperationNeeds`**：管道喂 `"12345X"` 后关写端，`cin.read(buf,5)` 之后直接
       `::read(0,&c,1)`——同步模式得 `'X'`（第六个字节留在 fd 上给 C stdio），非同步得 0（整块进了本流缓冲）。这正是第 633 条第 6 项订正的
       「同步」定义，此前**零覆盖**。
     - **`IoObjectsChar.SwitchingBackInsideAnInsertionSplitsIt`**：测试内为一个 `flipper` 类型特化 `io_traits<char, flipper>`，`swrite` 里写 `'1'`
       → `cout.sync_with_stdio(true)` → `printf("P")` → 写 `'2'`；`stdout_full_buffer` 下断言 `"X1P2Y"`——第 633 条第 4 项刚写进 `@note` 的那句。
     - **`StdObjectsExit.ExitIsNotBlockedByAStreamLock`** 加第四种子进程模式 `failed`：非同步缓冲 `PENDING-AT-EXIT` 后 `setstate(devfailbit)`
       再 `exit(0)`，父进程断言退出码 0 且**文件里没有**那批字节——第 633 条第 5 项（流级 `flush()` 的 `operator bool` 门槛）。复用既有的
       fork + `execl` + 超时骨架。
     - **`RootCvtStd.AMovedFromOutputRootFlushesNothingWhenItDies`** 与 **`StreamBufChar.AMovedFromOstreambufOverAStdDeviceWritesNothingWhenItDies`**：
       §6 的第 609 条同族欠账，两层各一条。判据从「看字节流」改成**全缓冲 + `__fpending(stdout)`**才有检出力：前者去掉 `std_device` 析构里的
       `if (!m_active) return;`（影子头）即失败——真正钉住了「冲刷责任随移动转交」；后者去掉该守卫仍通过，因为 `ostreambuf` 的移动是整个
       `unique_ptr` 换手、源手里**什么也不剩**（没有第二个设备析构），注释已照此改写，钉的是「整体交接」这条结构性质。
     - **反向验证**：四条在改前的头上全部通过（它们钉的是本来就正确的行为，不是修复的护栏）；第 627/628 条自己的两条用例
       （`SwitchingBackToSyncOnAFailedStreamStillHandsOverItsBytes`、`UnsyncedIgnoreReturnsOnTheByteItFoundWithoutWaitingForMore`）退回 `7e09356a`
       的头仍失败，护栏有效。四个套件（`io.objects` 36、`concur`、`cvt.root_cvt` 154、`io.stream_buf`）全过。

### 本轮方法记录

632. - **§6 待办真跑一遍，两条确认问题都是「顺藤摸出来的」**：`m_c` 那条待办本身关闭了（字节保留），但同一条链上的 `dget` 补读暴露了活性问题；
       「第二十三轮新代码就是靶子」再次成立——`out_impl.h` 的门槛是第 611 条加的、第 619 条改写时保留的，两轮的保留理由今天都不成立。
     - **修活性问题时要同时量吞吐**：短读版的活性探针（`A_02`/`A_03`/pty）全绿，吞吐退化（逐字节 `read()`）是落地之后用 `LD_PRELOAD` 计数才看见的。
       凡是改「设备一次交多少」的地方，探针里要有一条「大文件上数 `read()` 次数」。本机无 `strace`，计数用 `LD_PRELOAD` 的 `F_count.c`
       （`dlsym(RTLD_NEXT, "read")`/`"poll"` 转发并计 fd 0 上的次数与请求量，析构函数打印；已随 `.review/` 删除，十几行可重写）。
     - **探针环境陷阱**：对已做过 I/O 的 `_IONBF` 流再 `setvbuf(_IOFBF)` 是 UB，glibc 下 `__fpending` 与直觉差 1；探针要在任何 I/O 之前设缓冲，或像
       `stdout_full_buffer` 那样在用例开头就设。全缓冲 stdout 下 `cout.flush()` 到 `/dev/full` 失败的是 `dflush`=`fflush`，glibc 连本流刚交出的字节一起丢
       （第 627 条的「真实失败」腿因此要用 `_IONBF` 让 `dput` 本身失败）。
     - 三种 agent 类型（claude / engineering-code-reviewer / Explore）分输入 / 输出 / 文档三轴，互不重叠、零重复；两条行为级问题主控都亲手复现。
     - **checked.md 自身的订正（本轮已改正文）**：第 571、573、613 条的 `[basic.start.dynamic]/3.2` → `/3.1`（查证 N4950 §6.9.3.3：/3.1 = partially-ordered
       规则 → sequenced / strongly happens before，/3.2 = 起了线程，/3.3 = indeterminately sequenced；第 625 条的怀疑成立）；第 600、617 条与 §6 引的
       `in_impl.h:193-194` → `:403-406`（ZH）/ `:432-436`（EN）；第 603、605、609、623、625 条的行号漂移各加了「现为」注；第 599 条 `streambuf.h`
       `sgetc` `:259-270` 未漂、`sbumpc` 现 `:290-306`、`is_eof` `:409-414`。**`d85f304a` 之后**的行号：`in_impl.h` `sync_with_stdio` 体 `:195-228`、
       `reset()` `:328-340`；`out_impl.h` `sync_with_stdio` 体 `:233-258`（搬运 `:251`）、`reset()` `:504-523`；`objects.h` `sync_error` `:113-179`、
       自由函数 `:225-250`；`std_device.h` `deof()` `:216-230`、`dget()` `:269-305`（`readable_now` `:281-287`）。**`eb365726`（第 633 条的注释）之后**
       再漂：`in_impl.h` `sync_with_stdio` `:200`、`reset()` `:338`；`out_impl.h` `sync_with_stdio` `:247`（搬运 `:265`）、`reset()` `:518`；
       `objects.h` 自由函数 `:226-251`；`std_device.h` 不变。

---

## 第二十五轮：从 §1「真待办 —— 代码/行为」挑四条实测，并清偿常驻用例欠账（原 §2；已并入 main：`0d2d765c` + `5942b110`）

基线 `main` = `d98dcab0`（2026-09-21；`streambuf → channel` 改名之后）。调研由另一个 LLM 在 arm64 `podman`
（`localhost/iov2-ci-arm64:local`：gcc 15.3.0 / clang 19.1.7 / glibc，`-std=c++23 -fsanitize=address,undefined`）上完成，
另在 Apple clang 21 / libc++ 上做了一处对照；11 个探针在 `~/memory/iov2-round25-probes/`（Mac 侧，**不在本仓库**），
调研报告在 `iov2-round25.md`。主控本机（x86_64 clang-debug）复核了每条引用的代码，落地改动后 60 个套件全过。
四条里三条把原记录的判断改掉了；落地为一个提交（一处签名改动、三处文档、三条用例），一条决定不改并转入「已知但刻意不处理」。
另顺手修了 `.review/A_operators.md` 报的哨兵 `@warning` 残句（第 639 条）。

### 四条待办的处置

635. **【订正 + 不改】`%OC` / `%Op`：根因不在 `expand_format`，在 `do_put` 的 E/O 合法组合表；且是有意的，不是遗漏。**
     原记录（§1）说"根因在 `expand_format`、未读其实现、不知是有意退化还是遗漏"，三点都要改：
     - `expand_and_filter`（`timeio.h:4749`）是 `do_put`（`:4199`）那个 switch 的精确镜像——37 个说明符的修饰符守卫逐格一致，
       `do_get` 同；唯一看起来不一致的 `%Z` 是抽取脚本的假阳性（`do_put` 的判据在 case 体里 `:4664-4674`：`tz` 非空或 `zi->abbrev` 非空；
       `expand_and_filter` 写的是 `has_zone`；六个 `time_value_fields` 特化上逐一等价）。`ExpandFormatAgreesWithWhatPutWrites`
       已把这条不变量全量钉死（6 × 53 × 3 = 954 格），**不需要再加对拍用例**。
     - 拒绝 `%OC` / `%Op` 的是 `do_put` 的 `case 'C'`（`:4306` `modifier == 'O'`）与 `case 'p'`（`:4453` 任何修饰符），
       正好复刻 C23 §7.29.3.5 的表：`E` 只配 `c C x X y Y`，`O` 只配 `d e H I m M S u U V w W y`。POSIX 与 C++ [time.format] 同一张表。
       表外组合在 C 标准里是**未定义**，所以 `bad_format` 回显与 glibc 渲染都合规。
     - `%OC` / `%Op` 是 **glibc 扩展**。查了 `strftime_l.c`：所有数字说明符汇到 `do_number`，那里统一
       `if (modifier == 'O') nl_get_alt_digit(...)`；`case 'C'` 没有拒 `'O'`（而 `%OY` / `%Oc` / `%Ox` / `%OX` 是拒的），
       `case 'p'` 根本不看 modifier。glibc 手册把 `O` 定义成通则"With all format specifiers that produce numbers"，所以 `%OC` 是**文档化通则的覆盖**，
       `%Op` 只是没拦；没有任何 `%OC` 测试（`tst-strftime3.c` 只测 `%EY`），没有 bugzilla / libc-alpha 讨论。glibc localedata 里
       `lzh_TW` / `mnw_MM` / `my_MM` / `shn_MM` 四个 locale 的 `d_t_fmt` 用 `%OC%Oy`（shn_MM 另用 `%Op`）——写 `%OC%Oy` 而不是 `%OY`，
       正是因为 `%OY` 被 glibc 拒了。**其它平台无一渲染**：musl `if (*f=='E'||*f=='O') f++` 跳过修饰符、全库无 alt_digits；
       FreeBSD / macOS / Android（同源 tzcode）`case 'O'` 置标志后 `goto label`，`case 'C'` 不看标志，注释原文抄 C99 表；MSVC 文档不列 E/O；
       libstdc++ `std::format` `case 'C': __allowed_mods = _Mod_E;`，`%OC` **抛** `format_error`。
     - 实测后果（`oc_op.cpp`，2024-09-04 13:33:18）：my_MM 上 glibc `%c` = `၂၀၂၄ စက် ၀၄ …`；IOv2 `put(tm, "%c")` = `%OC၂၄ စက် ၀၄ …`
       （`bad_format` 回显字面量）；经 `expand_format`（`tm.h:291` 往返走的路）= `၂၄ စက် ၀၄ …`（连分隔符摘掉，世纪静默丢）。
       两条路都丢世纪，形态不同；`tm.h` 的往返本身自洽。
     - **决定不改（2026-09-21，用户拍板）**。讨论过三条路：(a) 给 `%OC` / `%Op` 加两格特例——是在标准表上打 glibc 形状的补丁，
       IOv2 的解释器按标准写、glibc 只是数据来源之一（alt_digits 经 POSIX `nl_langinfo(ALT_DIGITS)` 读，换平台照样能读），
       不接受"必须与 glibc 相同"这个框架；(b) 通则"已知说明符配未定义修饰符按无修饰符渲染"（POSIX 的退化精神，musl / BSD 的实际做法）——
       三个 switch 里带 `modifier` 的守卫有 **50 处**，`do_get` 的修饰符语义嵌在 case 体里，等于把 E/O 表从守卫里抽出集中一处再重排三个 switch，
       是一轮独立重构的量；(c) 不改。选 (c)，已写入「已知但刻意不处理」。

636. **【已修，文档】不完整的 `in_ctx_t` 不只是静默 false，而且在整个 TU 里粘住。** 原记录只说"两个编译器都静默答 false"。
     - 触发条件：`extractable_with_ctx`（现 `istream_operators.h:489-500`）首项 `is_same_v` 是短路闸门，主模板 `parse_context_type` 是恒等映射，
       所以**默认情形走不到** `default_initializable`（`:493`）；要摸到它必须有一个用户特化且其 `::type` 在求值点尚未定义。库自带的两个上下文都完整。
       **这是用户扩展点上的坑，不是库内路径。**
     - 实测（`incomplete.cpp` / `ctx_incomplete.cpp` / `ctx_control.cpp`）：gcc 15.3 与 clang 19.1（libstdc++）不完整时静默 `false`、
       补全后**仍** `false`；Apple clang 21（libc++）**硬报错**。两者都合规（IFNDR），但结论随标准库实现而变——原记录只记了 libstdc++ 一侧。
     - 粘性（`sticky_kinds.cpp`）：`is_nothrow_destructible_v<T>` 是变量模板，首次实例化的结果在 TU 里定死；概念满足性本身也被记忆
       （[temp.constr.decl]）。**换成 `requires { sizeof(T); }` 同样粘**——毛病是"在不完整类型上问问题"本身，不是 `default_initializable`。
       `ctx_lateok.cpp`：没人提前探过时晚补全完全正常；`ctx_usererror.cpp`：用户看到的是 gcc 196 行 / clang 105 行的 "no match for `operator>>`"，
       20 个候选没一个提原因。跨 TU 即 ODR 分歧（并入 §2 那一族），且不需要不同的 include 集合，求值顺序不同就够。
     - 处置：三个选项里选**只补文档**。"响亮失败"的出口（无约束兜底 `operator>>` + `static_assert`）会吃掉所有本该是"没有这个重载"的情形，
       与"特化不存在 = 不支持"的整体设计冲突；换完整性检查再求初始化性没用（同样粘）。改动：`traits_base.h`「解析上下文」/「Parse contexts」
       一节在 `final` 那条 `@warning` 后加一段（`::type` 必须在特化可见之处就已完整；IFNDR 两种表现；粘性；跨 TU 指回「方向」一节的 ODR `@warning`；
       与 `make_parse_context` 写坏的"响亮失败"对照——这一格库**探测不到**，给不了具名断言）；`extractable_with_ctx` 的 `@note` 点明短路闸门与粘性，指回 `traits_base.h`。
     - §1 里"`type` 写坏静默 / maker 写坏 `static_assert` 政策不对称"那条与本条同轴：本条补的文档已经把"探测不到"说清，但政策本身未动，那条保留。

637. **【已修，签名】`ignore()` × 2 与 `write()` 的计数形参改 `std::ptrdiff_t`，负数拒（`strfailbit`）。** 原记录说"不构成内存安全问题、改不改是一致性取舍"；
     `38f638d9`（2026-08-23）曾以"不写调用方缓冲"为由保留无符号。本轮把判据坐实之后决策基础变了：
     - `write_count.cpp`：`write(s, SIZE_MAX)` 被 `mem_device` **和** `ofile_device` 双双以 `devfailbit` 挡下（`std::string::replace` 越过 `max_size()` 才报），
       但 `write(s[8], 1 MiB)` 是 ASan 确认的 stack-buffer-overflow READ，回溯穿过 `mem_device::dput → root_cvt::put → runtime_cvt::put → base_channel::putn → write`
       四层没有一层看 `n`。**签名从来不是内存安全的那道线**，`read` 那边真正挡住的只是"负数→巨值"这一种形态。所以剩下的是纯 API 一致性问题。
     - `ignore_cmp.cpp`：`ignore(-1)` 在 IOv2 抽干到 EOF、`ignore(-1, '|')` 扫到分隔符，libstdc++ 两者都一个字符不动。**订正报告的说法**：IOv2 改有符号后走的是
       本库惯例——拒掉、置 `strfailbit`——不是标准的静默 no-op；桌上的选项是"静默抽干"还是"响亮失败"，两个都不等于标准。`ignore(count - 1)` 在 `count == 0` 时就踩到。
     - 现状对照：`width()` / `precision()` / `read()` / `get(s, n)` / `get(s, n, delim)` 全是 `ptrdiff_t` + 拒负 + 有 `@note`；`ignore` × 2 / `write` 是 `size_t` 且文档只字未提。
       **决定改（2026-09-21，用户拍板）**：一条规则比"一条规则加一个理由已失效的例外"便宜。
     - 改动：`istream_operators.h` 两个 `ignore`（现 `:988`、`:1051`）哨兵之后 `n < 0` 抛 `stream_error{"istream ignore fail: negative character count"}`，循环计数同改；
       `ostream_operators.h` `write`（现 `:647`）负数检查在空指针检查之前，`putn` 处 `static_cast<std::size_t>`；`@note` 明确写了只挡回绕值、正数超缓冲区任何签名都挡不住。
       调用方无感：库内没有别的调用点，测试里"无界"已是 `numeric_limits<std::streamsize>::max()`。
     - 用例：`IstreamIgnoreChar.ACountThatIsNegativeIsRejected`（`-1` 与 `PTRDIFF_MIN`、两个重载、`rdstate()` **恰好** `strfailbit`——不含 `eofbit` 证明没抽干、
       `peek()` 仍 `'0'`、掩码后抛）；`OstreamInsertCharacterChar.WriteWithANegativeCountIsRejectedWithoutReadingTheSource`（真实 4 字节源与 `nullptr`、设备为空、
       `clear()` 后可继续写、掩码后抛）。只加了 `char` 版，`wchar_t` 镜像判定不加（性质与字符类型无关）。

638. **【已修，文档 + 用例】`seek()` / `rseek()` 越界状态位：机制坐实，边界补齐，两个设备一致。** `seek_bits.cpp`：
     `seek(size)` 成功 `tell=size`、`seek(size+1)` 与 `seek(1000)` 都 `devfailbit`；`rseek(size)` 成功 `tell=0`、`rseek(size+1)` 与 `rseek(1000)` 都 `cvtfailbit`；
     `mem_device` 与 `ifile_device` 一致。机制：`root_cvt::seek`（`:918`）只查 `pos + m_bos_len` 溢出，越界交给 `dseek()` → `device_error`；
     `root_cvt::rseek`（`:955`）自己取 `dsize()` 判边界，到达设备前就抛 `cvt_error`。`stream_common_operators.h` 两个函数体形状完全相同，差异全来自下层谁先发现。
     文件设备**也**自己做边界检查（不依赖 `lseek` 越过 EOF 合法）；失败后 `tell()` 返回 `nullopt`（`!self` 时提前返回），两条路径一致。
     改动：`stream_common_operators.h` `seek()`（现 `:259`）/ `rseek()`（现 `:306`）各加 `@note`（正向由设备报 `devfailbit`、反向由转换层报 `cvtfailbit`、
     边界为流长度本身、"这是分层结果不是承诺，要统一处理看 `fail()`"）；用例 `IstreamSeekChar.ForwardOverrunIsTheDevicesFailureAndReverseOverrunIsTheConverters`
     把六格钉住，跑 `mem_device` / `ifile_device` / `file_device` × `istream` / `iostream`（比探针多验了 `file_device` + `iostream` 一格）。

639. **【已修，文档】哨兵 `@warning` 的残句（`.review/A_operators.md` A-1）。** `3f87b884`（istream 侧）与 `7290c614`（ostream 侧）都意在删掉
     "该前置条件无法在运行期校验——`copyable_mutex` 不记录属主……"那一句（维护者备注），但那句起于行中，三处把前一行中段也带掉了：
     `in_sentry` ZH 丢"栈展开会先析构本地的锁守卫，置位就落到解锁之后了。锁因此必须是调用方的局部变量，而"；`in_sentry` EN 丢
     "the local lock guard first, so the state bits land after the unlock. The lock"；`out_sentry` EN 把 "unlock. The lock therefore …" 误替换成重复的 "the sentry. …"。
     `out_sentry` ZH 是唯一删对的，作对照补回三处；运行期校验那句按原意保持删除。

### 本轮方法记录与 checked.md 自身订正

640. - **四条里三条订正了原记录的判断**（635 根因与"是否有意"、636 严重程度、637 判据），全部靠"读码 + 探针"而非重审；§1 的记录质量问题是
       "未读实现就下结论"（635）和"只记了一种标准库"（636）。
     - **决策记录要写清被否的选项及原因**（635 的 (a)(b)、636 的"响亮失败"、637 的"保持 `size_t`"），否则下一轮会重提。
     - **`streambuf → channel` 改名（`d98dcab0`）后 `checked.md` 里 `io/` 各文件行号大面积漂移，且漂移不是一律的**：全文 140 处「文件:行号」引用
       （`ostream_operators.h` 31、`io_base.h` 28、`istream_operators.h` 16、`io_manip.h` 15、`streambuf.h` 13、`stream_common_operators.h` 13、
       `ostream.h` 7、`istream.h` 6、`iostream.h` 6、`streambuf_iterator.h` 5）。抽查：§0 `extractable_with_ctx` `:411-425` → 现 `:489-500`、
       分派体 `:1199-1260` → 现 `:1254-1353`（含本轮改动）；§1 `ignore` `:863` / `:919` → 现 `:988` / `:1051`、`write` `:685` → 现 `:647`（往回漂）；
       `istream_operators.h:478` 在 `d98dcab0` 上**没漂**（本轮后 `:493`）；§6 `io_base.h:1042-1046`、`stream_common_operators.h:431` / `:469` **没漂**
       （后者 `@warning` 正文里引的 `root_cvt.h:476-486` 现为 `:484` / `:515`，§6 已记对）。**逐条核过再改，不要按固定偏移平移。**
       §4 的 `io/streambuf.h`、`io/streambuf_iterator.h` 已改为 `io/iochannel.h`、`io/iochannel_iterator.h`。
     - 探针在仓库之外（Mac 侧 `~/memory/iov2-round25-probes/`），索引见附录「第二十五轮」。
     - **本轮之后行号又漂了两次**，本章引用已按 HEAD 重定位：`0d2d765c` 给 `seek()`/`rseek()` 加 `@note`，使
       `stream_common_operators.h` `:320` 之后的一切 +20（`attach()` 的 `@warning` 由 `:431`/`:469` 到 `:451`/`:489`）；
       `92d84657`/`c6a0f7da`/`7409f327` 三个 device/cvt 提交使 `root_cvt.h` `:486` 之后 +2（`attach` 到 `:486-518`）、
       `mem_device.h` 的 `get_buf` 到 `:329`。`io_base.h`、`traits_base.h`、`char_and_str.h` 的引用未漂。

### 常驻用例欠账（原 §2）的清偿（`test/io-round25-ledger` 已并入 main 为 `5942b110`）

原 §2 的 7 条里 2 条本就判定不做（9 个有损 locale、`zlib`/`chacha20` 守卫黑盒），其余 5 条按"最便宜、边界最明确"的顺序全部落地，
**无一条改 `include/`**；一条在动手前先读码判定，把"欠账"改判成了"不可构造"。每条在 clang-debug 下单独跑过，整套 60 个套件全过。

641. **【已做】`io_traits<TChar, char*>` 加宽路径的长串与 `width` 形态；`char16_t` 那一半改判为不可构造。**
     动手前先探了 `char16_t`：`ostream<mem_device<char16_t>, char16_t>` 连默认构造都编译不过——`ctype_conf<char16_t>` 未定义
     （`locale.h:1094`、`ori_facet_buf.h:198`），`locale<char16_t>` 不存在，所以"`char16_t` 一个用例都没有"不是欠账，**IOv2 没有 `char16_t` 流**。
     剩下三种宽类型（`wchar_t` / `char8_t` / `char32_t`）加了 `OstreamInsertCharacterWideningEdges.LongAndPaddedNarrowStringsWidenOnEveryWideCharacterType`：
     70000 字符长串逐字节相同、`setw(6)` 右对齐 / `left`+`setfill('*')` 左对齐、`width` 小于长度时不填充且 `width` 归零、长串 + `setw(70003)` 填充在整段之前。

642. **【已做】`max_pad_count` 的 65536 / 65537 边界在 `char_and_str.h` 五条写路径上逐条钉住。** 新文件
     `test_ostream_inserters_character_fill_ceiling.cpp`：单字符、加宽单字符、字符串、加宽字符串、`basic_string`，每条 `setw(cap + len)` 通过且长度恰为
     `cap + len`、`setw(cap + len + 1)` 置 `strfailbit` 且**一个字符都没写**、`width` 两边都归零；另一条 1000 字符的值证明边界随长度平移（是填充数的边界，不是宽度的）。
     五条路径都汇到同一个 `ostream_insert`（`char_and_str.h:123`），所以钉的是"每个调用点都把正确的长度交给了它"。

643. **【已做，改判】`basic_string::sread` 在 `max_size()` 小于 token 时的行为；原欠账里的"0 / 1"改判为不可测。** 第一版用例真拿 `max_size()` 0 / 1 跑
     （复用 `failure_shapes.cpp` 的 `capped_alloc`，cap {1,2,3}），clang-debug（-O0）全过、**CI gcc-release 挂**，本机 gcc-release 复现：无 `width` 时
     `max_size()==0` 报 `otherfailbit` 而非 `strfailbit`、`==1` 时串空且不 `good()`。探针（`tiny.cpp`）定位到 libstdc++ 自身：
     `basic_string::size()` 里 `if (_M_string_length > max_size()) __builtin_unreachable();`，`_M_is_local()` 里同样对 `_S_local_capacity` 有 unreachable，
     即 libstdc++ **假定 `capacity() <= max_size()`**，而 SSO 缓冲区的 `capacity()` 恒为 15；`max_size()` 低于它时优化器推出矛盾——
     gcc -O0 一切正常，**-O1 下连 `append("", 0)` 都抛 `length_error`（cap 3 / `max_size` 2 也抛），-O2 直接 SEGV**，IOv2 的代码还没跑到。
     这是标准库的前置条件（allocator 的 `max_size()` 至少要装下 SSO），不是 IOv2 的缺陷，也钉不住。
     **改成 cap 40**（两种 libstdc++ 公式下 `max_size()` 为 39 或 19，都 ≥ 16，用例 `ASSERT_GE(bound, 16)` 守着，且按实际报出的值算期望）：
     `AMaxSizeBelowTheTokenIsABoundWithoutAWidthAndAFailingAppendWithOne`——60 字符 token，无 `width` 时取 `max_size()` 个、其余留在流里、`good()`
     （与 `setw(max_size())` 一样）；`setw(max_size()+1)` 时循环照跑、末尾 `append` 抛 `length_error` → `otherfailbit`、救援 `append` 同样失败、串空、
     字符已消费、`width` 归零。gcc-release 与 clang-debug 都过。与类文档"上界为 `width()` 或 `max_size()`"一致，无缺陷。
     **教训**：欠账写的是"0 / 1"，动手前没先问标准库容不容许；-O0 下探针通过不代表 release 通过——涉及标准库不变量的用例要先在 gcc-release 跑一遍。

644. **【已做，改判】第 418 条钳位的 `zoned_time` / `sys_time` 用例：`zoned_time` 路径可达，`sys_time` 路径按构造不可达。** 原记录"那两条路径够不着钳位分支，是推理"，
     读码后只对了一半：`put(zoned_time<Duration, TimeZonePtr>)`（`timeio.h:2413`）**对 `TimeZonePtr` 是模板**，偏移取自 `t.get_info().offset`；
     tzdb 的 `time_zone` 给不出 ±24h 以外的值，但自定义的指针类型给得出。用例里造了 `fixed_offset_zone`（`get_info` 返回任意 `sys_info`、`to_local` 加偏移）
     与 `fixed_offset_zone_ptr`（`operator->` + 隐式转 `const time_zone*` 得 `nullptr`，facet 于是从 `sys_info.abbrev` 取 `%Z`），
     `TimeioChar.AZonedTimeWhoseZoneReportsAnOutOfRangeOffsetIsClampedToo`：0 / −5:30 / 86399 / 86400 / −86400 / ±2³¹ 七格，
     `%z` 钳到 `±2359`、`%Z` 为 `FAKE`、`%F %T` 仍按 zone 自己的算术（24h 偏移 → 次日）。`sys_time` 的 `zone_info{seconds{0}, "UTC"}` 是硬编码，
     **没有可钳的偏移**，钉成 `%z %Z` == `+0000 UTC`。这条也顺带给了"自定义 `TimeZonePtr` 能走通 `put`"一个常驻证据。

645. **【已做】四种字符类型的读侧对拍。** 第十四轮的 1287 格只在 `char` 上与 libstdc++ 对过，第十五轮的 `r15cs_readsweep` 探针跑过四种类型但没有常驻用例；
     `char8_t` / `char32_t` 流在 libstdc++ 里没有对应物，所以"对拍"的参照只能是**本库的 `char` 流**（它自己已与 libstdc++ 对过）。
     新文件 `test_istream_extractors_character_every_char_type.cpp`：10 种输入 × 8 档 `width` × `skipws` 开关 × 三种形式（单字符 / `C[5]` / `basic_string`）
     = 480 格，`wchar_t` / `char8_t` / `char32_t` 各与 `char` 逐格比较 (状态位, 目标内容含未写哨兵, 读位置) 三元组，全部一致；
     另一条 `TheBaselineVisitsEveryExit` 钉住 `char` 基线确实走过哨兵失败 / 全空白 / 短于数组 / 长于数组 / `width` 收紧 / 串形式 EOF / `noskipws` 七种出口，
     免得"一致"是平凡的。写的时候抓到一处自己的错误假设：串形式读到输入末尾是 `eofbit` 不是 `goodbit`（与标准一致）。

---

## 第二十六轮：`io/objects/*.h` 从 §6 待办出发，找到一条跨进 `cvt/` 的确认问题（已并入 main：`fe87ca04` + `6689e745` + `de53d7c5`；存疑处置 `34524ac6`，分支 `docs/io-objects-round26-suspects`）

基线 `main` = `7409f327`（2026-09-23）。三个 agent 分工，范围互不重叠：A 管 `in_impl.h`，B 管 `out_impl.h`，C 管 `objects.h` 加三个文件共用的全局对象生命周期。
§6 里落在本范围的待办全部实跑出了结论。确认问题 1 条，根因在 `cvt/code_cvt.h`，经 `wcout` 触发，已修并带常驻用例。
另有三处注释与代码不符，已改文档（第 647 条）；§6 的两项常驻用例欠账已还（第 648 条）。主控对确认问题和关键的「无问题」结论都亲手重跑过探针，
新用例都做了反向验证（去掉对应守卫或回退修复后失败）。两条存疑经主控重测，都判为只差文档（第 653 条），`issues.md` 已清空。
探针索引见附录「第二十六轮」。

### 确认问题与处置

646. **【已修 `fe87ca04`】有状态编码下，一次编码失败会把 `mbstate_t` 复位成初始态，却不写出复位序列，后续 ASCII 字节被解码成错字。**
     `codecvt_kernel<char, TInt>::out_helper`（`cvt/code_cvt.h` 原 `:346-350`）在 `wcrtomb` 返回 -1 时调 `init_state()`。
     问题在于失败时一个字节也没写出：调用前的状态仍是已写出字节的真实移位态，而 `wcrtomb` 失败后留下的状态是未指定的（C11 7.29.6.3.3）。
     以 ISO-2022-JP 为例，写 `中` 后字节流处于 JIS X 0208 态（`ESC $ B`）；复位之后，taint 恢复链
     `recover()` → `attach` → `detach_impl` → `close_stream()` → `unshift()` 以为不必回到 ASCII，不写 `ESC ( B`。
     实测 `wcout` 依次写 `中`、`é`（失败）、`clear()`、`ab`，得 `1b 24 42 43 66 61 62`，iconv 解出 `中痰`，而状态位全 0。
     连带后果：`switch_code` 的 `is_init_state()` 门（`code_cvt_stdio.h:258-259`）会在字节流仍处于移位态时放行。
     **修法**：调用前保存 `m_state`，失败时恢复保存值（不加注释，用户定）。输入侧 `in_helper` 的复位保留（第 584 条），因为那里坏字节已被消耗。
     **可达性订正**：glibc 不带有状态编码的 LC_CTYPE，但手写一个只列 ASCII 128 项、`<code_set_name> ISO-2022-JP` 的 charmap，
     加一个最小 locale 源，就能 `localedef`（实际编码由 glibc 的 gconv 模块完成）。所以 §6 原记的「本机无 `glibc-locale-source` 无法 `localedef`」
     与第 592 条「glibc 下不可达」**都不成立**。
     **常驻用例** `test/cvt/code_cvt/test_code_cvt_stateful_encoding.cpp`：资源在 `test/IOv2TestResources/iso2022jp/`，运行时 `localedef` 到套件工作目录（约 5 ms）。
     `LOCPATH` 会改变全进程的 locale 查找，且 glibc 在该路径上自身泄漏（`argz_add_sep` ← `__newlocale`），所以检查放在子进程里做：
     设 `LOCPATH`、清空 locale 环境变量、`detect_leaks=0`。覆盖 kernel 两格（失败后仍在移位态，下一个 ASCII 与 `unshift()` 都先出 `ESC ( B`）
     与 `code_cvt` 两种根转换器各一格（失败的 `put` 之后 `detach`，末尾是 `ESC ( B`）；回退修复则五条断言全失败。
     CI 镜像基于 Debian `gcc:15`，`localedef`（libc-bin，Essential）与 gconv 模块（libc6）必在。
     同根未修：`unshift()` 自身的失败分支（`code_cvt.h:211-215`）也是先 `init_state()` 后抛，未构造出触发（见 §6）。

647. **【已改文档 `6689e745`】三处注释与代码不符。**
     - `objects.h` 自由函数 `sync_with_stdio()`：原写「默认掩码下八个流全部切换完毕」，其实只有六个输出流一定切换；
       输入流重建失败时保持原模式、停在未附接状态。探针替换 `operator new` 让分配全部失败：`cout`/`wcout` 已切，`cin`/`wcin` `synced=1` `0x10`。
     - `in_impl.h` `stdin_api::sync_with_stdio()` 的 `@note` 与函数体注释：「此后每次操作都失败」收窄为「每次要读设备的操作」。
       `putback()`、`code()`、`switch_code()` 不碰设备，照常成功，且无害：`putback` 的字符由随后的 `get()` 取回。
     - `ostream.h` 拷贝/移动赋值的 `@note`：原写「经 `root_cvt::operator=` 先 `flush()`」，实为 `runtime_cvt` 内部 `unique_ptr` 换手，
       目标原来的管线被整个析构：`~root_cvt` 把待刷字节 `dput()` 给旧设备，设备析构再收尾冲刷（`std_device` 即 `fflush`），两步的异常都被吞。
       结论（写给旧设备、异常吞掉、绕过失败态守卫）不变，只改机制。第 630 条纠正过同一前提，但当时没回头改这段注释。

648. **【已做 `de53d7c5`】§6 两项常驻用例欠账。**
     - **第 609 条同族**（第 634 条只钉了移动构造）：`test_root_cvt_std.cpp` 加三条，分别是移动赋值（目标先把自己的缓冲 `dput` 给 stdio，
       源对象死时 `__fpending` 不变，结果 `SDT`）、`/dev/full`（持有者的 `attach()` 恰好报一次 `device_error`）、自移动；
       `test_code_cvt_stdio_char.cpp` 在 `wcout` 实际使用的那层加三条：移动构造、移动赋值（多一步 `close_stream()`）、自移动。
       反向验证：去掉 `std_device` 析构的 `m_active` 守卫，五条 moved-from 用例（含原有一条）失败；
       去掉 `root_cvt` / `code_cvt` 移动赋值的自赋值守卫，两条自移动用例失败。
     - **第 610 条链接序形态**：§6 原记「检出力取决于 glob 字母序，做不成可靠用例」，**改判为能做**。
       给析构里用 facet 数据的静态对象加 `__attribute__((init_priority(101)))`，它在一切默认优先级初始化器之前构造、之后析构，与链接序无关。
       `test_facet_statics_exit.cpp` 加第三种子进程模式 `late_static`，析构里解析 `%Z` 并查一次文本域表。
       反向验证：把 `s_timezone_tree` 改回按值定义（`119fa14` 之前的形态），这条腿报 heap-use-after-free。
       C 另做的链接序矩阵：当前头 gcc 两种链接序 × 加/不加共四格，加 clang 一格，全干净；
       回归版不加 `init_priority` 时有一种链接序漏检，加上后两种都报。

### 已验证无问题

649. **`in_impl.h`（agent A）**
     - **`O_NONBLOCK` 的 stdin（§6，关闭）**：管道上两种 sync 模式 × 十种形态，ASan/UBSan 干净。形态包括空管道 + 写端已关、延迟写入、
       写端一直开着不写后再关（只收到 POLLHUP 按 EOF）、token 跨两次写入、`ignore()` 活性、`get`/`read` 跨写入、sync 翻转、`reset`、
       `wcin` 的 UTF-8 半个序列跨 EAGAIN 到达。`std_device.h:382-400` 的 EAGAIN+poll 分支把非阻塞 fd 还原为阻塞语义；
       第 628 条 `readable_now` 的零超时 poll 只决定「是否补读」。未测：tty 上的 `O_NONBLOCK`、`POLLERR`/`POLLNVAL` 支（见 §6）。
     - **截断的多字节序列停在 EOF（§6，按现状关闭）**：现状与第 600 条逐字一致；`clear()` + `switch_code` 得 `cvtfailbit`，已写进 `in_impl.h:413-415`。
       翻转 sync 会绕过 `is_init_state()` 守卫，但在 `d98dcab0` 改名后仍无害：新 kernel 由 `code_cvt_stdio_creator(acc.code)`（`:216-218`）重建，本就处于初始态。
     - **`sync_with_stdio` 失败路径的注入点（§6）**：题面三个候选都不行。重建路径里 creator 写死（`:210`/`:218`），设备被 `requires` 挡住（`:89`），
       CRTP 参数 `T` 与重建无关；`LOCPATH=/nonexistent` 也注入不了失败，还会触发 glibc 自身泄漏。
       **可行的是链接期 `-Wl,--wrap=_Znwm` / `--wrap=newlocale`**：它们不定义全局 `operator new`，所以能与 ASan 共存（第 581 条的冲突来自「定义」）。
       扫描「第 k 次分配失败」：重建恰好 2 次分配，失败后状态 `0x10`、标志不变，`reset()` 后 kernel 模式与标志一致，`detach()` 不分配；
       主控复跑 16 格全 OK。`--wrap=newlocale` 得 `cvtfailbit`，掩码开时抛 `cvt_error`。常驻化方案见 §6。
     - **`in_sentry` 的 tie 提示（§6，只记不评）**：另一线程持 `cout` 锁时 `cin >> x` 照常返回、fd 1 上 0 字节，与 `istream_operators.h:81-83` 的 `@warning` 一致。
     - **失败后未附接态下的各操作**：首次在 ASan 下跑全，要读设备的都得 `cvtfailbit`，无越界。
       `= delete` 的 `detach`/`attach`（`:274-275`）可用限定名绕过，效果等同「sync 失败再 `reset()`」，没有可破坏的不变量。
     - `d98dcab0` 对本文件只是机械改名，第二十四轮结论改名后重跑仍成立。
     **【第二十七轮订正】** 上面「翻转 sync 绕过守卫仍无害」与「截断序列按现状关闭」在有状态编码下不成立，见第 654、655 条。

650. **`out_impl.h`（agent B）**
     - **`unset_state()` 缺 `ignore_exception_mask`（§6 顺手记，关闭）**：`out_impl.h` 不调用它；经继承唯一可达的是 `seek`/`rseek`
       （`stream_common_operators.h:264`/`:311`），在标准流上恒失败，缺这个形参只改变掩码武装时抛哪一个异常。
     - **tainted 宽流 `recover()` 的 `dflush`**：`code_cvt_stdio.h:297-302` 与 `root_cvt.h:486-518` 未漂移；
       `sync_with_stdio` 与 `switch_code` 两处的文档限定准确。观察时必须用裸 `write(2)`：`IOv2::cerr` tie 到 `cout`，会把差别抹平。
     - **第 609 条同族的其余形态**：全部干净，且探针有检出力；已做成用例，见第 648 条。
     - **有状态编码的输出侧（§6，关闭）**：退出时缺 unshift，与 glibc C stdio、libstdc++ `std::wcout`（同步与非同步）逐字节一致，不是缺陷；
       `reset()` 正确写出 `1b 28 42`；`switch_code` 的门按文档工作。顺带找到第 646 条。
     - **输出侧重入 `reset()`/`switch_code()`**（从用户 `swrite` 里调）：同步与非同步 × 三腿，ASan 干净，字节完整有序。
       原因是插入持有的是 `m_channel` 的地址，`detach`/`attach` 与内核移动赋值只改内容不改地址。

651. **`objects.h` 与全局对象生命周期（agent C）**
     - `sync_error` 的初始化顺序逐行核对：基类先于成员（[class.base.init]/13），`describe` 读按值形参，之后才移动。
       `what()` 不可能抛：覆盖一个 `noexcept` 虚函数而不加 `noexcept` 是编译错误（探针实测）。
       `sync_error`/`failures` 的拷贝与移动都是 nothrow（static_assert 通过）。
     - `one` lambda 的 `noexcept`：只有 `pthread_cancel` 的强制展开会破坏它（实测 abort），而 `io_base.h:1170-1175` 已声明不支持取消。
     - **静态初始化期另一线程 `exit()`（§6，关闭，并订正第 616 条）**：K=1…177 × 8 档延迟全干净，九个单例自身没问题。
       第 616 条原探针有两处缺陷：`main` 返回与 exit 线程并发调两次 `exit()`，本身是 UB；按 `newlocale` 序号定位只覆盖初始化窗口最后约 2 ms，
       tzdb 遍历那约 40 ms 扫不到。改为从 `init_priority(101)` 起线程按时间扫，约 25% 命中；
       ASan 报告被截断时，用弱钩子 `__asan_on_error` + `SIGUSR1` 停住 exit 线程。
       报告里有 UAF 的读访问帧落在 IOv2 的 `timeio_details.h:386`/`:398`，所以「只在 libstdc++ 内部」要改措辞；
       但被释放的全是 libstdc++ 在 exit 线程里析构掉的 tzdb，属于 [basic.start.term] 下调用方的 UB，不报。
     - **`_NL_LOCALE_NAME`（第 588 条）**：调用链只在运行期经过（`wcin` 重建、`code()`、`switch_code()`），构造期不经过。
       10 种环境全部正确，包括不存在的 locale、`LANG=C`、`POSIX`、空值、路径名、`C.UTF-8`、GB18030。移植风险照旧（§6）。
     - 全库退出时会析构的静态对象重扫仍为零；只包含部分头文件的多 TU 初始化顺序，gcc/clang 各四种链接序全干净，`tie` 与 `code()` 正确。
     - `stream_common_operators.h:451`/`:489` 的 `@warning` 行号本轮未再漂（§6 那条照旧）。

652. **checked.md 引用漂移（以 `7409f327` 为准，未逐条改原文）**：
     第 630、632 条与原 §6「截断的多字节序列」的 `in_impl.h:403-406`/`:432-436` 现为 `:413-415`（ZH）/ `:444-445`（EN）；
     第 630 条的 `:274-275`（`reset()` 丢弃契约）现指 `= delete` 的 `detach`/`attach`，契约在 `:279-286`；第 630 条的 `:208-210`/`:202` 现为 `:216-218`/`:207`。
     另：第 647 条改注释后，`in_impl.h` 此后的行号又后移约 2 行。

653. **【存疑 → 已改文档 `34524ac6`】两条存疑重测后都不是缺陷，只差文档。**
     - **同步模式下，编码失败的那次宽字符插入本身就会 `fflush(stdout)`。** 链路：`~out_sentry`（`ostream_operators.h:198`）在栈展开中仍 `flush()`
       → `code_cvt_stdio::flush` → `recover()` → `root_cvt::attach` 的 `dflush()`。第 633 条原记「`clear()` 之后下一次操作才恢复」，
       实际在默认配置下就发生在出错的插入内部。关键在于和「字符可编码」的对照组比：
       管道上唯一的差别是时机（`printf` 的字节提前落到 fd，`FILE` 内顺序不变，最终 `Pb`）；
       `/dev/full` 上对照组同样丢掉 `printf` 与本流的全部字节（最后的 `fflush` 报 `ferror=1`），且 IOv2 状态位为 0、完全看不到失败，
       而出错组把失败记在 `wcout` 上（`0x6`）。所以这次 `fflush` 没有让任何本来能保住的字节丢失，设备也确实坏了，谈不上错误归因。
       **结论**：行为不改，原 §6「一个不 `dflush` 的重附接入口」随之关闭。`code_cvt_stdio.h` 类文档原写「已提交的数据在重新附接时刷出」，
       改为写明 `dflush()` 即 `fflush` 整个 `FILE`、别人的字节随之落盘或一并丢失、失败记在本流，以及同步模式下发生在出错的插入内部。
     - **宽流不能与 `wprintf` 混用。** 宽流自己编码，再用 `fwrite` 写字节，`FILE` 是字节定向的。实测配对（`LC_ALL=C.UTF-8`）：

       | 同一个 stdout 上混用 | IOv2 | libstdc++ |
       |---|---|---|
       | 宽流 + `printf` | 成功 | `std::wcout` 先写后 `printf` 返回 -1 |
       | 窄流 + 宽流 | 两种顺序都成功 | `std::wcout` 先写后 `std::cout` 失败 |
       | 宽流 + `wprintf` | `wprintf` 先：IOv2 窄流、宽流都得 `0x2`；IOv2 先：`wprintf` 返回 -1 | 成功 |

       C11 7.21.2 本就禁止在同一个流上混用两种定向，两边只是配对对象相反：IOv2 让 `cout` / `wcout` / `printf` 任意交错，代价是不能用宽字符 stdio。
       **结论**：设计取舍，不改代码。全库原本对「定向」只字未提（grep `fwide|定向|wprintf` 零命中），而 `wprintf` 失败只返回 -1、很少有人检查，
       故在 `out_impl.h` 文件头中英两块各加一条 `@note`，写明可交错与不可混用的范围和两种失败方式。
     探针（scratchpad，未入库）：`s1.cpp`（`bad` / `good` / `std` 三模式 × 管道 / `/dev/full`，stderr 用裸 `write` 打标记看落盘时机）、
     `s2.cpp`（九种配对，报各方返回值、状态位与 `fwide`）。

---

## 第二十七轮：`io/objects/*.h` 从 §6 待办出发，有状态编码的输入侧挖出三条（已并入 main：`ffc6b658` + `0b23dcf5` + `d1de8e1d` + `be650898`）

基线 `main` = `6897ac91`（2026-09-25）。五个 agent 分工，范围互不重叠：A 管 §6「有状态编码的输入侧」（`wcin` 读 ISO-2022-JP），
B 管 §6「`sync_with_stdio` 失败路径」的落地方案实跑，C 管 §6「`O_NONBLOCK` tty 与 `POLLERR`/`POLLNVAL` 支」，D 管 `out_impl.h` 全文件与 `6897ac91` 重构，
E 管 `objects.h` 与第 592 条。确认问题 5 条，全部已修；存疑 7 条，全部按文档处理。`include/` 的改动由用户逐块审过。
主控对 A-1、D-1、E-1 亲手复跑过探针；各修复都有常驻用例，gcc-sanitizer 全量 60 个通过。探针索引见附录「第二十七轮」。

### 确认问题与处置

654. **【已修 `ffc6b658`】`wcin` 逐字符读有状态编码时静默出错（A-1）。** `get(&c, 1)` 时 `get_main` 每次只给 `in_helper` 1 字节。
     glibc 的 `mbrtowc` 从存着半条移位序列的状态续接时返回 1 却不写 `*pwc`、并丢掉移位态；一条完整移位序列恰落在输入末尾时返回 3 也不写。
     `in_helper` 把 `r > 0` 一律当字符交出（`wch` 在循环外初始化为 0）。`a ESC$B 中 中 ESC(B bc` 读成 `a \0 C f C f \0 b c`，整块读则重复一个字符，状态位全无。
     **修法**：kernel 自己保存半个字符的原始字节（`m_pending`，`MB_LEN_MAX` 大），下次接在新输入前整条喂给 `mbrtowc`；每次调用前把 `wch` 置哨兵
     `(wchar_t)-1`，正数返回而哨兵未改即「只有移位序列」，不出字符。`codecvt_kernel<char8_t>` 同样收字节，原热循环原样保留为私有 `decode_run`。
     新增 `is_mid_seq()`；`is_init_state()` 只看移位态（`m_state` 不再存半条序列，两者正交），`init_state()` 两样都清。
     `switch_to_put` 与 `code_cvt_stdio::adjust` 的门加 `is_mid_seq()`。`get_main` 到 EOF 时本次没交出字符而 kernel 收着字节，抛 `partial input sequence`
     （已交出字符的那次照常返回，`read()` 不丢）。**行为变化**：截断在字符中间的输入，`wcin` 由 eof 变为 cvtfailbit。依据：C11 7.29.3.1 把 `fgetwc`
     「字节不够」算编码错误；libstdc++ `fstream.tcc:468-472` 本意报错（实测打不到）；C++ 标准无规定。实测 glibc `fgetwc` 与 libstdc++ 都静默当 EOF。
     **放弃的方案**：「`-2` 不消费、交 `get_main` 回滚」——回滚不把字节还给设备，`get_main` 抛出前也没回滚，`adjust` 看不见读缓冲里的半个字符。
     **性能**（`-O2`，12 MB 中英混排，绑核取最小值）：locale kernel 整块读无可测差异、逐字符读 +12%；`char8_t` kernel 整块读 −7%、逐字符读 +45%。
     试过拆快慢路径、就地拼接，逐字符读无可测改进；根在 `get(&c, 1)` 每次只给 1 字节，多取会让无缓冲 stdin 阻塞。

655. **【已修 `ffc6b658`】`wcin.sync_with_stdio()` 在移位态中途切换丢移位态（A-2）。** `in_impl.h` 先 `detach()`（`close_stream` → `init_state()`），
     再按编码名新建 kernel，从初始态起步；JIS 段剩下的字节读成 ASCII，状态位全无（JIS X 0208 字节都是合法 ASCII，永不报错）。
     **修法（方案 B，把解码器带过去）**：`code_cvt_stdio_state`（同时派生 `cvt_status` 与 `cvt_behavior`，成员 `std::optional<codecvt_kernel<char, wchar_t>>`；
     kernel 无默认构造，且空即「没人回答」）。`detach()` 之前经 `retrieve()` 拷出（拷贝失败时流原样未动），新 iochannel 建好后经 `adjust()` 放回
     （`ichannel` 构造函数即 `init_cvt()`，放回的状态不会被随后清掉）。`code_cvt_stdio` 重写公开的 `retrieve()`，与它重写 `put`/`get`/`flush`/`adjust` 一致。
     两个方向都搬；非同步 → 同步按第 582 条契约照旧丢缓冲。
     **放弃的方案**：A「加门拒绝」（可行，实测拒绝后 `clear()` 能接着读，但用户要切换成功）；C「切换成功、之后读失败」（无法自愈：新 kernel 分不出何时回到 ASCII）；
     把 `code_cvt::retrieve_impl` 改 protected 由派生类写钩子——派生钩子若非 public 又忘了对 `abs_cvt` 声明友元，`requires { self.retrieve_impl(s); }`
     静默为 false，连基类钩子一起跳过（`code()` 静默返回空串，`.review/tmp/a2/access*.cpp` 实测）。
     **【第二十八轮】** 放回本身可失败（`duplocale`），已改为移动（第 671 条）；第 672 条后宽标准流不再接受状态依赖的编码，本条「移位状态随之搬走」对标准流只剩历史意义，搬运半个字符的部分仍有效。

656. **【已修 `ffc6b658`】EILSEQ 之后 `in_helper` 丢移位态（A-3）。** `-1` 分支的 `init_state()`（第 584 条所加）把 `m_state` 复位，`clear()` 后
     `中 0xFF 中` 的第二个「中」读成 `C f`。C11 说「状态未指定」的只是交给 `mbrtowc` 的 `tmp_state`；`m_state` 仍是坏字节之前的真实状态（glibc 实测保留）。
     **修法**：`init_state()` → `m_pending_len = 0`（只丢收着的字节）。**行为变化**：有状态编码在移位段里出错，`clear()` 后仍在移位段，`switch_code()`
     按原有的门被拒，读到 `ESC ( B` 后放行；无状态编码不变（第 584 条的 GBK 用例照过）。`in_impl.h` 的 `reset()` 说明与 `switch_code()` 正文、`@note` 相应改写。
     **【第二十八轮】** 第 672 条后宽标准流不再接受状态依赖的编码，本条对标准流不再适用；`code_cvt` 本身仍按本条保留移位态。

657. **【已修 `d1de8e1d`】共用 stderr 的宽流各持移位态，交错写出错字（D-1）。** `wcerr << L"中"; wclog << L"ab";`
     得 `ESC$B Cf ab`（`中痰`），两种 sync 模式都中；`wcerr` 之后接 `cerr`、`printf` 同样。libstdc++ 的 `std::wcerr`/`std::wclog` 共用一个缓冲（`rdbuf()` 相同），
     这一对正确；但 `std::cerr` 接在 `std::wcerr` 后，同步模式下失败（`FILE` 已宽定向），非同步模式下同样 `中痰`（实测）。
     **修法**：`code_cvt_stdio` 新增私有钩子 `flush_impl()`（按 `abs_cvt.h:668-676` 对 `abs_cvt` 声明友元）：有状态编码下冲刷前若不在初始态，先写复位序列，
     写法同 `close_stream()`；失败由 `abs_cvt::flush` 标 taint。**每次冲刷交给 fd 的字节都以初始状态结尾**。同步模式下哨兵每次插入都冲刷，
     单线程时任意交错正确，一次插入中途写满缓冲也不例外（实测 3000 字）。**边界**：非同步或多线程下，缓冲写满自动写出的那一段仍可能停在移位段，
     另一写入者插进来即错字；此类交错本就不保证顺序（UTF-8 下实测也乱序），写进 `out_impl.h` 新增的 `@note`，原「可以任意交错」加限定。
     代价：两次插入「中」得 `ESC$B Cf ESC(B` 两遍（实测），解码不变；无状态编码字节不变。
     **放弃的方案**：仿 libstdc++ 共用 iochannel（结构大改，且管不到 `cerr`/`printf`）；逐字符补复位（JIS 文本约三倍体积）；`code_cvt_stdio` 加小缓冲、每块补复位
     （为不被下面两层缓冲切开须每块冲到设备，改动面大，换回的只是乱序场景不出错字）。
     **【第二十八轮】** 被第 672 条取代：`flush_impl()` 已删除，宽标准流不再接受状态依赖的编码（写复位序列时设备失败会丢序列，见第 672 条）。

658. **【已修 `0b23dcf5`】`iov2_export.h` 的漏宏自检对 clang 查不出（E-1）。** 自检 `nm -D prog | grep ' u _ZN4IOv2'` 只认 gcc 的 STB_GNU_UNIQUE；
     clang 把 inline 变量发成弱对象 `V`，漏宏的 clang 消费者 9 个单例全在可执行文件里（`LD_DEBUG=bindings` 确认 .so 的引用绑到它们），配方却 0 行。
     CI 只用 g++ 编两个对照，所以没发现。**修法**：配方改 `grep -E ' [uV] _ZN4IOv2'`（inline 函数为 `W`，不在内），中英说明写明两种编译器各显示什么；
     `ci.yml` 那一步外套 `for cxx in g++ clang++`（镜像自带 clang）。本机抽出该步实跑：两编译器都是正确 0 / 漏宏 9；旧模式下 clang 漏宏为 0，该步会失败。
     **余项**：模式不精确到这 9 个单例，用户程序里别的 `IOv2` inline 变量也可能命中（gcc 下旧模式同样如此）。

659. **【存疑 → 已改文档 `be650898`】七条存疑都不改行为。**
     **改正**（原文与实际不符）：`io_base.h` 中断阻塞 I/O 的建议（本进程另一线程 `close(0)`/`dup2` 打断不了标准流：`read`/`poll(-1)` 一直等，
     `O_NONBLOCK` 被 `poll(-1)` 还原，`EINTR` 被重试；须关对端或自定义设备带超时）；`std_device` 类说明「非阻塞读取」（实为阻塞）；
     `deof()`「挂断按 EOF」（tty 挂断为 `EIO`/`POLLERR` → devfailbit，阻塞 fd 偶得 EOF；`std::cin` 报 eof|fail。选改文档而非向 `std::cin` 看齐，
     因为已有程序可能依赖 devfailbit，且阻塞 fd 上内核结果本就不定）。
     **补充**：`reset()` 在同步模式下丢不掉 fd 上未读的字节，移位段中途调用会解错；`sync_with_stdio` 重建失败丢 `deof()` 预读的 1 字节
     （`m_c` 随旧 iochannel 销毁；成功路径随设备带走，第 630 条）；输出侧不处理 `EAGAIN`（tty `O_NONBLOCK` 下 64K 只送达约 23.5K，`std::cout` 同）；
     raw tty `VMIN == 0` 时阻塞 fd 超时读 0 当粘性 EOF、`O_NONBLOCK` fd 以 `poll(-1)` 等而无视 `VTIME`；`RTLD_LOCAL` 头文件模式插件 gcc 合并一份、
     clang 各一套（`tie_graph_mutex()` 亦然），插件用 `IOV2_SHARED`。

### 已验证无问题

660. **`sync_with_stdio` 失败路径，第 615/622/649 条落地方案全部断言实跑（agent B）**：`-Wl,--wrap=_Znwm` / `--wrap=newlocale`，RAII 只包那一次调用，
     每格断言 `hits > 0`；cin/wcin × 两方向。返回值 == 旧值、标志不变；`_Znwm` 得 0x10、`newlocale` 得 0x04；`clear()` 后提取 0x0c；`reset()` 后提取成功并清掩码；
     `::read(0)` 可见性与标志一致；掩码开时抛原始类型，抛前状态位已置；重建恰 2 次分配（2048 B 根缓冲 + 80/152 B kernel），k = 1、2 失败，k = 2 时第一块正确释放。
     失败态 `reset()` 分配 0 次、`newlocale` 0 次；未附接态各操作与 `@note` 逐句一致；第 622 条 `err` 分支仍不可达。
     **配置矩阵**：`IOV2_SHARED` **打得到**（模板成员在测试 TU 实例化）；gcc/clang ASan、TSan、-O2/-O3、coverage、Valgrind、clang LTO 可用；gcc `-flto` 须
     `detect_leaks=0`（`facet/` 永不释放单例的 LSan 误报，与 wrap 无关）；lld 未测（本机与镜像都没有）。方案文字要改四处，已写进 §6。

661. **`std_device` 的 poll 分支与 pty（agent C，§6 两项关闭）**：`POLLNVAL` 支（`std_device.h:390-391`）用 `--wrap=read` 在 `read` 返回 EAGAIN 那一拍 `close(0)`，
     确定性打到（`hits == 1`）：抛 `device_error` → devfailbit，掩码开时原样抛，不自旋，fd 重接后 `clear()` 即可读；`:399` 不可达。
     `O_NONBLOCK` tty（forkpty、canonical、VEOF=^D）：确认走了 EAGAIN→poll，nb 与 blk 四格一致（跨写入的 `>>`、行中/行首 ^D、UTF-8 被 ^D 劈开后拼回）；
     `std::cin` 同条件第一次读即 EAGAIN 失败。pty 主端关闭：nb revents=0x19 → devfailbit，blk `EIO` → devfailbit，无死循环。

662. **`out_impl.h` 与 `6897ac91`（agent D）**：deducing-this 重构等价——`abs_cvt` 按 `self` 静态类型找钩子，`code_cvt_stdio` 当时无同名成员遮蔽，无切片，
     `seek`/`rseek` 返回 `S&` 有 `static_assert`；六个对象的全部公共成员 gcc/clang 跑遍。第 646 条修复在 `wcout` 上：2048 字节根缓冲两次写满时的每个偏移
     × sync 0/1 × ASCII/汉字共 200 格、`sync_with_stdio`/`reset`/`switch_code`/`flush` 共 16 格，每格恰好一次 `ESC ( B`。

663. **`objects.h` 与第 592 条（agent E）**：`sync_error::describe`/`what()` 各种 failures 组合（空、全满混非 std 异常、10 万字符、嵌套）顺序与分隔符正确，
     拷贝/移动/切片/重抛后不悬垂；构造中第 1-5 次分配失败 `bad_alloc` 逃出、无泄漏。**第 592 条「静态初始化期 `tie(&cout)` 撞上持锁线程」原记未探针，
     现三种形态实测都只是一次等待**：header-only（`/proc`+`nm`+gdb 确认阻塞在 `out_impl.h:599` 那把锁上，TSan 干净、正控每次报）、`IOV2_SHARED` 病态依赖图、
     dlopen 在 `dl_load_lock` 下初始化（lazy/`-z now` × ASan/TSan 各 25 次）。`cerr_t()` 体内窗口不可达（`instance()` private）。
     契约外不报：用户异常 `what()` 返回 nullptr（UBSan 在 `objects.h:168`）。

664. **checked.md 引用与结论的订正**：第 584、600、630、649 条关于截断序列与 `sync_with_stdio`「绕过门但无害」的结论在有状态编码下被第 654-656 条推翻，
     已在各条末尾加注；第 592 条「未探针」见第 663 条；第 583 条的 `X_t::ptr()` 已不存在。行号漂移（以 `6897ac91` 为准，未逐条改原文）：
     第 646 条 `code_cvt_stdio.h:258-259` → `:269-270`；`in_impl.h:413-415` → `:416-418`；第 652 条 `:216-218`/`:207` → `:219-221`/`:210`；§6 的 requires `:88` → `:89`；
     成员 `m_streambuf` 已改名 `m_channel`。第 654-659 条的提交之后行号又有移动，动手前以当前代码为准。

665. **【已做常驻用例 `5cee3163`，分支 `test/round27-resident-cases`】本轮只靠探针验证的结论，五项固化。**
     - **`sync_with_stdio` 失败路径**（第 615/622/649/660 条，原 §6 待办）：新套件 `test/io/objects/sync_failure/`，6 个用例：全部分配失败（cin/wcin × 两方向）、
       `newlocale` 失败只在 wcin（cin 调 0 次）、掩码开时抛原始类型、逐个让重建的第 k 次分配失败（次数先数出来，不写死 2）、失败态 `reset()` 零分配零 `newlocale`、
       `deof()` 预读的字节随失败的重建丢失（第 659 条写进文档的那句）。方案的四处写法订正（第 660 条）全部照做；每次注入都断言 wrapper 确实生效。
     - **`POLLNVAL` 支**（第 661 条）：新套件 `test/device/std_device/poll_error/`，`--wrap=read` 在 EAGAIN 那一拍 `close(0)`，
       断言抛 `device_error`、`read` 只调一次、fd 重接后照常读。注意 wrapper 关掉 fd 0 后，新 `pipe()` 的读端可能正是 fd 0（初版因此 SIGPIPE）。
     - `test/CMakeLists.txt` 只给这两个目标加 `LINKER:--wrap=…` 与编译宏（`UNIX AND NOT APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8`），其余平台用例 `GTEST_SKIP`。
     - **`char32_t` 流的有状态编码**（§6 余项）：`CodeCvtStatefulEncoding.Char32StreamsRoundTrip`，两种根 × 逐字符 / 一次读。
     - **`wcout` → `wcin` 往返**（§6 余项）与**流层 UTF-8 截断**（第 654 条的行为变化）：`IoObjectsWchar.WcoutOutputReadsBackThroughWcinUnderAStatefulEncoding`、
       `IoObjectsWchar.WcinHandsOutWhatCameBeforeACutOffCharacter`。
     gcc-sanitizer 全量 62 个套件通过。未做：pty 上的 tty 形态（CI 容器是否有 `/dev/ptmx` 未确认，阻塞模式挂断结果本就不定）。

---

## 第二十八轮：`io/objects/*.h` 从 §6 待办出发，挖出八条（main：`fa41ab2e` + `503db289`；第 671-673 条与存疑处置：`1daff04f` + `71d42254` + `dfe0b716` + `418bc780` + `2d7416e4` + `4c84eccd`，均已并入 main；复核续作第 680-681 条：分支 `fix/cvt-is-eof-pending-input` 已并入 main 为 `a16a2d13` + `d55c84fc`）

基线 `main` = `24b546eb`（2026-09-30）。四个 agent 分工，范围互不重叠：A（general-purpose）管 `wcin` 读路径上的**解码**，沿链进 `cvt/code_cvt.h`；
B（code-reviewer）管 `in_impl.h` 的**控制路径**（`sync_with_stdio` / `reset` / `switch_code` / 第 655 条状态搬运 / tie / `deof`）；C（appsec）管 `out_impl.h` 全文件与
`code_cvt_stdio` 输出侧；D（architect）管 `objects.h` 与跨对象不变量（跨插件 tie、第 658 条漏宏自检、静态析构期）。确认 7 条，复核问题 1-3 的修复时又发现 1 条（第 669 条后半），
共 8 条，全部已处置；存疑 10 条随后逐条调研并处置（第 679 条），`issues.md` 与 `.review/` 已删。复核 checked.md 时又在第 668 条的推迟报错上找到一处（第 680 条），并固化了三项只靠探针的结论（第 681 条）。主控对每条确认问题都用 agent 的探针亲手复跑过。**8 条里 6 条的修改点在 `cvt/`**（从标准流对象可达，故在本轮报告）。
问题 1-3 与 5-6 由用户按 `issues.md` 自行修改，主控复核 diff 后补了第 669 条、第 672 条的两处并提交。探针索引见附录「第二十八轮」。

自建 locale 的造法（下文反复用到）：取 `test/IOv2TestResources/iso2022jp/` 的 charmap，改 `<code_set_name>` / `<mb_cur_max>`，
`LC_ALL=C localedef -c -f X.cm -i mini.src <dir>/xx_XX.<codeset>`，运行时 `LOCPATH=<dir>` 只在子进程里设、`ASAN_OPTIONS=detect_leaks=0`。

### 确认问题与处置

666. **【已修 `503db289`】`mb_cur_max` 声明偏小的 locale 下，`wcout` 写出越过根缓冲末尾（C-1，§6「`mb_cur_max` 越界写」坐实）。**
     `codecvt_kernel<char,TInt>::out_helper` 只要求槽有 `m_epc`（= `MB_CUR_MAX`）字节，就让 `wcrtomb` 直接写进槽；glibc 的 `wcrtomb` 不按 `MB_CUR_MAX` 截断
     （`MB_CUR_MAX=1` 的 locale 下对 U+4E2D 返回 5）——charmap 的 `<mb_cur_max>` 只是声明，真正编码的是 gconv 模块。标准流走根特化 writer，槽开在 2048 字节的
     `root_cvt::m_buffer` 里：k=2044-2046 个 `a` 后写「中」ASan 报 heap-buffer-overflow（WRITE of size 5）；不越界时写进下一个槽，`put_buf_guard::used()` 无符号回绕、
     `rollback` 的抛出被吞，输出静默错乱而流 `good()`（300 个「中」只剩 303 字节）。越界点在根缓冲之后，**不是** §6 原先推测的通用 writer 256 字节缓冲。
     **修法**：先 `wcrtomb` 进 `MB_LEN_MAX` 大的局部数组，长度超过槽则恢复 `m_state` 并抛 `cvt_error`；`put_buf_guard::used()` 拒绝超过预留的长度而非回绕。
     **常驻用例**：`CodeCvtStatefulEncoding.AnUndersizedMbCurMaxIsRejected`（`support/stateful_locale.h` 新增 `<mb_cur_max> 1` 的 `xy_XY.ISO-2022-JP`）、
     `CvtIo.PutBufGuardRejectsUsingMoreThanWasReserved`。

667. **【已修 `503db289`】ISO-2022-JP 下 JIS 段里出现 0x00，`wcin` 任意读法都让进程 abort（A-3）。** glibc `mbrtowc.c:102` 在解出 NUL 时
     `assert(__mbsinit(state))`，而 ISO-2022-JP 的 gconv 模块允许 C0 字符出现在任何字符集段且不复位，`ESC $ B … 0x00` → `Fatal glibc error` → `abort()`，
     库的异常处理与状态位都拦不住。`get(c)` / `read` / `>>` × 两种 sync 全中，最短输入 `1b 24 42 00`；`fgetwc` 与 libstdc++ `std::wcin` 同输入不崩；
     KR / CN-EXT / IBM930 / IBM933 / UTF-7 同位置得 EILSEQ。
     **修法**：状态依赖的编码下 `mbrtowc` 不再看到 0x00 字节——交给它的长度截到 NUL 之前，NUL 本身由 kernel 解成 `L'\0'` 并回到初始状态（C11 7.29.6.3.2），
     NUL 之前是半个字符则判编码错误。**行为取舍**：NUL 之后按初始态解读（`ESC$B Cf 00 20 61 62` 读成 `中 \0 空格 a b`），`fgetwc` 保持 JIS 态（读成 `痰`）、
     libstdc++ 在 NUL 后停住。原「NUL 由多字节编码」的那段循环删除，`conv == 0` 现判失败（UTF-7 `+AAA-` 这类编码出的 NUL 未测，见 §6）。
     **CI 修补**（并入同一提交）：`find_nul` 的 `from == from_end` 在 GCC 15.3 `-O3` 下报 `-Wstringop-overread`（推不出长度非负），改为 `from >= from_end`；
     本机 gcc 不报，须在 CI 镜像里用真实编译（`-fsyntax-only` 不跑中端，复现不出）。
     **常驻用例**：`CodeCvtStatefulEncoding.ANullByteInAShiftedStateIsANullCharacter`、`CodeCvtMemChar32.ANullByteDecodesToANullCharacterInAMultibyteLocale`。

668. **【已修 `503db289`】`wcin.read()` 块读遇到解码错误，本块坏字节之前已解码的字符与之后已取出的字节全部丢失（A-1）。** 与编码无关，UTF-8 即可：
     `ab\xffcd ef` 上 `read(buf, 4096)` 后 `clear()` 再读，六个字符一个不剩；`read(buf, 2)` 丢坏字节后的 `c`。`get_main` 在 `in_helper` 已写出字符后照样抛，
     `getn` 的 `got` 不含它们；每次 `get_main` 又 `reader.reset`，同一块里坏字节之后已从 fd 取走的字节随之丢弃。违反 `in_impl.h`「`clear()` 即可继续、从坏字节之后对齐读取」
     与 `iochannel.h` 对 `got` 的契约；第 584 条只测了逐字符读，块读此前无用例。libstdc++ 同输入 `gcount() == 2`。
     **修法**：取数区从每次的 `cvt_reader` 移进 `abs_cvt`（`m_rd_cur` / `m_rd_end`），未消费的字节跨 `get()`、拷贝与移动保留；`abs_cvt::m_kernel` 改 private，
     派生层经 `kernel_get` / `kernel_put` / `kernel_tell` / `kernel_seek` / `kernel_rseek` 访问（`tell` 扣掉未读字节，`seek` 清空取数区）。`get_main` 交出已解码的字符，
     从坏字节之后（或从暴露了收着的前导字节的那一字节）续读，错误推迟到下一次调用（`m_get_failed`）。顺带修了存疑 A-S4（`\xe4ab` 丢 `a`）。
     **行为变化**：有未读输入时，不能定位的 kernel 上 `switch_to_put()` 改为抛出（原先静默丢弃）。
     **常驻用例**：新文件 `test/cvt/code_cvt/test_code_cvt_read_after_error.cpp`（三种根上的块读、中途移动、收着的前导字节）。

669. **【已修 `503db289`】合法 ISO-2022-JP 输入按 `read()` 块大小时好时坏地被判编码错误（A-2，§6「`m_pending` 放满」坐实）；以及复核第 668 条时发现的
     「坏字节紧跟移位序列，块读把 `$ B` 当 ASCII 交出」。** 两者同根：glibc 对「完整移位序列 + 坏字节或半个字符」整段返回 -1 或 -2，不说开头那条移位序列已被消化。
     前者：块读时移位序列在 `m_pending` 里越攒越多，5 条冗余 `ESC $ B`（15 字节）即撑满，n ∈ {8, 9, 11, 12, 14, 15, 17} 失败；该出口用 `init_state()`，
     `clear()` 后读出 `B C f b`（第 656 条只改了 `-1` 分支）。后者：第 668 条的 `resume = ext_cur + 1` 只跳过 `ESC`，`a ESC$B C FF Cf ESC(B b` 块读得
     `a ! $ B C ! C f b`，逐字符读则得 `a ! ! 中 b`；UTF-7 `a+Ti\x80b` 块读把 `T i` 当 ASCII 交出。
     **修法**（主控补）：状态依赖的编码下拿到 -1 或 -2 后，先从短到长试探前缀，开头是完整移位序列（正数返回且哨兵未改）就当一次成功消费，再判错或收字节；
     超长出口 `init_state()` → `m_pending_len = 0`，与 `-1` 分支一致。无状态编码不走这段。
     **验证**：上述输入块读与逐字符读结果一致；k = 5 / 6 / 8 条冗余移位序列 × n = 1..40 全对；agent A 的 `kf.cpp` 随机分块差分（6 种编码各 5 万例、`char8_t` 20 万例）新旧皆 0 差异；
     反证：两个新用例编到去掉修复的副本上都失败（`a!$BC!Cfb`、k=5 n=8 得 `a!BCfb`）。
     **常驻用例**：`CodeCvtStatefulEncoding.ABlockReadRecoversLikeOneCharacterAtATime`、`RedundantShiftSequencesReadAtAnyBlockSize`（两种根）。

670. **【CI 修补，并入 `503db289`】clang-tidy job 失败。** `cvt_reader` 新构造函数相邻的两个 `std::size_t&` 触发 `bugprone-easily-swappable-parameters`（该族为 warnings-as-errors），
     按 `numeric.h` / `timeio.h` 先例行尾 NOLINT。同时清掉 CI 输出里的三条警告（`run_status : std::uint8_t`、`close_stream` 的 `state_guard` 补删除的移动、`std_device.h`
     `readable_now` 的 `do-while` 改 `while`），以及本机 clang-tidy 21 多报的两处 `modernize-use-integer-sign-comparison`（同文件先例 NOLINT）。
     **教训**：CI 镜像的 clang-tidy 版本与本机不同，两边报的警告集合不完全一致；提交前按 `ci.yml` 的参数本机跑一遍能挡住大部分。

671. **【已修 `71d42254`】`wcin.sync_with_stdio()` 放回解码器状态失败时，流已按新模式重建而标志仍是旧值（B-1）。** 第 655 条把放回（`adjust(state)`）放在新 iochannel
     已赋给 `m_channel` 之后，而 `code_cvt_stdio::adjust` 拷贝 kernel 要 `duplocale`；失败时 catch 块按「新的没建起来」处理：标志不更新、装上的是新建内核（第 655 条原样复现，
     中读成 `C f`）、按标志切回是空操作。常驻用例只 wrap `_Znwm` / `newlocale`，这个分配点在盲区。**修法**：`code_cvt_stdio_state::kernel` 改 `mutable`，`adjust` 移入并清空，
     不会失败；空状态由「什么也不做」改为抛 `cvt_error`。**常驻用例**：`SyncWithStdioFailure.AFailedDuplocaleLeavesWcinAsItWas`（`--wrap=duplocale`，先数次数再逐次注入；
     旧代码有第 2 次 `duplocale`，正被 `expect_twelve_in_the_reported_mode` 抓到）、`CodeCvtStdio.AHeldHalfCharacterGoesOverToANewConverter`、`AnEmptyStateIsRefusedAndChangesNothing`。

672. **【改设计 `71d42254`】`flush_impl()` 写复位序列时设备失败，复位序列丢而移位态已清，恢复后 fd 停在 JIS 段（C-2）。** `flush_impl` 先 `unshift()`
     （`m_state` 归初始）再 `put`；根缓冲剩余 ≤ 3 字节时设备失败（`/dev/full`、EBADF、EPIPE），`root_cvt::put` 两个分支都丢复位序列，`clear()` 后 fd 上 `… ESC$B Cf b` 而流 `good()`。
     只在 `mb_cur_max` 声明为诚实上界 5 时打得到（测试资源声明 8，最少剩 4 字节）。
     **处置**：不修 `flush_impl`，而是让 `code_cvt_stdio`（宽标准流背后的转换器）**不接受状态依赖的编码**：构造时遇到则改用 `"C"`，`adjust()` 切换到这样的编码
     （`code_cvt_switch` 指名，或手工填进 `code_cvt_stdio_state` 的内核——主控补）在任何改动之前抛 `cvt_error`；`flush_impl` 删除。理由：一个 fd 由多个写端共用
     （`wcerr`、`wclog`、`cerr`、`printf`），移位状态却记在各自的转换器里，写失败后也无从得知 fd 停在哪个状态。`recover()` 只重新附接设备、不重建内核，`code()` 报内核实际的
     `"C"`，所以宽流在任何路径上都拿不到状态依赖内核（主控核过）。**这取代了第 657 条，以及第 655、656 条在标准流上的有状态编码部分**；`code_cvt` 本身对有状态编码的支持
     （第 654、666-669 条）不变，供文件流等使用。**行为变化**：环境为 ISO-2022-JP 时 `wcin` 静默按 `"C"` 读（`ESC $ B` 按 ASCII 字符读出），`out_impl.h`、`in_impl.h`
     （主控补）、`code_cvt_stdio.h` 均已写明。**常驻用例**：`CodeCvtStatefulEncoding.TheStdioConverterTakesNoStateDependentEncoding`、`IoObjectsWchar.SwitchCodeRefusesAStateDependentEncoding`。

673. **【已修 `1daff04f`】第 658 条的漏宏自检配方在 `g++ -flto` 下失效（D-1）。** g++ `-flto` 把漏宏消费者自定义的单例降为普通全局对象，`nm -D` 显示 `B`，
     且无 COPY 重定位——与正确消费者在 `nm -D` 上逐字相同，配方得 0 行；`LD_DEBUG=bindings` 显示 `libiov2.so` 的 9 个引用绑进了可执行文件。clang `-flto` 仍显示 `V`。
     **修法**：判据改为「可执行文件**已定义**、却**不是 COPY 重定位**的 `_ZN4IOv2` 数据符号」（`nm -D --defined-only` 与 `readelf -rW` 的 COPY 取差集，需 bash）；
     `ci.yml` 对两个编译器各加一格 `-flto`。**验证**：本轮 26 个对照二进制（两编译器 × 默认 / `-O2` / `-flto` / `-O2 -flto` / `-fvisibility-inlines-hidden` / g++ `-fno-gnu-unique` ×
     正确 / 漏宏）全部正确 0 行、漏宏 9 行。**仍查不出**：`-fvisibility=hidden` 的漏宏消费者（0 行，但后果是两套单例互不相干而非插桩，见第 679 条 D-S2）。

### 已验证无问题

674. **有状态编码的输入侧（agent A）**：ISO-2022-KR / UTF-7 / IBM930 / IBM933（EBCDIC SO/SI）/ ISO-2022-CN-EXT 都能用 `localedef -c` 造出 locale（EBCDIC 须按真实映射造 charmap，
     否则 `wcout` 静态初始化时 `timeio_conf` 转 ASCII 失败 abort——夹具问题，非库问题）。正常文本 × 四种读法 × 两种 sync 与 `fgetwc` 逐项对拍一致；移位序列恰在输入末尾全部干净 eof；
     纯移位型编码 EILSEQ 后 `clear()` 续读对齐正确（IBM930 DBCS 段内坏字节后错位是固有歧义，`fgetwc` 同样恢复不了）；第 654 条 `m_pending` 拼接边界随机分块差分 0 差异、无越界。
     `in_impl.h` 只有 `cin` / `wcin`，不存在 char8/16/32 的标准输入对象。（第 672 条之后，这些结论对标准流只剩历史意义，对 `code_cvt` 本身仍成立。）

675. **`in_impl.h` 控制路径（agent B）**：第 655 条新路径全分配点故障注入（`_Znwm` 4 次、`newlocale` 1 次、`duplocale` 2 次），除第 2 次 `duplocale`（第 671 条）外逐句与 `@note` 一致；
     第 656 条的 `switch_code()` / `reset()` 门两种 sync 都与 `@note` 一致；`m_pending` 半个字符与 `deof()` 预读字节不可能同时存在（`m_pending` 只在设备 EOF 时跨调用留存、
     `deof()` 只在未 EOF 时预读、`m_eof_hit` 粘性且随设备搬走，FIFO 探针确认）；`in_impl` 上构造不出 tie 环（`tie()` 只收 `abs_flusher*`，`cin` / `wcin` 入度恒为 0）。

676. **`out_impl.h` 与输出侧（agent C）**：§6「通用 `cvt_writer` 丢批」在六个标准输出对象上不可达——`code_cvt_stdio` 的下层恰为根，选中根特化 writer（`commit()` 空操作），
     `stdout_api` 删除了 `detach()` / `attach()`，用户换不了层；`--wrap=wcrtomb` 地址追踪确认 1000 字写进连续地址。只属 `cvt/` 轮。
     `flush_impl()` 在 17 个场景下恰好补一次复位序列、无状态编码 6 个 locale × 15 个场景新旧逐字节 0 差异、根缓冲尾部 804 格 ASan 干净（第 672 条删除 `flush_impl` 后只剩历史意义）。

677. **`objects.h` 与跨对象（agent D）**：第 659 条余项「两插件互 tie 成环」——`RTLD_LOCAL` 载入两份插件拷贝，g++ 头文件模式与 `IOV2_SHARED` 各并发 10 万轮零成环；
     clang 头文件模式下环会形成（两把 `tie_graph_mutex()`），但成环后两端插入照常返回、两线程各 10 万次同时写无死锁（`try_flush` 不沿 tie 递归），再 `tie()` 被 Floyd 拒绝置
     `strfailbit` 不挂死（措辞问题见第 679 条 D-S1）。静态析构期使用流、另一线程 `exit` / `quick_exit`（80 次）与 `objects.h:27-33`、`out_impl.h:32-37` 的契约一致。
     `objects.h` 在第二十七轮后只有 `be650898` 加的 `@note`，其「gcc 合并一份、clang 各一套」实测成立。
     （**补，第二十九轮**：非同步模式下，定义在头文件之前或另一 TU 里的静态对象析构时写的输出会丢失——属 `objects.h:27-33` 点名的契约外形态，见第 690 条 B-2；`ce02942c` 起退出钩子先切回同步，这些输出不再丢，见第 692 条。）

678. **checked.md 引用与结论的订正**：第 655 条「新 iochannel 建好后经 `adjust()` 放回」未提放回本身可失败（第 671 条）；第 660 条「重建恰 2 次分配」在 locale 名超出 SSO 时为
     4 次 `_Znwm`；第 662 条「每格恰好一次 `ESC ( B`」测的是 `d1de8e1d` 之前的代码，本轮重跑仍成立（随后被第 672 条取代）；第 663 条「阻塞在 `out_impl.h:599`」现为 `:635`
     （`cerr_t()` 里的 `tie(&cout)`）；§6「`io_base.h:1170`」起始行未漂、`@warning` 止于 `:1178`。**测试文件改名**（`fa41ab2e`）：`test/` 下 24 个不以 `test_` 开头的源文件
     加了前缀（`cvt/code_cvt`、`cvt/crypt`、`cvt/root_cvt`、`cvt/comp`、`io/traits/include_combinations/comb_*`），本文件里的旧文件名已一并替换。

679. **第二十八轮存疑 10 条的处置（2026-10-01，主控在 `71d42254` 上逐条调研，探针对当前代码重编复跑）。**
     - **改文档、行为不改（用户拍板，均已移入「已知但刻意不处理」）**：A-S1「输入在字符中间到达 EOF，`clear()` 后永远 cvtfailbit、到不了 eof」（`418bc780`）；
       A-S2 / A-S3「UTF-7 这类状态里存半个字符的编码，出错后续读拼出错字、截断到 EOF 不报错」（`4c84eccd`）；D-S1「clang 头文件模式插件各一把 `tie_graph_mutex()`，
       并发互 tie 实测成环」与 D-S2「`-fvisibility=hidden` 的漏宏消费者配方无输出」——由「多模块必须用共享库模式、不支持混用」的新规则一并关闭（`2d7416e4`，第 612 条的「共存」形态作废）。
     - **A-S4 已修**：「逐字符读时坏前导字节后紧跟的合法字节被一并丢掉」（`\xe4ab` 丢 `a`）随第 668 条的续读位置修掉，当前代码读出 `[错] a b`。
     - **A-S5 不可达**：哨兵 `(wchar_t)-1` 在 16 位 `wchar_t` 上与 U+FFFF 冲突。`codecvt_kernel<char, wchar_t>` 的 `requires` 不限宽度，但库依赖 glibc 扩展 `_NL_LOCALE_NAME`
       与 POSIX `newlocale`，16 位 `wchar_t` 的平台（Windows、AIX）上本就编不过；移植时随 §5 多平台一项一并看。
     - **B-S1 已不适用**：「移位段中 `sync_with_stdio` 重建失败后 `reset()` 静默解错」——宽标准流不再接受状态依赖的编码（第 672 条），`in_impl.h` 那段说明也已删。
     - **C-S1 无可观察后果**：`unshift()` 失败分支先 `init_state()` 后抛（`--wrap=wcrtomb` 可注入，真实 glibc 无触发）。删掉 `flush_impl` 后唯一调用方是 `close_stream()`，
       其 `state_guard` 每条退出路径都 `init_state()`，与「恢复原状态再抛」外部看不出差别。
     - **C-S2 已不适用**：`out_impl.h` `switch_code()` 的 `@note`「有状态编码写出非 ASCII 之后被拒」已在 `71d42254` 改写为「该编码是状态依赖的」。
     - **CI 修补**：`1daff04f` 的自检配方用到进程替换，而容器里 `run:` 默认是 `sh`（镜像里是 dash），`dfe0b716` 给该步加 `shell: bash -e {0}`（不用 `shell: bash`，
       其 pipefail 会被 `readelf -d | grep -q` 误伤）。**教训**：别假设 CI 用 bash，镜像的 `/bin/sh` 与本机不同，要在镜像里 `sh -n` 实测。

680. **【已修 `0bf0c887`，并入 main 为 `a16a2d13`】有错误待报或收着半个字符时，`is_eof()` 仍说流已读完（复核第 668 条时发现，2026-10-02）。**
     `code_cvt` 在交出字符之后遇到的解码错误推迟到下一次 `get` 报（`m_get_failed`，第 668 条），到 EOF 时还可能收着半个字符（`m_pending`）；这两种情况下一次 `get` 都会报错，
     但 `is_eof()` 只看取数区与下层，回答「已到末尾」。后果：(1) 变长编码的 `switch_to_put()` 以 `is_eof()` 为门禁，读 `ab\xff` 块读交出 `a b` 后照样放行，写入、切回读，
     下一次 `get` 抛出一个过时的错误（`code_cvt` 层直接操作才打得到：流层 `getn` 拿到不足量会立即再调 `get`，错误当场报出）；(2) 输入在字符中间到达 EOF，`clear()` 后
     `get()` 每次 cvtfailbit，`ignore()` 却从第二次起只报 eof（它先问 `is_eof()`）——与第 679 条 A-S1 定的契约不一致（`wcin` 实测）。
     **修法**（用户定方向：改 `is_eof` 的逻辑）：`abs_cvt` 加 protected `kernel_is_eof()`（取数区用尽且下层 EOF），默认 `is_eof()` 调它；`code_cvt` 提供 `is_eof_impl()`：
     `!m_get_failed && !m_cvt_kernel.is_mid_seq() && kernel_is_eof()`。变长分支的门禁由此自动拒绝上述两种情形，`ignore()` / `iochannel_iterator` 也会真去读一次而报出错误。
     **定长编码**（用户订正主控的方案）：变长 / 状态依赖编码要求读到末尾，是因为未到末尾就写会影响写入内容之后的解析；定长编码没有这个问题，可以读到一半就切换，
     但切换前要抵消「一半读」：底层位置与逻辑位置不一致（取数区里多取了字节）或有错误待报时，先 `seek_impl(m_accu_len)` 重定位并清状态，写入从逻辑位置开始（覆盖坏字节）；
     底层不能定位而又有错误待报时拒绝。主控原先建议「不论编码都要求先读掉待报的错误」，被否。
     **异常安全**（主控分析、用户复核）：`kernel_tell()` / 拒绝 / `seek_impl` 抛出时本对象未变（`seek_impl` 先提交下层、余下 noexcept）；唯一非强保证的一格是重定位之后下层
     `switch_to_put()` 抛出——本层仍在读模式、停在逻辑位置，再读会重新读到那些字节（坏字节再报一次），外部序列与切换前相同。
     **常驻用例**（`test_code_cvt_read_after_error.cpp`）：`APendingErrorKeepsTheStreamFromEnding`、`HeldBytesAtEofKeepTheStreamFromEnding`、`AFixedLengthSwitchWritesWhereReadingStopped`（`"C"` 下
     `ab\xffcd` 写出 `abXcd`、再读得 `cd`）、`AFixedLengthSwitchWithoutPositioningIsRefusedWhileInputIsPending`（包装 kernel 去掉定位；三格：坏字节在前、推迟报错、坏字节在末尾）、
     `AFailedSwitchAfterRepositioningReadsOnAsBefore`（包装 kernel 让 `switch_to_put()` 抛出，把上段异常安全结论变成实测）。定长用例用 glibc 的 `"C"`（拒绝 0x80 以上字节），不拒绝时自跳过。
     **反证**：同一份测试对 HEAD 编译，前四条失败；假 kernel 那条在旧代码上也通过（旧代码这一格外部序列恰好相同，它锁的是承诺本身）。gcc-sanitizer 18 个相关套件通过，clang、clang-tidy 干净。
     **教训**：用户在 VS Code 里确认了 `code_cvt.h` 却漏了写入，接着写用例、跑测试，新用例全挂才发现，并对用户误报「已写入」——确认后立刻落盘并 `git diff --stat` 核实。

681. **【已做常驻用例 `0b82aa7a`，并入 main 为 `d55c84fc`】复核 checked.md 时列出的只靠探针的结论，固化三项。**
     - `IoObjectsWchar.WcinHandsOutWhatCameBeforeACutOffCharacter` 补全：截断在字符中间到 EOF 后，`clear()` 再读 `get()` 与 `ignore()` 都仍 cvtfailbit、不到 eof；`reset()` 后为普通 eof
       （第 679 条 A-S1 的契约，`418bc780` 的文档；`ignore` 那两行无第 680 条修复时失败）。
     - `IoObjectsWchar.WideStreamsFallBackToCUnderAStateDependentEnvironment`：子进程以 `LC_ALL=xx_XX.ISO-2022-JP` 启动，`wcin` / `wcout` / `wcerr` / `wclog` 的 `code()` 都是 `"C"`（第 672 条）。
       `support/stateful_locale.h` 的 `run_stateful_child()` 为此加了可选的 `lc_all` 参数。
     - 第 668 条「有未读输入时，不可定位的 kernel 上 `switch_to_put()` 抛出」由第 680 条的 `AFixedLengthSwitchWithoutPositioningIsRefusedWhileInputIsPending` 一并锁住。

## 第二十九轮：`io/objects/*.h` 只找「刚才没有提到」的问题（main：`add90a23` + `ef4f6966` + `6c905966` + `9ca6588e`；`3fa8260c` + `07925312` + `e75dfa63` + `3fde4969` + `78d60b7c` + `a892258e`；存疑处置 `27f50ca9` + `ce02942c` + `0353bfa0` + `42cc2bd4`，均已并入 main）

基线 `main` = `4c84eccd`（2026-10-02）。主控做机械项（include 分组与字母序、行尾空白、连续空行、末尾换行、Tab / CRLF）；四个 agent 范围互不重叠：
A（general-purpose）`in_impl.h` 语义、B（claude）`out_impl.h` 语义、C（general-purpose）`objects.h` 与跨对象并发（clang TSan，带正控）、
D（Explore，只读）三文件的拼写、注释与代码一致性、include 必要性。确认 7 条，主控处理第 3 条时又发现 1 条（第 689 条），共 8 条，全部已处置；
存疑 7 条与两项待定随后逐条调研并处置（第 692 条），`issues.md` 与 `.review/` 已删。主控对每条确认问题都亲手复跑过。**8 条里 3 条的修改点在 `device/` / `cvt/`**（从标准流对象可达）。
修改由主控做，`include/` 的每处改动经用户在 VS Code diff 里逐块确认。探针索引见附录「第二十九轮」。

### 确认问题与处置

682. **【已修 `add90a23`，注释 `6c905966`，测试修补 `9ca6588e`】部分写入失败后，恢复时把已写出的前缀再写一遍（B-1）。**
     `fwrite` 返回值小于 `n` 时前若干字节已交给 stdio（无缓冲的 stderr 上已经 `write` 落盘），而 `std_device::dput` 只抛 `partial write`、不说接收了多少，
     `root_cvt` 整批留着；下一次冲刷把已落盘的前缀再写一遍。`RLIMIT_FSIZE=5` 下 `clog << "0123456789"`，再按 `out_impl.h` 文件头推荐的 `clear()` + `flush()` 恢复，
     fd 2 得 `012340123456789` 而流 `good()`（stdio 对照只丢后半、不重复）。`clear()` 后续写、退出钩子冲刷、`reset()`、`sync_with_stdio` 的搬运同样重复。
     `ENOSPC`、`EFBIG`、`O_NONBLOCK` 上的 `EAGAIN` 都走 glibc 同一分支。第 659 条只记过 `EAGAIN` 下「丢」，没有「重复」这一面。
     **修法（契约）**：`defs.h` 新增 `dput_error : device_error`，带 `written()`；`dput` 抛 `dput_error(what, written)` 表示前 `written()` 个字符已被下层接收，
     抛普通 `device_error` 表示一个也没收（`device_concepts.h` `support_put` 的 `@note`）。`std_device`、`file_device` 的 `dput` 在 `fwrite` 之后失败时抛前者。
     `root_cvt` 新增私有 `dput_buffer(len, old)`，`put` 的两个分支、`flush`、根特化 writer 的 `put_buf` 都经它：只留未被接收的部分；本次 `put` 自己的字符
     只有在 `written > old`（越过了先前的字符）时才算已接收（`root_cvt.h` 那段长注释用 ABCDE / ABCDEF 举例）。设备未必知道接收了多少——不知道就抛普通 `device_error`，
     退回原来的整批保留；宽字符的 `file_device` 不存在，按字符计数即字节数。
     **测试修补**：CI memcheck 报 fork 出的子进程 `_exit` 时「possibly lost」（Valgrind 跟 fork 不跟 exec），改为 `test/support/test_child.h`：fork + `execl` 自身、`--gtest_filter`
     指名一条用例、环境变量 `IOV2_TEST_CHILD` 带参数。**常驻用例**：`test_root_cvt_partial_write.cpp`（`RootCvtPartialWrite.*` 6 条：flush 部分接收、普通 `device_error`、
     恰好填满与溢出两个 `put` 分支、上层 writer 经根缓冲、真实 `file_device` 在 `RLIMIT_FSIZE` 下报 `written() == 5`）、
     `test_io_objects_partial_write.cpp`（`clog` / `cerr` / `wcerr` × sync / nosync）。
     **顺带（`ef4f6966`）**：按另一 AI 的建议给 `ctype.h` 18 处、`collate.h` 6 处纯查询加 `[[nodiscard]]`，`ctype.h` 6 处读表函数加 `noexcept`。

683. **【文档 `3fa8260c`】输出流「先 `io_mutex()` 后 FILE 锁」的锁序，与用户的 `flockfile` 相撞（C-1）。** 插入先取流锁，哨兵析构在同步 / unitbuf 下 `flush()` →
     `fwrite` / `fflush` 取 `stdout` 的 FILE 锁；用户在 `flockfile(stdout)` 里写 `IOv2::cout`（想把 `printf` 与本库输出圈成一段），与另一个直接写 `IOv2::cout` 的线程
     形成 AB-BA，`timeout 5` 3/3 挂死（`std::cout` 同样用法正常结束：libstdc++ 的 `stdio_sync_filebuf` 不另持流锁）。tie 变体：另一线程只写 `cerr`，`try_flush` 用 `try_lock`
     拿到 `cout` 的流锁后 `fflush(stdout)` 仍阻塞在 FILE 锁上（9 次 7 次挂死）。
     **处置（用户定：只改文档）**：`io_base.h` `io_mutex()` 加锁序 `@note`；`IOv2::sync` 第一条 `@warning` 限定为「流锁之间」，新增一条 `@warning` 给出正确写法
     （`IOv2::sync guard(stream); flockfile(stdout); …`，外层流锁、内层 FILE 锁）与三种死锁形态；`out_impl.h` 加锁序 `@note`；`ostream_operators.h`
     `abs_flusher::try_flush` 的「必须非阻塞」限定为「取流锁不阻塞，之后的 `fflush` 仍可能等 FILE 锁」。彻底改法（插入时先 `flockfile` 再取流锁）改动面大，未做。

684. **【已修 `07925312`】`std_device` 写失败时 `clearerr` 抹掉共享 FILE 的错误标志（C-2）。** `dput` / `dflush` 的失败分支对 `stdout` / `stderr` 调 `clearerr`，
     连用户自己的 `printf` 失败先置上的标志也清掉；退出前看 `ferror(stdout)` 的惯用法（gnulib `close_stdout`）因此把丢了输出报成成功。`ferr2.c` 实测 glibc 在标志置位时照常写、照常返回，
     这个 `clearerr` 不是继续写所必需的。**修法**：删掉两处 `clearerr`（用户定：不加说明）。全库搜过其余 `clearerr`：`file_device` 的是设备私有的 `FILE`，保留（并引出第 689 条）。
     **常驻用例**：`test_std_device_error_indicator.cpp`（`StdDeviceErrorIndicator.KeptAfterAFailure`，子进程 stdout 指向 `/dev/full`，flush / 大块 write / 用户先失败三格，
     结果经退出码报回）。订正见第 691 条。**探针注意**：`> /dev/full | grep` 在 zsh 下被 MULTIOS 变成 tee、写不失败，须 `bash -c` 包。

685. **【已改 `e75dfa63`：删掉旁证】`out_impl.h` 引用的 glibc 退出冲刷「不取 FILE 锁」只对 glibc ≤ 2.37 成立（D-1）。** 2.38 起 `_IO_flush_all_lockp` 删除，`_IO_cleanup`
     改调逐 FILE 取锁的 `_IO_flush_all()`（agent 逐版核对 `libio/genops.c` 2.35-2.42，本机 2.42）。主控复跑：一线程 `flockfile(stdout)` 不放、主线程 `exit(0)` → `timeout 5` rc=124。
     本库「至多试三次」的取舍不受影响，只删了 `out_impl.h` 中英两处那句旁证。第 586 条同一说法的订正见第 691 条。

686. **【已改 `3fde4969`】`reset()` 说同步模式下只丢解码器状态，实际也丢已从 fd 取走的字节（A-1）。** 两类：格式化提取探分隔符时偷看的字符（`printf '12x34' | …`，
     `cin >> int` 得 12，`reset()` 后 `read(0)` 只得 `34`），以及 `wcin` 块读遇坏字节后第 668 条留在 `abs_cvt` 取数区里的后续字节。那句注释加于 `be650898`，早于第 668 条。
     **修法**：`in_impl.h` `reset()` 中英两处照实改写（行为正是 `reset()` 的用途，不改）。

687. **【已改 `3fde4969`：补文档，并把 `initial_locale_name` 移为自由函数】`switch_code("")` 与启动时对无效环境的处理不一致（A-2）。** `LC_ALL=xx_YY.bogus` 下 `wcin` 启动经
     `ori_facet_buf::resolve_locale` 的宽松回退静默用 `"C"`，`switch_code("")` 却交给 `newlocale` 硬失败、置 `cvtfailbit`；`@param` 列的优先级链与启动时相同，读者会以为结果也相同。
     **修法**：`in_impl.h`、`out_impl.h` 的 `switch_code` `@param` 写明这一差异，回到启动编码用 `switch_code(IOv2::initial_locale_name(LC_CTYPE))`。
     **顺带（用户定）**：`initial_locale_name` 原是 `locale<CharT>` 的静态成员，结果与字符类型无关，移到 `locale.h` 的 `namespace IOv2` 下作自由函数（中英文档），静态成员删除；
     `out_impl.h` 三个构造函数、`in_impl.h` `wcin` 构造函数、`test_locale_char.cpp` 随之改。
     **常驻用例**：`IoObjectsWchar.SwitchCodeReturnsToTheStartupEncodingThroughInitialLocaleName`（`run_stateful_child(..., "xx_YY.no-such-locale")`）。

688. **【已改 `78d60b7c`】`in_impl.h` 本地 include 组不按字母序（主控机械项）。** `IOv2/io/objects/out_impl.h` 排在 `IOv2/io/iochannel.h`、`iochannel_iterator.h` 之前；
     由 `2710a5e3`（头文件移到 `IOv2/` 下）引入，第二十三轮的「include 组内字母序」结论漏了它（订正见第 691 条）。移到 `iochannel_iterator.h` 之后。
     全库另有 4 个头不合逐字符字母序，随后一并改正（第 692 条，`42cc2bd4`）。

689. **【已修 `a892258e`】读写两用的 `file_device` 写失败后，读到文件尾被误报成读错误，读到的字节也丢了（主控处理第 684 条时发现）。** `dget` 以「读得不足量且 `ferror`」判读错误，
     而错误标志整个 `FILE` 共用：`basic_file_device<true, true, …>` 写失败后标志一直置着（`dput` 不清、`fseek` 也不清），之后一次正常读到文件尾的 `fread` 就被当成读错误；
     `fread` 已经交出的字节、已前移的文件位置随异常一起丢掉。只读设备碰不到。探针：`RLIMIT_FSIZE=5` 下写 10000 字节 → `partial write`，恢复限制、`dseek(0)`、`dget(buf, 100)` 抛
     `read error`，再读得 0 字节，而文件实有 `xxxxx`。
     **修法（用户定，两项一起）**：`dget` 在 `fread` 之前若 `ferror` 则 `clearerr`（`FILE` 是设备私有的，清它不影响别人，EOF 标志不动，glibc 2.28 起 EOF 粘住的行为照旧）；
     读出错时已读到部分数据就先返回 `count`，错误留给下一次。`dget` 的中英文档同步。
     **常驻用例**：`test_file_device_stale_error.cpp`（`FileDeviceStaleError.AReadAfterAFailedWriteReachesTheEndCleanly`：失败的 `dput` 与失败的 `dflush` 之后都读得 `xxxxx`；
     `EofStaysStickyOnTheNormalPath`）。

### 不算新问题、可关闭的待办、已验证无问题

690. - **B-2 非同步模式下，退出钩子之后写入的字节静默丢失**：第二十八轮 agent D 已实测，判为 `objects.h:27-33` 点名的契约外形态（要求改用函数内静态量）；
       `atexit` 早于 IOv2 初始化登记即同一形态。只是第 677 条写得太简（已补）。agent 提的可选改进随后已做（第 692 条，`ce02942c`），非同步模式下也不再丢。探针 `.review/tmp/b/p2_afterhook.cpp`。
     - **关闭 §6「推迟报错（`m_get_failed`）在 `sync_with_stdio` 切换时丢失」**：流层只有 `read()` 走 `getn`，`getn` 拿到不足量即再调 `get`，错误当场抛出——
       `a1_takearea.cpp` 实测 `read()` 返回时已置 `cvtfailbit`，标志活不过一次流调用，流层碰不到。
     - **A**：异常掩码与已置位的状态位下 `sync_with_stdio` / `switch_code` / `reset()` 的行为与文档一致（`a5_mask.cpp`）；启动环境无效时 `wcin` 不抛、`code()` 为 `"C"`；
       从 `cin` 单例切片拷贝出的 `stdin_api` 能编译但读不出东西（`in_sentry` 只认 `cin_t&`），不会交出重复字节（`a4_copy_base.cpp`）；输入在字符中间到 EOF 后 `sync_with_stdio` 与第 679 条契约一致。
     - **B**：2048 字节根缓冲边界扫描（9 个偏移 × 16 个写入长度 × 同步 / 非同步 × unitbuf 开 / 关 × `cout` / `wcout`，文件与管道）逐字节正确（`p4_sweep.cpp`）；
       tainted 的 `wcout` 经 `switch_code` 恢复符合契约（`p5_taint.cpp`）；共享库模式无 B-2。
     - **C**：跨对象 TSan 压测（`tsan_mix.cpp`，clang，10 线程：入口函数正反切换、改写 tie、`reset`、`switch_code`、六流插入、`cin` / `wcin` 读）正控报 1 条、负腿 3/3 零告警；
       宽流的编码字节不会被兄弟流插进同一字符中间（`split.cpp`，最大 90 MB 输出零次）；失败态的 tie 目标被发起方的插入加 `strfailbit` 与 libstdc++ 一致（`std::cout` 实测 `0x5`）；
       输出侧 `sync_with_stdio` 无锁切 false、持锁切 true 的交错，入口函数部分成功时各流状态，取锁的查询函数被阻塞读拖住——读码确认属设计如此。
     - **D**：三文件 include 必要性（第 553 条判据）逐个核对无多余、无缺失；第二十八轮改动（`71d42254`、`418bc780`、`2d7416e4`）中英块对应、与代码一致；
       注释引用的函数、成员、行为逐个核对存在且一致；拼写扫描（英文重复词、常见误拼、中文叠字与重复标点）无命中。
     - **主控机械项**：三文件末尾换行齐、无行尾空白、无连续空行、无 Tab / CRLF；`objects.h`、`out_impl.h` 的 include 分组与字母序正确。

691. **checked.md 订正（本轮合并时已就地加注）**：第 677 条补上「非同步模式下『头文件之前』『另一 TU』的静态对象析构输出会丢失（契约外）」；第 594、608、609、631 条
     「`fflush` 失败后经 `clearerr` 清错」「`ferror` 被清成 0」在 `07925312` 后不再成立（第二次无事可报是因为 glibc 已丢弃缓冲，与 `clearerr` 无关）；
     第 586 条「glibc `_IO_cleanup` 不取 FILE 锁」只对 ≤ 2.37 成立；第二十三轮「include 组内字母序干净」漏了 `in_impl.h`（第 688 条）；
     第二十八轮第 680-681 条的分支已并入 main（`a16a2d13` + `d55c84fc`）。
     **教训**：(1) 这套测试全量编译要几十分钟，只编、只跑受影响的套件，全量以 CI 为准；(2) 用 fork 出的子进程做隔离要 exec，否则 CI 的 Valgrind memcheck 报泄漏；
     (3) 新增 `[[nodiscard]]` 之前先搜调用点里被丢弃的返回值（CI 是 `-Werror`）。

692. **第二十九轮存疑 7 条与待定 2 项的处置（2026-10-04，主控对当前代码重编探针复跑；修改均经用户在 VS Code diff 里确认）。**
     - **C-S1 已修 `27f50ca9`**：输入流已是同步模式时，入口 `IOv2::sync_with_stdio(true)` 仍要等另一线程阻塞着的 `cin >> x`（`stdin_api::sync_with_stdio` 先取锁再比较新旧值；
       `blk.cpp` 复跑等了 2680 ms）。改为取锁前无锁比较、相同即返回（`m_sync_with_stdio` 是原子量），并发下等价于排在正在进行的切换之前；文档补「与当前模式相同的调用不取锁」。
       输出侧不照搬：`stdout_api` 切 `true` 要在锁内把缓冲交给 stdio，提前返回会破坏「返回即已交给 stdio」；输出侧的等待 `objects.h` 已写明。
       **常驻用例**：`IoObjectsChar.SyncWithStdioToTheCurrentModeDoesNotWaitForABlockedRead`（读线程阻塞在空管道上，逐流调用与入口函数都须在喂入前返回；修前失败）。
     - **B-2 可选改进已做 `ce02942c`**（第 690 条）：`stdout_api` 新增 `flush_at_exit()`——先 `m_sync_with_stdio.store(true)` 再 `try_flush()`，六个退出钩子改调它。头文件模式下晚于钩子执行的
       `atexit` 函数与静态析构的插入在结束时交给 stdio，由 glibc 最后冲刷；`std::cout` 两种模式本就不丢，因为 libstdc++ 的 `ios_base::Init` 自 GCC 13 起在 `libstdc++.so` 里
       （本机 g++ 15.3 的目标文件已无 `__ioinit`），冲刷排在可执行文件的一切退出处理之后。两步都不抛；晚到插入按同步模式报告失败（默认置位，掩码武装时抛出）；钩子之后
       `synced_with_stdio()` 报 `true`，`out_impl.h` 文件头加 `@note` 写明。不能在钩子里调 `sync_with_stdio(true)`：它阻塞取锁，违背第 586 条。B-S1（detached 线程跨 `main` 写）一并收口。
       **常驻用例**：`test_io_objects_after_exit_hook.cpp`（子进程里以 `constructor(101)` 登记 `atexit`，`main` 后写 `clog`，两种模式 fd 2 都得 `main\nlate\n`；修前非同步只得 `main\n`）。
     - **A-S1 已改 `0353bfa0`**：`stdin_api::sync_with_stdio` 说半个字符随解码器带过去、「后续字节照样解对」，实际流层调用只有在输入于字符中间到 EOF 时才收着半个字符返回
       （`a6b.cpp`：写端先发 `a\xe4`、停 1 秒再发 `\xb8\xad`，同步模式 `read(2)` 阻塞到字符完整）；非同步模式 `read(1)` 把 `a\xe4` 一起读进来，`\xe4` 在取数区而不在解码器里，
       切到同步时按 `detach()` 契约丢弃，之后读 `\xb8\xad` 报 cvtfailbit。文档改为唯一可见的效果：EOF 截断后切换不改变「`clear()` 后仍 cvtfailbit、到不了 eof」（第 679 条 A-S1）。
     - **D-S1 已改 `0353bfa0`**：`reset()` 的 `@note`「实测五个标准流」（第 618 条只测了 `cout`/`wcout`/`wclog`/`cin`/`wcin`）补测到全部八个：`reset8.cpp` 替换 `operator new`
       让全部分配失败（正控确认已挂上）、fd 指向 `/dev/full`，八个流 × sync / nosync 均 0 次分配、不抛、状态位 0。测前须先 `fflush`，否则探针自己 `printf` 的字节冲向 `/dev/full`，
       `cout` 得 1 次分配、`0x10`——那是丢弃缓冲一步，与重新附接无关。四处改为「全部八个」。
     - **范围外的 include 顺序已改 `42cc2bd4`**（第 688 条顺带）：`common/prefix_tree.h`、`facet/timeio.h`、`io/io_manip.h` 的 `iochannel_defs.h` 挪到 `metafunctions.h` 之前；
       `io/utilities/istream_operators.h` 改为 `io_base.h` → `iochannel_iterator.h` → `traits/traits_base.h`（`_` 0x5F < `c` 0x63）。四个头各自单独编译 gcc / clang `-Werror` 通过，
       CI 参数的 clang-tidy 0 报告；只影响编译，未在本地跑全量。
     - **B-S1 关闭**：与 `exit()` 竞争的写入，`out_impl.h` 已写明退出刷新「尽力而为」，第 586 条常驻用例形态 b 锁住「不崩、不挂」；`ce02942c` 后晚到的字节也进 stdio。
     - **B-S2 关闭**：`dput_error` 取 `fwrite` 返回值、与 errno 无关，`EAGAIN` 与 `EFBIG` 在 glibc 同一分支，第 682 条按构造覆盖。实测「重复」在常见载体上本就打不到：根缓冲一批至多
       2048 字节，管道上 ≤ `PIPE_BUF`（4096）的写原子、AF_UNIX 流套接字也整批成败（`eagain2.cpp`、`eagain3.cpp`，修前修后都恰好一次）；≥ 1024 字节的直写一路部分写后丢其余
       （修前修后相同，插入失败语义）。按字节部分写的 TCP / tty 未能稳定复现（`eagain4.cpp` 被 TCP 后台发送扰乱，结果作废）。
     - **D-S2、C-S2 未改**（影响很小，见「已知但刻意不处理」）。
     - **验证**：gcc-sanitizer `io.objects`（45 个用例）、`io.objects.sync_failure` 通过；gcc-debug 下这两个套件 Valgrind memcheck 0 错误 0 泄漏；两个新测试 clang `-fsyntax-only -Werror` 通过；
       两条修复的用例都对 HEAD 头文件反证失败。全量交给 CI。
     **教训**：(1) memcheck 的 `-R '^io\.objects'` 会连带同前缀、未编译的套件（`io.objects.sync_failure`），先确认目标都编了；(2) 同一文件混着两个主题时，从 HEAD 逐步套用生成中间版本再分开提交，
     并核对最后一步与确认过的版本逐字节一致。

## 第三十轮：`io/objects/*.h` 从 §6 待办出发，三条确认问题全部落在 `cvt/` 与退出钩子（main：`39bc5c3e` + `7c61bd94` + `b93fa6c1` + `9affb7c1`；异常类型整理 `e8c5d3e2`、第 700 条用例 `56483617`，均已并入 main）

基线 `main` = `d2a5c8a5`（2026-10-05）。三个 agent，范围互不重叠：A（general-purpose）`in_impl.h` 与只为它服务的下层读路径、B（engineering-code-reviewer）`out_impl.h` 与下层写路径、
C（claude）`objects.h`、`src/iov2_objects.cpp` 与跨对象并发 / 进程级生命周期。主控做机械项，并对 HEAD 头文件重编、复跑了全部 3 条确认问题的探针（B-1 另用 `ce02942c^` 头文件反证）。
确认 3 条，全部已修；修复过程中顺着讨论又做了两项重构（第 696、697 条）。存疑 4 条：S-1 随 B-1 修掉，S-3 随第 695 条失去前提，S-2、S-4 判为不改（见「已知但刻意不处理」）。
`include/` 的改动经用户在编辑器 diff 里逐块确认。探针索引见附录「第三十轮」。

693. **【已修 `39bc5c3e`】退出钩子在锁外置同步标志，之后的 `sync_with_stdio(true)` 跳过搬运，缓冲里的字节永久丢失（B-1，`ce02942c` 引入的回归）；存疑 S-1 一并修掉。**
     `flush_at_exit()` 先 `m_sync_with_stdio.store(true)` 再 `try_flush()`；后者在流失败态（流级 `flush()` 直接放弃）或锁被占（`try_lock` 三次失败）时什么也不搬，
     而 `sync_with_stdio(true)` 以 `exchange(true)` 的旧值为真作「已搬过」的证据提前返回——晚于钩子的 `clear(); sync_with_stdio(true)` 返回 `true`、`pre` 永久丢失
     （`p1b_single.cpp`：HEAD 空 / `ret=1`，`ce02942c^` 得 `pre` / `ret=0`；两线程形态 `p1_sync_after_hook.cpp` 5/5）。S-1：钩子之后再 `sync_with_stdio(false)`，此后插入同样丢（`b1_late_nosync.cpp`）。
     **修法**：钩子改置每流一个的 `copyable_atomic<bool> m_exited`（钩子不取锁写它，故须原子；放全局要导出、改漏宏自检判据，不值），哨兵按 `m_sync_with_stdio || m_exited` 推送；
     `m_sync_with_stdio` 重新只由 `sync_with_stdio` 在锁内伴随搬运翻真，第 692 条 C-S1 的论证随之重新成立。`synced_with_stdio()` 与 `sync_with_stdio()` 的返回值**不改**，
     仍报用户设定的模式（用户 2026-10-05 拍板；曾考虑改报 `||`，被否）。讨论中否掉的两个替代：哨兵析构时读标志（修不掉 B-1，且改写既有契约）、钩子里 `clear()`+清掩码（阻塞取锁违背第 586 条，锁被占仍复现）。
     **常驻用例**：`test_io_objects_after_exit_hook.cpp` 的 `BytesTheHookLeftBehindGoOutOnALaterSwitchBack`、`SwitchingOffAfterTheHookDoesNotLoseLaterInsertions`，均对修前头文件反证失败。

694. **【已修 `7c61bd94`】`wcin.read()` 短读后设备出错，已解码、已写进用户缓冲的字符不计数、丢失（A-1）；同形问题在所有 `get_main` 里都有，改为 `abs_cvt` 统一处理。**
     `code_cvt::get_main` 一次 `get` 里循环调 `get_buf`，第二次起下层抛出时异常穿出、`total_size` 丢失，`iochannel::getn` 的 `*got` 停在 0，违反其「抛出后 `*got` 仍准确」的契约
     （`a1_deverr.cpp` `--wrap=read`、`a1_pty.cpp` 真实 pty 挂断：`wcin gcount=0` 而 `cin gcount=4`）。libstdc++ 对照（真实挂断）：同步模式 `gcount=4` 但错误被当成 EOF，非同步模式 `gcount=0`、`badbit`。
     `vigenere_cvt` / `chacha20_cvt` / `zlib_cvt` 的 `get_main` 同形（读码确认，后由用例锁住）。讨论后定的修法（用户提出）：**`get_main` 改为 `noexcept`、返回 `std::pair<size_t, exception_ptr>`**，
     `abs_cvt::get` 以 `static_assert` 强制；计数为 0 当场抛，否则先交字符、存下错误、下一次 `get` 开头（taint 检查之前）重抛；有待报错误时 `is_eof()` 为 `false`，
     `switch_to_put()` 视同未读输入；重定位、`attach()`、`detach()` 丢弃它。`code_cvt` 的 `m_get_failed` 撤掉。附带行为变化：截断 / partial sequence / zlib 截断流 / chacha20 未对齐结尾
     在已交出字符时先交出、错误后报（zlib 截断交出前缀，用户确认）。曾讨论把修复放在设备层或 `get_buf`——都不行：设备每次调用本守规则，`get_buf` 不知道交出的**字符**数。
     **常驻用例**：`test/cvt/cvt_io/test_get_main_deferred_error.cpp`（四个转换器各一条 + `getn` 计数一条），对修前头文件 5/5 失败；`test_root_cvt_io.cpp` 的 4 个桩转换器迁到新签名。

695. **【已修 `b93fa6c1`】逐字符读遇到「收着 ≥ 2 字节后才判错」的坏序列，把收着的第 2 个起的字节一并丢掉；块读不丢（A-2）。修法是去掉 `m_pending`、改用回退。**
     glibc 对 GB18030（`81 30 41` → -2，再加 `62` → -1）、EUC-JP `0x8F` 段要到第 3、4 字节才判错；逐字符读时前几个字节已进内核的 `m_pending`，`-1` 分支整个清空，
     `a 81 30 41 b` 读成 `a [E] b`（块读 `a [E] 0 A b`），EUC-JP 丢「あ」（`b_ff.cpp`；`c_fuzz.cpp` GB18030 73/300 不同）。第 668、679 条只覆盖了「收着 1 个前导字节」。
     **修法**（用户提出：既然取数区已跨调用保留、错误可延后，`m_pending` 已无必要）：两个内核都不再收字节，`in_helper` 遇不完整或无效序列都让 `from` 停在序列开头；`get_main` 用读取器回退留住这些字节
     （带缓冲的根之上回退在根的缓冲里，否则在 `abs_cvt` 取数区）；无效序列在已交出字符时**不消费、不报**，留给下一次 `get` 自己撞上（数据引起的错误随数据走，重定位丢 `m_get_error` 才透明——写进 `get_main` 扩展点说明），
     一个没交出时跳首字节再抛。`is_mid_seq()`、`code_cvt::is_eof_impl` 删除；`abs_cvt::switch_to_put` 改为**先**把取数区还给下层、再调派生钩子（用户定，派生类不必知道缓冲）。
     行为变化：`switch_code` 在字符中间到 EOF 后不再被拒，余下字节按新编码解；存疑 S-3（设备错误也能留半个字符、文档只写 EOF）随之失去前提。
     **常驻用例**：`test_code_cvt_read_modes_agree.cpp`（三例 × 两种根 × 1/64 字符，及每编码 300 例随机差分；末尾补 4 个 ASCII，因 glibc 的 GB18030 要第 4 字节才判 -1），对修前头文件 4/4 失败；
     内核用例改为断言 `from` 停在序列开头。

696. **【已做 `9affb7c1`】输入流的 `sync_with_stdio` 改为只翻根上的一个标志，不再 `detach` 重建。**
     原实现按同步与否换根的**类型**（`no_rb_root_cvt` / `rb_root_cvt`），只能拆管线重建：非同步时已预读的输入随旧 iochannel 丢失（第十八轮 `in_01_sync_lookahead` 的 `" 22 33\n"`），
     重建失败会让流停在未附接状态，宽流还要 `code_cvt_stdio_state` 搬解码器。新增 `cvt/stdin_root_cvt.h`：`rb_root_cvt` 的派生，`m_synced` 经 `adjust(stdin_sync{...})` 翻转；
     `root_cvt::get`（改 deducing this）与带缓冲根的 `cvt_reader` 特化在补缓冲时以 `if constexpr` 询问可选钩子 `read_ahead(need, room)`，其它根零开销（用户否掉了「所有根加运行时变量」与「`variant` 换根」）。
     `iochannel` 新增接收任意现成转换器的 `base_channel(TCvt)`（用户否掉了带 creator 的第二个）。切到同步时缓冲里的预读字节先交出、交完才按需读（用户确认）。
     删 `code_cvt_stdio_state`、重建失败路径与 `io.objects.sync_failure` 套件及其 `--wrap` 链接选项（用户：不该多一个目标）。
     **常驻用例**：`io.objects` 的 `SwitchingToSynchronizedKeepsTheInputReadAhead`（`cin`、`wcin` 各一），对修前头文件均失败。

697. **【已做 `e8c5d3e2`】`facet/` 与 `locale/` 的异常类型按一条规则整理：流上数据失败抛 `stream_error`，其余一律 `io_error`。**
     起因是 `clocale_wrapper` 抛 `cvt_error`（当初只有 `code_cvt` 用它）。排查中发现构造期与误用错误在 `stream_error` / `std::runtime_error` / `cvt_error` 之间混用。改为 `io_error` 的：
     `clocale_wrapper`、`ctype_conf` 的 `wctype_l`、`collate_conf<char8_t>` 的 UTF-8 检查、`messages_conf` 的 `.mo` 加载与无可用语言、`timeio` 的 locale 数据检查、各 facet 的空 conf 指针、
     `locale::involve(nullptr)`、`locale_name` 不支持的类别、`collate::transform` 的 `n == 0` / 目标太小与 `strxfrm` 失败（`collate` 不被任何流使用）。误用不另立 `logic_error`，以免两种口径。
     `codecvt_kernel<char>` 构造时把 `io_error` 转回 `cvt_error`，`switch_code("bogus")` 仍置 `cvtfailbit`；`numeric` put/get 内部 `clocale_wrapper("C")` 失败（只可能是内存耗尽）改报 `otherfailbit`。
     对调用方可见：原来 `catch` `stream_error` / `cvt_error` / `std::runtime_error` 接这些错误的代码要改接 `io_error`。

698. **第三十轮验证后确认无问题。**
     - **A**：库内无 `readsome` / `in_avail` 入口，`deof()` 预读只在上层全空时发生（`a_pending.cpp`）；`C FF` 两种读法同为 `a[E][E]b`；`in_helper` 缓冲按 `MB_LEN_MAX` 定长（第 695 条后整个删除）；
       tty 挂断 `cin` 报 `devfailbit` 与文档一致；`27f50ca9` 无锁快速返回 clang TSan 正控 1 条、负腿 0 条，线性化 2000 轮。
     - **B**：钩子与并发插入 clang TSan 正控 2 条、负腿 0 条，9-16 万行序号连续无重复无缺口（`p2_tsan_exit.cpp`，数字要 `LC_ALL=C`）；`dput_buffer` 的 `written()` 算术窄流 105276 例、宽流 8208 例 0 失败；
       六个流的晚到插入、钩子后 `reset()` / `switch_code()` 正常；钩子两步不抛。
     - **C**：另一线程 `exit()` 的 clang TSan（header-only 与 `.so`，正控各 1 条，负腿各 30 次 0 告警不挂）；`27f50ca9` 后 `sync_error` 汇总仍准确；
       共享库模式 gcc 插件因 `STB_GNU_UNIQUE` 永不卸载，clang 下保留 tie 再 `dlclose` 会 SEGV（契约内，见存疑 S-4）；`.so` 的隐藏私有副本只被静态初始化器引用；退出钩子不碰 tie 目标。
     - **主控机械项**：三文件无行尾空白 / Tab / CRLF / 连续空行，末尾换行齐，include 分组与字母序正确。

699. **checked.md 订正**：第 675 条「`m_pending` 跨调用留存只在设备 EOF」——设备抛异常也会留下（第 695 条后 `m_pending` 已不存在）；第 690 条与第 677 条附近引用的 `objects.h:27-33` 当时已是 `:28-34`；
     `root_cvt.h:486-518`（`attach`）当时为 `:487-518`。第 692 条 C-S1「输出侧不照搬无锁早返回」的论证一度因 B-1 失效，第 693 条修后重新成立。
     **教训**：(1) 用 `head` 截断测试输出会漏看失败（第 697 条时漏了一条 `MessagesWchar`），汇总失败要看完整输出；(2) 随机差分的输入末尾要补足 `MB_LEN_MAX` 个 ASCII，
     否则 glibc 可能把最后几字节当作不完整序列，按约定永远报错；(3) 跨多个文件的接口改动（`get_main` 签名）在全部迁移完之前编译不过，要先说明再分步给 diff。

700. **复核「未覆盖的范围」时关闭的三项（2026-10-06）。**
     - **§1「`zoned_time` 的 `%z` get 侧把解析结果丢弃」**：已不成立。`timeio.h` `time_parse_context::convert_to(zoned_time&)`（现 `:1317` 起）在解析到 `%z` 时以偏移定瞬时、
       与解析到的区名交叉核验，矛盾即抛（第 697 条后为 `stream_error`——这是解析失败，规则内）。
     - **§1「`tz_level::offset` 档运行期零覆盖」**：已有运行期用例——`test_timeio_{char,wchar_t,char32_t}.cpp` 直接构造 `time_parse_context<…, tz_level::offset>`。
     - **§5「末尾的 `%z` 预读并置 `eofbit` 而提取成功」**：不是问题。`is >> get_time(&tm, "%z")` 读 `"+0800"` 得 `gmtoff=28800`、只置 `eofbit`；读 `"+0800 x"` 不置 `eofbit`——
       与任何提取在输入末尾的约定一致（`is >> int` 读 `"42"` 同样置 `eofbit`），调用方应查 `fail()` 而非 `good()`。libstdc++ 的 `std::get_time` 不支持 `%z`（两种输入都 `failbit`），无从对照。
     - **固化第 698 条 B 的一项探针结论**：设备在 `dput_error` 里多报 `written()`（超出这一批、乃至 `SIZE_MAX`）时，`root_cvt::dput_buffer` 先夹到批长再用，不越界、不保留、不重复写出。
       常驻用例 `RootCvtPartialWrite.AClaimBeyondTheBatchCountsAsTheWholeBatch`（`test_root_cvt_partial_write.cpp`，报 11 与 `SIZE_MAX` 两格，夹值是常数时间，不慢）；
       变异检查：去掉 `std::min` 夹值，ASan 当即报 `heap-buffer-overflow`。

## 第三十一轮：`io/objects/*.h` 只找「刚才没有提到」的问题（main：`fc47fd02` + `2642aa4e`）

基线 `main` = `56483617`（2026-10-06）。四个 agent，范围互不重叠、类型各异：A（general-purpose）`in_impl.h` 及第 696 条新加的 `stdin_root_cvt` / `read_ahead` / `base_channel(TCvt)`、
B（claude）`out_impl.h` 与写路径、C（Plan，只读）`objects.h` 与跨对象、D（Explore，只读）注释 / 拼写 / include / 空白。C、D 的结论由主控代写进 `.review/`。
主控复跑了全部确认问题的探针。确认 11 条（行为 3、文档 8），存疑 3 条（1 条修、1 条改文档、1 条判为不是问题）。机械项（include 分组与字母序、行尾空白、连续空行、末尾换行）全部干净。
`include/` 的改动经用户在编辑器 diff 里逐块确认。探针索引见附录「第三十一轮」。

701. **【已修 `fc47fd02`】公开的 `adjust(stdin_sync{...})` 绕过 `sync_with_stdio`，根上的标志与 `m_sync_with_stdio` 分叉（A-1）。**
     第 696 条把读法的真值放到 `stdin_root_cvt::m_synced`，`m_sync_with_stdio` 降为查询副本，二者只在 `sync_with_stdio` 里同改；但流继承了公开的 `adjust(const cvt_behavior&)`。
     `cin.adjust(stdin_sync{false})` 后 `synced_with_stdio()` 仍报 1、实际整块预读，随后 `sync_with_stdio(true)` 无锁早退返回 1、切不回去（`p2_adjust_bypass.cpp`：`raw_rest=[]`；对照 `[yz\nrest\n]`）。
     **修法**（三案中选 (b)）：`stdin_api` 隐藏继承的 `adjust`，遇 `stdin_sync` 转 `sync_with_stdio(s.synced)`，其余交基类。否掉的：(a) `stdin_sync` 构造私有 + `friend stdin_api`——cvt 层要认识 io 层，
     且误伤 `test_code_cvt_stdio_char.cpp` 等在 cvt 层直接 `adjust` 转换器的合法用法；(c) 以根上标志为唯一真值——`synced_with_stdio()` 要么取锁（读阻塞时跟着阻塞）、要么持一个随 `reset()` 悬空的指针。
     用户问「`sync_with_stdio` 调 `adjust`、`adjust` 又调 `sync_with_stdio`」：前者调的是 `m_channel.adjust`（iochannel），不是流自己的，不成环；第 215 行加了一句注释防将来改错。
     **常驻用例**：`AdjustingWithStdinSyncIsSyncWithStdio`（`char`、`wchar_t`），对修前头文件均失败。

702. **【已修 `fc47fd02`】同步模式下部分写失败留在本流缓冲里的字节，`clear()` 再 `sync_with_stdio(true)` 救不回来，而状态已是 `good()`（B-2 / D 存疑 S-2）。**
     `out_impl.h` 文件头（第 693 条写入）说钩子没交出的字节「之后 `clear()` 再 `sync_with_stdio(true)` 仍会交给 stdio」；但 `sync_with_stdio(true)` 以 `exchange(true)` 旧值为真直接返回，
     而同步模式下 `dput_buffer` 部分写失败同样保留 `[written, len)`（第 682 条）。`b3_hook_leftover_sync.cpp`（stderr 须为普通文件，`RLIMIT_FSIZE=5`）：同步 `01234`、`sync_ret=1`、`state_after=0`；
     非同步 `0123456789`；同步改用 `clear(); flush()` 也是 `0123456789`。同一根因让 `sync_with_stdio` 自己的保证在「先到的那次搬运失败」时也不成立。
     **修法**（用户选实现方案）：切到 `true` 时无论旧值都在锁内 `m_channel.flush()` 一次（空缓冲时什么也不做、不 `fflush`），返回值仍为旧值。代价：已同步时调用也可能重试失败的字节并再报一次位；
     宽流转换器已 tainted 时自动恢复会提前到这里（下一次插入本来也会做）。`objects.h` 四处「切回同步」随之改为「切到同步」。
     **常驻用例**：`BytesAFailedSynchronizedWriteLeftGoOutOnALaterSyncWithStdio`（`test_io_objects_after_exit_hook.cpp`，子进程设 `RLIMIT_FSIZE=5`，晚到 `atexit` 抬限制后恢复），对修前头文件得 `"01234"`。
     **注意**：主控第一次复跑这个探针时 stderr 指向终端，`RLIMIT_FSIZE` 不起作用，两种模式都输出全文，险些判为不成立。

703. **【已修 `fc47fd02`】`switch_code` 的返回值在并发切换时不可线性化（B-1 / A 存疑 S-1）。**
     `code()` 与 `adjust()` 各自取锁，中间无锁：初态 `C`，两线程同时切 X、Y，可能都返回 `C`（`b1_switch_ret.cpp`：2000 轮 474 轮）。只影响返回值，终态正确、无数据竞争（第 606 条）。
     **修法**：输入、输出两处 `switch_code` 开头取 `io_mutex()`（递归锁，内层再取无妨；仍是同一把锁，不添锁序）。
     **常驻用例**：`ConcurrentSwitchCodesEachReturnTheEncodingBeforeThem`（`wcout`、`wcin` 各 300 轮，`C` → `zh_CN.UTF-8` / `zh_CN.GBK`，约 35 ms），去锁后 78/300、69/300 失败。

704. **【已改文档 `fc47fd02`】`sync_with_stdio` 的「随时可调」没说会无限期等待阻塞着的读写（C-1）。**
     模式不同时输入流切换要取流锁，另一线程阻塞在 `read(0)` 的 `cin >> x` 持着它：全局 `sync_with_stdio(false)` 等了约 1.75 s，期间 `cout=0`、`wcin=1`（`c1.cpp`）。用户指出输出侧同样如此：
     输出流切到同步**总是**取锁（已同步也取），写满的管道上阻塞的插入让 `sync_with_stdio(true)` 等了 1690 ms，切到 `false` 0 ms（`c1out.cpp`）。
     **处置**（用户定只改注释）：`objects.h` 合成一段，写明何时取锁（输出流切到同步总是取；输入流模式不同才取；其余不取）、取锁即等另一线程正在进行的读写、阻塞在设备上时无上限、
     此时前面的流已切后面的未切；`in_impl.h`、`out_impl.h` 各补一句。不改实现：输入侧无锁需持根上原子标志的指针（随 `reset()` 悬空）且 `wcin` 的 taint 恢复必须在锁内；
     输出侧的锁是「返回时字节已在 stdio 手里」这一保证本身。

705. **【已改文档 `fc47fd02`】`dput_buffer` 的「失败的 put 一个字符没落地就不写出」只在单次 `root_cvt::put` 内成立（B-3）。**
     `code_cvt` 经根特化 `cvt_writer` 逐字符 `put_buf`，缓冲满时 `dput_buffer(buf_used, buf_used)`，本次上层 put 已编进根缓冲的前缀被算作「之前的」；窄流格式化插入由多次 put 组成，同理。
     `b4_writer_prefix.cpp`：直接根 put 尾部为空，经 `code_cvt` 写出 `xy`；`b5_stream_prefix.cpp`（流层，`head` 后限 4 字节插 3000 个 `x`）：`clog <<`、`wclog <<`、`wclog.write()` 恢复后写出 2043 个 `x`，
     只有 `clog.write()` 不写。**处置**（选 (a)）：`root_cvt.h` 中英注释把规则限定为单次根 put，写明上层分批与多次 put 的插入保留已进缓冲的前缀，与编码失败时 `code_cvt_stdio::recover()`
     写出前缀（第 585 条）一致；`out_impl.h` 文件头补「插入失败只保证置位，不保证一个字节也没写出」。否掉 (b)「根特化 writer 记批起点」：只修宽流单次 put、窄流 `<<` 照旧不一致，
     且与第 585 条相撞（编码失败写出前缀、设备失败丢前缀）；同步模式 ≥1024 字节的直写部分失败本来就写出前半（第 692 条 B-S2），流层能诚实承诺的只有置位。

706. **【已改文档 `fc47fd02`】第 696、697 条之后的文档漂移（A-2/A-3/A-4、D-1/D-2/D-3/D-4、存疑 S-3）。**
     - `in_impl.h` `reset()`：「同步模式没有读缓冲」改为「不往前多读」，丢弃的字节补上「此前非同步预读、切到同步后还没交出的」（可达一整块；`p1_basic.cpp e`：`line1\nline2\n` 读 1 字节后 11 字节全丢）；
       删掉「与 `sync_with_stdio()` 一样不要重入」（它已不丢输入）。
     - `in_impl.h` `sync_with_stdio` 的 `@warning`：「`read(buf, n)` 是一次 `read`」改为按需分次（`p3_readcount.cpp`：`cin.read(5000)` 为 2048/2048/904；`wcin` 读 300 个「中」19 次，递减）。
     - `stdin_root_cvt.h`：`read_ahead` 不只在缓冲空时问，剩余不够本次所需时也问（`root_cvt.h` 的 `cvt_reader` 先搬移再问），`room` 是尾部余量；一句缺主语的英文补上。
     - `code()` 失败置 `otherfailbit` 而非 `cvtfailbit`（四处）：移出状态下 `clocale_wrapper::name()` 抛 `io_error`，途中不转换。
     - `iochannel.h` `detach()` 的 `@warning` 与代码注释：「换缓冲的操作」的例子由 `sync_with_stdio` 改为 `reset()`（第 696 条后前者不再 detach，原句的建议与现状相反）。

707. **【已修 `2642aa4e`】多线程程序 fork 时另一线程持有某个流的锁，子进程里该流永久阻塞（C 存疑 S-1）；改在 `copyable_mutex` 里通用处理。**
     `fork()` 只带调用线程，别的线程持有的锁在子进程里永不释放。`cs1.cpp`：线程持 `IOv2::sync(cerr)` 时 fork，子进程 `cerr << ...` 挂住；同形下 `std::cerr` / `fprintf` 正常——glibc 在 fork 时重置 `FILE` 锁，
     不是标准保证（POSIX：多线程程序 fork 后 exec 前只许调异步信号安全函数）。用户指出「fork 后 exec 失败往 `cerr` 报错」是常见写法，只写文档不够。
     **修法**（用户提出做成通用组件）：`pthread_atfork` 的 child 回调只把进程级 fork 代数加一并重建 `refresh` 锁；每个 `copyable_mutex` 记自己见过的代数，`lock` / `try_lock` / `lock_shared` / `try_lock_shared`
     发现落后就在 `refresh` 下双检重建（`std::construct_at`，不析构旧对象——[basic.life] 允许，避开「析构被持有的 mutex」的 UB）。不用 prepare 回调：在 fork 前取全部锁会让 `fork()` 等阻塞着的 `cin` 读。
     只在 `__unix__` / `__APPLE__` 登记（Windows 无 fork，`CreateProcess` 不复制内存，问题不存在）。共享库模式下代数与登记由 `iov2_objects.cpp` 唯一定义（同 `tie_graph_mutex`）。
     顺带：`copyable_mutex` 以 `requires` 转发 `lock_shared` 一族；`tie_graph_mutex()`、`s_domain_mutex`、`ori_facet_buf::m_mutex`、`locale::m_facet_mutex` 四处裸锁换成 `copyable_mutex`
     （`s_domain_mutex` 所在 `base_ft<messages>` 是显式特化，inline 静态成员按定义序初始化，改为动态初始化无序问题）。
     **开销**：原型每对加锁 / 解锁 +0.6 ns（`recursive_mutex` 约 10 ns）；`cout << 'c'` / `<< int` / `<< "hello"` 前后差异在噪声内。
     **只救锁不修数据**、**持锁线程自己 fork 为 UB**两点写进类说明，后者的取舍见「已知但刻意不处理」。
     **常驻用例**：`test_copyable_mutex.cpp` 的共享形态转发、另一线程持独占锁（`mutex` / `recursive_mutex` / `shared_mutex`）与共享锁时 fork 子进程可用、父进程的锁不受影响；
     `io.objects` 的 `AChildForkedWhileAnotherThreadHoldsCerrCanStillWriteToIt`。裸 fork、子进程 `_exit`，只看 5 s 内管道回报、不看退出码（Valgrind 会改）；去掉重建三条均超时失败。

708. **D 存疑 S-1 判为不是问题**：`objects.h:9-10` / `:42`「调用时机见 `stdin_api::sync_with_stdio`」——指过去仍能找到「什么时候可调」的答案（随时可调 + 第 704 条补的等待说明），未改。

709. **第三十一轮验证后确认无问题。**
     - **A**：`reset()` 保留同步模式，`stdin_root_cvt` 拷贝被删、移动 `noexcept` 且带走 `m_synced`（`p1 b`、`p5_copymove.cpp`）；非同步预读后切同步、缓冲半满、末尾停在多字节字符中间，只补缺的字节
       （`p1 a/c`）；同步模式不多读（`p1 d`、`p3`）；切换与读并发 clang TSan `cin` 20 万字节、`wcin` 10 万字符各 3 次 0 告警、阳性对照 1 条（`p4_tsan_toggle.cpp`）；
       `to_max == 0`、`deof()` 预读、回退时设备出错均不可达或一致；`base_channel(TCvt)` 不劫持拷贝构造、不与按设备构造二义。
     - **B**：钩子之后的组合（`reset()`、`sync_with_stdio` 往返、`cerr` unitbuf + tie、`switch_code` 后宽字符插入）顺序与状态全对（`b2_late_combo.cpp`）；`m_exited` 不会被拷贝出第二份（`stdout_api` 不可拷贝）；
       所有写通道的路径都经哨兵；`try_flush` 在 tie 目标掩码武装时的落点与文档一致；`close_stream()` 对 tainted 的无状态转换器不报假错；
       钩子与 `wcout.switch_code` + 插入并发 clang TSan 同步 / 非同步各 3/3 次 0 告警、阳性对照各 1 条（`b6_tsan_hook_switch.cpp`）；`code_cvt_stdio::adjust` 提交后无可抛步骤。
     - **C**（读码）：入口函数不嵌套取锁；`cin_err` 恒空；栈展开期调用不 terminate；`.so` / 头文件模式初始化顺序与 `tie_graph_mutex` 退出期可用；钩子之间交错不丢字节。
     - **D**：拼写、第三十轮残留词、退出钩子文字、中英对照、交叉引用、include 必要性（集合差）。

710. **本轮方法记录与教训。**
     (1) 探针的前提要照搬：`b3_hook_leftover_sync` 要求 stderr 是普通文件，主控第一次复跑时指向终端，结果「不复现」——复跑与 agent 不一致时先核对运行条件（第 702 条）。
     (2) glibc 的递归锁按 TID 记持有者：持锁线程 fork 后，子进程里同一线程（`get_id()` 不变）`try_lock` 失败、再加锁永久阻塞（`forkown.cpp`）。所以「持锁期间 fork」连 `std::recursive_mutex` 都不保证，
     想用 `try_lock` 判断持有者的省事办法行不通；要确定行为只能自己记持有者。
     (3) 开销要拆开测：记持有者 +3.5 ns 里大头是 `get_id()` 经 PLT 调 `pthread_self()`；换 `__builtin_thread_pointer()` 只省 0.5-1 ns，剩下是多出的读写本身。噪声大时绑核取 9 次最小值。
     (4) 本机 scratchpad 会被清掉（第三十一轮中途基准目录消失），要留的探针放 `.review/tmp/`。

711. **第三十一轮评估后不做的方向（防重复调研）。**
     - A-1 形态对全局 `sync_with_stdio()` 复跑：全局函数逐个调各流的 `sync_with_stdio`，第 701 条后 `adjust(stdin_sync)` 也走它，不再有分叉路径。
     - GB18030 / EUC-JP 下同步 `wcin` 逐字符读的 `read` 次数：文档已改为通用说法，无结论依赖具体次数。
     - 真实 tty / pty 上的切换：切换只翻标志，与设备无关；tty 的「`read` 返回少于所求」管道已覆盖。
     - B-3 方案 (b) 原型：已选 (a)，(b) 与第 585 条相撞（第 705 条）。
     - 用真实 `EAGAIN` / `ENOSPC` 复现第 702、705 条：第 692 条已论证 glibc 走同一分支（`fwrite` 短计数，errno 不同），本库不看 errno。

## 第三十二轮：`io/objects/*.h` 从 §6 待办出发，挖出一条跨进 `locale/` 的插件卸载崩溃（已并入 main：`f583fb76` + `16bc71c9` + `27750065`）

基线 `main` = `2642aa4e`（2026-10-08）。四个 agent，范围互不重叠、类型各异：A（general-purpose）`in_impl.h`、B（engineering-code-reviewer）`out_impl.h`、
C（claude）`objects.h` + `src/iov2_objects.cpp` + 跨对象生命周期、D（security-appsec-engineer）第 707 条 fork 修复作用在八个流上的进程级行为。
确认 2 条，存疑 5 条（4 条修、1 条判为不改）。主控复跑了两条确认问题的探针，并在用户修改后逐条复核（第 719 条）。探针索引见附录「第三十二轮」。

712. **【已修 `16bc71c9`】共享库模式下插件卸载后，进程级流对象里的 facet 缓存悬空：插件只写过一次 `cout << 42`，宿主随后的 `cout << 1` 段错误（C-1）。**
     插件按 README 契约构建（`-DIOV2_SHARED`、链接 `libiov2.so`）。插件里内联展开的 `operator<<` → `locale<char>::get<numeric<char>>`（`locale.h:1023-1040` 未命中）
     → 插件内实例化的 `ft_wrapper` 构建 facet，键取插件的 `typeid`（`facet_common.h:112-120`）→ 写进 **`libiov2.so` 里 `cout` 对象的** `m_facets`。
     facet 的 vtable、`type_info` 键、`make_shared` 控制块都在插件映像里；`dlclose` 后宿主 `m_facets.find` → `type_index::operator==` 读已 unmap 的 `__name`。
     另两种形态（插件先建 `de_DE` facet、插件里 `cout.locale(fr_FR)`）经 `ori_facet_buf::try_get`（`ori_facet_buf.h:196-224`）死在 `__dynamic_cast` ← `locale.h:988`。
     clang 插件与 g++ `-fno-gnu-unique` 插件 5/5 次 rc=139；g++ 默认插件因 STB_GNU_UNIQUE 永不卸载，把问题盖住了（第三十轮第 698 条就是这样没碰到）。
     原有 dlclose 文档只覆盖 `libiov2.so` 自身被卸载与用户放在插件里的 tie 目标（第三十轮 S-4），都不覆盖这一形态。对照 std：插件里 `std::cout << 42` 不往 libstdc++ 的全局状态里存插件代码段的对象。
     **修法**（用户在四案中选定）：不逐个插入点处理，而是每个模块在**加载时**把自己钉成不可卸载。共享模式下 `iov2_export.h` 给每个消费者模块一个 **hidden** 的 inline `module_pin`，
     构造时调 `detail::pin_module_of(this)`；`iov2_objects.cpp` 的定义 `dladdr` 找到模块，再 `dlopen(RTLD_LAZY | RTLD_NOLOAD | RTLD_NODELETE)`，失败静默忽略；只在 `__unix__` / `__APPLE__`。
     必须 hidden：默认可见性会跨模块合并（gcc 还发成 STB_GNU_UNIQUE），只钉住第一个模块。触发不能放进 cpp：`.so` 里的代码只认得 `.so` 自己，插件用 `cout` 是数据访问，
     插入缓存的又是插件里实例化的头文件模板；glibc 也没有「新模块加载」回调（`LD_AUDIT` 要环境变量）。
     **顺带**：第三十轮 S-4（插件里永不析构的 tie 目标）随之消失；被钉住的模块一直握着对 `libiov2.so` 的依赖，`iov2_objects.cpp` 原 `@warning` 的「libiov2.so 被 dlclose」也不再 unmap。
     **代价**（写进 README 与 `iov2_export.h`）：这类插件不能热重载、内存不回收、静态析构推迟到进程退出。`libiov2.so` 加 `-ldl`。
     **常驻用例**：`io.objects.plugin_unload`（只在 installed-shared 消费者下构建插件，g++ 加 `-fno-gnu-unique`）：插件写 42、`dlclose`、断言仍驻留、宿主 `cout << 1` 得 `"421"`。

713. **【已修 `f583fb76`】`stdout_api` / `stdin_api` 的隐式移动构造可用：`std::move(IOv2::cout)` 切片移动掏空单例，绕过 `= delete` 的 `detach()`（B 存疑 S-2，主控补测输入侧）。**
     派生类 `cout_t` 等只删了自己的拷贝，xvalue 仍绑到基类隐式的 `stdout_api(stdout_api&&)`；`stdout_api<cout_t, …> b = std::move(IOv2::cout);` 不用强转即编译并移走转换器。
     之后 `cout.reset()` / `cin.reset()` 经 `runtime_cvt::detach`（`runtime_cvt.h:602`）`std::terminate`（rc=134）；`cerr.tie(&b)` 后 `cerr <<` 经 `ostream_operators.h:413` 把 `b` 当 `cout_t`（UBSan vptr）。
     拷贝那条路能编译、运行期 `clone()` 抛出，无害。列为确认是因为本库特意删了 `detach()`/`attach()`，移动构造是同一件事的无声旁路。
     **订正**：第 709 条 B 的「`stdout_api` 不可拷贝」在类型层面不成立；第二十八轮对 `cin` 切片**拷贝**的判断未考虑移动。
     **修法**：两个基类显式删除拷贝构造与拷贝赋值（隐式移动随之不再声明）。**常驻用例**：`static_assert` 两个基类既不可拷贝也不可移动。

714. **【已修 `f583fb76`】`stdin` 的同步状态仍有两条绕路（A 存疑 S-1、S-2）。**
     (1) 限定名 `cin.stream_common_operators::adjust(stdin_sync{false})`：`self` 推导为 `cin_t&`、基类是友元，第 701 条的名字隐藏拦不住，标志再次分叉；
     (2) 同时派生自 `stdin_sync` 与另一策略的组合类型，经 `stdin_api::adjust` 只执行同步切换，另一半被丢掉。
     **修法**（用户实现，比两案都彻底）：真值只在根上，`stdin_root_cvt` 每次翻转都通知 `stdin_sync_listener`（`stdin_api` 私有继承它、更新查询副本），
     无论 `stdin_sync` 从哪条路径来；第 701 条的隐藏 `adjust` 删掉；`stdin_sync` 改 `final`。`stdin_root_cvt` 删拷贝，移动构造带走监听者、移动赋值保留自己的。
     复核：`switch_code` 只换 `m_cvt_kernel`、`reset()` 走 `root_cvt::attach`，根对象都不换，监听者不会丢。**常驻用例**：`AdjustingWithStdinSyncIsSyncWithStdio` 补限定名调用一段。

715. **【已修 `f583fb76`】钩子运行时正在进行的非同步插入，连同之前积压的字节全部丢失（B 存疑 S-3）。**
     哨兵只在构造时读一次 `m_sync_with_stdio || m_exited`：非同步模式下钩子取不到锁放弃，持锁线程的插入即使在钩子之后、glibc 最后冲刷之前写完，也按「非同步」收尾
     （`p1_hook_giveup` 的 `u` 模式输出为空，同步 `s` 为 `head|AB`）。**修法**：哨兵改在析构时读 `m_exited`（`ostream_operators.h:196-202`，`m_exited` 唯一读点）；成员注释同步。
     修后 `u` 为 `head|AB`，其余四种不变。**常驻用例**：`AnInsertionInProgressWhileTheHookRunsStillReachesTheDevice`。

716. **【已修 `27750065`】CI 的 Valgrind 那一腿对 fork 出的子进程没有判别力（D 存疑 S-1）。**
     `ctest -T memcheck` 退出码为 0，却报 `Defects: 15 + 3`，全部是子进程 `_exit` 时的泄漏；缺陷计数不影响退出码、用例又不看子进程退出码，子进程里真出现非法访问 CI 也是绿的。
     **修法**（用户实现，未用 D 提的 `VALGRIND_COUNT_ERRORS`）：子进程改 exec `/bin/true` 结束（exec 不做退出期泄漏检查，访问错误在发生时已报出）；CI 另查本次 `DynamicAnalysis.xml` 有无 `<Defect type=`。
     本地 `ctest -T memcheck -R '^(common|io\.objects)$'`：rc=0，Defect 0。

717. **D 存疑 S-2 判为不改（用户 2026-10-08）**：`copyable_mutex::rebuild` 的 `construct_at` 落在 const 完整对象的 `mutable` 成员上（如 `const IOv2::locale`），按 [basic.life]/12 字面是 UB。
     处置与理由见「已知但刻意不处理」。

718. **第三十二轮验证后确认无问题。**
     - **A（`in_impl.h`）**：关闭 §6「钩子与 `wcin` 读并发的 TSan 腿」——sync/nosync × get/fmt 每格 3 次 12/12 次 0 告警、阳性对照 12/12 次恰 1 条，「输入在退出那一刻才到」与「阻塞在 `read(0)`」变体库内 0 告警（B、C 各用独立探针复核）；
       用户从 `stdin_sync` 派生的类型正确走到 `sync_with_stdio`；继承链逐个核对，除第 714 条 (1) 外无入口把 `stdin_sync` 直送到根；第 696/701 条后从 `sread` 重入 `sync_with_stdio` / `adjust` / `reset()` / `switch_code()` ASan+UBSan 干净
       （此前的重入实测是旧「重建 iochannel」设计下做的）；`wcin` 先 `switch_code` 再 `reset()` 编码与模式都保留；`wcin` 读 + 切换 + `switch_code` + `reset()` 并发 TSan 3/3 次 0 告警；字符中间遇 EOF 与 `reset()` 注释一致。
     - **B（`out_impl.h`）**：§6「钩子取不到锁时持锁线程的最后一次插入」做了确定性探针（五种模式，修后见第 715 条）；第 702 条组合（插入内部调用、tainted、掩码武装、`/dev/full`）单流都对；
       第 703 条 `switch_code` 递归锁无冲突（插入中途切编码字节正确、坏名字抛出后锁释放）；第 690 条「输出侧 false/true 交错」改为 TSan 实测 3/3 次 0 告警；钩子处掩码武装 + 写失败不 terminate；7 种怪异 `LC_ALL` 下 `wcout` 构造不 abort；
       tainted 的 `wcout` 上 `reset()` 与文档一致；`tell`/`seek`/`rseek` 按文档失败、不丢缓冲。
     - **C（`objects.h` 与进程级）**：第 709 条 C 部分原全是读码，本轮全部实测——入口函数不嵌套取锁（插桩 `pthread_mutex_*` 六种条件 `max_distinct=0`、阳性对照 2；TSan 单/双线程调用各 3/3 次 0 告警）；
       `cin_err` 恒空（三种 stdin 各 12 次 `sync_error`）；栈展开期调用不 terminate（gcc/clang，`uncaught` = 1/0/1/2 四形态）；`copyable_mutex<mutex|recursive_mutex>` 平凡可析构、三种构建 × 两态四条 tie 边与字节全对；
       钩子之间交错 14 种组合字节精确；`objects.h:198-200`「六个输出流一定切换」成立；§6「`-fno-gnu-unique` 插件 `dlclose` + tie」tie 本身无问题（牵出第 712 条）。
     - **D（fork 修复）**：关闭 §6 第三十一轮「交给 CI」一项——共享库模式 fork 代数只有一份（`.so` 内 `b`、函数 `T` 导出、内部调用走 `JUMP_SLOT`），头文件模式两 TU 共用一份；六种 fork 形态（cerr、wcout、tie 链三锁、`refresh` 被持有、孙进程、子进程多线程）全 OK；
       Valgrind 下真跑子进程 0 条非泄漏错误；ASan/UBSan 干净；去掉重建的 shadow 头作阳性对照三形态挂到 alarm；子进程 `exit()` 钩子穿过别的线程持有的 `cout` 锁；`posix_spawn`/`vfork`/`system` 不改代数、`_Fork` 照旧挂住（非回归）；
       `refresh` 被持有时 fork、fork 恰在双检中间都正确；阻塞读时 fork 四格正确；`construct_at` 对 libstdc++ 三种 mutex 是透明替换。**子进程一侧 TSan 是瞎的**（故意造的竞争也不报），不能作 fork 后的证据。

719. **修复复核（主控，用户工作区改动提交前）。**
     第 712 条：installed-shared 下 `io.objects.plugin_unload` 通过；C 的插件形态复跑——clang 与 g++ `-fno-gnu-unique` 插件 × `coutnum`/`facet`/`coutloc`/`tie` 八格 rc=0、仍驻留，`-U__unix__` 编的不钉插件三形态 rc=139（阳性对照）；
     第三十轮 S-4 形态修后 rc=0、对照 rc=139。**落实了动手前的头号疑点**：glibc 2.42 上插件在自己的构造期内经 `.so` 做 `RTLD_NODELETE` 生效。
     限定名 `detach()` 新文档（第 713 条同提交）：四个流 detach 后输入侧 `cvtfailbit`、输出侧 `cvtfailbit|strfailbit`（插入短写另加），`reset()` 后恢复。
     回归：gcc-release 20 个受影响套件全过；clang 21 `-Werror` 对改过的五个测试与共享模式 `objects.h` 干净。全套件与 clang 全量交给 CI。
     **checked.md 引用漂移**（本轮核出）：第 701 条「第 215 行注释」→ `in_impl.h:220`（该注释已随第 714 条删除）；第 622 条 `in_impl.h:88` → `:99-100`，且已约束为 `std_device<STDIN_FILENO>`，前提对输入侧不再成立；
     第三十轮 S-2 `out_impl.h:42` → `:40-47`；S-4 `stream_common_operators.h:707-711` → `:708-712`；第三十一轮探针 p1-p5 已随 `.review/` 删除，A 的 `a5_tsan_mix.cpp` 重新覆盖了 p4 的形态。

720. **本轮方法记录、教训与评估后不做的方向。**
     (1) g++ 默认构建的插件因 STB_GNU_UNIQUE 永不卸载，**测 dlclose 一律要配 clang 或 `-fno-gnu-unique`**，并用 `dlopen(RTLD_NOLOAD)` 核实真的卸载了——第三十轮正是因此漏掉第 712 条。
     (2) installed-shared 预设拒绝仓库内的前缀（`IOv2TestOptions.cmake` 的守卫），本地复跑要把 `make install-shared PREFIX=` 指到仓库外。
     (3) `pkill -f <模式>` 会匹配到发出它的那条 shell 命令本身并把它杀掉（rc=144）；本机 zsh 不拆分 `$VAR`，多参数的编译配方放进 bash 脚本。
     (4) 第 712 条否掉的修法：逐个插入点 `dladdr`（要审计全部存放点，facet / `type_info` / 控制块三者各要钉）；`extern template` + 导出工厂（只覆盖库自带 facet，漏一个实例化即静默退回）；
     只写文档（违约即段错误）；`iov2-shared.pc` 的 `Libs:` 加 `-Wl,-z,nodelete`（只覆盖用 pkg-config 链接的模块，Windows 链接器无对应选项、macOS ld64 不认）。
     (5) 第 716 条未采用 `VALGRIND_COUNT_ERRORS` 回传：本机无 valgrind 头文件，exec 方案无需依赖。

## 第三十三轮：`io/objects/*.h` 从 §6 待办出发，一条确认落在 `src/` 的模块钉住，两条存疑都是 `0e2f4a6d` 留下的契约缺口（分支 `fix/io-objects-round33`：`61ea4e4c` + `c4c88748` + `dc67860f`）

基线 `main` = `3da40b44`（2026-10-09），重点是 §6 第三十二轮的待办与第三十二轮之后未经审查的 `0e2f4a6d`（标准流的 detach/attach/adjust 改靠访问控制、删 `stdin_sync_listener`、
tie 环检测改经虚函数 `tied_to()`）、`16235eb5`（`abs_flusher` → `tie_target` 改名）、`8e8262da`（clang-tidy）。三个 agent，按文件分、类型各异：A（engineering-code-reviewer）`in_impl.h`、
B（security-appsec-engineer）`out_impl.h` + tie 图、C（general-purpose）`objects.h` 与进程级生命周期。确认 1 条，存疑 2 条，主控均已复现。探针索引见附录「第三十三轮」。

721. **【已修 `61ea4e4c`】共享模式下主程序自己的 `module_pin` 拿 `argv[0]` 去 `dlopen`：`argv[0]` 指向 FIFO 时进程卡在 `main` 之前，为 `/dev/stdin` 时读走 stdin 开头，经 PATH 启动时留下待取的 `dlerror()`（C-1）。**
     位置 `src/iov2_objects.cpp` 的 `pin_module_of`，触发点 `common/iov2_export.h` 的 `s_module_pin`（每个共享模式消费者模块都有，**含主程序**）。glibc 的 `dladdr` 对主程序（`l_name == ""`）把 `dli_fname` 换成 `_dl_argv[0]`，
     于是 `dlopen(argv[0], RTLD_LAZY|RTLD_NOLOAD|RTLD_NODELETE)` 按 exec 调用方给的字符串搜索、`open()`、读 ELF 头；对主程序它永远不成功（主程序本来也卸载不了），只留副作用。
     实测（g++ 15、glibc 2.42）：`exec -a /dev/stdin` 下 `main` 里 `read(0)` 得 0 字节；`exec -a <FIFO>` rc=124；`exec -a <无斜杠名>` 后首个 `dlerror()` 得 `cannot open shared object file`（任何一次成功的 dl 调用都会清掉它）。
     **修法**：glibc 上改用 `dladdr1(…, RTLD_DL_LINKMAP)` 取 `link_map`，`l_name` 为空（主程序）即返回，其余按 `l_name` `dlopen`（加载器按名匹配，不碰文件）；非 glibc 仍走 `dladdr`（macOS 返回映像真实路径）；`dlopen` 失败后一律 `dlerror()` 清掉残留。
     **常驻用例**：`PluginUnload.TheMainProgramsPinLeavesArgv0Alone`（以 `argv[0]="/dev/stdin"`、管道喂 `hello` 重执行自身，子进程须读全）：修复前的 `libiov2.so` 失败（读到 0 字节）、修复后通过；
     原插件用例仍过（非主程序模块照样被钉住）。FIFO 与 PATH 启动两格在新旧两个库上手工复跑。installed-shared（gcc）62 个套件全过；`src/` 与新用例 g++ / clang `-Wall -Wextra -Werror` 干净。

722. **【已处置 `c4c88748`】`tie_target::tied_to()` 在持有进程级 `tie_graph_mutex()` 时执行覆写，注释对覆写者一字未提（B 存疑 S1）。**
     `0e2f4a6d` 之前 `check_tie` 持图锁时只做 `dynamic_cast` 与原子读；之后每一步都虚调 `tied_to()`。覆写里调任何 `tie()` setter（含 `tie(nullptr)`）即同线程二次锁非递归 `std::mutex`（UB），
     实测挂住（`timeout 5` → rc=124），此后全进程的 `tie()` 都挂住；覆写里取流锁会引入 `tie()` 注释刻意避免的 graph → io 锁序边（只有读码）。
     **用户决定（2026-10-09）**：派生 `tie_target` 不对应任何合理用法，**只写契约、不加机械限制**。`tie_target` 加 `@warning`「用户不得派生」（中英两块，点名两个虚函数各自的隐藏约束）；
     `tied_to()` 注释改为「在持有 `tie_graph_mutex()` 时被调用」，不再把裸 `tie_target` 说成可用的目标，`check_tie` 的 `@param` 与行内注释同改；`out_tie_target` 的 `try_flush()` / `tied_to()` 改为 `final`，
     派生自流的类（受支持的用法）也换不掉它们。库内测试仍可派生（测试代码属于本库），`test_ostream_derive.cpp` 的替身注明「仅测试用」，并删掉「环检测 `dynamic_cast`」的过时说法。
     撤回 `16235eb5` 提交说明里「构造函数 `protected` 是为了让用户派生」的说法。CI 那套 clang-tidy 对改过的两个头与 `ostream.h` 无报错（`stream_common_operators.h:890` 一条 `modernize-use-scoped-lock` 是旧的、非 bugprone）；
     `out_tie_target` 是模板，`final` 只在实例化时检查，故对 `ostream_derive`、`istream_ws`、`io.objects` 三个测试做了 `g++ -fsyntax-only -Wall -Werror`。按用户意见未跑全量。

723. **【已处置 `dc67860f`】`sync_with_stdio` 依赖「根翻转之后不再有可失败的步骤」，`0e2f4a6d` 却删了写明这一依赖的那句文档；监听器去掉后违约的后果从「无害」变成「永久失配」（A 存疑 S-1）。**
     `in_impl.h` 的 `sync_with_stdio` 在 `m_channel.adjust(stdin_sync{sync})` 正常返回后才 `store`。若某层先把行为传给下层、之后才抛，根已切换而查询值未变；此后 `sync_with_stdio(原值)` 早退，永远修不回来。
     监听器时代根一翻转就通知本流，同样的异常不造成失配。库自带各层（`abs_cvt::adjust` 先 `adjust_impl` 后下传、`code_cvt_stdio::adjust` 先恢复后下传、根的 `BT::adjust` 为空）都满足顺序，cin/wcin 不可达。
     实测（自定义 `faulty_root::adjust` 先调根再抛，gcc 与 clang）：返回 1、badbit、`synced_with_stdio()==1`，随后 `sync_with_stdio(true)` 早退，读 1 字符后 fd0 被读空。
     **处置**（方案 (a)）：把依赖说明写回文档（中英两块），按监听器已删的现状改写，并写明违约后的失配不会自行恢复。未采用方案 (b)（catch 里经新 `cvt_status` 向根查询真实标志）。

724. **第三十三轮验证后确认无问题。**
     - **A（`in_impl.h`）**：关闭 §6「`wcin` 切换失败时保持原模式」——读码逐个核对输入侧能置 taint 的点（`abs_cvt.h` 的 attach / bos / main_cont_beg / put / flush 与 `set_tainted`），库自带 wcin 管线上都抛不出来，
       `sync_with_stdio` 的失败分支对真实 cin/wcin **不可达**；注入实测（`faulty_root::attach` 让 `reset()` 置 taint、恢复再失败）：返回 1、查询值不变、badbit，fd0 剩 "bcdef"（仍逐字节读），阳性对照剩 ""。
       关闭 §6「`read_ahead` 在 `need==0`」——两个调用点都有守卫，影子头插桩（`need==0` 即 abort）下 cin/wcin × 同步/不同步跑 `read(0)`/`read(-1)`/`get(buf,0/1)`/`ignore(0)` 等全部 exit 0，计数器同步 7 次、不同步 1 次。
       删监听器后第 714 条两条绕路未复活：限定名 adjust/detach、转基类引用、取成员指针、访问 `m_channel`、调 `adjust_impl`、wcin 同类写法 gcc 15 / clang 21 一致被拒，`stdin_sync` 仍 `final`；`reset`/`switch_code`/`reset` 后查询值与根的读法四格一致。
       wcin 在 9 种怪异 `LC_ALL` 下构造不 abort，无效名 `code()=="C"`（第 718 条 B 只测了 wcout）。
     - **B（`out_impl.h` + tie 图）**：经公开基类引用调 `tie()` 绕过环检测编译不过——`if constexpr` 为假时其后的 `self.handle_exception` 在基类上不存在（**防线是顺带的，见 §6**）；
       protected 的 `detach` 只剩「派生类取显式对象成员地址」一条旁路，属 C++ protected 的固有缺口、需刻意写派生类，后果同第三十二轮的限定名 detach；tie 图并发 clang TSan 4 线程 0 告警、阳性对照 1 条；
       关闭 §6「持锁线程在 `_IO_cleanup` 之后、`_exit` 之前写完」的 `cout` 一格——gdb `break _exit` + `scheduler-locking` 只放行持锁线程，同步与非同步两种模式迟到的插入都在 `_exit` 前落进文件
       （`--wrap`/LD_PRELOAD 拦不到 exit 内部对 `_exit` 的调用；**第三十四轮 B-S1 订正：非同步一格之所以落地，是因为探针里先有一次 `printf` 让 stdio 用过 stdout**）；
       clang 共享模式插件 `cout`/自定义 `tie_target`/`wcout` 三形态 rc=0、仍驻留，`-U__unix__` 对照两格崩；混合实例化成环（3 环、含 iostream 的 5 环、宽流环、自 tie、宽窄互 tie）全部被拒，Floyd 逐行核对不误拒；
       裸目标 `try_flush()` 里再写流已被「必须非阻塞」排除；tie 目标先析构留下悬垂指针已是 `stream_common_operators.h:712-724` 的用户契约。
     - **C（`objects.h` 与进程级）**：关闭 §6「fork 子进程 Helgrind/DRD」——原样头文件下两工具子进程各报 2000-5000 条，全是误报（valgrind 复用线程号把新线程当成被 fork 带走的锁持有者，且不建模 `std::atomic`），**没有判别力**；
       影子头（重建后 `pthread_mutex_destroy`+`init`、`m_generation` 加 HB 注解）下 helgrind/drd × sync/nosync × 3 次子进程库代码 0 条竞争，阳性对照 `++g_plain` 两工具都报出（B 用另一探针得到同样的「原样头文件只有误报」）。
       关闭 §6「libc++ 下 `copyable_mutex` 的重建」——**两个前提都不成立**：本机有 libc++ 21.1.8，且 libc++ 在 Linux 上 `std::mutex` 平凡可析构（非平凡的是 `recursive_mutex`/`shared_mutex`）；
       重建实测覆盖三种 mutex、独占与共享持有，普通 / ASan+UBSan / `_LIBCPP_HARDENING_MODE_DEBUG` 三种配置 3000/3000，去掉 `construct_at` 的阳性对照子进程挂起；整套流在 libc++ 下 fork 正常。
       §6「README 与 `iov2_export.h` 说宽了」不构成崩溃路径（措辞仍待定，见 §6）：逐头确认 30 个头不钉，其中唯一的命名空间级存放点是 `runtime_cvt_null_err`；`RTLD_LOCAL` 下只含这些头的插件真被卸载后宿主正常，
       `RTLD_GLOBAL` 下被借用符号时 glibc 经 `l_reldeps` 把插件留在内存。header-only 插件卸载后宿主再 fork，插件登记的 `pthread_atfork` 回调已随 `__cxa_finalize` 注销。
       三个新提交对单例生命周期无影响（`objects.h`、`src/` 零改动，八个 `sing_temp`、退出钩子、`_*_init` 定义顺序不变，构造函数里的 `tie()` 仍公开，自由函数 `sync_with_stdio` 不经被 protected 的 `adjust`）。

725. **【第三十四轮已改 `ab464122`，见第 730 条】第三十三轮核出的注释漂移。**
     `stream_common_operators.h:719-720`（ZH）/ `:823-824`（EN）仍写「`check_tie` 靠 `dynamic_cast` 跨转」，现为虚调 `tied_to()`（悬垂节点变成经垃圾 vptr 的间接调用）；
     `:141-143` / `:176-177`「此后任意 `tie()` 永久自旋」已过时（Floyd 判圈下成环只会让后续 `tie()` 被拒）；`out_impl.h:13`（ZH）/ `:89`（EN）/ `:500` / `:508` 仍写 `stream_common_operators`，应为 `std_stream_common_operators`。
     **checked.md 引用漂移**：§6 退出钩子行号 `flush_at_exit` 现 `out_impl.h:663-667`、`m_exited` `:672`；§6「libc++ `copyable_mutex`」与「`wcin` 切换失败」两项的前提不成立（见第 724 条）。

726. **本轮方法记录、教训与评估后不做的方向。**
     (1) 测 dlclose 与插件仍要配 clang 或 g++ `-fno-gnu-unique`（第 720 条 (1)）；本轮 C-1 是在**主程序**而不是插件上，所以 `exec -a` 改 `argv[0]` 是关键手法，普通启动永远看不到。
     (2) Helgrind/DRD 对 fork 子进程**不能直接用**：要么在重建处加客户端注解，要么不用；valgrind 头文件本机靠解开官方 `valgrind-devel` rpm（签名校验后）才有。
     (3) libc++ 构建门槛：`timeio` 在静态初始化期就调 `get_tzdb_list()`，需 `-fexperimental-library` + `libc++experimental.a`（系统未装，取自 `libcxx-static` rpm，未装进系统）。
     (4) 第 722 条评估后不做：`tie_target` 私有构造 + 友元 `out_tie_target` + `out_tie_target` 构造函数里 `static_assert` 校验 `T` 是本库的流——`struct Mine : out_tie_target<Mine>` 能借友元绕过，
     `final` 也拦不住（`tied_to()` 经 CRTP 调的是 `Mine::tie()`），要堵住得再加一层校验，代价超过收益；改由契约 + `final`。
     (5) 第 723 条未采用方案 (b)（catch 里向根查询真实标志）：违约方只能是用户自定义层，写成契约即可。
     (6) zsh 不对 `$VAR` 分词（第 720 条 (3)）本轮主控又踩了一次；多参数编译配方一律 `bash -c`。

## 第三十四轮：`io/objects/*.h` 只找「刚才没有提到」的问题（四条确认已并入 main：`d4756720` + `80d4a371` + `a2764053` + `ab464122`；两条存疑的修复在分支 `fix/io-objects-round34-suspects`：`1270ccc5` + `4ad2b355`）

基线 `main` = `dc67860f`（2026-10-09）。三个 agent 按**角度**分工（三个文件都在每个 agent 的范围内），类型与第三十三轮不重复：A（engineering-software-architect）边界条件与隐含假设、
B（claude）竞态与状态错误、C（engineering-minimal-change-engineer）头文件包含与拼写。主控自做机械检查：三个文件无行尾空白、无连续空行、均以单个 `\n` 结尾，`#include` 分组与组内字母序正确。
确认 4 条、存疑 2 条，存疑经调研后也都判为问题并修复。确认的 4 条主控均独立复现（A-1、A-2 用主控自写的探针）。探针索引见附录「第三十四轮」。

727. **【已修 `d4756720`】`synced_with_stdio()` 在切到同步、积压字节还没交给 stdio 时就已报 `true`（B-1）。**
     `out_impl.h` 的 `sync_with_stdio(true)` 在锁内先 `exchange(true)`、后 `m_channel.flush()` 搬运；`synced_with_stdio()` 无锁读。另一线程照很自然的写法
     `if (!cout.synced_with_stdio()) cout.sync_with_stdio(true); printf(…);` 在搬运完成前看到 `true`，跳过取锁直接 `printf`，字节排到积压之前——第 611 / 621 条修掉的 `BP` 失序经查询函数回来了
     （第 621 条「快照在锁内」管不到不取锁的查询）。输入侧先翻根、后 `store`，查询值只会晚于实际，没有这个问题。实测：`--wrap=fwrite` 注入 3/3 得 `BP`，无注入自旋每 2000 轮失序约 1990 轮。
     **修法**：锁内先搬运，异常存进局部 `exception_ptr`；再 `exchange(true)`；最后 `handle_exception`（掩码武装时会重抛，故必须在翻标志之后，「模式已切换」的语义不变）。
     哨兵只在持有 `io_mutex()` 时构造并读标志，其它线程的插入碰不到中间态。`synced_with_stdio()` 文档补上「读到 `true` 时此前缓冲的字节已交给 stdio」。
     **常驻用例**：`StdObjectsSync.SyncedWithStdioIsFalseUntilTheHandOverIsDone`——无缓冲的 `fopencookie` 流顶替 `stdout`（`std_device` 每次写都用全局 `stdout` 变量），在搬运自己的写回调里查询；
     旧头文件下失败（`synced_seen` 为 `true`），修复后通过；注入探针修复后 3/3 得 `PB`。只在 glibc 上运行。

728. **【已修 `80d4a371`】同步模式下三处计数边界多读 1 个字符（A-1）。**
     `in_impl.h` 同步模式的 `@warning` 承诺「每次 `read(0)` 只要本次操作还缺的字节」，但 `istream_operators.h` 里：`get<keep_sep>(s, n[, delim])` 存满后仍 `nextc()` 预取下一个；
     `ignore(n, delim)` 丢满 n 个后仍 `nextc()`；`get<keep_sep>(s, 1)` 一个字符也存不下仍先 `getc()` 向设备要 1 字节。多读的字节进了本流读缓冲，`getchar()` 看不到；管道上写端只写恰好够用并保持打开时调用会阻塞
     （实测 2001 ms，`read(b,2)` 1 ms；`std::cin` 同样会阻塞，问题在于文档许诺得比 std 强）。第 421 条确立了「这次预取不置 `eofbit`」，但没看到它会真去读设备；第三十一轮「同步不多读」的实测没覆盖这三个调用。
     **修法**（用户选「改行为」而非只收窄文档）：存满 / 丢满时 `bumpc()` 后停；`get<keep_sep>` 零容量时在 `getc()` 之前失败。`getline`（`cons_sep`）**有意不变**：存满时必须看下一个字符才能分辨分隔符与溢出，
     零容量时分隔符仍是要吃掉的空行（与 `std::getline` 一致），这一点写进了 `in_impl.h` 的 `@warning`。第 421 条的 `eofbit` 判据结果不变（两种提前退出时 `at_eof` 的前一半都已为假）。
     **常驻用例**（fd 0 换成写端已关的管道，读回 fd 上剩余字节）：`IoObjectsChar.SynchronizedGetAndIgnoreReadNoFurtherThanTheyConsume`、`IoObjectsWchar.SynchronizedGetAndIgnoreReadNoFurtherThanTheyConsume`
     （旧代码下 6 处断言全部失败，各多读 1 字节）；`IoObjectsChar.SynchronizedGetlineLooksAtOneMoreOnlyToDecide` 作对照（新旧都过）。gcc-release 输入相关 19 个套件通过。

729. **【已修 `a2764053`】`sing_temp<T>::init` 无条件原地构造：用户照 `std::ios_base::Init` 的惯用法再构造一个，正在使用的单例被重建（A-2）。**
     八个流类与 `ori_facet_buf` 都 `public` 继承 `sing_temp`，`init` 公开；注释「全程序只有一个 inline init」是没有机制保证的隐含假设（第 575 条判「私有继承没有再要藏的东西」时没考虑用户自己构造 `init`）。
     头文件模式下 `static IOv2::cout_t::init g;` 或块作用域里构造一个：已缓冲字节丢失、`sync_with_stdio(false)` 等设置复位、旧对象的堆内存泄漏（ASan 2392 字节），块结束时退出钩子被提前执行；
     无钩子的通用单例更糟——第二个 `init` 析构时把单例析构并置空，第一个 `init` 的 `get()` 成为空引用、段错误。
     **修法**：恢复 `e014bdd5` 删掉的引用计数（当时删它是因为改成了唯一的 inline `init`，与 ODR 修复本身无关），做三处调整：计数改为 `constinit std::atomic<std::size_t>`（`init` 可能在 `main` 之后、任意线程上来去）；
     最后一个 `init` 执行退出逻辑时保留 `101330d3` 的钩子语义（有钩子跑钩子，否则析构）；构造条件为「计数 0→1 **且** `instance()` 为空」，退出钩子留下的活对象不会被计数归零后迟到的 `init` 重建。
     库自己的 inline `init` 一定先于包含该头文件的 TU 里其后的静态对象初始化、晚于它们析构（共享模式下在 `libiov2.so` 里），用户的任何 `init` 都嵌套在它的生命期内，成为空操作。
     **共享模式不在代码里拦（用户决定）**：各模块的 `sing_temp<T>` 静态量私有，消费者模块里构造这些 `init` 会另造一份、`get()` 拿到的不是全局那一个；共享模式本就不该这样用，写进 `sing_temp.h` 与 `objects.h` 即可。
     **常驻用例**：`test_sing_temp` 新增 4 个（第二个 init 不重建不收尾、最后一个 init 才跑钩子、计数归零后不重建活对象、无钩子单例归零后重新构造）；`IoObjectsChar.AnotherCoutInitLeavesTheLiveCoutAlone`（共享模式跳过）。
     gcc-release 全量 62 个套件通过；installed-shared 本地未跑（交给 CI）。

730. **【已修 `ab464122`】`in_impl.h` 四处把基类写成 `stream_common_operators`（C34-1），连同第 725 条记下的同类漂移。**
     `in_impl.h` / `out_impl.h` 文件头基类名改为 `std_stream_common_operators`；两处「换设备接口」改为「普通流（`stream_common_operators`）上公开的换设备接口在标准流上删除」
     （主控核对：派生类里 `detach()` / `attach()` 仍有 `= delete` 声明，「删除」成立）；`stream_common_operators.h` 两处「`check_tie` 靠 `dynamic_cast`」改为经虚函数 `tied_to()`（已销毁节点上是经垃圾 vptr 的间接调用），
     两处「此后任意 `tie()` 永久自旋」改为「沿链能走到这个环的 `tie()` 都会被拒」（Floyd 判圈）。只改注释。

731. **【已修 `1270ccc5`，分支 `fix/io-objects-round34-suspects`】退出钩子之后结束的插入，若结束在 glibc 最后一次冲刷 stdio 之后，非同步的 `cout` / `wcout` 积压与晚到插入全部丢失（B 存疑 S1，调研后确认）。**
     钩子取不到锁放弃；持锁线程的插入结束时按第 715 条把字节交给 stdio，但此时 `_IO_cleanup` 已跑过，而 `_IO_unbuffer_all` 跳过从未定向（从未用过）的流，stdout 仍是全缓冲，字节留在 stdio 缓冲里直到 `_exit`。
     同步模式此前已经 stdio 写过、stderr 本就无缓冲，所以 12 格 gdb 窗口探针里只有 `cout` / `wcout` 非同步两格为空。`std::cout` 在同一窗口同样丢；判为问题是因为本库明确承诺过钩子运行时仍在进行的插入会交给 stdio
     （`m_exited` 注释、第 715 条），且 `out_impl.h`「由 glibc 在退出的最后冲刷」只对 `_IO_cleanup` 之前的插入成立。**订正第 724 条 B**：其 `cout` 窗口探针里先有一次 `printf` 让 stdio 用过 stdout，所以当时非同步一格落地。
     **修法**：输出哨兵析构在 `exited()` 为真时也 `dflush()`（`fflush` 设备的 `FILE`）。只在钩子之后的插入上多一次 `fflush`，锁序仍为先流锁、后 `FILE` 锁。哨兵文档与 `out_impl.h` 的「晚于退出钩子的插入」改写。
     **常驻用例**：`IoObjectsAfterExitHook.AnInsertionAfterStdiosLastFlushStillReachesTheDevice`——晚到的 `atexit` 函数写 `cout` 后立即 `std::_Exit`，作为「glibc 最后一次冲刷之后」的确定性替身；
     旧头文件下文件只以 `head|` 结尾，修复后以 `head|late` 结尾。真实的线程窗口只能用 gdb 探针复现（源码已内联进附录），修复后 `cout` / `wcout` 非同步两格恢复 `PRE|LATE`。gcc-release 输出相关 20 个套件通过。
     **CI 补丁 `2490bb7b`**：这条用例在 installed-shared 任务里失败——共享模式下钩子在 `libiov2.so` 初始化时登记，比程序的 `constructor(101)` 还早，所以最后才执行，
     `write_late` 先跑且它的 `_Exit` 让钩子根本没机会跑，捕获文件为空。改为 `IOV2_SHARED` 下跳过（被测的哨兵是两种模式共用的内联代码），只此用例用的 stdout 重定向辅助函数一并条件编译。
     **教训：依赖退出顺序的用例要在两种模式下各跑一次**——本地只跑了 gcc-release。

732. **【已修 `4ad2b355`，分支 `fix/io-objects-round34-suspects`】`in_impl.h` 类级概述的列举漏了 `adjust()` 的行为差别（C 存疑 S1，调研后确认为文档遗漏）。**
     概述列出了与 `istream` 的全部差别（增加的成员、删除的换设备接口），没列 `stdin_api::adjust` 收到 `stdin_sync` 时改走 `sync_with_stdio()`（`istream::adjust` 原样送到根）。`adjust` 自己的文档已写清，概述补半句（中英）。
     `stdout_api::adjust` 原样转发，`out_impl.h` 的概述不需要改。

733. **第三十四轮验证后确认无问题。**
     - **A（边界与隐含假设）**：`TChar` 模板参数域——`wchar_t` 不带 creator、`char16_t`、`char8_t`、`char32_t` 全部编译失败；进程启动时 fd 0/1/2 异常（stdin 已关 / 是目录 / 只写打开，stdout 已关 / 只读打开）五种情况都 rc=0、不挂起、置 `devfailbit`，ASan 干净；
       `in_impl.h` 文档里的读取数字（2048 / 2048 / 904、`wcin` 读取逐次变小）与 `read(0)` 日志逐字吻合；BIG5-HKSCS、CP1255 在 mbstate 里挂住组合字符，但两者 `mbtowc(NULL,…)` 都返回 1，宽标准流按第 672 条拒绝并回退到 "C"；
       同步模式下其余十几种提取都不多读。
     - **B（竞态与状态）**：`0e2f4a6d` 新开的公开 `adjust` 与查询、读写、`switch_code`、`reset`、`tie` 并发，clang TSan 10 线程 × 300 轮 3/3 次 0 告警（阳性对照 1 条、rc=66）；输入侧查询值不会「早」于实际；
       改状态操作两两并发都有锁覆盖；各成员异常路径上状态位与内部标志一致；`_IO_cleanup` 窗口其余十格都落地（关闭 §6 第三十三轮「窗口探针只跑了 `cout`」）；fork 落在 stdin「根已翻、查询值未 `store`」之间属
       `copyable_mutex.h` 已写明的契约内情形、输出侧无锁切 `false` 可线性化（**这两项只读码**，见 §6）。
     - **C（头文件与拼写）**：缺失 include 0、多余 0（逐符号核对定义所在头；`objects.h` 的 `istream.h` / `ostream.h` 是写明的入口 re-export）；g++ 影子树逐行删除实验 39 个 include 里只有 6 个删掉后编不过，
       「能编过」不等于「多余」（与第 623/624 条一致）。1070 个英文词过 hunspell，命中全是标识符或技术词；"Carries on on the same fd" 是 carry on + on，不是笔误；中文叠字、半角 / 全角混用、字符串字面量均为 0；
       中英块对照 9+6+10 对无笔误。
     - **主控**：三个文件的机械检查（行尾空白、连续空行、结尾 `\n`、include 分组与字母序）全部干净。

734. **本轮方法记录、教训与评估后不做的方向。**
     (1) **`oguard` 会吞掉 gtest 的失败信息**：它在用例体内把 stdout 重定向到临时文件，断言失败时的输出也写进那个文件。要在 `oguard` 作用域里断言，先把值存下来，出了作用域再 `EXPECT`（第 729 条的 cout 用例就是这样写的）。
     (2) **单线程确定性地复现并发窗口**：「搬运进行到一半时查询」用 `fopencookie` 顶替 `stdout` 并在写回调里检查（第 727 条）；「插入结束在 `_IO_cleanup` 之后」用 `atexit` 函数里插入后立即 `std::_Exit` 替身（第 731 条）。都比 gdb 或注入适合进常驻用例。
     (3) **一个块里混着两条修复时的提交拆分**：按块拆用 `git diff` 切块后 `git apply --cached`；同一块里交错的（第 728、729 条的用例相邻）就生成去掉另一条的文件内容，`git hash-object -w` + `git update-index --cacheinfo` 写进暂存区。
     (4) 第 729 条评估后不做：流类改私有继承 `sing_temp`（用户仍可写 `sing_temp<cout_t>::init`，且标准 `ios_base::Init` 惯用法依旧不能用）；只写文档（违约即重建单例）；共享模式在代码里拦（`sing_temp` 是通用组件，exe 里的用户单例是正当用法）。
     (5) 第 728 条未采用「只收窄文档」：改行为成本小、不影响 `eofbit` 判据，且与文档原有承诺一致。
     (6) agent 报告「`detach()` / `attach()` 被 `= delete` 已过时」这类说法前要看派生类本身：标准流派生类里的 `= delete` 声明仍在，只是基类同名函数改成了 protected。

735. **【固化 2026-10-10，`e788ed43` + `60281c24`，分支 `fix/io-objects-round34-suspects`】把只有探针的结论做成常驻用例，并把一道附带的防线改成明确约束。**
     - **`tie()` setter 加约束**（`e788ed43`，改 `stream_common_operators.h`）：`TSelf` 不得是 `basic_stream_common_operators` 本身。经基类引用调用原先只因基类上没有 `handle_exception` 而偶然编不过（第 724 条 B），
       把报错挪进 `if constexpr` 或给基类加 `handle_exception` 就会让环检测被静默绕过；约束让它退出重载决议，`requires` 表达式也才测得到（体内的实例化错误是硬错误）。
       `static_assert`：两种基类引用上不可调、`cout` / `cin` / `ostream` 上可调；旧头文件下两条否定断言编译失败。
     - **`IoObjectsWchar.ASwitchWhoseRecoveryFailsKeepsTheMode`**：第 724 条 A 的注入探针转正——自定义 `stdin_api` 配一个 `attach` 可失败的根，`reset()` 置 taint、`sync_with_stdio(false)` 的恢复失败，
       返回值、查询值与根的逐字节读都保持原模式。影子头把 `store` 挪到 `adjust` 之前时失败。
     - **`IoObjectsChar.EveryTieCycleShapeThroughStandardStreamsIsRejected`**：第 724 条 B 的混合实例化成环形态（三环、含 `iostream` 的五环、自 tie、跨字符类型）。
     - **`IoObjectsChar.AStandardStreamOverABrokenFdFailsItsFirstOperation`**：第 733 条 A 的五种启动时 fd 异常，fork 后改 fd 再 `exec` 自身，子进程带 `alarm`。
     - **`StdObjectsSync.WcoutSyncedWithStdioIsFalseUntilTheHandOverIsDone`**：第 727 条的宽流一侧，与 `cout` 共用一个辅助函数；`d4756720` 之前的 `out_impl.h` 下两条都失败。
     本地只跑了 gcc-release 的 `io.objects` 与 `concur` 及 clang `-Werror`；全量与 installed-shared 交给 CI（用户决定）。写测试时 clang 独有的 `-Wmissing-field-initializers` 又撞了一次（聚合初始化只给首个成员）。

## 已知但刻意不处理

与「未覆盖的范围」不同：那一节是**还没查**的地方，这一节是**查过、认了、决定不动**的。
重报这些等于推翻已经做过的判断，要推翻请先给出新的证据。

- **`hexfloat` 下的字母填充仍放行**（`setfill('f')` 配 `hexfloat` 写出 `"ffffffff0x1.8p+0"`）。
  `fill_alters_reading` 的判据只看 `basefield` 不看 `floatfield`，而 hexfloat 的字形是十六进制，
  按判据的字面表述算命中。不处理是因为它读不成任何数，危害远轻于整数一路。
  要处理就得让判据再收一个 `floatfield`，或让浮点那个调用点自己换算出等效 radix（见第 416 条末）。
- **`%z` 丢弃秒**（`tm_gmtoff = 1172`，即 `+00:19:32`，写出 `"+0019"`）。这是 `%z` 格式本身的
  有损性，`strftime` 与 libstdc++ 一样；get 侧存的是 `std::chrono::minutes`，分辨率天生是分钟。
  tzdb 里真有带秒的 LMT 值，拒绝会挡掉合法输入。往返闭合只在域内且整分钟时成立（见第 418 条）。
- **`p11` 型 tie 死锁**（见 `tie-deadlock-plan.md`）。方案已定未动工。
- **`%OC` / `%Op` 不实现**（`facet/timeio.h` `do_put` `case 'C'` / `case 'p'` 的修饰符守卫，`do_get` / `expand_and_filter` 镜像）。
  IOv2 的 E/O 合法组合表逐格等于 C23 / POSIX / C++ [time.format]；`%OC` / `%Op` 是 glibc 扩展（`do_number` 通则的覆盖 + `case 'p'` 不看修饰符），
  musl / BSD / macOS / Android / MSVC 无一渲染，libstdc++ `std::format` 抛异常。后果只落在 glibc 的 `lzh_TW` / `mnw_MM` / `my_MM` / `shn_MM`
  四个 locale 的 `%c` 上：`put("%c")` 回显字面量 `%OC`，`expand_format` 路径静默丢世纪。**决定不改（2026-09-21，用户拍板）**：
  解释器按标准写、glibc 只是数据来源之一，不接受"必须与 glibc 相同"；"未定义修饰符退回本体"的通则要动 50 处守卫 + `do_get` 的 case 体，
  是独立一轮重构的量。详见第 635 条。

- **宽流上负 `signed char` 的 `hex` / `oct` 字段比标准短一半**（`wos << hex << (signed char)-1`
  本库写 `ff`，标准写 `ffffffff`）。根因是 `operator<<` 的模板推导**不做整型提升**，`TValue`
  保持 `signed char`，`insert_int` 的位宽由 `make_unsigned_t<signed char>` 决定；标准那边宽流上
  没有 `signed char` 重载，提升后选中 `operator<<(int)`，再按 LWG 23 重解释为 `unsigned int`。
  **只有负 `signed char` 露出来**：`short` 等有自己重载的类型，标准重解释的宽度恰是
  `make_unsigned_t<TValue>`，本就一致；`unsigned char` 提升后非负，数字相同；窄流上两边都按
  字符语义走 `char_and_str.h`。
  **决定按现状保留（2026-09-07，用户拍板），已写进 `arithmetic.h` 的 `@warning`。** 理由是
  实测出来的，不是"更直观"：(1) 读侧 `signed char` / `unsigned char` 在宽流上**两个实现都不接受**
  （实测 IOv2 与 std 同为 0），复刻标准换不到任何互操作性；(2) 写出的文本必须换类型读回，
  而 8 位写法用**最自然的 `int`** 就能干净还原（`ff` → 255 → `-1`，`80` → 128 → `-128`，无失败位），
  32 位写法读进 `int` 会溢出 `INT_MAX`、置 `strfailbit`，且 `-128` 还原**错**（得 `-1`），
  必须改用 `unsigned` 才行——即对齐标准会把这个瑕疵一并复刻进来。
  探针：`.review/tmp/r14an_promo.cpp`（35 格差分，8 格不一致）、`r14r_read.cpp`（读侧可提取性
  IOv2 vs std）、`r14r_recover.cpp`（两方案的还原对拍）。

- **那 9 个 `%c` 装不下字段的往返有损 locale 不做 xfail 清单**（原 §2 欠账，第十四轮登记）：
  会把测试绑死在特定 glibc 版本上，与第十五轮对 `locale_dependent` 直方图和「被拒绝的 locale
  清单」的判断同理。
- **`zlib_cvt` / `chacha20_cvt` 上第 314 条守卫的黑盒测试不做**（原 §2 欠账）：要触发得替换
  libz / Botan 的实现（`LD_PRELOAD` 或假库），而造个假转换器只测得到守卫的**形状**，
  那已由 `test_put_buf_guard_1` 覆盖。

---
- **输入在字符中间到达 EOF，`clear()` 之后永远报 cvtfailbit、到不了 eof**（第二十八轮存疑 A-S1；文档 `418bc780`）：转换器仍收着凑不成字符的字节，报 cvt 错误是如实反映它的状态，须 `reset()` 或 seek 才丢掉（`init_state()`）；`test_code_cvt_mem_char8_t.cpp` 的 `TheCharactersBeforeACutOffStillComeOut` 锁住 cvt 层，流层见第 681 条。`ignore()` 原先第二次起把它当 eof，已随第 680 条一致。
  **决定不改（2026-10-01，用户拍板）**，只把 `in_impl.h` 里 `reset()` 与 `switch_code()` 两处「`clear()` 即可继续」补上这个例外。对照：C++ 标准未规定；C11 7.29.3.1 算编码错误；libstdc++ `wcin` / `wifstream` 与 glibc `fgetwc` 静默当 eof，libc++ `wcin` 先多交一个 `L'\0'`（`.review/tmp/std/s.cpp`）。
  曾考虑的两种改法——EOF 报错时丢掉收着的字节，或只报一次后按 eof 处理——都放弃了。顺带实测：自带设备读到 EOF 后都不再给新字节（`file_device` 的 `fread` 受 `FILE` EOF 标志粘住，`std_device` 有 `m_eof_hit`），所以「保留字节等文件增长」在本库里本就打不到（`.review/tmp/std/grow*.cpp`）。
- **多模块用 header-only 时 tie 可能跨插件成环，`-fvisibility=hidden` 的漏宏程序自检查不出**（第二十八轮存疑 D-S1、D-S2；`2d7416e4`）：**规则（2026-10-01，用户拍板）：header-only 只适用于单一模块；进程里不止一个模块用到 IOv2 时，每个模块都必须用共享库模式，混用（含「header-only 主程序 + 链接 `libiov2.so` 的插件」）不支持。**
  README、`iov2_export.h`、`objects.h`、`tie_graph_mutex()` 的说明已按此改写；原先 `iov2_export.h` 认可的「共存」形态（第 612 条）就此作废。D-S1（clang 头文件模式插件各一把 `tie_graph_mutex()`，并发互 tie 实测成环，由 Floyd 接住）与 D-S2（`-fvisibility=hidden` 的漏宏消费者配方无输出）都落在契约之外，`tie()` 与 Floyd 的两条 `@note` 不再逐处加限定。README 原有「默认可见性下 header-only 跨 `.so` 通常会被合并」与实测不符，已删。
- **UTF-7 这类「转换状态里存着半个字符」的编码：出错后续读可能拼出错字，截断在半个字符中间到 EOF 不报错**（第二十八轮存疑 A-S2、A-S3；文档 `4c84eccd`）：
  第 656 条「出错后保留 `m_state`」对只记移位的编码正确，对 UTF-7 会把残缺比特与后续字节拼接（`a+Ti\x80b` → `a`、出错、U+4E26）；`a+T` 的 6 比特进了 `mbstate_t` 而非 `m_pending`，到 EOF `is_mid_seq()` 为假、静默得 eof。`mbstate_t` 不透明，分不出编码类别、也读不到剩余比特数（合法的 `+Ti0-` 结尾同样留 2 个填充比特、不在初始态），无通用解法。
  **决定不改（2026-10-01，用户同意）**，只在 `code_cvt` 类说明加 `@note`。glibc `fgetwc` 前者永远报 EILSEQ、后者同样静默 eof。宽标准流不接受状态依赖的编码（第 672 条），只剩用户自建的 `code_cvt` 流，且 UTF-7 locale 须自行 `localedef`。探针 `.review/tmp/u7/u7.cpp`。
- **漏宏自检的判据不精确到 9 个单例**（第 658、673 条）：「已定义且非 COPY 的 `_ZN4IOv2` 数据符号」也会命中用户程序里别的 `IOv2` inline 变量（如模板静态数据成员）；
  旧的 `' [uV] _ZN4IOv2'` 同样如此，只作自检提示，不改。
- **include 卫生没有自动防线**（第 553 条；原「未覆盖的范围」§2）：删掉直接 include 仍编得过，测试锁不住，CI 里也没有检查。
  判据是集合差 `comm -23 <(提到 X 的头) <(直接包含 X 定义头的头)`。用户 2026-09-10 决定不做 lint，改头文件时按这个判据手工核对。

- **第二十九轮存疑 D-S2、C-S2 不改**（第 692 条）：`objects.h:23` / `:61`「见 `iov2_export.h` 的 `@warning`」那里有三条，但句子本身就是第一条的内容（`IOV2_SHARED` 口径一致），
  读者不至于找错；`sync_error` 的公开构造函数接受空的 `failures` 时 `what()` 是残句 `"IOv2::sync_with_stdio failed for"`，库内唯一构造点在 `failures::any()` 为真时，只有用户自己构造才出现。

- **第三十轮存疑 S-2 不改、S-4 原判不改**（2026-10-06，用户拍板，不补文档；探针在 scratchpad，未入库。S-4 已于第三十二轮消失，见下）：
  - **S-2　钩子之后的晚到插入在掩码武装时 `std::terminate`**（`out_impl.h:42` 的 `@note`）：在 `constructor(101)` 登记的 `atexit` 函数里打开掩码写 `cout`，stdout 无缓冲指向 `/dev/full`：
    `IOv2::cout` 同步 / 非同步都 rc=134；`std::cout` 同步同样 rc=134，非同步 rc=0——那是插入只进了 `filebuf`、`rdstate()` 为 0，随后 libstdc++ 在退出最后冲刷时把写失败静默吞掉、输出丢失，
    并非处理得更好。`exit()` 调用的 `atexit` 函数以异常退出、或从（隐式 `noexcept` 的）静态析构里抛出即 `terminate`，是语言规则；`@note` 已写「掩码含该位时抛出」。
  - **S-4　`dlclose` 之后 tie 目标的虚表随模块卸载**：**已于第三十二轮随第 712 条消失**（`16bc71c9`）。共享库模式下插件在加载时把自己钉成不可卸载，
    插件里永不析构的 tie 目标的代码一直在；修后同形探针 rc=0、不钉的对照 rc=139（第 719 条）。头文件模式本就只支持单一模块，不涉及。

- **`mem_device::get_buf<Saturate>` 不加 `noexcept(!Saturate)`**（原 §6 待办）：收益只是表达意图，库内无 `noexcept` 上下文调用它；`c6a0f7da` 的 `noexcept` 清扫
  明确把需要条件 `noexcept` 的 `iochannel::device()`、`abs_cvt::device()`、`copyable_mutex::unlock()` 留着不动，本条按同一先例不做。

- **持有 `copyable_mutex` 的线程自己调用 `fork()`，子进程里随后的解锁是 UB**（第 707 条，用户 2026-10-07 选「档 1」）。
  例：`IOv2::sync` 圈住的一段里 fork、子进程用这个流后 `sync` 析构；或在用户 `io_traits::swrite` 里 fork。子进程里这把锁第一次加锁时被重建，之后多出的继承解锁作用在不属于自己的锁上。
  实际表现：glibc + libstdc++ 下返回 `EPERM` 被忽略，看不出异常；libc++ 开断言时 `recursive_mutex::unlock` 会 abort。对照：裸 `std::recursive_mutex` 下同一线程在子进程里再加锁**永久阻塞**
  （glibc 按 TID 认持有者，第 710 条），所以现状已比标准锁好。「档 2」记持有者（`m_owner` + `m_depth`，重建后按原深度重新加锁、`unlock` 也查代数）可使之确定，
  代价每对加锁 / 解锁约 +2.5 ns（`__builtin_thread_pointer()` 取持有者），`cout << 'c'` 约慢 15-20%，为一个标准锁都不保证的边角不值。要推翻请先给出这种写法确实常见的证据。

- **`copyable_mutex::rebuild` 的 `construct_at` 落在 const 完整对象的 `mutable` 成员上不改，也不加文档**（第 717 条，第三十二轮 D 存疑 S-2；用户 2026-10-08 拍板）。
  触发：`const IOv2::locale loc(...)`（静态或自动存储期），fork 时另一线程持有它的 `m_facet_mutex`（`locale.h:1136`，`mutable copyable_mutex<std::shared_mutex>`），子进程里再用它 → `rebuild` 在 `loc` 的存储里 `construct_at`。
  按 [basic.life]/12（C++23 为 /11）字面是 UB，条文无 `mutable` 例外。不改的理由：[basic.type.qualifier] 与 CWG 1428 规定 `mutable` 子对象不是 const 对象；
  CWG 2677（2025-11 作为 DR 通过）已在 /9.2.2 把「o1 是 mutable 成员子对象」列为透明替换的条件，说明委员会认可在 const 完整对象的 mutable 成员上原地建对象；CWG 2514 仍在修订这块的不一致。
  实现上 gcc 15.3 与 clang 19 / 21 都把带 `mutable` 成员的 const 静态对象放进可写 `.data`（arm64 容器与本机 x86-64 均实测），写入不段错误、优化器也不能假定不变。
  替代方案都不成立：`pthread_mutex_init` 重新初始化已加锁的锁是 POSIX 未定义；字节数组 / union 自管生命周期仍是在 const 存储上建对象；锁放堆上换指针条文合规，但每个 `copyable_mutex` 多一次分配、
  构造不再 noexcept，并破坏 `io_converter` 要求的 noexcept 移动链（`cvt_concepts.h:265-266`）。在条文自相矛盾处写「这是 UB」会误导读者，故不加文档。
  用户的完整说明原在工作区 `iov2-round32-s5.md`（未入库，2026-10-08 内联到此处后删除）。

## 余量为零、改动时极危险的位置

这些位置当前都**正确**，不是缺陷；但缓冲或判据的余量恰好为 0，任何看似无害的改写都会
立刻越界或改变语义。改到这几处时先读这一节。

1. **`arithmetic.h` 算术特化约束里的 `!is_floating_point_v<TValue>` 是承重的左支短路。**
   把 `||` 改成 `&&`、或把该项挪走，`__int128` 一路（digits 127 > 64）立刻**静默消失**。
   **【第十六轮补】同一条合取里的 `digits` 那一项是 `float128_t` 的唯一拦截项**——
   它的 `min_exponent` / `max_exponent` 与 x87 `long double` **逐字相同**，
   删掉 `digits` 那项 `float128_t` 立刻被静默接纳。见第 544 条。
2. **`arithmetic.h` 指针 `swrite` 的两步 cast**：正确性依赖「`numeric::put` 从不解引用该
   指针」这一**未被断言锁住**的契约。若将来 `put` 改为解引用，去掉 `volatile` 立刻是 UB。
3. **`nullptr.h` 的 `TChar buf[7]`**：`"nullptr"` 恰 7 字符，写满且**不依赖终止符**
   （`ostream_insert` 用显式长度）。改字面量必须同步改数组长度。
4. **`tm.h` 里 `tm_stream_format` 的两个 `TChar[]` 字面量**：`+=` 走
   `operator+=(const CharT*)` 故**依赖末尾的 `TChar()`**。漏写终止符会让
   `traits_type::length` 越过数组末尾读。
5. **`tm.h` 的 `±4'000'000` 日偏移钳位**：在标准只保证「至少 25 位」的
   `chrono::days` 实现上余量仅约 9 万天（0.5%）。libstdc++ 用 `int64_t`，无压力。
   **【第十六轮实测】** 余量精确为**负侧 89'788 天、正侧 1'528'508 天**，与上面的推理逐位吻合；
   且「加法在钳位之前」**不构成溢出**，因为 `norm_year` 已在 `:135-137` 先被钳到日历范围。
   **新事实：`days::rep` 在 libstdc++ 是 63 位，在 libc++ 只有 31 位**——都无压力，
   但把 `±4'000'000` 放大到 `±2'000'000'000` 就会在 libc++ 上真的溢出。见第 538 条。
6. **`monetary::insert` 的 `runs` 数组余量为零**（见第 4306 行附近那条）。

---

## `io_traits` 可探测性的 TU 局部性：调研结论

背景：`requires { os << x; }` 的答案取决于本 TU 包含了 `traits/` 下的哪些头，
在跨 TU 发射的实体里拿它分支是 IFNDR。第十三轮问题 1 的处置是**只改文档**
（用户 2026-09-06 拍板），此处保留调研结论，供将来重开时不必重跑。

- **`std::hash` 与 `io_traits` 逐格同构。** 同样的用法在 `std::hash` 上也不编译失败，
  而是**静默分叉**，且症状随优化级别分两种：`-O0` 下模板的离线副本以弱符号（`W`）发射、
  链接器静默挑一个，于是**换个链接顺序输出就从 `1/1` 变成 `0/0`**；`-O2` 下被内联掉，
  **每个 TU 各答各的**。两种都无诊断。`std::hash` 的主模板「已定义但被禁用」只买到
  诊断质量，**不买 TU 不变性**。
- **没有任何工具能抓到它。** gcc `-flto -Wodr`、ASan `detect_odr_violation=2` 均无报告；
  两侧发射的是 token 完全相同的实体，链接器只比名字。
- **没有办法在保住可探测性的前提下让「TU 间分歧」本身变成错误。** 能做的只有两类：
  (1) **消 UB 但不报错**——把探测放进匿名 namespace（实测符号由 `W` 变 `t`，两种链接序
  都稳定），或把判据结果编进模板实参（实测发射两个不同符号，不构成 ODR 违反）；
  (2) **报错但要付代价**——放弃 fallback 分支（即本库现状）、或放弃可探测性。
- **哨兵 `static_assert` 方案已否决。** 依赖型别名那一手**不可移植**：gcc 上能推迟到
  实例化点从而拿到自定义诊断，clang 上仍是硬错误。依据是 [expr.prim.req]/5——
  「求值为 false」只适用于**模板实参替换**失败。两个编译器都干净的写法要多绕一层
  `is_complete_v` 偏特化，收益配不上复杂度，且只覆盖库自带的四个特化。
- **不要重开的另一条**：让 `ostream.h`/`istream.h` 默认包含四个 traits 头——被否，
  理由是包含策略要与 device（开集只包含 `device_concepts.h`）、facet（闭集因编译所需
  而包含）保持自洽，**不是**编译期开销（实测仅 +0.41% 行 / +7% 时间）。
- **【第十五轮订正】「缺头在『使用』路径上是硬编译错误」有反例**：见第 514、515 条。
  `arithmetic.h` 单独包含时 `os << "hello"` 是**使用路径上的静默错误输出**（打地址、流仍 `good()`）。
  已于 `98e9cc5` 修掉（条件排除），并由 `test/io/traits/include_combinations/` 常驻锁住。
  本节其余关于**探测**路径的结论不受影响。

- **落到文档的三条**：安全用法（关进单 TU，或保证相关 TU 包含同一套 traits 头）、
  两种症状都要写明（否则用户在 `-O2` 下试一次没复现会误以为不是问题）、
  以及分界线——**缺头在「使用」路径上是硬编译错误，只有「探测」路径静默**。

---

## 本清单**未**覆盖的范围

**本章 2026-09-10 重构过一次。** 此前它是 append-only 的流水账：八个小节按轮次堆叠、
老的从不删，672 行里混着大量**已完成的记录**、**已判定不做的项**、以及**同一笔债的四次重复登记**
（「只有探针没有常驻用例」四次、「仍未被系统审查的文件」四次、「应当单独排一轮」四次）。
已完成的内容已提成第 559-562 条，已判定不做的归入「已知但刻意不处理」。
**本章从此只留真待办。以后每轮结束时先删已关闭的，再加新的。**

---

### 0. 下一轮该做什么（结论先行）

**换靶：`cvt/`。**（2026-10-06 改写，第三十一至三十四轮后复核未变；原 §0 写于第十七轮后，建议换靶 `io/utilities`，此后第十八至三十四轮做的都是 `io/objects`。）

理由：`io/objects/*.h` 已连续十七轮（十八至三十四）。第三十三、三十四轮的确认问题一半修在 `io/objects` 之外（`src/iov2_objects.cpp` 的模块钉住、`common/sing_temp.h` 的引用计数、
`io/utilities/istream_operators.h` 的计数边界、`ostream_operators.h` 的退出后冲刷），留在三个头文件里的只剩并发可见性（第 727 条）与文档。更早几轮同样如此——第三十二轮的两条确认问题一条落在 `locale/` 的进程级缓存与共享库加载，一条是基类的隐式移动；最近几轮的确认问题全都落在下层——第三十轮三条里两条改在 `cvt/code_cvt.h`、一条牵出 `abs_cvt` 的
`get_main` 协议与 `root_cvt` 的读路径；第三十一轮的行为问题只剩并发与状态同步的边角（第 701-703 条），其余是第 696 条之后的文档漂移，
唯一的新机制（第 707 条，fork）落在 `common/copyable_mutex.h`。§6 现存的待办也几乎都标着「属 `cvt/` 轮」。下一轮从这些线索接：

- `zlib_cvt` / `chacha20_cvt` 的 `bos` 不接受短读（§6 第三十轮项）；
- `code_cvt::tell()` 与 `kernel_tell()` 在读出错后差一个字节（§6 第三十轮项）；
- `codecvt_kernel<char>::unshift()` 失败分支、「先 `unshift` 后 `put`」的同族、通用 `cvt_writer` 丢批后的移位态（§6，第 646、657、672 条同族）；
- `dput_error` 契约在 `mem_device` 与非根层上的余项（§6 第二十九轮项）；
- 第 694 条之后的 `get_main` 新协议（`noexcept`、`pair`、数据错误不消费）对四个转换器之外的扩展点用户是否讲清了。

`io/utilities/{istream,ostream}_operators.h` 的「提取与插入逻辑」轴（第二十五轮起，第 636-640 条）仍未系统做完，可作为再下一轮的靶子。

### 1. 真待办 —— 代码/行为

（第二十五轮关闭了 4 条：`ignore`/`write` 无符号计数 → 已改 `ptrdiff_t`（第 637 条）；`seek`/`rseek` 状态位 → 文档 + 用例（第 638 条）；
`%OC`/`%Op` → 订正根因后决定不改，转入「已知但刻意不处理」（第 635 条）；不完整 `in_ctx_t` → 文档（第 636 条）。
**行号提示**：`d98dcab0` 改名后 `io/` 各文件行号漂移且不一律，见第 640 条。）

- **`++iter` 抛出时目标可能比已消费量多一个字符**（`char_and_str.h:176-178` / `:663-665`）。
  形状真实，与「已消费字符的完整前缀」差一格。**未能构造复现**：`ichannel_iterator` 的
  `operator++` 在 `operator*` 已把字符拉进取数区之后才 `bumpc()`，`throwing_get_device`
  打不到那一拍。要坐实需自定义 channel（`bumpc` 抛而 `getc` 不抛）。

- **`type` 写坏是静默 no match，而 maker 写坏是具名 `static_assert`——政策不对称**
  （`traits_base.h:114-121`）。与第十六轮存疑 1 同族不同轴。**若处置那一条，把这条一并考虑。**
  第二十五轮（第 636 条）为不完整 `::type` 这一格补了文档并明说"本库探测不到、给不了具名断言"，政策本身未动。

### 2. 真待办 —— 没有自动防线的类别

- **中英块一致性**：标签计数法查不出整段缺失（第 521 条），无标签正文段落数法查不出标签差异，
  且裸计数会被正文里引用 `` `@note` `` 这类记号骗到（第十七轮踩过）。
  正确写法要**行首锚定** `^\s*\*\s*@tag`。同样未做成 lint。
- **多 TU + 不同 traits 包含 + 链接序的 ODR 问题**：单 TU 测试永远自洽，
  `-flto -Wodr` 与 ASan `detect_odr_violation=2` 均无报告。只能靠文档 + `include_combinations/`。

### 3. 真待办 —— 差分对拍（本地职责，不进 CI）

回归已进 CI（第 384 条，12 个 locale 全套件重跑）。剩下的是**与 libstdc++ 逐字节对拍**，
结论形如「两个实现在这一格上不同」而非「本库坏了」，不适合放进常规回归：
- `timeio` 的 **get 侧**未做全量差分（put 侧见第 404 条）；两处的**宽字符一侧**都未做。
- 非 UTF-8 编码 locale 只测了分隔符与小数点（第 360-361 条）。其余 locale 敏感行为
  ——货币符号、正负号串、`timeio` 名称字段——未测。其中**货币符号在单字节编码下是多字节**
  值得单独看，它走的存储路径与分隔符不同（不是单个 `CharT`）。

### 4. 本清单没有审查记录的文件（**都已审过**）

**这一节不是待办。** `checked.md` 是第十二轮前后才建的，此前的审查没有落在这里。用户 2026-09-23
确认：**`ci.yml` 的 clang-tidy 那一串里的头文件都已经审过**——那是 `common/`、`device/`、`cvt/`、
`facet/`、`locale/`、`io/utilities/`、`io/traits/` 全目录，加上 `io/` 下点名的 `iochannel.h`、
`iochannel_iterator.h`、`io_concepts.h`、`io_base.h`、`io_manip.h`、`istream.h`、`ostream.h`、
`iostream.h`，共 59 个头；余下 3 个 `io/objects/*.h` 由第十八至二十六轮覆盖（第 563-653 条）。
**`include/IOv2` 的 62 个头因此无一遗漏。**

所以本节的含义只是「**本文件里找不到那一轮的结论**」。要重审哪个头，先问用户那轮的记录在哪里，
不要因为这里没写就当成没审过、从头再来。本清单有记录的是：`io/objects/*.h`（第 563-653 条）、
`io/traits/*.h`（第十二至十七轮）、`io/utilities/*_operators.h`（第二十五轮起，见 §0）。

下面两条是**本清单记录的深度**，不是审查的有无：

- `facet/timeio.h`：本文件只记了第 226-228 条（纪元推导与修饰符作用域）、第 329 条（末尾落单的
  `%`）、第 635 条（`do_put` / `do_get` / `expand_and_filter` 三个 switch 的 E/O 修饰符表逐格核对）。
  各说明符的解析分支、`compute_ymd` 的其它推导链、时区辅助、`sread` 那 36 个截断前缀的抛出
  （`:3786-3787`）与 `contains_specifier`（`:2305-2324`）本文件无记录。
- `facet/numeric.h`：本文件只记了第 236-243 条（两个特化的约束判据、cv 路由、`void*` 的写出读回）。

### 5. 环境与范围外

- **多平台**：全部结论只在 Linux / glibc / libstdc++ 上验证过。
  `tm_gmtoff` / `tm_zone` 是 POSIX Issue 8（2024）才标准化的，MSVC 的 UCRT 上**不存在**。
  `chrono::days::rep` 在 libstdc++ 是 63 位、**libc++ 只有 31 位**（第 538 条）。
- **性能、二进制体积、编译期开销**——包括第 306-308 条判据的基准（每次写填充多做常数次比较）。
- **20 亿位以上的数字输入**：`facet/monetary.h::extract` 用 `int n` 计位数，
  2^31 位会有符号溢出。需 2 GB 级输入，libstdc++ 的 `money_get::do_get` 写法相同。
- **Botan 编译标志**（注意事项）：本机 Botan 2 在 `/usr/include/botan-2/` 与 `/lib64/`；
  用非 CMake 的命令行编译牵涉 `cvt` 的探针要补 `-I/usr/include/botan-2/ -lbotan-2`。
- **线程取消与哨兵**（范围外，运算符轴，只记）：同步模式、stdout 管道满时取消阻塞在 `cout << big` 里的线程，强制展开途中 `~out_sentry`（`ostream_operators.h:198`）
  再次 `flush()` → `fwrite` 阻塞，线程永远停在退出中。`io_base.h:1170` 已声明不支持取消；若将来支持，要看哨兵析构在 `std::uncaught_exceptions()` 增加时是否应跳过冲刷
  （libstdc++ 的 `sentry` 析构对 `unitbuf` 冲刷有 `!uncaught_exception()` 门槛）。
- **`clocale_wrapper::name()` 依赖 glibc 扩展 `_NL_LOCALE_NAME`**（第 588、651 条）：移植到 musl / 其它 libc 时要另存解析后的名字。
- **Windows / DLL 侧**完全未看；至今都只在 Linux/ELF 上验证（`-rdynamic` / STB_GNU_UNIQUE / COPY 重定位这些结论都是 ELF 专属；
  第 612 条的 `nm -D` 判据也只对 ELF 成立——Windows 上 `dllimport` 与 inline 定义在链接期就冲突，本来不需要）。
- **跑 `LOCPATH` 相关探针或用例时**：glibc 在该路径上自身泄漏（每次 `newlocale` 约 51 字节），要 `ASAN_OPTIONS=detect_leaks=0` 或 suppression，
  且 `LOCPATH` 只在子进程里设。绝对路径作 locale 名，glibc 不接受。
- **漏宏自检**的判据（第 658、673 条）与第三十轮的 `.so` 结论都只在 ELF / x86-64 上验证过；Windows/macOS 上 inline 变量如何跨模块合并未看。

### 6. 第十八至三十四轮（`io/objects/*.h`）留下的待办

各轮的处置见正文：第十八至二十二轮第 563-617 条，第二十三轮第 619-626 条，第二十四轮第 627-633 条，第二十六轮第 646-653 条，第二十七轮第 654-665 条，
第二十八轮第 666-681 条，第二十九轮第 682-692 条，第三十轮第 693-700 条，第三十一轮第 701-711 条，第三十二轮第 712-720 条，第三十三轮第 721-726 条，第三十四轮第 727-735 条。已关闭的项从本节删去，只留真待办。第 672 条后宽标准流不再接受状态依赖的编码，
下面凡涉及有状态编码的项只剩 `code_cvt` 本身。

- **`codecvt_kernel<char,TInt>::unshift()` 的失败分支**（`cvt/code_cvt.h:215-220`，先 `init_state()` 后抛）：与第 646 条同根，
  失败时状态同样与字节流失配。真实 glibc 上仍找不到触发；第二十八轮用 `--wrap=wcrtomb` 注入打到过（第 679 条 C-S1）。第 672 条后标准流不再走到这里，属 `cvt/` 轮。
- **有状态编码输入侧的余项**（第 654-656 条修复之后）：
  - **逐字符读的开销**：locale kernel +12%、`char8_t` kernel +45%（`-O2`，按 `m_pending` 实现测的）。第 695 条改为回退后未重测：`get(&c, 1)` 时多字节字符仍要逐字节
    `get_buf` 并重解前缀；多取会让无缓冲 stdin 阻塞，要优化须另想办法。
  - glibc `mbrtowc` 续接移位序列的缺陷可向上游报。最小复现：ISO-2022-JP 下把 `ESC $ B C f` 逐字节喂给 `mbrtowc`（共用一个 `mbstate_t`），
    `B` 那一字节返回 1 却不写 `*pwc`、`mbsinit` 变为 1，随后 `C` 被解成 ASCII；整段一次喂入则得 `r=5`、`U+4E2D`。
    同可上报：ISO-2022-JP 的 JIS 段里解出 0x00 时 `mbrtowc.c:102` 的 `__mbsinit` 断言失败、进程 abort（第 667 条，最短输入 `1b 24 42 00`）。
- **有状态编码输出侧的读码推理项**（第 657 条同族，未测）：
  通用 `cvt_writer`（`abs_cvt.h`）在 put 失败时把整批未提交字节丢掉，但 `m_state` 已前移——`code_cvt` 叠在非 root 下层（如 zlib）上时，
  流可能以移位态结尾（**只有读码推理**，属 `cvt/` 轮；标准流不可达，第 676 条）。
- **第 659 条（存疑按文档处理）的余项**：第 5 条若将来要向 `std::cin` 看齐（tty 挂断按 EOF），是行为改动；`file_device` 上「关闭设备可中断阻塞」未测。
- 范围外顺手记：`stream_common_operators.h` 现 `:451`（ZH）/ `:489`（EN）`attach()` 的 `@warning` 仍引用 `root_cvt.h:476-486`，已漂移
  （`attach` 现 `:486-518`，重抛 `:517`）；同文件现 `:483-500`、`:145-168` 的 `@note`/`@warning` 行顶格无缩进。
  **这两处行号会随 `io/utilities/` 与 `cvt/` 的每次改动再漂**，动手前重新定位（第二十六轮按 `7409f327` 又核过一次，未漂）。

**第二十八轮（2026-09-30 / 10-02）新增的待办**（处置见第 666-681 条）：

- **第 667 条删掉了「NUL 由多字节编码」的处理**：`conv == 0` 现判失败。UTF-7 的 `+AAA-` 这类编码出的 NUL 未测，且 glibc 在非初始态解出 NUL 时可能同样触发断言。
- **其它有状态编码未跑**：ISO-2022-JP-3、ISO-2022-CN（非 EXT）、IBM935/937/939；第 667 条的 NUL 断言只在 ISO-2022-JP 复现，其它 ISO-2022 变体未逐一试。
- **「先 `unshift` 后 `put`」的同族**：`close_stream()` 与第 672 条的 `flush_impl` 同形（`state_guard` 无论如何清零），设备在写复位序列时失败会丢序列。第 672 条后标准流不再涉及，
  只剩用户自建的 `code_cvt` 流，属 `cvt/` 轮。

**第二十九轮（2026-10-02 / 10-04）**见第 682-691 条：关闭了「推迟报错（`m_get_failed`）在 `sync_with_stdio` 切换时丢失」（第 690 条，流层碰不到）；第 680-681 条的分支已并入 main。存疑与待定项的处置见第 692 条，未留下新的待办。

- **`dput_error` 契约的余项**：只有 `std_device`、`file_device` 报 `written()`；`mem_device` 与 `cvt/` 里非根的层（zlib、crypt、`code_cvt` 叠在非 root 下层）失败时仍按「一个也没收」处理，是否会重复或丢，属 `cvt/` 轮。

**第三十轮（2026-10-05 / 10-06）**见第 693-699 条：关闭了「另一线程 `exit` 未跑 clang TSan」「clang 头文件模式插件 `dlclose`」「`m_pending` 与 `deof()` 预读互斥对 `readsome`/`in_avail` 的复核」
（库内无此入口）、「逐字符读 `C FF` 报两次错」「`MB_CUR_MAX` 低报的读侧对应物」（第 695 条后 `m_pending` 已删）、「第 668 条的行为变化未写进文档」（第 695 条已写进 `switch_to_put` 说明）。
存疑 S-2、S-4 处置见「已知但刻意不处理」。本轮新留下的（「钩子取不到锁时持锁线程的最后一次插入」「钩子与 `wcin` 读并发的 TSan 腿」「`-fno-gnu-unique` 插件 `dlclose` + tie」三项已由第 718 条关闭）：

- **`zlib_cvt` / `chacha20_cvt` 的 `bos` 不接受短读**：两处注释写「BOS 路径上 `kernel.get()` 保证给足字节」，但下层直接是 `root_cvt` 时不成立（管道、套接字都会短读）。
  `chacha20_cvt::bos_impl`（读 IV）抛「incomplete IV read」；`zlib_cvt::bos_impl`（读 2 字节头）只有 `assert`，release 下拿全零头部继续。第 694 条写用例时用「每次 1 字节」的设备打到。属 `cvt/` 轮。
- **`code_cvt::tell()` 与 `kernel_tell()` 在出错后差一个字节**：无效序列被当场报出时跳过的首字节算已消费、不计入 `m_accu_len`，定长编码下 `tell()` 比下层位置少 1，`seek(tell())` 会重读坏字节。
  `switch_to_put_impl` 的「定位到逻辑位置」因此会覆盖坏字节。只在「定长编码 + 读出错」时出现，未决定口径。
- **随机差分的余量**：A-2 的差分每个编码 300 例，EUC-TW（`0x8E` 四字节段）没跑；有状态编码由 `test_code_cvt_stateful_encoding.cpp`（ISO-2022-JP，子进程）覆盖，其它 ISO-2022 变体仍见上。

**第三十一轮（2026-10-06 / 10-07）**见第 701-711 条：关闭了第三十轮的「钩子与 `switch_code` 并发」中 `wcout` 一腿（第 709 条）；评估后不做的方向见第 711 条。本轮留下的唯一一项（第 707 条在 Valgrind 与共享库模式下的验证）已由第 718 条 D 部分关闭。

**第三十二轮（2026-10-08）**见第 712-720 条：关闭了第三十轮的三项与第三十一轮的一项（见上）。本轮新留下的：

- **README 与 `iov2_export.h` 的「包含 IOv2 头文件的模块不可卸载」说宽了**（第 719 条复核时发现，未改）：63 个公开头里 30 个在共享模式下不传递包含 `iov2_export.h`
  （`cvt/`、`device/` 全部，`iochannel.h`、`clocale_wrapper.h` 等），只包含它们的模块不会被钉住。不构成库自身的崩溃路径——这 30 个头里没有进程级存放点（唯一的 inline 变量
  `runtime_cvt_null_err` 是各模块自己的），碰到流对象或 locale 缓存必然包含会钉住的头；剩下的只有「插件把自己的多态对象交给宿主持有的流」这一用户责任形态。
  二选一：措辞收窄为「用到流对象、locale、facet 的模块」；或让 `common/defs.h` 包含 `iov2_export.h`（覆盖 30 个里的 21 个，其余 8 个不经过 `defs.h`）。
  第三十三轮用探针确认不构成崩溃路径（第 724 条 C），措辞仍待用户定。
- **第 712 条的 Mach-O 一侧未验证**：README 写「ELF / Mach-O」，只在 Linux / glibc 2.42 上实测；Windows 分支（`GetModuleHandleExW` 带 `GET_MODULE_HANDLE_EX_FLAG_PIN`，以及能否在 `DllMain` 里调用）留到做 DLL 打包时。
- **插件回归用例只在 gcc 的 installed-shared 任务里真正执行**：clang 插件形态只有第 719 条的手工复跑。

**第三十三轮（2026-10-09）**见第 721-726 条：关闭了第三十二轮的「`wcin` 切换失败」「`read_ahead need==0`」「`_IO_cleanup` 窗口」（`cout` 一格）「fork 子进程 Helgrind/DRD」「libc++ `copyable_mutex`」五项。
clang 那一腿的 `ctest -T memcheck` 本地仍未跑（交给 CI）。本轮新留下的：

- **protected 的 `detach` 只锁住了限定名形态**：`test_io_objects_char.cpp:77-93` 的 static_assert 不覆盖「派生类取显式对象成员地址」（B-N2，判为 C++ 固有缺口不报，但没有回归防线）。
  **1.0.2 起不再成立**：`std_stream_common_operators` 里的 protected `detach` / `attach` 已删除，标准流上没有这两个成员可取地址，旁路随之消失；限定名调用改由公开版本的 `requires (!is_std)` 拦住。
- **插件回归 `-U__unix__` 阳性对照下 `wcout` 一格不崩**的原因未深究（B-N5，推测插件没在进程级缓存里留下宽流条目）。
- **Helgrind/DRD 要进 CI 必须加注解**：原样头文件下子进程一侧两工具都只有误报（C-N1）。影子头 `.review/tmp/C-shadow/include` 证明「重建后 `pthread_mutex_destroy`+`init` + `m_generation` HB 注解」有效；注解进库代码还是做测试专用影子头，待用户定。
- **`dlmopen(LM_ID_NEWLM)` 载入插件会在新命名空间再载一份 `libiov2.so`、得到第二套单例**：只想到、未测。
- **C-1 修法的 Mach-O 一侧**：macOS 无 `dladdr1`；其 `dladdr` 不受 `argv[0]` 影响，但 `dlerror` 残留同样要清，未验证。

**第三十四轮（2026-10-09）**见第 727-735 条：关闭了第三十三轮的「`_IO_cleanup` 窗口探针只跑了 `cout`」（第 733 条 B；`cout` / `wcout` 非同步两格即第 731 条）。本轮新留下的：

- **本地未跑的腿**：第 729、731 条的修复未在本地跑 installed-shared（第 729 条的共享模式跳过分支、插件卸载用例都在那里才执行），gcc-tsan / gcc-sanitizer 也未跑，交给 CI。
- **第 731 条只在 glibc 2.42 / x86-64 上测过**：未逐版核对 glibc 的 `_IO_unbuffer_all`（「未定向即未用过」的判据），也未跑共享库模式；常驻用例用 `_Exit` 作替身，真实的线程窗口只有附录里的 gdb 探针可手工复跑。
- **第 733 条 B 的两项只有读码**：fork 落在 stdin「根已翻、查询值未 `store`」之间；输出侧无锁切 `false` 的可线性化。未构造探针。
- **第 728 条只查了 `get` / `ignore`**：`io/traits/` 下其它提取器在字段恰好读满时会不会多探 1 个字符，未系统扫。
- **include 删除实验只跑了 g++**（第 733 条 C），未跑 clang。

---

## 附录：第十四至十七轮探针索引（`.review/` 删除前内联）

`.review/` 已于 2026-09-08 整个删除（654M 中 99.2% 是 ASan/UBSan 二进制），
第十六轮重建后又于 2026-09-09 再次删除（208M，同样几乎全是二进制）。
删除前把探针的**自述注释**内联到这里，使正文里 `.review/tmp/xxx` 那些引用仍然可读：
知道每个探针**证明了什么**，比留着源码更重要——结论都已写进正文的编号条目。

**⭐ 标记**的是正文条目具名引用过的探针。2026-10-06 精简：第十四至二十四轮的索引只保留 ⭐ 探针（原 292 条删去 227 条），
其余探针的结论都已在正文编号条目里，不再单列；2026-10-10 对第二十五至三十四轮做了同样的处理，各轮通用的手法归并到下面「探针手法与配方」。
**这些文件都不再存在**，需要复现时按正文条目描述重写即可，编译配方见下。

```
g++ -std=c++23 -I include -fsanitize=address,undefined -g -O1 <src> -o <out>
```
（`__int128` 一路改 `-std=gnu++23`；牵涉 `cvt` 的探针补 `-I/usr/include/botan-2/ -lbotan-2`；
本机构建并发一律 `--parallel 3`。）

**探针手法与配方**（各轮反复用到、比单个探针更值得留的东西）：

- **注入**：链接期 `-Wl,--wrap=<sym>`（`read`、`fwrite`、`wcrtomb`、`duplocale`、`newlocale`、`_Znwm` 都用过）打到真实环境里走不到的失败分支；替换全局 `operator new` 与 ASan 冲突，那类探针只开 UBSan（第 581 条）。
  `exit` 内部对 `_exit` 的调用 `--wrap` / `LD_PRELOAD` 都拦不到，用 gdb `break _exit`。
- **影子头对照**：把 `git show HEAD:<头文件>`（或手改过的副本）放进另一目录，用 `-I` 置于 `include/` 之前编同一份探针或测试——修复的正反对照、新用例的分辨力都这样验（第 727-731 条）。
- **有状态编码**：`LC_ALL=C localedef -c -f <x>.cm -i mini.src <dir>/xx_XX.<x>`（`-c` 下只有警告时退出码为 1，属正常），运行时只在子进程里设
  `LOCPATH=<dir> LC_ALL=xx_XX.<x> ASAN_OPTIONS=detect_leaks=0`（glibc 在 `LOCPATH` 路径上自身泄漏）；ISO-2022-JP 夹具已进仓库 `test/IOv2TestResources/iso2022jp/`。
- **写失败**：`RLIMIT_FSIZE` 设小并忽略 `SIGXFSZ`，或 `bash -c '… > /dev/full'`（zsh 的 MULTIOS 会把 `>/dev/full | grep` 变成 tee，程序看不到写失败）。
- **并发**：TSan 用 `clang++ -fsanitize=thread -O1 -g`（本机 gcc 的 libtsan 缺失），每个探针都带一个故意竞争的阳性对照；fork 后子进程一侧 TSan 是瞎的，Helgrind / DRD 不加注解只有误报（第 724 条 C）。
- **退出窗口**：gdb `break _exit` + `set scheduler-locking on` 只放行持锁线程（第 731 条，源码见第三十四轮索引）；能进常驻用例的确定性替身是 `atexit` 函数里插入后立即 `std::_Exit`，
  但只在头文件模式下成立——共享模式下钩子在 `libiov2.so` 初始化时登记、最后才执行，那条用例在 `IOV2_SHARED` 下跳过（`2490bb7b`）。
- **搬运进行到一半的状态**：`fopencookie` 造一个无缓冲流赋给 `stdout`（`std_device` 每次写都用全局 `stdout` 变量），在写回调里查询（第 727 条）。
- **插件与共享库**：测 `dlclose` 一律用 clang 或 g++ `-fno-gnu-unique`（g++ 默认的 STB_GNU_UNIQUE 让插件永不卸载），并用 `dlopen(RTLD_NOLOAD)` 核实；`-U__unix__` 编出的不钉插件是阳性对照；
  installed-shared 预设拒绝仓库内前缀，`make install-shared PREFIX=<仓库外>`；改 `argv[0]` 用 `exec -a`（第 721 条）。
- **测试里的输出捕获**：`oguard` 作用域内断言失败的信息会被写进捕获文件，先存值、出作用域再 `EXPECT`（第 734 条）。

- **`r14an_promo`** ⭐ — arithmetic.h keeps signed char / unsigned char in the class-template list and documents the wide-stream case as "reaching numeric insertion through integral promotion". Does the observable result actually eq…
- **`r14tb_sink`** ⭐ — Probe B: is the "user cannot supply his own TIter" premise (checked.md 478) still true? io_traits<TChar,T>::swrite is a *public* class template member; the recipe in traits_base.h :20-35 spells out its signa…
- **`r14tm_expand`** ⭐ — `facet/timeio.h::expand_format` 对 `%OC` / `%Op` 的处理。实测（`shn_MM.utf8`）：`%OC%Oy` → `%Oy`、`%OC` → 空、`%Op` → 空、`%OI %Op` → `%OI`，而不带 `O` 的 `%C` / `%p` 与同样带 `O` 的 `%Oy` / `%OI` / `%OM` / `%OS` 都保留。根因在 `expand_format`，留给 `facet/timeio.h` 专轮。

**注意**：第十一轮及更早的探针（`r11*`）在本轮之前就已不存在，正文里指向它们的引用
是历史记录而非可访问路径。本附录只覆盖第十四、十五轮。

---

### 第十六轮探针（`r16*`，2026-09-09 随 `.review/` 一并删除）

（本轮没有被正文点名的探针，索引已于 2026-10-06 删去。）

编译配方同上；TSan 一路必须用 clang（本机 gcc 的 libtsan 缺失），
valgrind 一路不带 sanitizer 且 `-O0`。**凡带阳性对照的都已注明**。

---

### 第十七轮探针（`r17*` 与协调者的 `verify_*`，2026-09-10 随 `.review/` 一并删除）

本轮的**核心证据**是几个反例迭代器/汇，重写成本很低，结论都已写进第 551-558 条。
其中 `r17fix_unstable` 与 `r17fix_count` 已被常驻用例取代
（`IstreamExtractCharacterIteratorContract` 两条），不必再重写。

- **`r17fix_count`** ⭐ — 第 551 条的解引用计数：4 字符 `2n+1=9` → `n+1=5`。**已被常驻用例取代。**
- **`r17fix_unstable`** ⭐ — 第 551 条的承重证据。一个 `static_assert` 钉住「恰好满足 `input_or_output_iterator` + `sentinel_for` + `value_type==char`，而**不是** `input_iterator`」的迭代器，`*i` 不推进但两次求值给不同值。修复前两个提取点都得到 `ABCD`，修复后 `abcd`。**已被常驻用例取代。**
- **`verify_a`** ⭐ — 协调者对 agent A 两条结论的独立复现：`operator*` 调用 9 次（2n+1）；定长数组形式在 boom=3/5 处留下 `ab`/`abcd`，即完整前缀——推翻了「没有前缀保证」那句注释。
- **`verify_c1b`** ⭐ — 第 554 条的越界复现：`char buf[8]` + `width(100)` 经文档化的手写调用 → ASan `stack-buffer-overflow`，WRITE of size 98，栈顶 `numeric.h:1100` ← `arithmetic.h:106`。
- **`verify_c1`** ⭐ — 第 554 条前提：`char_sink_for<char*,char>=1`、`<wchar_t*,wchar_t>=1`、`<char*,wchar_t>=0`。**已被 `test_io_traits.cpp` §13 的三条 `static_assert` 取代。**
- **`verify_c1_std2`** ⭐ — 第 554 条的 std 对照：`std::format_to(buf, "{:100}", 42)` 对 8 字节缓冲**同样越界**（WRITE of size 32）——证明暴露面与标准一致，这是降级为调用方契约的依据。
- **`negctl`** ⭐ — 第 551 条修法的阴性对照：向**串形式**注入「`break` 前多一个 `++iter`」，
  `peek` 由 `' '` 变成 `c`（分隔符被吃掉）。证明
  `IstreamExtractCharacterBounds.TheStringFormLeavesTheDelimiterToo` 有分辨力，
  且该笔误在没有这条用例时**完全静默**——两次连续提取照样成功，第二次的哨兵会跳空白。 

### 第十八轮探针索引（`.review/` 于 2026-09-12 删除前内联）

编译配方：`g++ -std=c++23 -fsanitize=address,undefined -g -O0 -Iinclude -Itest -I/usr/include/botan-2
-D_POSIX_C_SOURCE=200809L <src> -lz -lbotan-2`；探针 TU 除 `<IOv2/io/objects/objects.h>` 还要包含
`<IOv2/io/traits/char_and_str.h>` 与 `<IOv2/io/traits/arithmetic.h>`（traits 是可选头）。TSan 用
`clang++ -fsanitize=thread -O1`。shared 模式按 `src/iov2_objects.cpp` 头注释的命令编 .so。

- **`in_01_sync_lookahead`** ⭐ — what does `cin >> x; cin.sync_with_stdio(false); cin >> y;` really lose?（S2：buf→unbuf 丢 `" 22 33\n"`）
- **`in_02_sync_throw`** ⭐ — make the istreambuf reconstruction inside stdin_api::sync_with_stdio throw（替换 `operator new`，只开 UBSan；第 567 条）
- **`in_03_tsan_sync`** ⭐ — does stdin_api::sync_with_stdio() / reset() race with a concurrent `cin >> x`（mode 9 = 裸 int 正控）
- **`in_04_tsan_switch_code`** ⭐ — wcin.switch_code() / wcin.code() bypass the locked stream_common_operators
- **`life_1b_tu1/tu2`**、**`life_1c_tu1`** ⭐ — TU1 does NOT include any IOv2 header; static uses cout in ctor / only in dtor（第 571 条）
- **`life_2b_exit_race`** ⭐ — std::exit() from a second thread while the main thread is inside `cout << ...`（`std` 参数切对照组）
- **`life_2b2_main_returns`** ⭐ — the more common shape: a detached worker thread logs via cout while main() returns
- **`life_4a_consumer`** ⭐ — shared-mode consumer: -DIOV2_SHARED, links libiov2.so; static uses cout in ctor and dtor（S5 的 `ptr()=(nil)` 也在这里）
- **`lsan_reachable`**、**`lsan_std_cout_heap`**、**`lsan_iov2_heap`** ⭐ — 第 564 条的三个 LSan 实验
- **`out_01_dtor_chain`** ⭐（+ `out_01_writeshim.c` LD_PRELOAD 计数 `write(2)`、`out_01b_glibc_retain.c` glibc fflush 失败后是否保留缓冲）— destructor-chain flush behaviour under failed device / fail state
- **`out_02_sync_stdio`** ⭐ — sync_with_stdio semantics vs printf interleaving; toggling false->true with pending buffer
- **`out_04_switch_race`** ⭐ — switch_code()/code() take no io_mutex(); race against concurrent wcout <<（`positive` 参数 = 正控）
- **`out_05_reset`** ⭐ — reset() on pending buffer / failed state / dead device / tainted converter（第 569 条、S4）

### 第十九轮探针索引（`.review/` 于 2026-09-14 删除前内联）

编译配方同第十八轮（`-fsanitize=address,undefined`，探针 TU 除 objects.h 还要包含两个 traits 头）。

- **`r19out_exithang`** ⭐ — the six exit hooks call p->flush(), which takes io_mutex(): does exit() hang when another thread holds it?
  （`mode 0/1` = IOv2/std；第二参数 hold_ms；第 586 条：3 s → 3.14 s vs 0.35 s，600 s → rc=124）
- **`r19out_reset`** ⭐ — stdout_api::reset() vs its own comment（sub 0/5 pending 被交付、sub 1 格式状态保留、sub 2 不触发 tie 刷新、
  sub 3 `/dev/full` 下裸抛 + 状态 0 + 之后 12、sub 6 宽流自愈）
- **`r19in_empty`** ⭐ — switch_code("")：mode 1 环境改变后 `sync_with_stdio` abort（rc=134），mode 2 具名对照，mode 3 含 NUL 名，
  mode 4 70000 字节名（第 588 条）
- **`hyg_reset_out`** ⭐ — reset() 是否「丢弃尚未提交的输出」（第 589 条：`PENDING` 照样写出）
- **`hyg_traits_only_objects`** ⭐ — 只包 objects.h 时哪些插入能编译（第 590 条）

### 第二十轮探针索引（`.review/` 于 2026-09-15 删除前内联）

编译配方同第十八轮。探针源已随 `.review/` 删除；需要复现时按条目描述重写。

- **`r20z_reset`** ⭐ — 主控：全程不碰 `setbuf`、`> 文件` 拿天然全缓冲，`reset()` 得 `0x0` 而 `flush()` 得 `0x2`，`PENDING` 都丢（第 594 条）；修后 `0x2`。
- **`r20z_switch`** ⭐ — 主控：缺省缓冲下 tainted `wcout` 的 `switch_code("C")` 不抛、`code()` 变 `C`、`PENDING` 丢（第 595 条）；修后抛 `device_error`、编码不变。
- **`r20z_glibc.c`** ⭐ — 主控：纯 C 归因，不做那次 `fflush` 则 `PENDING|AFTER`，做了则 `|AFTER`（第 594 条）。
- **`r20b_01_exit_in_swrite`** ⭐ — 用户 `swrite` 中途 `std::exit(0)` / 重入 `flush()` / 从 cerr 的 swrite 退出，12 组合（第 600 条）。
- **`r20c_tie_tu1/tu2`** ⭐ — TU2 静态在构造里起 4 线程狂 `tie()`，TU1 的 `_cin_init` 撞上去；两种链接序 + TSan + ASan（第 600 条）。
- **`r20c_singexit`** ⭐ — 自定义 `sing_temp` 用户单例在构造函数体内 `exit()`：钩子已登记 + T 未完成；exit 落在两个 init 之间（第 600 条）。

### 第二十一轮探针索引（`.review/` 于 2026-09-16 删除前内联）

编译配方同第十八轮；TSan 用 `clang++ -fsanitize=thread -O1 -g`，正控见第 607 条。

- **`r21z_nul`** ⭐ — 主控：首字节 NUL 的名字静默切到环境编码（第 602 条；`LC_ALL=C` 下 `before=zh_CN.UTF-8 after=C rdstate=0x0`）。
- **`r21p_tty_eof.cpp` + `.py`** ⭐ — pty：Ctrl-D 之后 `clear()` / `reset()` 的行为，与 `std::cin` 对照（第 606 条）。
- **`r21r_switch2` / `r21r_switch2_in`** ⭐ — 三段式 `switch_code` 的并发（输出侧 / 输入侧含自缓冲模式）（第 606 条）。
- **`r21r_exit_switch`** ⭐ — 退出钩子落在 `switch_code` 的无锁窗口（第 606、607 条）。

### 第二十二轮探针索引（`.review/` 于 2026-09-17 删除前内联）

编译配方同第十八轮；TSan 用 `clang++ -fsanitize=thread -O1 -g`，正控见第 607 条。shared 模式 .so 按 `src/iov2_objects.cpp` 头注释的命令编。

- **`z_wreset`** ⭐ — 主控：`cout`/`wcout` × sync 两态 `> /dev/full` 下 `reset()` 的 `rdstate`（第 609 条：`0x2` vs `0x0`）；gdb 在 `out_impl.h:402` 后断 `fflush` 得 `~pair ← abs_cvt::detach` 栈。
- **`z_tz_exit`**、**`z_12`/`z_21`**、**`z_late`**、**`z_std_tz`** ⭐ — 主控：第 610 条的 detached 线程形态（3/3 UAF）、两 TU 链接序形态（TU1 在前 UAF、在后正常）、单 TU「静态定义在包含之前」形态、libstdc++ tzdb 的 detached 线程对照（3/3 干净）。
- **`z_sync`** ⭐ — 主控：第 615 条端到端——默认掩码不抛全切；武装 `cout` 掩码 + `/dev/full` 抛 `sync_error`，`failed().cout_err` 独非空，`what()` 带流名与原因，其余流仍切换。
- **`d/`（`solib.cpp` + `good.cpp` + `bad.cpp`）**、**`d2/`（真库 + `ok`/`bad` 消费者）** ⭐ — 主控：第 612 条——`dlsym`+`dladdr` 自检在正确消费者上同样报 INTERPOSED（COPY 重定位）；`nm -D` 的 `B`+COPY vs `u` 判据在真库上 0 vs 9。
- **`z_attach`** ⭐ — 主控：第 618 条——五个标准流在全部分配失败 + fd 指向 `/dev/full` 下 `reset()`，0 分配、状态位全 0。
- **`A_tty.cpp` + `A_tty.py`** ⭐ — pty.fork：`cin>>n;getchar()` / `getchar();cin>>n` / `cin>>a;wcin>>b` × sync 两态，与 `std::cin` 对照（第 613 条）。
- **`A_reset_alloc`/`A_reset_alloc2`** ⭐ — 替换 `operator new` 计数 + 全部失败（只开 UBSan）：`reset()` 的 `attach()` 零分配（第 613 条）。
- **`B_a_sync`** ⭐ — `sync_with_stdio(false)` → 插入 → `(true)` → `printf` 得 `BA`；mode 2 = 锁下 `flush()` 的模拟（第 611 条）。
- **`B_c_reset`**、**`B_e_flip`**、**`B_f_wreset`**、**`B_g_wreset_lost`** ⭐、**`B_a_wide`** — `reset()`/`switch_code` 边界；翻转落在 sentry 内；第 609 条的发现与 gdb 栈；编码切换保留已编码字节。
- **`C_a_tu1/tu2`**、**`C_as_*`** ⭐ — 第九个单例跨 TU 顺序：header-only / shared × 两链接序 × ASan（第 613 条）。
- **`C_b_tsan`** + `so_tsan/` ⭐ — shared 模式 TSan：10 线程 tie/插入/`sync_with_stdio` 翻转 + noinline 正控。
- **`C_c_host` + `C_c_plugin`** + `so_nogu/` ⭐ — `-fno-gnu-unique` 下 `dlclose` 真卸载、`RTLD_NOLOAD` 复查、三轮 dlopen（每轮 LSan 47475 B）。
- **`C_f2_exit_at_k`** ⭐ — 插桩 `newlocale`，第 k 次（1…177）时另一线程 `exit(0)`；**`C_f_exit_during_init`** 微秒延时扫描（偶发 libstdc++ tzdb SEGV）（第 616 条）。
- **`C_g_tz_exit`**、**`C_g_msg_exit`**、**`C_g2_tu1/tu2`**、**`C_g3_std_tu2`** ⭐ — 第 610 条的三种形态与 libstdc++ `locate_zone` 对照。

### 第二十三轮探针索引（`.review/` 于 2026-09-17 删除前内联）

编译配方同第十八轮；TSan 用 `clang++ -fsanitize=thread -O1 -g`（`R_build_tsan.sh` 可重建），正控见第 607 条。

- **`z_b1`** ⭐ — 主控：第 619 条——全缓冲 stdout → `/dev/full`，`cout` 零字节、只有 `printf("P")` 在 stdio 缓冲，`sync_with_stdio(true)` 得 `rdstate=0x2 ferror=0`。
- **`B_1`** ⭐ — 第 619 条的 agent 版：mode 1 `printf` 字节 + `/dev/full` 切回 `0x2`，同设置同步插入 `0x0`。
- **`B_2`** ⭐ — 第 622 条：自定义可定位设备实例化 `stdin_api`，掩码命中后「kernel is SYNC, flag says unsync」。
- **`R_01_handover`** ⭐ — 三线程交接（mode 1）与两个切换者（mode 2，第 621 条）；mode 9 noinline 正控。
- **`R_07_double_report`** ⭐ — 第 620 条：双重登记的幻影位 1–4 轮命中；单次登记对照 2000 轮 0 次。

### 第二十四轮探针索引（`.review/` 于 2026-09-18 删除前内联）

编译配方同第十八轮（`-D_GNU_SOURCE -ldl` 给插桩 `read()` 的探针）；本机无 `strace`，系统调用计数用 `F_count.c`（`LD_PRELOAD`，`dlsym(RTLD_NEXT)` 计 fd 0 的 `read`/`poll`）。
pty 形态用 `E_pty.py`（`pty.fork` 驱动，先敲空行、1 s 后看有无输出、再敲第二行）。

- **`D_issue1_demo`** ⭐ — 主控：第 627 条——`<<"A"` | 失败位（人工 / `_IONBF`+`/dev/full` 真失败）| `sync(true)` | `clear()` | `printf("P")` | `<<"B"`，
  `__fpending` 逐步：`PAB` vs good/eofbit 的 `APB`；`-I patched` 影子头上三态全 `APB`。
- **`D_issue1_retry`** ⭐ — 主控：第 627 条改前核对——重试再失败同一位、掩码武装再抛且 `synced=1`、tainted `wcout` 未 `clear()` + `/dev/full` `0x4→0x6`。
- **`A_02_ignore_block`** ⭐、**`A_03_extract_block`**、**`A_08_wignore_block`** — 第 628 条：fork+pipe 写端保持打开，`ignore()` / `>> int` / `>> string` /
  `wcin` 同步 vs 非同步 vs `std::cin` 的返回时刻（旧 2001/4002 ms → 1/2001 ms）。**`A_09_tty_ignore`** ⭐ + `E_pty.py` — 真 pty 敲空行。
- **`F_ign`** ⭐ — 第 628 条的吞吐核对：文件上非同步 `ignore(1000)` × 6，`read()` 次数 旧 5 / 短读 3001 / poll 5（+2 `poll`）——短读版走错的证据。
  **`F_bulk`** — 200000 整数的 `>> int`（文件），计数受 `m_read_buf` 峰值影响不稳定，只作参考。**`F_dev`** — 设备层三形态（就绪 / 只一字节 / 挂断）。
- **`A_01_mc_switch`** ⭐ — 第 630 条：`cin.device().deof()` 留下 `m_c` 后翻 `sync_with_stdio` / `reset()`，五态。
- **`A_04_sync_fail`** / **`A_05_wsync_fail`** ⭐ — 第 630 条：替换 `operator new` + 插桩 `read()`，失败路径与 `reset()` 后 kernel 模式（n=1 / n=2048）。
- **`B_01_flip_inside`**（a-e）⭐ — 第 611 条那一格的五种形态。**`B_02_failed_skip`**（s/r/e）⭐ + `B_02b`/`B_02c` — 第 627 条 agent 版与 `__fpending` 陷阱核对。
  **`B_03_wcout_tainted`**（1/2/3，`LC_ALL=C`）— tainted 宽流经 `recover()` 的 `fflush`（第 633 条）。**`B_04_moved_ostreambuf`**（a/b/s/f/r/w）⭐ — 第 609 条同族两层。
  **`B_05_reset_fullbuf`**（1/2/3）— `reset()` 全缓冲 + `/dev/full` 不会一次失败记两流。**`B_06_unwind_sync`** — 栈展开中搬运失败 + 掩码。
  **`B_07_exit_failed`** — 失败态流退出丢自缓冲字节（第 633 条，libstdc++ 同）。
- **`G_s3_tainted_fflush`** ⭐ — 主控：第 633 条——tainted `wcout` 的搬运经 `recover()` `fflush`（pending 1→0；`/dev/full` 下 `P`+`A` 丢、`0x2` 记在 `wcout`），
  未 tainted 对照 1→2。其余存疑用 `B_01` b/`B_07`/`A_10`/`A_07` 重跑。

### 第二十五轮探针索引（Mac 侧 `~/memory/iov2-round25-probes/`，不在本仓库；本机已不存在）

在 arm64 CI 镜像里跑（`podman run … localhost/iov2-ci-arm64:local`，gcc 15.3 / clang 19.1 / glibc）；`incomplete.cpp` 另在 Apple clang 21 / libc++ 上跑过。

- **`glibc_oc.c`** ⭐ / **`oc_op.cpp`** ⭐ — 第 635 条：glibc 对 `%OC` / `%Op` 的渲染与四个 locale 上 IOv2 的 `%c` 逐项对比。
- **`incomplete.cpp`** ⭐ / **`sticky_kinds.cpp`** ⭐ — 第 636 条：`default_initializable<不完整>` libstdc++ 静默 false、libc++ 硬报错，两种探测都会「粘住」。
- **`write_count.cpp`** ⭐ / **`ignore_cmp.cpp`** ⭐ — 第 637 条：`write(s[8], 1 MiB)` 的越界读；`ignore(-1)` 与 libstdc++ 对拍。
- **`seek_bits.cpp`** ⭐ — 第 638 条：`seek` / `rseek` 在两种设备 × 三种偏移下的状态位。

### 第二十六轮探针索引（`.review/` 于 2026-09-24 删除前内联）

- **`A_nb`** ⭐ — 第 649 条：stdin 设 `O_NONBLOCK` 的管道，两种 sync 模式 × 十种形态。
- **`A_inj2`** ⭐ — 第 649 条：`--wrap=_Znwm` 第 k 次分配失败扫描，`printf '42 X' | ./A_inj2 {c|w} {s|u} k`。
- **`B_609_family`** ⭐ — 第 648 条的前身：根层与 `code_cvt_stdio` 层的移动赋值 / 自移动；影子头去 `m_active`、去自赋值守卫即反向验证。
- **`B_iso2022`** ⭐ — 第 646、650 条：ISO-2022-JP 下 `exit` / `reset` / `taint` 三腿的字节，影子头恢复 `m_state` 作对照。
- **`C_lo_tu1` + `C_lo_tu2`** ⭐ — 第 648 条的前身：两种链接序 × 加 / 不加 `init_priority(101)`。
- **`C_exit_init`** ⭐ — 第 651 条：插桩 `newlocale` 后在第 k 次时起 exit 线程；`__asan_on_error` + `SIGUSR1` 让报告完整。

### 第二十七轮探针索引（`.review/` 于 2026-09-29 删除前内联）

- **`mb.c`** ⭐ — 第 654 条：glibc `mbrtowc` 逐字节喂 `ESC $ B C f` 与整段喂的对照（上游报告的最小复现）；**`w1.cpp`** ⭐ — `wcin` 读 ISO-2022-JP 的 `get` / `>>` / `read` 矩阵。
- **`bench.cpp` / `bench8.cpp`** ⭐ — 第 654 条的开销：`-O2`、`taskset -c 2`、15 次取最小。
- **`a2b.cpp`** ⭐、**`access.cpp` / `access2.cpp`** ⭐ — 第 655 条：切换后逐字符读与状态位；`requires` 探测钩子时非 public 且缺友元的派生钩子被静默跳过。
- **`p5_shared_fd_shift.cpp`** ⭐、**`d1_std.cpp`** ⭐、**`d1_big.cpp`** ⭐ — 第 657 条：共用 fd 的两条宽流各持移位态，与 libstdc++ 对照。
- **`B_sync_failure/probe.cpp`** ⭐ — 第 660 条：`--wrap` 注入主矩阵与 k 扫描。**`drv.cpp` + `tgt.cpp`** ⭐ — 第 659、661 条：forkpty 驱动 `O_NONBLOCK` tty。**`s5_consumer.cpp`** ⭐ — 漏宏自检四格。

### 第二十八轮探针索引（`.review/` 于 2026-10-01 删除前内联）

- **`w.cpp`** ⭐ / **`kf.cpp`** ⭐ — 第 667-669 条：`wcin` 读法矩阵；kernel 随机分块差分（`argv`: locale 名 [`redundant`] 或 `8` = `char8_t` kernel）。
- **`b1_adjust_fail.cpp`** ⭐ — 第 671 条：`--wrap=duplocale` 第几次失败 × 方向。
- **`p3_edge.cpp`** ⭐ / **`p1_fail.cpp`** ⭐ — 第 666、672 条：`MB_CUR_MAX` 低报越界；写复位序列时 `/dev/full` / 已关 / 管道失败。
- **`p2/run.sh`** ⭐ — 第 673 条：漏宏配方矩阵（26 个对照二进制）。**`s.cpp`** ⭐ — 第 679 条：libstdc++ / libc++ / glibc `fgetwc` 读 `ab\xe6` 的对照。

### 第二十九轮探针索引（`.review/` 于 2026-10-04 删除前内联）

- **`a2_peek_reset.cpp`** ⭐（`printf '12x34' | ./a2 1`，`reset()` 丢偷看的字符）、**`a3_switch_env.cpp`** ⭐（`LC_ALL=xx_YY.bogus` 下启动与 `switch_code("")`）— 第 686、687 条。
- **`p1_dup.cpp`** ⭐ — 第 682 条：`clog` 在 `RLIMIT_FSIZE=5` 下的重复与丢失，`argv` `std` 为 stdio 对照。
- **`flock.cpp`** ⭐（`flockfile` 锁序死锁）、**`ferr.cpp`** ⭐（`clearerr` 抹掉 `ferror`）— 第 683、684 条。

### 第三十轮探针索引（`.review/` 于 2026-10-06 删除前内联）

- **`a1_deverr.cpp`** ⭐（`--wrap=read` 让第 2 次 `read(0)` 置 `EIO`）、**`a1_pty.cpp`** ⭐（pty 挂断，加 `-D_XOPEN_SOURCE=700`）、**`b_ff.cpp`** ⭐（`printf 'a\x81\x30\x41b' | LC_ALL=zh_CN.gb18030 ./b_ff g`）— 第 694、695 条。
- **`p1b_single.cpp`** ⭐ — 第 693 条：单线程复现钩子后的同步切换。**`b1_late_nosync.cpp`** ⭐ — 第 693 条 S-1：`-DUSE_STD` 为 `std` 对照。

### 第三十一轮探针索引（`.review/` 于 2026-10-07 删除前内联）

- **`p1_basic.cpp`** ⭐（`argv` 模式 `a`-`e`：预读后切同步、`reset()` 保留模式、停在多字节字符中间、同步不多读、切同步后 `reset()` 丢预读）、**`p2_adjust_bypass.cpp`** ⭐ — 第 701、706 条。
- **`b1_switch_ret.cpp`** ⭐、**`b3_hook_leftover_sync.cpp`** ⭐（`./b3 s|n 2>file | cat`，stderr 必须是普通文件）— 第 702、705 条。
- **`cs1.cpp`** ⭐ — 第 707 条：另一线程持锁时 fork，`argv` `iov2|std|stdio`。

### 第三十二轮探针索引（`.review/` 于 2026-10-08 删除前内联）

- **`a1_tsan_hook_wcin.cpp`** ⭐ — 第 718 条 A：钩子与 `wcin` 读并发，sync / nosync × get / fmt，`pc` 为阳性对照。
- **`p1_hook_giveup.cpp`** ⭐ — 第 715 条：`atexit(late)` 在 `constructor(101)` 登记，`./p1 s|u|uc|sl|ul > file`（stdout 须为普通文件）。**`p6d_move.cpp`** ⭐ — 第 713 条：`std::move(IOv2::cout)` 切片移动。
- **`plug/`** ⭐ — 第 712 条：`plugin.cpp`（形态 `tie|facet|coutloc|coutnum`）+ `host.cpp` / `host2.cpp`；**`plug_recheck.sh`** ⭐ — 对已安装的 `libiov2.so` 复跑这些形态。
- **`fp.cpp`** ⭐ — 第 718 条 D：六种 fork 形态，子进程带 alarm；去掉重建的影子头为阳性对照。

### 第三十三轮探针索引（`.review/` 于 2026-10-09 删除前内联）

- **`A-1_taint_recover.cpp`** ⭐ — 第 723、724 条：自定义 `faulty_root` 让 `attach` / `adjust` 抛出。已由常驻用例 `IoObjectsWchar.ASwitchWhoseRecoveryFailsKeepsTheMode` 取代（无参模式）。
- **`B-tiedto-reentry.cpp`** ⭐ — 第 722 条：覆写 `tied_to()` 里调 `tie()` 挂住（`timeout 5` → rc=124）。
- **`B-exit-window.cpp`** ⭐ — 第 724 条 B 的 `cout` 窗口探针，第三十四轮推广为下面的六流版。
- **`C-pin-*.cpp` + `C-dlopen-shim.c`** ⭐ — 第 721 条：用 `exec -a` 改 `argv[0]`。已由常驻用例 `PluginUnload.TheMainProgramsPinLeavesArgv0Alone` 覆盖 `/dev/stdin` 一格。

### 第三十四轮探针索引（`.review/` 于 2026-10-09 删除前内联）

- **`r34-B-1_query.cpp`** ⭐ — 第 727 条：`-Wl,--wrap=fwrite` 让切换线程停在搬运里。已由常驻用例 `StdObjectsSync.{,Wcout}SyncedWithStdioIsFalseUntilTheHandOverIsDone` 取代。
- **`r34-main-a1.cpp`** / **`r34-main-a2.cpp`** — 第 728、729 条的主控复现，已由常驻用例取代。
- **`r34-B-2_exit_window.cpp` + `r34-B-2_run.sh`** ⭐ — 第 731 条，唯一能复现真实线程窗口的手段，源码内联如下。

**第 731 条的 gdb 窗口探针**（手工回归用；`g++ -std=c++23 -O0 -g -Iinclude -I/usr/include/botan-2 -D_POSIX_C_SOURCE=200809L r34-B-2_exit_window.cpp -lbotan-2 -pthread -o r34-B-2_exit_window`，
再 `bash r34-B-2_run.sh <cout|cerr|clog|wcout|wcerr|wclog> <sync|nosync> go [touch]`；修复前 `cout`/`wcout` 的 `nosync` 两格 fd1 为空，修复后与其余格一样为 `PRE|LATE`；`touch` 先经 stdio 写一次 stdout）：

```cpp
// r34 B-2: extends round-33 B-exit-window (cout only) to all six output streams.
// gdb stops at _exit (after every atexit handler, the six exit hooks and glibc's
// _IO_cleanup), lets only the worker -- which held the stream's lock across exit, so the hook
// gave up -- finish one insertion, then lets _exit run. argv: <stream> <sync|nosync>.
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/char_and_str.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>
std::atomic<int> ready{0};
volatile int go = 0;
extern "C" __attribute__((noinline)) void worker_done() { asm volatile(""); }

template <typename S, typename C>
void run(S& s, const C* pre, const C* late, bool nosync)
{
    if (nosync) s.sync_with_stdio(false);
    s << pre;                         // before exit: buffered (nosync) or handed over (sync)
    std::thread([&s, late]{
        std::unique_lock lk(s.io_mutex());
        ready = 1;
        while (!go) ;
        s << late;
        lk.unlock();
        worker_done();
        for (;;) pause();
    }).detach();
    while (!ready) ;
}

int main(int argc, char** argv)
{
    const std::string w = argv[1];
    const bool nosync = std::strcmp(argv[2], "nosync") == 0;
    if (argc > 3 && std::strcmp(argv[3], "touch") == 0)
        std::fputs("T|", stdout);     // orients stdout before exit (round 33's probe printf'd)
    if (w == "cout")  run(IOv2::cout,  "PRE|",  "LATE\n",  nosync);
    if (w == "cerr")  run(IOv2::cerr,  "PRE|",  "LATE\n",  nosync);
    if (w == "clog")  run(IOv2::clog,  "PRE|",  "LATE\n",  nosync);
    if (w == "wcout") run(IOv2::wcout, L"PRE|", L"LATE\n", nosync);
    if (w == "wcerr") run(IOv2::wcerr, L"PRE|", L"LATE\n", nosync);
    if (w == "wclog") run(IOv2::wclog, L"PRE|", L"LATE\n", nosync);
    return 0;
}
```

```bash
#!/bin/bash
# usage: r34-B-2_run.sh <stream> <sync|nosync> <go|nogo>
cd "$(dirname "$0")"
s=$1; m=$2; g=$3
o=r34-B-2_out.$s.$m.$g.${4:-plain}; rm -f $o.1 $o.2
{
  echo 'set pagination off'; echo 'set confirm off'; echo 'set breakpoint pending on'
  echo 'set debuginfod enabled off'; echo 'break _exit'
  echo "run $s $m $4 > $o.1 2> $o.2"
  if [ "$g" = go ]; then
    echo 'set var go = 1'; echo 'set scheduler-locking on'; echo 'thread 2'
    echo 'break worker_done'; echo 'continue'; echo 'thread 1'; echo 'set scheduler-locking off'
  fi
  echo 'delete'; echo 'continue'; echo 'quit'
} > $o.gdb
timeout 60 gdb -batch -nx -x $o.gdb ./r34-B-2_exit_window > $o.gdblog 2>&1
hit=$(grep -c 'worker_done' $o.gdblog)
printf '%-6s %-6s %-4s fd1=[%s] fd2=[%s] worker_done_hits=%s\n' $s $m $g "$(tr '\n' '/' < $o.1)" "$(tr '\n' '/' < $o.2)" $hit
```


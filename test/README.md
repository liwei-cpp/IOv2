# IOv2Test: Build Instructions

[中文](#中文) | [English](#english)

---

## 中文

### 构建系统

本目录的测试由顶层的 CMake/CTest 构建，配置以 preset 形式给出（见仓库根目录的
`CMakePresets.json`）。189 个源文件被分成 **57 个套件**，每个套件对应一个目录，
CTest 中的名字就是该目录的点分形式：`concur`、`facet.collate`、`io.istream.read`。

构建产物一律在 `build/<preset>/` 下，不写进源码树。所有命令都在**仓库根目录**执行，
不是本目录。

### 快速开始

```bash
cmake --preset gcc-release                     # 配置
cmake --build --preset gcc-release --parallel "$(nproc)"  # 编译全部 57 个套件
ctest --preset gcc-release --parallel          # 运行全部
```

`cmake --list-presets` 列出全部可用配置。

`--parallel` 后面**必须给数字**。CMake 默认的生成器是 Unix Makefiles，不带数字时它把
裸 `-j` 交给 make，而裸 `-j` 对 GNU make 意为「不限并发」。全新构建时 235 个翻译单元
同时就绪，每个峰值接近 1GB，make 会一口气全部 fork——在 16GB 的机器上这会触发 OOM，
而被内核挑中杀掉的往往不是编译器，是你的桌面会话。增量构建看不出来，因为同一时刻就
绪的目标没几个。

### 预设一览

| preset | 编译选项 | 用途 |
|---|---|---|
| `gcc-release` / `clang-release` | `-O3 -g -DNDEBUG` | 默认，CI 的主力 |
| `gcc-debug` / `clang-debug` | `-g -O0` | 本地调试 |
| `gcc-sanitizer` / `clang-sanitizer` | `-O1 -g` + ASan/UBSan（**无** `NDEBUG`） | 越界、未定义行为 |
| `gcc-tsan` | `-O1 -g` + TSan，只编 `concur` | 数据竞争 |
| `gcc-coverage` | `-O0 -g --coverage` | 覆盖率 |
| `gcc-installed-shared` | release，链接已安装的 `libiov2.so` | 验证安装产物 |
| `gcc-installed-header-only` | release，只用已安装的头文件 | 验证安装产物 |

模式由 `IOV2_TEST_MODE` 决定，不是 `CMAKE_BUILD_TYPE`（后者被强制清空：sanitizer
模式是 `-O1 -g` 且不带 `NDEBUG`，没有任何标准 build type 对应）。

### 只跑一部分

```bash
ctest --preset gcc-release -L io               # 按模块标签：io 下的全部套件
ctest --preset gcc-release -R '^io\.ostream'   # 按名字正则
ctest --preset gcc-release -R '^facet\.collate$' -V   # 单个套件，输出全部
ctest --preset gcc-release --rerun-failed      # 只重跑上次失败的
```

可用标签：`all`（每个套件都有）、七个模块名 `common` `concur` `cvt` `device`
`facet` `io` `locale`，以及 `thread`（只有 `concur`）。

只编不跑某个套件，用目标名（点换成下划线并加 `test_` 前缀）：

```bash
cmake --build --preset gcc-release --target test_facet_collate
```

### Valgrind

```bash
ctest --test-dir build/gcc-release -T memcheck --output-on-failure -L all --parallel
```

注意是 `--test-dir` 而不是 `--preset`：`-T memcheck` 让 ctest 进入 dashboard 模式，
该模式从当前工作目录解析工程目录、忽略 test preset 的 `binaryDir`，给了 `--preset`
反而会找不到 `DartConfiguration.tcl` 而报 “Memory checker not set”。

判据是 **ctest 的退出码**，不是日志：`Testing/Temporary` 下没有任何文件记录缺陷计数。

### 覆盖率

```bash
cmake --preset gcc-coverage
cmake --build --preset gcc-coverage --parallel "$(nproc)"
ctest --preset gcc-coverage --parallel
lcov --capture --directory build/gcc-coverage --output-file coverage.info \
     --rc branch_coverage=1 \
     --ignore-errors mismatch --ignore-errors inconsistent --ignore-errors negative
lcov --extract coverage.info "${PWD}/include/*" --output-file coverage.info \
     --rc branch_coverage=1 --ignore-errors unused
lcov --list coverage.info --rc branch_coverage=1
```

`--rc branch_coverage=1` 要在每一条 lcov 命令上都给：分支数据本来就在 `.gcda` 里，
lcov 默认丢弃而已，所以加上它不需要重新编译，也不会改变行覆盖——只是多出 `BRDA`
记录。重写批次的验收要比 line/function/branch 三项。

`--extract` 的模式必须锚定在仓库路径上。写成 `*/include/*` 会把
`/usr/local/include/c++/...` 一并匹配进来，多出上百个 libstdc++ 头文件。

### ThreadSanitizer

```bash
cmake --preset gcc-tsan
cmake --build --preset gcc-tsan --parallel "$(nproc)"
ctest --preset gcc-tsan
```

只编 `concur`：其它套件不起线程，TSan 无可观察。test preset 自带 `until-fail` 重复
10 次——竞争只有在调度真的撞上时才会被报出来，跑一次干净说明不了什么。反方向是可靠的：
TSan 是 happens-before 检测器，不会凭空造出竞争，所以报红就是真缺陷。

### 新增一个测试文件

1. 把 `.cpp` 放进它所属的目录。同一目录下的所有 `.cpp` 自动进入该目录对应的套件，
   glob 带 `CONFIGURE_DEPENDS`，不需要登记文件名。
2. 新建目录 = 新建套件，无需额外配置。
3. 用 `TEST(前缀, 用例名)` 写用例，它自己会注册，不需要在任何地方登记。新建目录时
   重新生成一次清单（`test/suites.cmake` 由脚本生成，**不要手改**）：

   ```bash
   python3 test/tools/derive_suites.py          # 重新生成
   python3 test/tools/derive_suites.py --check  # 校验：与源码不符则失败
   ```

4. 重新配置一次（`cmake --preset gcc-release`），CMake 会核对源文件总数并检查每个
   `.cpp` 都归属于某个套件。数量对不上或有文件无人认领，配置阶段直接报错——这两种
   情况都会让测试悄悄变少而 CI 仍然全绿。

全部套件都已是 GoogleTest，用例自行注册，所以入口表是空的，当年那七个聚合 `main()`
和纯派发文件也都删掉了。清单现在只剩两件事：套件目录清单，以及哪些是 GoogleTest。

### 对着已安装的库跑

```bash
make install-shared PREFIX=/opt/iov2                 # 顶层 Makefile 负责打包安装
export PKG_CONFIG_PATH=/opt/iov2/lib/pkgconfig
export LD_LIBRARY_PATH=/opt/iov2/lib
cmake --preset gcc-installed-shared
cmake --build --preset gcc-installed-shared --parallel "$(nproc)"
ctest --preset gcc-installed-shared --parallel
```

头文件模式同理，用 `make install` 与 `gcc-installed-header-only`。这两种模式下源码树的
`include/` 不参与编译，CMake 会拒绝落在源码树内的 pkg-config 前缀。

### 清理

```bash
rm -rf build/gcc-release   # 单个配置
rm -rf build              # 全部
```

---

## English

### Build System

The tests here are built by the top-level CMake/CTest; configurations are given as
presets (see `CMakePresets.json` in the repository root). The 189 sources are split
into **57 suites**, one per directory, and a suite's CTest name is that directory in
dotted form: `concur`, `facet.collate`, `io.istream.read`.

Build output always goes to `build/<preset>/`, never into the source tree. Run every
command below from the **repository root**, not from this directory.

### Quick Start

```bash
cmake --preset gcc-release                     # configure
cmake --build --preset gcc-release --parallel "$(nproc)"  # build all 57 suites
ctest --preset gcc-release --parallel          # run them
```

`cmake --list-presets` lists every available configuration.

**Always give `--parallel` a number.** CMake's default generator here is Unix Makefiles,
and without one it hands make a bare `-j`, which to GNU make means *unlimited*. On a
clean build all 235 translation units are ready at once and each peaks near 1GB, so make
forks the lot; on a 16GB machine that ends in the OOM killer, and what it reaps is
usually not the compiler but your desktop session. Incremental builds hide this, because
only a handful of targets are ever ready together.

### Presets

| Preset | Flags | Purpose |
|---|---|---|
| `gcc-release` / `clang-release` | `-O3 -g -DNDEBUG` | default; what CI runs |
| `gcc-debug` / `clang-debug` | `-g -O0` | local debugging |
| `gcc-sanitizer` / `clang-sanitizer` | `-O1 -g` + ASan/UBSan (**no** `NDEBUG`) | out-of-bounds, UB |
| `gcc-tsan` | `-O1 -g` + TSan, builds `concur` only | data races |
| `gcc-coverage` | `-O0 -g --coverage` | coverage |
| `gcc-installed-shared` | release, against an installed `libiov2.so` | validates the install |
| `gcc-installed-header-only` | release, against installed headers only | validates the install |

The mode comes from `IOV2_TEST_MODE`, not `CMAKE_BUILD_TYPE` (which is forced empty:
the sanitizer mode is `-O1 -g` with `NDEBUG` absent, which no standard build type
produces).

### Running a Subset

```bash
ctest --preset gcc-release -L io               # by module label: every suite under io
ctest --preset gcc-release -R '^io\.ostream'   # by name, as a regex
ctest --preset gcc-release -R '^facet\.collate$' -V   # one suite, full output
ctest --preset gcc-release --rerun-failed      # only what failed last time
```

Labels: `all` (on every suite), the seven module names `common` `concur` `cvt`
`device` `facet` `io` `locale`, and `thread` (on `concur` alone).

To build one suite without running it, use its target name -- dots become
underscores, with a `test_` prefix:

```bash
cmake --build --preset gcc-release --target test_facet_collate
```

### Valgrind

```bash
ctest --test-dir build/gcc-release -T memcheck --output-on-failure -L all --parallel
```

`--test-dir`, not `--preset`: `-T memcheck` puts ctest in dashboard mode, which
resolves the project directory from the working directory and ignores a test
preset's `binaryDir`. Given `--preset` it looks for `DartConfiguration.tcl` beside
the sources, does not find it, and fails with "Memory checker not set".

The verdict is **ctest's exit code**, not the log: no file under
`Testing/Temporary` carries the defect count.

### Coverage

```bash
cmake --preset gcc-coverage
cmake --build --preset gcc-coverage --parallel "$(nproc)"
ctest --preset gcc-coverage --parallel
lcov --capture --directory build/gcc-coverage --output-file coverage.info \
     --rc branch_coverage=1 \
     --ignore-errors mismatch --ignore-errors inconsistent --ignore-errors negative
lcov --extract coverage.info "${PWD}/include/*" --output-file coverage.info \
     --rc branch_coverage=1 --ignore-errors unused
lcov --list coverage.info --rc branch_coverage=1
```

`--rc branch_coverage=1` goes on every lcov call. The branch data is already in
the `.gcda`; lcov drops it by default, so adding the flag needs no rebuild and
cannot change line coverage -- it only adds `BRDA` records. Rewrite batches are
accepted on a line/function/branch comparison.

The `--extract` pattern has to be anchored at the repository. Written as
`*/include/*` it also matches `/usr/local/include/c++/...` and drags in a hundred-odd
libstdc++ headers.

### ThreadSanitizer

```bash
cmake --preset gcc-tsan
cmake --build --preset gcc-tsan --parallel "$(nproc)"
ctest --preset gcc-tsan
```

Only `concur` is built: no other suite starts a thread, so TSan has nothing to
observe. The test preset repeats it ten times with `until-fail` -- a race is only
reported when the schedule actually exercises it, so one clean run is weak evidence.
The reverse direction is reliable: TSan is a happens-before detector and does not
invent races, so a red result is a real defect.

### Adding a Test File

1. Put the `.cpp` in the directory it belongs to. Every `.cpp` in a directory joins
   that directory's suite automatically; the glob uses `CONFIGURE_DEPENDS`, so no
   file has to be registered by name.
2. A new directory is a new suite, with no further configuration.
3. Write the case as `TEST(Prefix, CaseName)`; it registers itself and does not
   have to be listed anywhere. When adding a directory, regenerate the manifest
   (`test/suites.cmake` is generated and **must not be edited by hand**):

   ```bash
   python3 test/tools/derive_suites.py          # regenerate
   python3 test/tools/derive_suites.py --check  # verify against the sources
   ```

4. Configure once more (`cmake --preset gcc-release`). CMake checks the total source
   count against the manifest and that every `.cpp` is claimed by some suite; either
   a wrong count or an unclaimed file is a hard error at configure time. Both are
   ways to silently run fewer tests while CI stays green.

Every suite is GoogleTest now, so the cases register themselves, the entry lists
are empty, and the seven aggregate `main()`s of the old build -- along with the
dispatch-only files under them -- have been deleted. What the manifest still
carries is the list of suite directories and which of them are GoogleTest.

### Running Against an Installed Library

```bash
make install-shared PREFIX=/opt/iov2                 # the top-level Makefile packages and installs
export PKG_CONFIG_PATH=/opt/iov2/lib/pkgconfig
export LD_LIBRARY_PATH=/opt/iov2/lib
cmake --preset gcc-installed-shared
cmake --build --preset gcc-installed-shared --parallel "$(nproc)"
ctest --preset gcc-installed-shared --parallel
```

Header-only mode is the same with `make install` and `gcc-installed-header-only`. In
both modes the source tree's `include/` takes no part in the build, and CMake refuses
a pkg-config prefix that lies inside the source tree.

### Cleaning

```bash
rm -rf build/gcc-release   # one configuration
rm -rf build               # all of them
```

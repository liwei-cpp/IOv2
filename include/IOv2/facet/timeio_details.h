// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file timeio_details.h
 * @lang{ZH}
 * `timeio` facet 的实现细节，包含：
 * - `base_ft<timeio>`：不带字符类型的中间基类特化，持有所有字符类型共用的时区前缀树；
 * - `ft_basic<timeio<CharT>>`：`timeio` 的基类模板特化，持有 facet 类型标识；
 * - `TimeioHelper`：日历纪元日期比较的辅助工具函数；
 * - `timeio_conf<CharT>`：各字符类型（`char`、`wchar_t`/`char32_t`、`char8_t`）的
 *   locale 配置类，负责从系统 locale（经平台后端，见 `facet/platform/timeio_backend.h`）
 *   或 C/POSIX 硬编码默认值中加载日期/时间格式串、星期/月份名称、AM/PM 字符串、替代数字
 *   及纪元数据。
 * @endif
 *
 * @lang{EN}
 * Implementation details for the `timeio` facet, comprising:
 * - `base_ft<timeio>`: the character-type-independent intermediate base specialization,
 *   holding the timezone prefix trie shared by every character type;
 * - `ft_basic<timeio<CharT>>`: base class template specialization for `timeio`,
 *   holding the facet type-identity token;
 * - `TimeioHelper`: helper utilities for calendar era date comparison;
 * - `timeio_conf<CharT>`: per-character-type locale configuration classes
 *   (`char`, `wchar_t`/`char32_t`, `char8_t`) that load date/time format strings,
 *   weekday/month names, AM/PM strings, alternative digits, and era data from the
 *   system locale (through the platform backend, see `facet/platform/timeio_backend.h`)
 *   or from C/POSIX hard-coded defaults.
 * @endif
 */
#pragma once
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/defs.h>
#include <IOv2/common/iov2_export.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/common/prefix_tree.h>
#include <IOv2/cvt/cvt_facilities.h>
#include <IOv2/facet/facet_common.h>
#include <IOv2/facet/facet_helper.h>
#include <IOv2/facet/platform/timeio_backend.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2
{
    template <typename CharT> class timeio;

    /**
     * @lang{ZH}
     * @brief 时区前缀树的值：`%Z` 匹配到的那串文本，加上它在 tzdb 里的身份。
     *
     * `is_name` 与 `is_abbrev` 是两个**互相独立**的谓词，不是三选一：
     * - `is_name`：`std::chrono::locate_zone()` 认这个标识符，即它在 `tzdb.zones` 或
     *   `tzdb.links` 里；
     * - `is_abbrev`：它是某个时区在某一时刻的 `std::chrono::sys_info::abbrev`。
     *
     * 两者同时成立的条目确实存在——本机 8 条（`CET`、`EET`、`EST`、`GMT`、`HST`、`MST`、
     * `UTC`、`WET`），它们既是 link 名又是缩写——所以这里必须是两个标志位，一个三值枚举
     * 表达不了。两者都不成立的那一格留给 @ref base_ft<timeio>::s_unknown_zone：那个记号
     * 解析得出来，但它什么区也不指。
     *
     * @ref text 是键的原文，**逐字节等于输入**，且随树活到程序结束，因此 `text.c_str()`
     * 可以直接交给 `std::tm::tm_zone` 这类只收 `const char*`、又没有配套释放接口的字段。
     * 它**不做规范化**：解析 `US/Pacific` 得到的就是 `US/Pacific`，解析 `EST` 得到的就是
     * `EST`。要规范名请走 `std::chrono::locate_zone(text)->name()`——`locate_zone` 自己
     * 会归一化，没有理由在树里再存一份。
     * @endif
     *
     * @lang{EN}
     * @brief A time-zone trie value: the text `%Z` matched, plus what the tzdb says it is.
     *
     * `is_name` and `is_abbrev` are two **independent** predicates, not a three-way choice:
     * - `is_name`: `std::chrono::locate_zone()` accepts this identifier, i.e. it appears in
     *   `tzdb.zones` or in `tzdb.links`;
     * - `is_abbrev`: it is the `std::chrono::sys_info::abbrev` of some zone at some instant.
     *
     * Entries where both hold do exist -- eight of them here (`CET`, `EET`, `EST`, `GMT`,
     * `HST`, `MST`, `UTC`, `WET`), each both a link name and an abbreviation -- so two flags
     * are required and a three-valued enumeration cannot express this. The combination where
     * neither holds is reserved for @ref base_ft<timeio>::s_unknown_zone: that token parses,
     * but it names no zone at all.
     *
     * @ref text is the key verbatim, **byte for byte what was parsed**, and it lives as long
     * as the trie does, i.e. for the whole program. `text.c_str()` may therefore be handed
     * straight to a field such as `std::tm::tm_zone`, which takes a `const char*` and offers
     * no matching release call. It is **not canonicalized**: parsing `US/Pacific` yields
     * `US/Pacific` and parsing `EST` yields `EST`. For the canonical name call
     * `std::chrono::locate_zone(text)->name()` -- `locate_zone` normalizes on its own, so
     * there is no reason to keep a second copy in the trie.
     * @endif
     */
    struct zone_ref
    {
        /// @lang{ZH} 键的原文；`UNKNOWN` 记号为空串。 @endif
        /// @lang{EN} The key verbatim; empty for the unknown-zone token. @endif
        std::string text;

        /// @lang{ZH} tzdb 认得这个标识符（`zones` 或 `links` 中）。 @endif
        /// @lang{EN} The tzdb knows this identifier (in `zones` or in `links`). @endif
        bool is_name = false;

        /// @lang{ZH} 它是某个时区在某一时刻的缩写。 @endif
        /// @lang{EN} It is some zone's abbreviation at some instant. @endif
        bool is_abbrev = false;

        /// @cond
        bool operator==(const zone_ref&) const = default;
        /// @endcond
    };

    /**
     * @lang{ZH}
     * @brief `timeio` 的中间基类特化，持有与字符类型无关的时区数据。
     *
     * `base_ft<TFacet>` 是 `abs_ft` 与各 `ft_basic<TFacet<CharT>>` 之间的桥接层（见
     * facet_common.h），同一 facet 模板的所有字符特化共享同一个它。放在这里的东西因此
     * 全程序只有一份：
     * - @ref s_unknown_zone：`%Z` 在区名未知时写出的记号；
     * - @ref s_timezone_tree：时区名与缩写的前缀树。
     *
     * 这两项都以字节（`char`）表述，与 `CharT` 无关：tzdb 里的区名、缩写与 link 全在
     * ASCII 范围内，各字符类型看到的是同一串码点，没有按字符类型各存一份的理由。早先
     * 每个 `CharT` 一棵树，四种字符类型就要走四遍 tzdb、建四棵内容相同的树。
     *
     * @note 树以字节为键，宽字符流的查找由 `prefix_tree::max_match` 的往返检查处理，
     *       只有 ASCII 条目能这样命中——对 tzdb 数据成立，见该函数的说明。
     * @endif
     *
     * @lang{EN}
     * @brief The intermediate base specialization for `timeio`, holding its
     *        character-type-independent time-zone data.
     *
     * `base_ft<TFacet>` is the bridge between `abs_ft` and each `ft_basic<TFacet<CharT>>`
     * (see facet_common.h), and every character specialization of one facet template shares
     * a single one of it. What lives here therefore exists once per program:
     * - @ref s_unknown_zone: the token `%Z` writes when the zone name is unknown;
     * - @ref s_timezone_tree: the prefix trie of zone names and abbreviations.
     *
     * Both are spelled in bytes (`char`) and owe nothing to `CharT`: every zone name,
     * abbreviation and link in the tzdb is within ASCII, so each character type sees the very
     * same code points and there is no reason to keep one copy per type. The trie used to be
     * a member per `CharT`, which walked the tzdb four times over and built four identical
     * tries for a program using four character types.
     *
     * @note The trie is keyed on bytes; lookups from a wide stream go through the round-trip
     *       check in `prefix_tree::max_match`, so only ASCII entries can match that way --
     *       true of tzdb data. See that function for the details.
     * @endif
     */
    template <>
    class base_ft<timeio> : public abs_ft
    {
    public:
        using abs_ft::abs_ft;

    public:
        /**
         * @lang{ZH}
         * @brief `%Z` 在「字段在、但没有内容」时写出的记号。
         *
         * 只有 `std::tm` 用得上：当前平台的 `std::tm` 带 `tm_zone` 成员时，这个类型在
         * 类型层面就宣称装得下区名，于是 `%Z` 必须给出一个答案；而具体这一个 `tm` 的
         * `tm_zone` 可能是空指针或空串，此时写出本记号，表示「区名未知」。
         *
         * 之所以不退化为字面 `%Z`：那两个字符出现在给人看的输出里太难看，而 `%c` 之类的
         * locale 复合格式里是否含 `%Z` 并不由调用方决定。之所以不回退到进程本地时区
         * （glibc 的 `strftime` 会查 `tzname[tm_isdst != 0]`）：那会把一个与该时间毫无关系
         * 的区名安上去，`tm_gmtoff` 为 `+0800` 的值在美国西岸的机器上会写出 `PST`。
         *
         * 本记号也在 @ref s_timezone_tree 中注册，映射到一个 @ref zone_ref{"", false, true}，
         * 因此写得出就读得回。解析到它时 `m_zone_abbrev` 指向**空串**而不是留 `nullptr`：
         * 这两者含义不同——`nullptr` 是「`%Z` 压根没解析到」，空串是「解析到了，而且它明说
         * 没有时区」。后者会把 `tm_zone` 写成空串，从而与 put 侧闭合；若什么都不写，调用方
         * `tm` 里上一次留下的 `tm_zone` 会残留，下一次 put 就写出那个陈旧的区名，往返断裂。
         *
         * @note 不会与真实时区记号冲突：对 tzdb 的全部区名、缩写与 link（本机 769 条）
         *       逐一比对过，无相等项，也无任一方向的前缀关系。这一条不只是观察结论——
         *       建树时会实际检查，一旦哪天的 tzdata 破坏了它，@ref s_timezone_tree 宁可
         *       整棵留空（`%Z` 全部报可捕获的 `stream_error`），也不悄悄错一条。
         * @endif
         *
         * @lang{EN}
         * @brief The token `%Z` writes when the field is there but holds nothing.
         *
         * Only `std::tm` ever needs it: when this platform's `std::tm` carries a `tm_zone`
         * member, the type declares at the type level that it can hold a zone name, so `%Z`
         * owes an answer; yet this particular `tm` may have a null or empty `tm_zone`, and
         * then this token is written to say the name is unknown.
         *
         * Why not degrade to a literal `%Z`: those two characters are ugly in output meant for
         * a human, and whether a locale composite such as `%c` contains a `%Z` is not the
         * caller's choice. Why not fall back to the process's local zone (glibc's `strftime`
         * consults `tzname[tm_isdst != 0]`): that attaches a zone name unrelated to the value,
         * so a `tm` whose `tm_gmtoff` is `+0800` would print `PST` on a machine in California.
         *
         * The token is also registered in @ref s_timezone_tree, mapped to a
         * @ref zone_ref{"", false, true}, so whatever is written can be read back. Parsing it
         * points `m_zone_abbrev` at an **empty string** rather than leaving it null, because
         * the two mean different things: null is "no `%Z` was parsed at all", empty is "one was
         * parsed, and it says outright that there is no zone". The latter writes an empty
         * `tm_zone`, closing the round trip with the put side; writing nothing would leave
         * whatever `tm_zone` the caller's `tm` already held, and the next put would emit that
         * stale zone name instead.
         *
         * @note It cannot collide with a real zone token: checked against every zone name,
         *       abbreviation and link in the tzdb (769 of them here) for equality and for a
         *       prefix relation in either direction; there is none. This is not merely an
         *       observation -- the trie builder checks it, and should some future tzdata break
         *       it, @ref s_timezone_tree is left empty (every `%Z` then reports a catchable
         *       `stream_error`) rather than quietly getting one entry wrong.
         * @endif
         */
        static constexpr std::string_view s_unknown_zone = "UNKNOWN";

        /**
         * @lang{ZH}
         * @brief 时区标识符与缩写的静态前缀树，值为 @ref zone_ref。
         *
         * 在程序启动时通过 `std::chrono::get_tzdb()` 构建，收三类键（三者可以重合，
         * 身份由 @ref zone_ref 的两个标志位表达，不是三选一）：
         * - `tzdb.zones` 里的区名；
         * - `tzdb.links` 里的别名——`US/Pacific`、`Asia/Calcutta`、`Japan` 这些
         *   `locate_zone()` 一样认的名字，本机 257 条；
         * - 每个时区走过的全部缩写。
         *
         * 值里的 @ref zone_ref::text 是键的原文，**不做规范化**：解析 `EST` 得到的就是
         * `EST`，不是 `America/Panama`。规范化的活交给 `locate_zone`，它自己会做；而原文
         * 是 `tm_zone` 往返所必需的——put 侧原样写出 `tm_zone`，get 侧要能原样还回去。
         *
         * 缩写取自每个时区**全部**转换的 `abbrev`：从 `year::min()` 年 1 月 1 日一直走到「当前时刻
         * + 10 年」（系统时钟异常偏早时以 2038-01-01 兜底）。起点不取 `sys_seconds::min()`：
         * libstdc++ 把每个时区第一段的起点定在 `year::min()` 年 1 月 1 日，早于它的时刻落在
         * 带规则的段上时 `get_info` 会抛异常（tzdata 2026d 起的 `CST6CDT` 等四个区即如此）；
         * 而这一刻之前不会再有别的转换，从这里起走不漏任何缩写。只在某一个时刻上取一次是不够
         * 的——那样只会拿到该时刻生效的那一个，夏令时缩写与南北半球各漏掉一半。不设下界是
         * 因为 put 侧的 `%Z` 原样写出 `std::tm::tm_zone`，而 `localtime()` 能表示任意时刻：
         * 树里缺哪个缩写，哪个就是本库写得出却读不回的。上界随系统时钟推移，不会因为写死
         * 一个年份而过期。
         *
         * 树里的条目互为前缀是常态（`WIT` 与 `WITA`、`Etc/GMT+1` 与 `Etc/GMT+10`、
         * `America/Bahia` 与 `America/Bahia_Banderas`），历史缩写里还有 `AT`、`CT` 这类两字母
         * 条目。这些都由最长匹配处理：`max_match` 取能匹配的最长条目，失配时完整回退，
         * 因此任何合法输入都按整体解析。不合法的输入若以某个合法缩写开头（如 `ATLANTIC`
         * 之于 `AT`），`%Z` 会读掉那个缩写就此停下，剩下的留在流里——与 `%d` 读 `12abc`
         * 只取 `12` 是同一回事，字段边界由说明符自己决定。
         *
         * 树以 `char` 为键、装的是字节，全部字符类型共用这一棵。tzdb 的区名、缩写与 link
         * 全在 ASCII 内，各字符类型看到的码点相同，所以这样是够的：宽字符流查找时，
         * `max_match` 逐字符转成 `char` 并检查能否原样转回，ASCII 一律成立。反过来说，
         * 若有朝一日往树里加了非 ASCII 条目，它在树中是 UTF-8 的各个字节，宽字符流永远
         * 匹配不上——真需要时应改为按 `CharT` 转换后再查，而不是直接加。
         *
         * header-only 下它是 inline 变量，因此包含本头文件的每个程序都会在启动时
         * 构建它，无论是否用到 `%Z`（本机实测约 16 ms、约 3.2 MB 常驻，770 个键、4095 个
         * 节点）。这是有意的取舍：换来的是全程序一棵树，而不是每个字符类型一棵。
         * `IOV2_SHARED` 下它由库定义并导出（`iov2_objects.cpp`），在库载入时建一次，整个进程
         * 一棵，不再每个包含本头文件的模块（DLL、插件）各建一棵。
         *
         * 成员本身是一个**引用**，绑定到 `new` 出来、永不 `delete` 的树：进程退出时没有任何
         * 析构函数会跑，`%Z` 在退出阶段仍可解析——标准流对象退出时不析构（见
         * `common/sing_temp.h`），它们经 `timeio` 触到的数据也必须活到最后，否则更早构造的
         * 静态对象的析构函数、或 `main` 返回后仍在运行的线程，会在已释放的树上读。
         * 树经这个静态引用可达，泄漏检测器不会报它。
         *
         * 供 `%Z` 格式说明符的解析使用。若时区数据库在静态初始化期间不可用或格式
         * 有误（如 Windows 上没有 `icu.dll`），树里只剩 @ref s_unknown_zone 与 `UTC` 两条，
         * 其余的 `%Z` 解析会在运行时产生可捕获的 `stream_error` 而非调用 `std::terminate`。
         * 标准库根本没有时区数据库（`IOV2_HAS_CHRONO_TZDB` 为 0）时，建树不去碰它，树里同样
         * 是这两条。`UTC` 是 `sys_time` 的 `%Z` 写出的内容，有没有数据库都必须读得回；有数据库时
         * 它本就在键里，带着数据库给的标记（既是名字也是缩写）。
         *
         * @note 分类先在一个 `std::map` 里做完，再一趟灌进树，因此同一个键不可能 `add`
         *       两次——`prefix_tree::add` 的重复冲突在这里结构上不可能发生，不必依赖
         *       「像缩写的名字碰巧都在 `links` 里」这类巧合。
         * @endif
         *
         * @lang{EN}
         * @brief Static prefix trie of time-zone identifiers and abbreviations, valued with
         *        @ref zone_ref.
         *
         * Built at program startup via `std::chrono::get_tzdb()` from three sources of keys
         * (which may overlap -- identity is carried by @ref zone_ref's two flags rather than by
         * a three-way choice):
         * - the zone names in `tzdb.zones`;
         * - the aliases in `tzdb.links` -- `US/Pacific`, `Asia/Calcutta`, `Japan` and the rest,
         *   names `locate_zone()` accepts just as readily, 257 of them here;
         * - every abbreviation each zone passes through.
         *
         * The @ref zone_ref::text in each value is the key verbatim and is **not canonicalized**:
         * parsing `EST` yields `EST`, not `America/Panama`. Canonicalization is `locate_zone`'s
         * job and it does it on its own, whereas the verbatim text is what a `tm_zone` round trip
         * needs -- the put side writes `tm_zone` out unchanged, so get must be able to put it back.
         *
         * The abbreviations come from the `abbrev` of **every** transition each zone goes
         * through, walked from January 1 of `year::min()` up to the current time plus ten
         * years (falling back to 2038-01-01 should the system clock read implausibly early).
         * The walk does not start at `sys_seconds::min()`: libstdc++ starts every zone's first
         * period on January 1 of `year::min()`, and `get_info` throws for an earlier instant
         * that falls on a period with rules (as the four zones `CST6CDT` and co. do since
         * tzdata 2026d); no transition comes before that instant, so starting there misses no
         * abbreviation.
         * Sampling a single instant is not enough: it yields only the one abbreviation in
         * effect then, missing every daylight-saving abbreviation and half of each hemisphere.
         * There is no lower bound because the put side of `%Z` writes `std::tm::tm_zone`
         * verbatim and `localtime()` can represent any instant: an abbreviation missing from
         * the trie is one this library can write but cannot read back. The upper bound moves
         * with the clock, so it does not go stale the way a hard-coded year would.
         *
         * Entries that are prefixes of one another are the norm here (`WIT` and `WITA`,
         * `Etc/GMT+1` and `Etc/GMT+10`, `America/Bahia` and `America/Bahia_Banderas`), and the
         * historical abbreviations add two-letter entries such as `AT` and `CT`. Longest match
         * covers all of them: `max_match` takes the longest entry that matches and backtracks
         * fully when it fails, so every valid input parses as a whole. Input that is not valid
         * but begins with a valid abbreviation (`ATLANTIC` against `AT`, say) has that
         * abbreviation consumed and the rest left in the stream -- the same thing `%d` does
         * with `12abc`, each specifier deciding where its own field ends.
         *
         * The trie is keyed on `char` and holds bytes, and every character type shares this
         * one instance. Every zone name, abbreviation and link in the tzdb is within ASCII, so
         * each character type sees the same code points and that is enough: on a lookup from a
         * wide stream `max_match` converts each character to `char` and checks that it converts
         * back unchanged, which always holds within ASCII. The converse is that a non-ASCII
         * entry, were one ever added, would sit in the trie as the individual bytes of its
         * UTF-8 form and could never match a wide stream -- should that day come, the fix is to
         * convert per `CharT` before looking up, not to add the entry as is.
         *
         * Header-only it is an inline variable, so it is built at startup by every program
         * that includes this header, whether or not it ever uses `%Z` (measured here at
         * roughly 16 ms and 3.2 MB resident, for 770 keys across 4095 nodes). That is the
         * deliberate trade: one trie per program instead of one per character type. Under
         * `IOV2_SHARED` the library defines and exports it (`iov2_objects.cpp`) and builds it
         * once, when it loads: one trie per process, not one per module (DLL, plugin) that
         * includes this header.
         *
         * The member itself is a **reference**, bound to a trie that is `new`ed and never
         * `delete`d: no destructor runs at process exit, so `%Z` still parses while the
         * process is exiting. The standard stream objects are not destroyed at exit (see
         * `common/sing_temp.h`), and the data they reach through `timeio` has to live as long,
         * or the destructor of a static object constructed earlier, or a thread still running
         * after `main` returned, would read a freed trie. The trie stays reachable through this
         * static reference, so leak checkers do not report it.
         *
         * Used by the `%Z` format specifier during parsing. If the timezone database is
         * unavailable or malformed at static-initialization time (no `icu.dll` on Windows, say),
         * the trie is left holding @ref s_unknown_zone and `UTC` alone, and any other `%Z`
         * parse produces a catchable `stream_error` at runtime rather than calling
         * `std::terminate`. With no time-zone database in the standard library at all
         * (`IOV2_HAS_CHRONO_TZDB` is 0) the builder does not touch one, and the trie holds the
         * same two keys. `UTC` is what a `sys_time`'s `%Z` writes, and it has to read back with a
         * database or without; with one it is among the keys anyway, with the database's flags
         * (both a name and an abbreviation).
         *
         * @note The classification is worked out in a `std::map` first and fed to the trie in a
         *       single pass, so no key can ever be `add`ed twice: a duplicate conflict in
         *       `prefix_tree::add` is structurally impossible here, with no reliance on
         *       coincidences such as "the abbreviation-shaped names all happen to live in
         *       `links`".
         * @endif
         */
#if defined(IOV2_SHARED)
        static IOV2_API const prefix_tree<char, zone_ref>& s_timezone_tree;   // defined in iov2_objects.cpp
#else
        static const prefix_tree<char, zone_ref>& s_timezone_tree;
#endif

    private:
        // Builds the trie s_timezone_tree is bound to, at startup: header-only right after
        // this class, in shared mode in iov2_objects.cpp. The walk catches its own exceptions.
        static prefix_tree<char, zone_ref> build_timezone_tree()
        {
            prefix_tree<char, zone_ref> res;

            // Registered ahead of the tzdb walk so that it is present even when the walk
            // throws and leaves the rest of the trie empty: the token the put side writes
            // for a nameless zone has to parse back regardless of the database. Its text is
            // empty on purpose -- see s_unknown_zone for why that is not the same as absent.
            res.add(s_unknown_zone.begin(), s_unknown_zone.end(),
                    zone_ref{.text = std::string{}, .is_name = false, .is_abbrev = true});

            constexpr std::string_view utc = "UTC";
            bool has_utc = false;
#if IOV2_HAS_CHRONO_TZDB
            try
            {
                const auto& tzdb = std::chrono::get_tzdb();

                // Classify every key before touching the trie. Two keys can name the same
                // string from different sources -- CET, EET, EST, GMT, HST, MST, UTC and WET
                // are each both a link name and an abbreviation -- and going through a map
                // merges those into one entry with both flags set, instead of calling add()
                // twice on one key and having it throw on the mismatched values.
                std::map<std::string, zone_ref> entries;

                // Every abbreviation each zone passes through.
                {
                    using namespace std::chrono;
                    const sys_seconds projected =
                        time_point_cast<seconds>(system_clock::now()) + years{10};
                    const sys_seconds floor{sys_days{2038y / January / 1}};
                    const sys_seconds horizon = (projected > floor) ? projected : floor;

                    // Not sys_seconds::min(): libstdc++ starts every zone's first period at
                    // year::min(), and an earlier instant on a period with rules throws
                    // (CST6CDT and co. since tzdata 2026d). Nothing is lost by starting here.
                    const sys_seconds start{sys_days{year::min() / January / 1}};

                    for (const auto& zone : tzdb.zones)
                    {
                        try
                        {
                            sys_seconds t = start;
                            while (t < horizon)
                            {
                                const sys_info info = zone.get_info(t);
                                if (!info.abbrev.empty()) entries[info.abbrev].is_abbrev = true;
                                if (info.end <= t) break;
                                t = time_point_cast<seconds>(info.end);
                            }
                        }
                        catch (...) {} // NOLINT(bugprone-empty-catch)
                    }
                }

                // Every identifier locate_zone() accepts. Links belong here just as much as
                // zones do: a link is a full name, merely not the canonical one, and nothing
                // downstream needs the canonical form -- locate_zone normalizes on its own.
                for (const auto& zone : tzdb.zones)
                    entries[std::string{zone.name()}].is_name = true;
                for (const auto& link : tzdb.links)
                    entries[std::string{link.name()}].is_name = true;

                // The unknown-zone token has to parse back to "no zone at all", so it cannot
                // share a key with a real one. That it does not is a documented invariant of
                // s_unknown_zone; if some future tzdata breaks it, refuse to answer rather
                // than quietly get that one entry wrong. Thrown here -- before a single tzdb
                // entry reaches the trie, and inside the catch below -- so the trie ends up
                // holding only the token and every real %Z reports a catchable stream_error.
                // Letting it escape the static initializer instead would call std::terminate
                // in every program that includes this header, %Z user or not.
                if (entries.contains(std::string{s_unknown_zone}))
                    throw io_error(
                        "timeio: the tz database has a zone or abbreviation named '"
                        + std::string{s_unknown_zone} + "', colliding with the unknown-zone token");

                // An ICU abbreviation (Windows) follows ICU's default locale and may not be ASCII;
                // the trie matches bytes, so such a key would never match a wide stream.
                const auto is_ascii = [](const std::string& key) {
                    return std::ranges::all_of(key, [](char c) { return static_cast<unsigned char>(c) < 0x80; });
                };
                for (const auto& [key, ident] : entries)
                {
                    if (!is_ascii(key))
                        continue;
                    res.add(key.begin(), key.end(),
                            zone_ref{.text = key, .is_name = ident.is_name, .is_abbrev = ident.is_abbrev});
                    has_utc = has_utc || key == utc;
                }
            }
            catch (...) // NOLINT(bugprone-empty-catch)
            {
                // tz database unavailable, malformed, or colliding with the unknown-zone
                // token at static-init time: degrade to a trie holding nothing but that
                // token, instead of letting the exception escape this static initializer
                // (which would call std::terminate). %Z then simply fails to match and
                // reports a catchable stream_error at parse time, matching the defensive
                // behaviour of time_zone_parse_helper.
            }
#endif
            // put writes "UTC" for a sys_time's %Z, and that has to read back with or without a
            // database. Added last, so a database's own UTC entry (a name too) is not refused.
            if (!has_utc)
                res.add(utc.begin(), utc.end(),
                        zone_ref{.text = std::string{utc}, .is_name = false, .is_abbrev = true});
            return res;
        }
    };

#if !defined(IOV2_SHARED)
    // Only bad_alloc can escape the build, at startup, where nothing could handle it anyway.
    // NOLINTNEXTLINE(bugprone-throwing-static-initialization)
    inline const prefix_tree<char, zone_ref>& base_ft<timeio>::s_timezone_tree =
        *new prefix_tree<char, zone_ref>(build_timezone_tree());
#endif

    /**
     * @lang{ZH}
     * @brief `timeio` facet 的基类模板特化。
     *
     * 为某一字符类型的 `timeio` facet 提供公共基础，包括：
     * - `era_entry`：描述单个日历纪元的结构体；
     * - 静态 `id()` 方法：返回 `timeio` facet 在 facet_store 中的唯一类型标识。
     *
     * 与字符类型无关的时区数据（@ref base_ft<timeio>::s_timezone_tree 与
     * @ref base_ft<timeio>::s_unknown_zone）不在这里，而在中间基类 `base_ft<timeio>`
     * 中，因而所有字符类型共用一份；经继承仍可由本类的名字访问。
     *
     * @tparam CharT 字符类型。
     * @endif
     *
     * @lang{EN}
     * @brief Base class template specialization for the `timeio` facet.
     *
     * Provides the common foundation of the `timeio` facet for one character type,
     * including:
     * - `era_entry`: a struct describing a single calendar era;
     * - static `id()`: returns the unique type-identity token of the `timeio` facet
     *   within a facet_store.
     *
     * The time-zone data that owes nothing to the character type
     * (@ref base_ft<timeio>::s_timezone_tree and @ref base_ft<timeio>::s_unknown_zone) does
     * not live here but in the intermediate base `base_ft<timeio>`, so that every character
     * type shares one copy; it remains reachable through this class's name by inheritance.
     *
     * @tparam CharT The character type.
     * @endif
     */
    template <typename CharT>
    class ft_basic<timeio<CharT>> : public base_ft<timeio>
    {
    public:
        /**
         * @lang{ZH}
         * @brief 表示一个日历纪元（calendar era）的条目。
         *
         * 每个纪元条目对应 locale 数据中定义的一段历史时期（如公元 A.D.、某国皇纪）。
         * 起止日期（`from_*` / `to_*`）定义了该纪元的日历范围，`offset` 与 `direction`
         * 共同决定了如何将公历年份映射到纪元年份：
         * @code
         *   era_year = offset + (calendar_year - from_year) * direction
         * @endcode
         * @endif
         *
         * @lang{EN}
         * @brief An entry representing a single calendar era.
         *
         * Each era entry corresponds to a historical period defined in locale data
         * (e.g. A.D., a country-specific imperial era). The start and end dates
         * (`from_*` / `to_*`) define the calendar range of the era; `offset` together
         * with `direction` determine how a Gregorian year maps to the era year:
         * @code
         *   era_year = offset + (calendar_year - from_year) * direction
         * @endcode
         * @endif
         */
        struct era_entry
        {
            /**
             * @lang{ZH}
             * @brief 纪元名称（用于 `%EC` 格式输出）。
             * @endif
             *
             * @lang{EN}
             * @brief Era name (used for `%EC` format output).
             * @endif
             */
            std::basic_string<CharT> name;
            /**
             * @lang{ZH}
             * @brief 纪元年份格式串（用于 `%EY` 格式输出）。
             * @endif
             *
             * @lang{EN}
             * @brief Era year format string (used for `%EY` format output).
             * @endif
             */
            std::basic_string<CharT> format;
            /**
             * @lang{ZH}
             * @brief 纪元起始年份（公历）。
             * @endif
             *
             * @lang{EN}
             * @brief Era start year (Gregorian calendar).
             * @endif
             */
            int32_t from_year = 0;
            /**
             * @lang{ZH}
             * @brief 纪元起始月份（1–12）。
             * @endif
             *
             * @lang{EN}
             * @brief Era start month (1–12).
             * @endif
             */
            uint8_t from_month = 0;
            /**
             * @lang{ZH}
             * @brief 纪元起始日（1–31）。
             * @endif
             *
             * @lang{EN}
             * @brief Era start day (1–31).
             * @endif
             */
            uint8_t from_day = 0;
            /**
             * @lang{ZH}
             * @brief 纪元结束年份（公历）。开放结尾的纪元规范化为 `INT32_MAX`。
             * @endif
             *
             * @lang{EN}
             * @brief Era end year (Gregorian calendar). Open-ended eras are normalised to `INT32_MAX`.
             * @endif
             */
            int32_t to_year = 0;
            /**
             * @lang{ZH}
             * @brief 纪元结束月份（1–12）。
             * @endif
             *
             * @lang{EN}
             * @brief Era end month (1–12).
             * @endif
             */
            uint8_t to_month = 0;
            /**
             * @lang{ZH}
             * @brief 纪元结束日（1–31）。
             * @endif
             *
             * @lang{EN}
             * @brief Era end day (1–31).
             * @endif
             */
            uint8_t to_day = 0;
            /**
             * @lang{ZH}
             * @brief 纪元纪年偏移量，即纪元起始年（`from_year`）所对应的纪元年号。
             * @endif
             *
             * @lang{EN}
             * @brief Year offset for the era: the era year number assigned to `from_year`.
             * @endif
             */
            int32_t offset = 0;
            /**
             * @lang{ZH}
             * @brief 纪元年份增长方向。
             *
             * `+1` 表示年份数值随时间向未来方向增大（如公元 A.D.）；
             * `-1` 表示年份数值随时间向过去方向增大（如公元前 B.C.）。
             * @endif
             *
             * @lang{EN}
             * @brief Direction in which era year numbers increase.
             *
             * `+1` indicates that the year number increases toward the future (e.g. A.D.);
             * `-1` indicates that the year number increases toward the past (e.g. B.C.).
             * @endif
             */
            int8_t direction = 0;

            /// @cond
            bool operator==(const era_entry&) const = default; // for test.
            /// @endcond
        };
    public:
        /**
         * @lang{ZH}
         * @brief 默认构造函数，使用 `id()` 初始化基类。
         * @endif
         *
         * @lang{EN}
         * @brief Default constructor, initializes the base class with `id()`.
         * @endif
         */
        ft_basic()
            : base_ft<timeio>(id()) {}

        using char_type = CharT;

        /**
         * @lang{ZH}
         * @brief 返回 `timeio` facet 的唯一类型标识。
         *
         * 经由统一入口 `type_id_v<ft_basic>()`（见 facet_common.h 顶部说明）：
         * header-only 模式下是每类型静态量的地址，共享库模式下是 `std::type_index(typeid)`，
         * 与 `base_ft` 的类型分发机制配合使用。
         * @return `timeio` facet 的唯一类型 ID。
         * @endif
         *
         * @lang{EN}
         * @brief Returns the unique type-identity token for the `timeio` facet.
         *
         * Via the single entry point `type_id_v<ft_basic>()` (see the note at the
         * top of facet_common.h): a per-type static's address in header-only mode,
         * `std::type_index(typeid)` in shared mode; integrates with the
         * type-dispatch mechanism of `base_ft`.
         * @return The unique type ID for the `timeio` facet.
         * @endif
         */
        static facet_id_t id() { return type_id_v<ft_basic>(); }
    };

/**
 * @lang{ZH}
 * @brief 供 `timeio` 内部使用的辅助工具命名空间。
 *
 * 包含用于日历纪元日期比较的内联函数，不属于 `timeio` 的公共接口。
 * @endif
 *
 * @lang{EN}
 * @brief Internal helper namespace for `timeio`.
 *
 * Contains inline functions for calendar era date comparison; these are
 * not part of the public interface of `timeio`.
 * @endif
 */
namespace TimeioHelper
{
    /**
     * @lang{ZH}
     * @brief 比较两个完整日期（年、月、日）的前后关系。
     *
     * 用于判断纪元起始日期是否早于或等于结束日期，从而确定纪元的方向。
     * @param from_year  起始年份。
     * @param from_month 起始月份（1–12）。
     * @param from_day   起始日（1–31）。
     * @param to_year    结束年份。
     * @param to_month   结束月份（1–12）。
     * @param to_day     结束日（1–31）。
     * @return 若起始日期早于或等于结束日期，返回 `true`；否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Compares two full dates (year, month, day) for ordering.
     *
     * Used to determine whether an era's start date is no later than its end date,
     * which in turn determines the era's direction.
     * @param from_year  The starting year.
     * @param from_month The starting month (1–12).
     * @param from_day   The starting day (1–31).
     * @param to_year    The ending year.
     * @param to_month   The ending month (1–12).
     * @param to_day     The ending day (1–31).
     * @return `true` if the start date is earlier than or equal to the end date; `false` otherwise.
     * @endif
     */
    inline bool era_small_or_equal(int from_year, uint8_t from_month, uint8_t from_day, // NOLINT(bugprone-easily-swappable-parameters)
                                   int to_year, uint8_t to_month, uint8_t to_day)        // NOLINT(bugprone-easily-swappable-parameters)
    {
        if (from_year < to_year) return true;
        if (from_year > to_year) return false;
        if (from_month < to_month) return true;
        if (from_month > to_month) return false;
        return from_day <= to_day;
    }

    /**
     * @lang{ZH}
     * @brief 比较两个年月组合（不含日）的前后关系。
     *
     * 为 `era_small_or_equal` 的简化重载，仅比较年份和月份。
     * @param from_year  起始年份。
     * @param from_month 起始月份（1–12）。
     * @param to_year    结束年份。
     * @param to_month   结束月份（1–12）。
     * @return 若起始年月早于或等于结束年月，返回 `true`；否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Compares two year-month pairs (without day) for ordering.
     *
     * A simplified overload of `era_small_or_equal` that compares only year and month.
     * @param from_year  The starting year.
     * @param from_month The starting month (1–12).
     * @param to_year    The ending year.
     * @param to_month   The ending month (1–12).
     * @return `true` if the start year-month is earlier than or equal to the end year-month; `false` otherwise.
     * @endif
     */
    inline bool era_small_or_equal(int from_year, uint8_t from_month,                   // NOLINT(bugprone-easily-swappable-parameters)
                                   int to_year, uint8_t to_month)                        // NOLINT(bugprone-easily-swappable-parameters)
    {
        if (from_year < to_year) return true;
        if (from_year > to_year) return false;
        return from_month <= to_month;
    }
}

template <typename CharT> class timeio_conf;

namespace detail
{
/**
 * @lang{ZH}
 * 一个 locale 的名字与格式串，按 `timeio_conf<CharT>` 保存的样子：格式串已补上回退值并规范化。
 * 由 `timeio_c_text` 或 `timeio_text_from` 生成，窄、宽配置类共用。
 * @endif
 * @lang{EN}
 * A locale's names and formats as `timeio_conf<CharT>` keeps them, the formats completed and
 * normalized. Built by `timeio_c_text` or `timeio_text_from`, for the narrow and the wide
 * configurations alike.
 * @endif
 */
template <typename CharT>
struct timeio_text
{
    std::array<std::basic_string<CharT>, 7>  day;
    std::array<std::basic_string<CharT>, 7>  abbr_day;
    std::array<std::basic_string<CharT>, 12> month;
    std::array<std::basic_string<CharT>, 12> abbr_month;
    std::basic_string<CharT>                 am;
    std::basic_string<CharT>                 pm;
    std::basic_string<CharT>                 date_format;
    std::basic_string<CharT>                 era_date_format;
    std::basic_string<CharT>                 time_format;
    std::basic_string<CharT>                 era_time_format;
    std::basic_string<CharT>                 date_time_format;
    std::basic_string<CharT>                 era_date_time_format;
    std::basic_string<CharT>                 am_pm_format;
};

// The "C" locale's formats: what it uses, and what a locale's format falls back to.
inline constexpr std::string_view timeio_c_date_format      = "%m/%d/%y";
inline constexpr std::string_view timeio_c_time_format      = "%H:%M:%S";
inline constexpr std::string_view timeio_c_date_time_format = "%a %b %e %H:%M:%S %Y";
inline constexpr std::string_view timeio_c_am_pm_format     = "%I:%M:%S %p";

// ASCII text as a string of any character type.
template <typename CharT>
std::basic_string<CharT> timeio_ascii(std::string_view text)
{
    return std::basic_string<CharT>(text.begin(), text.end());
}

/**
 * @lang{ZH}
 * @brief 把 locale 提供的时间格式串归一化为本库 `timeio` 能识别的形式。
 *
 * C 库给出的格式串（POSIX 上来自 `nl_langinfo()`）是 locale 数据，不是用户写的；其中可能出现 glibc 的
 * 扩展语法，而本库的 `timeio` 只实现 `'%' [E|O]? <说明符>` 这一标准文法。若原样保留，
 * `timeio` 的 put 侧会把不认识的序列连同 `%` 一起字面回显（与 `strftime` 对真正未知
 * 说明符的行为一致），使用户仅写了 `%c` / `%x` 却得到含 `%` 的输出，并可能被 get 侧
 * 静默读回错值。本函数在 locale 数据进入本库时做一次转换：
 *
 * - 丢弃补位标志（`-` `_` `0` `^` `#`）与字段宽度：它们只影响填充与大小写，
 *   丢弃后字段顺序、分隔符、语言全部保留；
 * - 把有等价标准说明符的 glibc 扩展改写过去：`%l` → `%I`、`%k` → `%H`、`%P` → `%p`；
 * - 保留 `E` / `O` 修饰符；
 * - 末尾落单的 `%`（其后没有说明符）原样保留，以维持第 329 条已钉住的行为。
 *
 * 若经上述处理后说明符仍不在支持集内，则返回 `std::nullopt`，由调用方整串回退，宁可丢失
 * locale 的字段顺序，也不输出本库读不回来的文本。只有 ASCII 字符才可能是说明符。
 *
 * @note 本函数只作用于 locale 提供的格式串；用户自己传给 `put_time` / `get_time`
 *       的格式串不经过这里，其未知说明符仍按原契约回显。
 *
 * @tparam CharT 格式串的字符类型；窄、宽配置类共用。
 * @param fmt locale 提供的格式串。
 * @return 归一化后的格式串；若含无等价物的说明符，则为 `std::nullopt`，
 *         由调用方代入整串兜底值。
 * @endif
 *
 * @lang{EN}
 * @brief Normalizes a locale-supplied time format string into the form this
 *        library's `timeio` understands.
 *
 * A format string from the C library (`nl_langinfo()` on POSIX) is locale data, not something the user wrote,
 * and it may use glibc's extended syntax, whereas `timeio` implements only the
 * standard grammar `'%' [E|O]? <specifier>`. Left as-is, `timeio`'s put side echoes
 * an unrecognized sequence literally together with its `%` (matching `strftime` for
 * genuinely unknown specifiers), so a user who wrote just `%c` or `%x` gets output
 * containing a `%`, which the get side may then read back as silently wrong values.
 * This function converts once, where locale data enters the library:
 *
 * - Padding flags (`-` `_` `0` `^` `#`) and field widths are dropped: they affect
 *   only padding and case, so field order, separators and language all survive;
 * - glibc extensions that have an equivalent standard specifier are rewritten:
 *   `%l` to `%I`, `%k` to `%H`, `%P` to `%p`;
 * - `E` / `O` modifiers are preserved;
 * - A trailing lone `%` with no specifier after it is kept verbatim, preserving the
 *   behaviour pinned by entry 329.
 *
 * If the specifier is still outside the supported set after that, the result is
 * `std::nullopt` and the caller falls back for the whole string: losing the locale's field
 * order is preferable to emitting text this library cannot read back. Only an ASCII
 * character can be a specifier.
 *
 * @note This applies only to locale-supplied format strings. A format string the user
 *       passes to `put_time` / `get_time` does not go through here, and its unknown
 *       specifiers are still echoed per the existing contract.
 *
 * @tparam CharT The character type of the format, shared by the narrow and the wide
 *         configurations.
 * @param fmt The locale-supplied format string.
 * @return The normalized format string, or `std::nullopt` if it holds a specifier with
 *         no equivalent, in which case the caller substitutes a whole-string fallback.
 * @endif
 */
template <typename CharT>
std::optional<std::basic_string<CharT>> timeio_normalize_format(std::basic_string_view<CharT> fmt)
{
    constexpr std::string_view supported = "%ABCDFGHIMRSTUVWXYZabcdeghjmnprtuwxyz";
    // A wide character beyond ASCII must not be narrowed onto a specifier first.
    const auto is_supported = [supported](CharT c) {
        return static_cast<std::uint32_t>(c) < 0x80
            && supported.find(static_cast<char>(c)) != std::string_view::npos;
    };

    std::basic_string<CharT> out;
    out.reserve(fmt.size());

    for (std::size_t i = 0; i < fmt.size(); ++i)
    {
        if (fmt[i] != '%') { out += fmt[i]; continue; }

        std::size_t j = i + 1;
        while (j < fmt.size() && (fmt[j] == '-' || fmt[j] == '_' || fmt[j] == '0'
                                  || fmt[j] == '^' || fmt[j] == '#'))
            ++j;
        while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') ++j;

        CharT modifier{};
        if (j < fmt.size() && (fmt[j] == 'E' || fmt[j] == 'O')) modifier = fmt[j++];

        if (j >= fmt.size()) { out.append(fmt.substr(i)); break; }

        CharT spec = fmt[j];
        if (spec == 'l') spec = 'I';
        else if (spec == 'k') spec = 'H';
        else if (spec == 'P') spec = 'p';

        if (!is_supported(spec)) return std::nullopt;

        out += static_cast<CharT>('%');
        if (modifier != CharT{}) out += modifier;
        out += spec;
        i = j;
    }

    return out;
}

// The names and formats of the "C" locale.
template <typename CharT>
timeio_text<CharT> timeio_c_text()
{
    static constexpr std::array<std::string_view, 7> days =
        {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static constexpr std::array<std::string_view, 12> months =
        {"January", "February", "March", "April", "May", "June",
         "July", "August", "September", "October", "November", "December"};

    timeio_text<CharT> res;
    for (std::size_t i = 0; i < days.size(); ++i)
    {
        res.day[i] = timeio_ascii<CharT>(days[i]);
        res.abbr_day[i] = timeio_ascii<CharT>(days[i].substr(0, 3));
    }
    for (std::size_t i = 0; i < months.size(); ++i)
    {
        res.month[i] = timeio_ascii<CharT>(months[i]);
        res.abbr_month[i] = timeio_ascii<CharT>(months[i].substr(0, 3));
    }
    res.am = timeio_ascii<CharT>("AM");
    res.pm = timeio_ascii<CharT>("PM");
    res.date_format = timeio_ascii<CharT>(timeio_c_date_format);
    res.era_date_format = res.date_format;
    res.time_format = timeio_ascii<CharT>(timeio_c_time_format);
    res.era_time_format = res.time_format;
    res.date_time_format = timeio_ascii<CharT>(timeio_c_date_time_format);
    res.era_date_time_format = res.date_time_format;
    res.am_pm_format = timeio_ascii<CharT>(timeio_c_am_pm_format);
    return res;
}

/**
 * @lang{ZH}
 * @brief 由后端给出的字符串生成一个 locale 的名字与格式串，并补上回退值、规范化格式串。
 *
 * 纪元格式为空表示该 locale 没有纪元，由对应的普通格式顶替；`%r` 的格式为空时用
 * `"%I:%M:%S %p"`；规范化失败的格式串，纪元格式退回规范化后的普通格式，其余退回 C locale 的。
 * @tparam CharT 生成的字符类型。
 * @param raw 窄字符的 `timeio_raw_data`，或宽字符的 `timeio_raw_wide`；两者成员同名。
 * @param conv 把 `raw` 的一个字符串转成 `std::basic_string<CharT>`。
 * @endif
 * @lang{EN}
 * @brief Builds a locale's names and formats from the strings the backend gave, completing and
 *        normalizing the formats.
 *
 * An empty era format means the locale has no eras, and the plain format stands in; an empty
 * `%r` format becomes `"%I:%M:%S %p"`; a format that fails to normalize falls back to the
 * normalized plain one if it is an era format, and to the "C" locale's otherwise.
 * @tparam CharT The character type to build.
 * @param raw The narrow `timeio_raw_data` or the wide `timeio_raw_wide`, whose members share names.
 * @param conv Turns one of `raw`'s strings into a `std::basic_string<CharT>`.
 * @endif
 */
template <typename CharT, typename TRaw, typename TConv>
timeio_text<CharT> timeio_text_from(const TRaw& raw, TConv conv)
{
    timeio_text<CharT> res;
    for (std::size_t i = 0; i < res.day.size(); ++i)
    {
        res.day[i] = conv(raw.day[i]);
        res.abbr_day[i] = conv(raw.abbr_day[i]);
    }
    for (std::size_t i = 0; i < res.month.size(); ++i)
    {
        res.month[i] = conv(raw.month[i]);
        res.abbr_month[i] = conv(raw.abbr_month[i]);
    }
    res.am = conv(raw.am);
    res.pm = conv(raw.pm);

    const auto era_or = [&conv](const auto& era, const std::basic_string<CharT>& plain) {
        return era.empty() ? plain : conv(era);
    };
    res.date_format = conv(raw.date_format);
    res.era_date_format = era_or(raw.era_date_format, res.date_format);
    res.time_format = conv(raw.time_format);
    res.era_time_format = era_or(raw.era_time_format, res.time_format);
    res.date_time_format = conv(raw.date_time_format);
    res.era_date_time_format = era_or(raw.era_date_time_format, res.date_time_format);
    res.am_pm_format = raw.am_pm_format.empty() ? timeio_ascii<CharT>(timeio_c_am_pm_format)
                                                : conv(raw.am_pm_format);

    const auto normalized = [](const std::basic_string<CharT>& fmt, std::basic_string<CharT> fallback) {
        return timeio_normalize_format<CharT>(fmt).value_or(std::move(fallback));
    };
    res.date_format = normalized(res.date_format, timeio_ascii<CharT>(timeio_c_date_format));
    res.era_date_format = normalized(res.era_date_format, res.date_format);
    res.time_format = normalized(res.time_format, timeio_ascii<CharT>(timeio_c_time_format));
    res.era_time_format = normalized(res.era_time_format, res.time_format);
    res.date_time_format = normalized(res.date_time_format,
                                      timeio_ascii<CharT>(timeio_c_date_time_format));
    res.era_date_time_format = normalized(res.era_date_time_format, res.date_time_format);
    res.am_pm_format = normalized(res.am_pm_format, timeio_ascii<CharT>(timeio_c_am_pm_format));
    return res;
}

/**
 * @lang{ZH}
 * @brief 把后端给出的一个纪元变成 `era_entry`：名称与格式串经 `conv` 转换，日期照搬，方向规范化。
 *
 * `direction` 被规范化为恰好 `+1` / `-1`，使得以下线性公式对所有纪元类型均一致有效：
 * @code
 *   era_year = offset + (calendar_year - from_year) * direction
 * @endcode
 * 规范化逻辑参照 glibc 的 era.c（约第 98 行，
 * https://github.com/lattera/glibc/blob/master/time/era.c）：
 * 前向纪元（from ≤ to，如公元 AD、日本皇纪、泰国佛历）方向标记直接映射，
 * '+' → +1，'-' → -1；后向纪元（from > to）方向标记取反：from_year 是纪元起点，to_year
 * 是"负无穷"的哨兵，公历年份离起点越往过去，(calendar - from_year) 越负，取反后的方向把这段
 * 距离变成正的纪元年份，同一个公式于是对两个时间方向都成立。
 *
 * 后向纪元路径**会被真实 locale 执行**，并非理论分支：`ja_JP` 的「紀元前」
 * 以及 `zh_TW` / `cmn_TW` / `hak_TW` / `lzh_TW` / `nan_TW` 的「民前」都是
 * 后向纪元。以本机 glibc 数据实测，24 个带纪元数据的 locale 共 108 条记录，
 * 其中 18 条为后向纪元。
 * @tparam TEntry 目标配置类的 `era_entry`。
 * @param raw 后端读出的纪元，方向为原样的 `'+'` / `'-'`。
 * @param conv 把窄字符串转成 `TEntry` 的字符串类型。
 * @return 规范化后的纪元条目。
 * @endif
 *
 * @lang{EN}
 * @brief Turns one era the backend gave into an `era_entry`: the name and the format through
 *        `conv`, the dates as they are, the direction normalized.
 *
 * `direction` is normalised to exactly `+1` / `-1` so that the linear formula
 * @code
 *   era_year = offset + (calendar_year - from_year) * direction
 * @endcode
 * is uniformly valid for all era types.
 * The normalisation mirrors glibc's era.c (≈ line 98,
 * https://github.com/lattera/glibc/blob/master/time/era.c):
 * for forward eras (from ≤ to, e.g. AD / Japanese / Thai), the marker maps
 * directly: '+' → +1, '-' → -1;
 * for backward eras (from > to), the marker is intentionally inverted.
 * The reason: in a backward era, from_year is the epoch and to_year is a
 * sentinel for "negative infinity". As the calendar year moves away from the
 * epoch toward the past, (calendar - from_year) grows increasingly negative;
 * the normalised negative direction turns that distance into a positive era
 * year, so the same formula works in either chronological direction.
 *
 * The backward branch **is reached by real locales**, not just in theory:
 * `ja_JP`'s 紀元前 and the 民前 of `zh_TW` / `cmn_TW` / `hak_TW` / `lzh_TW` /
 * `nan_TW` are all backward eras. Measured against this machine's glibc data,
 * 18 of the 108 records across the 24 locales that carry era data are
 * backward.
 * @tparam TEntry The target configuration's `era_entry`.
 * @param raw An era as the backend read it, its direction the `'+'` / `'-'` as written.
 * @param conv Turns a narrow string into `TEntry`'s string type.
 * @return The era entry, normalized.
 * @endif
 */
template <typename TEntry, typename TConv>
TEntry timeio_era_from_raw(const timeio_raw_era& raw, TConv conv)
{
    TEntry value{};
    value.name = conv(raw.name);
    value.format = conv(raw.format);
    value.from_year = raw.from_year;
    value.from_month = raw.from_month;
    value.from_day = raw.from_day;
    value.to_year = raw.to_year;
    value.to_month = raw.to_month;
    value.to_day = raw.to_day;
    value.offset = raw.offset;

    const int8_t marked_direction = raw.direction == '+' ? int8_t{1} : int8_t{-1};
    const bool runs_forward = TimeioHelper::era_small_or_equal(
        value.from_year, value.from_month, value.from_day,
        value.to_year, value.to_month, value.to_day);
    value.direction = runs_forward ? marked_direction : static_cast<int8_t>(-marked_direction);
    return value;
}
}

/**
 * @lang{ZH}
 * @brief `timeio` facet 的 `char` 字符类型 locale 配置类。
 *
 * 这是 `timeio_conf` 的主特化，负责从系统 locale 或 C/POSIX 硬编码默认值中加载
 * `timeio` facet 所需的全部 locale 数据，包括日期/时间格式串、星期与月份全称及
 * 缩写、AM/PM 字符串及格式、替代数字，以及纪元（era）条目。
 *
 * 当 locale 名称为 `"C"` 或 `"POSIX"` 时，使用硬编码的英语默认值；
 * 其他 locale 名称经平台后端（`detail::timeio_read_data`）在切换到该 locale 的线程上读出
 * C 库的原样数据，再由本类补上空缺项的回退值、规范化格式串与纪元方向。
 * @endif
 *
 * @lang{EN}
 * @brief Locale configuration class for the `timeio` facet specialised for `char`.
 *
 * This is the primary specialization of `timeio_conf`, responsible for loading all
 * locale data required by the `timeio` facet from the system locale or from C/POSIX
 * hard-coded defaults. The loaded data includes: date/time format strings, full and
 * abbreviated weekday and month names, AM/PM strings and format, alternative digits,
 * and era entries.
 *
 * When the locale name is `"C"` or `"POSIX"`, hard-coded English defaults are used.
 * For other locales the platform backend (`detail::timeio_read_data`) reads the C library's
 * data as it is, on a thread switched to that locale, and this class fills in the fallbacks
 * for missing items and normalizes the formats and the era directions.
 * @endif
 */
template <>
class timeio_conf<char> : public ft_basic<timeio<char>>
{
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，根据 locale 名称加载日期时间 locale 数据。
     *
     * 若 `name` 为 `"C"` 或 `"POSIX"`，则使用硬编码的英语默认值（ISO C 标准格式）；
     * 否则经平台后端从系统 locale 数据库中加载相应数据。
     * @param name locale 名称（如 `"C"`、`"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads date-time locale data for the given locale name.
     *
     * If `name` is `"C"` or `"POSIX"`, hard-coded English defaults (ISO C standard
     * formats) are used. Otherwise the corresponding data is loaded from the system locale
     * database through the platform backend.
     * @param name The locale name (e.g. `"C"`, `"zh_CN.UTF-8"`).
     * @endif
     */
    timeio_conf(const std::string& name)
        : ft_basic<timeio<char>>()
    {
        if (FacetHelper::is_c_locale_name(name))
        {
            take_text(detail::timeio_c_text<char>());
            return;
        }

        // Copied out with the thread switched to the locale; the backend may need loc itself.
        const detail::timeio_raw_data raw = [&name] {
            clocale_wrapper loc(name);
            clocale_user    guard(loc);
            return detail::timeio_read_data(loc.c_locale);
        }();
        const auto same = [](const std::string& src) { return src; };
        take_text(detail::timeio_text_from<char>(raw, same));
        for (std::size_t i = 0; i < raw.alt_digits.size() && i < m_alt_digits.size(); ++i)
            m_alt_digits[i] = raw.alt_digits[i];
        m_era_items.reserve(raw.eras.size());
        for (const detail::timeio_raw_era& era : raw.eras)
            m_era_items.push_back(detail::timeio_era_from_raw<era_entry>(era, same));
    }

    /**
     * @lang{ZH}
     * @brief 返回星期全称数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 full weekday name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::string, 7>& day_names() const { return m_day; }
    /**
     * @lang{ZH}
     * @brief 返回星期缩写数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 abbreviated weekday name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::string, 7>& abbr_day_names() const { return m_abbr_day; }
    /**
     * @lang{ZH}
     * @brief 返回月份全称数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 full month name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::string, 12>& month_names() const { return m_month; }
    /**
     * @lang{ZH}
     * @brief 返回月份缩写数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 abbreviated month name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::string, 12>& abbr_month_names() const { return m_abbr_month; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的替代数字字符串数组（最多 100 项）。
     *
     * 替代数字用于 `%Od`、`%Oe` 等替代格式说明符的输出与解析。
     * 未被 locale 定义的条目为空字符串。
     * @return 包含 100 个替代数字字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale-defined alternative digit strings (up to 100 entries).
     *
     * Alternative digits are used for output and parsing of alternative format specifiers
     * such as `%Od` and `%Oe`. Entries not defined by the locale are empty strings.
     * @return A constant reference to the array of 100 alternative digit strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::string, 100>& alt_digit_names() const { return m_alt_digits; }
    /**
     * @lang{ZH}
     * @brief 返回 AM 时段字符串（如 `"AM"`）。
     * @return AM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM period string (e.g. `"AM"`).
     * @return A constant reference to the AM string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& am_name() const { return m_am; }
    /**
     * @lang{ZH}
     * @brief 返回 PM 时段字符串（如 `"PM"`）。
     * @return PM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the PM period string (e.g. `"PM"`).
     * @return A constant reference to the PM string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& pm_name() const { return m_pm; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期格式串（对应 `%x`）。
     * @return 日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date format string (corresponding to `%x`).
     * @return A constant reference to the date format string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& date_format() const { return m_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期格式串（对应 `%Ex`）。
     *
     * 若 locale 未定义纪元修饰格式，则回退为普通日期格式串。
     * @return 纪元修饰日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date format string (corresponding to `%Ex`).
     *
     * Falls back to the plain date format string if the locale does not define an
     * era-modified variant.
     * @return A constant reference to the era-modified date format string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& era_date_format() const { return m_era_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 时间格式串（对应 `%X`）。
     * @return 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale time format string (corresponding to `%X`).
     * @return A constant reference to the time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& time_format() const { return m_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰时间格式串（对应 `%EX`）。
     *
     * 若 locale 未定义纪元修饰格式，则回退为普通时间格式串。
     * @return 纪元修饰时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified time format string (corresponding to `%EX`).
     *
     * Falls back to the plain time format string if the locale does not define an
     * era-modified variant.
     * @return A constant reference to the era-modified time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& era_time_format() const { return m_era_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期时间格式串（对应 `%c`）。
     * @return 日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date-time format string (corresponding to `%c`).
     * @return A constant reference to the date-time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& date_time_format() const { return m_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期时间格式串（对应 `%Ec`）。
     *
     * 若 locale 未定义纪元修饰格式，则回退为普通日期时间格式串。
     * @return 纪元修饰日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date-time format string (corresponding to `%Ec`).
     *
     * Falls back to the plain date-time format string if the locale does not define an
     * era-modified variant.
     * @return A constant reference to the era-modified date-time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& era_date_time_format() const { return m_era_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 AM/PM 时间格式串（对应 `%r`）。
     * @return AM/PM 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM/PM time format string (corresponding to `%r`).
     * @return A constant reference to the AM/PM time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& am_pm_format() const { return m_am_pm_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的纪元条目列表。
     * @return 纪元条目列表的常量引用；若 locale 无纪元定义则为空 `vector`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the list of locale-defined era entries.
     * @return A constant reference to the era entry list; an empty `vector` if the locale
     *         defines no eras.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<era_entry>& era_items() const { return m_era_items; }

private:
    // Takes the names and formats over from text.
    void take_text(detail::timeio_text<char> text)
    {
        m_day = std::move(text.day);
        m_abbr_day = std::move(text.abbr_day);
        m_month = std::move(text.month);
        m_abbr_month = std::move(text.abbr_month);
        m_am = std::move(text.am);
        m_pm = std::move(text.pm);
        m_date_format = std::move(text.date_format);
        m_era_date_format = std::move(text.era_date_format);
        m_time_format = std::move(text.time_format);
        m_era_time_format = std::move(text.era_time_format);
        m_date_time_format = std::move(text.date_time_format);
        m_era_date_time_format = std::move(text.era_date_time_format);
        m_am_pm_format = std::move(text.am_pm_format);
    }

    std::array<std::string, 7>   m_day;
    std::array<std::string, 7>   m_abbr_day;
    std::array<std::string, 12>  m_month;
    std::array<std::string, 12>  m_abbr_month;
    std::array<std::string, 100> m_alt_digits;
    std::string                  m_am;
    std::string                  m_pm;
    std::string                  m_date_format;
    std::string                  m_era_date_format;
    std::string                  m_time_format;
    std::string                  m_era_time_format;
    std::string                  m_date_time_format;
    std::string                  m_era_date_time_format;
    std::string                  m_am_pm_format;
    std::vector<era_entry>       m_era_items;
};

/**
 * @lang{ZH}
 * @brief `timeio` facet 的宽字符（`wchar_t` / `char32_t` UTF-32）locale 配置类。
 *
 * 此特化适用于 `wchar_t`，以及在 `wchar_t` 为 UTF-32 的平台上的 `char32_t`。
 * 经平台后端读出 locale 数据：C 库给出了宽字符形式（Windows 的 NLS）时，名字与格式串直接取用；
 * 否则把窄字符串转换为对应的宽字符或 UTF-32 字符串。回退值与格式串规范化和 `timeio_conf<char>`
 * 共用同一份实现。
 *
 * @tparam CharT 字符类型，限定为 `wchar_t` 或（在 `wchar_t` 为 UTF-32 的平台上）`char32_t`。
 * @endif
 *
 * @lang{EN}
 * @brief Locale configuration class for the `timeio` facet specialised for wide characters
 *        (`wchar_t` / `char32_t` UTF-32).
 *
 * This specialization applies to `wchar_t`, and to `char32_t` on platforms where
 * `wchar_t` is UTF-32. It reads the locale data through the platform backend: where the C
 * library gives the names and formats in wide form (Windows' NLS) they are taken as they are;
 * otherwise the narrow strings are converted to wide or UTF-32 ones. The fallbacks and the
 * normalization of the formats share one implementation with `timeio_conf<char>`.
 *
 * @tparam CharT The character type, constrained to `wchar_t` or (on UTF-32 platforms) `char32_t`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
            (std::is_same_v<CharT, char32_t> &&
            wchar_t_is_utf32)
class timeio_conf<CharT> : public ft_basic<timeio<CharT>>
{
    using era_entry = typename ft_basic<timeio<CharT>>::era_entry;

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，经平台后端加载 locale 数据；有宽字符形式就直接用，否则把窄字符串转成宽字符。
     * @param name locale 名称（如 `"C"`、`"ja_JP.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads locale data through the platform backend, taking its wide form
     *        where there is one and converting the narrow strings otherwise.
     * @param name The locale name (e.g. `"C"`, `"ja_JP.UTF-8"`).
     * @endif
     */
    timeio_conf(const std::string& name)
        : ft_basic<timeio<CharT>>()
    {
        if (FacetHelper::is_c_locale_name(name))
        {
            take_text(detail::timeio_c_text<CharT>());
            return;
        }

        const auto convert = [&name](const std::string& src) -> std::basic_string<CharT>
        {
            if constexpr (std::is_same_v<CharT, wchar_t>)
                return detail::to_wstring(src.c_str(), name);
            else
                // <CharT> makes the call dependent: clang resolves a non-dependent call even in a
                // discarded branch, and to_u32string does not exist where wchar_t is not UTF-32.
                return detail::to_u32string<CharT>(src.c_str(), name);
        };

        // Copied out with the thread switched to the locale; the backend may need loc itself.
        const detail::timeio_raw_data raw = [&name] {
            clocale_wrapper loc(name);
            clocale_user    guard(loc);
            return detail::timeio_read_data(loc.c_locale);
        }();
        if (raw.wide)
        {
            const auto same = [](const std::wstring& src) {
                return std::basic_string<CharT>(src.begin(), src.end());
            };
            take_text(detail::timeio_text_from<CharT>(*raw.wide, same));
        }
        else
            take_text(detail::timeio_text_from<CharT>(raw, convert));

        for (std::size_t i = 0; i < raw.alt_digits.size() && i < m_alt_digits.size(); ++i)
            m_alt_digits[i] = convert(raw.alt_digits[i]);
        m_era_items.reserve(raw.eras.size());
        for (const detail::timeio_raw_era& era : raw.eras)
            m_era_items.push_back(detail::timeio_era_from_raw<era_entry>(era, convert));
    }

    /**
     * @lang{ZH}
     * @brief 返回星期全称数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 full weekday name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<CharT>, 7>& day_names() const { return m_day; }
    /**
     * @lang{ZH}
     * @brief 返回星期缩写数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 abbreviated weekday name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<CharT>, 7>& abbr_day_names() const { return m_abbr_day; }
    /**
     * @lang{ZH}
     * @brief 返回月份全称数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 full month name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<CharT>, 12>& month_names() const { return m_month; }
    /**
     * @lang{ZH}
     * @brief 返回月份缩写数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 abbreviated month name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<CharT>, 12>& abbr_month_names() const { return m_abbr_month; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的替代数字字符串数组（最多 100 项）。
     * @return 包含 100 个替代数字字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale-defined alternative digit strings (up to 100 entries).
     * @return A constant reference to the array of 100 alternative digit strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<CharT>, 100>& alt_digit_names() const { return m_alt_digits; }
    /**
     * @lang{ZH}
     * @brief 返回 AM 时段字符串。
     * @return AM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM period string.
     * @return A constant reference to the AM string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& am_name() const { return m_am; }
    /**
     * @lang{ZH}
     * @brief 返回 PM 时段字符串。
     * @return PM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the PM period string.
     * @return A constant reference to the PM string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& pm_name() const { return m_pm; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期格式串（对应 `%x`）。
     * @return 日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date format string (corresponding to `%x`).
     * @return A constant reference to the date format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& date_format() const { return m_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期格式串（对应 `%Ex`）。
     * @return 纪元修饰日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date format string (corresponding to `%Ex`).
     * @return A constant reference to the era-modified date format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& era_date_format() const { return m_era_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 时间格式串（对应 `%X`）。
     * @return 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale time format string (corresponding to `%X`).
     * @return A constant reference to the time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& time_format() const { return m_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰时间格式串（对应 `%EX`）。
     * @return 纪元修饰时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified time format string (corresponding to `%EX`).
     * @return A constant reference to the era-modified time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& era_time_format() const { return m_era_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期时间格式串（对应 `%c`）。
     * @return 日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date-time format string (corresponding to `%c`).
     * @return A constant reference to the date-time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& date_time_format() const { return m_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期时间格式串（对应 `%Ec`）。
     * @return 纪元修饰日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date-time format string (corresponding to `%Ec`).
     * @return A constant reference to the era-modified date-time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& era_date_time_format() const { return m_era_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 AM/PM 时间格式串（对应 `%r`）。
     * @return AM/PM 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM/PM time format string (corresponding to `%r`).
     * @return A constant reference to the AM/PM time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& am_pm_format() const { return m_am_pm_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的纪元条目列表。
     * @return 纪元条目列表的常量引用；若 locale 无纪元定义则为空 `vector`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the list of locale-defined era entries.
     * @return A constant reference to the era entry list; an empty `vector` if the locale
     *         defines no eras.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<era_entry>& era_items() const { return m_era_items; }

private:
    // Takes the names and formats over from text.
    void take_text(detail::timeio_text<CharT> text)
    {
        m_day = std::move(text.day);
        m_abbr_day = std::move(text.abbr_day);
        m_month = std::move(text.month);
        m_abbr_month = std::move(text.abbr_month);
        m_am = std::move(text.am);
        m_pm = std::move(text.pm);
        m_date_format = std::move(text.date_format);
        m_era_date_format = std::move(text.era_date_format);
        m_time_format = std::move(text.time_format);
        m_era_time_format = std::move(text.era_time_format);
        m_date_time_format = std::move(text.date_time_format);
        m_era_date_time_format = std::move(text.era_date_time_format);
        m_am_pm_format = std::move(text.am_pm_format);
    }

    std::array<std::basic_string<CharT>, 7>   m_day;
    std::array<std::basic_string<CharT>, 7>   m_abbr_day;
    std::array<std::basic_string<CharT>, 12>  m_month;
    std::array<std::basic_string<CharT>, 12>  m_abbr_month;
    std::array<std::basic_string<CharT>, 100> m_alt_digits;
    std::basic_string<CharT>                  m_am;
    std::basic_string<CharT>                  m_pm;
    std::basic_string<CharT>                  m_date_format;
    std::basic_string<CharT>                  m_era_date_format;
    std::basic_string<CharT>                  m_time_format;
    std::basic_string<CharT>                  m_era_time_format;
    std::basic_string<CharT>                  m_date_time_format;
    std::basic_string<CharT>                  m_era_date_time_format;
    std::basic_string<CharT>                  m_am_pm_format;
    std::vector<era_entry>                    m_era_items;
};

/**
 * @lang{ZH}
 * @brief `timeio` facet 的 `char8_t`（UTF-8）locale 配置类。
 *
 * 数据来源与 `numeric_conf<char8_t>` 相同：`"C"` 与 UTF-8 locale 直接使用窄字符配置的字节；
 * 其他 locale 在 `wchar_t` 为 Unicode 的平台（Linux、Windows）上经 `timeio_conf<wchar_t>`
 * 转成 UTF-8；macOS 的 `wchar_t` 在非 UTF-8 locale 下不是 Unicode 码位，在那里以非 UTF-8
 * locale 构造会抛 `io_error`。
 * @endif
 *
 * @lang{EN}
 * @brief Locale configuration class for the `timeio` facet specialised for `char8_t` (UTF-8).
 *
 * The data comes as it does for `numeric_conf<char8_t>`: for `"C"` and UTF-8 locales, the
 * narrow configuration's bytes as they are; for other locales, through
 * `timeio_conf<wchar_t>` converted to UTF-8 where `wchar_t` is Unicode (Linux, Windows).
 * macOS's `wchar_t` holds no Unicode code points under a non-UTF-8 locale, and constructing
 * from such a locale throws `io_error` there.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, char8_t>
class timeio_conf<CharT> : public ft_basic<timeio<char8_t>>
{
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，加载 locale 数据并转换为 UTF-8 字符串（来源见类说明）。
     * @param name locale 名称（如 `"C"`、`"zh_CN.UTF-8"`）。
     * @throw io_error 若 `name` 不是 UTF-8 locale，而本平台的 `wchar_t` 不是 Unicode（macOS）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads locale data and converts all strings to UTF-8 (see the
     *        class documentation for where it comes from).
     * @param name The locale name (e.g. `"C"`, `"zh_CN.UTF-8"`).
     * @throw io_error If `name` is not a UTF-8 locale and this platform's `wchar_t` is not
     *        Unicode (macOS).
     * @endif
     */
    timeio_conf(const std::string& name)
        : ft_basic<timeio<char8_t>>()
    {
        // The narrow configuration's bytes are UTF-8 already, or ASCII for "C".
        if (FacetHelper::is_c_locale_name(name) || clocale_wrapper(name).is_utf8())
        {
            const timeio_conf<char> narrow(name);
            copy_from(narrow, [](const std::string& s) { return std::u8string(s.begin(), s.end()); });
        }
        else
            init_from_wide<>(name);
    }

private:
    // A non-UTF-8 locale: through timeio_conf<wchar_t> where wchar_t is Unicode. W only makes
    // the body dependent, so where wchar_t is not Unicode the discarded branch, and the
    // to_u8string it names, is never instantiated.
    template <typename W = wchar_t>
    void init_from_wide(const std::string& name)
    {
        if constexpr (wchar_t_is_utf32 || wchar_t_is_utf16)
        {
            const timeio_conf<W> wide(name);
            copy_from(wide, [](const std::basic_string<W>& s) {
                return detail::to_u8string(std::basic_string_view<W>(s));
            });
        }
        else
            throw io_error("timeio_conf<char8_t>: " + name +
                           " is not a UTF-8 locale, and wchar_t is not Unicode on this platform");
    }

    // Every string of another configuration through conv; the era dates as they are.
    template <typename TConf, typename TConv>
    void copy_from(const TConf& src, TConv conv)
    {
        m_date_format = conv(src.date_format());
        m_era_date_format = conv(src.era_date_format());
        m_time_format = conv(src.time_format());
        m_era_time_format = conv(src.era_time_format());
        m_date_time_format = conv(src.date_time_format());
        m_era_date_time_format = conv(src.era_date_time_format());

        m_am = conv(src.am_name());
        m_pm = conv(src.pm_name());
        m_am_pm_format = conv(src.am_pm_format());

        for (std::size_t i = 0; i < 7; ++i)
        {
            m_day[i] = conv(src.day_names()[i]);
            m_abbr_day[i] = conv(src.abbr_day_names()[i]);
        }

        for (std::size_t i = 0; i < 12; ++i)
        {
            m_month[i] = conv(src.month_names()[i]);
            m_abbr_month[i] = conv(src.abbr_month_names()[i]);
        }

        for (std::size_t i = 0; i < 100; ++i)
            m_alt_digits[i] = conv(src.alt_digit_names()[i]);

        m_era_items.reserve(src.era_items().size());
        for (const auto& era : src.era_items())
        {
            era_entry aim;
            aim.name = conv(era.name);
            aim.format = conv(era.format);
            aim.from_year = era.from_year;
            aim.from_month = era.from_month;
            aim.from_day = era.from_day;
            aim.to_year = era.to_year;
            aim.to_month = era.to_month;
            aim.to_day = era.to_day;
            aim.offset = era.offset;
            aim.direction = era.direction;
            m_era_items.push_back(std::move(aim));
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回星期全称数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 full weekday name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<char8_t>, 7>& day_names() const { return m_day; }
    /**
     * @lang{ZH}
     * @brief 返回星期缩写数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 abbreviated weekday name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<char8_t>, 7>& abbr_day_names() const { return m_abbr_day; }
    /**
     * @lang{ZH}
     * @brief 返回月份全称数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 full month name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<char8_t>, 12>& month_names() const { return m_month; }
    /**
     * @lang{ZH}
     * @brief 返回月份缩写数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 abbreviated month name strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<char8_t>, 12>& abbr_month_names() const { return m_abbr_month; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的替代数字字符串数组（最多 100 项）。
     * @return 包含 100 个替代数字字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale-defined alternative digit strings (up to 100 entries).
     * @return A constant reference to the array of 100 alternative digit strings.
     * @endif
     */
    [[nodiscard]] virtual const std::array<std::basic_string<char8_t>, 100>& alt_digit_names() const { return m_alt_digits; }
    /**
     * @lang{ZH}
     * @brief 返回 AM 时段字符串。
     * @return AM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM period string.
     * @return A constant reference to the AM string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& am_name() const { return m_am; }
    /**
     * @lang{ZH}
     * @brief 返回 PM 时段字符串。
     * @return PM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the PM period string.
     * @return A constant reference to the PM string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& pm_name() const { return m_pm; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期格式串（对应 `%x`）。
     * @return 日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date format string (corresponding to `%x`).
     * @return A constant reference to the date format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& date_format() const { return m_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期格式串（对应 `%Ex`）。
     * @return 纪元修饰日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date format string (corresponding to `%Ex`).
     * @return A constant reference to the era-modified date format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& era_date_format() const { return m_era_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 时间格式串（对应 `%X`）。
     * @return 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale time format string (corresponding to `%X`).
     * @return A constant reference to the time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& time_format() const { return m_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰时间格式串（对应 `%EX`）。
     * @return 纪元修饰时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified time format string (corresponding to `%EX`).
     * @return A constant reference to the era-modified time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& era_time_format() const { return m_era_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期时间格式串（对应 `%c`）。
     * @return 日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date-time format string (corresponding to `%c`).
     * @return A constant reference to the date-time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& date_time_format() const { return m_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期时间格式串（对应 `%Ec`）。
     * @return 纪元修饰日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date-time format string (corresponding to `%Ec`).
     * @return A constant reference to the era-modified date-time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& era_date_time_format() const { return m_era_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 AM/PM 时间格式串（对应 `%r`）。
     * @return AM/PM 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM/PM time format string (corresponding to `%r`).
     * @return A constant reference to the AM/PM time format string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& am_pm_format() const { return m_am_pm_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的纪元条目列表。
     * @return 纪元条目列表的常量引用；若 locale 无纪元定义则为空 `vector`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the list of locale-defined era entries.
     * @return A constant reference to the era entry list; an empty `vector` if the locale
     *         defines no eras.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<era_entry>& era_items() const { return m_era_items; }

private:
    std::array<std::basic_string<char8_t>, 7>   m_day;
    std::array<std::basic_string<char8_t>, 7>   m_abbr_day;
    std::array<std::basic_string<char8_t>, 12>  m_month;
    std::array<std::basic_string<char8_t>, 12>  m_abbr_month;
    std::array<std::basic_string<char8_t>, 100> m_alt_digits;
    std::basic_string<char8_t>                  m_am;
    std::basic_string<char8_t>                  m_pm;
    std::basic_string<char8_t>                  m_date_format;
    std::basic_string<char8_t>                  m_era_date_format;
    std::basic_string<char8_t>                  m_time_format;
    std::basic_string<char8_t>                  m_era_time_format;
    std::basic_string<char8_t>                  m_date_time_format;
    std::basic_string<char8_t>                  m_era_date_time_format;
    std::basic_string<char8_t>                  m_am_pm_format;
    std::vector<era_entry>                      m_era_items;
};
}

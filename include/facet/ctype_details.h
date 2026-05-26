/**
 * @file ctype_details.h
 * @lang{ZH}
 * `ctype` facet 的内部实现细节。
 *
 * 本文件提供以下核心组件：
 * - `base_ft<ctype>`：`ctype` facet 专用的基类特化，定义共享的 `mask` 类型及各字符分类掩码常量。
 * - `ctype_conf<char>`：`char` 特化，构造时通过 POSIX `*_l` 函数为全部 256 个字节值预建
 *   查找表，实现 O(1) 的字符分类、大小写转换与宽窄字符映射。
 * - `ctype_conf<CharT>`（`wchar_t` / `char32_t`）：宽字符特化，构造时初始化 `wctype_t`
 *   句柄，每次分类调用时通过 `iswctype_l`/`towupper_l`/`towlower_l` 查询 locale；
 *   `widen` 通过预建的 256 项查找表实现 O(1) 映射。
 * - `ctype_conf<char8_t>`：UTF-8 代码单元特化，利用 UTF-8 编码的结构保证将
 *   [0x00, 0x7F] 区间委托给内部 `ctype_conf<char>` 处理，对 [0x80, 0xFF] 区间
 *   采用保守策略（不分类、不转换大小写、不映射到 `char`）。
 *
 * @par 构建要求
 * 本文件依赖 POSIX.1-2008（需定义 `_POSIX_C_SOURCE >= 200809L` 或 `_GNU_SOURCE`）。
 * 所用的 `*_l` 字符分类与转换函数（`isupper_l`、`toupper_l`、`wctype_l`、`iswctype_l`、
 * `towupper_l` 等）是 POSIX 扩展，非标准 C++。构建系统须在包含任何系统头文件前定义
 * 相应的特性测试宏。
 * @endif
 *
 * @lang{EN}
 * Internal implementation details for the `ctype` facet.
 *
 * This file provides the following core components:
 * - `base_ft<ctype>`: The `ctype`-specific base class specialization, defining the
 *   shared `mask` type and character-classification mask constants.
 * - `ctype_conf<char>`: The `char` specialization; builds precomputed lookup tables
 *   for all 256 byte values at construction time using POSIX `*_l` functions,
 *   yielding O(1) classification, case conversion, and widen/narrow operations.
 * - `ctype_conf<CharT>` (`wchar_t` / `char32_t`): The wide-character specialization;
 *   initializes `wctype_t` handles at construction time and dispatches each
 *   classification call through `iswctype_l`/`towupper_l`/`towlower_l`; `widen`
 *   uses a precomputed 256-entry table for O(1) mapping.
 * - `ctype_conf<char8_t>`: The UTF-8 code-unit specialization; exploits the structural
 *   guarantees of UTF-8 to delegate the [0x00, 0x7F] range to an internal
 *   `ctype_conf<char>` and applies a conservative treatment to [0x80, 0xFF]
 *   (no classification, no case conversion, no `char` mapping).
 *
 * @par Build requirement
 * This file requires POSIX.1-2008 (`_POSIX_C_SOURCE >= 200809L` or `_GNU_SOURCE`).
 * The `*_l` character-classification and conversion functions used here
 * (`isupper_l`, `toupper_l`, `wctype_l`, `iswctype_l`, `towupper_l`, etc.) are
 * POSIX extensions, not standard C++. The build system must define the appropriate
 * feature-test macro before any system header is included.
 * @endif
 */

#pragma once
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <facet/facet_common.h>

#include <array>

#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

namespace IOv2
{
// The ctype lookup tables in this header and in ctype.h are sized as
// std::numeric_limits<unsigned char>::max() + 1, on the assumption that a
// byte is 8 bits and that the table covers all values an unsigned char can
// hold. Exotic platforms with CHAR_BIT != 8 (some DSPs, historical mainframes)
// would silently grow every per-byte table by 2^(CHAR_BIT-8)x. Reject them at
// compile time rather than ship a non-obviously-broken binary.
static_assert(CHAR_BIT == 8,
    "facet/ctype tables assume an 8-bit byte; see ctype.h::s_len");

/**
 * @lang{ZH}
 * `ctype` facet 模板的前向声明。
 * @endif
 *
 * @lang{EN}
 * Forward declaration of the `ctype` facet template.
 * @endif
 */
template <typename CharT> class ctype;

/**
 * @lang{ZH}
 * `ctype` facet 专用的基类特化。
 *
 * 定义所有 `ctype` 特化版本共享的 `mask` 类型及字符分类掩码常量。`mask` 为
 * `unsigned short` 类型的位域，每一位对应一个字符分类属性。
 *
 * @attention 新增原语掩码位时，须同步更新以下四处：
 * 1. 本类中的常量定义。
 * 2. `ctype_conf<CharT>` 中对应的私有成员 `m_wmask_<name>`。
 * 3. `ctype_conf<CharT>` 构造函数中对应的 `wctype_wrapper` 赋值语句。
 * 4. `ctype_conf<CharT>::is()` 中对应的 `iswctype_l` 检查分支。
 *
 * 遗漏第 2 或第 3 步会导致 `wctype_t` 未初始化，在 `iswctype_l` 时引发未定义行为；
 * 遗漏第 4 步会导致新位从 `is()` 结果中静默丢失。
 * @endif
 *
 * @lang{EN}
 * `ctype`-specific base class specialization.
 *
 * Defines the `mask` type and character-classification mask constants shared by
 * all `ctype` specializations. `mask` is a bitfield of type `unsigned short`,
 * where each bit corresponds to one character-classification property.
 *
 * @attention When adding a new primitive mask bit, update ALL of the following:
 * 1. The constant definitions in this class.
 * 2. The corresponding private member `m_wmask_<name>` in `ctype_conf<CharT>`.
 * 3. The corresponding `wctype_wrapper` assignment in the `ctype_conf<CharT>` constructor.
 * 4. The corresponding `iswctype_l` check in `ctype_conf<CharT>::is()`.
 *
 * Omitting step 2 or 3 leaves the `wctype_t` uninitialized — undefined behavior at
 * `iswctype_l`. Omitting step 4 silently drops the new bit from `is()` results.
 * @endif
 */
template <>
class base_ft<ctype> : public abs_ft
{
public:
    using abs_ft::abs_ft;

public:
    /** @lang{ZH} 字符分类属性的位掩码类型。 @endif
     *  @lang{EN} Bitmask type for character-classification properties. @endif */
    using mask = unsigned short;

    /** @lang{ZH} 大写字母（`isupper`）。 @endif
     *  @lang{EN} Uppercase letter (`isupper`). @endif */
    constexpr static mask upper  = 0x0001;

    /** @lang{ZH} 小写字母（`islower`）。 @endif
     *  @lang{EN} Lowercase letter (`islower`). @endif */
    constexpr static mask lower  = 0x0002;

    /** @lang{ZH} 字母（`isalpha`）。 @endif
     *  @lang{EN} Alphabetic character (`isalpha`). @endif */
    constexpr static mask alpha  = 0x0004;

    /** @lang{ZH} 十进制数字（`isdigit`）。 @endif
     *  @lang{EN} Decimal digit (`isdigit`). @endif */
    constexpr static mask digit  = 0x0008;

    /** @lang{ZH} 十六进制数字（`isxdigit`）。 @endif
     *  @lang{EN} Hexadecimal digit (`isxdigit`). @endif */
    constexpr static mask xdigit = 0x0010;

    /** @lang{ZH} 空白字符（`isspace`）。 @endif
     *  @lang{EN} Whitespace character (`isspace`). @endif */
    constexpr static mask space  = 0x0020;

    /** @lang{ZH} 可打印字符（`isprint`）。 @endif
     *  @lang{EN} Printable character (`isprint`). @endif */
    constexpr static mask print  = 0x0040;

    /** @lang{ZH} 控制字符（`iscntrl`）。 @endif
     *  @lang{EN} Control character (`iscntrl`). @endif */
    constexpr static mask cntrl  = 0x0080;

    /** @lang{ZH} 标点符号（`ispunct`）。 @endif
     *  @lang{EN} Punctuation character (`ispunct`). @endif */
    constexpr static mask punct  = 0x0100;

    /** @lang{ZH} 字母或数字（`alpha | digit`）。 @endif
     *  @lang{EN} Alphanumeric character (`alpha | digit`). @endif */
    constexpr static mask alnum = alpha | digit;

    /** @lang{ZH} 可见字符（`alnum | punct`）。 @endif
     *  @lang{EN} Visible (graphical) character (`alnum | punct`). @endif */
    constexpr static mask graph = alnum | punct;

    /** @lang{ZH} 所有分类位的并集。 @endif
     *  @lang{EN} Union of all classification bits. @endif */
    constexpr static mask all = upper | lower | alpha | digit | xdigit |
        space | print | graph | cntrl | punct;
};

template <typename CharT> class ctype_conf;

/**
 * @lang{ZH}
 * `char` 特化的 ctype 配置类。
 *
 * 构造时通过 POSIX `*_l` 函数为全部 256 个 `unsigned char` 值预建三张查找表：
 * `m_table`（分类掩码）、`m_toupper_table`（大写映射）、`m_tolower_table`（小写映射）。
 * 所有字符操作均为 O(1) 的查表操作。
 *
 * `widen` 为恒等映射（`char` 到 `char` 无需转换）；`narrow` 始终成功（`char` 本身即窄字符）。
 * @endif
 *
 * @lang{EN}
 * `ctype` configuration class specialization for `char`.
 *
 * At construction time, three lookup tables are built using POSIX `*_l` functions
 * for all 256 `unsigned char` values: `m_table` (classification mask),
 * `m_toupper_table` (uppercase mapping), and `m_tolower_table` (lowercase mapping).
 * All character operations are O(1) table lookups.
 *
 * `widen` is the identity mapping (`char` to `char` requires no conversion);
 * `narrow` always succeeds (`char` is itself a narrow character).
 * @endif
 */
template <>
class ctype_conf<char> : public ft_basic<ctype<char>>
{
public:
    /**
     * @lang{ZH}
     * 构造函数。通过 POSIX `*_l` 函数为全部 256 个字节值预建分类掩码表、
     * 大写映射表和小写映射表。
     * @endif
     *
     * @lang{EN}
     * Constructor. Builds the classification mask table, uppercase mapping table,
     * and lowercase mapping table for all 256 byte values using POSIX `*_l` functions.
     * @endif
     *
     * @param name
     * @lang{ZH} 用于查询的 locale 名称。 @endif
     * @lang{EN} The locale name to use for table construction. @endif
     */
    ctype_conf(const std::string& name)
        : ft_basic<ctype<char>>()
        , m_inter_locale(name.c_str())
    {
        // Build lookup tables using standard POSIX functions
        for (unsigned c = 0; c <= std::numeric_limits<unsigned char>::max(); ++c)
        {
            m_toupper_table[c] = toupper_l(static_cast<int>(c), m_inter_locale.c_locale);
            m_tolower_table[c] = tolower_l(static_cast<int>(c), m_inter_locale.c_locale);

            mask m = 0;
            if (isupper_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::upper;
            if (islower_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::lower;
            if (isalpha_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::alpha;
            if (isdigit_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::digit;
            if (isxdigit_l(static_cast<int>(c), m_inter_locale.c_locale)) m |= base_ft<ctype>::xdigit;
            if (isspace_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::space;
            if (isprint_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::print;
            if (iscntrl_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::cntrl;
            if (ispunct_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::punct;
            m_table[c] = m;
        }
    }

public:
    /**
     * @lang{ZH}
     * 返回字符 `c` 的分类掩码（O(1) 查表）。
     * @endif
     *
     * @lang{EN}
     * Returns the classification mask for character `c` (O(1) table lookup).
     * @endif
     *
     * @param c
     * @lang{ZH} 待分类的字符。 @endif
     * @lang{EN} The character to classify. @endif
     *
     * @return
     * @lang{ZH} `c` 对应的分类掩码，为 `base_ft<ctype>` 中各掩码常量的位或组合。 @endif
     * @lang{EN} The classification mask for `c`, a bitwise OR of the mask constants
     * in `base_ft<ctype>`. @endif
     */
    [[nodiscard]] virtual mask is(char c) const
    {
        return m_table[static_cast<unsigned char>(c)];
    }

    /**
     * @lang{ZH}
     * 返回字符 `c` 对应的大写形式（O(1) 查表）。
     * @endif
     *
     * @lang{EN}
     * Returns the uppercase form of character `c` (O(1) table lookup).
     * @endif
     *
     * @param c
     * @lang{ZH} 待转换的字符。 @endif
     * @lang{EN} The character to convert. @endif
     *
     * @return
     * @lang{ZH} `c` 的大写形式；若无对应大写则返回 `c` 本身。 @endif
     * @lang{EN} The uppercase form of `c`; returns `c` unchanged if no uppercase
     * mapping exists. @endif
     */
    [[nodiscard]] virtual char toupper(char c) const
    {
        return static_cast<char>(m_toupper_table[static_cast<unsigned char>(c)]);
    }

    /**
     * @lang{ZH}
     * 返回字符 `c` 对应的小写形式（O(1) 查表）。
     * @endif
     *
     * @lang{EN}
     * Returns the lowercase form of character `c` (O(1) table lookup).
     * @endif
     *
     * @param c
     * @lang{ZH} 待转换的字符。 @endif
     * @lang{EN} The character to convert. @endif
     *
     * @return
     * @lang{ZH} `c` 的小写形式；若无对应小写则返回 `c` 本身。 @endif
     * @lang{EN} The lowercase form of `c`; returns `c` unchanged if no lowercase
     * mapping exists. @endif
     */
    [[nodiscard]] virtual char tolower(char c) const
    {
        return static_cast<char>(m_tolower_table[static_cast<unsigned char>(c)]);
    }

    /**
     * @lang{ZH}
     * 将窄字符 `c` 拓宽为 `char`（恒等映射）。
     * @endif
     *
     * @lang{EN}
     * Widens the narrow character `c` to `char` (identity mapping).
     * @endif
     *
     * @param c
     * @lang{ZH} 待拓宽的字符。 @endif
     * @lang{EN} The character to widen. @endif
     *
     * @return
     * @lang{ZH} `c` 本身。 @endif
     * @lang{EN} `c` unchanged. @endif
     */
    [[nodiscard]] virtual char widen(char c) const { return c; }

    /**
     * @lang{ZH}
     * 将字符 `c` 窄化为 `char`（始终成功）。
     * @endif
     *
     * @lang{EN}
     * Narrows character `c` to `char` (always succeeds).
     * @endif
     *
     * @param c
     * @lang{ZH} 待窄化的字符。 @endif
     * @lang{EN} The character to narrow. @endif
     *
     * @return
     * @lang{ZH} 包含 `c` 的 `std::optional<char>`，永不为 `std::nullopt`。 @endif
     * @lang{EN} A `std::optional<char>` containing `c`; never `std::nullopt`. @endif
     */
    [[nodiscard]] virtual std::optional<char> narrow(char c) const { return c; }

private:
    /** @lang{ZH} 持有构造时所用 locale 的 POSIX locale 包装对象。 @endif
     *  @lang{EN} POSIX locale wrapper holding the locale used at construction. @endif */
    clocale_wrapper m_inter_locale;

    /** @lang{ZH} 预建的大写映射表，索引为 `unsigned char` 值，元素为对应的大写 `int` 值。 @endif
     *  @lang{EN} Precomputed uppercase mapping table; indexed by `unsigned char` value,
     *  elements are the corresponding uppercase `int` values. @endif */
    std::array<int,  std::numeric_limits<unsigned char>::max() + 1> m_toupper_table {};

    /** @lang{ZH} 预建的小写映射表，索引为 `unsigned char` 值，元素为对应的小写 `int` 值。 @endif
     *  @lang{EN} Precomputed lowercase mapping table; indexed by `unsigned char` value,
     *  elements are the corresponding lowercase `int` values. @endif */
    std::array<int,  std::numeric_limits<unsigned char>::max() + 1> m_tolower_table {};

    /** @lang{ZH} 预建的分类掩码表，索引为 `unsigned char` 值，元素为对应的 `mask` 位域。 @endif
     *  @lang{EN} Precomputed classification mask table; indexed by `unsigned char` value,
     *  elements are the corresponding `mask` bitmasks. @endif */
    std::array<mask, std::numeric_limits<unsigned char>::max() + 1> m_table {};
};

/**
 * @lang{ZH}
 * 宽字符类型（`wchar_t` 及与 `wchar_t` 等价的 `char32_t`）的 ctype 配置类特化。
 *
 * 构造时通过 `wctype_l` 为每个字符分类属性初始化对应的 `wctype_t` 句柄，同时预建
 * `m_widen` 表（通过 `btowc` 完成 256 个字节到 `CharT` 的映射）。每次字符分类、大小写
 * 转换调用时，均通过 `iswctype_l`/`towupper_l`/`towlower_l` 实时查询 locale，不缓存
 * 每码点结果。
 *
 * 对超出 `wchar_t` 表示范围的 `char32_t` 值（由 `out_of_wchar_range` 检测），`is`
 * 返回 0，`toupper`/`tolower` 原样返回输入，`narrow` 返回 `std::nullopt`。
 *
 * @tparam CharT
 * @lang{ZH}
 * 目标宽字符类型。须为 `wchar_t`，或在 `char32_t` 与 `wchar_t` 等价的平台上为
 * `char32_t`（由 `requires` 子句在编译期强制验证）。
 * @endif
 * @lang{EN}
 * The target wide character type. Must be `wchar_t`, or `char32_t` on platforms
 * where `char32_t` and `wchar_t` are equivalent (enforced at compile time by the
 * `requires` clause).
 * @endif
 *
 * @lang{EN}
 * `ctype` configuration class specialization for wide character types
 * (`wchar_t` and `char32_t` when equivalent to `wchar_t`).
 *
 * At construction time, `wctype_l` is called once per classification category to
 * initialize the corresponding `wctype_t` handle, and the `m_widen` table is built
 * via `btowc` for all 256 byte values. Each classification or case-conversion call
 * dispatches to `iswctype_l`/`towupper_l`/`towlower_l` at call time without caching
 * per-code-point results.
 *
 * For `char32_t` values outside the `wchar_t` representable range (detected by
 * `out_of_wchar_range`): `is` returns 0, `toupper`/`tolower` return the input
 * unchanged, and `narrow` returns `std::nullopt`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
                (std::is_same_v<CharT, char32_t> &&
                 (sizeof(char32_t) == sizeof(wchar_t)) &&
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class ctype_conf<CharT> : public ft_basic<ctype<CharT>>
{
public:
    /**
     * @lang{ZH}
     * 构造函数。通过 `wctype_l` 初始化每个字符分类属性对应的 `wctype_t` 句柄，
     * 并通过 `btowc` 预建 256 项 `m_widen` 查找表。
     *
     * 若任意 `wctype_l` 调用返回 0（分类名称不被 locale 支持），则抛出 `cvt_error`。
     * @endif
     *
     * @lang{EN}
     * Constructor. Initializes one `wctype_t` handle per classification category via
     * `wctype_l`, and builds the 256-entry `m_widen` lookup table via `btowc`.
     *
     * Throws `cvt_error` if any `wctype_l` call returns 0 (category not supported
     * by the locale).
     * @endif
     *
     * @param name
     * @lang{ZH} 用于查询的 locale 名称。 @endif
     * @lang{EN} The locale name to use for table construction. @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若 `wctype_l` 对任意分类名称返回 0。 @endif
     * @lang{EN} If `wctype_l` returns 0 for any classification category name. @endif
     */
    ctype_conf(const std::string& name)
        : ft_basic<ctype<CharT>>()
        , m_inter_locale(name.c_str())
    {
        {
            clocale_user guard(m_inter_locale);
            for (size_t j = 0; j < std::size(m_widen); ++j)
                m_widen[j] = btowc(static_cast<int>(j));
        }

        auto wctype_wrapper = [&](const char* category)
        {
            wctype_t res = wctype_l(category, m_inter_locale.c_locale);
            if (res == 0)
                throw cvt_error(std::string("ctype_conf constructor failed: wctype_l returned 0 for category ") + category);
            return res;
        };
        m_wmask_upper  = wctype_wrapper("upper");
        m_wmask_lower  = wctype_wrapper("lower");
        m_wmask_alpha  = wctype_wrapper("alpha");
        m_wmask_digit  = wctype_wrapper("digit");
        m_wmask_xdigit = wctype_wrapper("xdigit");
        m_wmask_space  = wctype_wrapper("space");
        m_wmask_print  = wctype_wrapper("print");
        m_wmask_cntrl  = wctype_wrapper("cntrl");
        m_wmask_punct  = wctype_wrapper("punct");
    }

    /**
     * @lang{ZH}
     * 返回宽字符 `_c` 的分类掩码，通过 `iswctype_l` 逐属性查询。
     *
     * 对超出 `wchar_t` 表示范围的值（`out_of_wchar_range` 返回 `true`），直接返回 0。
     * @endif
     *
     * @lang{EN}
     * Returns the classification mask for wide character `_c` by querying each
     * attribute via `iswctype_l`.
     *
     * Returns 0 immediately for values outside the `wchar_t` representable range
     * (when `out_of_wchar_range` returns `true`).
     * @endif
     *
     * @param _c
     * @lang{ZH} 待分类的宽字符。 @endif
     * @lang{EN} The wide character to classify. @endif
     *
     * @return
     * @lang{ZH} `_c` 对应的分类掩码，为 `base_ft<ctype>` 中各掩码常量的位或组合；
     * 超出范围时返回 0。 @endif
     * @lang{EN} The classification mask for `_c`, a bitwise OR of mask constants from
     * `base_ft<ctype>`; 0 if the value is out of range. @endif
     */
    [[nodiscard]] virtual base_ft<ctype>::mask is(CharT _c) const
    {
        if (out_of_wchar_range(_c)) return 0;

        base_ft<ctype>::mask res = 0;
        const auto c = static_cast<wchar_t>(_c);
        if (iswctype_l(c, m_wmask_upper, m_inter_locale.c_locale))  res |= base_ft<ctype>::upper;
        if (iswctype_l(c, m_wmask_lower, m_inter_locale.c_locale))  res |= base_ft<ctype>::lower;
        if (iswctype_l(c, m_wmask_alpha, m_inter_locale.c_locale))  res |= base_ft<ctype>::alpha;
        if (iswctype_l(c, m_wmask_digit, m_inter_locale.c_locale))  res |= base_ft<ctype>::digit;
        if (iswctype_l(c, m_wmask_xdigit, m_inter_locale.c_locale)) res |= base_ft<ctype>::xdigit;
        if (iswctype_l(c, m_wmask_space, m_inter_locale.c_locale))  res |= base_ft<ctype>::space;
        if (iswctype_l(c, m_wmask_print, m_inter_locale.c_locale))  res |= base_ft<ctype>::print;
        if (iswctype_l(c, m_wmask_cntrl, m_inter_locale.c_locale))  res |= base_ft<ctype>::cntrl;
        if (iswctype_l(c, m_wmask_punct, m_inter_locale.c_locale))  res |= base_ft<ctype>::punct;
        return res;
    }

    /**
     * @lang{ZH}
     * 通过 `towupper_l` 返回宽字符 `c` 的大写形式。
     *
     * 对超出 `wchar_t` 表示范围的值，原样返回 `c`。
     * @endif
     *
     * @lang{EN}
     * Returns the uppercase form of wide character `c` via `towupper_l`.
     *
     * Returns `c` unchanged for values outside the `wchar_t` representable range.
     * @endif
     *
     * @param c
     * @lang{ZH} 待转换的宽字符。 @endif
     * @lang{EN} The wide character to convert. @endif
     *
     * @return
     * @lang{ZH} `c` 的大写形式；若 `c` 超出范围或无对应大写，则返回 `c` 本身。 @endif
     * @lang{EN} The uppercase form of `c`; returns `c` unchanged if out of range
     * or if no uppercase mapping exists. @endif
     */
    [[nodiscard]] virtual CharT toupper(CharT c) const
    {
        if (out_of_wchar_range(c)) return c;
        return towupper_l(static_cast<wchar_t>(c), m_inter_locale.c_locale);
    }

    /**
     * @lang{ZH}
     * 通过 `towlower_l` 返回宽字符 `c` 的小写形式。
     *
     * 对超出 `wchar_t` 表示范围的值，原样返回 `c`。
     * @endif
     *
     * @lang{EN}
     * Returns the lowercase form of wide character `c` via `towlower_l`.
     *
     * Returns `c` unchanged for values outside the `wchar_t` representable range.
     * @endif
     *
     * @param c
     * @lang{ZH} 待转换的宽字符。 @endif
     * @lang{EN} The wide character to convert. @endif
     *
     * @return
     * @lang{ZH} `c` 的小写形式；若 `c` 超出范围或无对应小写，则返回 `c` 本身。 @endif
     * @lang{EN} The lowercase form of `c`; returns `c` unchanged if out of range
     * or if no lowercase mapping exists. @endif
     */
    [[nodiscard]] virtual CharT tolower(CharT c) const
    {
        if (out_of_wchar_range(c)) return c;
        return towlower_l(static_cast<wchar_t>(c), m_inter_locale.c_locale);
    }

    /**
     * @lang{ZH}
     * 将窄字符 `c` 拓宽为 `CharT`（O(1) 查表）。
     *
     * 语义与 `std::ctype<wchar_t>::widen()` 一致：仅保证对基本源字符集中的字符结果正确。
     * 对于多字节 locale（如 UTF-8），高字节不是独立字符，`btowc()` 在建表时对其返回
     * `WEOF`；因此 `m_widen` 中对应条目存储的是 `WEOF` 强转为 `CharT` 的值（一个看似
     * 哨兵但实际未定义含义的值）。调用方不得对此类字节调用 `widen()`。
     * @endif
     *
     * @lang{EN}
     * Widens the narrow character `c` to `CharT` (O(1) table lookup).
     *
     * Matches `std::ctype<wchar_t>::widen()` semantics: only required to be
     * correct for the basic source character set. For multi-byte locales
     * (e.g. UTF-8), high bytes are not standalone characters and `btowc()`
     * returns `WEOF` for them at table-build time; the corresponding entries
     * in `m_widen` hold `WEOF` cast to `CharT` (a sentinel-looking but
     * unspecified value). Callers must not call `widen()` on such bytes.
     * @endif
     *
     * @param c
     * @lang{ZH} 待拓宽的窄字符。 @endif
     * @lang{EN} The narrow character to widen. @endif
     *
     * @return
     * @lang{ZH} `c` 在当前 locale 下对应的 `CharT` 值；对高字节返回值未定义。 @endif
     * @lang{EN} The `CharT` value corresponding to `c` in the current locale;
     * the result is unspecified for high bytes. @endif
     */
    [[nodiscard]] virtual CharT widen(char c) const
    {
        return static_cast<CharT>(m_widen[static_cast<unsigned char>(c)]);
    }

    /**
     * @lang{ZH}
     * 将宽字符 `wc` 窄化为 `char`，通过 `wctob` 实现。
     *
     * 对超出 `wchar_t` 表示范围的值，返回 `std::nullopt`。对 `wctob` 返回 `EOF`
     * 的值（无对应单字节表示），同样返回 `std::nullopt`。
     *
     * @note `wctob` 没有 `_l` 变体，读取的是调用线程当前激活的 locale。因此本函数
     * 在调用 `wctob` 期间通过 `clocale_user` 临时将线程 locale 切换为 `m_inter_locale`
     * 并在作用域结束时恢复。
     *
     * **可见副作用**：在此函数执行窗口内，调用线程的 locale 为 `m_inter_locale`，而非
     * 进入时的外部 locale。此期间若有任何代码执行 locale 敏感的 IO（如 `std::ostream`
     * 数字格式化、`std::format`、第三方 locale 感知代码），将观察到 `m_inter_locale`
     * 而非线程原有 locale。派生类重写本函数时应保持其为纯计算操作，避免在重写内部触发
     * locale 敏感的副作用；这与 `std::ctype` facet 约定所隐含的指导原则一致。
     * @endif
     *
     * @lang{EN}
     * Narrows wide character `wc` to `char` via `wctob`.
     *
     * Returns `std::nullopt` for values outside the `wchar_t` representable range,
     * and for values for which `wctob` returns `EOF` (no single-byte representation).
     *
     * @note `wctob` has no `_l` variant — it reads whatever locale is currently active
     * on the calling thread. Therefore this function temporarily installs
     * `m_inter_locale` via `clocale_user` for the duration of the `wctob` call and
     * restores the previous locale on scope exit.
     *
     * **Visible side effect**: for the window of this call, the calling thread's locale
     * is `m_inter_locale`, not whatever it was on entry. Anything reached during this
     * call — in particular, a callback dispatched through a user-derived override of
     * this function — that performs locale-sensitive I/O (`std::ostream` numeric
     * formatting, `std::format`, third-party locale-aware code) will observe
     * `m_inter_locale` instead of the thread's outer locale. Derivations should keep
     * this function pure-computational and avoid locale-sensitive side effects inside
     * the override; the same guidance the `std::ctype` facet conventions imply.
     * @endif
     *
     * @param wc
     * @lang{ZH} 待窄化的宽字符。 @endif
     * @lang{EN} The wide character to narrow. @endif
     *
     * @return
     * @lang{ZH} 包含窄化结果的 `std::optional<char>`；若无对应单字节表示或值超出范围则为 `std::nullopt`。 @endif
     * @lang{EN} A `std::optional<char>` containing the narrowed result; `std::nullopt`
     * if no single-byte representation exists or the value is out of range. @endif
     */
    [[nodiscard]] virtual std::optional<char> narrow(CharT wc) const
    {
        if (out_of_wchar_range(wc)) return std::nullopt;
        clocale_user guard(m_inter_locale);
        const int c = wctob(wc);
        if (c == EOF) return std::nullopt;
        else return static_cast<char>(c);
    }

private:
    // The out-of-wchar-range guards used by is/toupper/tolower/narrow
    // above are provided by IOv2::out_of_wchar_range in facet_common.h
    // (reusable across facets); resolved here via unqualified lookup
    // since this class lives in the same namespace.

    /** @lang{ZH} 持有构造时所用 locale 的 POSIX locale 包装对象。 @endif
     *  @lang{EN} POSIX locale wrapper holding the locale used at construction. @endif */
    clocale_wrapper   m_inter_locale;

    /** @lang{ZH} 预建的宽化查找表，索引为 `unsigned char` 值，元素为 `btowc` 返回的 `wint_t` 值。 @endif
     *  @lang{EN} Precomputed widen lookup table; indexed by `unsigned char` value,
     *  elements are the `wint_t` values returned by `btowc`. @endif */
    std::array<wint_t, 1 + std::numeric_limits<unsigned char>::max()> m_widen {};

    /** @lang{ZH} `upper` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `upper` classification category. @endif */
    wctype_t          m_wmask_upper;

    /** @lang{ZH} `lower` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `lower` classification category. @endif */
    wctype_t          m_wmask_lower;

    /** @lang{ZH} `alpha` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `alpha` classification category. @endif */
    wctype_t          m_wmask_alpha;

    /** @lang{ZH} `digit` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `digit` classification category. @endif */
    wctype_t          m_wmask_digit;

    /** @lang{ZH} `xdigit` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `xdigit` classification category. @endif */
    wctype_t          m_wmask_xdigit;

    /** @lang{ZH} `space` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `space` classification category. @endif */
    wctype_t          m_wmask_space;

    /** @lang{ZH} `print` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `print` classification category. @endif */
    wctype_t          m_wmask_print;

    /** @lang{ZH} `cntrl` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `cntrl` classification category. @endif */
    wctype_t          m_wmask_cntrl;

    /** @lang{ZH} `punct` 分类对应的 `wctype_t` 句柄。 @endif
     *  @lang{EN} `wctype_t` handle for the `punct` classification category. @endif */
    wctype_t          m_wmask_punct;
};

/**
 * @lang{ZH}
 * UTF-8 代码单元（`char8_t`）的 ctype 配置类特化。
 *
 * 设计基于 UTF-8 编码的两条结构保证：
 * 1. `[0x00, 0x7F]` 区间内的字节不会出现在多字节序列的前导字节或续字节位置，因此该
 *    区间内的代码单元与 ASCII 完全兼容，可像对应的 `char` 值一样在 locale 下精确分类。
 * 2. `[0x80, 0xFF]` 区间内的字节要么是多字节序列的前导字节（`0xC0–0xFF`），要么是续字节
 *    （`0x80–0xBF`），不代表独立的 Unicode 码点。
 *
 * 对 `[0x00, 0x7F]`：所有操作委托给内部 `ctype_conf<char>`，后者在构造时为全部 256 个
 * `char` 值建立 locale 感知的查找表；仅该表的低半部分在此处有意义，高半部分不被访问。
 *
 * 对 `[0x80, 0xFF]`：各 API 采用以下保守策略：
 * - `is`：返回 0（非独立码点，无分类属性）。
 * - `toupper`/`tolower`：原样返回输入（无大小写信息）。
 * - `widen`：原样转换为 `char8_t`（`char8_t` 与 `unsigned char` 等值域，无 `WEOF` 哨兵问题）。
 * - `narrow`：返回 `std::nullopt`（非独立码点，无对应 `char`）。
 * @endif
 *
 * @lang{EN}
 * `ctype` configuration class specialization for UTF-8 code units (`char8_t`).
 *
 * The design rests on two structural guarantees of the UTF-8 encoding:
 * 1. No byte in `[0x00, 0x7F]` ever appears as a leading or continuation byte of a
 *    multi-byte sequence, so code units in that range are ASCII-compatible and can be
 *    classified exactly like the corresponding `char` value in the locale.
 * 2. Every byte in `[0x80, 0xFF]` is either a leading byte (`0xC0–0xFF`) or a
 *    continuation byte (`0x80–0xBF`) of a multi-byte sequence, and therefore does not
 *    represent a standalone Unicode code point.
 *
 * For `[0x00, 0x7F]`: all operations delegate to an internal `ctype_conf<char>`,
 * which builds locale-aware lookup tables for all 256 `char` values at construction
 * time. Only the lower half of that table is meaningful here; the high half is never
 * accessed.
 *
 * For `[0x80, 0xFF]`: each API applies the following conservative treatment:
 * - `is`: returns 0 (not a standalone code point; no classification applies).
 * - `toupper`/`tolower`: returns the input unchanged (no case information).
 * - `widen`: converts trivially to `char8_t` (`char8_t` has the same range as
 *   `unsigned char`, so every `char` maps to exactly one `char8_t` code unit with
 *   no loss and no `WEOF` sentinel issue).
 * - `narrow`: returns `std::nullopt` (not a standalone code point; no `char` equivalent).
 * @endif
 */
template <>
class ctype_conf<char8_t> : public ft_basic<ctype<char8_t>>
{
public:
    /**
     * @lang{ZH}
     * 构造函数。初始化内部 `ctype_conf<char>` 以建立 locale 感知的查找表。
     * @endif
     *
     * @lang{EN}
     * Constructor. Initializes the internal `ctype_conf<char>` to build
     * locale-aware lookup tables.
     * @endif
     *
     * @param name
     * @lang{ZH} 用于查询的 locale 名称。 @endif
     * @lang{EN} The locale name to use for table construction. @endif
     */
    ctype_conf(const std::string& name)
        : ft_basic<ctype<char8_t>>()
        , m_internal(name)
    {}

public:
    /**
     * @lang{ZH}
     * 返回 `char8_t` 代码单元 `c` 的分类掩码。
     *
     * 对 `[0x80, 0xFF]`（非独立码点），直接返回 0。
     * 对 `[0x00, 0x7F]`，委托给内部 `ctype_conf<char>::is`。
     * @endif
     *
     * @lang{EN}
     * Returns the classification mask for `char8_t` code unit `c`.
     *
     * Returns 0 for `[0x80, 0xFF]` (not a standalone code point).
     * Delegates to the internal `ctype_conf<char>::is` for `[0x00, 0x7F]`.
     * @endif
     *
     * @param c
     * @lang{ZH} 待分类的 UTF-8 代码单元。 @endif
     * @lang{EN} The UTF-8 code unit to classify. @endif
     *
     * @return
     * @lang{ZH} `c` 的分类掩码；对 `[0x80, 0xFF]` 返回 0。 @endif
     * @lang{EN} The classification mask for `c`; 0 for `[0x80, 0xFF]`. @endif
     */
    [[nodiscard]] virtual mask is(char8_t c) const
    {
        if (c & 0x80)
            return static_cast<mask>(0);  // not a standalone code point; no classification applies
        return m_internal.is(static_cast<char>(c));
    }

    /**
     * @lang{ZH}
     * 返回 `char8_t` 代码单元 `c` 的大写形式。
     *
     * 对 `[0x80, 0xFF]`（非独立码点，无大小写信息），原样返回 `c`。
     * 对 `[0x00, 0x7F]`，委托给内部 `ctype_conf<char>::toupper`。
     * @endif
     *
     * @lang{EN}
     * Returns the uppercase form of `char8_t` code unit `c`.
     *
     * Returns `c` unchanged for `[0x80, 0xFF]` (not a standalone code point;
     * no case information). Delegates to the internal `ctype_conf<char>::toupper`
     * for `[0x00, 0x7F]`.
     * @endif
     *
     * @param c
     * @lang{ZH} 待转换的 UTF-8 代码单元。 @endif
     * @lang{EN} The UTF-8 code unit to convert. @endif
     *
     * @return
     * @lang{ZH} `c` 的大写形式；对 `[0x80, 0xFF]` 原样返回 `c`。 @endif
     * @lang{EN} The uppercase form of `c`; returns `c` unchanged for `[0x80, 0xFF]`. @endif
     */
    [[nodiscard]] virtual char8_t toupper(char8_t c) const
    {
        if (c & 0x80) return c;  // not a standalone code point; no case information
        return static_cast<char8_t>(m_internal.toupper(static_cast<char>(c)));
    }

    /**
     * @lang{ZH}
     * 返回 `char8_t` 代码单元 `c` 的小写形式。
     *
     * 对 `[0x80, 0xFF]`（非独立码点，无大小写信息），原样返回 `c`。
     * 对 `[0x00, 0x7F]`，委托给内部 `ctype_conf<char>::tolower`。
     * @endif
     *
     * @lang{EN}
     * Returns the lowercase form of `char8_t` code unit `c`.
     *
     * Returns `c` unchanged for `[0x80, 0xFF]` (not a standalone code point;
     * no case information). Delegates to the internal `ctype_conf<char>::tolower`
     * for `[0x00, 0x7F]`.
     * @endif
     *
     * @param c
     * @lang{ZH} 待转换的 UTF-8 代码单元。 @endif
     * @lang{EN} The UTF-8 code unit to convert. @endif
     *
     * @return
     * @lang{ZH} `c` 的小写形式；对 `[0x80, 0xFF]` 原样返回 `c`。 @endif
     * @lang{EN} The lowercase form of `c`; returns `c` unchanged for `[0x80, 0xFF]`. @endif
     */
    [[nodiscard]] virtual char8_t tolower(char8_t c) const
    {
        if (c & 0x80) return c;  // not a standalone code point; no case information
        return static_cast<char8_t>(m_internal.tolower(static_cast<char>(c)));
    }

    /**
     * @lang{ZH}
     * 将窄字符 `c` 拓宽为 `char8_t`（trivial 转换，无精度损失）。
     *
     * `char8_t` 与 `unsigned char` 具有相同的值域，因此每个 `char` 值都恰好映射为
     * 一个 `char8_t` 代码单元，不存在 `WEOF` 哨兵问题（对比 `wchar_t`/`char32_t`
     * 特化中 `btowc` 对高字节返回 `WEOF` 的情形）。
     * @endif
     *
     * @lang{EN}
     * Widens the narrow character `c` to `char8_t` (trivial conversion, no loss).
     *
     * `char8_t` has the same range as `unsigned char`, so every `char` value maps
     * to exactly one `char8_t` code unit with no loss and no `WEOF` sentinel issue
     * (cf. the `wchar_t`/`char32_t` specialization, where `btowc()` may return
     * `WEOF` for high bytes).
     * @endif
     *
     * @param c
     * @lang{ZH} 待拓宽的窄字符。 @endif
     * @lang{EN} The narrow character to widen. @endif
     *
     * @return
     * @lang{ZH} `c` 对应的 `char8_t` 代码单元。 @endif
     * @lang{EN} The `char8_t` code unit corresponding to `c`. @endif
     */
    [[nodiscard]] virtual char8_t widen(char c) const { return static_cast<char8_t>(c); }

    /**
     * @lang{ZH}
     * 将 `char8_t` 代码单元 `c` 窄化为 `char`。
     *
     * 对 `[0x80, 0xFF]`（非独立码点，无对应 `char`），返回 `std::nullopt`。
     * 对 `[0x00, 0x7F]`，直接转换为 `char` 并返回（值域一致，始终成功）。
     * @endif
     *
     * @lang{EN}
     * Narrows `char8_t` code unit `c` to `char`.
     *
     * Returns `std::nullopt` for `[0x80, 0xFF]` (not a standalone code point;
     * no `char` equivalent). For `[0x00, 0x7F]`, converts directly to `char`
     * (ranges are compatible; always succeeds).
     * @endif
     *
     * @param c
     * @lang{ZH} 待窄化的 UTF-8 代码单元。 @endif
     * @lang{EN} The UTF-8 code unit to narrow. @endif
     *
     * @return
     * @lang{ZH} 包含窄化结果的 `std::optional<char>`；对 `[0x80, 0xFF]` 返回 `std::nullopt`。 @endif
     * @lang{EN} A `std::optional<char>` containing the narrowed result; `std::nullopt`
     * for `[0x80, 0xFF]`. @endif
     */
    [[nodiscard]] virtual std::optional<char> narrow(char8_t c) const
    {
        if (c & 0x80) return std::nullopt;  // not a standalone code point; no char equivalent
        return static_cast<char>(c);
    }

private:
    /** @lang{ZH} 内部 `char` 特化实例，提供 `[0x00, 0x7F]` 区间的 locale 感知查找表。 @endif
     *  @lang{EN} Internal `char` specialization instance providing locale-aware lookup
     *  tables for the `[0x00, 0x7F]` range. @endif */
    ctype_conf<char> m_internal;
};
}

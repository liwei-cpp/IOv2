/**
 * @file messages_details.h
 * @lang{ZH}
 * 定义了 `messages` facet 的实现细节，包括 `base_ft<messages>` 基类和
 * `messages_conf` 的各字符类型特化。`base_ft<messages>` 负责管理 gettext
 * 文本域绑定、语言解析和 `.mo` 文件的加载与解析；`messages_conf` 的各特化
 * 在此基础上提供具体字符类型的翻译字典。
 * @endif
 *
 * @lang{EN}
 * Defines the implementation details of the `messages` facet, including
 * the `base_ft<messages>` base class and per-character-type specializations
 * of `messages_conf`. `base_ft<messages>` manages gettext text-domain
 * binding, language resolution, and loading and parsing of `.mo` files;
 * the `messages_conf` specializations build per-character-type translation
 * dictionaries on top of that base.
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/code_cvt.h>
#include <cvt/cvt_facilities.h>
#include <cvt/root_cvt.h>
#include <device/mem_device.h>
#include <facet/facet_common.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace IOv2
{
template <typename CharT> class messages;

/**
 * @lang{ZH}
 * @brief `messages` facet 的基类，提供 gettext 文本域管理和 `.mo` 文件解析功能。
 *
 * 该类维护一个全局的文本域与目录的映射（`s_domain_dirs`），支持线程安全地
 * 绑定和查询域目录，并提供语言过滤（`filter_lang`）和 `.mo` 文件可用性检查
 * （`available`）等静态工具函数。`messages_conf` 的各特化类继承此基类，
 * 在构造时通过 `get_translate_dictionary` 加载翻译字典。
 *
 * @note 该类不直接实例化，仅作为 `messages_conf` 的基类使用。
 * @endif
 *
 * @lang{EN}
 * @brief Base class for the `messages` facet, providing gettext text-domain
 *        management and `.mo` file parsing.
 *
 * This class maintains a global mapping of text domains to directories
 * (`s_domain_dirs`), supports thread-safe binding and lookup of domain
 * directories, and provides static helpers for language filtering
 * (`filter_lang`) and `.mo` file availability checking (`available`).
 * The `messages_conf` specializations inherit this class and load their
 * translation dictionaries via `get_translate_dictionary` at construction.
 *
 * @note This class is not instantiated directly; it serves only as the
 *       base for `messages_conf`.
 * @endif
 */
template <>
class base_ft<messages> : public abs_ft
{
    /// @cond
    struct file_closer
    {
        file_closer(std::FILE* fp)
            : m_fp(fp) {}
        file_closer(const file_closer&) = delete;
        file_closer& operator=(const file_closer&) = delete;
        file_closer(file_closer&&) = delete;
        file_closer& operator=(file_closer&&) = delete;
        ~file_closer() { fclose(m_fp); } // NOLINT(cppcoreguidelines-owning-memory)
    private:
        std::FILE* m_fp;
    };
    /// @endcond

public:
    /**
     * @lang{ZH}
     * @brief 将文本域绑定到指定目录。
     *
     * 如果该域已存在，则更新其目录；否则插入新条目。此操作是线程安全的。
     *
     * @param domain 文本域名称（`.mo` 文件的基名）。
     * @param dirname 存放该域本地化文件的目录路径（gettext 的 `dirname`）。
     * @endif
     *
     * @lang{EN}
     * @brief Binds a text domain to a specified directory.
     *
     * If the domain already exists, its directory is updated; otherwise a new
     * entry is inserted. This operation is thread-safe.
     *
     * @param domain The text domain name (the `.mo` file's basename).
     * @param dirname The directory path holding the locale files for this domain
     *                (gettext's `dirname`).
     * @endif
     */
    static void bind_text_domain(const std::string& domain, const std::string& dirname)
    {
        std::scoped_lock guard(s_domain_mutex);
        auto it = s_domain_dirs.find(domain);
        if (it != s_domain_dirs.end())
            it->second = dirname;
        else
            s_domain_dirs.insert({domain, dirname});
    }

    /**
     * @lang{ZH}
     * @brief 检查指定域和语言对应的 `.mo` 文件是否可用。
     *
     * 先原子性地快照域的目录名，再委托给内部重载，以确保在调用期间
     * 即使另一线程通过 `bind_text_domain` 修改了目录绑定，
     * 单次调用也只观察到一致的目录名。
     *
     * @param domain 文本域名称。
     * @param lang 语言标识符，或 `:` 分隔的候选语言列表，或空字符串（使用环境变量）。
     * @return 如果对应的 `.mo` 文件存在，则返回 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Checks whether the `.mo` file for the given domain and language is available.
     *
     * The domain's dirname is resolved once (atomically snapshotted) and then
     * delegated to the internal overload, so that even if another thread
     * rebinds the domain via `bind_text_domain` concurrently, a single call
     * observes only one consistent dirname.
     *
     * @param domain The text domain name.
     * @param lang A language identifier, a `:` -separated list of candidate languages,
     *             or an empty string (falls back to environment variables).
     * @return `true` if the corresponding `.mo` file exists.
     * @endif
     */
    static bool available(const std::string& domain, const std::string& lang)
    {
        return available({.dirname = get_dirname(domain), .domain = domain}, lang);
    }

    /**
     * @lang{ZH}
     * @brief 从候选语言列表中找出第一个可用的语言。
     *
     * 对域的目录名进行一次快照，然后委托给内部重载。
     *
     * @param domain 文本域名称。
     * @param p_lang 语言标识符或 `:` 分隔的候选列表，或空字符串（使用环境变量）。
     * @return 第一个可用的语言字符串，若均不可用则返回空字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the first available language from a list of candidates.
     *
     * The domain's dirname is snapshotted once and then delegated to the
     * internal overload.
     *
     * @param domain The text domain name.
     * @param p_lang A language identifier, a `:` -separated candidate list,
     *               or an empty string (falls back to environment variables).
     * @return The first available language string, or an empty string if none is available.
     * @endif
     */
    static std::string filter_lang(const std::string& domain, const std::string& p_lang)
    {
        return filter_lang({.dirname = get_dirname(domain), .domain = domain}, p_lang);
    }

    /**
     * @lang{ZH}
     * @brief 构造函数，快照域目录名并初始化域信息。
     *
     * 在初始化列表中，`m_dirname` 先于 `m_filtered_lang` 和 `m_domain_info`
     * 初始化（按声明顺序），确保对 `filter_lang` 和域信息字符串的计算都使用
     * 同一个目录快照，从而避免与并发的 `bind_text_domain` 调用产生竞争。
     * `messages_conf::init` 通过 `dirname()` 读回该快照，因此整个构造过程
     * 从同一个 dirname 解析 `.mo` 文件位置，`domain_info()` 与实际加载的
     * 字典永远不会不一致。
     *
     * @param id facet 的唯一标识符。
     * @param p_domain 文本域名称。
     * @param p_lang 候选语言字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that snapshots the domain dirname and initializes domain info.
     *
     * In the member initializer list, `m_dirname` is initialized before
     * `m_filtered_lang` and `m_domain_info` (in declaration order), ensuring
     * that both `filter_lang` and the domain-info string computation use the
     * same dirname snapshot, avoiding races with concurrent `bind_text_domain`
     * calls. `messages_conf::init` reads the snapshot back via `dirname()`, so
     * the whole construction resolves the `.mo` location from one dirname and
     * `domain_info()` can never disagree with the dictionary actually loaded.
     *
     * @param id The unique identifier for the facet.
     * @param p_domain The text domain name.
     * @param p_lang The candidate language string.
     * @endif
     */
    base_ft(facet_id_t id, const std::string& p_domain, const std::string& p_lang)
        : abs_ft(id)
        // Snapshot the dirname first (declared before the members below, so it
        // is initialised first) and feed that same snapshot to filter_lang and
        // into m_domain_info. Together with messages_conf::init reading it back
        // via dirname(), the whole construction resolves the .mo location from
        // one dirname, so the recorded domain_info can never disagree with the
        // dictionary actually loaded.
        , m_dirname(get_dirname(p_domain))
        , m_filtered_lang(filter_lang({.dirname = m_dirname, .domain = p_domain}, p_lang))
        , m_domain_info('[' + p_domain + "] [" + p_lang + '(' + m_filtered_lang + ')' + "] [" +
                        m_dirname + "]")
    {}

    /**
     * @lang{ZH}
     * @brief 返回描述该 facet 实例的域信息字符串。
     *
     * 格式为 `[domain] [lang(filtered_lang)] [dirname]`，可用于调试和日志记录。
     *
     * @return 包含域、语言和目录信息的字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns a string describing this facet instance's domain information.
     *
     * The format is `[domain] [lang(filtered_lang)] [dirname]`, useful for
     * debugging and logging.
     *
     * @return A string containing domain, language, and directory information.
     * @endif
     */
    [[nodiscard]] const std::string& domain_info() const { return m_domain_info; }

    /**
     * @lang{ZH}
     * @brief 返回构造时确定的有效语言标识符。
     * @return 过滤后的语言字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the effective language identifier determined at construction.
     * @return The filtered language string.
     * @endif
     */
    [[nodiscard]] const std::string& filtered_lang() const { return m_filtered_lang; }

protected:
    /**
     * @lang{ZH}
     * @brief 返回构造时快照的域目录名。
     *
     * 派生类（`messages_conf` 的各特化）通过此函数获取目录名，
     * 以确保字典加载使用的目录与 `domain_info()` 报告的目录一致。
     *
     * @return 绑定到此域的目录路径。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the domain dirname snapshotted at construction.
     *
     * Derived classes (`messages_conf` specializations) read this so their
     * dictionary load uses the same directory that `domain_info()` reports.
     *
     * @return The directory path bound to this domain.
     * @endif
     */
    [[nodiscard]] const std::string& dirname() const { return m_dirname; }

    /**
     * @lang{ZH}
     * @brief 捆绑文本域名称与其目录的辅助结构体。
     *
     * `dirname` 和 `domain` 这两个类型相同的字符串在所有查找辅助函数中总是成对传递。
     * 将它们捆绑在一起（而非使用两个位置相同的 `std::string` 参数）可以避免意外交换
     * （解决 bugprone-easily-swappable-parameters 警告）。语言字符串保持独立参数，
     * 因为它会变化 —— `filter_lang()` 会针对一个固定的 `(dirname, domain)` 对
     * 测试多个候选语言。
     * @endif
     *
     * @lang{EN}
     * @brief Helper struct that bundles a text domain name with its directory.
     *
     * The two same-typed strings `dirname` and `domain` are always passed
     * together to lookup helpers. Bundling them here (rather than two
     * positional `std::string` parameters) makes them impossible to swap by
     * mistake (addressing bugprone-easily-swappable-parameters). The language
     * stays a separate argument because it varies independently — `filter_lang()`
     * probes several candidate languages against one fixed `(dirname, domain)` pair.
     * @endif
     */
    struct text_domain
    {
        std::string dirname;  ///< @lang{ZH} gettext 目录名：存放本地化文件树的目录。 @endif @lang{EN} gettext dirname: the directory holding the locale tree. @endif
        std::string domain;   ///< @lang{ZH} 文本域名：`.mo` 文件的基名。 @endif @lang{EN} text domain: the `.mo` file's basename. @endif
    };

    /**
     * @lang{ZH}
     * @brief 构建 `.mo` 文件的完整路径。
     *
     * 返回路径格式为 `<dirname>/<lang>/LC_MESSAGES/<domain>.mo`。
     *
     * @param td 包含目录名和域名的 `text_domain` 结构体。
     * @param lang 语言标识符。
     * @return `.mo` 文件的完整路径字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Builds the full path to a `.mo` file.
     *
     * The returned path follows the format `<dirname>/<lang>/LC_MESSAGES/<domain>.mo`.
     *
     * @param td A `text_domain` struct containing the dirname and domain name.
     * @param lang The language identifier.
     * @return The full path string to the `.mo` file.
     * @endif
     */
    static std::string get_domain_file(const text_domain& td, const std::string& lang)
    {
        std::filesystem::path dom{td.dirname};
        dom = dom / lang / "LC_MESSAGES" / (td.domain + ".mo");
        return dom.string();
    }

    /**
     * @lang{ZH}
     * @brief 解析 GNU gettext `.mo` 文件并返回翻译字典。
     *
     * 此函数仅实现 `gettext()`/`dgettext()` 语义，即简单的一对一
     * （`msgid` → `msgstr`）查找。
     *
     * **字符串读取方式：** 每个 `.mo` 字符串作为 NUL 终止的 C 字符串读取。
     * 条目的 `length` 字段可能更长（GNU gettext 使用嵌入的 NUL/EOT 分隔符将
     * 额外数据打包到同一记录中），但第一个 `'\0'` 之后的内容在此处被忽略，
     * 与 `gettext` 本身使用 `strcmp()` 比较 `msgid` 的行为一致，且映射键与调用者传入的值相同。
     *
     * **编码：** 假定字符串为 UTF-8 编码（GNU gettext >= 0.22 的 `msgfmt` 默认生成）。
     * `.mo` 文件头中 `Content-Type: ...; charset=...` 声明的字符集不被读取或处理，
     * 因此非 UTF-8 编码的 `.mo` 文件（旧版文件或使用 `msgfmt --no-convert` 构建的文件）
     * 的非 ASCII `msgstr` 将被错误解码。`msgid` 不受影响，因为 gettext 建议其使用 US-ASCII。
     *
     * **不支持的特性**（有意丢弃额外数据，而非疏忽）：
     * - `ngettext()`/`dngettext()` 复数形式：原文存储为 `"msgid\0msgid_plural"`，
     *   译文为 `"msgstr[0]\0msgstr[1].."`。支持它需要头部的 `Plural-Forms:` 规则、
     *   基于长度（而非 C 字符串）的记录存储，以及支持 `n` 的查找 API。
     * - `pgettext()` 消息上下文：原文存储为 `"msgctxt\x04msgid"`（EOT 分隔，无 NUL），
     *   因此此处的键为整个 `"msgctxt\x04msgid"` 块；支持上下文的 API 需要显式拼接该键。
     *
     * 如需支持 `ngettext`/`msgctxt`，需修改此函数及查找 API。
     *
     * @param filename `.mo` 文件的路径。
     * @return 从 `msgid`（UTF-8）到 `msgstr`（UTF-8）的翻译字典。
     * @throw stream_error 如果文件无法打开、格式非法，或字符串数量/长度超出合理范围。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a GNU gettext `.mo` file and returns a translation dictionary.
     *
     * This function only implements `gettext()`/`dgettext()` semantics, i.e. a
     * plain one-to-one (`msgid` → `msgstr`) lookup.
     *
     * **String reading:** Each `.mo` string is read as a NUL-terminated C string.
     * The record's stored `length` may be longer (GNU gettext packs extra data
     * using embedded NUL/EOT separators), but anything past the first `'\0'` is
     * intentionally ignored here, matching gettext's own behaviour of comparing
     * `msgid`s with `strcmp()` and keeping the map key identical to what a caller passes in.
     *
     * **Encoding:** Strings are assumed to be UTF-8 (the default that GNU gettext
     * >= 0.22's `msgfmt` produces). The `.mo` charset declared in the header
     * entry's `Content-Type: ...; charset=...` is neither read nor honoured, so a
     * `.mo` in another encoding (a legacy file, or one built with
     * `msgfmt --no-convert`) will have its non-ASCII `msgstr`s misdecoded.
     * `msgid`s are unaffected, as gettext recommends they be US-ASCII.
     *
     * **Unsupported features** (extra payload dropped intentionally, not by accident):
     * - `ngettext()`/`dngettext()` plural forms: the original is stored as
     *   `"msgid\0msgid_plural"` and the translation as `"msgstr[0]\0msgstr[1].."`.
     *   Supporting them needs the header's `Plural-Forms:` rule, length-based
     *   (not C-string) record storage, and an `n`-aware lookup API.
     * - `pgettext()` message contexts: the original is stored as
     *   `"msgctxt\x04msgid"` (EOT-separated, no NUL), so the key here is the
     *   whole `"msgctxt\x04msgid"` blob; a context-aware API would need to
     *   assemble that key explicitly.
     *
     * Revisit this function (and the lookup API) if `ngettext`/`msgctxt` support
     * is ever required.
     *
     * @param filename The path to the `.mo` file.
     * @return A translation dictionary mapping `msgid` (UTF-8) to `msgstr` (UTF-8).
     * @throw stream_error If the file cannot be opened, has an invalid format, or
     *                     the string count or length exceeds a reasonable bound.
     * @endif
     */
    static std::unordered_map<std::u8string, std::u8string> get_translate_dictionary(const std::string& filename)
    {
        std::FILE* fp = fopen(filename.c_str(), "rb"); // NOLINT(cppcoreguidelines-owning-memory)
        if (!fp)
            throw stream_error("get_translate_dictionary fail: cannot open file " + filename);

        file_closer guard(fp);

        if (std::fseek(fp, 0, SEEK_END) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");
        const long file_size = std::ftell(fp);
        if (file_size < 0)
            throw stream_error("get_translate_dictionary fail: file inconsistent");
        if (std::fseek(fp, 0, SEEK_SET) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");

        auto read_num = [fp](unsigned char* buf, bool need_swap)
        {
            if (std::fread(buf, 1, 4, fp) != 4)
                throw stream_error("get_translate_dictionary fail: invalid format");

            std::uint32_t res = 0;
            std::memcpy(&res, buf, 4);
            if (need_swap)
                res = std::byteswap(res);
            return res;
        };

        std::array<unsigned char, 8> buff{};
        if (std::fread(buff.data(), 1, 8, fp) != 8)
            throw stream_error("get_translate_dictionary fail: invalid format");

        bool need_swap = false;
        if ((buff[0] == 0x95) && (buff[1] == 0x04) && (buff[2] == 0x12) && (buff[3] == 0xde))
            need_swap = (std::endian::native == std::endian::little);
        else if ((buff[0] == 0xde) && (buff[1] == 0x12) && (buff[2] == 0x04) && (buff[3] == 0x95))
            need_swap = (std::endian::native == std::endian::big);
        else throw stream_error("get_translate_dictionary fail: invalid format");

        // Accept any .mo whose major revision (the high 16 bits) is 0, regardless of
        // minor revision. Only minor 0 (basic) and minor 1 (system-dependent strings)
        // are defined today, but minor revisions are backward-compatible additions:
        // they only add separate tables (sysdep strings, hash table) and never change
        // the regular orig/trans tables this parser reads. Sysdep strings live in their
        // own orig/trans_sysdep tables we never touch, so a sysdep string is simply
        // absent (lookup misses and the caller keeps the original) rather than returned
        // with its segment markers unexpanded. A non-zero major revision uses a layout
        // this basic parser does not commit to, so it is rejected.
        std::uint32_t revision = 0;
        std::memcpy(&revision, buff.data() + 4, 4);
        if (need_swap)
            revision = std::byteswap(revision);
        if ((revision >> 16) != 0)
            throw stream_error("get_translate_dictionary fail: unsupported .mo major revision");

        auto str_num = read_num(buff.data(), need_swap);
        auto ori_offset = read_num(buff.data(), need_swap);
        auto aim_offset = read_num(buff.data(), need_swap);

        // Each entry needs an 8-byte (length, offset) descriptor in both the
        // original and translation tables, so a valid str_num cannot exceed
        // file_size / 16. Rejecting larger counts caps the O(str_num) growth of
        // oris and the result map, which the per-string length cap does not.
        if (str_num > static_cast<std::uint64_t>(file_size) / 16u)
            throw stream_error("get_translate_dictionary fail: implausible string count");

        // The .mo file offsets (and string lengths) are unsigned 32-bit, but
        // std::fseek takes a signed long. Where long is 64-bit (e.g. LP64
        // Linux) every uint32_t is representable, so all seeks below are exact.
        // On a 32-bit-long platform (LLP64/ILP32) an offset >= 2 GiB would
        // convert to a negative long and seek wrongly — but that can only arise
        // from a > 2 GiB or corrupt .mo, and every fseek/fread return value is
        // checked, so the worst case is a clean "invalid format" throw, never a
        // misread or out-of-bounds access.
        if (std::fseek(fp, ori_offset, SEEK_SET) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");

        // The .mo format stores each string length as an unbounded 32-bit
        // integer, so the buffer is grown to fit each entry rather than
        // assuming a fixed maximum. A sanity cap guards against a corrupt or
        // malicious file requesting a huge allocation.
        constexpr std::uint32_t k_max_str_len = 64u * 1024u * 1024u; // 64 MiB
        std::uint64_t total_str_bytes = 0;
        std::vector<char8_t> str_buf;
        std::vector<std::u8string> oris;
        for (size_t i = 0; i < str_num; ++i)
        {
            auto length = read_num(buff.data(), need_swap);
            auto offset = read_num(buff.data(), need_swap);
            auto cur_pos = std::ftell(fp);
            if (cur_pos == -1L) throw stream_error("get_translate_dictionary fail: file inconsistent");

            if (length > k_max_str_len)
                throw stream_error("get_translate_dictionary fail: string too long");
            total_str_bytes += length;
            if (total_str_bytes > static_cast<std::uint64_t>(file_size))
                throw stream_error("get_translate_dictionary fail: total string bytes exceed file size");
            if (str_buf.size() < static_cast<size_t>(length) + 1)
                str_buf.resize(static_cast<size_t>(length) + 1);

            if (std::fseek(fp, offset, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");

            if (std::fread(str_buf.data(), 1, length, fp) != length)
                throw stream_error("get_translate_dictionary fail: invalid format");

            str_buf[length] = u8'\0';
            oris.emplace_back(str_buf.data());

            if (std::fseek(fp, cur_pos, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");
        }

        if (std::fseek(fp, aim_offset, SEEK_SET) != 0)
            throw stream_error("get_translate_dictionary fail: invalid format");
        std::unordered_map<std::u8string, std::u8string> res;

        for (size_t i = 0; i < str_num; ++i)
        {
            auto length = read_num(buff.data(), need_swap);
            auto offset = read_num(buff.data(), need_swap);
            auto cur_pos = std::ftell(fp);
            if (cur_pos == -1L) throw stream_error("get_translate_dictionary fail: file inconsistent");

            if (length > k_max_str_len)
                throw stream_error("get_translate_dictionary fail: string too long");
            total_str_bytes += length;
            if (total_str_bytes > static_cast<std::uint64_t>(file_size))
                throw stream_error("get_translate_dictionary fail: total string bytes exceed file size");
            if (str_buf.size() < static_cast<size_t>(length) + 1)
                str_buf.resize(static_cast<size_t>(length) + 1);

            if (std::fseek(fp, offset, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");

            if (std::fread(str_buf.data(), 1, length, fp) != length)
                throw stream_error("get_translate_dictionary fail: invalid format");

            str_buf[length] = u8'\0';
            res.insert({oris[i], str_buf.data()});

            if (std::fseek(fp, cur_pos, SEEK_SET) != 0)
                throw stream_error("get_translate_dictionary fail: invalid format");
        }

        return res;
    }

private:
    /**
     * @lang{ZH}
     * @brief `available` 的内部重载，接受已快照的 `text_domain`。
     *
     * 调用者负责在外部解析目录名，此处不再访问 `s_domain_dirs`，
     * 从而避免在语言扫描时重复加锁或递归进入公开重载的 `available()`。
     *
     * @param td 包含已快照目录名和域名的 `text_domain` 结构体。
     * @param lang 语言标识符。
     * @return 如果对应的 `.mo` 文件存在，则返回 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Internal overload of `available` that accepts an already-snapshotted `text_domain`.
     *
     * The caller resolves the dirname before calling here, so `s_domain_dirs`
     * is not accessed, avoiding repeated locking or recursion back into the
     * public overload of `available()` during the language scan.
     *
     * @param td A `text_domain` struct with an already-snapshotted dirname and domain name.
     * @param lang The language identifier.
     * @return `true` if the corresponding `.mo` file exists.
     * @endif
     */
    static bool available(const text_domain& td, const std::string& lang)
    {
        if (lang.empty() || lang.find(':') != std::string::npos)
        {
            std::string resolved = filter_lang(td, lang);
            if (resolved.empty())
                return false;
            return std::filesystem::exists(get_domain_file(td, resolved));
        }
        return std::filesystem::exists(get_domain_file(td, lang));
    }

    /**
     * @lang{ZH}
     * @brief `filter_lang` 的内部重载，接受已快照的 `text_domain`。
     *
     * 如果 `p_lang` 非空，则在 `:` 分隔的候选列表中找出第一个可用的语言。
     * 如果 `p_lang` 为空，则依次查询环境变量 `LANGUAGE`（按 `:` 分隔），
     * 然后是 `LC_ALL`、`LC_MESSAGES`、`LANG`（均视为单一语言名称，镜像 gettext 的行为），
     * 返回第一个可用的语言字符串。
     *
     * @note `std::getenv` 在与 `setenv()`/`putenv()` 并发执行时不是线程安全的——
     *       无论是返回的指针还是其所指向的字节，都可能在读取过程中被另一线程置为无效——
     *       而 C++ 和 POSIX 均不提供可移植的线程安全环境变量读取方式，
     *       因此此处的锁无法关闭与其他修改线程之间的竞争。
     *       本代码假定进程环境在 `messages` facet 构造期间不会被并发修改
     *       （对于"在启动时配置 locale 一次"的常见用法，此假定成立）。
     *       每个值在读取后立即复制到 `std::string` 中，以将读取窗口降至最低。
     *
     * @param td 包含已快照目录名和域名的 `text_domain` 结构体。
     * @param p_lang 候选语言字符串，或空字符串（使用环境变量）。
     * @return 第一个可用的语言字符串，若均不可用则返回空字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Internal overload of `filter_lang` that accepts an already-snapshotted `text_domain`.
     *
     * If `p_lang` is non-empty, scans it as a `:` -separated candidate list and
     * returns the first available language. If `p_lang` is empty, queries the
     * environment variables `LANGUAGE` (`:` -separated list), then `LC_ALL`,
     * `LC_MESSAGES`, and `LANG` (each treated as a single locale name, mirroring
     * gettext), and returns the first available one.
     *
     * @note `std::getenv` is not thread-safe against concurrent `setenv()`/`putenv()` calls —
     *       the returned pointer and the bytes it addresses may be invalidated mid-read —
     *       and neither C++ nor POSIX offers a portable thread-safe environment read,
     *       so a lock here could not close the race against mutators elsewhere.
     *       This code therefore assumes the process environment is not modified
     *       concurrently during `messages` facet construction (true for the typical
     *       "configure locale once at startup" usage). Each value is copied into a
     *       `std::string` at once to keep the read window minimal.
     *
     * @param td A `text_domain` struct with an already-snapshotted dirname and domain name.
     * @param p_lang The candidate language string, or empty (use environment variables).
     * @return The first available language string, or empty if none is available.
     * @endif
     */
    static std::string filter_lang(const text_domain& td, const std::string& p_lang)
    {
        auto match_lang = [&td](const std::string& str_lang)
        {
            std::size_t start = 0;
            std::size_t end = 0;
            while ((end = str_lang.find(':', start)) != std::string::npos)
            {
                auto cur_lang = str_lang.substr(start, end - start);
                if ((!cur_lang.empty()) && (base_ft<messages>::available(td, cur_lang)))
                    return cur_lang;
                start = end + 1;
            }

            auto last_str = str_lang.substr(start);
            if ((!last_str.empty()) && (base_ft<messages>::available(td, last_str)))
                return last_str;

            return std::string{};
        };

        if (p_lang.empty())
        {
            // Environment fallback (mirrors gettext): LANGUAGE, then
            // LC_ALL / LC_MESSAGES / LANG. std::getenv is NOT synchronised
            // against a concurrent setenv()/putenv() on another thread — the
            // returned pointer and the bytes it addresses may be invalidated
            // mid-read — and neither C++ nor POSIX offers a portable
            // thread-safe environment read, so a lock here could not close the
            // race against mutators elsewhere. This code therefore assumes the
            // process environment is not modified concurrently with messages
            // facet construction (true for the usual "configure locale once at
            // startup" usage). Each value is copied into a std::string at once
            // to keep the read window minimal.
            std::string res;
            if (const char* p = std::getenv("LANGUAGE"))
            {
                res = match_lang(p);
                if (!res.empty()) return res;
            }

            for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"})
            {
                if (const char* p = std::getenv(var); p && p[0] != '\0')
                {
                    res = p;
                    // Unlike LANGUAGE, these are a single locale name, not a
                    // ':'-separated list (gettext never splits them). A value
                    // containing ':' is therefore not a valid locale name:
                    // skip it and try the next variable.
                    if (res.find(':') == std::string::npos && base_ft<messages>::available(td, res))
                        return res;
                }
            }

            return "";
        }
        else
            return match_lang(p_lang);
    }

    // Global mapping from text domain name to its directory path.
    // Protected by s_domain_mutex; read/write only while holding the lock.
    inline static std::unordered_map<std::string, std::string> s_domain_dirs;
    inline static std::mutex s_domain_mutex;

    /**
     * @lang{ZH}
     * @brief 线程安全地查询指定域绑定的目录名。
     *
     * 如果域未绑定，则返回默认目录 `/usr/share/locale`。
     *
     * @param domain 文本域名称。
     * @return 该域绑定的目录路径，或默认目录。
     * @endif
     *
     * @lang{EN}
     * @brief Thread-safely looks up the dirname bound to the given domain.
     *
     * Returns the default directory `/usr/share/locale` if the domain has
     * not been bound.
     *
     * @param domain The text domain name.
     * @return The directory path bound to the domain, or the default directory.
     * @endif
     */
    static std::string get_dirname(const std::string& domain)
    {
        const static std::string def_dir = "/usr/share/locale";

        std::scoped_lock guard(s_domain_mutex);
        auto it = s_domain_dirs.find(domain);
        return (it == s_domain_dirs.end()) ? def_dir : it->second;
    }

private:
    // Declared first: initialised before m_filtered_lang and m_domain_info (both
    // of which depend on this snapshot) because members are initialised in
    // declaration order. See the constructor for the full rationale.
    const std::string m_dirname;
    const std::string m_filtered_lang;
    const std::string m_domain_info;
};

template <typename CharT> class messages_conf;

/**
 * @lang{ZH}
 * @brief `messages_conf` 的 `char8_t` 特化，以 UTF-8 字符串为键的翻译 facet。
 *
 * 从 `.mo` 文件加载翻译字典，直接使用 `char8_t` 字符串作为键和值，
 * 无需任何编码转换。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `messages_conf` for `char8_t`, a translation facet
 *        keyed on UTF-8 strings.
 *
 * Loads the translation dictionary from a `.mo` file using `char8_t` strings
 * directly as keys and values, with no encoding conversion.
 * @endif
 */
template <>
class messages_conf<char8_t> : public ft_basic<messages<char8_t>>
{
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，加载指定域和语言的翻译字典。
     *
     * @param domain 文本域名称。
     * @param lang 候选语言字符串，或空字符串（使用环境变量）。
     * @param throw_if_fail 如果为 `true`（默认），加载失败时抛出异常；
     *                      否则静默地使翻译字典为空。
     * @throw stream_error 如果 `throw_if_fail` 为 `true` 且字典加载失败。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads the translation dictionary for the given domain and language.
     *
     * @param domain The text domain name.
     * @param lang The candidate language string, or empty (use environment variables).
     * @param throw_if_fail If `true` (default), throws on failure; otherwise silently
     *                      leaves the dictionary empty.
     * @throw stream_error If `throw_if_fail` is `true` and dictionary loading fails.
     * @endif
     */
    messages_conf(const std::string& domain, const std::string& lang, bool throw_if_fail = true)
        : ft_basic<messages<char8_t>>(domain, lang)
        , m_dict(init(this->dirname(), domain, this->filtered_lang(), throw_if_fail))
    {}

    /**
     * @lang{ZH}
     * @brief 查找 `ori` 的翻译。
     *
     * 返回指向字典中翻译的指针，而非翻译字符串本身，以避免与调用者参数产生别名
     * 关系（不会悬空，也不会产生额外拷贝）。`messages` facet 在此之上处理 gettext
     * 的"未命中时返回原文"语义（按值类别安全地进行）。`nullptr` 明确表示"未找到"，
     * 与找到了但翻译恰好为空的情况不同。
     *
     * @param ori 原始字符串（`msgid`）。
     * @return 指向字典中翻译字符串的指针，若未找到则为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Looks up the translation for `ori`.
     *
     * Returns a pointer to the translation rather than the string by value, so
     * it never aliases the caller's argument (no dangling, no copy). The
     * `messages` facet layers the gettext pass-through (return the original on
     * a miss) on top, where it can do so safely per value category.
     * `nullptr` unambiguously means "not found", distinct from a found
     * translation that happens to be empty.
     *
     * @param ori The original string (`msgid`).
     * @return A pointer to the translation in the dictionary, or `nullptr` if not found.
     * @endif
     */
    virtual const std::u8string* translate(const std::u8string& ori) const
    {
        auto it = m_dict.find(ori);
        return it != m_dict.end() ? &it->second : nullptr;
    }

private:
    /**
     * @lang{ZH}
     * @brief 加载翻译字典的私有工厂函数。
     *
     * @param dirname 域目录名。
     * @param domain 文本域名称。
     * @param lang 过滤后的语言字符串。
     * @param throw_if_fail 失败时是否抛出异常。
     * @return 已加载的翻译字典；若 `throw_if_fail` 为 `false` 且失败，则返回空字典。
     * @endif
     *
     * @lang{EN}
     * @brief Private factory function that loads the translation dictionary.
     *
     * @param dirname The domain directory name.
     * @param domain The text domain name.
     * @param lang The filtered language string.
     * @param throw_if_fail Whether to throw on failure.
     * @return The loaded translation dictionary, or an empty dictionary on failure
     *         (if `throw_if_fail` is `false`).
     * @endif
     */
    static std::unordered_map<std::u8string, std::u8string> init(const std::string& dirname, const std::string& domain, const std::string& lang, bool throw_if_fail)
    {
        try
        {
            if (lang.empty())
                throw stream_error("messages_conf init fail: no language available");
            else
                return get_translate_dictionary(get_domain_file({.dirname = dirname, .domain = domain}, lang));
        }
        catch(...)
        {
            if (throw_if_fail)
                throw;
        }
        return std::unordered_map<std::u8string, std::u8string>{};
    }

private:
    const std::unordered_map<std::u8string, std::u8string> m_dict;
};

/**
 * @lang{ZH}
 * @brief `messages_conf` 的 `char32_t`/UTF-32 `wchar_t` 特化。
 *
 * 从 `.mo` 文件加载 UTF-8 翻译字典，再将所有字符串转换为 `CharT` 类型。
 * 仅当 `CharT` 为 `char32_t`，或 `wchar_t` 且其编码为 UTF-32 时启用此特化。
 *
 * @tparam CharT 字符类型，必须为 `char32_t` 或 UTF-32 的 `wchar_t`。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `messages_conf` for `char32_t` / UTF-32 `wchar_t`.
 *
 * Loads the UTF-8 translation dictionary from a `.mo` file and converts all
 * strings to `CharT`. This specialization is enabled only when `CharT` is
 * `char32_t`, or `wchar_t` with UTF-32 encoding.
 *
 * @tparam CharT The character type; must be `char32_t` or UTF-32 `wchar_t`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, char32_t> ||
                (std::is_same_v<CharT, wchar_t> &&
                 wchar_t_is_utf32)
class messages_conf<CharT> : public ft_basic<messages<CharT>>
{
    using TString = std::basic_string<CharT>;
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，加载指定域和语言的翻译字典。
     *
     * @param domain 文本域名称。
     * @param lang 候选语言字符串，或空字符串（使用环境变量）。
     * @param throw_if_fail 如果为 `true`（默认），加载失败时抛出异常；
     *                      否则静默地使翻译字典为空。
     * @throw stream_error 如果 `throw_if_fail` 为 `true` 且字典加载失败。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads the translation dictionary for the given domain and language.
     *
     * @param domain The text domain name.
     * @param lang The candidate language string, or empty (use environment variables).
     * @param throw_if_fail If `true` (default), throws on failure; otherwise silently
     *                      leaves the dictionary empty.
     * @throw stream_error If `throw_if_fail` is `true` and dictionary loading fails.
     * @endif
     */
    messages_conf(const std::string& domain, const std::string& lang, bool throw_if_fail = true)
        : ft_basic<messages<CharT>>(domain, lang)
        , m_dict(init(this->dirname(), domain, this->filtered_lang(), throw_if_fail))
    {}

private:
    /**
     * @lang{ZH}
     * @brief 加载翻译字典并将 UTF-8 字符串转换为 `CharT` 编码的私有工厂函数。
     *
     * @param dirname 域目录名。
     * @param domain 文本域名称。
     * @param lang 过滤后的语言字符串。
     * @param throw_if_fail 失败时是否抛出异常。
     * @return 已加载并转换的翻译字典；若 `throw_if_fail` 为 `false` 且失败，则返回空字典。
     * @endif
     *
     * @lang{EN}
     * @brief Private factory function that loads the translation dictionary and
     *        converts UTF-8 strings to `CharT`.
     *
     * @param dirname The domain directory name.
     * @param domain The text domain name.
     * @param lang The filtered language string.
     * @param throw_if_fail Whether to throw on failure.
     * @return The loaded and converted translation dictionary, or an empty dictionary
     *         on failure (if `throw_if_fail` is `false`).
     * @endif
     */
    static std::unordered_map<TString, TString> init(const std::string& dirname, const std::string& domain, const std::string& lang, bool throw_if_fail)
    {
        try
        {
            if (lang.empty())
                throw stream_error("messages_conf init fail: no language available");

            std::unordered_map<TString, TString> res;
            auto tmp_dict = base_ft<messages>::get_translate_dictionary(base_ft<messages>::get_domain_file({.dirname = dirname, .domain = domain}, lang));
            for (const auto& [k, v] : tmp_dict)
            {
                const std::u32string uk = detail::to_u32string(k.c_str());
                const std::u32string uv = detail::to_u32string(v.c_str());
                res.insert({TString(uk.begin(), uk.end()),
                            TString(uv.begin(), uv.end())});
            }
            return res;
        }
        catch(...)
        {
            if (throw_if_fail)
                throw;
        }
        return std::unordered_map<TString, TString>{};
    }

public:
    /**
     * @lang{ZH}
     * @brief 查找 `ori` 的翻译。参见 `char8_t` 特化：`nullptr` 表示"未找到"。
     *
     * @param ori 原始字符串（`msgid`）。
     * @return 指向字典中翻译字符串的指针，若未找到则为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Looks up the translation for `ori`. See the `char8_t` specialization:
     *        `nullptr` means "not found".
     *
     * @param ori The original string (`msgid`).
     * @return A pointer to the translation in the dictionary, or `nullptr` if not found.
     * @endif
     */
    virtual const TString* translate(const TString& ori) const
    {
        auto it = m_dict.find(ori);
        return it != m_dict.end() ? &it->second : nullptr;
    }

private:
    const std::unordered_map<TString, TString> m_dict; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

/**
 * @lang{ZH}
 * @brief `messages_conf` 的 `char` 特化，通过 `wchar_t` 转换器进行编码转换。
 *
 * 从 `.mo` 文件加载 UTF-8 翻译字典，再通过指定的 `char`↔`wchar_t` 转换 facet
 * 将字符串转换为当前 `char` 编码。仅在 `wchar_t` 为 UTF-32 的平台上支持。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `messages_conf` for `char`, with encoding conversion
 *        via a `wchar_t` converter.
 *
 * Loads the UTF-8 translation dictionary from a `.mo` file, then converts
 * strings to the current `char` encoding using the specified `char`↔`wchar_t`
 * converter facet. Only supported on platforms where `wchar_t` is UTF-32.
 * @endif
 */
template <>
class messages_conf<char> : public ft_basic<messages<char>>
{
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，加载翻译字典并进行编码转换。
     *
     * 与其他特化相比，此特化需要额外的 `cvt_ft` 参数来指定
     * `char`↔`wchar_t` 转换 facet 的名称。这是公开、稳定的构造 API，
     * 因此不能将参数打包到结构体中，否则会破坏调用者，或与兄弟特化的
     * `(domain, lang)` 参数形式产生不一致。
     *
     * @param domain 文本域名称。
     * @param lang 候选语言字符串，或空字符串（使用环境变量）。
     * @param cvt_ft `char`↔`wchar_t` 转换 facet 的名称。
     * @param throw_if_fail 如果为 `true`（默认），加载失败时抛出异常；
     *                      否则静默地使翻译字典为空。
     * @throw stream_error 如果 `throw_if_fail` 为 `true` 且字典加载失败。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads the translation dictionary with encoding conversion.
     *
     * Unlike other specializations, this one requires an extra `cvt_ft` parameter
     * naming the `char`↔`wchar_t` converter facet. This is the public, stable
     * construction API, so the arguments cannot be bundled into a struct without
     * breaking callers or diverging from the sibling specializations'
     * `(domain, lang)` argument shape.
     *
     * @param domain The text domain name.
     * @param lang The candidate language string, or empty (use environment variables).
     * @param cvt_ft The name of the `char`↔`wchar_t` converter facet.
     * @param throw_if_fail If `true` (default), throws on failure; otherwise silently
     *                      leaves the dictionary empty.
     * @throw stream_error If `throw_if_fail` is `true` and dictionary loading fails.
     * @endif
     */
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    messages_conf(const std::string& domain, const std::string& lang, const std::string& cvt_ft, bool throw_if_fail = true)
        : ft_basic<messages<char>>(domain, lang)
        , m_dict(init(this->dirname(), domain, this->filtered_lang(), cvt_ft, throw_if_fail))
    {}

    /**
     * @lang{ZH}
     * @brief 查找 `ori` 的翻译。参见 `char8_t` 特化：`nullptr` 表示"未找到"。
     *
     * @param ori 原始字符串（`msgid`）。
     * @return 指向字典中翻译字符串的指针，若未找到则为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Looks up the translation for `ori`. See the `char8_t` specialization:
     *        `nullptr` means "not found".
     *
     * @param ori The original string (`msgid`).
     * @return A pointer to the translation in the dictionary, or `nullptr` if not found.
     * @endif
     */
    virtual const std::string* translate(const std::string& ori) const
    {
        auto it = m_dict.find(ori);
        return it != m_dict.end() ? &it->second : nullptr;
    }

private:
    /**
     * @lang{ZH}
     * @brief 加载翻译字典并将 UTF-8 字符串转换为 `char` 编码的私有工厂函数。
     *
     * 参数形式与公开构造函数一致：`dirname`/`domain` 仅在调用 `get_domain_file`
     * 时才打包为 `text_domain`；`lang` 和 `cvt_ft` 保持独立字符串，
     * 这是 `char` 专属 API 的固有参数形式。
     *
     * @param dirname 域目录名。
     * @param domain 文本域名称。
     * @param lang 过滤后的语言字符串。
     * @param cvt_ft `char`↔`wchar_t` 转换 facet 的名称。
     * @param throw_if_fail 失败时是否抛出异常。
     * @return 已加载并转换的翻译字典；若 `throw_if_fail` 为 `false` 且失败，则返回空字典。
     * @endif
     *
     * @lang{EN}
     * @brief Private factory function that loads the translation dictionary and
     *        converts UTF-8 strings to `char` encoding.
     *
     * The argument shape mirrors the public constructor: `dirname`/`domain` are
     * bundled into a `text_domain` only where `get_domain_file` is called;
     * `lang` and `cvt_ft` remain distinct strings inherent to this `char`-only API.
     *
     * @param dirname The domain directory name.
     * @param domain The text domain name.
     * @param lang The filtered language string.
     * @param cvt_ft The name of the `char`↔`wchar_t` converter facet.
     * @param throw_if_fail Whether to throw on failure.
     * @return The loaded and converted translation dictionary, or an empty dictionary
     *         on failure (if `throw_if_fail` is `false`).
     * @endif
     */
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    static std::unordered_map<std::string, std::string> init(const std::string& dirname, const std::string& domain, const std::string& lang, const std::string& cvt_ft, bool throw_if_fail)
    {
        try
        {
            if constexpr (wchar_t_is_utf32)
            {
                if (lang.empty())
                    throw stream_error("messages_conf init fail: no language available");

                std::unordered_map<std::string, std::string> res;
                auto tmp_dict = get_translate_dictionary(get_domain_file({.dirname = dirname, .domain = domain}, lang));

                auto cvt = IOv2::code_cvt_creator<char, wchar_t>(cvt_ft).create(rb_root_cvt{mem_device{""}});

                for (const auto& [k, v] : tmp_dict)
                {
                    const std::u32string uk = detail::to_u32string(k.c_str());
                    const std::u32string uv = detail::to_u32string(v.c_str());
                    std::wstring wk(uk.begin(), uk.end());
                    std::wstring wv(uv.begin(), uv.end());

                    cvt.bos(); cvt.main_cont_beg();
                    cvt.put(wk.data(), wk.size());
                    auto [dev_k, err_k] = cvt.detach();
                    cvt.attach(mem_device{""});
                    if (err_k) std::rethrow_exception(err_k);
                    std::string ck = dev_k.str();

                    cvt.bos(); cvt.main_cont_beg();
                    cvt.put(wv.data(), wv.size());
                    auto [dev_v, err_v] = cvt.detach();
                    cvt.attach(mem_device{""});
                    if (err_v) std::rethrow_exception(err_v);
                    std::string cv = dev_v.str();

                    res.insert({std::move(ck), std::move(cv)});
                }
                return res;
            }
            else
                throw stream_error("messages_conf init error: unsupported wchar_t / char32_t");
        }
        catch(...)
        {
            if (throw_if_fail)
                throw;
        }
        return std::unordered_map<std::string, std::string>{};
    }

private:
    const std::unordered_map<std::string, std::string> m_dict;
};
}

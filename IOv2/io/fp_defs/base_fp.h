#pragma once

namespace IOv2
{
template <typename TChar, typename T>
struct writer;

template <typename TChar, typename T>
struct parse_context_type
{
    using type = T;
};

template <typename TChar, typename T>
struct reader;

template <typename TChar, typename T>
concept is_writer_def = requires(TChar, T)
{
    sizeof(writer<TChar, T>);
};

template <typename TChar, typename T>
concept is_reader_def = requires(TChar, T)
{
    sizeof(reader<TChar, T>);
};
}
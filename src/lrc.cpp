#include "lrc.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

auto trim_ascii(std::string_view text) -> std::string_view
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }

    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }

    return text;
}

auto starts_with_utf8_bom(const std::string &text) -> bool
{
    return text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF;
}

auto starts_with_utf16le_bom(const std::string &text) -> bool
{
    return text.size() >= 2
        && static_cast<unsigned char>(text[0]) == 0xFF
        && static_cast<unsigned char>(text[1]) == 0xFE;
}

auto starts_with_utf16be_bom(const std::string &text) -> bool
{
    return text.size() >= 2
        && static_cast<unsigned char>(text[0]) == 0xFE
        && static_cast<unsigned char>(text[1]) == 0xFF;
}

#ifdef _WIN32
auto utf8_from_wide(const std::wstring &wide) -> std::string
{
    if (wide.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

auto decode_bytes_with_code_page(const std::string &bytes, unsigned int code_page, DWORD flags)
    -> std::optional<std::string>
{
    if (bytes.empty()) {
        return std::string();
    }

    const int wide_size = MultiByteToWideChar(
        code_page, flags, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (wide_size <= 0) {
        return std::nullopt;
    }

    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    if (MultiByteToWideChar(
            code_page,
            flags,
            bytes.data(),
            static_cast<int>(bytes.size()),
            wide.data(),
            wide_size)
        <= 0) {
        return std::nullopt;
    }

    return utf8_from_wide(wide);
}

auto utf8_from_utf16_bytes(const std::string &bytes, bool little_endian) -> std::string
{
    std::wstring wide;
    wide.reserve((bytes.size() - 2) / 2);

    for (std::size_t index = 2; index + 1 < bytes.size(); index += 2) {
        const auto first = static_cast<unsigned char>(bytes[index]);
        const auto second = static_cast<unsigned char>(bytes[index + 1]);
        const wchar_t code_unit = little_endian
            ? static_cast<wchar_t>(static_cast<unsigned int>(first) | (static_cast<unsigned int>(second) << 8U))
            : static_cast<wchar_t>(static_cast<unsigned int>(second) | (static_cast<unsigned int>(first) << 8U));
        wide.push_back(code_unit);
    }

    return utf8_from_wide(wide);
}
#endif

auto decode_text_file_bytes(const std::string &bytes) -> std::string
{
    if (bytes.empty()) {
        return {};
    }

    if (starts_with_utf8_bom(bytes)) {
        return bytes.substr(3);
    }

#ifdef _WIN32
    if (starts_with_utf16le_bom(bytes)) {
        return utf8_from_utf16_bytes(bytes, true);
    }

    if (starts_with_utf16be_bom(bytes)) {
        return utf8_from_utf16_bytes(bytes, false);
    }

    if (const auto utf8 = decode_bytes_with_code_page(bytes, CP_UTF8, MB_ERR_INVALID_CHARS)) {
        return *utf8;
    }

    if (const auto local = decode_bytes_with_code_page(bytes, CP_ACP, 0)) {
        return *local;
    }
#endif

    return bytes;
}

auto parse_int(std::string_view text) -> std::optional<int>
{
    int value = 0;
    const auto *begin = text.data();
    const auto *end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

auto parse_fraction_ms(std::string_view text) -> std::optional<int>
{
    if (text.empty() || text.size() > 3) {
        return std::nullopt;
    }

    const auto value = parse_int(text);
    if (!value.has_value()) {
        return std::nullopt;
    }

    if (text.size() == 1) {
        return *value * 100;
    }
    if (text.size() == 2) {
        return *value * 10;
    }
    return *value;
}

auto parse_timestamp_ms(std::string_view token) -> std::optional<int>
{
    const auto colon = token.find(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }

    const auto dot = token.find('.', colon + 1);
    const std::string_view minutes_text = token.substr(0, colon);
    const std::string_view seconds_text =
        dot == std::string_view::npos ? token.substr(colon + 1) : token.substr(colon + 1, dot - colon - 1);
    const std::string_view fraction_text =
        dot == std::string_view::npos ? std::string_view() : token.substr(dot + 1);

    const auto minutes = parse_int(minutes_text);
    const auto seconds = parse_int(seconds_text);
    if (!minutes.has_value() || !seconds.has_value() || *seconds < 0 || *seconds >= 60) {
        return std::nullopt;
    }

    int milliseconds = 0;
    if (!fraction_text.empty()) {
        const auto fraction = parse_fraction_ms(fraction_text);
        if (!fraction.has_value()) {
            return std::nullopt;
        }
        milliseconds = *fraction;
    }

    return (*minutes * 60 + *seconds) * 1000 + milliseconds;
}

auto parse_metadata_line(LrcDocument &document, std::string_view key, std::string_view value) -> bool
{
    const auto trimmed_value = std::string(trim_ascii(value));

    if (key == "ti") {
        document.title = trimmed_value;
        return true;
    }
    if (key == "ar") {
        document.artist = trimmed_value;
        return true;
    }
    if (key == "al") {
        document.album = trimmed_value;
        return true;
    }
    if (key == "offset") {
        if (const auto offset = parse_int(trim_ascii(value))) {
            document.offset_ms = *offset;
        }
        return true;
    }

    return false;
}

auto apply_offset_and_sort(LrcDocument &document) -> void
{
    for (auto &line : document.lines) {
        line.time_ms = max(0, line.time_ms + document.offset_ms);
        line.text = std::string(trim_ascii(line.text));
    }

    document.lines.erase(
        std::remove_if(document.lines.begin(), document.lines.end(), [](const LrcLine &line) {
            return line.text.empty();
        }),
        document.lines.end());

    std::sort(document.lines.begin(), document.lines.end(), [](const LrcLine &lhs, const LrcLine &rhs) {
        if (lhs.time_ms != rhs.time_ms) {
            return lhs.time_ms < rhs.time_ms;
        }
        return lhs.text < rhs.text;
    });
}

} // namespace

auto find_lrc_for_track(const std::filesystem::path &track_path) -> std::optional<std::filesystem::path>
{
    if (track_path.empty()) {
        return std::nullopt;
    }

    const auto direct_match = track_path.parent_path() / (track_path.stem().native() + std::filesystem::path(".lrc").native());
    if (std::filesystem::exists(direct_match) && std::filesystem::is_regular_file(direct_match)) {
        return direct_match;
    }

    return std::nullopt;
}

auto load_lrc_file(const std::filesystem::path &path) -> std::optional<LrcDocument>
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }

    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const std::string content = decode_text_file_bytes(bytes);
    if (content.empty()) {
        return LrcDocument {};
    }

    LrcDocument document;
    std::size_t cursor = 0;

    while (cursor <= content.size()) {
        const auto line_end = content.find('\n', cursor);
        const auto raw_line = content.substr(
            cursor,
            line_end == std::string::npos ? std::string::npos : line_end - cursor);
        std::string_view line = trim_ascii(raw_line);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
            line = trim_ascii(line);
        }

        if (!line.empty()) {
            std::vector<int> timestamps;
            std::size_t pos = 0;

            while (pos < line.size() && line[pos] == '[') {
                const auto closing = line.find(']', pos + 1);
                if (closing == std::string_view::npos) {
                    break;
                }

                const std::string_view token = line.substr(pos + 1, closing - pos - 1);
                const auto colon = token.find(':');

                if (colon != std::string_view::npos) {
                    const auto maybe_time = parse_timestamp_ms(token);
                    if (maybe_time.has_value()) {
                        timestamps.push_back(*maybe_time);
                    } else {
                        parse_metadata_line(document, token.substr(0, colon), token.substr(colon + 1));
                    }
                }

                pos = closing + 1;
            }

            if (!timestamps.empty()) {
                const std::string text = std::string(trim_ascii(line.substr(pos)));
                for (const int timestamp : timestamps) {
                    document.lines.push_back(LrcLine { timestamp, text });
                }
            }
        }

        if (line_end == std::string::npos) {
            break;
        }
        cursor = line_end + 1;
    }

    apply_offset_and_sort(document);
    return document;
}

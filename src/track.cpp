#include "track.h"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace {

auto to_lower_ascii(std::string value) -> std::string
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

auto fallback_title_from_path(const std::filesystem::path &path) -> std::string
{
    const auto stem = path.stem();
    return stem.empty() ? std::string("Unknown Track") : utf8_from_path(stem);
}

auto read_tag_string(const TagLib::String &value) -> std::string
{
    return value.to8Bit(true);
}

} // namespace

auto utf8_from_path(const std::filesystem::path &path) -> std::string
{
    const auto utf8 = path.u8string();
    return { reinterpret_cast<const char *>(utf8.c_str()), utf8.size() };
}

auto is_supported_audio_file(const std::filesystem::path &path) -> bool
{
    if (!path.has_extension()) {
        return false;
    }

    static constexpr std::array supported_extensions {
        ".mp3",
        ".flac",
        ".ogg",
        ".wav",
        ".aif",
        ".aiff",
    };

    const auto extension = to_lower_ascii(utf8_from_path(path.extension()));
    return std::find(supported_extensions.begin(), supported_extensions.end(), extension)
        != supported_extensions.end();
}

auto scan_audio_files(const std::filesystem::path &folder) -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> results;
    std::error_code error;

    if (!std::filesystem::exists(folder, error) || !std::filesystem::is_directory(folder, error)) {
        return results;
    }

    const auto options = std::filesystem::directory_options::skip_permission_denied;

    for (std::filesystem::recursive_directory_iterator it(folder, options, error), end; it != end;
         it.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }

        if (!it->is_regular_file(error) || error) {
            error.clear();
            continue;
        }

        if (is_supported_audio_file(it->path())) {
            results.push_back(it->path());
        }
    }

    std::sort(results.begin(), results.end(), [](const auto &lhs, const auto &rhs) {
        return to_lower_ascii(utf8_from_path(lhs.filename()))
            < to_lower_ascii(utf8_from_path(rhs.filename()));
    });

    return results;
}

auto load_track_metadata(const std::filesystem::path &path) -> Track
{
    Track track;
    track.path = path;
    track.title = fallback_title_from_path(path);
    track.artist = "Unknown Artist";
    track.album.clear();
    track.extension = path.has_extension() ? utf8_from_path(path.extension()).substr(1) : "audio";

    TagLib::FileRef file_ref(path.c_str());
    if (file_ref.isNull()) {
        return track;
    }

    if (const auto *tag = file_ref.tag()) {
        const auto title = read_tag_string(tag->title());
        const auto artist = read_tag_string(tag->artist());
        const auto album = read_tag_string(tag->album());

        if (!title.empty()) {
            track.title = title;
        }
        if (!artist.empty()) {
            track.artist = artist;
        }
        if (!album.empty()) {
            track.album = album;
        }
    }

    if (const auto *properties = file_ref.audioProperties()) {
        track.duration_seconds = properties->lengthInSeconds();
        track.bitrate_kbps = properties->bitrate();
        track.sample_rate_hz = properties->sampleRate();
        track.channels = properties->channels();
    }

    return track;
}

auto format_duration(int total_seconds) -> std::string
{
    if (total_seconds < 0) {
        total_seconds = 0;
    }

    const int hours = total_seconds / 3600;
    const int minutes = (total_seconds % 3600) / 60;
    const int seconds = total_seconds % 60;

    std::ostringstream stream;
    stream << std::setfill('0');

    if (hours > 0) {
        stream << hours << ':' << std::setw(2) << minutes << ':' << std::setw(2) << seconds;
        return stream.str();
    }

    stream << std::setw(2) << minutes << ':' << std::setw(2) << seconds;
    return stream.str();
}

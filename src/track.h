#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class PlaybackMode {
    Sequence = 0,
    Shuffle = 1,
    RepeatOne = 2,
};

struct Track {
    std::filesystem::path path;
    std::string title;
    std::string artist;
    std::string album;
    std::string extension;
    int duration_seconds = 0;
    int bitrate_kbps = 0;
    int sample_rate_hz = 0;
    int channels = 0;
};

auto is_supported_audio_file(const std::filesystem::path &path) -> bool;
auto scan_audio_files(const std::filesystem::path &folder) -> std::vector<std::filesystem::path>;
auto load_track_metadata(const std::filesystem::path &path) -> Track;
auto format_duration(int total_seconds) -> std::string;
auto utf8_from_path(const std::filesystem::path &path) -> std::string;

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct LrcLine {
    int time_ms = 0;
    std::string text;
};

struct LrcDocument {
    std::string title;
    std::string artist;
    std::string album;
    int offset_ms = 0;
    std::vector<LrcLine> lines;

    auto empty() const -> bool
    {
        return lines.empty();
    }
};

auto find_lrc_for_track(const std::filesystem::path &track_path) -> std::optional<std::filesystem::path>;
auto load_lrc_file(const std::filesystem::path &path) -> std::optional<LrcDocument>;

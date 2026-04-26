#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace platform_dialogs {

auto pick_audio_files() -> std::vector<std::filesystem::path>;
auto pick_folder() -> std::optional<std::filesystem::path>;

} // namespace platform_dialogs

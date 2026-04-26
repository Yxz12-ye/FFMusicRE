#pragma once

#include <filesystem>

namespace sf {
class Music;
}

class AudioPlayer {
public:
    enum class Status {
        Stopped,
        Paused,
        Playing,
    };

    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer &) = delete;
    auto operator=(const AudioPlayer &) -> AudioPlayer & = delete;

    auto open(const std::filesystem::path &path, bool autoplay) -> bool;
    auto has_track() const -> bool;
    auto clear() -> void;
    auto play() -> void;
    auto pause() -> void;
    auto toggle() -> void;
    auto seek_to_ratio(float ratio) -> void;
    auto seek_to_seconds(float seconds) -> void;
    auto set_volume(float normalized_volume) -> void;
    auto volume() const -> float;
    auto set_looping(bool looping) -> void;
    auto progress() const -> float;
    auto position_seconds() const -> float;
    auto duration_seconds() const -> float;
    auto status() const -> Status;

private:
    class Impl;
    Impl *impl_;
};

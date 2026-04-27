#pragma once

#include "audio_player.h"
#include "lrc.h"
#include "track.h"

#include "app-window.h"

#include <slint.h>

#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

class AppController {
public:
    explicit AppController(const slint::ComponentHandle<AppWindow> &window);

    auto initialize() -> void;

private:
    auto bind_callbacks() -> void;
    auto start_ui_timer() -> void;
    auto load_queue_from_paths(
        const std::vector<std::filesystem::path> &paths, const std::string &source_description)
        -> void;
    auto append_queue_from_paths(
        const std::vector<std::filesystem::path> &paths, const std::string &source_description)
        -> void;
    auto clear_queue(const std::string &message) -> void;
    auto open_track_at(std::size_t index, bool autoplay) -> bool;
    auto open_next_playable_from(std::size_t start_index, bool autoplay) -> bool;
    auto on_open_files_requested() -> void;
    auto on_clear_queue_requested() -> void;
    auto on_open_folder_requested() -> void;
    auto on_toggle_play_requested() -> void;
    auto on_previous_track_requested() -> void;
    auto on_next_track_requested() -> void;
    auto on_queue_item_selected(int index) -> void;
    auto on_seek_requested(float value) -> void;
    auto on_volume_requested(float value) -> void;
    auto on_playback_mode_selected(int index) -> void;
    auto on_toggle_queue_visibility_requested() -> void;
    auto on_toggle_sync_requested() -> void;
    auto on_ui_tick() -> void;
    auto handle_track_finished() -> void;
    auto select_track(std::size_t index, bool autoplay) -> void;
    auto sequence_next_index(bool wrap) const -> std::optional<std::size_t>;
    auto random_other_index() -> std::optional<std::size_t>;
    auto rebuild_queue_model() -> void;
    auto rebuild_playback_mode_model() -> void;
    auto rebuild_cover_tags() -> void;
    auto load_lyrics_for_current_track() -> void;
    auto rebuild_lyric_model() -> void;
    auto sync_lyrics_to_position() -> void;
    auto refresh_cover_image() -> void;
    auto refresh_now_playing() -> void;
    auto refresh_transport_state() -> void;
    auto refresh_queue_labels() -> void;
    auto set_status_message(const std::string &message) -> void;
    auto current_track() const -> const Track *;

    slint::ComponentHandle<AppWindow> window_;
    std::vector<Track> queue_;
    std::filesystem::path current_source_;
    std::string source_description_;
    std::size_t current_index_ = 0;
    PlaybackMode playback_mode_ = PlaybackMode::Sequence;
    bool queue_visible_ = true;
    bool sync_enabled_ = false;
    LrcDocument lyrics_;
    int active_lyric_index_ = -1;
    AudioPlayer player_;
    AudioPlayer::Status last_status_ = AudioPlayer::Status::Stopped;
    slint::Timer ui_timer_;
    std::mt19937 random_engine_;
};

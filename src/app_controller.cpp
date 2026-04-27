#include "app_controller.h"

#include "lrc.h"
#include "platform_dialogs.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>

namespace {

auto to_shared(const std::string &value) -> slint::SharedString
{
    return slint::SharedString(value);
}

auto uppercase_ascii(std::string value) -> std::string
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

auto track_artist_line(const Track &track) -> std::string
{
    if (!track.album.empty()) {
        return track.artist + " · " + track.album;
    }
    return track.artist;
}

auto track_format_label(const Track &track) -> std::string
{
    std::string label = uppercase_ascii(track.extension.empty() ? std::string("audio") : track.extension);
    if (track.bitrate_kbps > 0) {
        label += " " + std::to_string(track.bitrate_kbps) + " kbps";
    }
    return label;
}

auto track_meta_line(const Track &track) -> std::string
{
    std::vector<std::string> pieces;

    if (!track.album.empty()) {
        pieces.push_back(track.album);
    }
    if (track.sample_rate_hz > 0) {
        pieces.push_back(std::to_string(track.sample_rate_hz) + " Hz");
    }
    if (track.channels > 0) {
        pieces.push_back(std::to_string(track.channels) + " ch");
    }

    pieces.push_back("Local Library");

    std::ostringstream stream;
    for (std::size_t index = 0; index < pieces.size(); ++index) {
        if (index != 0) {
            stream << "  ·  ";
        }
        stream << pieces[index];
    }
    return stream.str();
}

auto describe_source(const std::filesystem::path &path, std::size_t count) -> std::string
{
    const auto name = path.empty() ? std::string("selection") : utf8_from_path(path.filename());
    const auto track_word = count == 1 ? "track" : "tracks";
    return "Loaded " + std::to_string(count) + " " + track_word + " from " + name + ".";
}

auto placeholder_lyrics(const Track *track) -> std::vector<LyricLineData>
{
    std::vector<LyricLineData> lyrics;
    lyrics.push_back(LyricLineData { to_shared("No synced LRC lyric found for the current track."), true, false });
    lyrics.push_back(LyricLineData {
        to_shared("Place a same-name .lrc file next to the audio file to enable sync."),
        false,
        false,
    });

    const std::string tail = track == nullptr
        ? "Open a file or folder to start."
        : "Current file: " + utf8_from_path(track->path.filename());
    lyrics.push_back(LyricLineData { to_shared(tail), false, true });
    return lyrics;
}

} // namespace

AppController::AppController(const slint::ComponentHandle<AppWindow> &window)
    : window_(window), random_engine_(std::random_device {}())
{
}

auto AppController::initialize() -> void
{
    bind_callbacks();
    rebuild_playback_mode_model();
    clear_queue("Choose a file or folder to start playback.");
    window_->set_volume(player_.volume());
    start_ui_timer();
}

auto AppController::bind_callbacks() -> void
{
    window_->on_open_file([this] { on_open_files_requested(); });
    window_->on_open_folder([this] { on_open_folder_requested(); });
    window_->on_toggle_play([this] { on_toggle_play_requested(); });
    window_->on_previous_track([this] { on_previous_track_requested(); });
    window_->on_next_track([this] { on_next_track_requested(); });
    window_->on_seek_requested([this](float value) { on_seek_requested(value); });
    window_->on_volume_requested([this](float value) { on_volume_requested(value); });
    window_->on_queue_item_selected([this](int index) { on_queue_item_selected(index); });
    window_->on_playback_mode_selected([this](int index) { on_playback_mode_selected(index); });
    window_->on_toggle_queue_visibility([this] { on_toggle_queue_visibility_requested(); });
    window_->on_toggle_sync([this] { on_toggle_sync_requested(); });
    window_->on_lyric_line_selected([this](int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= lyrics_.lines.size()) {
            return;
        }

        player_.seek_to_seconds(static_cast<float>(lyrics_.lines[static_cast<std::size_t>(index)].time_ms) / 1000.0f);
        sync_lyrics_to_position();
        refresh_transport_state();
    });
}

auto AppController::start_ui_timer() -> void
{
    ui_timer_.start(
        slint::TimerMode::Repeated, std::chrono::milliseconds(200), [this] { on_ui_tick(); });
}

auto AppController::load_queue_from_paths(
    const std::vector<std::filesystem::path> &paths, const std::string &source_description) -> void
{
    if (paths.empty()) {
        clear_queue("No supported audio files were selected.");
        return;
    }

    queue_.clear();
    queue_.reserve(paths.size());

    for (const auto &path : paths) {
        if (is_supported_audio_file(path)) {
            queue_.push_back(load_track_metadata(path));
        }
    }

    if (queue_.empty()) {
        clear_queue("No supported audio files were found.");
        return;
    }

    current_index_ = 0;
    current_source_ = queue_.front().path.parent_path();
    source_description_ = source_description;

    rebuild_queue_model();
    refresh_queue_labels();
    rebuild_cover_tags();
    load_lyrics_for_current_track();

    if (!open_next_playable_from(0, true)) {
        clear_queue("The selected audio files could not be opened by the playback engine.");
    }
}

auto AppController::clear_queue(const std::string &message) -> void
{
    player_.clear();
    queue_.clear();
    current_source_.clear();
    source_description_.clear();
    current_index_ = 0;
    lyrics_ = {};
    active_lyric_index_ = -1;
    player_.set_looping(playback_mode_ == PlaybackMode::RepeatOne);
    last_status_ = AudioPlayer::Status::Stopped;

    window_->set_queue_model(std::make_shared<slint::VectorModel<QueueTrack>>(std::vector<QueueTrack> {}));
    // window_->set_cover_tags(std::make_shared<slint::VectorModel<TagData>>(std::vector<TagData> {
    //     TagData { to_shared("Ready"), true },
    //     TagData { to_shared("0 loaded"), false },
    // }));
    window_->set_lyric_model(std::make_shared<slint::VectorModel<LyricLineData>>(placeholder_lyrics(nullptr)));
    window_->set_lyrics_subtitle(to_shared("Centered on the current phrase for low-distraction reading."));
    window_->set_next_lyric_hint(to_shared("Next: Load music"));
    window_->set_song_title(to_shared("No track loaded"));
    window_->set_song_artist(to_shared("Import audio to begin"));
    // window_->set_song_meta(to_shared("SFML playback core  ·  TagLib metadata"));
    window_->set_format_label(to_shared("No file"));
    // window_->set_collection_note(to_shared(message));
    window_->set_elapsed_label(to_shared("00:00"));
    window_->set_duration_label(to_shared("00:00"));
    window_->set_progress(0.0f);
    window_->set_playing(false);
    refresh_queue_labels();
}

auto AppController::open_track_at(std::size_t index, bool autoplay) -> bool
{
    if (index >= queue_.size()) {
        return false;
    }

    const bool opened = player_.open(queue_[index].path, autoplay);
    if (!opened) {
        return false;
    }

    current_index_ = index;
    player_.set_looping(playback_mode_ == PlaybackMode::RepeatOne);
    last_status_ = player_.status();
    refresh_now_playing();
    rebuild_queue_model();
    rebuild_cover_tags();
    load_lyrics_for_current_track();
    refresh_transport_state();
    return true;
}

auto AppController::open_next_playable_from(std::size_t start_index, bool autoplay) -> bool
{
    if (queue_.empty()) {
        return false;
    }

    for (std::size_t offset = 0; offset < queue_.size(); ++offset) {
        const std::size_t index = (start_index + offset) % queue_.size();
        if (open_track_at(index, autoplay)) {
            return true;
        }
    }

    return false;
}

auto AppController::on_open_files_requested() -> void
{
    const auto paths = platform_dialogs::pick_audio_files();
    if (paths.empty()) {
        return;
    }

    const std::string source = describe_source(paths.front().parent_path(), paths.size());
    load_queue_from_paths(paths, source);
}

auto AppController::on_open_folder_requested() -> void
{
    const auto folder = platform_dialogs::pick_folder();
    if (!folder.has_value()) {
        return;
    }

    auto paths = scan_audio_files(*folder);
    if (paths.empty()) {
        clear_queue("No supported audio files were found in the selected folder.");
        // window_->set_collection_note(to_shared("No supported audio files were found in the selected folder."));
        window_->set_queue_subtitle(to_shared("Selected folder contains no supported audio files."));
        return;
    }

    load_queue_from_paths(paths, describe_source(*folder, paths.size()));
}

auto AppController::on_toggle_play_requested() -> void
{
    if (queue_.empty()) {
        on_open_files_requested();
        return;
    }

    player_.toggle();
    refresh_transport_state();
}

auto AppController::on_previous_track_requested() -> void
{
    if (queue_.empty()) {
        return;
    }

    if (player_.position_seconds() > 3.0f) {
        player_.seek_to_ratio(0.0f);
        refresh_transport_state();
        return;
    }

    const std::size_t previous_index =
        current_index_ == 0 ? queue_.size() - 1 : current_index_ - 1;
    select_track(previous_index, true);
}

auto AppController::on_next_track_requested() -> void
{
    if (queue_.empty()) {
        return;
    }

    if (playback_mode_ == PlaybackMode::Shuffle) {
        if (const auto next_index = random_other_index()) {
            select_track(*next_index, true);
        }
        return;
    }

    if (const auto next_index = sequence_next_index(true)) {
        select_track(*next_index, true);
    }
}

auto AppController::on_queue_item_selected(int index) -> void
{
    if (index < 0) {
        return;
    }

    select_track(static_cast<std::size_t>(index), true);
}

auto AppController::on_seek_requested(float value) -> void
{
    player_.seek_to_ratio(value);
    sync_lyrics_to_position();
    refresh_transport_state();
}

auto AppController::on_volume_requested(float value) -> void
{
    player_.set_volume(value);
    window_->set_volume(player_.volume());
}

auto AppController::on_playback_mode_selected(int index) -> void
{
    if (index < 0 || index > 2) {
        return;
    }

    playback_mode_ = static_cast<PlaybackMode>(index);
    player_.set_looping(playback_mode_ == PlaybackMode::RepeatOne);
    rebuild_playback_mode_model();
}

auto AppController::on_toggle_queue_visibility_requested() -> void
{
    queue_visible_ = !queue_visible_;
    refresh_queue_labels();
}

auto AppController::on_toggle_sync_requested() -> void
{
    sync_enabled_ = !sync_enabled_;
    rebuild_lyric_model();
}

auto AppController::on_ui_tick() -> void
{
    auto current_status = player_.status();

    if (last_status_ == AudioPlayer::Status::Playing
        && current_status == AudioPlayer::Status::Stopped
        && playback_mode_ != PlaybackMode::RepeatOne) {
        handle_track_finished();
        current_status = player_.status();
    }

    sync_lyrics_to_position();
    refresh_transport_state();
    last_status_ = current_status;
}

auto AppController::handle_track_finished() -> void
{
    if (queue_.empty()) {
        return;
    }

    if (playback_mode_ == PlaybackMode::Shuffle) {
        if (const auto next_index = random_other_index()) {
            select_track(*next_index, true);
            return;
        }
    } else if (const auto next_index = sequence_next_index(false)) {
        select_track(*next_index, true);
        return;
    }

    window_->set_playing(false);
    window_->set_progress(1.0f);
    if (const auto *track = current_track()) {
        window_->set_elapsed_label(to_shared(format_duration(track->duration_seconds)));
    }
}

auto AppController::select_track(std::size_t index, bool autoplay) -> void
{
    if (index >= queue_.size()) {
        return;
    }

    if (!open_track_at(index, autoplay)) {
        set_status_message("Failed to open " + utf8_from_path(queue_[index].path.filename()) + '.');
    }
}

auto AppController::sequence_next_index(bool wrap) const -> std::optional<std::size_t>
{
    if (queue_.empty()) {
        return std::nullopt;
    }

    const std::size_t next_index = current_index_ + 1;
    if (next_index < queue_.size()) {
        return next_index;
    }

    if (wrap) {
        return std::size_t { 0 };
    }

    return std::nullopt;
}

auto AppController::random_other_index() -> std::optional<std::size_t>
{
    if (queue_.empty()) {
        return std::nullopt;
    }

    if (queue_.size() == 1) {
        return current_index_;
    }

    std::uniform_int_distribution<std::size_t> distribution(0, queue_.size() - 1);
    std::size_t next_index = current_index_;

    while (next_index == current_index_) {
        next_index = distribution(random_engine_);
    }

    return next_index;
}

auto AppController::rebuild_queue_model() -> void
{
    std::vector<QueueTrack> items;
    items.reserve(queue_.size());

    for (std::size_t index = 0; index < queue_.size(); ++index) {
        std::ostringstream order;
        order << std::setw(2) << std::setfill('0') << (index + 1);

        items.push_back(QueueTrack {
            to_shared(order.str()),
            to_shared(queue_[index].title),
            to_shared(queue_[index].artist),
            to_shared(format_duration(queue_[index].duration_seconds)),
            index == current_index_,
        });
    }

    window_->set_queue_model(std::make_shared<slint::VectorModel<QueueTrack>>(items));
}

auto AppController::rebuild_playback_mode_model() -> void
{
    std::vector<ModeOption> items {
        ModeOption { to_shared("Sequence"), playback_mode_ == PlaybackMode::Sequence },
        ModeOption { to_shared("Shuffle"), playback_mode_ == PlaybackMode::Shuffle },
        ModeOption { to_shared("Repeat one"), playback_mode_ == PlaybackMode::RepeatOne },
    };

    window_->set_playback_modes(std::make_shared<slint::VectorModel<ModeOption>>(items));
}

auto AppController::rebuild_cover_tags() -> void
{
    // std::vector<TagData> items;

    // if (const auto *track = current_track()) {
    //     items.push_back(TagData { to_shared(uppercase_ascii(track->extension)), true });
    // } else {
    //     items.push_back(TagData { to_shared("Ready"), true });
    // }

    // items.push_back(
    //     TagData { to_shared(std::to_string(queue_.size()) + " loaded"), false });

    // window_->set_cover_tags(std::make_shared<slint::VectorModel<TagData>>(items));
}

auto AppController::load_lyrics_for_current_track() -> void
{
    lyrics_ = {};
    active_lyric_index_ = -1;

    const auto *track = current_track();
    if (track == nullptr) {
        rebuild_lyric_model();
        return;
    }

    const auto lrc_path = find_lrc_for_track(track->path);
    if (lrc_path.has_value()) {
        if (const auto loaded = load_lrc_file(*lrc_path)) {
            lyrics_ = *loaded;
        }
    }

    rebuild_lyric_model();
}

auto AppController::rebuild_lyric_model() -> void
{
    const auto *track = current_track();

    if (lyrics_.empty()) {
        window_->set_lyric_model(
            std::make_shared<slint::VectorModel<LyricLineData>>(placeholder_lyrics(track)));

        const std::string subtitle = sync_enabled_
            ? "Sync toggle is on. Waiting for a same-name .lrc file."
            : "LRC support is enabled. Add a same-name .lrc file for synced lyrics.";
        window_->set_lyrics_subtitle(to_shared(subtitle));

        const std::string next_hint = track == nullptr
            ? "Next: Load music"
            : "Next: " + utf8_from_path(track->path.stem()) + ".lrc";
        window_->set_next_lyric_hint(to_shared(next_hint));
        return;
    }

    std::vector<LyricLineData> items;
    items.reserve(lyrics_.lines.size());

    for (std::size_t index = 0; index < lyrics_.lines.size(); ++index) {
        const bool active = static_cast<int>(index) == active_lyric_index_;
        const bool muted = active_lyric_index_ >= 0
            && static_cast<int>(index) < active_lyric_index_ - 1;

        items.push_back(LyricLineData {
            to_shared(lyrics_.lines[index].text),
            active,
            muted,
        });
    }

    window_->set_lyric_model(std::make_shared<slint::VectorModel<LyricLineData>>(items));

    std::string subtitle = "Synced LRC";
    if (!lyrics_.title.empty() || !lyrics_.artist.empty()) {
        subtitle += " · ";
        if (!lyrics_.title.empty()) {
            subtitle += lyrics_.title;
        }
        if (!lyrics_.artist.empty()) {
            if (!lyrics_.title.empty()) {
                subtitle += " / ";
            }
            subtitle += lyrics_.artist;
        }
    }
    subtitle += " · " + std::to_string(lyrics_.lines.size()) + " lines";
    window_->set_lyrics_subtitle(to_shared(subtitle));

    if (active_lyric_index_ >= 0
        && static_cast<std::size_t>(active_lyric_index_ + 1) < lyrics_.lines.size()) {
        window_->set_next_lyric_hint(
            to_shared("Next: " + lyrics_.lines[static_cast<std::size_t>(active_lyric_index_ + 1)].text));
    } else if (!lyrics_.lines.empty()) {
        window_->set_next_lyric_hint(to_shared("Next: " + lyrics_.lines.front().text));
    } else {
        window_->set_next_lyric_hint(to_shared("Next: ..."));
    }
}

auto AppController::sync_lyrics_to_position() -> void
{
    if (lyrics_.empty()) {
        return;
    }

    const int position_ms = static_cast<int>(player_.position_seconds() * 1000.0f);
    int new_active_index = -1;

    for (std::size_t index = 0; index < lyrics_.lines.size(); ++index) {
        if (lyrics_.lines[index].time_ms > position_ms) {
            break;
        }
        new_active_index = static_cast<int>(index);
    }

    if (new_active_index == active_lyric_index_) {
        return;
    }

    active_lyric_index_ = new_active_index;
    rebuild_lyric_model();
}

auto AppController::refresh_now_playing() -> void
{
    const auto *track = current_track();
    if (track == nullptr) {
        return;
    }

    window_->set_song_title(to_shared(track->title));
    window_->set_song_artist(to_shared(track_artist_line(*track)));
    // window_->set_song_meta(to_shared(track_meta_line(*track)));
    window_->set_format_label(to_shared(track_format_label(*track)));
    // window_->set_collection_note(to_shared(source_description_.empty()
    //         ? "Local file playback is active."
    //         : source_description_));

    const int duration_seconds = track->duration_seconds > 0
        ? track->duration_seconds
        : static_cast<int>(player_.duration_seconds());
    window_->set_duration_label(to_shared(format_duration(duration_seconds)));
}

auto AppController::refresh_transport_state() -> void
{
    const auto *track = current_track();
    const int elapsed_seconds = static_cast<int>(player_.position_seconds());
    const int duration_seconds = track != nullptr && track->duration_seconds > 0
        ? track->duration_seconds
        : static_cast<int>(player_.duration_seconds());

    window_->set_elapsed_label(to_shared(format_duration(elapsed_seconds)));
    window_->set_duration_label(to_shared(format_duration(duration_seconds)));
    window_->set_progress(player_.progress());
    window_->set_playing(player_.status() == AudioPlayer::Status::Playing);
}

auto AppController::refresh_queue_labels() -> void
{
    const auto count = queue_.size();
    window_->set_queue_loaded_label(to_shared(std::to_string(count) + " loaded"));

    std::string subtitle;
    if (count == 0) {
        subtitle = "Playback order, import actions, and the upcoming tracks.";
    } else {
        subtitle = source_description_;
        subtitle += queue_visible_ ? " Queue focus is on." : " Queue focus is hidden.";
    }

    window_->set_queue_subtitle(to_shared(subtitle));
}

auto AppController::set_status_message(const std::string &message) -> void
{
    // window_->set_collection_note(to_shared(message));
}

auto AppController::current_track() const -> const Track *
{
    if (queue_.empty() || current_index_ >= queue_.size()) {
        return nullptr;
    }

    return &queue_[current_index_];
}

#include "audio_player.h"

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/SoundSource.hpp>
#include <SFML/System/Time.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

class AudioPlayer::Impl {
public:
    struct Snapshot {
        float normalized_volume = 0.68f;
        float position_seconds = 0.0f;
        float duration_seconds = 0.0f;
        bool has_track = false;
        bool looping = false;
        AudioPlayer::Status status = AudioPlayer::Status::Stopped;
    };

    struct WorkerState {
        sf::Music music;
        std::filesystem::path current_path;
        float normalized_volume = 0.68f;
        bool has_track = false;
        bool looping = false;
    };

    Impl()
    {
        worker_ = std::thread([this] { thread_main(); });
    }

    ~Impl()
    {
        {
            std::lock_guard lock(mutex_);
            stop_requested_ = true;
        }
        condition_.notify_one();

        if (worker_.joinable()) {
            worker_.join();
        }
    }

    template<typename Fn>
    auto invoke_sync(Fn &&fn) -> std::invoke_result_t<Fn, WorkerState &>
    {
        using Result = std::invoke_result_t<Fn, WorkerState &>;

        auto promise = std::make_shared<std::promise<Result>>();
        auto future = promise->get_future();

        enqueue([fn = std::forward<Fn>(fn), promise](WorkerState &state) mutable {
            try {
                if constexpr (std::is_void_v<Result>) {
                    std::invoke(fn, state);
                    promise->set_value();
                } else {
                    promise->set_value(std::invoke(fn, state));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return future.get();
    }

    auto snapshot() const -> Snapshot
    {
        std::lock_guard lock(mutex_);
        return snapshot_;
    }

private:
    using Command = std::function<void(WorkerState &)>;

    auto enqueue(Command command) -> void
    {
        {
            std::lock_guard lock(mutex_);
            commands_.push_back(std::move(command));
        }
        condition_.notify_one();
    }

    static auto map_status(sf::SoundSource::Status status) -> AudioPlayer::Status
    {
        switch (status) {
        case sf::SoundSource::Status::Playing:
            return AudioPlayer::Status::Playing;
        case sf::SoundSource::Status::Paused:
            return AudioPlayer::Status::Paused;
        case sf::SoundSource::Status::Stopped:
        default:
            return AudioPlayer::Status::Stopped;
        }
    }

    auto publish_snapshot_locked(const WorkerState &state) -> void
    {
        snapshot_.normalized_volume = state.normalized_volume;
        snapshot_.has_track = state.has_track;
        snapshot_.looping = state.looping;
        snapshot_.status = map_status(state.music.getStatus());
        snapshot_.position_seconds =
            state.has_track ? state.music.getPlayingOffset().asSeconds() : 0.0f;
        snapshot_.duration_seconds = state.has_track ? state.music.getDuration().asSeconds() : 0.0f;
    }

    auto thread_main() -> void
    {
        WorkerState state;
        state.music.setVolume(state.normalized_volume * 100.0f);

        for (;;) {
            std::deque<Command> pending;

            {
                std::unique_lock lock(mutex_);
                publish_snapshot_locked(state);
                condition_.wait_for(lock, std::chrono::milliseconds(40), [this] {
                    return stop_requested_ || !commands_.empty();
                });

                if (stop_requested_ && commands_.empty()) {
                    state.music.stop();
                    state.has_track = false;
                    publish_snapshot_locked(state);
                    return;
                }

                pending.swap(commands_);
            }

            for (auto &command : pending) {
                command(state);
            }

            {
                std::lock_guard lock(mutex_);
                publish_snapshot_locked(state);
            }
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Command> commands_;
    Snapshot snapshot_;
    bool stop_requested_ = false;
    std::thread worker_;
};

namespace {

auto clamp01(float value) -> float
{
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

AudioPlayer::AudioPlayer() : impl_(new Impl())
{
}

AudioPlayer::~AudioPlayer()
{
    delete impl_;
}

auto AudioPlayer::open(const std::filesystem::path &path, bool autoplay) -> bool
{
    return impl_->invoke_sync([path, autoplay](Impl::WorkerState &state) {
        state.music.stop();
        state.has_track = false;
        state.current_path.clear();

        if (!state.music.openFromFile(path)) {
            return false;
        }

        state.current_path = path;
        state.has_track = true;
        state.music.setVolume(state.normalized_volume * 100.0f);
        state.music.setLooping(state.looping);

        if (autoplay) {
            state.music.play();
        }

        return true;
    });
}

auto AudioPlayer::has_track() const -> bool
{
    return impl_->snapshot().has_track;
}

auto AudioPlayer::clear() -> void
{
    impl_->invoke_sync([](Impl::WorkerState &state) {
        state.music.stop();
        state.current_path.clear();
        state.has_track = false;
    });
}

auto AudioPlayer::play() -> void
{
    impl_->invoke_sync([](Impl::WorkerState &state) {
        if (state.has_track) {
            state.music.play();
        }
    });
}

auto AudioPlayer::pause() -> void
{
    impl_->invoke_sync([](Impl::WorkerState &state) {
        if (state.has_track) {
            state.music.pause();
        }
    });
}

auto AudioPlayer::toggle() -> void
{
    impl_->invoke_sync([](Impl::WorkerState &state) {
        if (!state.has_track) {
            return;
        }

        if (state.music.getStatus() == sf::SoundSource::Status::Playing) {
            state.music.pause();
            return;
        }

        state.music.play();
    });
}

auto AudioPlayer::seek_to_ratio(float ratio) -> void
{
    impl_->invoke_sync([ratio](Impl::WorkerState &state) {
        if (!state.has_track) {
            return;
        }

        const auto duration = state.music.getDuration().asSeconds();
        state.music.setPlayingOffset(sf::seconds(duration * clamp01(ratio)));
    });
}

auto AudioPlayer::set_volume(float normalized_volume) -> void
{
    impl_->invoke_sync([normalized_volume](Impl::WorkerState &state) {
        state.normalized_volume = clamp01(normalized_volume);
        state.music.setVolume(state.normalized_volume * 100.0f);
    });
}

auto AudioPlayer::volume() const -> float
{
    return impl_->snapshot().normalized_volume;
}

auto AudioPlayer::set_looping(bool looping) -> void
{
    impl_->invoke_sync([looping](Impl::WorkerState &state) {
        state.looping = looping;
        state.music.setLooping(looping);
    });
}

auto AudioPlayer::progress() const -> float
{
    const auto snapshot = impl_->snapshot();
    const auto duration = snapshot.duration_seconds;
    if (duration <= 0.0f) {
        return 0.0f;
    }
    return clamp01(snapshot.position_seconds / duration);
}

auto AudioPlayer::position_seconds() const -> float
{
    return impl_->snapshot().position_seconds;
}

auto AudioPlayer::duration_seconds() const -> float
{
    return impl_->snapshot().duration_seconds;
}

auto AudioPlayer::status() const -> Status
{
    return impl_->snapshot().status;
}

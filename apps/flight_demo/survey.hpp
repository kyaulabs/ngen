#pragma once
#include "flight_demo/app.hpp"
#include "quests/voyage.hpp"
#include "render/scene.hpp"
#include "tools/reload.hpp"
#include <array>
#include <chrono>

namespace ngen::demo {
[[nodiscard]] bool destination_landmark(const assets::Scene &scene);

// Window-owner bridge. Hold at most one uncommitted destination; only publish
// its scene after both GPU preparation and the voyage's saved arrival.
class SurveyFlight {
  public:
    SurveyFlight(const Options &options, const Preset &preset, render::ScenePin ship_scene);
    [[nodiscard]] std::optional<flight::Snapshot> local() const;
    [[nodiscard]] Result<std::monostate> submit(flight::Intent intent);
    void apply_action();
    void tick();
    void stream(render::Context &context);
    void panel(bool replay);
    void replay();
    void report(bool replay, std::size_t landmark_frames) const;
    void begin_soak(std::chrono::steady_clock::time_point start);
    void sample_soak(std::uint64_t elapsed_ms, const render::ResourceStatistics &memory);
    struct FrameTiming {
        std::uint64_t frame_ns{}, queue_ns{};
    };
    void sample_frame(FrameTiming timing);
    void finish_soak(std::uint32_t seconds, std::uint64_t elapsed_ms) const;
    [[nodiscard]] bool in_transit() const { return voyage_->destination().has_value(); }
    [[nodiscard]] render::ScenePin landmark() const { return landmark_; }

  private:
    struct Issued {
        quests::Command command;
        quests::Receipt receipt;
    };
    struct Soak {
        explicit Soak(int descriptor) noexcept : root(descriptor) {}
        ~Soak();
        Soak(const Soak &) = delete;
        Soak &operator=(const Soak &) = delete;
        const int root;
        std::chrono::steady_clock::time_point start;
        std::optional<Issued> departure;
        unsigned charter{}, exhausted{}, sales{}, wins{}, losses{}, reopens{}, samples{};
        std::uint64_t last_sale_ms{}, last_sample_ms{};
        // 100 us ceiling buckets, with 200 ms and above in the last bucket.
        std::array<std::uint64_t, 2001> frames{};
        std::uint64_t frame_count{}, max_frame_ns{}, first_queue_ns{}, last_queue_ns{},
            max_queue_gap_ns{};
    };
    void reopen();
    void next_charter();
    void check_step(const quests::State &before, const quests::VoyageOutcome &outcome);
    void record_sale(const quests::VoyageOutcome &outcome, const quests::SaveTimings &timings);
    void prepare();
    void start_upload(const std::filesystem::path &path);
    void action(quests::Action action);
    void receive(const quests::VoyageOutcome &outcome);
    const char *system() const;
    const char *message_{"Surveyors charter: ready"};
    const std::filesystem::path path_, retry_path_;
    std::filesystem::path directory_;
    const flight::ShipDefinition &ship_;
    const std::span<const physics::Box> contacts_;
    const render::ScenePin ship_scene_;
    std::unique_ptr<quests::Voyage> voyage_;
    std::optional<tools::PackageReloader> reader_;
    std::optional<render::ScenePrefetch> pending_;
    render::ScenePin landmark_;
    bool upload_failed_{}, upload_retried_{}, replay_out_{}, replay_home_{};
    unsigned arrivals_{}, exit_polls_{};
    std::optional<quests::Action> queued_;
    quests::Command last_command_;
    std::optional<Soak> soak_;
    bool uncertain_{};
};
} // namespace ngen::demo

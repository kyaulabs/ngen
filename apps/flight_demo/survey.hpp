#pragma once
#include "flight_demo/app.hpp"
#include "quests/voyage.hpp"
#include "render/scene.hpp"
#include "tools/reload.hpp"

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
    [[nodiscard]] bool in_transit() const { return voyage_->destination().has_value(); }
    [[nodiscard]] render::ScenePin landmark() const { return landmark_; }

  private:
    void prepare();
    void start_upload(const std::filesystem::path &path);
    void action(quests::Action action);
    void receive(const quests::VoyageOutcome &outcome);
    const char *system() const;
    const char *message_{"Surveyors charter: ready"};
    const std::filesystem::path path_, retry_path_;
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
    bool uncertain_{};
};
} // namespace ngen::demo

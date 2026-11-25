#pragma once
#include "quests/save.hpp"
#include "travel/journey.hpp"
#include "travel/system_manifest.hpp"

namespace ngen::quests {
struct InTransit {};
using VoyageOutcome = std::variant<Receipt, InTransit, Rejection, SaveError, Error>;
struct SiteTarget {
    coordinates::Position origin;
    std::uint64_t revision{};
};
struct VoyageFrame {
    // Reload preserves this campaign identity; recovery issues a new one.
    // The native journey's Entity and body IDs are local to its flight domain.
    std::uint64_t campaign_vessel{};
    travel::Snapshot journey;
};

// One game owner. The quest observes only the published berth, not intermediate
// trajectory ticks or unchecked poses. Pending travel is deliberately not saved.
class Voyage {
  public:
    [[nodiscard]] static std::variant<std::unique_ptr<Voyage>, SaveError>
    open(const std::filesystem::path &directory, const flight::ShipDefinition &ship);
    Voyage(const Voyage &) = delete;
    Voyage &operator=(const Voyage &) = delete;

    [[nodiscard]] State snapshot() const { return save_->snapshot(); }
    [[nodiscard]] std::uint32_t revision() const { return save_->revision(); }
    [[nodiscard]] travel::Security security() const;
    [[nodiscard]] SiteTarget departure() const;
    [[nodiscard]] std::optional<SiteTarget> destination() const;
    // No contact snapshot is exposed after a failed save until local preparation.
    [[nodiscard]] std::optional<VoyageFrame> native() const;
    // Materialize the committed system after opening or recovering. Never
    // replace a live contact domain as a shortcut to parking.
    [[nodiscard]] Result<VoyageFrame> prepare_local(const travel::CollisionSite &site);
    [[nodiscard]] Result<std::monostate> submit(flight::Intent intent);
    // If no local domain exists, the headless owner can supply source contacts
    // at departure. Existing local contacts are never replaced on request.
    // A pending trip excludes all quest commands until settled/cancelled.
    [[nodiscard]] VoyageOutcome request(Command command,
                                        const travel::CollisionSite *source = nullptr);
    [[nodiscard]] Result<VoyageFrame> advance();
    [[nodiscard]] Result<std::monostate> cancel();
    [[nodiscard]] VoyageOutcome arrive(const travel::CollisionSite *site,
                                       SaveTimings *timings = nullptr);

  private:
    Voyage(std::unique_ptr<Save> save, const flight::ShipDefinition &ship);
    const std::unique_ptr<Save> save_;
    const flight::ShipDefinition ship_;
    const travel::ConnectedSystems catalog_{travel::streaming_fixture(19)};
    struct Active {
        Command command;
        SiteTarget target;
    };
    [[nodiscard]] Result<travel::Route> route() const;
    [[nodiscard]] travel::Command envelope() const;
    std::optional<Active> active_;
    std::unique_ptr<travel::Journey> journey_;
    Tick tick_;
    std::uint64_t sequence_{};
};
} // namespace ngen::quests

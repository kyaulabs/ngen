#include "quests/voyage.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <stdexcept>
#include <utility>

using namespace ngen;
using namespace ngen::quests;

namespace {
class Workspace {
  public:
    Workspace() {
        char pattern[] = "/tmp/ngen-voyage-XXXXXX";
        const auto *directory = mkdtemp(pattern);
        REQUIRE(directory != nullptr);
        root = directory;
    }
    ~Workspace() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    Workspace(const Workspace &) = delete;
    Workspace &operator=(const Workspace &) = delete;
    std::filesystem::path root;
};

std::unique_ptr<Voyage> open(const Workspace &workspace) {
    auto result = Voyage::open(workspace.root, {});
    REQUIRE(std::holds_alternative<std::unique_ptr<Voyage>>(result));
    return std::get<std::unique_ptr<Voyage>>(std::move(result));
}

template <class T> T present(std::optional<T> value) {
    if (!value)
        throw std::runtime_error("expected voyage state");
    return *value;
}

VoyageOutcome arm(Voyage &voyage, quests::Command command) {
    const auto source = voyage.departure();
    const travel::CollisionSite site{source.origin, source.revision, {}};
    return voyage.request(command, &site);
}

SiteTarget exit(Voyage &voyage) {
    for (int tick = 0; tick < 9; ++tick)
        REQUIRE(std::holds_alternative<VoyageFrame>(voyage.advance()));
    REQUIRE(present(voyage.native()).journey.phase == travel::Phase::exit);
    return present(voyage.destination());
}

Receipt act(Voyage &voyage, Action action) {
    const auto result = voyage.request({voyage.revision(), action});
    REQUIRE(std::holds_alternative<Receipt>(result));
    return std::get<Receipt>(result);
}
} // namespace

TEST_CASE("a saved voyage does not commit a departure fee before native arrival",
          "[survey-voyage]") {
    Workspace workspace;
    auto voyage = open(workspace);
    act(*voyage, Action::learn_law);
    const auto pledged = act(*voyage, Action::join_survey);
    const quests::Command outbound{pledged.revision, Action::accept_risk};
    CHECK(std::get<Error>(voyage->request(outbound)) == Error::busy);
    const auto start = voyage->departure();
    const travel::CollisionSite stale_source{start.origin, start.revision + 1, {}};
    CHECK(std::get<Error>(voyage->request(outbound, &stale_source)) == Error::stale);
    const std::array source_contact{physics::Box{{1, 1, 1}, {}, 1000, false}};
    const travel::CollisionSite blocked_source{start.origin, start.revision, source_contact};
    CHECK(std::get<Error>(voyage->request(outbound, &blocked_source)) == Error::busy);
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, outbound)));
    CHECK(voyage->snapshot() == pledged.after);
    CHECK(voyage->revision() == pledged.revision);
    CHECK(voyage->security() == travel::Security::protected_space);
    CHECK(std::get<Error>(voyage->request({pledged.revision, Action::mine})) == Error::busy);
    CHECK(std::get<Error>(voyage->request({0, Action::learn_law})) == Error::busy);
    const auto entry = present(voyage->native());
    CHECK(entry.journey.phase == travel::Phase::entry);
    CHECK(entry.campaign_vessel == pledged.after.vessel);
    const auto target = exit(*voyage);
    CHECK(target.origin == travel::streaming_fixture(19).systems[1].berth);
    CHECK_FALSE(present(voyage->native()).journey.local);
    CHECK(std::get<Error>(voyage->arrive(nullptr)) == Error::busy);
    const travel::CollisionSite stale{target.origin, target.revision + 1, {}};
    CHECK(std::get<Error>(voyage->arrive(&stale)) == Error::stale);
    const std::array occupied{physics::Box{{1, 1, 1}, {}, 1000, false}};
    const travel::CollisionSite blocked{target.origin, target.revision, occupied};
    CHECK(std::get<Error>(voyage->arrive(&blocked)) == Error::busy);
    CHECK(voyage->snapshot() == pledged.after);
    CHECK(voyage->revision() == pledged.revision);
    const travel::CollisionSite clear{target.origin, target.revision, {}};
    const auto result = voyage->arrive(&clear);
    REQUIRE(std::holds_alternative<Receipt>(result));
    const auto arrived = std::get<Receipt>(result);
    CHECK(arrived.after.stage == Stage::prospecting);
    CHECK(arrived.after.credits == 8);
    CHECK(arrived.after.vessel == pledged.after.vessel);
    CHECK(voyage->security() == travel::Security::lawless);
    const auto landed = present(voyage->native());
    CHECK(landed.journey.vessel == entry.journey.vessel);
    CHECK(landed.campaign_vessel == entry.campaign_vessel);
    CHECK(present(landed.journey.local).frame.origin == target.origin);
    CHECK(std::get<Receipt>(voyage->request(outbound)) == arrived);
    CHECK_FALSE(voyage->destination());
}

TEST_CASE("cancellation and restart before arrival leave the charter at the source",
          "[survey-voyage]") {
    Workspace workspace;
    auto voyage = open(workspace);
    act(*voyage, Action::learn_law);
    const auto charter = act(*voyage, Action::join_survey);
    const quests::Command outbound{charter.revision, Action::accept_risk};
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, outbound)));
    CHECK(std::holds_alternative<std::monostate>(voyage->cancel()));
    CHECK_FALSE(voyage->destination());
    CHECK(voyage->snapshot() == charter.after);
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, outbound)));
    const auto target = exit(*voyage);
    CHECK(std::get<Error>(voyage->cancel()) == Error::busy);
    voyage.reset();
    voyage = open(workspace);
    CHECK(voyage->revision() == charter.revision);
    CHECK(voyage->snapshot() == charter.after);
    CHECK(voyage->security() == travel::Security::protected_space);
    CHECK_FALSE(voyage->native());
    CHECK_FALSE(voyage->destination());
    CHECK(std::get<Error>(voyage->arrive(nullptr)) == Error::busy);
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, outbound)));
    CHECK(exit(*voyage).origin == target.origin);
}

TEST_CASE("the return sale needs a fresh native arrival if save publication fails",
          "[survey-voyage]") {
    Workspace workspace;
    auto voyage = open(workspace);
    act(*voyage, Action::learn_law);
    act(*voyage, Action::join_survey);
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, {2, Action::accept_risk})));
    const auto frontier = exit(*voyage);
    const travel::CollisionSite frontier_site{frontier.origin, frontier.revision, {}};
    REQUIRE(std::holds_alternative<Receipt>(voyage->arrive(&frontier_site)));
    const auto outbound_tick = present(voyage->native()).journey.tick;
    act(*voyage, Action::mine);
    bool recovered = false;
    SECTION("return after victory") {}
    SECTION("return after recovery") {
        recovered = true;
        act(*voyage, Action::lose_ship);
        CHECK_FALSE(voyage->native());
        CHECK(std::get<Error>(voyage->prepare_local(frontier_site)) == Error::busy);
        act(*voyage, Action::recover);
    }
    const auto returnable = act(*voyage, recovered ? Action::salvage : Action::defeat_rival);
    const quests::Command inbound{returnable.revision, Action::dock_and_sell};
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, inbound)));
    const auto returning = present(voyage->native());
    CHECK(returning.journey.tick == outbound_tick);
    CHECK(returning.campaign_vessel == (recovered ? 2 : 1));
    const auto home = exit(*voyage);
    CHECK(home.origin == travel::streaming_fixture(19).systems[0].berth);
    const travel::CollisionSite home_site{home.origin, home.revision, {}};
    SaveTimings failed_timings;
    CHECK(std::get<Error>(voyage->arrive(nullptr, &failed_timings)) == Error::busy);
    CHECK_FALSE(failed_timings.market_ns);
    CHECK_FALSE(failed_timings.persistence_ns);
    REQUIRE(std::filesystem::create_directory(workspace.root / ".ngen-voyage.pending"));
    CHECK(std::get<SaveError>(voyage->arrive(&home_site, &failed_timings)) == SaveError::io);
    CHECK(failed_timings.market_ns.has_value());
    CHECK(failed_timings.persistence_ns.has_value());
    CHECK(voyage->snapshot() == returnable.after);
    CHECK_FALSE(voyage->native());
    CHECK_FALSE(voyage->destination());
    CHECK(voyage->security() == travel::Security::lawless);
    voyage.reset();
    REQUIRE(std::filesystem::remove(workspace.root / ".ngen-voyage.pending"));
    voyage = open(workspace);
    CHECK(voyage->snapshot() == returnable.after);
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, inbound)));
    CHECK(exit(*voyage).origin == home.origin);
    SaveTimings sold_timings;
    const auto result = voyage->arrive(&home_site, &sold_timings);
    REQUIRE(std::holds_alternative<Receipt>(result));
    CHECK(sold_timings.market_ns.has_value());
    CHECK(sold_timings.persistence_ns.has_value());
    const auto sold = std::get<Receipt>(result);
    CHECK(sold.after.credits == 27);
    CHECK(sold.after.cargo == 0);
    CHECK(sold.after.station_stock == 3);
    CHECK(sold.after.vessel == (recovered ? 2 : 1));
    CHECK(present(voyage->native()).campaign_vessel == sold.after.vessel);
    CHECK(voyage->security() == travel::Security::protected_space);
    voyage.reset();
    voyage = open(workspace);
    CHECK(voyage->security() == travel::Security::protected_space);
    CHECK(std::get<Receipt>(voyage->request(inbound)) == sold);
    CHECK_FALSE(voyage->destination());
    CHECK(voyage->snapshot() == sold.after);
}

TEST_CASE("return entry and cancellation keep the landed contact body", "[survey-voyage]") {
    Workspace workspace;
    auto voyage = open(workspace);
    act(*voyage, Action::learn_law);
    act(*voyage, Action::join_survey);
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, {2, Action::accept_risk})));
    const auto frontier = exit(*voyage);
    const travel::CollisionSite frontier_site{frontier.origin, frontier.revision, {}};
    REQUIRE(std::holds_alternative<Receipt>(voyage->arrive(&frontier_site)));
    const auto landed = present(present(voyage->native()).journey.local);
    act(*voyage, Action::mine);
    const auto clear = act(*voyage, Action::defeat_rival);
    const quests::Command home{clear.revision, Action::dock_and_sell};
    REQUIRE(std::holds_alternative<InTransit>(arm(*voyage, home)));
    CHECK(present(present(voyage->native()).journey.local).body.id == landed.body.id);
    CHECK(std::holds_alternative<std::monostate>(voyage->cancel()));
    CHECK(present(present(voyage->native()).journey.local).body.id == landed.body.id);
    CHECK(voyage->snapshot() == clear.after);
}

TEST_CASE("pilot commands cannot be bypassed by recreating a parked departure body",
          "[survey-voyage]") {
    Workspace workspace;
    auto voyage = open(workspace);
    const auto source = voyage->departure();
    const travel::CollisionSite wrong{source.origin, source.revision + 1, {}};
    CHECK(std::get<Error>(voyage->prepare_local(wrong)) == Error::stale);
    const travel::CollisionSite site{source.origin, source.revision, {}};
    const auto prepared = voyage->prepare_local(site);
    REQUIRE(std::holds_alternative<VoyageFrame>(prepared));
    const auto original = present(std::get<VoyageFrame>(prepared).journey.local);
    act(*voyage, Action::learn_law);
    const auto charter = act(*voyage, Action::join_survey);
    const quests::Command outbound{charter.revision, Action::accept_risk};
    REQUIRE(
        std::holds_alternative<std::monostate>(voyage->submit(flight::Destination{{0, 0, -10}})));
    CHECK(std::get<Error>(voyage->request(outbound)) == Error::busy);
    for (int tick = 0; tick < 150; ++tick)
        REQUIRE(std::holds_alternative<VoyageFrame>(voyage->advance()));
    const auto outside = present(present(voyage->native()).journey.local);
    REQUIRE(outside.body.position_metres[2] < -3);
    CHECK(std::get<Error>(voyage->request(outbound, &site)) == Error::invalid);
    CHECK(std::get<Error>(voyage->prepare_local(site)) == Error::busy);
    CHECK(present(present(voyage->native()).journey.local).body.position_metres ==
          outside.body.position_metres);
    CHECK(voyage->snapshot() == charter.after);
    REQUIRE(std::holds_alternative<std::monostate>(voyage->submit(flight::Destination{{}})));
    for (int tick = 0; tick < 600; ++tick)
        REQUIRE(std::holds_alternative<VoyageFrame>(voyage->advance()));
    const auto berth = present(present(voyage->native()).journey.local);
    REQUIRE(berth.body.position_metres != original.body.position_metres);
    REQUIRE(std::holds_alternative<InTransit>(voyage->request(outbound)));
    CHECK(std::get<Error>(voyage->submit(flight::SetMass{1200})) == Error::busy);
    const auto entry = present(present(voyage->native()).journey.local);
    CHECK(entry.body.id == original.body.id);
    CHECK(entry.body.position_metres == berth.body.position_metres);
    CHECK(entry.tick == berth.tick);
    REQUIRE(std::holds_alternative<std::monostate>(voyage->cancel()));
    CHECK(present(present(voyage->native()).journey.local).body.position_metres ==
          berth.body.position_metres);
    REQUIRE(std::holds_alternative<std::monostate>(voyage->submit(flight::SetMass{1200})));
    const auto resumed = std::get<VoyageFrame>(voyage->advance());
    CHECK(present(resumed.journey.local).body.id == original.body.id);
    CHECK(present(resumed.journey.local).body.mass_kg == 1200);
}

TEST_CASE("a text save is neither a voyage checkpoint nor proof of arrival", "[survey-voyage]") {
    Workspace workspace;
    {
        auto result = Save::open(workspace.root);
        REQUIRE(std::holds_alternative<std::unique_ptr<Save>>(result));
        auto save = std::get<std::unique_ptr<Save>>(std::move(result));
        REQUIRE(std::holds_alternative<Receipt>(save->step({0, Action::learn_law})));
    }
    {
        const auto voyage = open(workspace);
        CHECK(voyage->revision() == 0);
    }
    REQUIRE(std::filesystem::copy_file(workspace.root / "ngen-survey.save",
                                       workspace.root / "ngen-voyage.save"));
    CHECK(std::get<SaveError>(Voyage::open(workspace.root, {})) == SaveError::invalid);
}

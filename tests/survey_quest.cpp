#include "quests/survey.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ngen::quests;

namespace {
Receipt advance(Survey &survey, Action action) {
    const auto response = survey.step({survey.revision(), action});
    REQUIRE(std::holds_alternative<Receipt>(response));
    return std::get<Receipt>(response);
}
} // namespace

TEST_CASE("the survey career starts protected and requires the lesson and risk decision",
          "[survey]") {
    Survey survey;
    CHECK(survey.snapshot() == State{});
    CHECK(survey.step({0, Action::accept_risk}) == Outcome{Rejection::wrong_stage});
    CHECK(survey.step({0, Action::join_survey}) == Outcome{Rejection::wrong_stage});
    CHECK(survey.revision() == 0);
    const auto lesson = advance(survey, Action::learn_law);
    CHECK(lesson.after.stage == Stage::enlistment);
    CHECK(advance(survey, Action::join_survey).after.stage == Stage::protected_port);
    CHECK(survey.step({1, Action::join_survey}) == Outcome{Receipt{survey.snapshot(), 2}});
    CHECK(survey.step({0, Action::join_survey}) == Outcome{Rejection::conflict});
    CHECK(survey.step({99, Action::accept_risk}) == Outcome{Rejection::order});
    const auto departure = advance(survey, Action::accept_risk);
    CHECK(departure.revision == 3);
    CHECK(departure.after.stage == Stage::prospecting);
    CHECK(departure.after.credits == 8);
    CHECK(departure.after.station_credits == 122);
    CHECK(survey.step({0, Action::learn_law}) == Outcome{lesson});
    CHECK(survey.snapshot() == departure.after);
}

TEST_CASE("mined ore and finite station credits fund repeatable survey progression", "[survey]") {
    Survey survey;
    advance(survey, Action::learn_law);
    advance(survey, Action::join_survey);
    for (std::uint64_t run = 1; run <= 2; ++run) {
        advance(survey, Action::accept_risk);
        CHECK(survey.step({survey.revision(), Action::dock_and_sell}) ==
              Outcome{Rejection::wrong_stage});
        const auto mined = advance(survey, Action::mine).after;
        CHECK(mined.cargo == 3);
        CHECK(mined.deposit == 12 - 3 * run);
        advance(survey, Action::defeat_rival);
        const auto sale = advance(survey, Action::dock_and_sell);
        const auto end = sale.after;
        CHECK(end.stage == Stage::protected_port);
        CHECK(end.cargo == 0);
        CHECK(end.station_stock == 3 * run);
        CHECK(end.completed_runs == run);
        CHECK(end.vessel == 1);
        CHECK(end.credits == 14 + 13 * run);
        CHECK(end.credits + end.station_credits == 130);
        CHECK(end.deposit + end.station_stock + end.cargo + end.wreck == 12);
        CHECK(survey.step({sale.revision - 1, Action::dock_and_sell}) == Outcome{sale});
        CHECK(survey.snapshot() == end);
    }
}

TEST_CASE("a repeatable loss and salvage route does not reward destroying a starter", "[survey]") {
    Survey survey;
    advance(survey, Action::learn_law);
    advance(survey, Action::join_survey);
    for (std::uint64_t run = 1; run <= 4; ++run) {
        advance(survey, Action::accept_risk);
        const auto mined = advance(survey, Action::mine).after;
        const auto loss = advance(survey, Action::lose_ship);
        CHECK(loss.after.stage == Stage::lost);
        CHECK(loss.after.cargo == 0);
        CHECK(loss.after.wreck == 3);
        CHECK(loss.after.credits == mined.credits);
        CHECK(survey.step({survey.revision(), Action::salvage}) == Outcome{Rejection::wrong_stage});
        const auto recover = advance(survey, Action::recover);
        CHECK(recover.after.stage == Stage::recovered);
        CHECK(recover.after.vessel == run + 1);
        CHECK(recover.after.cargo == 0);
        CHECK(recover.after.credits == mined.credits);
        const auto loot = advance(survey, Action::salvage);
        CHECK(loot.after.cargo == mined.cargo);
        CHECK(loot.after.wreck == 0);
        CHECK(survey.step({recover.revision - 1, Action::recover}) == Outcome{recover});
        CHECK(survey.step({loot.revision - 1, Action::salvage}) == Outcome{loot});
        const auto end = advance(survey, Action::dock_and_sell).after;
        CHECK(end.credits == 14 + 13 * run);
        CHECK(end.deposit + end.cargo + end.wreck + end.station_stock == 12);
        CHECK(end.credits + end.station_credits == 130);
    }
    CHECK(survey.step({survey.revision(), Action::accept_risk}) == Outcome{Rejection::exhausted});
    CHECK(survey.snapshot().station_stock == 12);
    CHECK(survey.snapshot().completed_runs == 4);
}

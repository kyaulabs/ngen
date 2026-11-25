#include "flight_demo/survey.hpp"
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <stdexcept>
#include <utility>

namespace ngen::demo {
using quests::Action;
using quests::Stage;

SurveyFlight::SurveyFlight(const Options &options, const Preset &preset, render::ScenePin scene)
    : path_(options.destination), retry_path_(options.destination_retry),
      contacts_(preset.contacts), ship_scene_(std::move(scene)) {
    auto opened = quests::Voyage::open(options.survey, preset.ship.flight);
    if (const auto *error = std::get_if<quests::SaveError>(&opened)) {
        if (*error == quests::SaveError::invalid)
            throw std::runtime_error("invalid Surveyors save; progress was not changed");
        if (*error == quests::SaveError::busy)
            throw std::runtime_error("Surveyors save is locked by another owner");
        throw std::runtime_error("cannot open private Surveyors save directory");
    }
    voyage_ = std::get<std::unique_ptr<quests::Voyage>>(std::move(opened));
    if (options.replay && voyage_->revision())
        throw std::runtime_error("Surveyors replay needs a new scratch save; existing progress can "
                                 "be opened interactively");
    // The existing checkpoint owns this berth already. The decorative
    // landmark may be unavailable; it must not disable flight or recovery.
    prepare();
    if (voyage_->security() == travel::Security::lawless)
        start_upload(path_);
}

std::optional<flight::Snapshot> SurveyFlight::local() const {
    const auto frame = voyage_->native();
    return frame ? frame->journey.local : std::nullopt;
}

Result<std::monostate> SurveyFlight::submit(flight::Intent intent) {
    return voyage_->submit(intent);
}

void SurveyFlight::prepare() {
    if (voyage_->snapshot().stage == Stage::lost)
        return;
    const auto target = voyage_->departure();
    const auto prepared = voyage_->prepare_local({target.origin, target.revision, contacts_});
    if (std::holds_alternative<Error>(prepared))
        throw std::runtime_error("Surveyors could not prepare the committed berth");
}

void SurveyFlight::start_upload(const std::filesystem::path &path) {
    pending_.emplace(ship_scene_);
    reader_.emplace(path);
    upload_failed_ = std::holds_alternative<Error>(reader_->request(1));
    message_ = upload_failed_ ? "Cannot read destination; retry the landmark"
                              : "Preparing the frontier landmark";
}

void SurveyFlight::action(Action choice) {
    if (uncertain_ || in_transit())
        return;
    last_command_ = {voyage_->revision(), choice};
    const auto outcome = voyage_->request(last_command_);
    receive(outcome);
    if (std::holds_alternative<quests::InTransit>(outcome)) {
        if (voyage_->security() == travel::Security::protected_space)
            start_upload(path_);
    } else if (choice == Action::recover && voyage_->snapshot().stage == Stage::recovered &&
               !local()) {
        prepare();
    }
}

void SurveyFlight::receive(const quests::VoyageOutcome &outcome) {
    if (std::holds_alternative<quests::Receipt>(outcome)) {
        uncertain_ = false;
        message_ = "Checkpoint saved";
    } else if (std::holds_alternative<quests::InTransit>(outcome)) {
        message_ = "Crossing: awaiting prepared berth";
    } else if (const auto *error = std::get_if<Error>(&outcome)) {
        message_ = *error == Error::invalid ? "Approach the berth in tactical mode and slow down"
                                            : "Berth or flight command not ready";
    } else if (const auto *save = std::get_if<quests::SaveError>(&outcome)) {
        uncertain_ = *save == quests::SaveError::uncertain;
        message_ = uncertain_ ? "Checkpoint sync uncertain; retry the exact command"
                              : "Could not save; the previous checkpoint is unchanged";
    } else {
        message_ = "Charter action rejected";
    }
}

void SurveyFlight::apply_action() {
    if (queued_) {
        const auto next_action = *queued_;
        queued_.reset();
        action(next_action);
    }
}

void SurveyFlight::tick() {
    if (!voyage_->native())
        return;
    const auto state = voyage_->advance();
    if (const auto *error = std::get_if<Error>(&state)) {
        if (*error != Error::invalid)
            throw std::runtime_error("Surveyors flight stopped; reopen the last checkpoint");
        message_ = "Combined flight intent rejected";
    }
}

void SurveyFlight::stream(render::Context &context) {
    if (pending_ && !upload_failed_) {
        if (reader_) {
            if (const auto result = reader_->poll()) {
                const auto *package = std::get_if<assets::PackagePin>(&result->package);
                upload_failed_ = !package || !destination_landmark(**package) ||
                                 pending_->begin(context, *package).has_value();
                reader_.reset();
            }
        }
        if (!reader_ && !upload_failed_)
            upload_failed_ = pending_->advance(context).has_value();
        if (upload_failed_)
            message_ = "Destination rejected; repair the landmark and retry";
    }
    const auto marker =
        pending_ && pending_->selected() != ship_scene_ ? pending_->selected() : render::ScenePin{};
    const auto target = voyage_->destination();
    if (!target && marker) {
        landmark_ = marker;
        pending_.reset();
        message_ = "Frontier landmark ready";
    }
    const auto state = voyage_->native();
    if (!target || !state || state->journey.phase != travel::Phase::exit)
        return;
    const bool outbound = voyage_->security() == travel::Security::protected_space;
    if (outbound && !marker) {
        ++exit_polls_;
        if (upload_failed_ && exit_polls_ >= 6 && !retry_path_.empty() && !upload_retried_) {
            upload_retried_ = true;
            start_upload(retry_path_);
        }
        return;
    }
    // The streamed landmark is decorative, never a collision source. Both
    // authored fixture berths use the checked stationary preset contacts.
    const travel::CollisionSite site{target->origin, target->revision, contacts_};
    const auto revision = voyage_->revision();
    const auto result = voyage_->arrive(&site);
    receive(result);
    if (in_transit()) {
        ++exit_polls_;
        return;
    }
    const bool published = voyage_->revision() != revision;
    if (published || outbound) {
        pending_.reset();
        reader_.reset();
        upload_failed_ = false;
    }
    if (published) {
        landmark_ = outbound ? marker : render::ScenePin{};
        ++arrivals_;
    } else {
        prepare(); // Native arrival happened, but the source checkpoint still owns the ship.
    }
}

const char *SurveyFlight::system() const {
    return voyage_->security() == travel::Security::lawless ? "Lawless frontier"
                                                            : "Protected starter";
}

void SurveyFlight::panel(bool replay) {
    const auto state = voyage_->snapshot();
    ImGui::SetNextWindowPos({8, 74});
    ImGui::SetNextWindowSize({290, 0});
    constexpr auto flags =
        static_cast<ImGuiWindowFlags>(static_cast<unsigned>(ImGuiWindowFlags_AlwaysAutoResize) |
                                      static_cast<unsigned>(ImGuiWindowFlags_NoSavedSettings) |
                                      static_cast<unsigned>(ImGuiWindowFlags_NoMove));
    if (ImGui::Begin("Surveyors charter", nullptr, flags)) {
        ImGui::Text("%s / checkpoint %u", system(), voyage_->revision());
        ImGui::Text("Vessel %llu / credits %llu", static_cast<unsigned long long>(state.vessel),
                    static_cast<unsigned long long>(state.credits));
        ImGui::Text("Hold %llu / wreck %llu / belt %llu",
                    static_cast<unsigned long long>(state.cargo),
                    static_cast<unsigned long long>(state.wreck),
                    static_cast<unsigned long long>(state.deposit));
        const auto frame = local();
        if (!frame)
            ImGui::TextUnformatted("Held view / no local vessel");
        ImGui::TextWrapped("%s", message_);
        ImGui::BeginDisabled(replay);
        if (upload_failed_ && ImGui::Button("Retry landmark"))
            start_upload(path_);
        if (uncertain_) {
            if (ImGui::Button("Retry checkpoint sync"))
                receive(voyage_->request(last_command_));
        } else if (!in_transit()) {
            if (frame && ImGui::Button("Approach berth")) {
                if (frame->mode != flight::Mode::tactical)
                    message_ = "Switch to tactical mode before approaching";
                else if (std::holds_alternative<Error>(submit(flight::Destination{{}})))
                    message_ = "Berth approach was not admitted";
                else
                    message_ = "Approaching the berth; wait for the ship to settle";
            }
            const auto button = [this](const char *text, Action choice) {
                if (ImGui::Button(text))
                    queued_ = choice; // Publish only at the next tick, not into the frozen frame.
            };
            switch (state.stage) {
            case Stage::training:
                ImGui::TextWrapped("Unprovoked attacks are unlawful in the starter system. "
                                   "This is a lesson, not live police enforcement.");
                button("Learn the rules", Action::learn_law);
                break;
            case Stage::enlistment:
                button("Join the Surveyors", Action::join_survey);
                break;
            case Stage::protected_port:
                if (state.deposit)
                    button("Accept 2-credit sortie", Action::accept_risk);
                else
                    ImGui::TextUnformatted("This charter's finite belt is exhausted.");
                break;
            case Stage::prospecting:
                button("Mine the fixture deposit", Action::mine);
                break;
            case Stage::encounter:
                button("Win fixture encounter", Action::defeat_rival);
                button("Lose ship in encounter", Action::lose_ship);
                break;
            case Stage::lost:
                button("Recover an empty starter", Action::recover);
                break;
            case Stage::recovered:
                button("Salvage the wreck", Action::salvage);
                break;
            case Stage::returning:
                button("Return and sell at station", Action::dock_and_sell);
                break;
            }
        }
        ImGui::EndDisabled();
    }
    ImGui::End();
}

void SurveyFlight::replay() {
    if (in_transit() || uncertain_)
        return;
    const auto state = voyage_->snapshot();
    switch (state.stage) {
    case Stage::training:
        queued_ = Action::learn_law;
        break;
    case Stage::enlistment:
        queued_ = Action::join_survey;
        break;
    case Stage::protected_port:
        if (state.completed_runs)
            break;
        if (!replay_out_) {
            replay_out_ =
                std::holds_alternative<std::monostate>(submit(flight::Destination{{0, 0, -20}}));
            break;
        }
        if (const auto frame = local();
            !replay_home_ && frame && frame->body.position_metres[2] < -8) {
            replay_home_ = std::holds_alternative<std::monostate>(submit(flight::Destination{{}}));
            break;
        }
        if (replay_home_)
            queued_ = Action::accept_risk;
        break;
    case Stage::prospecting:
        queued_ = Action::mine;
        break;
    case Stage::encounter:
        queued_ = Action::lose_ship;
        break;
    case Stage::lost:
        queued_ = Action::recover;
        break;
    case Stage::recovered:
        queued_ = Action::salvage;
        break;
    case Stage::returning:
        queued_ = Action::dock_and_sell;
        break;
    }
}

void SurveyFlight::report(bool replay, std::size_t landmark_frames) const {
    const auto state = voyage_->snapshot();
    std::printf(
        "survey_revision=%u survey_arrivals=%u survey_wait_polls=%u survey_retry=%u "
        "survey_landmark_frames=%zu survey_local=%u survey_system=%s "
        "survey_vessel=%llu survey_credits=%llu survey_cargo=%llu survey_wreck=%llu "
        "survey_stock=%llu survey_deposit=%llu survey_runs=%llu\n",
        voyage_->revision(), arrivals_, exit_polls_, upload_retried_ ? 1U : 0U, landmark_frames,
        local() ? 1U : 0U, system(), static_cast<unsigned long long>(state.vessel),
        static_cast<unsigned long long>(state.credits),
        static_cast<unsigned long long>(state.cargo), static_cast<unsigned long long>(state.wreck),
        static_cast<unsigned long long>(state.station_stock),
        static_cast<unsigned long long>(state.deposit),
        static_cast<unsigned long long>(state.completed_runs));
    if (replay && !(replay_home_ && arrivals_ == 2 && state.completed_runs == 1 &&
                    landmark_frames && (retry_path_.empty() || upload_retried_) && !uncertain_))
        throw std::runtime_error("Surveyors replay did not complete the loss and return charter");
}
} // namespace ngen::demo

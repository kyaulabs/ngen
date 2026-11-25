#include "flight_demo/survey.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <imgui.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace ngen::demo {
using quests::Action;
using quests::Stage;

SurveyFlight::Soak::~Soak() {
    if (root >= 0)
        close(root);
}

SurveyFlight::SurveyFlight(const Options &options, const Preset &preset, render::ScenePin scene)
    : path_(options.destination), retry_path_(options.destination_retry),
      directory_(options.survey), ship_(preset.ship.flight), contacts_(preset.contacts),
      ship_scene_(std::move(scene)) {
    if (options.soak_seconds) {
        const auto parent_path = directory_.parent_path();
        const auto name = directory_.filename();
        auto &soak = soak_.emplace();
        constexpr int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
        const int parent = ::open(parent_path.empty() ? "." : parent_path.c_str(), flags);
        if (parent < 0)
            throw std::runtime_error("cannot open B4 scratch parent");
        struct stat status = {};
        constexpr mode_t shared_write = S_IWGRP + S_IWOTH;
        if (fstat(parent, &status) == 0 && status.st_uid == geteuid() &&
            !(status.st_mode & shared_write) && mkdirat(parent, name.c_str(), 0700) == 0)
            soak.root = openat(parent, name.c_str(), flags);
        close(parent);
        if (soak.root < 0)
            throw std::runtime_error(
                "B4 scratch must be new under an owned, non-writable-by-others parent");
        next_charter();
        return;
    }
    reopen();
    if (options.replay && voyage_->revision())
        throw std::runtime_error("Surveyors replay needs a new scratch save; existing progress can "
                                 "be opened interactively");
    // The existing checkpoint owns this berth already. The decorative
    // landmark may be unavailable; it must not disable flight or recovery.
    prepare();
    if (voyage_->security() == travel::Security::lawless)
        start_upload(path_);
}

void SurveyFlight::reopen() {
    voyage_.reset();
    auto opened = quests::Voyage::open(directory_, ship_);
    if (const auto *error = std::get_if<quests::SaveError>(&opened)) {
        if (*error == quests::SaveError::invalid)
            throw std::runtime_error("invalid Surveyors save; progress was not changed");
        if (*error == quests::SaveError::busy)
            throw std::runtime_error("Surveyors save is locked by another owner");
        throw std::runtime_error("cannot open private Surveyors save directory");
    }
    voyage_ = std::get<std::unique_ptr<quests::Voyage>>(std::move(opened));
}

void SurveyFlight::next_charter() {
    if (!soak_)
        throw std::logic_error("B4 charter requires soak mode");
    auto &soak = *soak_;
    const auto name = "charter-" + std::to_string(++soak.charter);
    if (mkdirat(soak.root, name.c_str(), 0700) != 0)
        throw std::runtime_error("cannot create exclusive B4 charter directory");
    // The retained parent descriptor prevents renaming the scratch root from
    // redirecting later save opens to a different directory.
    directory_ = std::filesystem::path("/proc/self/fd") / std::to_string(soak.root) / name;
    reopen();
    prepare();
    replay_out_ = replay_home_ = false;
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
    const auto before = voyage_->snapshot();
    const auto outcome = voyage_->request(last_command_);
    if (soak_) {
        const bool armed = std::holds_alternative<quests::InTransit>(outcome);
        const auto *error = std::get_if<Error>(&outcome);
        // A scripted approach may reach the berth before it has settled.
        const bool settling = choice == Action::accept_risk && error && *error == Error::invalid;
        if (armed || settling) {
            if (voyage_->revision() != last_command_.expected || voyage_->snapshot() != before ||
                in_transit() != armed)
                throw std::runtime_error("B4 berth admission changed the committed checkpoint");
        } else {
            check_step(before, outcome);
        }
    }
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
        if (soak_ && upload_failed_)
            throw std::runtime_error("B4 landmark upload failed");
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
    const auto before = voyage_->snapshot();
    quests::SaveTimings timings;
    const auto result = voyage_->arrive(&site, soak_ && !outbound ? &timings : nullptr);
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
    } else if (!soak_) {
        prepare(); // Native arrival happened, but the source checkpoint still owns the ship.
    }
    if (auto *soak = soak_ ? &*soak_ : nullptr) {
        if (!outbound && !std::holds_alternative<quests::Receipt>(result))
            std::printf("b4_sale_failed,%llu,%llu\n",
                        static_cast<unsigned long long>(timings.market_ns.value_or(0)),
                        static_cast<unsigned long long>(timings.persistence_ns.value_or(0)));
        check_step(before, result);
        if (outbound)
            soak->departure = Issued{last_command_, std::get<quests::Receipt>(result)};
        else
            record_sale(result, timings);
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
        if (!soak_ && state.completed_runs)
            break;
        if (soak_ && !state.deposit) {
            next_charter();
            break;
        }
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
        queued_ =
            soak_ && (state.completed_runs % 2 == 1) ? Action::defeat_rival : Action::lose_ship;
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

void SurveyFlight::check_step(const quests::State &before, const quests::VoyageOutcome &outcome) {
    const auto *receipt = std::get_if<quests::Receipt>(&outcome);
    if (!soak_ || !receipt || receipt->revision != voyage_->revision())
        throw std::runtime_error("B4 fixture action did not publish a receipt");
    const auto &after = receipt->after;
    const auto action = last_command_.action;
    if (after.credits + after.station_credits != 130 ||
        after.deposit + after.cargo + after.wreck + after.station_stock != 12 ||
        after.vessel != before.vessel + (action == Action::recover ? 1U : 0U) ||
        (action == Action::lose_ship && (after.cargo || after.wreck != before.cargo)) ||
        (action == Action::recover &&
         (after.cargo || after.credits != before.credits || after.wreck != before.wreck)) ||
        (action == Action::salvage && (after.wreck || after.cargo != before.wreck)))
        throw std::runtime_error("B4 fixture violated ownership or conservation");
    if (action == Action::defeat_rival)
        ++soak_->wins;
    if (action == Action::lose_ship)
        ++soak_->losses;
}

void SurveyFlight::record_sale(const quests::VoyageOutcome &outcome,
                               const quests::SaveTimings &timings) {
    const auto sold = std::get<quests::Receipt>(outcome);
    if (!soak_ || !soak_->departure || !timings.market_ns || !timings.persistence_ns)
        throw std::runtime_error("B4 sale lacks a departure receipt or separate timings");
    auto &soak = *soak_;
    const auto outbound = *soak.departure;
    soak.departure.reset();
    const auto stable_checkpoint = [this, &sold] {
        return voyage_->revision() == sold.revision && voyage_->snapshot() == sold.after &&
               voyage_->security() == travel::Security::protected_space && !voyage_->destination();
    };
    const auto verify_retry = [this, &stable_checkpoint](const Issued &issued) {
        const auto retried = voyage_->request(issued.command);
        const auto *receipt = std::get_if<quests::Receipt>(&retried);
        if (!receipt || *receipt != issued.receipt || !stable_checkpoint())
            throw std::runtime_error("B4 retry did not return its historical receipt");
    };
    const auto elapsed_ms =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - soak.start)
                                       .count());
    if (elapsed_ms - soak.last_sale_ms > 45'000)
        throw std::runtime_error("B4 sale gap exceeded 45 seconds");
    soak.last_sale_ms = elapsed_ms;
    ++soak.sales;
    std::printf("b4_sale,%u,%u,%u,%llu,%llu,%llu\n", soak.sales, soak.charter, sold.revision,
                static_cast<unsigned long long>(elapsed_ms),
                static_cast<unsigned long long>(timings.market_ns.value_or(0)),
                static_cast<unsigned long long>(timings.persistence_ns.value_or(0)));
    reopen();
    if (!stable_checkpoint())
        throw std::runtime_error("B4 save did not restore the committed sale");
    verify_retry(outbound);
    verify_retry({last_command_, sold});
    const auto conflict = voyage_->request({last_command_.expected, Action::mine});
    if (const auto *reason = std::get_if<quests::Rejection>(&conflict);
        !reason || *reason != quests::Rejection::conflict || !stable_checkpoint())
        throw std::runtime_error("B4 changed retry modified the sale");
    ++soak.reopens;
    if (!sold.after.deposit) {
        const auto exhausted = voyage_->request({sold.revision, Action::accept_risk});
        if (const auto *reason = std::get_if<quests::Rejection>(&exhausted);
            !reason || *reason != quests::Rejection::exhausted || sold.after.completed_runs != 4 ||
            sold.after.station_stock != 12 || sold.after.credits != 66 || sold.after.vessel != 3 ||
            !stable_checkpoint())
            throw std::runtime_error("B4 exhausted charter did not reject a new sortie");
        ++soak.exhausted;
    }
    prepare();
    replay_out_ = replay_home_ = false;
}

void SurveyFlight::begin_soak(std::chrono::steady_clock::time_point start) {
    if (soak_)
        soak_->start = start;
}

void SurveyFlight::sample_soak(std::uint64_t elapsed_ms, const render::ResourceStatistics &memory) {
    if (!soak_ || (soak_->samples && elapsed_ms / 1000 <= soak_->last_sample_ms / 1000))
        return;
    auto &soak = *soak_;
    soak.last_sample_ms = elapsed_ms;
    if (elapsed_ms - soak.last_sale_ms > 45'000)
        throw std::runtime_error("B4 made no sale progress for 45 seconds");
    std::ifstream statm("/proc/self/statm");
    std::uint64_t virtual_pages{}, resident_pages{};
    const auto page_size = sysconf(_SC_PAGESIZE);
    if (!(statm >> virtual_pages >> resident_pages) || resident_pages == 0 || page_size <= 0)
        throw std::runtime_error("B4 process RSS is unavailable");
    const bool settled = !in_transit() && local() && !pending_ && !landmark_ &&
                         voyage_->security() == travel::Security::protected_space &&
                         memory.retiring == 0;
    std::printf(
        "b4_mem,%llu,%llu,%u,%u,%llu,%llu,%u\n", static_cast<unsigned long long>(elapsed_ms),
        static_cast<unsigned long long>(resident_pages) *
            static_cast<unsigned long long>(page_size),
        memory.live, memory.retiring, static_cast<unsigned long long>(memory.allocation_bytes),
        static_cast<unsigned long long>(memory.reserved_bytes), settled ? 1U : 0U);
    std::fflush(stdout);
    ++soak.samples;
}

void SurveyFlight::sample_frame(FrameTiming timing) {
    if (!soak_)
        return;
    auto &soak = *soak_;
    const auto [frame_ns, queue_ns] = timing;
    if (queue_ns <= soak.last_queue_ns)
        throw std::runtime_error("B4 frame timing did not observe a new queued frame");
    if (!soak.first_queue_ns)
        soak.first_queue_ns = queue_ns;
    constexpr std::uint64_t bucket_ns = 100'000;
    const auto bucket =
        std::min<std::uint64_t>((frame_ns + bucket_ns - 1) / bucket_ns, soak.frames.size() - 1);
    ++soak.frames[bucket];
    ++soak.frame_count;
    soak.max_frame_ns = std::max(soak.max_frame_ns, frame_ns);
    if (soak.last_queue_ns) {
        const auto gap = queue_ns - soak.last_queue_ns;
        soak.max_queue_gap_ns = std::max(soak.max_queue_gap_ns, gap);
        if (gap > 200'000'000)
            std::printf("flight_soak_gap frame=%llu gap_ns=%llu charter=%u revision=%u phase=%u\n",
                        static_cast<unsigned long long>(soak.frame_count),
                        static_cast<unsigned long long>(gap), soak.charter, voyage_->revision(),
                        static_cast<unsigned>(voyage_->snapshot().stage));
    }
    soak.last_queue_ns = queue_ns;
}

void SurveyFlight::finish_soak(std::uint32_t seconds, std::uint64_t elapsed_ms) const {
    if (!soak_)
        return;
    const auto &soak = *soak_;
    for (std::size_t bin = 0; bin < soak.frames.size(); ++bin)
        if (soak.frames[bin])
            std::printf("b4_frame,%zu,%llu\n", bin,
                        static_cast<unsigned long long>(soak.frames[bin]));
    const auto epoch_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(soak.start.time_since_epoch())
            .count());
    const auto queue_ms = [epoch_ns](std::uint64_t queue) {
        return static_cast<unsigned long long>(queue > epoch_ns ? (queue - epoch_ns) / 1'000'000
                                                                : 0);
    };
    std::printf("b4_frame_end,%llu,%llu,%llu,%llu,%llu\n",
                static_cast<unsigned long long>(soak.frame_count),
                static_cast<unsigned long long>(soak.max_frame_ns),
                static_cast<unsigned long long>(soak.max_queue_gap_ns),
                queue_ms(soak.first_queue_ns), queue_ms(soak.last_queue_ns));
    std::printf("b4_end,%llu,%u,%u,%u,%u,%u,%u\n", static_cast<unsigned long long>(elapsed_ms),
                soak.exhausted, soak.sales, soak.wins, soak.losses, soak.reopens, soak.samples);
    if (elapsed_ms < static_cast<std::uint64_t>(seconds) * 1000 || !soak.exhausted ||
        elapsed_ms - soak.last_sale_ms > 45'000 || soak.sales != soak.reopens || !soak.losses ||
        !soak.wins)
        throw std::runtime_error("B4 elapsed time, charter or replay acceptance was incomplete");
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

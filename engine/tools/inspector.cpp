#include "tools/inspector.hpp"
#include <algorithm>
#include <cmath>

namespace ngen::tools {
void Inspector::install(assets::PackagePin package) {
    state_.package = std::move(package);
    state_.selection.reset();
    state_.materials = {};
    state_.material_count = static_cast<std::uint32_t>(state_.package->materials.size());
    for (std::size_t i = 0; i < state_.material_count; ++i) {
        const auto &material = state_.package->materials[i];
        state_.materials[i] = {material.base_color, material.metallic, material.roughness,
                               material.emissive_strength};
    }
}
Result<Inspector> Inspector::create(assets::PackagePin package) {
    if (!package || package->materials.size() > 256)
        return Error::invalid;
    Inspector result;
    result.install(std::move(package));
    return result;
}
Result<std::monostate> Inspector::enqueue(InspectorCommand command) {
    if (std::visit([](const auto &value) { return value.generation; }, command) !=
        state_.generation)
        return Error::stale;
    if (const auto *selection = std::get_if<Select>(&command))
        if (selection->instance && *selection->instance >= state_.package->instances.size())
            return Error::invalid;
    if (const auto *edit = std::get_if<EditMaterial>(&command)) {
        const auto unit = [](float value) {
            return std::isfinite(value) && value >= 0 && value <= 1;
        };
        const auto &factors = edit->factors;
        if (edit->material >= state_.material_count ||
            !std::all_of(factors.base_color.begin(), factors.base_color.end(), unit) ||
            !unit(factors.metallic) || !unit(factors.roughness) ||
            !std::isfinite(factors.emissive_strength) || factors.emissive_strength < 0 ||
            factors.emissive_strength > 64)
            return Error::invalid;
    }
    if (const auto *view = std::get_if<SetView>(&command))
        if (!std::isfinite(view->settings.exposure) || view->settings.exposure < 0 ||
            view->settings.exposure > 64)
            return Error::invalid;
    if (size_ == commands_.size())
        return Error::capacity;
    commands_[size_++] = command;
    return std::monostate{};
}
Result<std::monostate> Inspector::stage_package(assets::PackagePin package) {
    if (!package || package->materials.size() > state_.materials.size())
        return Error::invalid;
    if (pending_)
        return Error::busy;
    const auto generation = next_generation(state_.generation);
    if (const auto *error = std::get_if<Error>(&generation))
        return *error;
    pending_ = std::move(package);
    return std::monostate{};
}
InspectorAdvance Inspector::advance() {
    InspectorAdvance result;
    if (pending_) {
        install(std::move(pending_));
        ++state_.generation; // stage_package validated the sole pending generation.
        result.published = true;
    }
    for (std::size_t i = 0; i < size_; ++i) {
        if (std::visit([](const auto &value) { return value.generation; }, commands_[i]) !=
            state_.generation) {
            ++result.rejected;
            continue;
        }
        if (const auto *selection = std::get_if<Select>(&commands_[i])) {
            state_.selection = selection->instance;
            ++result.applied;
        } else if (const auto *edit = std::get_if<EditMaterial>(&commands_[i])) {
            state_.materials[edit->material] = edit->factors;
            ++result.applied;
        } else if (const auto *view = std::get_if<SetView>(&commands_[i])) {
            state_.view = view->settings;
            ++result.applied;
        } else if (std::holds_alternative<Reload>(commands_[i]) &&
                   state_.reload_request != UINT64_MAX) {
            ++state_.reload_request;
            ++result.applied;
        } else {
            ++result.rejected;
        }
    }
    size_ = 0;
    return result;
}
} // namespace ngen::tools

#pragma once
#include "assets/package.hpp"
#include "core/values.hpp"
#include <optional>

namespace ngen::tools {
using MaterialFactors = assets::MaterialFactors;
struct ViewSettings {
    float exposure{1};
    bool fxaa{true};
    float bloom{.04F};
};
struct InspectorSnapshot {
    assets::PackagePin package;
    std::uint64_t generation{1}, reload_request{};
    std::optional<std::uint32_t> selection;
    std::array<MaterialFactors, 256> materials{};
    std::uint32_t material_count{};
    ViewSettings view;
};
struct Select {
    std::uint64_t generation{};
    std::optional<std::uint32_t> instance;
};
struct EditMaterial {
    std::uint64_t generation{};
    std::uint32_t material{};
    MaterialFactors factors;
};
struct SetView {
    std::uint64_t generation{};
    ViewSettings settings;
};
struct Reload {
    std::uint64_t generation{};
};
using InspectorCommand = std::variant<Select, EditMaterial, SetView, Reload>;
struct InspectorAdvance {
    std::size_t applied{}, rejected{};
    bool published{};
};
// Owner-thread development state. Commands become visible at advance(), never
// through direct writes to a checked package or an already-copied frame snapshot.
class Inspector {
  public:
    static constexpr std::size_t command_capacity = 16;
    [[nodiscard]] static Result<Inspector> create(assets::PackagePin package);
    Inspector(Inspector &&) noexcept = default;
    Inspector &operator=(Inspector &&) noexcept = default;
    Inspector(const Inspector &) = delete;
    Inspector &operator=(const Inspector &) = delete;
    [[nodiscard]] InspectorSnapshot snapshot() const { return state_; }
    [[nodiscard]] Result<std::monostate> enqueue(InspectorCommand command);
    // Stage only a successfully prepared package; publication occurs at advance.
    [[nodiscard]] Result<std::monostate> stage_package(assets::PackagePin package);
    [[nodiscard]] InspectorAdvance advance();
    [[nodiscard]] std::size_t pending_commands() const { return size_; }

  private:
    Inspector() = default;
    void install(assets::PackagePin package);
    InspectorSnapshot state_;
    assets::PackagePin pending_;
    std::array<InspectorCommand, command_capacity> commands_{};
    std::size_t size_{};
};
} // namespace ngen::tools

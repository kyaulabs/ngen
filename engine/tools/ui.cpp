#include "tools/ui.hpp"
#include "tools/gui.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <stdexcept>

namespace ngen::tools {
static_assert(IM_COL32_R_SHIFT == 0 && IM_COL32_G_SHIFT == 8 && IM_COL32_B_SHIFT == 16 &&
              IM_COL32_A_SHIFT == 24);
namespace {
template <class T> T take(std::variant<T, render::Failure> value) {
    if (const auto *failure = std::get_if<render::Failure>(&value))
        throw std::runtime_error(failure->message);
    return std::get<T>(std::move(value));
}
constexpr auto panel_flags =
    static_cast<ImGuiWindowFlags>(static_cast<unsigned>(ImGuiWindowFlags_NoDecoration) |
                                  static_cast<unsigned>(ImGuiWindowFlags_NoMove) |
                                  static_cast<unsigned>(ImGuiWindowFlags_NoSavedSettings));
} // namespace
struct InspectorUi::Impl {
    std::unique_ptr<Gui> gui;
    bool show_bounds{true};
    const char *issue{};
    void initialize(render::Context &owner) { gui = take(Gui::create(owner)); }
    void queue(Inspector &inspector, InspectorCommand command) {
        const auto result = inspector.enqueue(command);
        issue = nullptr;
        if (const auto *error = std::get_if<Error>(&result)) {
            switch (*error) {
            case Error::invalid:
                issue = "Command rejected: invalid value";
                break;
            case Error::overflow:
                issue = "Command rejected: counter exhausted";
                break;
            case Error::capacity:
                issue = "Command rejected: queue full";
                break;
            case Error::stale:
                issue = "Command rejected: stale generation";
                break;
            case Error::order:
                issue = "Command rejected: invalid order";
                break;
            case Error::closed:
                issue = "Command rejected: owner closed";
                break;
            case Error::busy:
                issue = "Command rejected: busy";
                break;
            }
        }
    }
    void bounds(const InspectorSnapshot &snapshot, const render::SceneView &view) const {
        if (!show_bounds)
            return;
        // A material/part selection is not the assembled hull. Package bounds
        // already include every authored instance's transform, including engines.
        const auto &box = snapshot.package->bounds;
        const auto display = ImGui::GetIO().DisplaySize;
        std::array<ImVec2, 8> points{};
        std::array<bool, 8> visible{};
        for (unsigned corner = 0; corner < 8; ++corner) {
            std::array<double, 4> world{0, 0, 0, 1}, clip{};
            for (std::size_t axis = 0; axis < 3; ++axis)
                world[axis] = (corner & (1U << axis)) ? box.maximum[axis] : box.minimum[axis];
            for (std::size_t row = 0; row < 4; ++row)
                for (std::size_t column = 0; column < 4; ++column)
                    clip[row] +=
                        static_cast<double>(view.view_projection.columns[column * 4 + row]) *
                        world[column];
            visible[corner] = clip[3] > 0 && clip[2] >= 0 && clip[2] <= clip[3];
            if (visible[corner])
                points[corner] = {static_cast<float>(clip[0] / clip[3] * .5 + .5) * display.x,
                                  static_cast<float>(clip[1] / clip[3] * .5 + .5) * display.y};
        }
        auto *draw = ImGui::GetBackgroundDrawList();
        for (unsigned corner = 0; corner < 8; ++corner)
            for (unsigned axis = 0; axis < 3; ++axis) {
                const auto other = corner ^ (1U << axis);
                if (corner < other && visible[corner] && visible[other])
                    draw->AddLine(points[corner], points[other], IM_COL32(130, 196, 207, 190));
            }
    }
};
InspectorUi::InspectorUi() : impl_(std::make_unique<Impl>()) {}
InspectorUi::~InspectorUi() = default;
std::variant<std::unique_ptr<InspectorUi>, render::Failure>
InspectorUi::create(render::Context &context) {
    try {
        auto ui = std::unique_ptr<InspectorUi>(new InspectorUi);
        ui->impl_->initialize(context);
        return ui;
    } catch (const std::exception &error) {
        return render::Failure{error.what()};
    }
}
void InspectorUi::event(const SDL_Event &event) { impl_->gui->event(event); }
std::uint64_t InspectorUi::allocation_count() const noexcept {
    return impl_->gui->allocation_count();
}
void InspectorUi::frame(Inspector &inspector, const InspectorSnapshot &snapshot,
                        const render::SceneView &view, const InspectorMetrics &metrics) {
    impl_->gui->begin();
    const auto display = ImGui::GetIO().DisplaySize;
    const auto width = std::max(640.0F, display.x), height = std::max(360.0F, display.y);
    constexpr float gutter = 16;
    ImGui::SetNextWindowPos({gutter, 8});
    ImGui::SetNextWindowSize({width - 2 * gutter, 40});
    if (ImGui::Begin("NGEN header", nullptr, panel_flags)) {
        ImGui::TextUnformatted("NGEN  /  SCENE INSPECTOR");
        ImGui::SameLine(360);
        ImGui::Text("LOD 2  |  %zu ship draws + 100 instances  |  frame %llu",
                    snapshot.package->instances.size(),
                    static_cast<unsigned long long>(metrics.frame));
    }
    ImGui::End();
    ImGui::SetNextWindowPos({gutter, 80});
    ImGui::SetNextWindowSize({250, 260});
    if (ImGui::Begin("SCENE", nullptr, panel_flags)) {
        ImGui::TextUnformatted("SCENE / SHIP");
        ImGui::TextDisabled("%zu primitives / %u materials", snapshot.package->primitives.size(),
                            snapshot.material_count);
        if (ImGui::BeginChild("instances", {0, 150}, ImGuiChildFlags_Borders)) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(snapshot.package->instances.size()));
            while (clipper.Step())
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    char label[64]{};
                    (void)std::snprintf(
                        label, sizeof(label), "instance %03u / primitive %u",
                        static_cast<unsigned>(i),
                        snapshot.package->instances[static_cast<std::size_t>(i)].primitive);
                    if (ImGui::Selectable(label,
                                          snapshot.selection == static_cast<std::uint32_t>(i)))
                        impl_->queue(inspector,
                                     Select{snapshot.generation, static_cast<std::uint32_t>(i)});
                }
        }
        ImGui::EndChild();
        ImGui::TextDisabled("ASTEROIDS / 100 SHARED INSTANCES");
        (void)ImGui::Checkbox("Ship bounds", &impl_->show_bounds);
    }
    ImGui::End();
    ImGui::SetNextWindowPos({width - 308 - gutter, 80});
    ImGui::SetNextWindowSize({308, height - 160});
    if (ImGui::Begin("INSPECTOR", nullptr, panel_flags)) {
        ImGui::TextUnformatted("INSPECTOR / NEXT-FRAME EDITS");
        if (ImGui::BeginTabBar("inspector tabs")) {
            if (ImGui::BeginTabItem("Material")) {
                auto material = UINT32_MAX;
                if (snapshot.selection)
                    material =
                        snapshot.package
                            ->primitives[snapshot.package->instances[*snapshot.selection].primitive]
                            .material;
                if (material < snapshot.material_count) {
                    const auto &name = snapshot.package->materials[material].name;
                    ImGui::Text("MATERIAL %u / %u", material + 1, snapshot.material_count);
                    ImGui::TextUnformatted(name.data(),
                                           name.data() + std::min(name.size(), std::size_t{96}));
                    auto factors = snapshot.materials[material];
                    bool changed = ImGui::ColorEdit4("Base", factors.base_color.data(),
                                                     ImGuiColorEditFlags_NoInputs);
                    changed =
                        ImGui::SliderFloat("Metallic", &factors.metallic, 0, 1, "%.3f") || changed;
                    changed = ImGui::SliderFloat("Roughness", &factors.roughness, 0, 1, "%.3f") ||
                              changed;
                    changed =
                        ImGui::SliderFloat("Emissive", &factors.emissive_strength, 0, 64, "%.2f") ||
                        changed;
                    if (changed)
                        impl_->queue(inspector,
                                     EditMaterial{snapshot.generation, material, factors});
                    ImGui::TextDisabled("Textures, UVs, alpha mode and facing\nremain authored. "
                                        "Edits apply next frame.");
                } else {
                    ImGui::TextWrapped("Select a ship instance with an authored material.");
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Render")) {
                auto settings = snapshot.view;
                bool changed = ImGui::SliderFloat("Exposure", &settings.exposure, 0, 4, "%.2f");
                changed = ImGui::Checkbox("FXAA", &settings.fxaa) || changed;
                changed = ImGui::SliderFloat("Bloom", &settings.bloom, 0, .1F, "%.3f") || changed;
                if (changed)
                    impl_->queue(inspector, SetView{snapshot.generation, settings});
                ImGui::TextDisabled("1024x1024 directional shadow\nOpaque / mask / sorted "
                                    "blend\nHDR / fused bloom + tone / FXAA / UI");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Asset")) {
                ImGui::Text(
                    "Vertices: %zu\nIndices: %zu\nLight instances: %zu\nPackage CRC32: %08x",
                    snapshot.package->vertices.size(), snapshot.package->indices.size(),
                    snapshot.package->light_instances.size(), snapshot.package->checksum);
                ImGui::TextDisabled("CRC32 detects corruption, not identity.\nImmutable CPU/GPU "
                                    "versions stay pinned.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Separator();
        ImGui::BeginDisabled(metrics.reloading);
        if (ImGui::Button("Reload asset [R]", {-1, 30}))
            impl_->queue(inspector, Reload{snapshot.generation});
        ImGui::EndDisabled();
        if (metrics.reloading)
            ImGui::TextColored({.88F, .71F, .49F, 1}, "Loading / preparing next version...");
        if (impl_->issue)
            ImGui::TextColored({.88F, .71F, .49F, 1}, "%s", impl_->issue);
        if (!metrics.status.empty()) {
            ImGui::PushTextWrapPos();
            const auto *first = metrics.status.data();
            const auto *last = first + std::min(metrics.status.size(), std::size_t{128});
            ImGui::TextUnformatted(first, last);
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
    ImGui::SetNextWindowPos({gutter, height - 70});
    ImGui::SetNextWindowSize({width - 2 * gutter, 54});
    if (ImGui::Begin("DIAGNOSTICS", nullptr, panel_flags)) {
        ImGui::Text("FRAME %.2f ms | GPU shadow %.2f / PBR %.2f / post+bloom %.2f / UI %.2f ms",
                    metrics.frame_ms, metrics.gpu.shadow_ns / 1e6, metrics.gpu.pbr_ns / 1e6,
                    metrics.gpu.post_ns / 1e6, metrics.gpu.ui_ns / 1e6);
        ImGui::Text("VMA %.1f MiB (not whole GPU) | queue %zu/16 | generation %llu",
                    static_cast<double>(metrics.memory.allocation_bytes) / (1024 * 1024),
                    inspector.pending_commands(),
                    static_cast<unsigned long long>(snapshot.generation));
    }
    ImGui::End();
    if (!ImGui::GetIO().WantTextInput && !metrics.reloading &&
        ImGui::IsKeyPressed(ImGuiKey_R, false))
        impl_->queue(inspector, Reload{snapshot.generation});
    impl_->bounds(snapshot, view);
    impl_->gui->end();
}
std::variant<std::optional<render::Overlay>, render::Failure>
InspectorUi::overlay(VkExtent2D extent) {
    return impl_->gui->overlay(extent);
}
} // namespace ngen::tools

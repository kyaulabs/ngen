#include "tools/ui.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <stdexcept>

namespace ngen::tools {
static_assert(IM_COL32_R_SHIFT == 0 && IM_COL32_G_SHIFT == 8 && IM_COL32_B_SHIFT == 16 &&
              IM_COL32_A_SHIFT == 24);
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T take(std::variant<T, render::Failure> value) {
    if (const auto *failure = std::get_if<render::Failure>(&value))
        throw std::runtime_error(failure->message);
    return std::get<T>(std::move(value));
}
void wait(render::Context &context, const render::Submission &submission) {
    const auto start = SDL_GetTicks();
    while (!take(context.submission_complete(submission))) {
        require(SDL_GetTicks() - start < 5000, "inspector font upload timed out");
        SDL_Delay(1);
    }
}
constexpr auto panel_flags =
    static_cast<ImGuiWindowFlags>(static_cast<unsigned>(ImGuiWindowFlags_NoDecoration) |
                                  static_cast<unsigned>(ImGuiWindowFlags_NoMove) |
                                  static_cast<unsigned>(ImGuiWindowFlags_NoSavedSettings));
} // namespace
struct InspectorUi::Impl {
    ImGuiContext *context{};
    ImGuiMemAllocFunc previous_allocate{};
    ImGuiMemFreeFunc previous_free{};
    void *previous_user{};
    bool allocators_set{}, platform{}, show_bounds{true};
    std::uint64_t allocations{};
    const char *issue{};
    render::Image font;
    std::array<render::shader::OverlayVertex, render::overlay_vertex_limit> vertices{};
    std::array<std::uint32_t, render::overlay_index_limit> indices{};
    std::array<render::OverlayDraw, render::overlay_draw_limit> draws{};
    std::size_t vertex_count{}, index_count{}, draw_count{};
    static void *allocate(std::size_t size, void *user) {
        ++static_cast<Impl *>(user)->allocations;
        return std::malloc(size);
    }
    static void free(void *pointer, void *) { std::free(pointer); }
    ~Impl() {
        if (context) {
            ImGui::SetCurrentContext(context);
            if (platform)
                ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(context);
        }
        if (allocators_set)
            ImGui::SetAllocatorFunctions(previous_allocate, previous_free, previous_user);
    }
    void initialize(render::Context &owner) {
        require(ImGui::GetCurrentContext() == nullptr,
                "inspector requires an isolated ImGui context");
        ImGui::GetAllocatorFunctions(&previous_allocate, &previous_free, &previous_user);
        ImGui::SetAllocatorFunctions(allocate, free, this);
        allocators_set = true;
        context = ImGui::CreateContext();
        require(context != nullptr, "inspector UI context creation failed");
        auto &io = ImGui::GetIO();
        io.IniFilename = io.LogFilename = nullptr;
        io.ConfigFlags = static_cast<ImGuiConfigFlags>(
            static_cast<unsigned>(io.ConfigFlags) |
            static_cast<unsigned>(ImGuiConfigFlags_NavEnableKeyboard));
        io.BackendRendererName = "ngen-bounded-overlay";
        io.Fonts->AddFontDefaultBitmap();
        platform = ImGui_ImplSDL3_InitForVulkan(owner.window());
        require(platform, "inspector SDL platform initialization failed");
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);
        ImGui::StyleColorsDark();
        auto &style = ImGui::GetStyle();
        style.WindowRounding = style.FrameRounding = 2;
        style.WindowBorderSize = 1;
        style.WindowPadding = {12, 10};
        style.FramePadding = {6, 4};
        style.ItemSpacing = {6, 6};
        style.Colors[ImGuiCol_Text] = {.80F, .85F, .89F, 1};
        style.Colors[ImGuiCol_TextDisabled] = {.58F, .65F, .70F, 1};
        style.Colors[ImGuiCol_WindowBg] = {.067F, .098F, .137F, .97F};
        style.Colors[ImGuiCol_Border] = {.20F, .26F, .31F, 1};
        style.Colors[ImGuiCol_FrameBg] = {.11F, .16F, .20F, 1};
        style.Colors[ImGuiCol_Header] = {.13F, .21F, .25F, 1};
        style.Colors[ImGuiCol_HeaderHovered] = {.18F, .29F, .33F, 1};
        style.Colors[ImGuiCol_HeaderActive] = {.22F, .34F, .39F, 1};
        style.Colors[ImGuiCol_Tab] = {.09F, .14F, .18F, 1};
        style.Colors[ImGuiCol_TabSelected] = {.13F, .21F, .25F, 1};
        style.Colors[ImGuiCol_TabHovered] = {.20F, .32F, .37F, 1};
        style.Colors[ImGuiCol_TabSelectedOverline] = {.50F, .75F, .79F, 1};
        style.Colors[ImGuiCol_FrameBgHovered] = {.16F, .25F, .29F, 1};
        style.Colors[ImGuiCol_FrameBgActive] = {.22F, .34F, .39F, 1};
        style.Colors[ImGuiCol_Button] = {.12F, .19F, .23F, 1};
        style.Colors[ImGuiCol_ButtonHovered] = {.20F, .32F, .37F, 1};
        style.Colors[ImGuiCol_ButtonActive] = {.26F, .40F, .44F, 1};
        style.Colors[ImGuiCol_CheckMark] =
            style.Colors[ImGuiCol_SliderGrab] = {.50F, .75F, .79F, 1};
        style.Colors[ImGuiCol_NavCursor] = {.65F, .90F, .94F, 1};
        unsigned char *pixels{};
        int width = 0, height = 0, channels = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &channels);
        require(pixels && width > 0 && height > 0 && width <= 2048 && height <= 2048 &&
                    channels == 4,
                "inspector atlas exceeds bounded RGBA8 profile");
        const auto length =
            std::size_t{static_cast<unsigned>(width)} * static_cast<unsigned>(height) * 4;
        font =
            take(owner.create_image({{static_cast<unsigned>(width), static_cast<unsigned>(height)},
                                     VK_FORMAT_R8G8B8A8_UNORM}));
        auto staging = take(owner.create_buffer(
            {length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, render::MemoryAccess::upload}));
        (void)take(staging.write(0, {reinterpret_cast<const std::byte *>(pixels), length}));
        wait(owner, take(owner.copy_to_image({staging, font})));
        io.Fonts->SetTexID(ImTextureID{1});
    }
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
void InspectorUi::event(const SDL_Event &event) {
    ImGui::SetCurrentContext(impl_->context);
    (void)ImGui_ImplSDL3_ProcessEvent(&event);
}
std::uint64_t InspectorUi::allocation_count() const noexcept { return impl_->allocations; }
void InspectorUi::frame(Inspector &inspector, const InspectorSnapshot &snapshot,
                        const render::SceneView &view, const InspectorMetrics &metrics) {
    ImGui::SetCurrentContext(impl_->context);
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
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
    ImGui::Render();
}
std::variant<std::optional<render::Overlay>, render::Failure>
InspectorUi::overlay(VkExtent2D extent) {
    ImGui::SetCurrentContext(impl_->context);
    const auto *data = ImGui::GetDrawData();
    if (!data || data->TotalVtxCount == 0 || extent.width == 0 || extent.height == 0)
        return std::optional<render::Overlay>{};
    if (extent.width > 8192 || extent.height > 8192 || data->TotalVtxCount < 0 ||
        data->TotalIdxCount < 0 ||
        data->TotalVtxCount > static_cast<int>(render::overlay_vertex_limit) ||
        data->TotalIdxCount > static_cast<int>(render::overlay_index_limit) ||
        data->DisplaySize.x <= 0 || data->DisplaySize.y <= 0)
        return render::Failure{"inspector draw data exceeds the bounded overlay profile"};
    impl_->vertex_count = impl_->index_count = impl_->draw_count = 0;
    const auto x_scale = static_cast<float>(extent.width) / data->DisplaySize.x;
    const auto y_scale = static_cast<float>(extent.height) / data->DisplaySize.y;
    for (int list_index = 0; list_index < data->CmdListsCount; ++list_index) {
        const auto *list = data->CmdLists[list_index];
        const auto vertex_base = impl_->vertex_count, index_base = impl_->index_count;
        if (list->VtxBuffer.Size < 0 || list->IdxBuffer.Size < 0 ||
            static_cast<std::size_t>(list->VtxBuffer.Size) > impl_->vertices.size() - vertex_base ||
            static_cast<std::size_t>(list->IdxBuffer.Size) > impl_->indices.size() - index_base)
            return render::Failure{"inspector lists exceed bounded storage"};
        for (const auto &vertex : list->VtxBuffer)
            impl_->vertices[impl_->vertex_count++] = {
                {(vertex.pos.x - data->DisplayPos.x) * x_scale,
                 (vertex.pos.y - data->DisplayPos.y) * y_scale},
                {vertex.uv.x, vertex.uv.y},
                vertex.col};
        for (const auto index : list->IdxBuffer)
            impl_->indices[impl_->index_count++] = static_cast<std::uint32_t>(vertex_base) + index;
        for (const auto &command : list->CmdBuffer) {
            if (command.UserCallback || command.VtxOffset || command.GetTexID() != ImTextureID{1})
                return render::Failure{"inspector supports only its copied atlas geometry"};
            const auto left =
                std::clamp(std::floor((command.ClipRect.x - data->DisplayPos.x) * x_scale), 0.0F,
                           static_cast<float>(extent.width));
            const auto top =
                std::clamp(std::floor((command.ClipRect.y - data->DisplayPos.y) * y_scale), 0.0F,
                           static_cast<float>(extent.height));
            const auto right =
                std::clamp(std::ceil((command.ClipRect.z - data->DisplayPos.x) * x_scale), 0.0F,
                           static_cast<float>(extent.width));
            const auto bottom =
                std::clamp(std::ceil((command.ClipRect.w - data->DisplayPos.y) * y_scale), 0.0F,
                           static_cast<float>(extent.height));
            if (right <= left || bottom <= top || command.ElemCount == 0)
                continue;
            if (impl_->draw_count == impl_->draws.size())
                return render::Failure{"inspector clip command capacity exhausted"};
            impl_->draws[impl_->draw_count++] = {
                {{static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)},
                 {static_cast<std::uint32_t>(right - left),
                  static_cast<std::uint32_t>(bottom - top)}},
                static_cast<std::uint32_t>(index_base) + command.IdxOffset,
                command.ElemCount};
        }
    }
    if (impl_->draw_count == 0)
        return std::optional<render::Overlay>{};
    return std::optional<render::Overlay>{{impl_->font,
                                           std::span(impl_->vertices).first(impl_->vertex_count),
                                           std::span(impl_->indices).first(impl_->index_count),
                                           std::span(impl_->draws).first(impl_->draw_count)}};
}
} // namespace ngen::tools

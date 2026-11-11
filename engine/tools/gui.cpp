#include "tools/gui.hpp"
#include <algorithm>
#include <array>
#include <cmath>
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
        require(SDL_GetTicks() - start < 5000, "GUI font upload timed out");
        SDL_Delay(1);
    }
}
} // namespace
struct Gui::Impl {
    ImGuiContext *context{};
    ImGuiMemAllocFunc previous_allocate{};
    ImGuiMemFreeFunc previous_free{};
    void *previous_user{};
    bool allocators_set{}, platform{};
    std::uint64_t allocations{};
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
        require(ImGui::GetCurrentContext() == nullptr, "GUI requires an isolated ImGui context");
        ImGui::GetAllocatorFunctions(&previous_allocate, &previous_free, &previous_user);
        ImGui::SetAllocatorFunctions(allocate, free, this);
        allocators_set = true;
        context = ImGui::CreateContext();
        require(context != nullptr, "GUI context creation failed");
        auto &io = ImGui::GetIO();
        io.IniFilename = io.LogFilename = nullptr;
        io.ConfigFlags = static_cast<ImGuiConfigFlags>(
            static_cast<unsigned>(io.ConfigFlags) |
            static_cast<unsigned>(ImGuiConfigFlags_NavEnableKeyboard));
        io.BackendRendererName = "ngen-bounded-overlay";
        io.Fonts->AddFontDefaultBitmap();
        platform = ImGui_ImplSDL3_InitForVulkan(owner.window());
        require(platform, "GUI SDL platform initialization failed");
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
                "GUI atlas exceeds bounded RGBA8 profile");
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
};
Gui::Gui() : impl_(std::make_unique<Impl>()) {}
Gui::~Gui() = default;
std::variant<std::unique_ptr<Gui>, render::Failure> Gui::create(render::Context &context) {
    try {
        auto gui = std::unique_ptr<Gui>(new Gui);
        gui->impl_->initialize(context);
        return gui;
    } catch (const std::exception &error) {
        return render::Failure{error.what()};
    }
}
void Gui::event(const SDL_Event &event) {
    ImGui::SetCurrentContext(impl_->context);
    (void)ImGui_ImplSDL3_ProcessEvent(&event);
}
void Gui::begin() {
    ImGui::SetCurrentContext(impl_->context);
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}
void Gui::end() {
    ImGui::SetCurrentContext(impl_->context);
    ImGui::Render();
}
bool Gui::wants_mouse() const {
    ImGui::SetCurrentContext(impl_->context);
    return ImGui::GetIO().WantCaptureMouse;
}
bool Gui::wants_keyboard() const {
    ImGui::SetCurrentContext(impl_->context);
    return ImGui::GetIO().WantCaptureKeyboard;
}
std::uint64_t Gui::allocation_count() const noexcept { return impl_->allocations; }
std::variant<std::optional<render::Overlay>, render::Failure> Gui::overlay(VkExtent2D extent) {
    ImGui::SetCurrentContext(impl_->context);
    const auto *data = ImGui::GetDrawData();
    if (!data || data->TotalVtxCount == 0 || extent.width == 0 || extent.height == 0)
        return std::optional<render::Overlay>{};
    if (extent.width > 8192 || extent.height > 8192 || data->TotalVtxCount < 0 ||
        data->TotalIdxCount < 0 ||
        data->TotalVtxCount > static_cast<int>(render::overlay_vertex_limit) ||
        data->TotalIdxCount > static_cast<int>(render::overlay_index_limit) ||
        data->DisplaySize.x <= 0 || data->DisplaySize.y <= 0)
        return render::Failure{"GUI draw data exceeds the bounded overlay profile"};
    impl_->vertex_count = impl_->index_count = impl_->draw_count = 0;
    const auto x_scale = static_cast<float>(extent.width) / data->DisplaySize.x;
    const auto y_scale = static_cast<float>(extent.height) / data->DisplaySize.y;
    for (int list_index = 0; list_index < data->CmdListsCount; ++list_index) {
        const auto *list = data->CmdLists[list_index];
        const auto vertex_base = impl_->vertex_count, index_base = impl_->index_count;
        if (list->VtxBuffer.Size < 0 || list->IdxBuffer.Size < 0 ||
            static_cast<std::size_t>(list->VtxBuffer.Size) > impl_->vertices.size() - vertex_base ||
            static_cast<std::size_t>(list->IdxBuffer.Size) > impl_->indices.size() - index_base)
            return render::Failure{"GUI lists exceed bounded storage"};
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
                return render::Failure{"GUI supports only its copied atlas geometry"};
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
                return render::Failure{"GUI clip command capacity exhausted"};
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

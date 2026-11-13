#include "tools/gui.hpp"
#include "brand_data.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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
std::string system_font() {
    std::array<std::filesystem::path, 4> candidates{};
#ifdef _WIN32
    if (const auto *root = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "SystemRoot"))
        candidates[0] = std::filesystem::path(root) / "Fonts/segoeui.ttf";
#else
    candidates = {"/usr/share/fonts/TTF/DejaVuSans.ttf",
                  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                  "/usr/share/fonts/noto/NotoSans-Regular.ttf",
                  "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"};
#endif
    for (const auto &path : candidates) {
        std::error_code error;
        if (!path.empty() && path.is_absolute() && !std::filesystem::is_symlink(path, error) &&
            std::filesystem::is_regular_file(path, error) &&
            std::filesystem::file_size(path, error) <= std::uintmax_t{8} * 1024 * 1024 && !error)
            return path.string();
    }
    return {};
}
} // namespace
struct Gui::Impl {
    ImGuiContext *context{};
    ImGuiMemAllocFunc previous_allocate{};
    ImGuiMemFreeFunc previous_free{};
    void *previous_user{};
    bool allocators_set{}, platform{};
    std::uint64_t allocations{};
    bool icon_applied{};
    ImFont *body{}, *mono{}, *readout{};
    ImFontAtlasRectId mark_rect{ImFontAtlasRectId_Invalid},
        wordmark_rect{ImFontAtlasRectId_Invalid};
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
        const auto font_path = system_font();
        const auto sans = [&](float size) {
            ImFontConfig config;
            config.SizePixels = size;
            return font_path.empty()
                       ? io.Fonts->AddFontDefaultVector(&config)
                       : io.Fonts->AddFontFromFileTTF(font_path.c_str(), size, &config);
        };
        body = sans(15);
        mono = io.Fonts->AddFontDefaultBitmap();
        readout = sans(28);
        require(body && mono && readout, "GUI fonts could not be prepared");
        io.FontDefault = body;
        wordmark_rect = io.Fonts->AddCustomRect(static_cast<int>(brand::wordmark_width),
                                                static_cast<int>(brand::wordmark_height));
        mark_rect = io.Fonts->AddCustomRect(static_cast<int>(brand::mark_width),
                                            static_cast<int>(brand::mark_height));
        require(wordmark_rect != ImFontAtlasRectId_Invalid &&
                    mark_rect != ImFontAtlasRectId_Invalid,
                "GUI brand artwork could not be packed");
        io.Fonts->TexPixelsUseColors = true;
        auto icon_pixels = brand::icon_pixels;
        const auto surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>(
            SDL_CreateSurfaceFrom(static_cast<int>(brand::icon_width),
                                  static_cast<int>(brand::icon_height), SDL_PIXELFORMAT_RGBA32,
                                  icon_pixels.data(), static_cast<int>(brand::icon_width * 4)),
            SDL_DestroySurface);
        require(surface != nullptr, "GUI icon surface could not be prepared");
        icon_applied = SDL_SetWindowIcon(owner.window(), surface.get());
        if (!icon_applied)
            SDL_ClearError(); // Some Wayland compositors do not expose window-icon support.
        platform = ImGui_ImplSDL3_InitForVulkan(owner.window());
        require(platform, "GUI SDL platform initialization failed");
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);
        ImGui::StyleColorsDark();
        auto &style = ImGui::GetStyle();
        style.WindowRounding = 2;
        style.FrameRounding = 1;
        style.WindowBorderSize = style.FrameBorderSize = 1;
        style.WindowPadding = {12, 10};
        style.FramePadding = {6, 4};
        style.ItemSpacing = {6, 6};
        const auto colour = [](std::uint32_t value) {
            return ImGui::ColorConvertU32ToFloat4(value);
        };
        style.Colors[ImGuiCol_Text] = colour(brand::text);
        style.Colors[ImGuiCol_TextDisabled] = colour(brand::muted);
        style.Colors[ImGuiCol_WindowBg] = colour(brand::bg);
        style.Colors[ImGuiCol_PopupBg] = colour(brand::surface);
        style.Colors[ImGuiCol_Border] = colour(brand::control_border);
        style.Colors[ImGuiCol_Separator] = colour(brand::border);
        for (const auto slot : {ImGuiCol_FrameBg, ImGuiCol_Header, ImGuiCol_Tab, ImGuiCol_Button})
            style.Colors[slot] = colour(brand::surface);
        for (const auto slot : {ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
                                ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive, ImGuiCol_TabSelected,
                                ImGuiCol_TabHovered, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive})
            style.Colors[slot] = colour(brand::surface_alt);
        for (const auto slot : {ImGuiCol_CheckMark, ImGuiCol_SliderGrab,
                                ImGuiCol_TabSelectedOverline, ImGuiCol_PlotLines})
            style.Colors[slot] = colour(brand::accent);
        style.Colors[ImGuiCol_NavCursor] = colour(brand::focus);
        style.Colors[ImGuiCol_PlotLinesHovered] = colour(brand::secondary);
        style.Colors[ImGuiCol_PlotHistogram] = colour(brand::warning);
        unsigned char *pixels{};
        int width = 0, height = 0, channels = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &channels);
        require(pixels && width > 0 && height > 0 && width <= 2048 && height <= 2048 &&
                    channels == 4,
                "GUI atlas exceeds bounded RGBA8 profile");
        const auto copy_art = [&](ImFontAtlasRectId id, std::span<const unsigned char> data) {
            ImFontAtlasRect rect;
            require(io.Fonts->GetCustomRect(id, &rect), "GUI brand atlas rect unavailable");
            require(data.size() == std::size_t{rect.w} * rect.h * 4 &&
                        unsigned{rect.x} + rect.w <= static_cast<unsigned>(width) &&
                        unsigned{rect.y} + rect.h <= static_cast<unsigned>(height),
                    "GUI brand atlas rect exceeds storage");
            for (unsigned y = 0; y < rect.h; ++y)
                std::memcpy(
                    pixels + (std::size_t{rect.y + y} * static_cast<unsigned>(width) + rect.x) * 4,
                    data.data() + std::size_t{y} * rect.w * 4, std::size_t{rect.w} * 4);
        };
        copy_art(wordmark_rect, brand::wordmark_pixels);
        copy_art(mark_rect, brand::mark_pixels);
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
void Gui::mark(float size) {
    ImGui::SetCurrentContext(impl_->context);
    ImFontAtlasRect rect;
    require(size >= 24 && size <= 256 &&
                ImGui::GetIO().Fonts->GetCustomRect(impl_->mark_rect, &rect),
            "GUI mark size or atlas rect invalid");
    ImGui::Image(ImTextureID{1}, {size, size}, rect.uv0, rect.uv1);
}
void Gui::wordmark(float width) {
    ImGui::SetCurrentContext(impl_->context);
    ImFontAtlasRect rect;
    require(width >= 160 && width <= 640 &&
                ImGui::GetIO().Fonts->GetCustomRect(impl_->wordmark_rect, &rect),
            "GUI wordmark size or atlas rect invalid");
    ImGui::Image(ImTextureID{1},
                 {width, width * static_cast<float>(brand::wordmark_height) /
                             static_cast<float>(brand::wordmark_width)},
                 rect.uv0, rect.uv1);
}
void Gui::push_font(GuiFont role) {
    ImGui::SetCurrentContext(impl_->context);
    ImGui::PushFont(role == GuiFont::readout ? impl_->readout
                    : role == GuiFont::mono  ? impl_->mono
                                             : impl_->body,
                    role == GuiFont::readout ? 28.0F
                    : role == GuiFont::mono  ? 13.0F
                                             : 15.0F);
}
void Gui::pop_font() {
    ImGui::SetCurrentContext(impl_->context);
    ImGui::PopFont();
}
bool Gui::window_icon_applied() const noexcept { return impl_->icon_applied; }
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

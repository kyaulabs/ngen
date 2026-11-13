#pragma once
#include "render/context.hpp"
#include "render/overlay.hpp"
#include <cstdint>
#include <optional>

namespace ngen::tools {
enum class GuiFont : std::uint8_t { body, mono, readout };
// One owner-thread ImGui/SDL frontend over the existing Context overlay path.
// Destroy before Context closes its window. Overlay spans borrow until the next
// overlay() call or destruction; Context copies them during submission.
class Gui {
  public:
    [[nodiscard]] static std::variant<std::unique_ptr<Gui>, render::Failure>
    create(render::Context &context);
    ~Gui();
    Gui(const Gui &) = delete;
    Gui &operator=(const Gui &) = delete;
    void event(const SDL_Event &event);
    void begin();
    void end();
    // Draw the supplied identity in the current UI layout; no extra texture/pass.
    void mark(float size = 32);
    void wordmark(float width = 176);
    void push_font(GuiFont font);
    void pop_font();
    [[nodiscard]] bool window_icon_applied() const noexcept;
    [[nodiscard]] std::variant<std::optional<render::Overlay>, render::Failure>
    overlay(VkExtent2D extent);
    [[nodiscard]] bool wants_mouse() const;
    [[nodiscard]] bool wants_keyboard() const;
    [[nodiscard]] std::uint64_t allocation_count() const noexcept;

  private:
    Gui();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ngen::tools

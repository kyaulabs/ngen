#include "package_fixture.hpp"
#include "render/scene_draw.hpp"
#include "tools/inspector.hpp"
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
using namespace ngen::render;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T take(std::variant<T, Failure> value) {
    if (const auto *failure = std::get_if<Failure>(&value))
        throw std::runtime_error(failure->message);
    return std::get<T>(std::move(value));
}
void wait(Context &context, const Submission &submission) {
    const auto start = SDL_GetTicks();
    while (!take(context.submission_complete(submission))) {
        require(SDL_GetTicks() - start < 5000, "inspector completion timed out");
        SDL_Delay(1);
    }
}
ngen::assets::PackagePin package() {
    auto sections = package_fixture::triangle();
    auto &material = sections[3];
    for (const float value : std::array<float, 13>{0, 0, 0, 1, .25F, .5F, .75F, 2, 1, 1, 1, 1, .5F})
        package_fixture::f32(material, value);
    package_fixture::u32(material, 0);
    package_fixture::u32(material, 1);
    for (unsigned i = 0; i < 5; ++i) {
        package_fixture::u32(material, UINT32_MAX);
        package_fixture::u32(material, 0);
    }
    material.resize(128, std::byte{0});
    package_fixture::set32(sections[2], 8, 0);
    auto loaded = ngen::assets::load_package(package_fixture::encode(sections));
    require(std::holds_alternative<ngen::assets::PackagePin>(loaded),
            "inspector fixture was not admitted");
    return std::get<ngen::assets::PackagePin>(std::move(loaded));
}
float half(std::uint16_t value) {
    const auto bits = std::uint32_t{value};
    const auto fraction = bits & 1023U;
    const auto exponent = static_cast<int>((bits >> 10U) & 31U);
    const auto magnitude = exponent == 0
                               ? std::ldexp(static_cast<float>(fraction), -24)
                               : std::ldexp(1 + static_cast<float>(fraction) / 1024, exponent - 15);
    return (bits & 0x8000U) ? -magnitude : magnitude;
}
} // namespace
int main() try {
    auto context = take(Context::create({true, false, false}));
    auto source = package();
    auto upload = take(SceneUpload::begin(*context, source));
    ScenePin version;
    const auto start = SDL_GetTicks();
    while (!version) {
        auto ready = take(upload->advance(*context));
        if (ready)
            version = std::move(*ready);
        require(SDL_GetTicks() - start < 5000, "inspector scene preparation timed out");
        SDL_Delay(1);
    }
    auto color = take(
        context->create_image({{4, 4},
                               VK_FORMAT_R16G16B16A16_SFLOAT,
                               static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
    auto depth = take(context->create_image(
        {{4, 4}, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
    auto shadow = take(context->create_image(
        {{4, 4},
         VK_FORMAT_D32_SFLOAT,
         static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) |
             VK_IMAGE_USAGE_SAMPLED_BIT}));
    auto white = take(context->create_image({{1, 1}, VK_FORMAT_R8G8B8A8_UNORM}));
    const auto pixels = [&](const Image &image, std::span<const std::byte> bytes) {
        auto staging = take(context->create_buffer(
            {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
        (void)take(staging.write(0, bytes));
        wait(*context, take(context->copy_to_image({staging, image})));
    };
    const std::array<std::byte, 4> white_pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                               std::byte{255}};
    pixels(white, white_pixel);
    std::array<Image, 3> environment;
    for (std::size_t i = 0; i < environment.size(); ++i) {
        environment[i] = take(
            context->create_image({{1, 1},
                                   VK_FORMAT_R16G16B16A16_SFLOAT,
                                   static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
                                       VK_IMAGE_USAGE_SAMPLED_BIT,
                                   1,
                                   i < 2}));
        const std::array<std::byte, 48> zero{};
        pixels(environment[i], std::span(zero).first(i < 2 ? 48U : 8U));
    }
    const SceneTargets targets{color,          depth,          shadow, environment[0],
                               environment[1], environment[2], white};
    auto readback = take(context->create_buffer(
        {VkDeviceSize{4} * 4 * 8, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
    SceneView view{};
    view.view_projection.columns = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, -4, 6, -5.5F, 1};
    view.light_view_projection = view.view_projection;
    view.camera_position = {4.25F, 6.25F, 10, 1};
    view.key_direction_intensity = {0, 0, -1, 0};
    SceneRenderer renderer;
    auto created = ngen::tools::Inspector::create(source);
    require(std::holds_alternative<ngen::tools::Inspector>(created),
            "inspector fixture model failed");
    auto inspector = std::get<ngen::tools::Inspector>(std::move(created));
    const auto render = [&](const ngen::tools::InspectorSnapshot &snapshot) {
        view.material_factors = std::span(snapshot.materials).first(snapshot.material_count);
        wait(*context, take(renderer.draw(*context, version, view, targets)));
        require(take(renderer.collect(*context)) == 0, "inspector frame did not retire");
        wait(*context, take(context->copy_from_image({color, readback})));
        std::array<std::uint16_t, 64> actual{};
        (void)take(readback.read(0, std::as_writable_bytes(std::span(actual))));
        const auto at = (1U * 4 + 2) * 4;
        return std::array{half(actual[at]), half(actual[at + 1]), half(actual[at + 2])};
    };
    const auto old = inspector.snapshot();
    const auto baseline = render(old);
    require(baseline == std::array{.5F, 1.0F, 1.5F}, "authored inspector emission pixels differ");
    auto changed = old.materials[0];
    changed.emissive_strength = 4;
    require(std::holds_alternative<std::monostate>(
                inspector.enqueue(ngen::tools::EditMaterial{old.generation, 0, changed})),
            "inspector material edit was rejected");
    require(render(inspector.snapshot()) == baseline, "queued edit mutated the current GPU frame");
    require(inspector.advance().applied == 1, "inspector edit did not advance");
    require(render(inspector.snapshot()) == std::array{1.0F, 2.0F, 3.0F},
            "next snapshot emission override did not reach GPU pixels");
    require(render(old) == baseline && source->materials[0].emissive_strength == 2,
            "inspector edit mutated an old snapshot or immutable source");
    view.key_direction_intensity[3] = 1;
    for (const auto parameters : std::array<std::array<float, 2>, 3>{{{0, 1}, {0, .5F}, {1, 1}}}) {
        auto factors = changed;
        factors.base_color = {.8F, .1F, .2F, 1};
        factors.metallic = parameters[0];
        factors.roughness = parameters[1];
        factors.emissive_strength = 0;
        require(std::holds_alternative<std::monostate>(
                    inspector.enqueue(ngen::tools::EditMaterial{old.generation, 0, factors})),
                "PBR factor edit was rejected");
        require(inspector.advance().applied == 1, "PBR factor edit did not advance");
        const auto actual = render(inspector.snapshot());
        for (std::size_t channel = 0; channel < 3; ++channel) {
            // Independent closed form at N=L=V, with zero IBL/local lights.
            const auto albedo = factors.base_color[channel];
            const auto f0 = .04F * (1 - factors.metallic) + albedo * factors.metallic;
            const auto rough2 = factors.roughness * factors.roughness;
            const auto expected =
                ((1 - f0) * (1 - factors.metallic) * albedo + f0 / (4 * rough2 * rough2)) /
                std::numbers::pi_v<float>;
            require(std::abs(actual[channel] - expected) < .002F,
                    "inspector base/metallic/roughness GPU factors differ");
        }
    }
    std::array<ngen::assets::MaterialFactors, 2> wrong{};
    view.material_factors = wrong;
    require(std::holds_alternative<Failure>(renderer.draw(*context, version, view, targets)) &&
                take(renderer.collect(*context)) == 0,
            "mismatched factor table published a frame");
    wrong[0].roughness = std::numeric_limits<float>::quiet_NaN();
    view.material_factors = std::span(wrong).first(1);
    require(std::holds_alternative<Failure>(renderer.draw(*context, version, view, targets)) &&
                take(renderer.collect(*context)) == 0,
            "nonfinite factor table published a frame");
    std::puts("inspector_material_gpu=1 next_frame_edit=1 old_snapshot_gpu=1 immutable_material=1 "
              "inspector_pbr_factors=3 factor_admission=1");
    require(context->close(), "inspector close/validation failed");
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Inspector probe: %s\n", error.what());
    return 1;
}

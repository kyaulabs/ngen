#include "package_fixture.hpp"
#include "render/scene_draw.hpp"
#include "tools/inspector.hpp"
#include "tools/ui.hpp"
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
ngen::assets::PackagePin translucent_package(bool reverse, bool coplanar) {
    package_fixture::Sections sections;
    for (std::uint32_t primitive = 0; primitive < 3; ++primitive) {
        const auto z = primitive == 2 ? .3F : primitive == 0 || coplanar ? -.2F : .2F;
        const auto left = primitive == 2 ? .5F : 0.0F;
        const std::array<std::array<float, 2>, 4> corners{{{left, 0}, {1, 0}, {1, 1}, {left, 1}}};
        for (const auto &corner : corners)
            for (const auto value :
                 std::array<float, 14>{corner[0], corner[1], z, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1})
                package_fixture::f32(sections[0], value);
        for (const auto index : std::array<std::uint32_t, 6>{0, 1, 2, 0, 2, 3})
            package_fixture::u32(sections[1], index);
        for (const auto value : std::array<std::uint32_t, 4>{primitive * 4, 4, primitive, 1})
            package_fixture::u32(sections[2], value);
        for (const auto value : std::array<float, 6>{left, 0, z, 1, 1, z})
            package_fixture::f32(sections[2], value);
        package_fixture::u32(sections[2], primitive * 6);
        package_fixture::u32(sections[2], 6);
        package_fixture::f32(sections[2], 0);
        sections[2].resize((std::size_t{primitive} + 1) * 80, std::byte{0});
        const auto alpha = primitive == 2 ? 1.0F : .5F;
        const std::array<float, 3> emission{primitive == 0 ? 1.0F : 0.0F,
                                            primitive == 1 ? 1.0F : 0.0F,
                                            primitive == 2 ? 1.0F : 0.0F};
        for (const auto value : std::array<float, 13>{0, 0, 0, alpha, emission[0], emission[1],
                                                      emission[2], 1, 0, 1, 1, 1, .5F})
            package_fixture::f32(sections[3], value);
        package_fixture::u32(sections[3], primitive == 2 ? 0U : 2U);
        package_fixture::u32(sections[3], 1);
        for (unsigned map = 0; map < 5; ++map) {
            package_fixture::u32(sections[3], UINT32_MAX);
            package_fixture::u32(sections[3], 0);
        }
        sections[3].resize((std::size_t{primitive} + 1) * 128, std::byte{0});
    }
    // Opaque input is deliberately between the two translucent inputs.
    for (const auto primitive : reverse ? std::array{1U, 2U, 0U} : std::array{0U, 2U, 1U}) {
        package_fixture::u32(sections[8], primitive);
        package_fixture::u32(sections[8], primitive);
        for (const auto value :
             std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 6, 6, 1})
            package_fixture::f32(sections[8], value);
    }
    auto loaded = ngen::assets::load_package(package_fixture::encode(sections));
    require(std::holds_alternative<ngen::assets::PackagePin>(loaded),
            "translucent fixture package was not admitted");
    return std::get<ngen::assets::PackagePin>(std::move(loaded));
}
class Gate {
  public:
    explicit Gate(Context &context) : context_(context) {
        VkSemaphoreTypeCreateInfo type{};
        type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = &type;
        require(vkCreateSemaphore(context.device(), &info, nullptr, &handle) == VK_SUCCESS,
                "overlay gate creation failed");
    }
    ~Gate() {
        if (!signal() || (pending && !completed && vkQueueWaitIdle(context_.queue()) != VK_SUCCESS))
            return;
        vkDestroySemaphore(context_.device(), handle, nullptr);
    }
    bool signal() {
        if (signaled_)
            return true;
        VkSemaphoreSignalInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        info.semaphore = handle;
        info.value = 1;
        signaled_ = vkSignalSemaphore(context_.device(), &info) == VK_SUCCESS;
        return signaled_;
    }
    VkSemaphore handle{};
    bool pending{}, completed{};

  private:
    Context &context_;
    bool signaled_{};
};
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
    auto context = take(Context::create({true, false, true, true}));
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
    {
        SceneView layered{};
        layered.view_projection.columns = {2, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, 0, -9, 13, 6.5F, 1};
        layered.light_view_projection = layered.view_projection;
        layered.camera_position = {4.5F, 6.5F, 10, 1};
        layered.key_direction_intensity = {0, 0, -1, 0};
        std::array<std::uint16_t, 64> reference{};
        for (unsigned scenario = 0; scenario < 4; ++scenario) {
            const bool reverse = scenario % 2 != 0, coplanar = scenario >= 2;
            auto pending =
                take(SceneUpload::begin(*context, translucent_package(reverse, coplanar)));
            ScenePin scene;
            const auto began = SDL_GetTicks();
            while (!scene) {
                auto progress = take(pending->advance(*context));
                if (progress)
                    scene = std::move(*progress);
                require(SDL_GetTicks() - began < 5000, "translucent fixture upload timed out");
                SDL_Delay(1);
            }
            wait(*context, take(renderer.draw(*context, scene, layered, targets)));
            require(take(renderer.collect(*context)) == 0, "translucent frame did not retire");
            wait(*context, take(context->copy_from_image({color, readback})));
            std::array<std::uint16_t, 64> actual{};
            (void)take(readback.read(0, std::as_writable_bytes(std::span(actual))));
            const auto unoccluded = (1U * 4 + 1) * 4, occluded = (1U * 4 + 2) * 4;
            // Linear HDR: near green(.5) over far red(.5), over black.
            const auto red = half(actual[unoccluded]), green = half(actual[unoccluded + 1]);
            if (!coplanar)
                require(red == .25F && green == .5F && half(actual[unoccluded + 2]) == 0,
                        "checked scene translucent order/composition differs");
            else
                // Equal-depth layers both pass LESS only when transparency does not write depth.
                // Either tie order is valid: do not couple this check to the sorting tie-breaker.
                require(red > 0 && green > 0 && red + green == .75F &&
                            half(actual[unoccluded + 2]) == 0,
                        "coplanar translucent geometry wrote depth");
            require(half(actual[occluded]) == 0 && half(actual[occluded + 1]) == 0 &&
                        half(actual[occluded + 2]) == 1,
                    "opaque depth did not occlude checked translucent geometry");
            if (scenario == 1)
                require(actual == reference, "translucent pixels depend on package instance order");
            else if (scenario == 0)
                reference = actual;
        }
        std::puts("scene_translucent_order=1 scene_opaque_occlusion=1 "
                  "scene_translucent_depth_readonly=1");
    }
    auto ldr = take(
        context->create_image({{4, 4},
                               VK_FORMAT_R8G8B8A8_SRGB,
                               static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT}));
    auto ldr_readback = take(
        context->create_buffer({64, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
    const auto composite = [&](const Overlay *overlay, float bloom = 0) {
        const auto submitted =
            take(context->post_process({color, ldr, 1, false, {}, overlay, bloom}));
        wait(*context, submitted);
        if (overlay && context->gpu_timings_available()) {
            const auto timing = take(context->submission_timings(submitted));
            require(timing && timing->post_ns > 0 && timing->ui_ns > 0,
                    "overlay GPU pass intervals did not publish after completion");
        }
        wait(*context, take(context->copy_from_image({ldr, ldr_readback})));
        std::array<std::uint8_t, 64> result{};
        (void)take(ldr_readback.read(0, std::as_writable_bytes(std::span(result))));
        return result;
    };
    const auto unmodified = composite(nullptr);
    std::array<shader::OverlayVertex, 4> quad{{{{0, 0}, {0, 0}, 0xff0000ffU},
                                               {{4, 0}, {1, 0}, 0xff0000ffU},
                                               {{4, 4}, {1, 1}, 0xff0000ffU},
                                               {{0, 4}, {0, 1}, 0xff0000ffU}}};
    const std::array<std::uint32_t, 6> quad_indices{0, 1, 2, 0, 2, 3};
    const std::array<OverlayDraw, 1> clips{{{{{0, 0}, {2, 4}}, 0, 6}}};
    const Overlay overlay{white, quad, quad_indices, clips};
    const auto painted = composite(&overlay);
    const auto left = (3U * 4 + 1) * 4, right = (3U * 4 + 3) * 4;
    require(painted[left] == 255 && painted[left + 1] == 0 && painted[left + 2] == 0,
            "tracked overlay did not composite after tone mapping");
    for (std::size_t channel = 0; channel < 4; ++channel)
        require(painted[right + channel] == unmodified[right + channel],
                "overlay clip did not preserve scene pixels");
    const auto bloomed = composite(nullptr, .1F);
    const auto bloomed_ui = composite(&overlay, .1F);
    require(bloomed_ui[left] == 255 && bloomed_ui[left + 1] == 0 && bloomed_ui[left + 2] == 0,
            "scene bloom modified the later opaque UI color");
    for (std::size_t channel = 0; channel < 4; ++channel)
        require(bloomed_ui[right + channel] == bloomed[right + channel],
                "bloom/UI clip modified background outside UI coverage");
    for (auto &vertex : quad)
        vertex.color = 0xff808080U;
    const auto gray = composite(&overlay);
    require(gray[left] == 128 && gray[left + 1] == 128 && gray[left + 2] == 128,
            "overlay sRGB vertex colors were encoded twice");
    for (auto &vertex : quad)
        vertex.color = 0x8000ff00U;
    const auto check_blend = [&](float coverage) {
        const auto blended = composite(&overlay);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const auto encoded = static_cast<float>(unmodified[left + channel]) / 255;
            const auto background =
                encoded <= .04045F ? encoded / 12.92F : std::pow((encoded + .055F) / 1.055F, 2.4F);
            const auto alpha = (128.0F / 255) * coverage;
            const auto linear = (channel == 1 ? alpha : 0) + background * (1 - alpha);
            const auto expected =
                255 * (linear <= .0031308F ? 12.92F * linear
                                           : 1.055F * std::pow(linear, 1.0F / 2.4F) - .055F);
            require(std::abs(static_cast<float>(blended[left + channel]) - expected) <= 2,
                    "overlay vertex/font alpha did not blend in linear light");
        }
    };
    check_blend(1);
    auto half_font = white_pixel;
    half_font[3] = std::byte{128};
    pixels(white, half_font);
    check_blend(128.0F / 255);
    pixels(white, white_pixel);
    auto bad_indices = quad_indices;
    bad_indices[0] = 99;
    const Overlay invalid_indices{white, quad, bad_indices, clips};
    require(std::holds_alternative<Failure>(
                context->post_process({color, ldr, 1, false, {}, &invalid_indices})),
            "overlay out-of-range index was admitted");
    const Overlay invalid_font{ldr, quad, quad_indices, clips};
    require(std::holds_alternative<Failure>(
                context->post_process({color, ldr, 1, false, {}, &invalid_font})),
            "overlay sampled its own color target");
    const auto reject_overlay = [&](const Overlay &candidate) {
        require(std::holds_alternative<Failure>(
                    context->post_process({color, ldr, 1, false, {}, &candidate})),
                "malformed/capacity/foreign overlay admission failed");
    };
    {
        auto malformed = quad;
        malformed[0].position[0] = std::numeric_limits<float>::quiet_NaN();
        reject_overlay({white, malformed, quad_indices, clips});
        malformed = quad;
        malformed[0].uv[0] = 1.01F;
        reject_overlay({white, malformed, quad_indices, clips});
        reject_overlay({white, {}, {}, {}});
        auto invalid_clip = clips;
        invalid_clip[0].clip.extent.width = 5;
        reject_overlay({white, quad, quad_indices, invalid_clip});
        std::vector<shader::OverlayVertex> excess_vertices(overlay_vertex_limit + 1);
        reject_overlay({white, excess_vertices, quad_indices, clips});
        std::vector<std::uint32_t> excess_indices(overlay_index_limit + 1);
        reject_overlay({white, quad, excess_indices, clips});
        std::vector<OverlayDraw> excess_draws(overlay_draw_limit + 1, clips[0]);
        reject_overlay({white, quad, quad_indices, excess_draws});
        auto uninitialized = take(context->create_image({{1, 1}, VK_FORMAT_R8G8B8A8_UNORM}));
        reject_overlay({uninitialized, quad, quad_indices, clips});
        auto foreign = take(Context::create({false, false, false}));
        auto foreign_font = take(foreign->create_image({{1, 1}, VK_FORMAT_R8G8B8A8_UNORM}));
        reject_overlay({foreign_font, quad, quad_indices, clips});
        require(foreign->close(), "foreign overlay owner close failed");
    }
    {
        auto alternate = take(context->create_image(
            {{4, 4},
             VK_FORMAT_R8G8B8A8_SRGB,
             static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT}));
        auto temporary_font = take(context->create_image({{1, 1}, VK_FORMAT_R8G8B8A8_UNORM}));
        pixels(temporary_font, white_pixel);
        Gate gate(*context);
        for (auto &vertex : quad)
            vertex.color = 0xff0000ffU;
        const Overlay borrowed{temporary_font, quad, quad_indices, clips};
        const auto first =
            take(context->post_process({color, ldr, 1, false, {gate.handle, 1}, &borrowed}));
        gate.pending = true;
        for (auto &vertex : quad)
            vertex.color = 0xffff0000U;
        const auto second =
            take(context->post_process({color, alternate, 1, false, {gate.handle, 1}, &borrowed}));
        for (auto &vertex : quad)
            vertex.color = 0xff00ff00U;
        const auto busy = context->post_process({color, ldr, 1, false, {}, &overlay});
        const auto *failure = std::get_if<Failure>(&busy);
        require(failure && failure->message == "transfer contexts are busy",
                "overlay completion slots reused while pending");
        temporary_font = {};
        require(take(context->resource_statistics()).retiring == 1,
                "overlay atlas retired before GPU completion");
        require(gate.signal(), "overlay gate signal failed");
        wait(*context, first);
        wait(*context, second);
        gate.completed = true;
        require(take(context->resource_statistics()).retiring == 0,
                "completed overlay atlas was not reclaimed");
        const auto image_pixel = [&](const Image &image) {
            wait(*context, take(context->copy_from_image({image, ldr_readback})));
            std::array<std::uint8_t, 64> result{};
            (void)take(ldr_readback.read(0, std::as_writable_bytes(std::span(result))));
            return std::array{result[left], result[left + 1], result[left + 2]};
        };
        require(image_pixel(ldr) == std::array<std::uint8_t, 3>{255, 0, 0} &&
                    image_pixel(alternate) == std::array<std::uint8_t, 3>{0, 0, 255},
                "submitted overlay spans were not copied into independent completion domains");
    }
    require(SDL_ShowWindow(context->window()), "overlay WSI window did not show");
    for (unsigned frame = 0; frame < 4; ++frame) {
        const auto deadline = SDL_GetTicks() + 5000;
        for (;;) {
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
            }
            const auto presented = take(context->render_frame({color, 1, false, &overlay}));
            if (presented.outcome == FrameOutcome::queued) {
                wait(*context, presented.resources);
                break;
            }
            require(SDL_GetTicks() < deadline, "overlay WSI admission timed out");
            SDL_Delay(1);
        }
    }
    require(SDL_SetWindowSize(context->window(), 1280, 720),
            "inspector frontend window resize failed");
    auto ui = take(ngen::tools::InspectorUi::create(*context));
    require(std::holds_alternative<std::monostate>(
                inspector.enqueue(ngen::tools::Select{old.generation, 0})),
            "frontend initial selection rejected");
    (void)inspector.advance();
    for (unsigned frame = 0; frame < 3; ++frame) {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
            ui->event(event);
        const auto snapshot = inspector.snapshot();
        ngen::tools::InspectorMetrics metrics{};
        metrics.frame = frame;
        metrics.memory = take(context->resource_statistics());
        ui->frame(inspector, snapshot, view, metrics);
        int width = 0, height = 0;
        require(SDL_GetWindowSizeInPixels(context->window(), &width, &height) && width > 0 &&
                    height > 0,
                "inspector frontend drawable is unavailable");
        const auto geometry =
            take(ui->overlay({static_cast<unsigned>(width), static_cast<unsigned>(height)}));
        if (!geometry || geometry->draws.empty() || geometry->vertices.empty())
            throw std::runtime_error("actual inspector frontend did not generate bounded geometry");
        if (frame == 1) {
            // Drawable may change after CPU layout but before swapchain recording.
            require(SDL_SetWindowSize(context->window(), 640, 360), "frontend race resize failed");
            while (SDL_PollEvent(&event))
                ui->event(event);
        }
        const auto deadline = SDL_GetTicks() + 5000;
        for (;;) {
            const auto result = take(context->render_frame({color, 1, false, &geometry.value()}));
            if (result.outcome == FrameOutcome::queued) {
                wait(*context, result.resources);
                break;
            }
            require(SDL_GetTicks() < deadline, "inspector frontend WSI timed out");
            SDL_Delay(1);
        }
    }
    ui.reset(); // SDL platform state ends before Context destroys its window.
    std::printf(
        "inspector_material_gpu=1 next_frame_edit=1 old_snapshot_gpu=1 immutable_material=1 "
        "inspector_pbr_factors=3 factor_admission=1 overlay_pixels=1 overlay_clip=1 overlay_srgb=1 "
        "overlay_alpha=1 overlay_font_alpha=1 overlay_bloom_order=1 overlay_admission=1 "
        "overlay_capacity=3 "
        "overlay_foreign=1 overlay_gpu_timing=%u "
        "overlay_snapshot=2 overlay_atlas_retirement=1 overlay_wsi_queued=4 "
        "inspector_frontend_frames=3 inspector_frontend_resize_race=1\n",
        context->gpu_timings_available() ? 1U : 0U);
    require(context->close(), "inspector close/validation failed");
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Inspector probe: %s\n", error.what());
    return 1;
}

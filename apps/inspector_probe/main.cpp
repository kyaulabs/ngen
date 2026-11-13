#include "package_fixture.hpp"
#include "render/scene_draw.hpp"
#include "tools/gui.hpp"
#include "tools/inspector.hpp"
#include "tools/ui.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <limits>
#include <numbers>
#include <optional>
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
enum class Fixture : std::uint8_t { emissive, rear_engine, point_light };
ngen::assets::PackagePin package(Fixture fixture = Fixture::emissive) {
    auto sections = package_fixture::triangle();
    auto &material = sections[3];
    std::array<float, 13> factors{0, 0, 0, 1, .25F, .5F, .75F, 2, 1, 1, 1, 1, .5F};
    if (fixture == Fixture::point_light) {
        factors[0] = factors[1] = factors[2] = 1;
        factors[4] = factors[5] = factors[6] = factors[8] = 0;
    }
    for (const float value : factors)
        package_fixture::f32(material, value);
    package_fixture::u32(material, 0);
    package_fixture::u32(material, 1);
    for (unsigned i = 0; i < 5; ++i) {
        package_fixture::u32(material, UINT32_MAX);
        package_fixture::u32(material, 0);
    }
    material.resize(128, std::byte{0});
    package_fixture::set32(sections[2], 8, 0);
    if (fixture == Fixture::rear_engine) {
        auto engine = sections[8];
        package_fixture::set32(engine, 8 + 12 * 4, std::bit_cast<std::uint32_t>(8.0F));
        sections[8].insert(sections[8].end(), engine.begin(), engine.end());
    }
    if (fixture == Fixture::point_light) {
        package_fixture::u32(sections[9], 1);
        for (const auto value : std::array<float, 7>{1, 1, 1, 4, 3, 0, 0})
            package_fixture::f32(sections[9], value);
        package_fixture::u32(sections[10], 0);
        package_fixture::u32(sections[10], 8);
        for (const auto value :
             std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4.25F, 6.25F, 7, 1})
            package_fixture::f32(sections[10], value);
    }
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
            for (const auto value : std::array<float, 14>{corner[0], corner[1], z, 0, 0, 1, .6F,
                                                          .5F, 0, 0, 1, 0, 0, 1})
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
    const auto read_scene = [&](const Submission &submission) {
        wait(*context, submission);
        require(take(renderer.collect(*context)) == 0, "inspector frame did not retire");
        wait(*context, take(context->copy_from_image({color, readback})));
        std::array<std::uint16_t, 64> actual{};
        (void)take(readback.read(0, std::as_writable_bytes(std::span(actual))));
        const auto at = (1U * 4 + 2) * 4;
        return std::array{half(actual[at]), half(actual[at + 1]), half(actual[at + 2])};
    };
    const auto render_scene = [&](ScenePin pin, const SceneView &frame_view) {
        return read_scene(take(renderer.draw(*context, std::move(pin), frame_view, targets)));
    };
    const auto render = [&](const ngen::tools::InspectorSnapshot &snapshot) {
        view.material_factors = std::span(snapshot.materials).first(snapshot.material_count);
        return render_scene(version, view);
    };
    const auto old = inspector.snapshot();
    const auto baseline = render(old);
    require(baseline == std::array{.5F, 1.0F, 1.5F}, "authored inspector emission pixels differ");
    view.root_transform.columns[12] = -1;
    require(render(old) == std::array<float, 3>{},
            "scene root translation did not move the old native pixel");
    std::array<std::uint16_t, 64> rooted_pixels{};
    (void)take(readback.read(0, std::as_writable_bytes(std::span(rooted_pixels))));
    constexpr auto rooted_at = (1U * 4) * 4;
    require(std::array{half(rooted_pixels[rooted_at]), half(rooted_pixels[rooted_at + 1]),
                       half(rooted_pixels[rooted_at + 2])} == baseline,
            "scene root translation did not reach its new native pixel");
    {
        Gate root_gate(*context);
        const auto queued_root =
            take(renderer.draw(*context, version, view, targets, {root_gate.handle, 1}));
        root_gate.pending = true;
        view.root_transform.columns[12] = 0;
        require(root_gate.signal(), "scene root gate did not signal");
        const auto copied_root = read_scene(queued_root);
        root_gate.completed = true;
        require(copied_root == std::array<float, 3>{},
                "pending scene root borrowed mutated view storage");
    }
    require(render(old) == baseline, "identity root did not restore authored native pixels");
    const auto identity_root = view.root_transform;
    for (unsigned scenario = 0; scenario < 7; ++scenario) {
        auto invalid = identity_root;
        switch (scenario) {
        case 0:
            invalid.columns[0] = 2;
            break;
        case 1:
            invalid.columns[0] = -1;
            break;
        case 2:
            invalid.columns[4] = .5F;
            break;
        case 3:
            invalid.columns[3] = .1F;
            break;
        case 4:
            invalid.columns[15] = 0;
            break;
        case 5:
            invalid.columns[12] = 16385;
            break;
        default:
            invalid.columns[12] = std::numeric_limits<float>::quiet_NaN();
            break;
        }
        view.root_transform = invalid;
        require(std::holds_alternative<Failure>(renderer.draw(*context, version, view, targets)) &&
                    take(renderer.collect(*context)) == 0,
                "invalid scene root published a frame");
    }
    view.root_transform = identity_root;
    {
        MeshDraw extra{};
        extra.vertices = &version->vertices();
        extra.indices = &version->indices();
        extra.index_count = 3;
        extra.frame.model.columns = source->instances[0].transform;
        extra.frame.normalMatrix = identity_root;
        extra.material.baseColor = {0, 0, 0, 1};
        extra.material.emissiveAlphaCutoff = {.75F, .25F, .5F, .5F};
        extra.material.flags[2] = 3;
        extra.facing = MeshFacing::both;
        extra.maps.fill(&white);
        view.root_transform.columns[12] = 10;
        const auto with_extra = read_scene(
            take(renderer.draw(*context, version, view, targets, {}, std::span{&extra, 1})));
        require(with_extra == std::array{.75F, .25F, .5F},
                "scene root incorrectly moved independent world-space extras");
        view.root_transform = identity_root;
    }
    {
        auto lit_upload = take(SceneUpload::begin(*context, package(Fixture::point_light)));
        ScenePin lit_scene;
        const auto began = SDL_GetTicks();
        while (!lit_scene) {
            auto progress = take(lit_upload->advance(*context));
            if (progress)
                lit_scene = std::move(*progress);
            require(SDL_GetTicks() - began < 5000, "rooted light fixture upload timed out");
            SDL_Delay(1);
        }
        auto lit_view = view;
        lit_view.material_factors = {};
        const auto lit = render_scene(lit_scene, lit_view);
        require(lit[0] > .1F, "rooted light fixture did not illuminate the reference surface");
        lit_view.root_transform.columns[12] = 10;
        lit_view.view_projection.columns[12] -= 10;
        lit_view.light_view_projection.columns[12] -= 10;
        lit_view.camera_position[0] += 10;
        const auto translated = render_scene(lit_scene, lit_view);
        for (std::size_t channel = 0; channel < lit.size(); ++channel)
            require(std::abs(translated[channel] - lit[channel]) < .002F,
                    "point-light instance did not follow its scene root");
        lit_view = view;
        lit_view.material_factors = {};
        lit_view.root_transform.columns = {0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
        lit_view.view_projection.columns = {0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0, 0, -4, 6, -5.5F, 1};
        lit_view.light_view_projection = lit_view.view_projection;
        lit_view.camera_position = {10, 6.25F, -4.25F, 1};
        const auto rotated = render_scene(lit_scene, lit_view);
        for (std::size_t channel = 0; channel < lit.size(); ++channel)
            require(std::abs(rotated[channel] - lit[channel]) < .002F,
                    "rigid scene rotation changed co-rotated normal/light pixels");
    }
    std::puts("scene_root_rigid=1 scene_root_copied=1 scene_root_lights=1 scene_root_extras=1");
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
    view.material_factors = {};
    std::array<EmissiveFlow, 2> wrong_flows{};
    view.material_flows = wrong_flows;
    require(std::holds_alternative<Failure>(renderer.draw(*context, version, view, targets)) &&
                take(renderer.collect(*context)) == 0,
            "mismatched emissive flow table published a frame");
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();
    for (const auto &flow : std::array<EmissiveFlow, 7>{
             {{-1, 0}, {1, 0}, {nan, 0}, {infinity, 0}, {0, -1}, {0, 2}, {0, nan}}}) {
        view.material_flows = std::span{&flow, 1};
        require(std::holds_alternative<Failure>(renderer.draw(*context, version, view, targets)) &&
                    take(renderer.collect(*context)) == 0,
                "invalid emissive phase/amount table published a frame");
    }
    const EmissiveFlow opaque_flow{0, 1};
    view.material_flows = std::span{&opaque_flow, 1};
    require(std::holds_alternative<Failure>(renderer.draw(*context, version, view, targets)) &&
                take(renderer.collect(*context)) == 0,
            "opaque material accepted emissive blend flow");
    view.material_flows = {};
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
            if (scenario == 0) {
                const auto authored_root = layered.root_transform;
                layered.root_transform.columns = {-1, 0, 0,  0, 0, 1, 0,  0,
                                                  0,  0, -1, 0, 9, 0, 12, 1};
                wait(*context, take(renderer.draw(*context, scene, layered, targets)));
                require(take(renderer.collect(*context)) == 0,
                        "rooted translucent frame did not retire");
                wait(*context, take(context->copy_from_image({color, readback})));
                std::array<std::uint16_t, 64> rotated{};
                (void)take(readback.read(0, std::as_writable_bytes(std::span(rotated))));
                require(half(rotated[occluded]) == .5F && half(rotated[occluded + 1]) == .25F &&
                            half(rotated[occluded + 2]) == 0,
                        "translucent sorting did not use rooted world depth");
                layered.root_transform = authored_root;
                std::puts("scene_root_transparent_order=1");
                std::array<EmissiveFlow, 3> flows{};
                flows[1] = {0, 1};
                layered.material_flows = flows;
                const auto read_flow = [&](const Submission &submission) {
                    wait(*context, submission);
                    require(take(renderer.collect(*context)) == 0,
                            "completed scene flow frame did not retire");
                    wait(*context, take(context->copy_from_image({color, readback})));
                    std::array<std::uint16_t, 64> result{};
                    (void)take(readback.read(0, std::as_writable_bytes(std::span(result))));
                    return result;
                };
                const auto zero = read_flow(take(renderer.draw(*context, scene, layered, targets)));
                require(zero[occluded + 2] == actual[occluded + 2],
                        "scene flow changed an unmarked opaque material");
                flows[1].phase = .25F;
                const auto quarter =
                    read_flow(take(renderer.draw(*context, scene, layered, targets)));
                require(quarter != zero, "scene material flow did not reach native pixels");
                Gate flow_gate(*context);
                const auto queued =
                    take(renderer.draw(*context, scene, layered, targets, {flow_gate.handle, 1}));
                flow_gate.pending = true;
                flows.fill({}); // The submitted constants must no longer borrow this table.
                require(flow_gate.signal(), "scene flow frame gate did not signal");
                const auto copied = read_flow(queued);
                flow_gate.completed = true;
                require(copied == quarter, "pending scene flow borrowed a mutated caller table");
                require(read_flow(take(renderer.draw(*context, scene, layered, targets))) == actual,
                        "disabled scene flow mutated source/default material factors");
                layered.material_flows = {};
                std::puts("scene_emissive_flow=1 scene_flow_copied=1 scene_flow_disabled=1");
            }
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
    for (auto &vertex : quad)
        vertex.color = 0xffffffffU;
    const std::array<std::byte, 4> branded_pixel{std::byte{99}, std::byte{221}, std::byte{248},
                                                 std::byte{255}};
    pixels(white, branded_pixel);
    const auto branded = composite(&overlay);
    require(std::abs(int{branded[left]} - 99) <= 1 && std::abs(int{branded[left + 1]} - 221) <= 1 &&
                std::abs(int{branded[left + 2]} - 248) <= 1,
            "coloured GUI atlas texels were discarded or encoded twice");
    pixels(white, white_pixel);
    std::puts("overlay_coloured_atlas=1");
    for (auto &vertex : quad)
        vertex.color = 0x8000ff00U;
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
    {
        auto created_box = ngen::tools::Inspector::create(package(Fixture::rear_engine));
        require(std::holds_alternative<ngen::tools::Inspector>(created_box),
                "ship bounds fixture model failed");
        auto boxed = std::get<ngen::tools::Inspector>(std::move(created_box));
        auto bounds_view = view;
        bounds_view.view_projection.columns[0] = .125F;
        bounds_view.view_projection.columns[5] = -.25F;
        bounds_view.view_projection.columns[12] = -.75F;
        bounds_view.view_projection.columns[13] = 1.5F;
        for (const auto selected :
             {std::optional<std::uint32_t>{0}, std::optional<std::uint32_t>{1},
              std::optional<std::uint32_t>{}}) {
            const auto current = boxed.snapshot();
            require(std::holds_alternative<std::monostate>(
                        boxed.enqueue(ngen::tools::Select{current.generation, selected})),
                    "ship bounds fixture selection failed");
            (void)boxed.advance();
            ui->frame(boxed, boxed.snapshot(), bounds_view, {});
            const auto geometry = take(ui->overlay({1280, 720}));
            if (!geometry)
                throw std::runtime_error("ship bounds fixture produced no overlay");
            float minimum = std::numeric_limits<float>::max(), maximum = -minimum;
            for (const auto &vertex : geometry->vertices)
                if ((vertex.color & 0x00ffffffU) == 0x00cfc482U && (vertex.color >> 24U) != 0) {
                    minimum = std::min(minimum, vertex.position[0]);
                    maximum = std::max(maximum, vertex.position[0]);
                }
            // Hull x=[3,5], separate engine x=[7,9]; full-ship projection x=[400,880].
            // Allow a scaled antialiased stroke (up to four pixels at 2x), not
            // the 320-pixel truncation from selecting only the hull component.
            require(std::abs(minimum - 400) < 5 && std::abs(maximum - 880) < 5,
                    "ship bounds omit the separately instanced rear engine or depend on part "
                    "selection");
        }
        std::puts("inspector_ship_bounds=1 inspector_rear_engine_bounds=1");
    }
    ui.reset(); // SDL platform state ends before Context destroys its window.
    {
        auto gui = take(ngen::tools::Gui::create(*context));
        require(std::holds_alternative<Failure>(ngen::tools::Gui::create(*context)),
                "generic frontend admitted a second active ImGui owner");
        gui->begin();
        ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, ImGui::GetIO().DisplaySize,
                                                      IM_COL32(255, 0, 0, 255));
        gui->end();
        const auto custom = take(gui->overlay({4, 4}));
        require(custom.has_value(), "generic frontend generated no custom overlay");
        const auto pixel = composite(&*custom);
        require(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0,
                "generic frontend custom geometry did not reach native pixels");
    }
    std::puts("gui_custom_pixels=1 gui_sequential_context=1 gui_single_owner=1");
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

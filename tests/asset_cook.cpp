#include "assetc/cook.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace ngen::assetc;
namespace {
Scene source_triangle() {
    Scene scene;
    Primitive p;
    for (auto position : std::array<std::array<float, 3>, 3>{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}}) {
        Vertex v;
        v.position = position;
        v.normal = {0, 0, 1};
        v.tangent = {1, 0, 0, 1};
        v.uv = {position[0], position[1]};
        v.uv1 = {.25F, .5F};
        p.vertices.push_back(v);
    }
    p.indices = {0, 1, 2};
    p.bounds = {{0, 0, 0}, {1, 1, 0}};
    scene.primitives.push_back(std::move(p));
    scene.meshes.push_back({0, 1});
    scene.instances.push_back({0, 0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 5, 6, 1}});
    scene.source_nodes = 1;
    scene.source_vertices = 3;
    scene.bounds = {{4, 5, 6}, {5, 6, 6}};
    return scene;
}
} // namespace
TEST_CASE("offline cooker preserves material factors and every punctual light", "[assets]") {
    auto source = source_triangle();
    Material material;
    material.name = "fixture paint";
    material.base_color = {.2F, .4F, .6F, .8F};
    material.emissive = {.1F, .2F, .3F};
    material.emissive_strength = 5;
    material.metallic = .25F;
    material.roughness = .75F;
    material.alpha = AlphaMode::blend;
    material.double_sided = true;
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    Light light;
    light.type = LightType::point;
    light.color = {1, .5F, .25F};
    light.intensity = 1234;
    light.range = 17;
    light.outer_angle = .7F;
    source.lights.push_back(light);
    source.light_instances.push_back({3, 0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 8, 9, 10, 1}});
    source.source_nodes = 4;
    const auto result = cook_scene(source);
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<CookedScene>(result).scene;
    REQUIRE(scene.materials.size() == 1);
    REQUIRE(scene.lights.size() == 1);
    REQUIRE(scene.light_instances.size() == 1);
    CHECK(scene.materials[0].name == "fixture paint");
    CHECK(scene.materials[0].base_color == material.base_color);
    CHECK(scene.materials[0].emissive_strength == 5);
    CHECK(scene.materials[0].metallic == .25F);
    CHECK(scene.materials[0].alpha == 2);
    CHECK(scene.materials[0].double_sided);
    CHECK(scene.primitives[0].material == 0);
    CHECK(scene.lights[0].intensity == 1234);
    CHECK(scene.lights[0].color == light.color);
    CHECK(scene.lights[0].range == 17);
    CHECK(scene.lights[0].outer_angle == 0); // Unused point-light cone fields are canonical zero.
    CHECK(scene.light_instances[0].source_node == 3);
    CHECK(scene.light_instances[0].transform[13] == 9);
}

TEST_CASE("offline cooker filters sRGB textures in linear light", "[assets]") {
    auto source = source_triangle();
    Image image;
    image.width = 2;
    image.height = 2;
    for (auto value : std::array<unsigned char, 16>{0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255,
                                                    255, 255, 255, 255})
        image.pixels.push_back(static_cast<std::byte>(value));
    source.images.push_back(image);
    source.textures.push_back({0, UINT32_MAX});
    Material material;
    material.base_color_map = {0, 0};
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 2});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &cooked = std::get<CookedScene>(result);
    REQUIRE(cooked.scene.images.size() == 1);
    REQUIRE(cooked.image_roles.size() == 1);
    CHECK(cooked.scene.images[0].format == ngen::assets::ImageFormat::rgba8_srgb);
    CHECK(cooked.image_roles[0] == ImageRole::base_color);
    REQUIRE(cooked.scene.images[0].mips.size() == 2);
    CHECK(cooked.scene.images[0].mips[0].pixels == image.pixels);
    CHECK(cooked.scene.images[0].mips[1].pixels ==
          std::vector<std::byte>{std::byte{188}, std::byte{188}, std::byte{188}, std::byte{255}});
    CHECK(cooked.texture_bytes == 20);
    CHECK(cooked.scene.materials[0].maps[0].texture == 0);
    REQUIRE(cooked.scene.textures.size() == 1);
    CHECK(cooked.scene.textures[0].image == 0);
    CHECK(source.images[0].pixels == image.pixels);
}

TEST_CASE("offline cooker filters linear material data without sRGB conversion", "[assets]") {
    auto source = source_triangle();
    Image image;
    image.width = 2;
    image.height = 2;
    for (auto value : std::array<unsigned char, 16>{0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255,
                                                    255, 255, 255, 255})
        image.pixels.push_back(static_cast<std::byte>(value));
    source.images.push_back(image);
    source.textures.push_back({0, UINT32_MAX});
    Material material;
    material.metallic_roughness_map = {0, 0};
    material.occlusion_map = {0, 0};
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 2});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &cooked = std::get<CookedScene>(result);
    REQUIRE(cooked.scene.images.size() == 1);
    REQUIRE(cooked.scene.textures.size() == 1);
    CHECK(cooked.scene.images[0].format == ngen::assets::ImageFormat::rgba8_unorm);
    CHECK(cooked.image_roles[0] == ImageRole::data);
    REQUIRE(cooked.scene.images[0].mips.size() == 2);
    CHECK(cooked.scene.images[0].mips[1].pixels ==
          std::vector<std::byte>{std::byte{128}, std::byte{128}, std::byte{128}, std::byte{255}});
    CHECK(cooked.scene.materials[0].maps[1].texture == cooked.scene.materials[0].maps[3].texture);
}

TEST_CASE("offline cooker renormalizes normal mips with a defined neutral cancellation",
          "[assets]") {
    auto source = source_triangle();
    Image image;
    image.width = 2;
    image.height = 2;
    auto expected =
        std::array<std::byte, 4>{std::byte{218}, std::byte{128}, std::byte{218}, std::byte{255}};
    std::array<unsigned char, 16> pixels{255, 128, 128, 255, 128, 128, 255, 255,
                                         255, 128, 128, 255, 128, 128, 255, 255};
    SECTION("average directions are normalized") {}
    SECTION("opposing directions have neutral fallback") {
        pixels = {255, 128, 128, 255, 0, 127, 127, 255, 255, 128, 128, 255, 0, 127, 127, 255};
        expected = {std::byte{128}, std::byte{128}, std::byte{255}, std::byte{255}};
    }
    for (auto value : pixels)
        image.pixels.push_back(static_cast<std::byte>(value));
    source.images.push_back(image);
    source.textures.push_back({0, UINT32_MAX});
    Material material;
    material.normal_map = {0, 0};
    material.normal_scale = .25F;
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 2});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &cooked = std::get<CookedScene>(result);
    REQUIRE(cooked.scene.images.size() == 1);
    REQUIRE(cooked.scene.images[0].mips.size() == 2);
    CHECK(cooked.scene.images[0].format == ngen::assets::ImageFormat::rgba8_unorm);
    CHECK(cooked.image_roles[0] == ImageRole::normal);
    CHECK(cooked.scene.images[0].mips[1].pixels ==
          std::vector<std::byte>(expected.begin(), expected.end()));
    CHECK(cooked.scene.materials[0].normal_scale == .25F);
}

TEST_CASE("opaque base-color mips ignore unused alpha", "[assets]") {
    auto source = source_triangle();
    Image image;
    image.width = 2;
    image.height = 2;
    for (auto value :
         std::array<unsigned char, 16>{255, 0, 0, 0, 0, 255, 0, 255, 255, 0, 0, 0, 0, 255, 0, 255})
        image.pixels.push_back(static_cast<std::byte>(value));
    source.images.push_back(image);
    source.textures.push_back({0, UINT32_MAX});
    Material material;
    material.base_color_map = {0, 0};
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 2});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &mip = std::get<CookedScene>(result).scene.images[0].mips[1];
    CHECK(mip.pixels ==
          std::vector<std::byte>{std::byte{188}, std::byte{188}, std::byte{0}, std::byte{128}});
}

TEST_CASE("blended base-color mips use alpha-weighted linear filtering", "[assets]") {
    auto source = source_triangle();
    Image image;
    image.width = 2;
    image.height = 2;
    for (auto value :
         std::array<unsigned char, 16>{255, 0, 0, 0, 0, 255, 0, 255, 255, 0, 0, 0, 0, 255, 0, 255})
        image.pixels.push_back(static_cast<std::byte>(value));
    source.images.push_back(image);
    source.textures.push_back({0, UINT32_MAX});
    Material material;
    material.base_color_map = {0, 0};
    material.alpha = AlphaMode::blend;
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 2});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &mip = std::get<CookedScene>(result).scene.images[0].mips[1];
    CHECK(mip.pixels ==
          std::vector<std::byte>{std::byte{0}, std::byte{255}, std::byte{0}, std::byte{128}});
}

TEST_CASE("emissive mip filtering ignores alpha and separates shared image roles", "[assets]") {
    auto source = source_triangle();
    Image image;
    image.width = 2;
    image.height = 2;
    for (auto value :
         std::array<unsigned char, 16>{255, 0, 0, 0, 0, 255, 0, 255, 255, 0, 0, 0, 0, 255, 0, 255})
        image.pixels.push_back(static_cast<std::byte>(value));
    source.images.push_back(image);
    source.textures.push_back({0, UINT32_MAX});
    Material material;
    material.base_color_map = {0, 0};
    material.emissive_map = {0, 0};
    material.alpha = AlphaMode::blend;
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 2});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &cooked = std::get<CookedScene>(result);
    REQUIRE(cooked.scene.images.size() == 2);
    REQUIRE(cooked.scene.textures.size() == 2);
    const auto &maps = cooked.scene.materials[0].maps;
    REQUIRE(maps[0].texture != maps[4].texture);
    const auto base = cooked.scene.textures[maps[0].texture].image,
               emission = cooked.scene.textures[maps[4].texture].image;
    CHECK(cooked.scene.images[base].mips[1].pixels ==
          std::vector<std::byte>{std::byte{0}, std::byte{255}, std::byte{0}, std::byte{128}});
    CHECK(cooked.scene.images[emission].mips[1].pixels ==
          std::vector<std::byte>{std::byte{188}, std::byte{188}, std::byte{0}, std::byte{128}});
    CHECK(cooked.image_roles[emission] == ImageRole::emissive);
    CHECK(cooked.scene.images[emission].format == ngen::assets::ImageFormat::rgba8_srgb);
    CHECK(cooked.texture_bytes == 40);
}

TEST_CASE("offline LODs reduce redundant untextured triangle seams without changing LOD0",
          "[assets]") {
    auto source = source_triangle();
    auto &p = source.primitives[0];
    p.vertices.clear();
    p.indices.clear();
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 8; ++x) {
            const auto fx = static_cast<float>(x), fy = static_cast<float>(y);
            const std::array<std::array<float, 3>, 6> positions{{{fx, fy, 0},
                                                                 {fx + 1, fy, 0},
                                                                 {fx, fy + 1, 0},
                                                                 {fx + 1, fy, 0},
                                                                 {fx + 1, fy + 1, 0},
                                                                 {fx, fy + 1, 0}}};
            for (std::size_t corner = 0; corner < positions.size(); ++corner) {
                Vertex v;
                v.position = positions[corner];
                v.normal = {0, 0, 1};
                v.tangent = {1, 0, 0, 1};
                v.uv = {corner % 3 == 1 ? 1.0F : 0.0F, corner % 3 == 2 ? 1.0F : 0.0F};
                p.indices.push_back(static_cast<std::uint32_t>(p.vertices.size()));
                p.vertices.push_back(v);
            }
        }
    p.bounds = {{0, 0, 0}, {8, 8, 0}};
    source.source_vertices = p.vertices.size();
    const auto result = cook_scene(source);
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &cooked = std::get<CookedScene>(result);
    CHECK(cooked.lod_triangles[0] == 128);
    CHECK(cooked.lod_triangles[1] <= 64);
    CHECK(cooked.lod_triangles[2] <= cooked.lod_triangles[1]);
    CHECK(cooked.scene.primitives[0].lods[0].error == 0);
}

TEST_CASE("odd-sized image downscaling includes the complete source area", "[assets]") {
    auto source = source_triangle();
    Image image;
    image.width = 3;
    image.height = 1;
    for (auto value : std::array<unsigned char, 12>{0, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255})
        image.pixels.push_back(static_cast<std::byte>(value));
    source.images.push_back(image);
    source.textures.push_back({0, UINT32_MAX});
    Material material;
    material.metallic_roughness_map = {0, 0};
    source.materials.push_back(material);
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 1});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &mips = std::get<CookedScene>(result).scene.images[0].mips;
    REQUIRE(mips.size() == 1);
    CHECK(mips[0].width == 1);
    CHECK(mips[0].height == 1);
    CHECK(mips[0].pixels ==
          std::vector<std::byte>{std::byte{85}, std::byte{85}, std::byte{0}, std::byte{255}});
    CHECK(source.images[0].width == 3);
}

TEST_CASE("cooker cancellation and invalid settings do not publish a scene", "[assets]") {
    const auto source = source_triangle();
    const auto invalid = cook_scene(source, {.maximum_texture_dimension = 0});
    REQUIRE(std::holds_alternative<ImportFailure>(invalid));
    CHECK(std::get<ImportFailure>(invalid).issue == ImportIssue::invalid);
    std::stop_source stop;
    REQUIRE(stop.request_stop());
    const auto cancelled = cook_scene(source, {}, stop.get_token());
    REQUIRE(std::holds_alternative<ImportFailure>(cancelled));
    CHECK(std::get<ImportFailure>(cancelled).issue == ImportIssue::cancelled);
}

TEST_CASE("derived image capacity rejects a valid many-role source without dropping data",
          "[assets]") {
    auto source = source_triangle();
    for (std::uint32_t i = 0; i < 64; ++i) {
        Image image;
        image.width = 1;
        image.height = 1;
        image.pixels = {std::byte{128}, std::byte{128}, std::byte{255}, std::byte{255}};
        source.images.push_back(image);
        source.textures.push_back({i, UINT32_MAX});
        Material opaque;
        opaque.base_color_map = {i, 0};
        opaque.normal_map = {i, 0};
        opaque.metallic_roughness_map = {i, 0};
        opaque.emissive_map = {i, 0};
        source.materials.push_back(opaque);
        Material blended;
        blended.base_color_map = {i, 0};
        blended.alpha = AlphaMode::blend;
        source.materials.push_back(blended);
    }
    source.primitives[0].material = 0;
    const auto result = cook_scene(source, {.maximum_texture_dimension = 1});
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::capacity);
    CHECK(source.images.size() == 64);
    CHECK(source.materials.size() == 128);
}

TEST_CASE("offline cooker retains full geometry and creates bounded LOD records", "[assets]") {
    const auto result = cook_scene(source_triangle());
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &cooked = std::get<CookedScene>(result);
    REQUIRE(cooked.scene.vertices.size() == 3);
    REQUIRE(cooked.scene.primitives.size() == 1);
    REQUIRE(cooked.scene.primitives[0].lods.size() == 3);
    CHECK(cooked.lod_triangles == std::array<std::uint64_t, 3>{1, 1, 1});
    CHECK(cooked.scene.primitives[0].bounds.maximum == std::array<float, 3>{1, 1, 0});
    REQUIRE(cooked.scene.instances.size() == 1);
    CHECK(cooked.scene.instances[0].transform[12] == 4);
    for (const auto &lod : cooked.scene.primitives[0].lods) {
        REQUIRE(lod.index_count == 3);
        const auto &a = cooked.scene.vertices[cooked.scene.indices[lod.first_index]];
        const auto &b = cooked.scene.vertices[cooked.scene.indices[lod.first_index + 1]];
        const auto &c = cooked.scene.vertices[cooked.scene.indices[lod.first_index + 2]];
        CHECK(a.position == std::array<float, 3>{0, 0, 0});
        CHECK(b.position == std::array<float, 3>{1, 0, 0});
        CHECK(c.position == std::array<float, 3>{0, 1, 0});
        CHECK(a.uv1 == std::array<float, 2>{.25F, .5F});
    }
}

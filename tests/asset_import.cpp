#include "assetc/import.hpp"
#include <bit>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <string_view>

namespace {
using namespace ngen::assetc;
constexpr std::string_view triangle_json = R"({
  "asset":{"version":"2.0"},
  "buffers":[{"byteLength":152}],
  "bufferViews":[{"buffer":0,"byteLength":144,"byteStride":48},{"buffer":0,"byteOffset":144,"byteLength":6}],
  "accessors":[
    {"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
    {"bufferView":0,"byteOffset":12,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":0,"byteOffset":24,"componentType":5126,"count":3,"type":"VEC2"},
    {"bufferView":0,"byteOffset":32,"componentType":5126,"count":3,"type":"VEC4"},
    {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
  ],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"TANGENT":3},"indices":4}]}],
  "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0
})";
void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::byte>((value >> shift) & 255U));
}
struct FloatWrite {
    std::size_t offset;
    float value;
};
void store_float(std::vector<std::byte> &bytes, FloatWrite write) {
    const auto bits = std::bit_cast<std::uint32_t>(write.value);
    for (unsigned i = 0; i < 4; ++i)
        bytes[write.offset + i] = static_cast<std::byte>((bits >> (i * 8)) & 255U);
}
std::vector<std::byte> triangle_bytes() {
    std::vector<std::byte> bytes;
    const std::array<std::array<float, 12>, 3> vertices{{{0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1},
                                                         {1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1},
                                                         {0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1}}};
    for (const auto &vertex : vertices)
        for (auto value : vertex)
            append_u32(bytes, std::bit_cast<std::uint32_t>(value));
    bytes.insert(bytes.end(), {std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0}, std::byte{2},
                               std::byte{0}, std::byte{0}, std::byte{0}});
    return bytes;
}
std::vector<std::byte> glb(std::string json = std::string{triangle_json},
                           std::vector<std::byte> binary = triangle_bytes()) {
    while (json.size() % 4 != 0)
        json.push_back(' ');
    while (binary.size() % 4 != 0)
        binary.push_back(std::byte{0});
    std::vector<std::byte> bytes;
    append_u32(bytes, 0x46546c67);
    append_u32(bytes, 2);
    append_u32(bytes, static_cast<std::uint32_t>(28 + json.size() + binary.size()));
    append_u32(bytes, static_cast<std::uint32_t>(json.size()));
    append_u32(bytes, 0x4e4f534a);
    const auto text = std::as_bytes(std::span{json.data(), json.size()});
    bytes.insert(bytes.end(), text.begin(), text.end());
    append_u32(bytes, static_cast<std::uint32_t>(binary.size()));
    append_u32(bytes, 0x004e4942);
    bytes.insert(bytes.end(), binary.begin(), binary.end());
    return bytes;
}
struct Replacement {
    std::string_view original;
    std::string_view replacement;
};
std::string changed_json(std::initializer_list<Replacement> changes) {
    std::string result{triangle_json};
    for (auto change : changes) {
        const auto offset = result.find(change.original);
        REQUIRE(offset != std::string::npos);
        result.replace(offset, change.original.size(), change.replacement);
    }
    return result;
}
std::vector<std::byte> fixture_png(bool gamma) {
    constexpr std::array<std::uint8_t, 70> encoded{
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
        0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
        0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x9c, 0x63, 0x50, 0x70, 0x68, 0xf8, 0x0f, 0x00, 0x03, 0x44, 0x01, 0xe0, 0x32, 0xaa,
        0xbf, 0x84, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
    std::vector<std::byte> bytes;
    bytes.reserve(encoded.size() + 16);
    for (auto value : encoded)
        bytes.push_back(static_cast<std::byte>(value));
    if (gamma) {
        constexpr std::array<std::uint8_t, 16> chunk{0x00, 0x00, 0x00, 0x04, 0x67, 0x41,
                                                     0x4d, 0x41, 0x00, 0x01, 0x86, 0xa0,
                                                     0x31, 0xe8, 0x96, 0x5f};
        std::vector<std::byte> converted;
        converted.reserve(chunk.size());
        for (auto value : chunk)
            converted.push_back(static_cast<std::byte>(value));
        bytes.insert(bytes.begin() + 33, converted.begin(), converted.end());
    }
    return bytes;
}
std::string image_json(std::size_t png_size) {
    return changed_json(
        {{"\"byteLength\":152", "\"byteLength\":" + std::to_string(152 + png_size)},
         {"\"byteOffset\":144,\"byteLength\":6}",
          "\"byteOffset\":144,\"byteLength\":6},{\"buffer\":0,\"byteOffset\":152,\"byteLength\":" +
              std::to_string(png_size) + "}"},
         {"\"buffers\":",
          "\"images\":[{\"bufferView\":2,\"mimeType\":\"image/png\"}],\"buffers\":"}});
}
} // namespace

TEST_CASE("tangent cooking reports degenerate UVs and orthogonalizes the reference default",
          "[assets]") {
    auto binary = triangle_bytes();
    const float component = 1.0F / std::sqrt(3.0F);
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
        for (std::size_t axis = 0; axis < 3; ++axis)
            store_float(binary, {vertex * 48 + 12 + axis * 4, component});
        store_float(binary, {vertex * 48 + 24, 0});
        store_float(binary, {vertex * 48 + 28, 0});
    }
    const auto result =
        import_glb(glb(std::string{triangle_json}, binary), {.rebuild_tangents = true});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    for (const auto &vertex : scene.primitives.at(0).vertices) {
        CHECK(vertex.normal == std::array<float, 3>{component, component, component});
        CHECK(vertex.tangent[0] == Catch::Approx(std::sqrt(2.0F / 3.0F)));
        CHECK(vertex.tangent[1] == Catch::Approx(-1.0F / std::sqrt(6.0F)));
        CHECK(vertex.tangent[2] == Catch::Approx(-1.0F / std::sqrt(6.0F)));
        CHECK(std::abs(vertex.tangent[3]) == 1);
    }
    CHECK(scene.tangents.degenerate_uv_triangles == 1);
    CHECK(scene.tangents.degenerate_projection_corners == 3);
    CHECK(scene.tangents.derivative_fallback_corners == 0);
}

TEST_CASE("tangent cooking retains a valid frame at narrow triangle corners", "[assets]") {
    auto binary = triangle_bytes();
    store_float(binary, {48, 100.0F});
    store_float(binary, {100, 0.001F});
    const auto json = changed_json({{"\"max\":[1,1,0]", "\"max\":[100,0.001,0]"}});
    const auto result = import_glb(glb(json, binary), {.rebuild_tangents = true});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &primitive = std::get<Scene>(result).primitives.at(0);
    REQUIRE(primitive.indices.size() == 3);
    CHECK(primitive.vertices[primitive.indices[1]].position == std::array<float, 3>{100, 0, 0});
    for (const auto &vertex : primitive.vertices)
        CHECK(vertex.tangent == std::array<float, 4>{1, 0, 0, 1});
}

TEST_CASE("approved tangent rebuilding preserves geometry and mirrored UV handedness", "[assets]") {
    auto binary = triangle_bytes();
    // Mirror U at vertex 1; the canonical tangent must point -X with negative sign.
    binary[72] = std::byte{0};
    binary[73] = std::byte{0};
    binary[74] = std::byte{0x80};
    binary[75] = std::byte{0xbf};
    const auto result =
        import_glb(glb(changed_json({{",\"TANGENT\":3", ""}}), binary), {.rebuild_tangents = true});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    REQUIRE(scene.primitives.size() == 1);
    const auto &primitive = scene.primitives[0];
    REQUIRE(primitive.vertices.size() == 3);
    REQUIRE(primitive.indices.size() == 3);
    CHECK(primitive.vertices[primitive.indices[1]].position == std::array<float, 3>{1, 0, 0});
    CHECK(primitive.vertices[primitive.indices[1]].uv == std::array<float, 2>{-1, 0});
    for (const auto &vertex : primitive.vertices) {
        CHECK(vertex.normal == std::array<float, 3>{0, 0, 1});
        CHECK(vertex.tangent == std::array<float, 4>{-1, 0, 0, -1});
    }
    CHECK(scene.source_vertices == 3);
    CHECK(scene.tangents.missing_source_vertices == 3);
    CHECK(scene.tangents.rebuilt_primitives == 1);
    CHECK(scene.tangents.rebuilt_corners == 3);
}

TEST_CASE("static GLB import retains texture slots samplers and UV selections", "[assets]") {
    const auto png = fixture_png(false);
    auto binary = triangle_bytes();
    binary.insert(binary.end(), png.begin(), png.end());
    auto json = image_json(png.size());
    const std::string token = "\"images\":";
    json.replace(
        json.find(token), token.size(),
        R"("samplers":[{"minFilter":9987,"magFilter":9729,"wrapS":33071,"wrapT":33648}],"textures":[{"source":0,"sampler":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":0,"texCoord":1}},"normalTexture":{"index":0,"scale":0.25},"occlusionTexture":{"index":0,"strength":0.5},"emissiveTexture":{"index":0}}],"images":)");
    const auto uv = json.find("\"TEXCOORD_0\":2");
    json.replace(uv, std::string_view{"\"TEXCOORD_0\":2"}.size(),
                 "\"TEXCOORD_0\":2,\"TEXCOORD_1\":2");
    const auto primitive = json.find("\"indices\":4");
    json.replace(primitive, std::string_view{"\"indices\":4"}.size(),
                 "\"indices\":4,\"material\":0");
    const auto result = import_glb(glb(json, binary));
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    REQUIRE(scene.samplers.size() == 1);
    REQUIRE(scene.textures.size() == 1);
    REQUIRE(scene.materials.size() == 1);
    CHECK(scene.textures[0].image == 0);
    CHECK(scene.textures[0].sampler == 0);
    CHECK(scene.samplers[0].min == Filter::linear_mipmap_linear);
    CHECK(scene.samplers[0].mag == Filter::linear);
    CHECK(scene.samplers[0].s == Wrap::clamp);
    CHECK(scene.samplers[0].t == Wrap::mirrored_repeat);
    CHECK(scene.materials[0].base_color_map.texture == 0);
    CHECK(scene.materials[0].base_color_map.uv_set == 0);
    CHECK(scene.materials[0].metallic_roughness_map.texture == 0);
    CHECK(scene.materials[0].metallic_roughness_map.uv_set == 1);
    CHECK(scene.materials[0].normal_map.texture == 0);
    CHECK(scene.materials[0].normal_scale == 0.25F);
    CHECK(scene.materials[0].occlusion_strength == 0.5F);
    CHECK(scene.materials[0].emissive_map.texture == 0);
}

TEST_CASE("static GLB import owns PNG channels without color metadata conversion", "[assets]") {
    bool gamma = false;
    SECTION("plain PNG") {}
    SECTION("linear gAMA metadata must not reinterpret glTF channel bytes") { gamma = true; }
    const auto png = fixture_png(gamma);
    auto binary = triangle_bytes();
    binary.insert(binary.end(), png.begin(), png.end());
    const auto result = import_glb(glb(image_json(png.size()), binary));
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    REQUIRE(scene.images.size() == 1);
    CHECK(scene.images[0].width == 1);
    CHECK(scene.images[0].height == 1);
    CHECK(scene.images[0].pixels == std::vector<std::byte>{std::byte{0x20}, std::byte{0x40},
                                                           std::byte{0x80}, std::byte{0xff}});
}

TEST_CASE("static GLB import preserves PBR factors alpha and emissive strength", "[assets]") {
    const auto result = import_glb(glb(changed_json(
        {{"\"asset\":{\"version\":\"2.0\"},",
          R"("asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_emissive_strength"],"materials":[{"name":"fixture material","pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,0.75,0.125],"metallicFactor":0.25,"roughnessFactor":0.5},"emissiveFactor":[0.25,0.5,1],"extensions":{"KHR_materials_emissive_strength":{"emissiveStrength":10}},"alphaMode":"MASK","alphaCutoff":0.25,"doubleSided":true}],)"},
         {"\"indices\":4", "\"indices\":4,\"material\":0"}})));
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    REQUIRE(scene.materials.size() == 1);
    const auto &material = scene.materials[0];
    CHECK(material.name == "fixture material");
    CHECK(material.base_color == std::array<float, 4>{0.25F, 0.5F, 0.75F, 0.125F});
    CHECK(material.emissive == std::array<float, 3>{0.25F, 0.5F, 1});
    CHECK(material.emissive_strength == 10);
    CHECK(material.metallic == 0.25F);
    CHECK(material.roughness == 0.5F);
    CHECK(material.alpha == AlphaMode::mask);
    CHECK(material.alpha_cutoff == 0.25F);
    CHECK(material.double_sided);
    CHECK(scene.primitives.at(0).material == 0);
}

TEST_CASE("static GLB import preserves punctual light units and world transforms", "[assets]") {
    const auto result = import_glb(glb(changed_json(
        {{"\"asset\":{\"version\":\"2.0\"},",
          R"("asset":{"version":"2.0"},"extensionsUsed":["KHR_lights_punctual"],"extensionsRequired":["KHR_lights_punctual"],"extensions":{"KHR_lights_punctual":{"lights":[{"type":"point","color":[0.25,0.5,1],"intensity":40,"range":10}] }},)"},
         {"\"nodes\":[{\"mesh\":0}]",
          R"("nodes":[{"mesh":0,"translation":[2,3,4],"extensions":{"KHR_lights_punctual":{"light":0}}}])"}})));
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    REQUIRE(scene.lights.size() == 1);
    REQUIRE(scene.light_instances.size() == 1);
    CHECK(scene.lights[0].type == LightType::point);
    CHECK(scene.lights[0].color == std::array<float, 3>{0.25F, 0.5F, 1});
    CHECK(scene.lights[0].intensity == 40);
    CHECK(scene.lights[0].range == 10);
    CHECK(scene.light_instances[0].light == 0);
    CHECK(scene.light_instances[0].node == 0);
    CHECK(scene.light_instances[0].transform[12] == 2);
    CHECK(scene.light_instances[0].transform[13] == 3);
    CHECK(scene.light_instances[0].transform[14] == 4);
}

TEST_CASE("static GLB import preserves mesh instances and parent child transforms", "[assets]") {
    const auto result = import_glb(glb(changed_json(
        {{"\"nodes\":[{\"mesh\":0}]", "\"nodes\":[{\"translation\":[2,3,4],\"children\":[1]},{"
                                      "\"mesh\":0,\"scale\":[2,3,1]}]"}})));
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    REQUIRE(scene.meshes.size() == 1);
    REQUIRE(scene.instances.size() == 1);
    CHECK(scene.source_nodes == 2);
    CHECK(scene.meshes[0].first_primitive == 0);
    CHECK(scene.meshes[0].primitive_count == 1);
    CHECK(scene.instances[0].node == 1);
    CHECK(scene.instances[0].primitive == 0);
    CHECK(scene.instances[0].transform ==
          std::array<float, 16>{2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, 2, 3, 4, 1});
    CHECK(scene.primitives[0].vertices[1].position == std::array<float, 3>{1, 0, 0});
    CHECK(scene.primitives[0].bounds.minimum == std::array<float, 3>{0, 0, 0});
    CHECK(scene.primitives[0].bounds.maximum == std::array<float, 3>{1, 1, 0});
    CHECK(scene.bounds.minimum == std::array<float, 3>{2, 3, 4});
    CHECK(scene.bounds.maximum == std::array<float, 3>{4, 6, 4});
}

TEST_CASE("static GLB import preserves a declared second UV set", "[assets]") {
    const auto result =
        import_glb(glb(changed_json({{"\"TEXCOORD_0\":2", "\"TEXCOORD_0\":2,\"TEXCOORD_1\":2"}})));
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    CHECK(std::get<Scene>(result).primitives.at(0).vertices.at(2).uv1 ==
          std::array<float, 2>{0, 1});
}

TEST_CASE("static GLB import rejects unusable normal tangent frames", "[assets]") {
    auto binary = triangle_bytes();
    SECTION("zero normal") {
        for (std::size_t i = 12; i < 24; ++i)
            binary[i] = std::byte{0};
    }
    SECTION("parallel tangent") {
        for (std::size_t i = 32; i < 44; ++i)
            binary[i] = std::byte{0};
        binary[42] = std::byte{0x80};
        binary[43] = std::byte{0x3f};
    }
    const auto result = import_glb(glb(std::string{triangle_json}, binary));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::invalid);
}

TEST_CASE("static GLB import rejects nonrenderable node transforms", "[assets]") {
    std::string node;
    ImportIssue expected = ImportIssue::invalid;
    SECTION("zero quaternion") { node = R"("nodes":[{"mesh":0,"rotation":[0,0,0,0]}])"; }
    SECTION("projective matrix") {
        node = R"("nodes":[{"mesh":0,"matrix":[1,0,0,1,0,1,0,0,0,0,1,0,0,0,0,1]}])";
    }
    SECTION("singular scale") {
        node = R"("nodes":[{"mesh":0,"scale":[0,1,1]}])";
        expected = ImportIssue::unsupported;
    }
    const auto result = import_glb(glb(changed_json({{"\"nodes\":[{\"mesh\":0}]", node}})));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == expected);
}

TEST_CASE("static GLB import names unsupported required extensions", "[assets]") {
    const auto result = import_glb(glb(changed_json(
        {{"\"asset\":{\"version\":\"2.0\"},",
          R"("asset":{"version":"2.0"},"extensionsUsed":["KHR_not_supported"],"extensionsRequired":["KHR_not_supported"],)"}})));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    const auto &failure = std::get<ImportFailure>(result);
    CHECK(failure.issue == ImportIssue::unsupported);
    CHECK(failure.message.find("KHR_not_supported") != std::string::npos);
}

TEST_CASE("static GLB import rejects unsafe accessors and indices", "[assets]") {
    auto json = std::string{triangle_json};
    auto binary = triangle_bytes();
    SECTION("out of range triangle index") { binary[148] = std::byte{9}; }
    SECTION("view extends beyond embedded data") {
        json = changed_json({{"\"byteLength\":144", "\"byteLength\":2147483647"}});
    }
    SECTION("accessor offset exceeds view") {
        json = changed_json({{"\"byteOffset\":32", "\"byteOffset\":4294967295"}});
    }
    SECTION("stride is not component aligned") {
        json = changed_json({{"\"byteStride\":48", "\"byteStride\":46"}});
    }
    SECTION("mismatched vertex attribute counts") {
        json = changed_json({{"\"count\":3,\"type\":\"VEC2\"", "\"count\":2,\"type\":\"VEC2\""}});
    }
    SECTION("missing index accessor") {
        json = changed_json({{"\"indices\":4", "\"indices\":99"}});
    }
    SECTION("nonfinite position") {
        store_float(binary, {0, std::numeric_limits<float>::quiet_NaN()});
    }
    SECTION("nonfinite tangent is not a cooking repair") {
        store_float(binary, {32, std::numeric_limits<float>::infinity()});
    }
    const auto result = import_glb(glb(json, binary), {.rebuild_tangents = true});
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::invalid);
}

TEST_CASE("static GLB import rejects all resource URIs before loading", "[assets]") {
    std::string json;
    SECTION("relative buffer traversal") {
        json = changed_json(
            {{"\"byteLength\":152", "\"byteLength\":152,\"uri\":\"../../outside.bin\""}});
    }
    SECTION("network image") {
        json = changed_json(
            {{"\"buffers\":",
              "\"images\":[{\"uri\":\"https://example.invalid/image.png\"}],\"buffers\":"}});
    }
    SECTION("relative image traversal") {
        json = changed_json(
            {{"\"buffers\":", "\"images\":[{\"uri\":\"../outside.png\"}],\"buffers\":"}});
    }
    SECTION("encoded data URI") {
        json = changed_json(
            {{"\"byteLength\":152",
              "\"byteLength\":152,\"uri\":\"data:application/octet-stream;base64,AAAA\""}});
    }
    const auto result = import_glb(glb(json));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::unsupported);
    CHECK(std::get<ImportFailure>(result).message.find("URI") != std::string::npos);
}

TEST_CASE("static GLB import rejects malformed PNGs and admits size before decode", "[assets]") {
    auto png = fixture_png(false);
    auto expected = ImportIssue::invalid;
    SECTION("IHDR CRC") { png[29] ^= std::byte{1}; }
    SECTION("compressed data") { png[45] ^= std::byte{0xff}; }
    SECTION("truncation") { png.resize(40); }
    SECTION("trailing content") { png.push_back(std::byte{1}); }
    SECTION("zero width") { png[19] = std::byte{0}; }
    SECTION("excessive dimensions") {
        png[18] = std::byte{0x20};
        png[19] = std::byte{1};
        expected = ImportIssue::capacity;
    }
    auto binary = triangle_bytes();
    binary.insert(binary.end(), png.begin(), png.end());
    const auto result = import_glb(glb(image_json(png.size()), binary));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == expected);
}

TEST_CASE("static GLB import rejects ambiguous or cyclic scene graphs", "[assets]") {
    std::string nodes;
    SECTION("self cycle") { nodes = R"("nodes":[{"mesh":0,"children":[0]}])"; }
    SECTION("cycle through two nodes") {
        nodes = R"("nodes":[{"children":[1]},{"mesh":0,"children":[0]}])";
    }
    SECTION("unselected orphan cycle") {
        nodes = R"("nodes":[{"mesh":0},{"children":[2]},{"children":[1]}])";
    }
    SECTION("multiple parents") {
        nodes = R"("nodes":[{"children":[2]},{"children":[2]},{"mesh":0}])";
    }
    SECTION("repeated child") { nodes = R"("nodes":[{"children":[1,1]},{"mesh":0}])"; }
    const auto result = import_glb(glb(changed_json({{"\"nodes\":[{\"mesh\":0}]", nodes}})));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::invalid);
}

TEST_CASE("static GLB import enforces metadata budgets and duplicate rejection", "[assets]") {
    auto json = std::string{triangle_json};
    auto expected = ImportIssue::capacity;
    SECTION("buffer size claim") {
        json = changed_json({{"\"byteLength\":152", "\"byteLength\":4294967295"}});
    }
    SECTION("node count") {
        std::string nodes = "\"nodes\":[{\"mesh\":0}";
        for (std::size_t i = 0; i < 4096; ++i)
            nodes += ",{}";
        nodes.push_back(']');
        json = changed_json({{"\"nodes\":[{\"mesh\":0}]", nodes}});
    }
    SECTION("nested metadata") {
        std::string nested(65, '[');
        nested += '0';
        nested += std::string(65, ']');
        json = changed_json({{"\"asset\":", "\"extras\":" + nested + ",\"asset\":"}});
    }
    SECTION("duplicate JSON key") {
        json = changed_json({{"\"byteLength\":152", "\"byteLength\":152,\"byteLength\":152"}});
        expected = ImportIssue::invalid;
    }
    const auto result = import_glb(glb(json));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == expected);
}

TEST_CASE("cancelled import publishes no scene and does not poison the next load", "[assets]") {
    std::stop_source stop;
    REQUIRE(stop.request_stop());
    const auto cancelled = import_glb(glb(), {}, stop.get_token());
    REQUIRE(std::holds_alternative<ImportFailure>(cancelled));
    CHECK(std::get<ImportFailure>(cancelled).issue == ImportIssue::cancelled);
    CHECK(std::holds_alternative<Scene>(import_glb(glb())));
}

TEST_CASE("bounded malformed GLB corpus preserves result and ownership contracts",
          "[assets][corpus]") {
    const auto seed = glb();
    for (std::size_t length = 0; length < seed.size(); ++length) {
        INFO("truncated length=" << length);
        REQUIRE(std::holds_alternative<ImportFailure>(import_glb(std::span{seed}.first(length))));
    }
    constexpr std::array masks{std::byte{1}, std::byte{0x80}, std::byte{0x55}, std::byte{0xff}};
    for (std::size_t attempt = 0; attempt < seed.size() * masks.size(); ++attempt) {
        auto mutated = seed;
        const auto offset = attempt / masks.size();
        mutated[offset] ^= masks[attempt % masks.size()];
        const auto result = import_glb(mutated, {.rebuild_tangents = (attempt % 2) == 0});
        INFO("mutation=" << attempt << " offset=" << offset);
        if (const auto *scene = std::get_if<Scene>(&result)) {
            for (const auto &primitive : scene->primitives) {
                for (auto index : primitive.indices)
                    REQUIRE(index < primitive.vertices.size());
                for (const auto &vertex : primitive.vertices) {
                    for (float value : vertex.position)
                        REQUIRE(std::isfinite(value));
                    for (float value : vertex.normal)
                        REQUIRE(std::isfinite(value));
                    for (float value : vertex.uv)
                        REQUIRE(std::isfinite(value));
                    for (float value : vertex.tangent)
                        REQUIRE(std::isfinite(value));
                }
            }
        } else {
            CHECK_FALSE(std::get<ImportFailure>(result).message.empty());
        }
    }
}

TEST_CASE("tangent cooking rejects a collapsed referenced vertex domain", "[assets]") {
    auto binary = triangle_bytes();
    binary[146] = std::byte{0};
    binary[148] = std::byte{0};
    const auto result =
        import_glb(glb(std::string{triangle_json}, binary), {.rebuild_tangents = true});
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::unsupported);
}

TEST_CASE("static GLB import rejects material coordinates absent from a primitive", "[assets]") {
    const auto png = fixture_png(false);
    auto binary = triangle_bytes();
    binary.insert(binary.end(), png.begin(), png.end());
    auto json = image_json(png.size());
    const std::string key = "\"images\":";
    json.replace(
        json.find(key), key.size(),
        R"("textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0,"texCoord":1}}}],"images":)");
    const std::string index = "\"indices\":4";
    json.replace(json.find(index), index.size(), "\"indices\":4,\"material\":0");
    const auto result = import_glb(glb(json, binary));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::invalid);
    CHECK(std::get<ImportFailure>(result).message.find("UV1") != std::string::npos);
}

TEST_CASE("static GLB import does not silently drop vertex color semantics", "[assets]") {
    const auto result =
        import_glb(glb(changed_json({{"\"POSITION\":0", "\"POSITION\":0,\"COLOR_0\":3"}})));
    REQUIRE(std::holds_alternative<ImportFailure>(result));
    CHECK(std::get<ImportFailure>(result).issue == ImportIssue::unsupported);
}

TEST_CASE("tangent cooking splits shared corners across mirrored UV seams", "[assets]") {
    auto binary = triangle_bytes();
    binary.resize(144);
    const std::array<float, 12> fourth{1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1};
    for (float value : fourth)
        append_u32(binary, std::bit_cast<std::uint32_t>(value));
    for (std::uint16_t index : std::array<std::uint16_t, 6>{0, 1, 2, 1, 3, 2}) {
        binary.push_back(static_cast<std::byte>(index & 255U));
        binary.push_back(static_cast<std::byte>(index >> 8U));
    }
    const auto json = changed_json(
        {{"\"byteLength\":152", "\"byteLength\":204"},
         {"\"byteLength\":144", "\"byteLength\":192"},
         {"\"byteOffset\":144,\"byteLength\":6", "\"byteOffset\":192,\"byteLength\":12"},
         {"\"count\":3,\"type\":\"VEC3\"", "\"count\":4,\"type\":\"VEC3\""},
         {"\"count\":3,\"type\":\"VEC3\"", "\"count\":4,\"type\":\"VEC3\""},
         {"\"count\":3,\"type\":\"VEC2\"", "\"count\":4,\"type\":\"VEC2\""},
         {"\"count\":3,\"type\":\"VEC4\"", "\"count\":4,\"type\":\"VEC4\""},
         {"\"count\":3,\"type\":\"SCALAR\"", "\"count\":6,\"type\":\"SCALAR\""}});
    const auto result = import_glb(glb(json, binary), {.rebuild_tangents = true});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &scene = std::get<Scene>(result);
    const auto &primitive = scene.primitives.at(0);
    CHECK(scene.source_vertices == 4);
    REQUIRE(primitive.vertices.size() == 6);
    REQUIRE(primitive.indices.size() == 6);
    CHECK(primitive.indices[1] != primitive.indices[3]);
    CHECK(primitive.indices[2] != primitive.indices[5]);
    for (std::size_t corner = 0; corner < 6; ++corner) {
        const auto &vertex = primitive.vertices[primitive.indices[corner]];
        CHECK(vertex.normal == std::array<float, 3>{0, 0, 1});
        if (corner < 3)
            CHECK(vertex.tangent == std::array<float, 4>{1, 0, 0, 1});
        else
            CHECK(vertex.tangent == std::array<float, 4>{0, -1, 0, -1});
    }
}

TEST_CASE("tangent cooking remains defined for a shared indexed grid", "[assets]") {
    constexpr std::uint32_t side = 32, vertex_count = (side + 1) * (side + 1),
                            index_count = side * side * 6;
    std::vector<std::byte> binary;
    binary.reserve(vertex_count * 48 + index_count * 2);
    for (std::uint32_t y = 0; y <= side; ++y)
        for (std::uint32_t x = 0; x <= side; ++x) {
            const auto fx = static_cast<float>(x), fy = static_cast<float>(y);
            for (float value :
                 std::array<float, 12>{fx, fy, 0, 0, 0, 1, fx / side, fy / side, 1, 0, 0, 1})
                append_u32(binary, std::bit_cast<std::uint32_t>(value));
        }
    for (std::uint32_t y = 0; y < side; ++y)
        for (std::uint32_t x = 0; x < side; ++x) {
            const auto a = y * (side + 1) + x, b = a + 1, c = a + side + 1, d = c + 1;
            for (auto index : std::array{a, b, c, b, d, c}) {
                binary.push_back(static_cast<std::byte>(index & 255U));
                binary.push_back(static_cast<std::byte>(index >> 8U));
            }
        }
    const auto count = std::to_string(vertex_count);
    const auto json = changed_json(
        {{"\"byteLength\":152", "\"byteLength\":" + std::to_string(binary.size())},
         {"\"byteLength\":144", "\"byteLength\":" + std::to_string(vertex_count * 48)},
         {"\"byteOffset\":144,\"byteLength\":6",
          "\"byteOffset\":" + std::to_string(vertex_count * 48) +
              ",\"byteLength\":" + std::to_string(index_count * 2)},
         {"\"count\":3,\"type\":\"VEC3\"", "\"count\":" + count + ",\"type\":\"VEC3\""},
         {"\"count\":3,\"type\":\"VEC3\"", "\"count\":" + count + ",\"type\":\"VEC3\""},
         {"\"count\":3,\"type\":\"VEC2\"", "\"count\":" + count + ",\"type\":\"VEC2\""},
         {"\"count\":3,\"type\":\"VEC4\"", "\"count\":" + count + ",\"type\":\"VEC4\""},
         {"\"count\":3,\"type\":\"SCALAR\"",
          "\"count\":" + std::to_string(index_count) + ",\"type\":\"SCALAR\""},
         {"\"max\":[1,1,0]", "\"max\":[32,32,0]"}});
    const auto result = import_glb(glb(json, binary), {.rebuild_tangents = true});
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &primitive = std::get<Scene>(result).primitives.at(0);
    REQUIRE(primitive.indices.size() == index_count);
    REQUIRE(primitive.vertices.size() == vertex_count);
    for (const auto &vertex : primitive.vertices)
        CHECK(vertex.tangent == std::array<float, 4>{1, 0, 0, 1});
}

TEST_CASE("static GLB import generates indices for unindexed triangles", "[assets]") {
    const auto result = import_glb(glb(changed_json({{",\"indices\":4", ""}})));
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    const auto &primitive = std::get<Scene>(result).primitives.at(0);
    CHECK(primitive.indices == std::vector<std::uint32_t>{0, 1, 2});
}

TEST_CASE("static GLB import returns owning indexed triangle values", "[assets]") {
    auto bytes = glb();
    auto result = import_glb(bytes);
    if (const auto *failure = std::get_if<ImportFailure>(&result))
        FAIL(failure->message);
    bytes.clear();
    bytes.shrink_to_fit();
    const auto &scene = std::get<Scene>(result);
    REQUIRE(scene.primitives.size() == 1);
    const auto &primitive = scene.primitives[0];
    REQUIRE(primitive.vertices.size() == 3);
    CHECK(primitive.indices == std::vector<std::uint32_t>{0, 1, 2});
    CHECK(primitive.vertices[1].position == std::array<float, 3>{1, 0, 0});
    CHECK(primitive.vertices[2].normal == std::array<float, 3>{0, 0, 1});
    CHECK(primitive.vertices[2].uv == std::array<float, 2>{0, 1});
    CHECK(primitive.vertices[1].tangent == std::array<float, 4>{1, 0, 0, 1});
}

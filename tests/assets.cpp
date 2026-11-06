#include "assets/package.hpp"
#include "package_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
using namespace ngen::assets;
namespace {
class TemporaryPackage {
  public:
    TemporaryPackage() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            auto candidate =
                std::filesystem::temp_directory_path() /
                ("ngen-package-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                directory_ = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("cannot create package test directory");
    }
    ~TemporaryPackage() {
        std::error_code ignored;
        std::filesystem::remove(path(), ignored);
        std::filesystem::remove(directory_, ignored);
    }
    TemporaryPackage(const TemporaryPackage &) = delete;
    TemporaryPackage &operator=(const TemporaryPackage &) = delete;
    std::filesystem::path path() const { return directory_ / "scene with spaces.ngas"; }
    void write(std::span<const std::byte> bytes) const {
        std::ofstream output(path(), std::ios::binary);
        if (!bytes.empty())
            output.write(reinterpret_cast<const char *>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
        if (!output)
            throw std::runtime_error("cannot write package test fixture");
    }

  private:
    std::filesystem::path directory_;
};
package_fixture::Sections textured_triangle(bool linear = false) {
    const auto path = std::filesystem::path{NGEN_TEST_FIXTURES} /
                      (linear ? "rgba8-unorm.ktx2" : "rgba8-srgb.ktx2");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot read synthetic texture fixture");
    std::vector<std::byte> texture(static_cast<std::size_t>(std::filesystem::file_size(path)));
    input.read(reinterpret_cast<char *>(texture.data()),
               static_cast<std::streamsize>(texture.size()));
    if (!input)
        throw std::runtime_error("truncated synthetic texture fixture");
    auto sections = package_fixture::triangle();
    auto &record = sections[6];
    package_fixture::u64(record, 0);
    package_fixture::u64(record, texture.size());
    for (auto value : std::array<std::uint32_t, 4>{2, 2, 2, linear ? 1U : 2U})
        package_fixture::u32(record, value);
    sections[7] = texture;
    package_fixture::u32(sections[4], 0);
    package_fixture::u32(sections[4], 0);
    for (auto value : std::array<std::uint32_t, 4>{9987, 9729, 33071, 10497})
        package_fixture::u32(sections[5], value);
    return sections;
}
} // namespace

TEST_CASE("runtime package reads native KTX2 mip bytes without a source decoder", "[assets]") {
    bool linear = false;
    SECTION("sRGB color channels") {}
    SECTION("linear data channels") { linear = true; }
    const auto result = load_package(package_fixture::encode(textured_triangle(linear)));
    if (const auto *failure = std::get_if<Failure>(&result))
        FAIL(failure->message);
    const auto pin = std::get<PackagePin>(result);
    REQUIRE(pin->images.size() == 1);
    REQUIRE(pin->images[0].mips.size() == 2);
    CHECK(pin->images[0].format == (linear ? ImageFormat::rgba8_unorm : ImageFormat::rgba8_srgb));
    const auto &base = pin->images[0].mips[0], &mip = pin->images[0].mips[1];
    CHECK(base.width == 2);
    CHECK(base.height == 2);
    CHECK(base.pixels.size() == 16);
    CHECK(base.pixels[0] == std::byte{255});
    CHECK(base.pixels[1] == std::byte{0});
    CHECK(mip.width == 1);
    CHECK(mip.height == 1);
    const auto value = linear ? std::byte{128} : std::byte{188};
    CHECK(mip.pixels == std::vector<std::byte>{value, value, value, std::byte{255}});
    REQUIRE(pin->textures.size() == 1);
    REQUIRE(pin->samplers.size() == 1);
    CHECK(pin->textures[0].image == 0);
    CHECK(pin->textures[0].sampler == 0);
    CHECK(pin->samplers[0].min == 9987);
    CHECK(pin->samplers[0].s == 33071);
}

TEST_CASE("runtime material slots enforce their color interpretation", "[assets]") {
    auto sections = textured_triangle();
    auto &material = sections[3];
    for (float value : std::array<float, 13>{1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, .5F})
        package_fixture::f32(material, value);
    package_fixture::u32(material, 0);
    package_fixture::u32(material, 0);
    for (unsigned i = 0; i < 5; ++i) {
        package_fixture::u32(material, UINT32_MAX);
        package_fixture::u32(material, 0);
    }
    material.resize(128, std::byte{0});
    package_fixture::set32(sections[2], 8, 0);
    bool linear = false;
    SECTION("base color samples sRGB with retained UV1 selection") {
        package_fixture::set32(material, 60, 0);
        package_fixture::set32(material, 64, 1);
    }
    SECTION("metallic roughness must not sample an sRGB image") {
        package_fixture::set32(material, 68, 0);
        linear = true;
    }
    const auto result = load_package(package_fixture::encode(sections));
    if (linear) {
        REQUIRE(std::holds_alternative<Failure>(result));
        CHECK(std::get<Failure>(result).issue == Issue::invalid);
    } else {
        if (const auto *failure = std::get_if<Failure>(&result))
            FAIL(failure->message);
        CHECK(std::get<PackagePin>(result)->materials[0].maps[0].uv_set == 1);
    }
}

TEST_CASE("runtime package retains bounded opaque provenance without resolving sources",
          "[assets]") {
    auto sections = package_fixture::triangle();
    const std::string provenance =
        R"({"source":"source.glb","rights":"development-only","preset":"laptop"})";
    for (char c : provenance)
        sections[12].push_back(static_cast<std::byte>(c));
    const auto result = load_package(package_fixture::encode(sections));
    if (const auto *failure = std::get_if<Failure>(&result))
        FAIL(failure->message);
    CHECK(std::get<PackagePin>(result)->provenance == provenance);
}

TEST_CASE("runtime package rejects source node identifiers outside its domain", "[assets]") {
    auto sections = package_fixture::triangle();
    package_fixture::set32(sections[8], 4, 4096);
    const auto result = load_package(package_fixture::encode(sections));
    REQUIRE(std::holds_alternative<Failure>(result));
    CHECK(std::get<Failure>(result).issue == Issue::invalid);
}

TEST_CASE("runtime package rejects damaged container metadata before publication", "[assets]") {
    auto bytes = package_fixture::encode();
    auto issue = Issue::invalid;
    bool update_checksum = true;
    SECTION("wrong magic") { bytes[0] = std::byte{0}; }
    SECTION("unknown schema") {
        package_fixture::set32(bytes, 8, 2);
        issue = Issue::unsupported;
    }
    SECTION("source size mismatch") { package_fixture::set64(bytes, 16, bytes.size() + 8); }
    SECTION("damaged payload checksum") {
        bytes.back() ^= std::byte{1};
        update_checksum = false;
    }
    SECTION("overlapping sections") { package_fixture::set64(bytes, 112, 480); }
    SECTION("unknown stride") {
        package_fixture::set32(bytes, 68, 52);
        issue = Issue::unsupported;
    }
    SECTION("excessive primitive count") {
        package_fixture::set32(bytes, 136, 4097);
        issue = Issue::capacity;
    }
    SECTION("excessive vertex count") {
        package_fixture::set32(bytes, 72, static_cast<std::uint32_t>(vertex_limit + 1));
        issue = Issue::capacity;
    }
    SECTION("unsupported header flag") {
        package_fixture::set32(bytes, 32, 1);
        issue = Issue::unsupported;
    }
    SECTION("nonzero alignment padding") { bytes[660] = std::byte{1}; }
    SECTION("unclaimed trailing data") {
        bytes.push_back(std::byte{0});
        package_fixture::set64(bytes, 16, bytes.size());
    }
    if (update_checksum)
        package_fixture::checksum(bytes);
    const auto result = load_package(bytes);
    REQUIRE(std::holds_alternative<Failure>(result));
    CHECK(std::get<Failure>(result).issue == issue);
}

TEST_CASE("runtime package checks geometry and transform semantics after checksums", "[assets]") {
    auto sections = package_fixture::triangle();
    auto issue = Issue::invalid;
    SECTION("nonfinite position") {
        package_fixture::set32(
            sections[0], 0, std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN()));
    }
    SECTION("zero normal") { package_fixture::set32(sections[0], 20, 0); }
    SECTION("invalid tangent handedness") { package_fixture::set32(sections[0], 52, 0); }
    SECTION("bounds exclude a vertex") { package_fixture::set32(sections[2], 16, 0); }
    SECTION("invalid triangle index") { package_fixture::set32(sections[1], 0, 99); }
    SECTION("invalid LOD triangle count") { package_fixture::set32(sections[2], 44, 2); }
    SECTION("missing material") { package_fixture::set32(sections[2], 8, 0); }
    SECTION("missing instance primitive") { package_fixture::set32(sections[8], 0, 99); }
    SECTION("projective instance") {
        package_fixture::set32(sections[8], 20, std::bit_cast<std::uint32_t>(1.0F));
    }
    SECTION("singular instance") {
        package_fixture::set32(sections[8], 8, 0);
        issue = Issue::unsupported;
    }
    SECTION("world bounds overflow") {
        const auto large = std::bit_cast<std::uint32_t>(std::numeric_limits<float>::max());
        package_fixture::set32(sections[8], 8, large);
        package_fixture::set32(sections[8], 56, large);
    }
    const auto result = load_package(package_fixture::encode(sections));
    REQUIRE(std::holds_alternative<Failure>(result));
    CHECK(std::get<Failure>(result).issue == issue);
}

TEST_CASE("runtime native texture validation rejects mismatched layouts and bindings", "[assets]") {
    auto sections = textured_triangle();
    auto issue = Issue::invalid;
    SECTION("bad KTX identifier") { sections[7][0] = std::byte{0}; }
    SECTION("wrong transfer descriptor") {
        sections[7][142] = std::byte{1};
        issue = Issue::unsupported;
    }
    SECTION("incomplete mip index") { package_fixture::set32(sections[7], 40, 1); }
    SECTION("mip points into its header") { package_fixture::set64(sections[7], 80, 64); }
    SECTION("mip byte count") { package_fixture::set64(sections[7], 88, 15); }
    SECTION("overlapping mip levels") { package_fixture::set64(sections[7], 80, 324); }
    SECTION("supercompression requires an unsupported decoder") {
        package_fixture::set32(sections[7], 44, 2);
        issue = Issue::unsupported;
    }
    SECTION("array texture") {
        package_fixture::set32(sections[7], 32, 1);
        issue = Issue::unsupported;
    }
    SECTION("wrong scalar size") {
        package_fixture::set32(sections[7], 16, 4);
        issue = Issue::unsupported;
    }
    SECTION("misaligned embedded image range") { package_fixture::set64(sections[6], 0, 5); }
    SECTION("missing texture image") { package_fixture::set32(sections[4], 0, 1); }
    SECTION("unsupported image type") {
        package_fixture::set32(sections[6], 28, 99);
        issue = Issue::unsupported;
    }
    SECTION("invalid sampler") { package_fixture::set32(sections[5], 0, 999999); }
    const auto result = load_package(package_fixture::encode(sections));
    REQUIRE(std::holds_alternative<Failure>(result));
    CHECK(std::get<Failure>(result).issue == issue);
}

TEST_CASE("runtime file admission and cancellation publish no partial package", "[assets]") {
    TemporaryPackage file;
    auto path = file.path();
    auto expected = Issue::io;
    std::stop_source stop;
    SECTION("missing source") {}
    SECTION("directory is not a package") { path = path.parent_path(); }
    SECTION("oversized file is rejected before payload allocation") {
        file.write({});
        std::filesystem::resize_file(path, package_byte_limit + 1);
        expected = Issue::capacity;
    }
    SECTION("damaged source data") {
        const std::array<std::byte, 4> bytes{};
        file.write(bytes);
        expected = Issue::invalid;
    }
    SECTION("cancelled before filesystem access") {
        REQUIRE(stop.request_stop());
        expected = Issue::cancelled;
    }
    const auto result = load_package_file(path, stop.get_token());
    REQUIRE(std::holds_alternative<Failure>(result));
    CHECK(std::get<Failure>(result).issue == expected);
    if (expected == Issue::cancelled) {
        const auto memory = load_package(package_fixture::encode(), stop.get_token());
        REQUIRE(std::holds_alternative<Failure>(memory));
        CHECK(std::get<Failure>(memory).issue == Issue::cancelled);
    }
}

TEST_CASE("bounded runtime package corpus never publishes unsafe ranges", "[assets][corpus]") {
    const auto seed = package_fixture::encode(textured_triangle());
    for (std::size_t length = 0; length < seed.size(); ++length) {
        INFO("truncated package bytes=" << length);
        REQUIRE(std::holds_alternative<Failure>(load_package(std::span{seed}.first(length))));
    }
    constexpr std::array masks{std::byte{1}, std::byte{0x80}, std::byte{0x55}, std::byte{0xff}};
    for (std::size_t attempt = 0; attempt < seed.size() * masks.size(); ++attempt) {
        auto bytes = seed;
        const auto offset = attempt / masks.size();
        bytes[offset] ^= masks[attempt % masks.size()];
        if (attempt % 2 == 0)
            package_fixture::checksum(bytes);
        INFO("package mutation=" << attempt << " offset=" << offset);
        const auto result = load_package(bytes);
        if (const auto *pin = std::get_if<PackagePin>(&result)) {
            const auto &scene = **pin;
            for (const auto &primitive : scene.primitives)
                for (const auto &lod : primitive.lods)
                    for (std::size_t i = lod.first_index; i < lod.first_index + lod.index_count;
                         ++i)
                        REQUIRE(scene.indices[i] < primitive.vertex_count);
            for (const auto &vertex : scene.vertices) {
                for (float value : vertex.position)
                    REQUIRE(std::isfinite(value));
                for (float value : vertex.normal)
                    REQUIRE(std::isfinite(value));
                for (float value : vertex.tangent)
                    REQUIRE(std::isfinite(value));
            }
            for (const auto &image : scene.images)
                for (const auto &mip : image.mips)
                    REQUIRE(mip.pixels.size() == std::size_t{mip.width} * mip.height * 4);
        } else
            CHECK(!std::get<Failure>(result).message.empty());
    }
}

TEST_CASE("runtime package file load publishes storage independent of its file", "[assets]") {
    TemporaryPackage file;
    file.write(package_fixture::encode());
    const auto result = load_package_file(file.path());
    if (const auto *failure = std::get_if<Failure>(&result))
        FAIL(failure->message);
    REQUIRE(std::filesystem::remove(file.path()));
    const auto pin = std::get<PackagePin>(result);
    REQUIRE(pin->vertices.size() == 3);
    CHECK(pin->vertices[2].position == std::array<float, 3>{0, 2, 0});
    CHECK(pin->bounds.maximum == std::array<float, 3>{5, 7, 6});
}

TEST_CASE("runtime package retains PBR material factors and bounded names", "[assets]") {
    auto sections = package_fixture::triangle();
    auto &material = sections[3];
    for (float value :
         std::array<float, 13>{.2F, .4F, .6F, .8F, .1F, .2F, .3F, 2, .25F, .75F, .4F, .5F, .3F})
        package_fixture::f32(material, value);
    package_fixture::u32(material, 1);
    package_fixture::u32(material, 1);
    for (unsigned i = 0; i < 5; ++i) {
        package_fixture::u32(material, UINT32_MAX);
        package_fixture::u32(material, 0);
    }
    package_fixture::u32(material, 0);
    package_fixture::u32(material, 16);
    material.resize(128, std::byte{0});
    for (char c : std::string_view{"fixture material"})
        sections[11].push_back(static_cast<std::byte>(c));
    package_fixture::set32(sections[2], 8, 0);
    const auto result = load_package(package_fixture::encode(sections));
    if (const auto *failure = std::get_if<Failure>(&result))
        FAIL(failure->message);
    const auto &scene = *std::get<PackagePin>(result);
    REQUIRE(scene.materials.size() == 1);
    const auto &loaded = scene.materials[0];
    CHECK(loaded.name == "fixture material");
    CHECK(loaded.base_color == std::array<float, 4>{.2F, .4F, .6F, .8F});
    CHECK(loaded.emissive == std::array<float, 3>{.1F, .2F, .3F});
    CHECK(loaded.emissive_strength == 2);
    CHECK(loaded.metallic == .25F);
    CHECK(loaded.roughness == .75F);
    CHECK(loaded.normal_scale == .4F);
    CHECK(loaded.occlusion_strength == .5F);
    CHECK(loaded.alpha == 1);
    CHECK(loaded.alpha_cutoff == .3F);
    CHECK(loaded.double_sided);
    CHECK(scene.primitives[0].material == 0);
}

TEST_CASE("runtime package preserves punctual light definitions and instances", "[assets]") {
    auto sections = package_fixture::triangle();
    auto &light = sections[9];
    package_fixture::u32(light, 2);
    for (float value : std::array<float, 7>{.25F, .5F, 1, 40, 12, .1F, .7F})
        package_fixture::f32(light, value);
    auto &instance = sections[10];
    package_fixture::u32(instance, 0);
    package_fixture::u32(instance, 11);
    for (float value : std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 6, 8, 1})
        package_fixture::f32(instance, value);
    const auto result = load_package(package_fixture::encode(sections));
    if (const auto *failure = std::get_if<Failure>(&result))
        FAIL(failure->message);
    const auto &scene = *std::get<PackagePin>(result);
    REQUIRE(scene.lights.size() == 1);
    REQUIRE(scene.light_instances.size() == 1);
    CHECK(scene.lights[0].type == 2);
    CHECK(scene.lights[0].color == std::array<float, 3>{.25F, .5F, 1});
    CHECK(scene.lights[0].intensity == 40);
    CHECK(scene.lights[0].range == 12);
    CHECK(scene.lights[0].inner_angle == .1F);
    CHECK(scene.lights[0].outer_angle == .7F);
    CHECK(scene.light_instances[0].light == 0);
    CHECK(scene.light_instances[0].source_node == 11);
    CHECK(scene.light_instances[0].transform[13] == 6);
}

TEST_CASE("runtime package owns independently encoded geometry and transforms", "[assets]") {
    auto bytes = package_fixture::encode();
    const auto result = load_package(bytes);
    if (const auto *failure = std::get_if<Failure>(&result))
        FAIL(failure->message);
    const auto pin = std::get<PackagePin>(result);
    bytes.clear();
    bytes.shrink_to_fit();
    REQUIRE(pin->vertices.size() == 3);
    REQUIRE(pin->indices == std::vector<std::uint32_t>{0, 1, 2});
    CHECK(pin->vertices[0].position == std::array<float, 3>{-1, 0, 0});
    CHECK(pin->vertices[1].uv == std::array<float, 2>{1, 0});
    CHECK(pin->vertices[2].uv1 == std::array<float, 2>{.75F, .5F});
    CHECK(pin->vertices[2].tangent == std::array<float, 4>{1, 0, 0, 1});
    REQUIRE(pin->primitives.size() == 1);
    REQUIRE(pin->primitives[0].lods.size() == 1);
    CHECK(pin->primitives[0].lods[0].index_count == 3);
    REQUIRE(pin->instances.size() == 1);
    CHECK(pin->instances[0].source_node == 7);
    CHECK(pin->bounds.minimum == std::array<float, 3>{3, 5, 6});
    CHECK(pin->bounds.maximum == std::array<float, 3>{5, 7, 6});
}

#include "tools/inspector.hpp"
#include "package_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace {
ngen::assets::PackagePin material_package() {
    auto sections = package_fixture::triangle();
    auto &material = sections[3];
    for (const float value :
         std::array<float, 13>{.2F, .3F, .4F, 1, .1F, .2F, .3F, 2, .7F, .4F, 1, 1, .5F})
        package_fixture::f32(material, value);
    package_fixture::u32(material, 0);
    package_fixture::u32(material, 0);
    for (unsigned i = 0; i < 5; ++i) {
        package_fixture::u32(material, UINT32_MAX);
        package_fixture::u32(material, 0);
    }
    material.resize(128, std::byte{0});
    package_fixture::set32(sections[2], 8, 0);
    return std::get<ngen::assets::PackagePin>(
        ngen::assets::load_package(package_fixture::encode(sections)));
}
} // namespace

TEST_CASE("inspector rejects missing packages and nonexistent material targets") {
    const auto missing = ngen::tools::Inspector::create(nullptr);
    REQUIRE(std::holds_alternative<ngen::Error>(missing));
    REQUIRE(std::get<ngen::Error>(missing) == ngen::Error::invalid);
    auto inspector =
        std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(material_package()));
    const auto before = inspector.snapshot();
    REQUIRE(inspector.stage_package(nullptr) == ngen::Result<std::monostate>{ngen::Error::invalid});
    REQUIRE(
        inspector.enqueue(ngen::tools::EditMaterial{before.generation, 1, before.materials[0]}) ==
        ngen::Result<std::monostate>{ngen::Error::invalid});
    REQUIRE(inspector.snapshot().package == before.package);
    REQUIRE(inspector.pending_commands() == 0);
}

TEST_CASE("inspector snapshots preserve authored material factors and own their package") {
    auto package = material_package();
    const auto authored = package->materials[0];
    auto inspector = std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(package));
    const auto snapshot = inspector.snapshot();
    REQUIRE(snapshot.material_count == 1);
    REQUIRE(snapshot.materials[0].base_color == authored.base_color);
    REQUIRE(snapshot.materials[0].metallic == authored.metallic);
    REQUIRE(snapshot.materials[0].roughness == authored.roughness);
    REQUIRE(snapshot.materials[0].emissive_strength == authored.emissive_strength);
    package.reset();
    REQUIRE(snapshot.package->materials[0].name == authored.name);
}

TEST_CASE("prepared reload publication rejects old commands and preserves owning old snapshots") {
    const auto first = material_package(), second = material_package();
    auto inspector = std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(first));
    const auto generation = inspector.snapshot().generation;
    auto edited = inspector.snapshot().materials[0];
    edited.base_color = {.8F, .7F, .6F, 1};
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::Select{generation, 0})));
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::EditMaterial{generation, 0, edited})));
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::SetView{generation, {2, false}})));
    REQUIRE(inspector.advance().applied == 3);
    const auto old = inspector.snapshot();
    REQUIRE(std::holds_alternative<std::monostate>(inspector.stage_package(second)));
    REQUIRE(inspector.stage_package(first) == ngen::Result<std::monostate>{ngen::Error::busy});
    REQUIRE(inspector.snapshot().package == first);
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::Select{generation, 0})));
    const auto applied = inspector.advance();
    REQUIRE(applied.published);
    REQUIRE(applied.applied == 0);
    REQUIRE(applied.rejected == 1);
    const auto changed = inspector.snapshot();
    REQUIRE(changed.package == second);
    REQUIRE(changed.generation == generation + 1);
    REQUIRE_FALSE(changed.selection.has_value());
    REQUIRE(changed.materials[0].base_color == second->materials[0].base_color);
    REQUIRE(changed.view.exposure == 2);
    REQUIRE_FALSE(changed.view.fxaa);
    REQUIRE(old.package == first);
    REQUIRE(old.selection == 0);
    REQUIRE(old.materials[0].base_color == edited.base_color);
    REQUIRE(inspector.enqueue(ngen::tools::Select{generation, 0}) ==
            ngen::Result<std::monostate>{ngen::Error::stale});
}

TEST_CASE("reload is a next-frame request and does not replace the displayed package") {
    const auto package = material_package();
    auto inspector = std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(package));
    const auto before = inspector.snapshot();
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::Reload{before.generation})));
    REQUIRE(inspector.snapshot().reload_request == 0);
    REQUIRE(inspector.advance().applied == 1);
    const auto requested = inspector.snapshot();
    REQUIRE(requested.reload_request == 1);
    REQUIRE(requested.package == package);
    REQUIRE(requested.generation == before.generation);
    REQUIRE(before.reload_request == 0);
}

TEST_CASE("inspector refuses render settings outside production exposure admission") {
    auto inspector =
        std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(material_package()));
    const auto before = inspector.snapshot();
    for (const auto exposure : {-.1F, 65.0F, std::numeric_limits<float>::quiet_NaN()})
        REQUIRE(inspector.enqueue(ngen::tools::SetView{before.generation, {exposure, false}}) ==
                ngen::Result<std::monostate>{ngen::Error::invalid});
    REQUIRE(inspector.pending_commands() == 0);
    REQUIRE(inspector.snapshot().view.exposure == before.view.exposure);
}

TEST_CASE("inspector applies bounded FIFO render commands only at the frame boundary") {
    auto inspector =
        std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(material_package()));
    const auto before = inspector.snapshot();
    for (std::size_t i = 0; i < ngen::tools::Inspector::command_capacity; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(inspector.enqueue(
            ngen::tools::SetView{before.generation, {static_cast<float>(i) * 4, i % 2 == 0}})));
    REQUIRE(inspector.enqueue(ngen::tools::Select{before.generation, 0}) ==
            ngen::Result<std::monostate>{ngen::Error::capacity});
    REQUIRE(inspector.snapshot().view.exposure == before.view.exposure);
    const auto applied = inspector.advance();
    REQUIRE(applied.applied == 16);
    REQUIRE(applied.rejected == 0);
    REQUIRE(inspector.snapshot().view.exposure == 60);
    REQUIRE_FALSE(inspector.snapshot().view.fxaa);
    REQUIRE(before.view.exposure == 1);
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::Select{before.generation, std::nullopt})));
    REQUIRE(inspector.advance().applied == 1);
    REQUIRE_FALSE(inspector.snapshot().selection.has_value());
}

TEST_CASE("inspector rejects unsafe material factors without publishing or queueing them") {
    auto inspector =
        std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(material_package()));
    const auto before = inspector.snapshot();
    auto invalid = before.materials[0];
    SECTION("nonfinite color") { invalid.base_color[2] = std::numeric_limits<float>::quiet_NaN(); }
    SECTION("alpha above one") { invalid.base_color[3] = 1.1F; }
    SECTION("negative metallic") { invalid.metallic = -.1F; }
    SECTION("infinite roughness") { invalid.roughness = std::numeric_limits<float>::infinity(); }
    SECTION("unbounded emissive override") { invalid.emissive_strength = 65; }
    REQUIRE(inspector.enqueue(ngen::tools::EditMaterial{before.generation, 0, invalid}) ==
            ngen::Result<std::monostate>{ngen::Error::invalid});
    REQUIRE(inspector.pending_commands() == 0);
    REQUIRE(inspector.snapshot().materials[0].base_color == before.materials[0].base_color);
}

TEST_CASE("queued material edits change only the next owning inspector snapshot") {
    const auto package = material_package();
    auto inspector = std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(package));
    const auto old = inspector.snapshot();
    auto factors = old.materials[0];
    factors.base_color = {.8F, .1F, .2F, .5F};
    factors.metallic = .3F;
    factors.roughness = .6F;
    factors.emissive_strength = 4;
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::EditMaterial{old.generation, 0, factors})));
    REQUIRE(inspector.snapshot().materials[0].base_color == old.materials[0].base_color);
    REQUIRE(inspector.advance().applied == 1);
    const auto changed = inspector.snapshot();
    REQUIRE(changed.materials[0].base_color == factors.base_color);
    REQUIRE(changed.materials[0].metallic == factors.metallic);
    REQUIRE(changed.materials[0].roughness == factors.roughness);
    REQUIRE(changed.materials[0].emissive_strength == factors.emissive_strength);
    REQUIRE(old.materials[0].base_color == package->materials[0].base_color);
    REQUIRE(package->materials[0].metallic == old.materials[0].metallic);
}

TEST_CASE("inspector rejects stale and out-of-range selection without consuming queue capacity") {
    const auto package =
        std::get<ngen::assets::PackagePin>(ngen::assets::load_package(package_fixture::encode()));
    auto inspector = std::get<ngen::tools::Inspector>(ngen::tools::Inspector::create(package));
    const auto initial = inspector.snapshot();
    REQUIRE(inspector.enqueue(ngen::tools::Select{initial.generation, 1}) ==
            ngen::Result<std::monostate>{ngen::Error::invalid});
    REQUIRE(inspector.enqueue(ngen::tools::Select{initial.generation + 1, 0}) ==
            ngen::Result<std::monostate>{ngen::Error::stale});
    REQUIRE(inspector.pending_commands() == 0);
    REQUIRE_FALSE(inspector.snapshot().selection.has_value());
}

TEST_CASE("inspector selection applies at the next frame without changing old snapshots") {
    auto loaded = ngen::assets::load_package(package_fixture::encode());
    REQUIRE(std::holds_alternative<ngen::assets::PackagePin>(loaded));
    const auto package = std::get<ngen::assets::PackagePin>(loaded);
    auto created = ngen::tools::Inspector::create(package);
    REQUIRE(std::holds_alternative<ngen::tools::Inspector>(created));
    auto inspector = std::get<ngen::tools::Inspector>(std::move(created));
    const auto old = inspector.snapshot();
    REQUIRE_FALSE(old.selection.has_value());
    REQUIRE(std::holds_alternative<std::monostate>(
        inspector.enqueue(ngen::tools::Select{old.generation, 0})));
    REQUIRE_FALSE(inspector.snapshot().selection.has_value());
    REQUIRE(inspector.pending_commands() == 1);
    const auto applied = inspector.advance();
    REQUIRE(applied.applied == 1);
    REQUIRE(applied.rejected == 0);
    REQUIRE(inspector.pending_commands() == 0);
    REQUIRE(inspector.snapshot().selection == 0);
    REQUIRE_FALSE(old.selection.has_value());
    REQUIRE(old.package == package);
}

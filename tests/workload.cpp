#include "scene_workload/allocations.hpp"
#include "scene_workload/fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <new>

TEST_CASE("workload allocation instrumentation observes scalar and aligned requests without "
          "replacing OOM policy") {
    const auto before = ngen::workload::allocation_count();
    void *scalar = ::operator new(16);
    void *aligned = ::operator new(64, std::align_val_t{64});
    const auto after = ngen::workload::allocation_count();
    const auto address = reinterpret_cast<std::uintptr_t>(aligned);
    ::operator delete(scalar);
    ::operator delete(aligned, std::align_val_t{64});
    REQUIRE(address % 64 == 0);
    REQUIRE(after - before == (ngen::workload::allocation_probe_enabled() ? 2U : 0U));
#if NGEN_ALLOCATION_PROBE
    struct HandlerCalled : std::exception {};
    const auto previous = std::set_new_handler(+[] { throw HandlerCalled{}; });
    struct Restore {
        std::new_handler previous;
        ~Restore() { std::set_new_handler(previous); }
    } restore{previous};
    volatile auto impossible = std::numeric_limits<std::size_t>::max();
    bool called = false;
    try {
        void *value = ::operator new(impossible);
        ::operator delete(value);
    } catch (const HandlerCalled &) {
        called = true;
    } catch (const std::bad_alloc &) {
        FAIL("allocation probe bypassed the installed new handler");
    }
    REQUIRE(called);
#endif
}

TEST_CASE("flight workload shares eighty asteroid triangles across one hundred instances") {
    const ngen::assets::Bounds ship{{-160, -18.9256668F, -34}, {160, 23.0743351F, 34}};
    const auto result = ngen::workload::make_fixture(ship);
    INFO((std::get_if<ngen::render::Failure>(&result)
              ? std::get<ngen::render::Failure>(result).message
              : "fixture generated"));
    REQUIRE(std::holds_alternative<ngen::workload::Fixture>(result));
    const auto &fixture = std::get<ngen::workload::Fixture>(result);
    REQUIRE(fixture.vertices.size() == 240);
    REQUIRE(fixture.indices.size() / 3 == 80);
    REQUIRE(fixture.instances.size() == 100);
    REQUIRE(238080 + fixture.indices.size() / 3 * fixture.instances.size() <= 250000);
    for (std::size_t i = 0; i < fixture.indices.size(); ++i) {
        REQUIRE(fixture.indices[i] < fixture.vertices.size());
        const auto &vertex = fixture.vertices[fixture.indices[i]];
        float length = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            REQUIRE(std::isfinite(vertex.position[axis]));
            length += vertex.normal[axis] * vertex.normal[axis];
        }
        REQUIRE(std::abs(length - 1) < .0001F);
    }
    for (const auto &instance : fixture.instances) {
        float distance2 = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto delta = instance.model.columns[12 + axis] - fixture.center[axis];
            distance2 += delta * delta;
        }
        REQUIRE(distance2 > 160 * 160);
        REQUIRE(distance2 < 190 * 190);
        for (std::size_t column = 0; column < 3; ++column)
            for (std::size_t other = 0; other < 3; ++other) {
                float product = 0;
                for (std::size_t row = 0; row < 3; ++row)
                    product += instance.model.columns[column * 4 + row] *
                               instance.normalMatrix.columns[other * 4 + row];
                REQUIRE(std::abs(product - (column == other ? 1.0F : 0.0F)) < .0001F);
            }
    }
    const auto repeated = ngen::workload::make_fixture(ship);
    REQUIRE(std::holds_alternative<ngen::workload::Fixture>(repeated));
    const auto &same = std::get<ngen::workload::Fixture>(repeated);
    for (std::size_t i = 0; i < same.vertices.size(); ++i) {
        REQUIRE(same.vertices[i].position == fixture.vertices[i].position);
        REQUIRE(same.vertices[i].normal == fixture.vertices[i].normal);
        REQUIRE(same.vertices[i].uv == fixture.vertices[i].uv);
        REQUIRE(same.vertices[i].uv1 == fixture.vertices[i].uv1);
        REQUIRE(same.vertices[i].tangent == fixture.vertices[i].tangent);
    }
    for (std::size_t i = 0; i < same.instances.size(); ++i) {
        REQUIRE(same.instances[i].model.columns == fixture.instances[i].model.columns);
        REQUIRE(same.instances[i].normalMatrix.columns ==
                fixture.instances[i].normalMatrix.columns);
    }
    const auto changed = ngen::workload::make_fixture(ship, ngen::workload::fixture_seed + 1);
    REQUIRE(std::holds_alternative<ngen::workload::Fixture>(changed));
    REQUIRE(std::get<ngen::workload::Fixture>(changed).instances[0].model.columns !=
            fixture.instances[0].model.columns);
}

TEST_CASE("flight replay encloses admitted ship bounds at every camera orientation") {
    const ngen::assets::Bounds ship{{-10, -10, -10}, {10, 10, 10}};
    const auto result = ngen::workload::make_fixture(ship);
    REQUIRE(std::holds_alternative<ngen::workload::Fixture>(result));
    const auto &fixture = std::get<ngen::workload::Fixture>(result);
    for (const auto frame : {0U, 75U, 150U, 225U}) {
        const auto view =
            std::get<ngen::render::SceneView>(ngen::workload::view(fixture, frame, {1280, 720}));
        for (unsigned corner = 0; corner < 8; ++corner)
            for (const auto &matrix : {view.view_projection, view.light_view_projection}) {
                std::array<float, 4> clip{};
                for (std::size_t row = 0; row < 4; ++row) {
                    clip[row] = matrix.columns[12 + row];
                    for (std::size_t column = 0; column < 3; ++column)
                        clip[row] += matrix.columns[column * 4 + row] *
                                     ((corner & (1U << column)) ? 10.0F : -10.0F);
                }
                REQUIRE(std::abs(clip[0]) < clip[3]);
                REQUIRE(std::abs(clip[1]) < clip[3]);
                REQUIRE(clip[2] > 0);
                REQUIRE(clip[2] < clip[3]);
            }
    }
}

TEST_CASE("flight replay keeps the fixed asteroid workload inside the target frustum") {
    const ngen::assets::Bounds ship{{-160, -18.9256668F, -34}, {160, 23.0743351F, 34}};
    const auto made = ngen::workload::make_fixture(ship);
    REQUIRE(std::holds_alternative<ngen::workload::Fixture>(made));
    const auto &fixture = std::get<ngen::workload::Fixture>(made);
    for (const auto frame : {0U, 75U, 150U, 225U, 300U, 375U, 450U, 525U}) {
        const auto selected = ngen::workload::view(fixture, frame, {1280, 720});
        INFO((std::get_if<ngen::render::Failure>(&selected)
                  ? std::get<ngen::render::Failure>(selected).message
                  : "view generated"));
        REQUIRE(std::holds_alternative<ngen::render::SceneView>(selected));
        const auto &view = std::get<ngen::render::SceneView>(selected);
        for (const auto &instance : fixture.instances) {
            bool enclosed = true;
            for (unsigned corner = 0; corner < 8; ++corner) {
                std::array<float, 4> world{0, 0, 0, 1};
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    world[axis] = instance.model.columns[12 + axis];
                    for (std::size_t column = 0; column < 3; ++column)
                        world[axis] += instance.model.columns[column * 4 + axis] *
                                       ((corner & (1U << column)) ? 1.2F : -1.2F);
                }
                for (const auto &matrix : {view.view_projection, view.light_view_projection}) {
                    std::array<float, 4> clip{};
                    for (std::size_t row = 0; row < 4; ++row)
                        for (std::size_t column = 0; column < 4; ++column)
                            clip[row] += matrix.columns[column * 4 + row] * world[column];
                    enclosed = enclosed && clip[3] > 0 && std::abs(clip[0]) < clip[3] &&
                               std::abs(clip[1]) < clip[3] && clip[2] > 0 && clip[2] < clip[3];
                }
            }
            REQUIRE(enclosed);
        }
    }
    const auto first =
        std::get<ngen::render::SceneView>(ngen::workload::view(fixture, 0, {1280, 720}));
    const auto repeated =
        std::get<ngen::render::SceneView>(ngen::workload::view(fixture, 600, {1280, 720}));
    REQUIRE(first.view_projection.columns == repeated.view_projection.columns);
    REQUIRE(first.lod == 2);
    REQUIRE(
        std::holds_alternative<ngen::render::Failure>(ngen::workload::view(fixture, 0, {0, 720})));
}

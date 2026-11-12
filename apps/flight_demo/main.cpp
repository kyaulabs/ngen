#include "flight_demo/app.hpp"
#include "flight_demo/view.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T rendered(std::variant<T, ngen::render::Failure> result) {
    if (const auto *error = std::get_if<ngen::render::Failure>(&result))
        throw std::runtime_error(error->message);
    return std::get<T>(std::move(result));
}
template <class T> T simulated(ngen::Result<T> result) {
    if (const auto *error = std::get_if<ngen::Error>(&result))
        throw std::runtime_error("flight state rejected: " +
                                 std::to_string(static_cast<unsigned>(*error)));
    return std::get<T>(std::move(result));
}
std::uint32_t number(std::string_view text, std::uint32_t maximum) {
    std::uint32_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value <= maximum,
            "invalid flight numeric option");
    return value;
}
ngen::demo::Options options(int argc, char **argv) {
    ngen::demo::Options output;
    for (int i = 2; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--verify")
            output.verify = true;
        else if (arg == "--replay")
            output.replay = true;
        else if (arg == "--walkthrough")
            output.walkthrough = true;
        else if (arg == "--validation")
            output.validation = true;
        else if (arg == "--mute")
            output.mute = true;
        else if (arg.starts_with("--frames=")) {
            output.frames = number(arg.substr(9), 3600);
            require(output.frames > 0, "explicit frame count must be positive");
        } else if (arg.starts_with("--warmup="))
            output.warmup = number(arg.substr(9), 600);
        else if (arg.starts_with("--width="))
            output.extent.width = number(arg.substr(8), 3840);
        else if (arg.starts_with("--height="))
            output.extent.height = number(arg.substr(9), 2160);
        else if (arg.starts_with("--report="))
            output.report = arg.substr(9);
        else if (arg.starts_with("--capture="))
            output.capture = arg.substr(10);
        else
            throw std::runtime_error("unknown flight option");
    }
    require(output.extent.width >= 320 && output.extent.height >= 180,
            "flight extent is too small");
    require(!(output.replay && output.walkthrough), "choose replay or SDL walkthrough, not both");
    require(!(output.replay || output.walkthrough) || output.frames > 0,
            "fixed-tick replay or SDL walkthrough requires a bounded frame count");
    require(output.report.empty() || output.frames > 0, "frame report requires a bounded run");
    for (const auto &path : {output.report, output.capture})
        require(path.empty() || !std::filesystem::exists(std::filesystem::symlink_status(path)),
                "flight output must be a new path");
    require(output.report.empty() || output.report.extension() == ".csv", "report must use .csv");
    require(output.capture.empty() || output.capture.extension() == ".bmp",
            "capture must use .bmp");
    require(output.report.empty() || output.capture.empty() || output.report != output.capture,
            "flight outputs must be distinct");
    return output;
}
void verify(const ngen::demo::Preset &preset, const ngen::assets::Scene &package) {
    auto game = simulated(ngen::flight::Flight::create(preset.ship.flight, preset.contacts));
    const auto before = game->snapshot();
    (void)simulated(game->submit(
        {ngen::Tick{1}, 1, before.entity, ngen::flight::SetMode{ngen::flight::Mode::direct}}));
    const auto after = simulated(game->step());
    auto rig = simulated(ngen::flight::CameraRig::create(preset.ship_radius));
    const auto camera = simulated(rig.sample({before, after}, 1, preset.camera_obstacles));
    const auto frame = rendered(ngen::demo::present(preset, camera, {1280, 720}));
    for (std::size_t axis = 0; axis < 3; ++axis)
        require(std::abs(camera.camera.position_metres[axis] -
                         preset.ship.flight.flight_camera.position_metres[axis]) < 1e-4,
                "authored first-person sensor mount differs");
    require(frame.scene.root_transform.columns[15] == 1, "ship root is not affine");
    std::printf(
        "flight_verified=1 package_crc=%08x instances=%zu triangles=%llu static_contacts=%zu "
        "fixed_hz=60 first_person_mount=1 mass_kg=%.0f camera_fov=%.0f\n",
        package.checksum, package.instances.size(),
        static_cast<unsigned long long>(preset.triangles), preset.contacts.size(),
        after.body.mass_kg, camera.vertical_fov_degrees);
    const auto banks = static_cast<std::size_t>(
        std::count_if(preset.rcs_materials.begin(), preset.rcs_materials.end(),
                      [](const auto &material) { return material.has_value(); }));
    std::printf("flight_rcs_banks=%zu flight_rcs_sockets=%zu\n", banks, banks * 2);
    if (banks) {
        // Check the compiled visual mix against actual immutable plume transforms,
        // independently of authoring provenance or duplicated expected coefficients.
        for (std::size_t axis = 0; axis < 6; ++axis)
            for (const float sign : {-1.0F, 1.0F}) {
                std::array<float, 3> force{}, torque{};
                if (axis < 3)
                    force[axis] = preset.ship.flight.maximum_force_newtons[axis] * sign;
                else
                    torque[axis - 3] =
                        preset.ship.flight.maximum_torque_newton_metres[axis - 3] * sign;
                const auto levels =
                    rendered(ngen::demo::rcs_levels(preset.ship, {0, 0, 0, 1}, force, torque));
                if (axis == 2 && sign < 0) {
                    require(levels == ngen::demo::RcsLevels{}, "rear drive was duplicated by RCS");
                    continue;
                }
                std::array<double, 6> wrench{};
                const auto &root = preset.ship.asset_to_body.columns;
                for (std::size_t bank = 0; bank < ngen::demo::rcs_bank_count; ++bank) {
                    const auto material = preset.rcs_materials[bank];
                    if (!material)
                        throw std::runtime_error("RCS bank binding disappeared");
                    const auto index = *material;
                    const auto instance = std::find_if(
                        package.instances.begin(), package.instances.end(), [&](const auto &i) {
                            return package.primitives[i.primitive].material == index;
                        });
                    if (instance == package.instances.end())
                        throw std::runtime_error("RCS bank geometry disappeared");
                    std::array<double, 3> center{}, direction{};
                    const auto &model = instance->transform;
                    for (std::size_t row = 0; row < 3; ++row) {
                        center[row] = root[12 + row];
                        for (std::size_t column = 0; column < 3; ++column) {
                            center[row] +=
                                static_cast<double>(root[column * 4 + row]) * model[12 + column];
                            direction[row] -=
                                static_cast<double>(root[column * 4 + row]) * model[column];
                        }
                    }
                    const auto length =
                        std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                                  direction[2] * direction[2]);
                    require(std::isfinite(length) && length > 0, "invalid RCS nozzle axis");
                    for (std::size_t component = 0; component < 3; ++component) {
                        direction[component] *= levels[bank] / length;
                        wrench[component] += direction[component];
                    }
                    wrench[3] += (center[1] * direction[2] - center[2] * direction[1]) / 100;
                    wrench[4] += (center[2] * direction[0] - center[0] * direction[2]) / 100;
                    wrench[5] += (center[0] * direction[1] - center[1] * direction[0]) / 100;
                }
                require(wrench[axis] * sign > 0, "RCS jets oppose their requested wrench axis");
                for (std::size_t other = 0; other < 6; ++other)
                    if (other != axis)
                        require(std::abs(wrench[other]) < std::abs(wrench[axis]) * 1e-4,
                                "RCS mix introduces an unintended force or moment");
            }
        std::puts("flight_rcs_geometry_verified=1");
    }
}
} // namespace
int main(int argc, char **argv) try {
    if (argc < 2 || std::string_view(argv[1]) == "--help") {
        std::fputs("Usage: flight_demo scene.ngas [--verify] [--replay|--walkthrough --frames=N] "
                   "[--warmup=N] "
                   "[--validation] [--mute] [--width=N --height=N] "
                   "[--report=new.csv] [--capture=new.bmp]\n",
                   stdout);
        return argc < 2 ? 2 : 0;
    }
    ngen::demo::Options settings;
    try {
        settings = options(argc, argv);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Flight arguments: %s\n", error.what());
        return 2;
    }
    auto loaded = ngen::assets::load_package_file(argv[1]);
    if (const auto *error = std::get_if<ngen::assets::Failure>(&loaded))
        throw std::runtime_error(error->message);
    auto package = std::get<ngen::assets::PackagePin>(std::move(loaded));
    const auto preset = rendered(ngen::demo::make_preset(package, ngen::demo::frigate_profile()));
    if (settings.verify) {
        verify(preset, *package);
        return 0;
    }
    ngen::demo::run(package, preset, settings);
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Flight demo: %s\n", error.what());
    return 1;
}

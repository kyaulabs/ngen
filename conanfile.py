from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain
from conan.tools.env import VirtualRunEnv


class NgenDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    options = {"renderer": [True, False], "asset_tools": [True, False]}
    default_options = {
        "renderer": False,
        "asset_tools": False,
        "sdl/*:shared": True,
        **{f"sdl/*:{name}": False for name in (
            "audio", "gpu", "render", "camera", "joystick", "haptic", "hidapi",
            "power", "sensor", "dialog", "tray", "opengl", "opengles", "directx",
            "libudev", "dbus", "x11",
        )},
        "xkbcommon/*:with_x11": False,
    }

    def requirements(self):
        self.requires("enkits/1.12")
        self.test_requires("catch2/3.16.0")
        if self.options.asset_tools:
            self.requires("fastgltf/0.9.1")
            self.requires("simdjson/4.6.4")
            self.requires("libpng/1.6.59")
            self.requires("mikktspace/cci.20200325@ngen/defined-rotate")
            self.requires("meshoptimizer/1.0")
        if self.options.renderer:
            # Only Linux needs the local Wayland ownership repair; Windows keeps
            # the independently verified upstream recipe and binaries.
            reference = "sdl/3.4.16@ngen/wayland" if self.settings.os == "Linux" else "sdl/3.4.16"
            self.requires(reference)
            self.requires("vulkan-memory-allocator/3.3.0")
            self.requires("vulkan-headers/1.3.296.0", override=True)
            if self.settings.os == "Linux":
                # The private protocol peer uses the same locked Wayland ABI as SDL.
                self.test_requires("wayland/1.24.0")

    def generate(self):
        CMakeDeps(self).generate()
        VirtualRunEnv(self).generate()
        toolchain = CMakeToolchain(self, generator="Ninja")
        toolchain.user_presets_path = False
        if self.options.asset_tools:
            toolchain.variables["NGEN_BUILD_ASSET_TOOLS"] = True
        toolchain.generate()

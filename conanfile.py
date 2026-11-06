from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain
from conan.tools.env import VirtualRunEnv


class NgenDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    options = {"renderer": [True, False], "shaders": [True, False]}
    default_options = {
        "renderer": False,
        "shaders": False,
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

    def build_requirements(self):
        if self.options.shaders:
            self.tool_requires("slangc/2026.21@ngen/tools")
            self.tool_requires("spirv-tools/1.4.357.0", options={"shared": False, "build_executables": True})

    def generate(self):
        CMakeDeps(self).generate()
        VirtualRunEnv(self).generate()
        toolchain = CMakeToolchain(self, generator="Ninja")
        toolchain.user_presets_path = False
        if self.options.shaders:
            toolchain.variables["NGEN_BUILD_SHADERS"] = True
            toolchain.variables["NGEN_SLANG_ROOT"] = self.dependencies.build["slangc"].package_folder.replace("\\", "/")
            validator = self.dependencies.build["spirv-tools"]
            suffix = ".exe" if self.settings_build.os == "Windows" else ""
            toolchain.variables["NGEN_SPIRV_VAL_EXECUTABLE"] = (validator.package_folder.replace("\\", "/") + "/bin/spirv-val" + suffix)
        toolchain.generate()

import json
import os
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMakeDeps, CMakeToolchain
from conan.tools.env import VirtualRunEnv
from conan.tools.files import save


class NgenDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    options = {"renderer": [True, False], "asset_tools": [True, False], "assets": [True, False], "shaders": [True, False], "inspector": [True, False], "physics": [True, False], "audio": [True, False]}
    default_options = {
        "renderer": False,
        "asset_tools": False,
        "assets": False,
        "shaders": False,
        "inspector": False,
        "physics": False,
        "audio": False,
        "sdl/*:shared": True,
        "libalsa/*:shared": True,
        **{f"sdl/*:{name}": False for name in (
            "gpu", "render", "camera", "joystick", "haptic", "hidapi",
            "power", "sensor", "dialog", "tray", "opengl", "opengles", "directx",
            "libudev", "dbus", "x11",
        )},
        "xkbcommon/*:with_x11": False,
    }

    def validate(self):
        if self.options.audio and not self.options.renderer:
            raise ConanInvalidConfiguration("The audio frontend requires the existing SDL platform graph")
        if self.options.inspector and not (self.options.renderer and self.options.assets and self.options.shaders):
            raise ConanInvalidConfiguration("The inspector frontend requires renderer, assets and offline shaders")

    def requirements(self):
        self.requires("enkits/1.12")
        self.test_requires("catch2/3.16.0")
        if self.options.physics:
            self.requires("joltphysics/5.6.0@ngen/headless-double")
        if self.options.inspector:
            # Build the packaged SDL backend against our already-locked SDL, not
            # the optional binding recipe's independent SDL version range.
            self.requires("imgui/1.92.9b", options={"with_sdl3_binding": False})
        if self.options.assets or self.options.asset_tools:
            self.requires("zlib/1.3.2")
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
            sdl_options = {"audio": bool(self.options.audio)}
            if self.settings.os == "Linux" and self.options.audio:
                # One native ALSA client, not a new sound framework or server.
                sdl_options.update(alsa=True, pulseaudio=False, sndio=False)
                # The eager-binding SDK regression directly calls this client;
                # do not rely on a shared SDL dependency's transitive lib trait.
                self.test_requires("libalsa/1.2.16.1")
            self.requires(reference, options=sdl_options)
            self.requires("vulkan-memory-allocator/3.3.0")
            self.requires("vulkan-headers/1.3.296.0", override=True)
            if self.settings.os == "Linux":
                # The private protocol peer uses the same locked Wayland ABI as SDL.
                self.test_requires("wayland/1.24.0")

    def build_requirements(self):
        if self.options.asset_tools:
            self.tool_requires("ktx/4.4.2", options={"tools": True})
        if self.options.shaders:
            self.tool_requires("slangc/2026.21@ngen/tools")
            self.tool_requires("spirv-tools/1.4.357.0", options={"shared": False, "build_executables": True})

    def generate(self):
        CMakeDeps(self).generate()
        VirtualRunEnv(self).generate()
        if self.options.audio:
            packages = []
            for _, dependency in self.dependencies.host.items():
                if not dependency.package_folder:
                    continue
                info = dependency.cpp_info.aggregated_components()
                packages.append({
                    "name": dependency.ref.name,
                    "reference": dependency.ref.repr_notime(),
                    "root": dependency.package_folder.replace("\\", "/"),
                    "libdirs": info.libdirs,
                    "bindirs": info.bindirs,
                })
            save(self, os.path.join(self.generators_folder, "demo-runtime.json"),
                 json.dumps({"schema": 1, "os": str(self.settings.os),
                             "configuration": str(self.settings.build_type),
                             "sanitizer": self.conf.get("user.ngen:sanitizer", default=""),
                             "packages": packages}, indent=2) + "\n")
        toolchain = CMakeToolchain(self, generator="Ninja")
        toolchain.user_presets_path = False
        if self.options.physics:
            toolchain.variables["NGEN_BUILD_PHYSICS"] = True
        if self.options.audio:
            toolchain.variables["NGEN_BUILD_AUDIO"] = True
        if self.options.inspector:
            toolchain.variables["NGEN_BUILD_INSPECTOR"] = True
            toolchain.variables["NGEN_IMGUI_BINDINGS_DIR"] = self.dependencies["imgui"].package_folder.replace("\\", "/") + "/res/bindings"
        if self.options.assets or self.options.asset_tools:
            toolchain.variables["NGEN_BUILD_ASSETS"] = True
        if self.options.asset_tools:
            toolchain.variables["NGEN_BUILD_ASSET_TOOLS"] = True
            suffix = ".exe" if self.settings_build.os == "Windows" else ""
            toolchain.variables["NGEN_KTX_EXECUTABLE"] = self.dependencies.build["ktx"].package_folder.replace("\\", "/") + "/bin/ktx" + suffix
        if self.options.shaders:
            toolchain.variables["NGEN_BUILD_SHADERS"] = True
            toolchain.variables["NGEN_SLANG_ROOT"] = self.dependencies.build["slangc"].package_folder.replace("\\", "/")
            validator = self.dependencies.build["spirv-tools"]
            suffix = ".exe" if self.settings_build.os == "Windows" else ""
            toolchain.variables["NGEN_SPIRV_VAL_EXECUTABLE"] = (validator.package_folder.replace("\\", "/") + "/bin/spirv-val" + suffix)
        toolchain.generate()

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class NgenDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("enkits/1.12")
        self.test_requires("catch2/3.16.0")

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self, generator="Ninja")
        toolchain.user_presets_path = False
        toolchain.generate()

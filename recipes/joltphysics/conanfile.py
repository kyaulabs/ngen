from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir
import os


class HeadlessDoubleJolt(ConanFile):
    name = "joltphysics"
    version = "5.6.0"
    license = "MIT"
    homepage = "https://github.com/jrouwe/JoltPhysics"
    description = "NGEN double-position CPU-only Jolt configuration"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}
    implements = ["auto_shared_fpic"]

    def layout(self):
        cmake_layout(self, src_folder="src")

    def validate(self):
        check_min_cppstd(self, 20)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["DOUBLE_PRECISION"] = True
        tc.cache_variables["CPP_EXCEPTIONS_ENABLED"] = True
        tc.cache_variables["CPP_RTTI_ENABLED"] = True
        for option in (
            "TARGET_UNIT_TESTS", "TARGET_HELLO_WORLD", "TARGET_PERFORMANCE_TEST",
            "TARGET_SAMPLES", "TARGET_VIEWER", "JPH_USE_DX12", "JPH_USE_VK",
            "JPH_USE_MTL", "JPH_USE_CPU_COMPUTE", "JPH_BUILD_SHARED_LIBS",
            "INTERPROCEDURAL_OPTIMIZATION", "OVERRIDE_CXX_FLAGS",
            "GENERATE_DEBUG_SYMBOLS", "ENABLE_ALL_WARNINGS", "ENABLE_OBJECT_STREAM",
            "DEBUG_RENDERER_IN_DEBUG_AND_RELEASE", "DEBUG_RENDERER_IN_DISTRIBUTION",
            "PROFILER_IN_DEBUG_AND_RELEASE", "PROFILER_IN_DISTRIBUTION",
            "FLOATING_POINT_EXCEPTIONS_ENABLED", "USE_SSE4_1", "USE_SSE4_2",
            "USE_AVX", "USE_AVX2", "USE_AVX512", "USE_LZCNT", "USE_TZCNT",
            "USE_F16C", "USE_FMADD",
        ):
            tc.cache_variables[option] = False
        runtime = self.settings.get_safe("compiler.runtime")
        if runtime is not None:
            tc.cache_variables["USE_STATIC_MSVC_RUNTIME_LIBRARY"] = runtime == "static"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "Build"))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        copy(self, "*.h", src=os.path.join(self.source_folder, "Jolt", "Shaders"),
             dst=os.path.join(self.package_folder, "include", "Jolt", "Shaders"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.libs = ["Jolt"]
        self.cpp_info.defines = ["JPH_DOUBLE_PRECISION", "JPH_OBJECT_LAYER_BITS=16"]
        self.cpp_info.set_property("cmake_file_name", "Jolt")
        self.cpp_info.set_property("cmake_target_name", "Jolt::Jolt")
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.append("pthread")

"""Pinned upstream offline compiler; no Slang runtime library is exposed to NGEN."""
import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get


class SlangCompiler(ConanFile):
    name = "slangc"
    package_type = "application"
    license = "Apache-2.0 WITH LLVM-exception"
    homepage = "https://shader-slang.org/"
    description = "NGEN's isolated upstream Slang compiler candidate"
    settings = "os", "arch"

    def validate(self):
        if str(self.settings.os) not in ("Linux", "Windows") or str(self.settings.arch) != "x86_64":
            raise ConanInvalidConfiguration("Pinned Slang tools support only Linux/Windows x86_64")

    def build(self):
        get(self, **self.conan_data["sources"][str(self.version)][str(self.settings.os)], strip_root=False)

    def package(self):
        executable = "slangc.exe" if self.settings.os == "Windows" else "slangc"
        copy(self, executable, src=os.path.join(self.build_folder, "bin"), dst=os.path.join(self.package_folder, "bin"))
        copy(self, "*.slang", src=os.path.join(self.build_folder, "bin"), dst=os.path.join(self.package_folder, "bin"))
        for directory in ("bin", "lib"):
            # Windows ships private DLLs beside slangc; Linux uses the lib RUNPATH.
            pattern = "*.dll" if directory == "bin" else "*"
            copy(self, pattern, src=os.path.join(self.build_folder, directory), dst=os.path.join(self.package_folder, directory))
        copy(self, "LICENSE", src=self.build_folder, dst=os.path.join(self.package_folder, "licenses"))
        for directory in ("LICENSES", "third-party-notices"):
            copy(self, "*", src=os.path.join(self.build_folder, directory), dst=os.path.join(self.package_folder, "licenses", directory))

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.buildenv_info.prepend_path("PATH", os.path.join(self.package_folder, "bin"))

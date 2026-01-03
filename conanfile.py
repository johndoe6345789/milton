from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps
from conan.tools.files import copy
import os

class MiltonConan(ConanFile):
    """
    Conan 2.x consumer for Milton.
    - Uses Conan to provide SDL2 and a consistent CMake toolchain.
    - Keeps system-provided X11/OpenGL/GTK2 on Linux (install via distro packages).
    """
    name = "milton-consumer"
    version = "0.1"
    settings = "os", "arch", "compiler", "build_type"

    # SDL2 is the only hard dependency we vendor via Conan here.
    # Keep GTK2/X11/OpenGL as system packages (distro), as they are typically
    # tightly integrated with the OS and drivers.
    requires = (
        "sdl/2.30.10",
    )

    generators = ()

    def layout(self):
        # Conan's default layout is fine; we keep it explicit for clarity.
        pass

    def generate(self):
        tc = CMakeToolchain(self)

        # Prefer Ninja when available; the driver script will set CMAKE_GENERATOR.
        tc.cache_variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True

        # Help some FindOpenGL variants pick GLVND when present.
        tc.cache_variables["OpenGL_GL_PREFERENCE"] = "GLVND"

        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def package(self):
        # No package output; consumer only.
        pass

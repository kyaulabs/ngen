cmake_minimum_required(VERSION 3.28)
project(ngen_window_capture LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus)
find_package(KPipeWire REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
add_custom_command(OUTPUT screencast-client.h screencast-protocol.c
  COMMAND "${WAYLAND_SCANNER}" client-header "${CMAKE_CURRENT_SOURCE_DIR}/screencast.xml" screencast-client.h
  COMMAND "${WAYLAND_SCANNER}" private-code "${CMAKE_CURRENT_SOURCE_DIR}/screencast.xml" screencast-protocol.c
  DEPENDS screencast.xml VERBATIM)
add_executable(ngen_window_capture main.cpp screencast-protocol.c screencast-client.h)
target_include_directories(ngen_window_capture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(ngen_window_capture PRIVATE Qt6::Core Qt6::Gui Qt6::DBus K::KPipeWireRecord PkgConfig::WAYLAND)
target_compile_options(ngen_window_capture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion;-Werror>)

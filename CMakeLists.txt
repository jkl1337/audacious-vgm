cmake_minimum_required(VERSION 3.12)

project(audacious-vgm)

include(GenerateExportHeader)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(BUILD_LIBAUDIO OFF CACHE BOOL "" FORCE)
set(BUILD_LIBEMU ON CACHE BOOL "" FORCE)
set(BUILD_LIBPLAYER ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(USE_SANITIZERS ON CACHE BOOL "")

add_subdirectory(libvgm)

add_library(vgm SHARED src/plugin.cc)
generate_export_header(vgm)

target_link_libraries(vgm vgm-emu_obj vgm-player_obj vgm-utils_obj)

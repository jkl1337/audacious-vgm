cmake_minimum_required(VERSION 3.12)

project(audacious-vgm)

include(FindPkgConfig)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(BUILD_LIBAUDIO OFF CACHE BOOL "" FORCE)
set(BUILD_LIBEMU ON CACHE BOOL "" FORCE)
set(BUILD_LIBPLAYER ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_VGM2WAV ON CACHE BOOL "" FORCE)
set(USE_SANITIZERS ON CACHE BOOL "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(libvgm)

find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
pkg_search_module(audacious REQUIRED IMPORTED_TARGET audacious>=3.9)

#qt5_create_translation(QM_FILES src/device-panel.cc vgm_es.ts OPTIONS
#        -tr-function-alias QT_TR_NOOP+=N_)

set(CMAKE_AUTOMOC ON)

add_library(vgm MODULE src/types.h src/vfsloader.cc src/gzfile.cc
        src/libvgm-dev-meta.cpp src/plugin.cc src/plugin.h src/internal.cc
        src/internal.h src/player.cc
        src/config.h src/tags.cc src/tags.h src/config.cc src/prefs.h
        src/prefs.cc src/comm.cc src/comm.h src/device-panel.cc src/device-panel.h src/i18n.h src/libvgm-int.cpp src/libvgm-int.h src/device-select.cc src/device-select.h src/channel-select.cc src/channel-select.h src/aud-comm.cc)


set_target_properties(vgm PROPERTIES PREFIX "")

add_executable(tester src/tester.cpp src/libvgm-dev-meta.cpp)

target_include_directories(tester PRIVATE libvgm)

target_link_libraries(vgm vgm-emu_obj vgm-player_obj vgm-utils_obj PkgConfig::audacious Qt5::Widgets audqt)

cmake_minimum_required(VERSION 3.14)

project(audvgm)

include(FindPkgConfig)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

pkg_search_module(audacious REQUIRED IMPORTED_TARGET audacious>=3.9)

add_library(vgm MODULE
        comm.cc config.cc config.h gzfile.cc gzfile.h
        log.h player.cc plugin.cc plugin.h tags.cc tags.h
        vfsloader.cc vfsloader.h)

set_target_properties(vgm PROPERTIES PREFIX "")
# add_executable(tester vgmtk/tester.cpp vgmtk/libvgm-dev-meta.cpp)
target_link_libraries(vgm corelib-obj vgm-player_obj vgm-emu_obj vgm-utils_obj PkgConfig::audacious) # Qt5::Widgets audqt)

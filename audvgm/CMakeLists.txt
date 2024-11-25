cmake_minimum_required(VERSION 3.14)

project(audvgm)

include(FindPkgConfig)

pkg_search_module(audacious REQUIRED IMPORTED_TARGET audacious>=3.9)

find_library(AUDQT audqt)

add_library(vgm MODULE
        comm.cc config.cc config.h gzfile.cc gzfile.h
        log.h player.cc plugin.cc plugin.h tags.cc tags.h
        vfsloader.cc vfsloader.h)

set_target_properties(vgm PROPERTIES PREFIX "")
if (APPLE)
    set_target_properties(vgm PROPERTIES SUFFIX ".dylib")
endif ()
target_compile_options(vgm PRIVATE -Wall -Wextra)

# add_executable(tester vgmtk/tester.cpp vgmtk/libvgm-dev-meta.cpp)
target_link_libraries(vgm corelib-obj vgm-player vgm-emu vgm-utils qtui PkgConfig::audacious ${AUDQT}) # Qt5::Widgets ${AUDQT})

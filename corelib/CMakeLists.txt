cmake_minimum_required(VERSION 3.16)
project(corelib)

set(VGMTK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/..)

add_library(corelib-obj OBJECT types.h
        libvgm-dev-meta.cc libvgm-dev-meta.h libvgm-iface.cc libvgm-iface.h
        libvgm-rt.cc libvgm-rt.h tagging.h)
target_include_directories(corelib-obj PUBLIC ${VGMTK_SOURCE_DIR})

target_link_libraries(corelib-obj vgm-player vgm-emu)
set_property(TARGET corelib-obj PROPERTY POSITION_INDEPENDENT_CODE ON)

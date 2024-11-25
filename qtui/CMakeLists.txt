cmake_minimum_required(VERSION 3.14)
project(qtui)
set(VGMTK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/..)

set(CMAKE_AUTOMOC ON)

option(AUDVGM_USE_QT5 "Force using Qt5" OFF)

if (NOT AUDVGM_USE_QT5)
    find_package(Qt6 COMPONENTS Gui Widgets LinguistTools)
endif ()

if (NOT Qt6_FOUND)
    find_package(
            Qt5 5.15
            COMPONENTS Gui Widgets LinguistTools
            REQUIRED)
endif ()

# qt5_create_translation(QM_FILES src/device-panel.cc vgm_es.ts OPTIONS
# -tr-function-alias QT_TR_NOOP+=N_)

add_library(
        qtui STATIC
        channel-select.cc
        channel-select.h
        device-select.cc
        device-select.h
        qvgm-factory.h
        core-selector.cc
        core-selector.h
        device-control-panel.h
        device-control-panel.cc)
target_sources(qtui PRIVATE i18n.h)
set_property(TARGET qtui PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_options(qtui PRIVATE -Wall -Wextra -Wno-unknown-pragmas)
target_include_directories(qtui PUBLIC ${VGMTK_SOURCE_DIR})
target_link_libraries(qtui PRIVATE corelib-obj Qt::Widgets Qt::Gui)

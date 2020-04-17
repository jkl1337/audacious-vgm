cmake_minimum_required(VERSION 3.14)

project(qtui)
set(VGMTK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/..)

set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)

# qt5_create_translation(QM_FILES src/device-panel.cc vgm_es.ts OPTIONS
#         -tr-function-alias QT_TR_NOOP+=N_)

add_library(qtui STATIC qvgm-player.cc qvgm-player.h channel-select.cc channel-select.h device-select.cc device-select.h qvgm-factory.h core-selector.cc core-selector.h)
target_sources(qtui PRIVATE i18n.h)
set_property(TARGET qtui PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(qtui PUBLIC ${VGMTK_SOURCE_DIR})
target_link_libraries(qtui corelib-obj Qt5::Widgets)

cmake_minimum_required(VERSION 3.14)

project(qtui)

find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)

# qt5_create_translation(QM_FILES src/device-panel.cc vgm_es.ts OPTIONS
#         -tr-function-alias QT_TR_NOOP+=N_)

add_library(qtui OBJECT device-panel.cc device-panel.h)
target_sources(PRIVATE i18n.h)
# set(CMAKE_AUTOMOC ON)

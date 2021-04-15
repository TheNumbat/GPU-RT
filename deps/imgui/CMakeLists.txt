cmake_minimum_required(VERSION 3.17)

project(imgui LANGUAGES CXX)

set(SOURCES_IMGUI "imgui.cpp"
                  "imgui.h"
                  "imconfig.h"
                  "imgui_internal.h"
                  "imstb_rectpack.h"
                  "imstb_textedit.h"
                  "imstb_truetype.h"
                  "imgui_draw.cpp"
                  "imgui_widgets.cpp"
                  "imgui_demo.cpp")

add_library(imgui STATIC ${SOURCES_IMGUI})

target_include_directories(imgui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

if(MSVC)
    target_compile_options(imgui PRIVATE /MP /fp:fast)
else()
    target_compile_options(imgui PRIVATE -fconcepts -ffast-math)
endif()

set_target_properties(imgui PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)

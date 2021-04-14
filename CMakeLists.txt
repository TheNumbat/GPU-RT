
cmake_minimum_required(VERSION 3.17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# define project

project(fcpw-gpu VERSION 0.2 DESCRIPTION "fcpw-gpu" LANGUAGES CXX)

# define sources

set(SOURCES_CLIENT "src/main.cpp"
                   "src/gpurt.h"
                   "src/gpurt.cpp"
                   "src/platform/window.h"
                   "src/platform/window.cpp"
                   "src/platform/vulkan.h"
                   "src/platform/vulkan.cpp"
                   "src/util/image.h"
                   "src/util/files.h"
                   "src/util/files.cpp"
                   "src/util/camera.h"
                   "src/util/camera.cpp"
                   "src/scene/scene.h"
                   "src/scene/scene.cpp"
                   "src/scene/object.h"
                   "src/scene/object.cpp"
                   "src/scene/pose.h"
                   "src/scene/pose.cpp"
                   "src/scene/material.h"
                   "src/scene/material.cpp"
                   "src/platform/imgui_impl_sdl.h"
                   "src/platform/imgui_impl_sdl.cpp"
                   "src/platform/imgui_impl_vulkan.h"
                   "src/platform/imgui_impl_vulkan.cpp")

# setup OS-specific options

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "-framework AppKit")
	find_package(SDL2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIRS}/..)
	include_directories(${SDL2_INCLUDE_DIRS})
	link_directories(${SDL2_LIBRARY_DIRS})
	add_definitions(${SDL2_CFLAGS_OTHER})
endif()

if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    include_directories(${SDL2_INCLUDE_DIRS})
    link_directories(${SDL2_LIBRARY_DIRS})
    add_definitions(${SDL2_CFLAGS_OTHER})
    include_directories("src")
endif()

# define executable

if(WIN32)
    add_executable(fcpw-gpu WIN32 ${SOURCES_CLIENT})
else()
    add_executable(fcpw-gpu ${SOURCES_CLIENT})
endif()
set_target_properties(fcpw-gpu PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(fcpw-gpu PRIVATE /MP /W4 /WX /wd4201 /wd4840 /wd4100 /wd4267 /fp:fast)
else()
    target_compile_options(fcpw-gpu PRIVATE -Wall -fconcepts -Wextra -Werror -Wno-missing-braces -Wno-reorder -ffast-math -Wno-unused-parameter)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(fcpw-gpu PRIVATE Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

# shaders 

function(add_shader TARGET SHADER)
find_program(GLSLC glslc)

    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${SHADER})
    set(current-output-path ${CMAKE_CURRENT_SOURCE_DIR}/media/shaders/${SHADER}.spv)

    # Add a custom command to compile GLSL to SPIR-V.
    get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
    file(MAKE_DIRECTORY ${current-output-dir})

    add_custom_command(
           OUTPUT ${current-output-path}
           COMMAND ${GLSLC} -o ${current-output-path} ${current-shader-path}
           DEPENDS ${current-shader-path}
           IMPLICIT_DEPENDS CXX ${current-shader-path}
           VERBATIM)

    # Make sure our build depends on this output.
    set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
    target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

add_shader(fcpw-gpu "1.vert")
add_shader(fcpw-gpu "1.frag")

# define include paths

target_include_directories(fcpw-gpu PRIVATE "deps/")
target_include_directories(fcpw-gpu PRIVATE "src/")
target_include_directories(fcpw-gpu PRIVATE "deps/" "deps/assimp/include")
target_include_directories(fcpw-gpu PRIVATE "${CMAKE_BINARY_DIR}/deps/assimp/include")

# build dependencies

add_subdirectory("deps/imgui/")
add_subdirectory("deps/sf_libs/")
add_subdirectory("deps/nfd/")

find_package(Vulkan REQUIRED FATAL_ERROR)
target_include_directories(fcpw-gpu PRIVATE ${VULKAN_INCLUDE_DIRS})

set(ASSIMP_BUILD_COLLADA_IMPORTER TRUE)
set(ASSIMP_BUILD_OBJ_IMPORTER TRUE)
set(ASSIMP_BUILD_PLY_IMPORTER TRUE)
set(ASSIMP_BUILD_FBX_IMPORTER TRUE)
set(ASSIMP_BUILD_GLTF_IMPORTER TRUE)
set(ASSIMP_BUILD_3DS_IMPORTER TRUE)
set(ASSIMP_BUILD_STL_IMPORTER TRUE)
set(ASSIMP_BUILD_BLEND_IMPORTER TRUE)
set(ASSIMP_BUILD_COLLADA_EXPORTER TRUE)
add_subdirectory("deps/assimp/")
include_directories(${ASSIMP_INCLUDE_DIRS})

# link libraries

target_link_libraries(fcpw-gpu PRIVATE imgui)
target_link_libraries(fcpw-gpu PRIVATE sf_libs)
target_link_libraries(fcpw-gpu PRIVATE nfd)
target_link_libraries(fcpw-gpu PRIVATE ${Vulkan_LIBRARY})
target_link_libraries(fcpw-gpu PRIVATE assimp)

if(WIN32)
    target_include_directories(fcpw-gpu PRIVATE "deps/win")
    if(MSVC)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /IGNORE:4098 /IGNORE:4099")
    endif()
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    target_link_libraries(fcpw-gpu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps/win/SDL2/SDL2main.lib")
    target_link_libraries(fcpw-gpu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps/win/SDL2/SDL2.lib")
    target_link_libraries(fcpw-gpu PRIVATE Winmm)
    target_link_libraries(fcpw-gpu PRIVATE Version)
    target_link_libraries(fcpw-gpu PRIVATE Setupapi)
    target_link_libraries(fcpw-gpu PRIVATE Shcore)
endif()

if(LINUX)
    target_link_libraries(fcpw-gpu PRIVATE SDL2)
endif()

if(APPLE)
	target_link_libraries(fcpw-gpu PRIVATE ${SDL2_LIBRARIES})
endif()


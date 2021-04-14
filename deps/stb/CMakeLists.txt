cmake_minimum_required(VERSION 3.17)

project(stb LANGUAGES CXX)

set(SOURCES_STB "stb.cpp" "stb_image.h" "stb_image_write.h" "stb_perlin.h")
add_library(stb STATIC ${SOURCES_STB})

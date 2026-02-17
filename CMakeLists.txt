cmake_minimum_required(VERSION 3.20)
project(sh_renderer_main VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
  $<$<CONFIG:Release>:-O3>
  $<$<CONFIG:Release>:-march=native>
  # $<$<CONFIG:Debug>:-fsanitize=address>
  $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)
add_link_options(
  $<$<CONFIG:Release>:-O3>
  # $<$<CONFIG:Debug>:-fsanitize=address>
)

# Dependencies
find_package(GTest)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

find_path(TINYGLTF_INCLUDE_DIR tiny_gltf.h)
if(NOT TINYGLTF_INCLUDE_DIR)
  message(FATAL_ERROR "tinygltf not found")
endif()

find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
  message(FATAL_ERROR "nlohmann/json.hpp not found")
endif()

find_path(STB_IMAGE_INCLUDE_DIR NAMES stb_image.h PATH_SUFFIXES stb)
if(NOT STB_IMAGE_INCLUDE_DIR)
  message(FATAL_ERROR "stb_image not found")
endif()

find_path(TINYEXR_INCLUDE_DIR NAMES tinyexr.h PATH_SUFFIXES tinyexr)
if(NOT TINYEXR_INCLUDE_DIR)
  message(FATAL_ERROR "tinyexr not found")
endif()

# stb_image
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${STB_IMAGE_INCLUDE_DIR})

# nlohmann_json
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})

# tinygltf
add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE ${TINYGLTF_INCLUDE_DIR})
target_link_libraries(tinygltf INTERFACE stb_image nlohmann_json)

# tinyexr
add_library(tinyexr INTERFACE)
target_include_directories(tinyexr INTERFACE ${TINYEXR_INCLUDE_DIR})

# Renderer library
set(LIB_SOURCES 
    src/camera.cpp
    src/draw_depth.cpp
    src/draw_radiance.cpp
    src/draw_tonemap.cpp
    src/glad.c
    src/input.cpp
    src/interaction.cpp
    src/implementations.cpp
    src/loader.cpp
    src/render_target.cpp
    src/scene.cpp
    src/shader.cpp
    src/window.cpp)

set(LIB_HEADERS
    src/camera.h
    src/colorspace.h
    src/draw_depth.h
    src/draw_radiance.h
    src/glad.h
    src/input.h
    src/interaction.h
    src/loader.h
    src/render_target.h
    src/scene.h
    src/shader.h
    src/window.h)

add_library(sh_renderer SHARED ${LIB_SOURCES} ${LIB_HEADERS})
target_include_directories(sh_renderer PUBLIC 
    src
)
target_link_libraries(sh_renderer PUBLIC
    glog::glog
    gflags
    OpenGL::GL
    glfw
    Eigen3::Eigen
    tinygltf
    nlohmann_json
    stb_image
    tinyexr
    Threads::Threads
    ZLIB::ZLIB
)

# Main executable
add_executable(sh_renderer_main
    src/main.cpp
)

target_link_libraries(sh_renderer_main PRIVATE
    sh_renderer
)

# Enable testing
enable_testing()

add_executable(sh_renderer_test
    src/camera_test.cpp
    src/input_test.cpp
    src/interaction_test.cpp
    src/window_test.cpp
)

target_link_libraries(sh_renderer_test PRIVATE
    sh_renderer
    GTest::gtest_main
)

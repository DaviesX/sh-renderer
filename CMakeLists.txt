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

option(SH_RENDERER_EMBED_GLSL
       "Build glsl/ into the library (MakeEmbeddedReader)" OFF)
option(SH_RENDERER_BUILD_TESTS "Build the test binaries (needs GoogleTest)" ON)
option(SH_RENDERER_BUILD_GLTF
       "Build the glTF target: loader, viewer, GLFW window and input" ON)
option(SH_RENDERER_Q3_COORDINATE
       "Compile the core with Q3_COORDINATE (Z-up map units)" OFF)

# --- Core library -------------------------------------------------------------
# Passes, scene, compositor, shader loader, render targets and logging. It
# links only OpenGL (through the bundled glad loader) and Eigen, so a host such
# as the ioquake3 module can embed it without glog, gflags, GLFW, tinygltf or
# tinyexr.
find_package(OpenGL REQUIRED)
find_package(Eigen3 REQUIRED)

set(CORE_SOURCES
    src/camera.cpp
    src/cascade.cpp
    src/compute_light_tile.cpp
    src/culling.cpp
    src/draw_depth.cpp
    src/draw_radiance.cpp
    src/draw_ssao.cpp
    src/draw_shadow_map.cpp
    src/draw_sky.cpp
    src/draw_tonemap.cpp
    src/file_reader.cpp
    src/glad.c
    src/log.cpp
    src/render_target.cpp
    src/scene.cpp
    src/shader.cpp
    src/ssbo.cpp)

set(CORE_HEADERS
    src/camera.h
    src/cascade.h
    src/colorspace.h
    src/compute_light_tile.h
    src/culling.h
    src/draw_depth.h
    src/draw_radiance.h
    src/draw_shadow_map.h
    src/draw_sky.h
    src/draw_ssao.h
    src/draw_tonemap.h
    src/file_reader.h
    src/glad.h
    src/khrplatform.h
    src/log.h
    src/q3_layer.h
    src/render_target.h
    src/scene.h
    src/shader.h
    src/ssbo.h)

add_library(sh_renderer_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
set_target_properties(sh_renderer_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(sh_renderer_core PUBLIC src)
target_link_libraries(sh_renderer_core PUBLIC OpenGL::GL Eigen3::Eigen)
if(SH_RENDERER_EMBED_GLSL)
  include(cmake/embed_glsl.cmake)
  sh_renderer_embed_glsl(sh_renderer_core)
  target_sources(sh_renderer_core PRIVATE src/embedded_glsl.h)
endif()
if(SH_RENDERER_Q3_COORDINATE)
  # Public: every translation unit that includes a core header (the core's
  # own, its tests and the engine glue) must see the same layout and
  # constants, so the definition propagates to consumers.
  target_compile_definitions(sh_renderer_core PUBLIC Q3_COORDINATE)
endif()

# --- glTF target ----------------------------------------------------------------
# The glTF loader (tinygltf, tinyexr, stb_image), the EXR lightmap reader, the
# GLFW window, input and interaction, and the viewer sh_renderer_main.
if(SH_RENDERER_BUILD_GLTF)
  find_package(gflags REQUIRED)
  find_package(glfw3 REQUIRED)
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
  target_include_directories(nlohmann_json
                             INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})

  # tinygltf
  add_library(tinygltf INTERFACE)
  target_include_directories(tinygltf INTERFACE ${TINYGLTF_INCLUDE_DIR})
  target_link_libraries(tinygltf INTERFACE stb_image nlohmann_json)

  # tinyexr
  add_library(tinyexr INTERFACE)
  target_include_directories(tinyexr INTERFACE ${TINYEXR_INCLUDE_DIR})

  set(GLTF_SOURCES
      src/gltf_scene_io.cpp
      src/implementations.cpp
      src/input.cpp
      src/interaction.cpp
      src/loader.cpp
      src/window.cpp)

  set(GLTF_HEADERS
      src/gltf_scene_io.h
      src/input.h
      src/interaction.h
      src/loader.h
      src/window.h)

  add_library(sh_renderer_gltf STATIC ${GLTF_SOURCES} ${GLTF_HEADERS})
  target_include_directories(sh_renderer_gltf PUBLIC src)
  target_link_libraries(sh_renderer_gltf PUBLIC
      sh_renderer_core
      tinygltf
      tinyexr
      stb_image
      nlohmann_json
      glfw
      gflags
      ZLIB::ZLIB
      Threads::Threads
  )

  # Main executable
  add_executable(sh_renderer_main
      src/main.cpp
  )

  target_link_libraries(sh_renderer_main PRIVATE
      sh_renderer_gltf
  )
endif()

# --- Tests ----------------------------------------------------------------------
if(SH_RENDERER_BUILD_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()

  # Core tests: no GL context, no GLFW.
  add_executable(sh_renderer_core_test
      src/camera_test.cpp
      src/cascade_test.cpp
      src/culling_test.cpp
      src/file_reader_test.cpp
      src/log_test.cpp
      src/scene_test.cpp
      src/shader_test.cpp
  )
  if(SH_RENDERER_EMBED_GLSL)
    target_sources(sh_renderer_core_test PRIVATE src/embedded_glsl_test.cpp)
  endif()
  target_link_libraries(sh_renderer_core_test PRIVATE
      sh_renderer_core
      GTest::gtest_main
  )

  # glTF-side tests: everything that needs GLFW, a GL context or the loader.
  if(SH_RENDERER_BUILD_GLTF)
    add_executable(sh_renderer_test
        src/input_test.cpp
        src/interaction_test.cpp
        src/loader_layers_test.cpp
        src/loader_test.cpp
        src/shader_gl_test.cpp
        src/window_test.cpp
    )
    target_link_libraries(sh_renderer_test PRIVATE
        sh_renderer_gltf
        GTest::gtest_main
    )
  endif()
endif()

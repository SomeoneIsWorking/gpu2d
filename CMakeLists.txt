cmake_minimum_required(VERSION 3.20)
project(gpu2d VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Gpu2dShaders)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPU2D_SDL3 IMPORTED_TARGET GLOBAL sdl3>=3.2)
pkg_check_modules(GPU2D_SDL3_TTF IMPORTED_TARGET GLOBAL sdl3-ttf>=3.2)
pkg_check_modules(GPU2D_HARFBUZZ IMPORTED_TARGET GLOBAL harfbuzz>=2.8.2)
pkg_check_modules(GPU2D_SDL3_IMAGE IMPORTED_TARGET GLOBAL sdl3-image>=3.2)
pkg_check_modules(GPU2D_PNG IMPORTED_TARGET GLOBAL libpng)
if(NOT GPU2D_SDL3_FOUND OR NOT GPU2D_SDL3_TTF_FOUND OR NOT GPU2D_HARFBUZZ_FOUND
   OR NOT GPU2D_SDL3_IMAGE_FOUND OR NOT GPU2D_PNG_FOUND)
  message(FATAL_ERROR
    "gpu2d needs SDL3, SDL3_ttf, HarfBuzz, SDL3_image and libpng development files. Install them "
    "with `sudo dnf install SDL3-devel SDL3_ttf-devel harfbuzz-devel SDL3_image-devel "
    "libpng-devel` (Fedora); on Ubuntu 24.04 `sudo apt install libharfbuzz-dev libpng-dev` and "
    "build SDL3, SDL3_ttf and SDL3_image from source")
endif()

# Images without a GPU: decoding, resizing and PNG encoding.
add_library(gpu2d_image STATIC
  src/image/decode.cpp
  src/image/resize.cpp
  src/image/png_encoder.cpp
)
add_library(gpu2d::image ALIAS gpu2d_image)
target_include_directories(gpu2d_image PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gpu2d_image
  PRIVATE PkgConfig::GPU2D_SDL3 PkgConfig::GPU2D_SDL3_IMAGE PkgConfig::GPU2D_PNG)
target_compile_features(gpu2d_image PUBLIC cxx_std_20)
target_compile_options(gpu2d_image PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(gpu2d_image PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The window, device, canvas, its pipelines, textures, offscreen targets and text.
add_library(gpu2d_render STATIC
  src/render/gpu_error.cpp
  src/render/gpu_window.cpp
  src/render/pipeline.cpp
  src/render/shape_pipeline.cpp
  src/render/mesh_pipeline.cpp
  src/render/blur_pipeline.cpp
  src/render/stream_buffer.cpp
  src/render/texture.cpp
  src/render/image_loader.cpp
  src/render/render_target.cpp
  src/render/canvas.cpp
  src/render/text.cpp
)
add_library(gpu2d::render ALIAS gpu2d_render)
# The window system's part of opening a window: X11's ARGB visual where windows are X11's.
if(UNIX AND NOT APPLE AND NOT ANDROID)
  find_package(X11 REQUIRED)
  target_sources(gpu2d_render PRIVATE src/render/window_platform_x11.cpp)
  target_link_libraries(gpu2d_render PRIVATE X11::X11)
else()
  target_sources(gpu2d_render PRIVATE src/render/window_platform_default.cpp)
endif()
gpu2d_compile_shaders(gpu2d_render
  src/shaders/shape.vert src/shaders/shape.frag
  src/shaders/mesh.vert src/shaders/mesh.frag
  src/shaders/blur.vert src/shaders/blur_horizontal.frag src/shaders/blur_vertical.frag)
target_include_directories(gpu2d_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gpu2d_render
  PUBLIC PkgConfig::GPU2D_SDL3 PkgConfig::GPU2D_SDL3_TTF PkgConfig::GPU2D_HARFBUZZ gpu2d::image)
target_compile_features(gpu2d_render PUBLIC cxx_std_20)
target_compile_options(gpu2d_render PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(gpu2d_render PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The UI toolkit: theme, layout, widgets, focus, the screen stack and motion. Title-neutral; the
# app supplies the theme's values, the face and the button glyphs.
add_library(gpu2d_ui STATIC
  src/ui/theme.cpp
  src/ui/motion.cpp
  src/ui/text_fit.cpp
  src/ui/typesetter.cpp
  src/ui/focus.cpp
  src/ui/widget.cpp
  src/ui/layout.cpp
  src/ui/widgets.cpp
  src/ui/cell_grid.cpp
  src/ui/tab_dock.cpp
  src/ui/screen.cpp
  src/ui/navigator.cpp
)
add_library(gpu2d::ui ALIAS gpu2d_ui)
target_include_directories(gpu2d_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gpu2d_ui PUBLIC gpu2d::render)
target_compile_features(gpu2d_ui PUBLIC cxx_std_20)
target_compile_options(gpu2d_ui PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(gpu2d_ui PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Tests are for developing gpu2d itself; a consumer adding it with add_subdirectory does not
# build them.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  option(GPU2D_BUILD_TESTS "Build gpu2d's tests" ON)
  if(GPU2D_BUILD_TESTS)
    enable_testing()
    # The WebP fixture is encoded with libwebp.
    pkg_check_modules(GPU2D_WEBP REQUIRED IMPORTED_TARGET libwebp)
    set(scratch "${CMAKE_CURRENT_BINARY_DIR}/test-scratch")
    file(MAKE_DIRECTORY "${scratch}")

    add_executable(gpu2d_image_test tests/image_test.cpp)
    target_link_libraries(gpu2d_image_test PRIVATE
      gpu2d::image PkgConfig::GPU2D_SDL3 PkgConfig::GPU2D_SDL3_IMAGE PkgConfig::GPU2D_WEBP)
    target_compile_definitions(gpu2d_image_test PRIVATE GPU2D_TEST_SCRATCH="${scratch}")
    target_compile_options(gpu2d_image_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME image COMMAND gpu2d_image_test)

    # A real GPU device behind a hidden (unmapped) window, drawing offscreen; labelled `gpu`.
    foreach(name canvas text image_loader)
      add_executable(gpu2d_${name}_test tests/${name}_test.cpp)
      target_link_libraries(gpu2d_${name}_test PRIVATE gpu2d::render)
      target_compile_options(gpu2d_${name}_test PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
    # The outlined label is drawn through the toolkit's face.
    target_link_libraries(gpu2d_text_test PRIVATE gpu2d::ui)
    target_compile_definitions(gpu2d_image_loader_test PRIVATE GPU2D_TEST_SCRATCH="${scratch}")
    add_test(NAME canvas COMMAND gpu2d_canvas_test)
    add_test(NAME text
      COMMAND gpu2d_text_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/fonts/CalSans-Regular.ttf")
    add_test(NAME image_loader COMMAND gpu2d_image_loader_test)
    set_tests_properties(canvas text image_loader PROPERTIES LABELS gpu)
    if(TARGET X11::X11)
      add_executable(gpu2d_window_x11_test tests/window_x11_test.cpp)
      target_link_libraries(gpu2d_window_x11_test PRIVATE gpu2d::render X11::X11)
      target_compile_options(gpu2d_window_x11_test PRIVATE -Wall -Wextra -Wpedantic)
      add_test(NAME window_x11 COMMAND gpu2d_window_x11_test)
      set_tests_properties(window_x11 PROPERTIES LABELS gpu)
    endif()

    # The toolkit through its public headers with a fake face: no GPU.
    foreach(name layout navigator widgets cell_grid tab_dock)
      add_executable(gpu2d_ui_${name}_test tests/ui/${name}_test.cpp)
      target_link_libraries(gpu2d_ui_${name}_test PRIVATE gpu2d::ui)
      target_compile_options(gpu2d_ui_${name}_test PRIVATE -Wall -Wextra -Wpedantic)
      add_test(NAME ui_${name} COMMAND gpu2d_ui_${name}_test)
    endforeach()
  endif()
endif()

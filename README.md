# gpu2d

A small C++20 2D drawing library on the SDL3 GPU API (Vulkan), and a GPU-free image target.

- `gpu2d::render`: an X11 window and Vulkan device, and a canvas that draws antialiased SDF
  shapes (rounded rectangles filled or stroked, circles, rings and arcs, lines), triangles,
  textured quads with tint, text (shaped by HarfBuzz, each character at its fractional pen, drawn
  from SDL3_ttf's GPU glyph atlas) and a backdrop blur, with transform and scissor stacks, blend
  modes and offscreen render targets that read back to RGBA.
  Draws are batched into few GPU passes.
  An image loader decodes on worker threads and lands textures a frame's budget at a time; see
  [docs/image-loader.md](docs/image-loader.md).
- `gpu2d::ui`: a widget toolkit on that canvas for menus and settings screens: theme tokens,
  layout, standard widgets (rows, switches, choices, sliders, tabs, lists, dialogs, hint bar,
  toast), spatial focus, a screen stack with eased motion; see [docs/ui-toolkit.md](docs/ui-toolkit.md).
- `gpu2d::image`: decode PNG, JPEG and WebP through SDL3_image, resize, and encode PNG to memory
  through libpng. No window or GPU.

It never logs: failures come back as `gpu2d::GpuError` or as error strings.

## Requirements

CMake 3.20+, a C++20 compiler (Clang is the verification toolchain), Ninja, `glslc`, and
pkg-config modules `sdl3` (3.2+), `sdl3-ttf`, `harfbuzz` (2.8.2+), `sdl3-image`, `libpng`, plus
Xlib. Tests also need `libwebp` and a Vulkan driver (lavapipe works).

Fedora: `sudo dnf install clang ninja-build cmake glslc SDL3-devel SDL3_ttf-devel harfbuzz-devel SDL3_image-devel libpng-devel libX11-devel libwebp-devel mesa-vulkan-drivers`

## Use

```cmake
set(GPU2D_ROOT "$ENV{HOME}/repo/gpu2d" CACHE PATH "gpu2d checkout")
add_subdirectory("${GPU2D_ROOT}" "${CMAKE_BINARY_DIR}/gpu2d" EXCLUDE_FROM_ALL)
target_link_libraries(app PRIVATE gpu2d::render)   # or gpu2d::ui, or gpu2d::image alone
```

`gpu2d_compile_shaders(target shader.vert shader.frag ...)` is available to consumers for their
own shaders.

## Build and test

See [docs/codemap.md](docs/codemap.md) for the owners and the test commands.

## License

MIT; see `LICENSE`. The test font `tests/fonts/CalSans-Regular.ttf` is under the SIL Open Font
License (`tests/fonts/CalSans-OFL.txt`).

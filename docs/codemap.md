# Codemap

gpu2d is a C++20 2D drawing library on SDL3 GPU (Vulkan; Metal on Apple systems): rounded rectangles, circles, rings,
lines, triangles, textured quads, text and a backdrop blur, batched into few GPU passes. A second
target decodes, resizes and PNG-encodes images without a GPU. It is title-neutral: callers supply
fonts, pixels and policy, and get failures back as exceptions (`GpuError`) or error strings; the
library itself never logs.

| Owner | Where | What it owns |
|---|---|---|
| Image pixels | `include/gpu2d/image/bitmap.hpp` | `Bitmap`: RGBA rows, top first, unpadded |
| Image decoding | `include/gpu2d/image/decode.hpp`, `src/image/decode.cpp` | PNG, JPEG, WebP (anything SDL3_image reads) from a file or memory into a `Bitmap` |
| Image resizing | `include/gpu2d/image/resize.hpp`, `src/image/resize.cpp` | Cubic `resized` (Mitchell down, Catmull-Rom up) and aspect-keeping `fittedWithin` |
| PNG encoding | `include/gpu2d/image/png_encoder.hpp`, `src/image/png_encoder.cpp` | RGBA to PNG bytes in memory through libpng |
| Window and device | `include/gpu2d/gpu_window.hpp`, `src/render/gpu_window.cpp` | SDL window plus the `SDL_GPUDevice` claiming it (driver `metal` on Apple systems, `vulkan` elsewhere, asked for the formats `builtShaderFormats` names); hidden mode (unmapped, no vsync); swapchain acquire; X11 window id |
| Window platform | `src/render/window_platform.hpp`, `window_platform_x11.cpp` (Linux and other X11 systems), `window_platform_default.cpp` (elsewhere), picked in `CMakeLists.txt` | SDL hints before video starts: X11 driver and 32-bit ARGB visual (translucent window), or an opaque window |
| Canvas | `include/gpu2d/canvas.hpp`, `src/render/canvas.cpp` | Drawing API, transform and clip stacks, blend modes, passes per target, batching, blur strips, `submit` |
| Pipelines | `include/gpu2d/pipeline.hpp`, `src/render/pipeline.cpp` | Shaders from embedded SPIR-V, or MSL (entry `main0`) when the device takes no SPIR-V, chosen from `SDL_GetGPUShaderFormats`; one pipeline per (target format, blend mode) |
| SDF shapes | `include/gpu2d/shape_pipeline.hpp`, `src/render/shape_pipeline.cpp`, `src/shaders/shape.*` | Rounded boxes (filled or stroked) and arcs, one instanced quad each, records in a storage buffer |
| Meshes | `include/gpu2d/mesh_pipeline.hpp`, `src/render/mesh_pipeline.cpp`, `src/shaders/mesh.*` | Indexed triangles with per-vertex colour sampling a texture (straight or premultiplied) |
| Backdrop blur | `include/gpu2d/blur_pipeline.hpp`, `src/render/blur_pipeline.cpp`, `src/shaders/blur*`, `src/shaders/gaussian.glsl` | Separable Gaussian: horizontal into a strip, vertical through a rounded-rect mask |
| Streaming buffers | `include/gpu2d/stream_buffer.hpp`, `src/render/stream_buffer.cpp` | Per-frame CPU-to-GPU buffers that grow and cycle |
| GPU object ownership | `include/gpu2d/gpu_handle.hpp`, `include/gpu2d/gpu_error.hpp`, `src/render/gpu_error.cpp` | RAII handles for SDL GPU objects; the exception carrying SDL's reason |
| Textures | `include/gpu2d/texture.hpp`, `src/render/texture.cpp` | Upload from RGBA (mipmapped when trilinear) and `TextureView` |
| Image loading | `include/gpu2d/image_loader.hpp`, `src/render/image_loader.cpp` ([image-loader.md](image-loader.md)) | `ImageLoader`: decodes on worker threads, lands textures on the render thread within a per-frame budget; handles that read pending until then, shared per key, cancelled when dropped, failures reported |
| Render targets | `include/gpu2d/render_target.hpp`, `include/gpu2d/surface.hpp`, `src/render/render_target.cpp` | Offscreen RGBA8 targets, sampled or read back into a `Bitmap` |
| Text | `include/gpu2d/text.hpp`, `src/render/text.cpp` | The one text layout: HarfBuzz shapes a label (kerning per `FaceOptions`) and each character stands at its exact fractional pen, letter spacing included; SDL3_ttf rasterises each character into its GPU atlas; an outline (`TextStyle::outline`, FreeType's stroker, centred on the glyph) drawn under a label; metrics, glyph ink bounds, measuring (the exact advance), cached characters retired after submit |
| UI toolkit | `include/gpu2d/ui/`, `src/ui/` ([ui-toolkit.md](ui-toolkit.md)) | `gpu2d::ui`: theme tokens, layout containers, widgets, spatial focus, the screen stack (`Navigator`), dialogs and choice lists, one motion clock, UTF-8 text fitting; app-supplied face, glyphs and sounds |
| Shader build | `cmake/Gpu2dShaders.cmake` | `gpu2d_compile_shaders(target ...)`: glslc to SPIR-V word lists included by the C++, and with `GPU2D_SHADER_MSL` (default on Apple) SDL_shadercross's MSL as raw string literals, the target getting `GPU2D_SHADER_MSL` defined; consumers may reuse it |
| shadercross provisioning | `cmake/ProvisionShadercross.cmake` | Builds the pinned, checksummed SDL_shadercross CLI and SPIRV-Cross (with its `spirv-cross` CLI) into a prefix: `cmake -DPREFIX=<dir> -P cmake/ProvisionShadercross.cmake` |
| Tests | `tests/` | `image_test` (PNG round trip, WebP/JPEG decode, refusals, resize), `canvas_test`, `text_test` and `image_loader_test` (on a device behind a hidden window; pixels drawn offscreen and read back; ctest label `gpu`), `window_x11_test` (the hidden window's X11 visual and map state, X11 only), `ui_layout`, `ui_navigator`, `ui_widgets` (the toolkit with fake text, no GPU) |
| CI | `.github/workflows/ci.yml` | Fedora container, Clang and Ninja, CTest on Mesa's lavapipe; macOS arm64: pinned shadercross, AppleClang build, Apple's `metal` compiles every MSL, CTest |

## Data flow

A frame: `Canvas::begin(surface, clear)` per target, draw calls (each appended to the current
pass, merged into the previous batch when the state matches), then `Canvas::submit(commands)`
uploads shape records, vertices and indices once and encodes every pass. The caller submits the
command buffer, then calls `Text::retire()` so labels not drawn that frame free their atlas cells.

## Invariants

- Colours in targets are premultiplied; `BlendMode::Over` is ONE, ONE_MINUS_SRC_ALPHA.
- Per-shape data lives in storage buffers, never uniform pushes (SDL GPU drops pushes past 4 KB).
- A blur's scene target must have been drawn in an earlier pass than the one the blur lands in.
- `gpu2d::image` needs no window or GPU and is safe on worker threads.
- An `ImageLoader`'s decodes run on its workers; its textures are made and released on the render
  thread in `upload`.

## Testing

```sh
CC=clang CXX=clang++ cmake -S . -B build/clang -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/clang
ctest --test-dir build/clang --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ctest --test-dir build/clang
```

The GPU tests open a hidden (unmapped) X11 window, so they need an X display; CI runs them under
`xvfb-run`.

The MSL path builds on Linux too, for checking the translation without a Mac (the device still
takes SPIR-V, so behaviour is unchanged):

```sh
cmake -DPREFIX=$HOME/.local/opt/sdl-shadercross-1ff05be -P cmake/ProvisionShadercross.cmake
CC=clang CXX=clang++ cmake -S . -B build/msl -G Ninja -DGPU2D_SHADER_MSL=ON \
  -DGPU2D_SHADERCROSS=$HOME/.local/opt/sdl-shadercross-1ff05be/bin/shadercross
cmake --build build/msl   # build/msl/shaders/*.msl
```

Only Apple's `metal` compiles MSL; the macOS CI job runs it over every generated file.

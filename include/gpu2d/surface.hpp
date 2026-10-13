// surface — a texture a canvas pass draws into: a render target or the window's swapchain image.
#pragma once

#include <SDL3/SDL_gpu.h>

namespace gpu2d {

struct Surface {
  SDL_GPUTexture *texture{};
  int width{};
  int height{};
  SDL_GPUTextureFormat format{SDL_GPU_TEXTUREFORMAT_INVALID};
};

} // namespace gpu2d

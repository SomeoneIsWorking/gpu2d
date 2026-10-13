#include "gpu2d/gpu_error.hpp"

#include <SDL3/SDL_error.h>

namespace gpu2d {

GpuError::GpuError(const std::string &what) : std::runtime_error{what + ": " + SDL_GetError()} {
}

} // namespace gpu2d

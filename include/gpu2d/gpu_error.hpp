// gpu_error — a failed SDL GPU, window or font call, with SDL's reason.
#pragma once

#include <stdexcept>
#include <string>

namespace gpu2d {

class GpuError : public std::runtime_error {
public:
  /// `what` names the failed step; SDL_GetError() is appended.
  explicit GpuError(const std::string &what);
};

} // namespace gpu2d

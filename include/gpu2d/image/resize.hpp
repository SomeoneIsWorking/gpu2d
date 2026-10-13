// resize — scaling a Bitmap with bilinear filtering.
#pragma once

#include "gpu2d/image/bitmap.hpp"

namespace gpu2d::image {

/// `source` scaled to `width` x `height`; empty when either is not positive or SDL fails
/// (SDL_GetError says why).
[[nodiscard]] Bitmap resized(const Bitmap &source, int width, int height);

/// `source` scaled down, aspect kept, so its longest side is at most `longest`; unchanged when it
/// already fits.
[[nodiscard]] Bitmap fittedWithin(Bitmap source, int longest);

} // namespace gpu2d::image

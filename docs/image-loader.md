# Image loader (`gpu2d::ImageLoader`)

Images decoded off the render thread and made textures on it, a frame's budget at a time, so a
burst of pictures (a shelf of covers, a set of console cards, icons rasterised at a new size)
never stalls a frame on decoding or on a run of uploads. `include/gpu2d/image_loader.hpp`,
`src/render/image_loader.cpp`, part of `gpu2d::render`.

## What it owns

| Concept | Where | Notes |
|---|---|---|
| Requests | `ImageLoader::load(ImageRequest)` | A key, a decode function run on a worker (`ImageDecode`: pixels, or nothing with a reason, or a throw) and the texture's `Filter` |
| Files | `ImageLoader::loadFile(path, filter, maxSide)` | SDL3_image's decode (`image::decodeFile`), scaled to fit `maxSide` (`image::fittedWithin`) when positive |
| Handles | `ImageLoader::Handle` (`ImageHandle`) | `state()` (`Empty`, `Pending`, `Ready`, `Failed`), `texture()` (null until it lands), `error()`; copies share the image |
| Workers | `ImageLoader(workers)` | Two by default; they take the queue oldest first |
| Landing | `upload(device, bytes)` | On the render thread once a frame: textures for decoded images, oldest first, until `bytes` of pixels went up (at least one image), and the failures since the last call |
| Still frames | `wait(handle)`, `waitAll()`, `idle()` | `wait` takes the image ahead of the queue and blocks until it is decoded; `waitAll` until nothing is queued or decoding |

## Data flow

`load` looks the key and filter up: a live image with both is shared, else a new entry is queued
and a worker woken. The worker runs the decode outside the lock, then under it either puts the
pixels on the decoded queue or records the failure (the handle reads `Failed` at once and the
failure waits for the next `upload`). `upload` takes decoded entries up to the budget, makes each
a `Texture` on the caller's device and frees its pixels; the handle reads `Ready` from then on.
The app draws a placeholder while a handle reads `Pending` and the texture once it is there; it
draws every frame anyway, so nothing has to be told to redraw.

## Invariants

- The loader's queues hold entries weakly; handles own them. An image whose last handle is
  dropped before a worker takes it is never decoded, and one dropped after is never uploaded.
- Requests with the same key and filter share one decode and one texture while a handle holds
  it; once none does, the next request decodes the file again (so a replaced file is read anew).
- Textures are made and released on the render thread: a worker drops its reference under the
  loader's lock, before the entry can have a texture.
- Every failure is reported twice: on the handle (`error()`) and once in an `upload`'s
  `failures`, which the app logs. gpu2d itself never logs.
- A `Texture` failing to be made throws `GpuError` out of `upload`.
- Decode functions run on the workers and must not touch the GPU device or the app's state.

## Checking it

`ctest --test-dir build/agent -R image_loader` decodes on a real device behind a hidden window:
a PNG lands on upload and draws, files larger than the limit are scaled down, one decode is shared,
a dropped handle cancels its decode, failures (a missing file, a throwing decoder) are reported
once, a frame keeps to its budget and `wait` returns once its image is decoded. In an app, read
the handles' states where it counts pending art (openSU reports `artworkPending` on `/state`).

---
description: OpenGL rendering, GPU resources, shaders, and debug overlay conventions
paths:
  - "src/resources/**"
  - "src/main_window/**"
  - "data/shaders/**"
---

# HiveWE Rendering

## Key files

- `src/resources/` — textures, meshes, shaders, particles, skinned meshes.
- `src/base/render_manager.ixx`, `src/main_window/glwidget.cpp` — render loop and viewport.
- `data/shaders/` — GLSL sources.

## Rules

- Load assets through `ResourceManager` — cache is intentional and thread-safe.
- `ResourceManager::load<GroundTexture>` / `load<Texture>` take bare paths: no extension and no `_diffuse` suffix. The loader adds `_diffuse` for HD and probes `.tga`/`.blp`/`.dds`; passing a suffixed path doubles it. Each probe goes through the whole `Hierarchy`, so a map file overrides a stock tile only with the stock extension (`.blp` in SD) or `.tga`; a DDS import has no effect in SD.
- `GLThreadPool` shared contexts + explicit sync — do not remove casually.
- `Map::update()` / `Map::render()` are hot paths.
- Debug overlay (`glwidget.cpp`): distinguish HiveWE terrain-space vs Warcraft world-space; user readouts prefer Warcraft coordinates.
- Treat debug overlays as user-facing tools — trim low-signal entries.

## Image decoding and fallbacks

- `Hierarchy::open_file` can return another format than the requested extension (a `filealiases.json` target gets a `.dds` retry, `.blp`/`.tga` targets only): image decoders choose by content (`blp::has_blp1_magic`), never by `path.extension()`.
- An undecodable image gives a placeholder and one log line in `Texture` and `GroundTexture`; `PathingTexture` returns an error. `ResourceManager::load` turns a loader throw into an error result and many callers use `.value()`, so check every caller before making any loader stricter.
- The linked SOIL2 library has no `SOIL_free_image_data` (declared in the header, link error): free SOIL buffers with `std::free`, BLP buffers with `delete[]`.

## Model Editor and preview

- The Model Editor and the main-window sidebar Model Preview share `ModelEditorGLWidget`: a playback change there also changes the preview.
- `SkeletalModelInstance::update(delta, advance_frame, wrapped)`: a caller that owns the clock must pass the loop-wrap flag itself (particle squirt emitters need it).
- `EditableMesh::geosets` skips `lod != 0` geosets, so its indices differ from `mdx->geosets`. Key per-geoset UI state on `MeshEntry::source_index`.
- The Model Editor viewport uses the default orbit camera, not the model's CAMS.
- HD rendering in offscreen hooks follows `HKCU\Software\HiveWE\HiveWE` value `hd`; restore it after the run. Hook script steps are separated by `;`.
- Model Editor Save MDX is a modal `QFileDialog`, not drivable offscreen: cover the save path with an in-process test.

## Bug-fix order

Resource loading → GL context/thread assumptions → render toggles → shaders last.

## Profiling

Use `ReleaseTracing` preset (Tracy) for startup, loading, or render performance work.

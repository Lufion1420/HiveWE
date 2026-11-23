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
- `GLThreadPool` shared contexts + explicit sync — do not remove casually.
- `Map::update()` / `Map::render()` are hot paths.
- Debug overlay (`glwidget.cpp`): distinguish HiveWE terrain-space vs Warcraft world-space; user readouts prefer Warcraft coordinates.
- Treat debug overlays as user-facing tools — trim low-signal entries.

## Model Editor and preview

- The Model Editor and the main-window sidebar Model Preview share `ModelEditorGLWidget`: a playback change there also changes the preview.
- `SkeletalModelInstance::update(delta, advance_frame, wrapped)`: a caller that owns the clock must pass the loop-wrap flag itself (particle squirt emitters need it).
- `EditableMesh::geosets` skips `lod != 0` geosets, so its indices differ from `mdx->geosets`. Key per-geoset UI state on `MeshEntry::source_index`.

## Bug-fix order

Resource loading → GL context/thread assumptions → render toggles → shaders last.

## Profiling

Use `ReleaseTracing` preset (Tracy) for startup, loading, or render performance work.

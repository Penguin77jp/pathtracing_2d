# Pathtracing2D Field Debugger MVP

Experimental 2D path tracing playground written in C++17 + GLFW/OpenGL/Dear ImGui.

## What is implemented

- 2D world-space luminance field renderer
  - Each pixel is a probe position in the configured world bounds.
  - Each sample jitters inside the pixel, samples one full-circle direction, then path traces from that point.
  - This replaces the previous 1D camera strip completely.
- Scene editor for segment and circle geometry
  - `Add wall`, `Add light`, and `Add glass circle` buttons.
  - Select a line segment, segment endpoint, circle center, or circle radius handle in the canvas.
  - Drag endpoints, whole segments, circle centers, or circle radius handles to edit geometry.
  - Delete, duplicate, and flip segment normals from the GUI.
  - Edit material, albedo/tint, emission, material kind, and glass IOR from the side panel.
  - Any scene edit resets accumulation and retraces the selected debug sample.
- 2D glass sphere/circle object
  - `MaterialKind::Dielectric` with configurable IOR.
  - Uses Schlick Fresnel to choose reflection/refraction.
  - Debug view shows the sampled specular direction and probability.
- Path debugger tied to the 2D result image
  - Click a field pixel to select the probe to debug.
  - Choose sample index to replay a specific sample deterministically.
  - The debugger visualizes the same `estimate_at()` path used by the field renderer.
  - Shows initial sampled direction, traced ray segments, hits, normals, BSDF/specular directions, PDFs, light samples, NEE shadow rays, and light hits.
- PT/NEE/BDPT-oriented interfaces
  - `IntegratorKind::PathTracing`
  - `IntegratorKind::PathTracingNEE`
  - `IntegratorKind::BidirectionalPT`
  - `estimate_at()` for field probes and debug replay
  - `PathVertex` reserved for BDPT
  - `DebugEventType::LightSample` and `DebugEventType::ShadowRay` are used by NEE debug overlays

- Scene JSON save/load
  - `Save scene` writes materials, segments, circles, renderer settings, field bounds, field resolution, max depth, seed, and sampling controls.
  - `Load scene` restores the scene and resets accumulation/debug replay.
  - The JSON is intentionally human-readable so scenes can also be edited by hand.
- PNG export
  - `Save PNG` writes the current tonemapped RGBA field without adding extra dependencies.
- Auto-stop sampling
  - `Auto stop samples = 0` means render indefinitely.
  - Positive values pause accumulation when the target sample count is reached.

## Notes

- The glass object is represented as a 2D circle, the 2D analogue of a sphere.
- NEE currently samples segment lights and treats intervening geometry, including glass circles, as visibility blockers. More physically correct caustics through glass should be handled later by BDPT/connection logic.
- BDPT is still a planned hook.

## Build

The first CMake configure downloads GLFW and Dear ImGui via `FetchContent`, so network access is required the first time.

```sh
./run_cmake.sh
```

Or manually:

```sh
cmake -S . -B build
cmake --build build -j
./build/pathtracing_2d
```

## Controls

- `Scene JSON path`: path for scene save/load, default `scene.json`
- `Save scene` / `Load scene`: serialize or restore scene and renderer settings
- `Add wall`: create a diffuse line segment near the current view center
- `Add light`: create an emissive line segment near the current view center
- `Add glass circle`: create a dielectric circle near the current view center
- `Delete selected`: remove the selected object
- `Flip normal`: flip selected segment normal
- Left click in the 2D field image: select a field pixel for debug replay
- Debug sample index: replay a specific deterministic sample
- Left click near segment endpoint in canvas: select endpoint
- Left click near segment body in canvas: select whole segment
- Left click near circle center: select/move circle
- Left click near circle radius handle: resize circle
- Drag selected handle/body: edit scene geometry
- Left click empty canvas: debug an exact world-space point instead of a field pixel
- Right drag canvas: pan
- Mouse wheel over canvas: zoom
- Space: retrace selected sample
- R: reset accumulation

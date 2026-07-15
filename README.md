# smallgine

A small real-time 3D engine in C++17 on GLFW + OpenGL ES 3.1. Data-driven scene
graph, lighting with shadows, a texture/mesh/material pipeline, procedural sky,
post-processing, instancing, audio, and an on-screen HUD.

## Features

**Rendering**
- Scene graph with per-node transform, material, and behavior (`spin`)
- Blinn-Phong lighting: directional + point lights, per-material specular
- Shadow mapping (directional, 3×3 PCF, front-face depth pass)
- Normal mapping (tangent-space, tangents auto-computed)
- Textured meshes (procedural quad/cube + Wavefront **OBJ** loader with `.mtl`)
- Per-node materials (color, texture, normal map, shininess, specular, alpha)
- Transparency: sorted back-to-front alpha blending
- Instanced rendering (many meshes, one draw call)
- Procedural cubemap **skybox**
- **Post-processing** (offscreen FBO → gamma / vignette / saturation)

**Assets / systems**
- File textures via stb_image (TGA/PNG/JPG/BMP…), procedural fallbacks
- OpenAL audio (procedural tones)
- Bitmap text HUD via stb_truetype
- JSON scene load/**save** + hot-reload (nlohmann_json)
- Runtime scene-graph mutation (find / add / remove nodes)
- Config file (`settings.json`) for window + camera
- Exe-relative asset resolution (runs from any directory)

## Controls

| Input | Action |
|-------|--------|
| `W A S D` | move camera |
| mouse | look |
| scroll | zoom (fov) |
| `Q` / `E` | up / down |
| `TAB` | release / capture cursor |
| `SPACE` | play a tone |
| `N` / `M` | spawn / despawn a node |
| `F5` / `F9` | save / reload scene |
| `ESC` | quit |

## Build

Dependencies: a C++17 compiler, CMake ≥ 3.10, Ninja, and dev packages for
GLFW3, OpenGL ES (`libGLESv2`), OpenAL, nlohmann_json, and glm. stb headers are
bundled in `third_party/`.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/smallgine
```

Assets and `settings.json` are staged next to the binary at build time, so the
executable runs from any working directory.

## Layout

```
core/       engine, scene graph, camera, material, light, config
platform/   window (GLFW), GLES context, exe paths
resources/  mesh, OBJ loader, texture, skybox, shadow, instancing, post-fx
tools/      shaders, draw helpers, text, GL error checks
audio/      OpenAL system
assets/     textures, models, font (generated/bundled)
```

## Smoke test

`tests/smoke.sh` builds the project, runs it briefly, and checks the startup log.
It launches a real window, so it needs a display (`$DISPLAY`).

```sh
tests/smoke.sh
```

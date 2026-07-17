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
- **Asset packaging**: bundle a folder into one `.sgpk` file (`smallgine_pack`),
  mounted transparently — loaders read the pack, then fall back to loose files

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
| `F3` | toggle collider debug view (AABB wireframes) |
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

## Asset packaging

Ship one file instead of a loose `assets/` tree. `smallgine_pack` bundles a
folder into a single `.sgpk` package; the engine mounts it at startup and every
asset loader (shaders, textures, fonts, glTF/OBJ meshes, terrain heightmaps,
Lua scripts, WAV audio, scene JSON) reads through a small virtual filesystem —
pulling bytes from the pack, or falling back to loose files when unpacked.

```sh
# build the tool, then pack the assets folder into one file
cmake --build build --target smallgine_pack
./build/smallgine_pack assets assets.sgpk          # keys: "assets/<path>"
cp assets.sgpk build/                               # next to the executable
```

Drop `assets.sgpk` beside the binary and it's picked up automatically (override
the name with `"pack": "..."` in `settings.json`, or `""` to disable). With the
pack present the loose `assets/` folder is no longer needed at runtime. Format:
`platform/vfs.hpp` (reader/mount) and `tools/packager.hpp` (writer) — a `"SGPK"`
header, an index of `(path, offset, size)`, then the concatenated file blob.

## Layout

```
core/       engine, scene graph, camera, material, light, config
platform/   window (GLFW), GLES context, exe paths, asset pack VFS
resources/  mesh, OBJ loader, texture, skybox, shadow, instancing, post-fx
tools/      shaders, draw helpers, text, noise, asset packager, GL checks
audio/      OpenAL system
assets/     textures, models, font (generated/bundled)
```

## Smoke test

`tests/smoke.sh` builds the project, runs it briefly, and checks the startup log.
It launches a real window, so it needs a display (`$DISPLAY`).

```sh
tests/smoke.sh
```

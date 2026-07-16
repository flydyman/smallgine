# smallgine demos

Standalone showcases built on the engine. Each demo is its own executable that
links the same engine sources but supplies its own `settings.json` + scene, so
the engine runs a clean stage (`"sandbox": false`) instead of the built-in
sandbox that `smallgine` (the main target) ships.

Demos are enabled by default. Configure with `-DSMALLGINE_DEMOS=ON/OFF`.

| Demo | Target | Folder |
|------|--------|--------|
| FPS  | `demo_fps` | `demos/fps` |
| RTS  | `demo_rts` | `demos/rts` |
| Racing | `demo_race` | `demos/race` |
| Space flight | `demo_space` | `demos/space` |
| Procedural noise | `demo_noise` | `demos/noise` |

Each demo builds into its own `build/demos/<name>/` dir with assets staged
alongside, e.g. `./build/demos/fps/demo_fps`.

## Packaging

Every demo is also staged with a self-contained `assets.sgpk` — a single-file
package holding the shared assets **plus that demo's own scene JSON** (built by
the `smallgine_pack` tool, see the root README). The engine mounts it at startup,
so a demo runs from just its binary + `settings.json` + `assets.sgpk`, with the
loose `assets/` folder kept only as a fallback. Set `"pack": ""` in a demo's
`settings.json` to force the loose files instead.

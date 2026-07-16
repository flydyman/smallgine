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

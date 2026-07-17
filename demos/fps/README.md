# FPS demo

A large walled arena with cover crates, pillars, and ten spinning red targets
(`target-large.glb`). Two switchable weapons ride the camera — a **blaster**
(`blaster-b.glb`, slower) and a **rifle** (`blaster-n.glb`, rapid) — each firing
glowing bolt tracers. Starts directly in the capsule player controller.

## Controls

| Input | Action |
|-------|--------|
| `W A S D` | Move (yaw-relative; the camera bobs while walking) |
| `Space` | Jump |
| Mouse | Look |
| Left mouse | Fire — muzzle flash + tracer + recoil; a hit turns the target green and scores |
| `1` / `2` | Select blaster / rifle |
| `Q` | Cycle weapon |
| `B` | Toggle free-fly / walk mode |
| `Tab` | Release / capture cursor |
| `Esc` | Quit |

## How it works

The demo ships no engine code of its own — it configures the engine via
`settings.json`:

- `"sandbox": false` — skip the built-in sandbox props (terrain, nav agent,
  ECS swarm, GPU fountain, scripted/network nodes, instanced ring).
- `"playerStart": true` — enter the player controller on launch.

The scene (`fps.json`) is the whole level. The player capsule collides with
solid, opaque scene boxes (walls, crates, pillars) horizontally.

The weapon is a single viewmodel node re-posed each frame from the camera basis,
offset to the lower-right so it never covers the crosshair; `1`/`2`/`Q` just swap
its mesh, scale, fire rate, and tracer colour (both models are preloaded so the
switch never hitches). The Kenney blaster models are authored -Z forward, so the
pose flips them 180 deg about up (a proper rotation) to aim the barrel downrange.
Firing is a **mode-side ray hitscan** — a ray from the eye down the look axis
tested against `target*` bounding spheres — *not* the engine's center-pixel pick,
which would just hit the viewmodel. A shot kicks the camera and gun back (recoil),
pops a muzzle flash, and launches a glowing bolt tracer from a small pool; a hit
recolors the target green, plays a blip, and increments the HUD score. Walking on
the ground adds a sinusoidal camera bob/sway; the shake is applied to the camera
*position* only, so it never disturbs the aim ray.

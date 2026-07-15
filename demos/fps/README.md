# FPS demo

A walled arena with cover crates, a pillar, and six spinning red targets.
Starts directly in the capsule player controller.

## Controls

| Input | Action |
|-------|--------|
| `W A S D` | Move (yaw-relative) |
| `Space` | Jump |
| Mouse | Look |
| Left mouse | Shoot — crosshair hitscan; hitting a target turns it green and scores |
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
solid, opaque scene boxes (walls, crates, pillar) horizontally. Shooting is a
crosshair pick against nodes named `target*`; a hit recolors the node, plays a
blip, and increments the HUD score.

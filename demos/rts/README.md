# RTS demo

Top-down real-time control of a 12-unit squad (blue) against 8 enemies (red)
on an open field. Pure gameplay: select, move, attack.

## Controls

| Input | Action |
|-------|--------|
| Left click | Select the unit under the cursor |
| Left drag | Box-select all friendly units in the rectangle |
| Right click (ground) | Move-order the selected units (grid formation) |
| Right click (enemy) | Attack-order: chase and destroy that enemy |
| `W A S D` | Pan the camera across the map |
| `Esc` | Quit |

## How it works

Config only, no demo engine code:

- `"sandbox": false` — clean stage, just the scene field + rocks.
- `"rtsMode": true` — engine spawns the two squads, switches to a fixed
  top-down camera, and routes the mouse to selection/command handling.

Selection ray-casts the cursor onto the ground plane. Move orders steer each
unit toward its formation slot; attack orders chase the target and deal damage
on a cooldown until its HP hits zero, at which point the enemy is removed.
Units softly separate so they don't stack. Selected units are highlighted by a
brightened tint.

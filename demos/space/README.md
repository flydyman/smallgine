# Space flight demo

Six-degrees-of-freedom ship flying through an asteroid field with Newtonian
momentum (thrust and coast — no auto-stop) and a chase camera that rolls with
the ship.

## Controls

| Input | Action |
|-------|--------|
| `Left Shift` | Main thrust (accelerate along ship forward) |
| `Left Ctrl` | Retro-brake (damp velocity) |
| `W` `S` | Pitch (nose down / up) |
| `A` `D` | Yaw (left / right) |
| `Q` `E` | Roll |
| `Esc` | Quit |

## How it works

Config only, no demo engine code:

- `"sandbox": false` — bare stage; engine spawns the ship + 70 asteroids.
- `"spaceMode": true` — 6DOF controller + chase camera.

Orientation is a **quaternion**, so rotations compose in ship-local axes with no
gimbal lock. Rotation input builds a small local delta quaternion each frame and
multiplies it in. Thrust adds to a velocity vector along the ship's forward axis;
with no input the ship coasts (momentum). The ship node renders via a new
`Node::RotMatrix` override (`glm::mat4_cast` of the quaternion) instead of Euler
angles. The chase camera trails behind and above in the ship's frame, using the
ship's up vector so a roll visibly rolls the view.

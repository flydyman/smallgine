# Racing demo

Arcade car on a wide oval gate circuit with launch ramps, a trailing chase
camera, and lap timing.

## Controls

| Input | Action |
|-------|--------|
| `W` | Throttle |
| `S` | Brake / reverse |
| `A` `D` | Steer (scales with speed) |
| `Esc` | Quit |

## How it works

Config only, no demo engine code:

- `"sandbox": false` — clean circuit (ground + infield).
- `"raceMode": true` — engine builds an 8-gate oval, spawns the car, and takes
  over update with the car controller + chase camera.

Car model: throttle/brake integrate a signed speed with rolling drag; steering
turns the heading proportionally to speed (no turning while parked) and inverts
in reverse. The car pushes out of solid barriers (gate posts) via an oriented-box
(OBB) test that rotates with the car, and scrubs speed on impact. Passing through
the numbered checkpoint gates in order completes a lap; the HUD shows speed, lap
count, current lap time, and best lap. The chase camera trails behind and above,
smoothed, looking just ahead of the car.

**Ramps + jumps (physics):** the wide circuit has launch ramps on the straights.
An analytic ramp surface raises the car as it drives up; climbing gives it upward
velocity, so leaving the top launches it into the air, where gravity arcs it back
down to land where it meets the surface again. **Flight is inertial:** the
horizontal velocity is frozen at launch, so throttle and steering do nothing in
the air — the car coasts along its launch trajectory and only regains control on
landing (its speed carries over). The nose pitches to follow the slope on the
ground and eases level in the air. Press `F3` to see the car's oriented collider
and the gate boxes.

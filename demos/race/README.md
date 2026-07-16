# Racing demo

Arcade car on an oval gate circuit with a trailing chase camera and lap timing.

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
in reverse. The car pushes out of solid barriers (gate posts) and scrubs speed
on impact. Passing through the numbered checkpoint gates in order completes a
lap; the HUD shows speed, lap count, current lap time, and best lap. The chase
camera trails behind and above, smoothed, looking just ahead of the car.

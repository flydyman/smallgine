# Noise demo (`demo_noise`)

A live showcase of the procedural generators in [`tools/noise.hpp`](../../tools/noise.hpp).
The engine builds a wide terrain grid whose heights come from one generator at a
time; cycle through them to compare their character on the same seed and domain.

## Landscape painting

The terrain is painted like a real landscape: **sand** along the shore, **grass**
and **forest** on the flats, bare **rock** on the cliffs, and **snow** on the
peaks. This is done without any special shader — the mesh writes UVs of
`(elevation, steepness)` (`makeHeightMesh(..., paletteUV = true)`) and samples a
`makeTerrainPalette()` ramp texture, so height and slope pick the color. Each
field is range-normalized per rebuild so every generator spans shore → peak.
Press `P` to toggle between the palette and a plain slope tint.

## Water

A **sea-level water plane** fills the low basins. It's a first-class engine
feature: `Material::water` flags a surface as water, and the lit shader (with a
new `uTime` uniform) animates ripples from a sum of directional sine waves,
reflects the sky cubemap with a Fresnel term, and blends translucently so the
submerged terrain shows through. Generated with `makeWaterPlane()` and placed at
a normalized sea elevation, it rings the shores with the palette's sand band.
Press `X` to toggle the water.

## Generators

| # | Generator | Notes |
|---|-----------|-------|
| 1 | White noise | Per-cell random, no interpolation — pure static |
| 2 | Value noise | Smoothstep-interpolated lattice values |
| 3 | Perlin (gradient) | Ken Perlin's improved gradient noise |
| 4 | Simplex | Gustavson 2D simplex |
| 5 | Worley (cellular F1) | Distance to nearest feature point |
| 6 | Worley cracks (F2-F1) | Voronoi edges / cracks |
| 7 | fBm | Fractal Brownian motion over Perlin (5 octaves) |
| 8 | Ridged multifractal | Sharp mountain ridge lines |
| 9 | Turbulence (billow) | Puffy, cloud-like folds |
| 10 | Domain warp | Perlin warped by Perlin — swirling distortion |

The fractal combiners (`fbm`, `ridged`, `turbulence`) are templated over any base
generator, and `sample01()` normalizes any of them to `[0,1]` for heightmaps or
textures.

## Controls

| Key | Action |
|-----|--------|
| `N` / `B` | Next / previous generator |
| `R` | Reseed |
| `P` | Toggle landscape painting vs. slope tint |
| `X` | Toggle the sea-level water |
| `[` / `]` | Zoom the noise domain (fewer / more features) |
| `WASD` + mouse | Free-fly camera |
| `Shift` | Movement boost |
| `Esc` | Quit |

Build and run:

```sh
cmake --build build --target demo_noise
./build/demos/noise/demo_noise
```

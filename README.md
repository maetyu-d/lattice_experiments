# Lattice Experiments

![](https://github.com/maetyu-d/lattice_experiments/blob/main/Screenshot%202026-03-14%20at%2013.41.30.png)

The Lattice Experiments are an audiovisual composition tool that use 3D lattice-type structures to explore several different types of music systems:

- `A1`: 3-D grid with snake transport and internal synth/drum engine
- `A2`: Cellular Grid with 3D cellular automata transport and internal melodic engine
- `A3`: Based on Tymoczko's A Geometry of Music
- `A4`: Bonus - based on Simon Holland's Harmony Space
- `B1..B4`: plugin-hosted versions of the same systems, routed through hosted AU/VST3 channel strips instead of the internal synths

![](https://github.com/maetyu-d/lattice_experiments/blob/main/Screenshot%202026-03-14%20at%2013.42.09.png) Above - A3: Based on Tymoczko's A Geometry of Music

## Build and run

From this folder:

```bash
cmake -S . -B build
cmake --build build --config Debug
open "/Users/md/Downloads/Patchable language/build/GlyphGridJUCE_artefacts/GlyphGrid.app"
```

App bundle:

`/Users/md/Downloads/Patchable language/build/GlyphGridJUCE_artefacts/GlyphGrid.app`

## Modes

### A modes

`A` modes use the built-in synthesis engine.

- `A1`: snakes traverse a 3D grid and trigger melodic, harmonic, rhythmic, and visual glyph stacks
- `A2`: a 3D cellular automaton evolves across the grid and triggers melodic material
- `A3`: harmonic-geometry mode
- `A4`: Harmony Space mode

### B modes

`B` modes mirror `A1..A4`, but send the music into hosted plugins.

- mixer strips host:
  - `MIDI FX`
  - `Instrument`
  - `FX1`
  - `FX2`
  - `FX3`
  - `FX4`
- plugins can be loaded from cached AU/VST3 scan results
- plugin state is saved with patches
- `param`, `route`, `channel`, and `cc` glyphs can drive hosted plugins

## Controls

Global:

- `Esc`: return to the title screen
- `Resume` appears on the title screen after entering a mode
- `V`: cycle layout views
- `M`: show/hide mixer
- `D`: cycle the 3 built-in demos for the current mode

Playback / editing:

- `Play`: start/stop transport
- `Layer -` / `Layer +`: move through the 8 grid layers
- `Save` / `Load`: save or restore a full patch

CA mode:

- `Rule`
- `Birth`
- `Stay`

These appear in CA mode and control the cellular automaton behavior directly.

## Plugin scanning

On macOS, the app scans AU component folders once and caches the results:

- `/Library/Audio/Plug-Ins/Components`
- `~/Library/Audio/Plug-Ins/Components`

Use `Rescan Plugins` on the title screen to rebuild the cache.

## Mixer

All modes now expose a mixer view.

- `A` modes: 3-strip internal mixer with volume, pan, and meters
- `B` modes: full plugin channel strips with slot UI, bypass, remove, reorder, parameter targeting, and plugin editor windows

The mixer is hidden by default.

## Patches

Patch save files include:

- mode / variant
- grid contents across all 8 layers
- tempo
- view/layout state
- cellular automata rule state
- mixer strip settings
- plugin paths
- plugin state blobs

## Release zip

Current packaged zip:

`/Users/md/Downloads/Patchable language/release/lattice experiments-macOS-d1b2e3c.zip`

## Notes

- The repository still contains older HTML experiments, but the primary app is the JUCE standalone build.
- `B` modes depend on installed AU/VST3 plugins.
- The `release/` folder is local packaging output and is not committed to git by default.

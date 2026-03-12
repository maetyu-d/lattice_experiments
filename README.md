# PulsePatch Prototype

Minimal working prototype of a patchable audiovisual language UI with:

- draggable patch nodes
- editable code-backed node logic
- bytebeat-inspired audio
- built-in audio nodes: `Osc`, `Noise`, `Env`, `Filter`, `Amp`, `Mixer`
- minimalist visual stage
- combined patch + inspector workflow

## Run

From this folder:

```bash
python3 -m http.server 4173
```

Then open:

`http://localhost:4173`

## JUCE GlyphGrid

The `glyphgrid.html` concept is also reimplemented as a JUCE standalone app.

Build:

```bash
cmake -S . -B build
cmake --build build --config Debug
```

App bundle:

`build/GlyphGridJUCE_artefacts/GlyphGrid.app`

## Notes

- Press `Play` to start the Web Audio output.
- Select `ByteLead` or `MinimalViz` to edit their code.
- Drag nodes around the canvas to inspect the patch structure.
- Click an output port, then an input or `mod` param port to create a cable.
- Shift-click a port to clear its incoming or outgoing cables.
- The default patch boots into a full demo voice: `Clock -> Env -> Osc -> Filter -> Amp -> Mixer -> Output`, with `ByteLead` layered in parallel.
- This is a vertical-slice prototype, not a full compiler/runtime yet.

## Alternate Concepts

- `glyphgrid.html`: grid/score-based concept where small glyph cells act as timed signal operators and sinks.
- `ribbon.html`: lane/ribbon-based concept where inline processors are arranged along flowing signal strands.

# Style Engine — final piece: in-app picker (2026-05-31)

The type × style × colorway engine is **functionally complete and committed**
(`f6d0929`). The ONLY thing left for "feature complete" is the in-app **UI
picker** so a user can choose a StyleKit + colorway without the script bridge.

Fresh session was requested because the tool I/O channel started fabricating
reads of `PedalDesignerComponent.cpp` (returned stale/wrong lines). Restart for a
clean channel, then do the picker first thing.

## What already works (committed in f6d0929, all A/B-verified)
- **Colorway** renders live on faceplate (PedalPainter) AND designer canvas.
  Set `colorwaySeed` (int64 ARGB) + `colorwayMode` (0=Semantic,1=Tint) on a
  PedalDesign.
- **StyleKit** `"neon"` is registered (`source/ui/NeonStyleKit.h`,
  `registerBuiltinStyleKits()` called from `PedalForgeEditor` ctor). Set
  `styleKit` per-pedal or `style` per-control.
- Both settable today via `write_pedal_design` / the script bridge
  (`setStyleKit`, `setColorway`, `setStyle`).

## The remaining task: Properties-panel picker
Add to the Pedal designer's **PropertiesPanel** (class
`PedalDesignerComponent::PropertiesPanel`, declared ~line 1237 of
`source/ui/PedalDesignerComponent.cpp`). Two controls when NO control is
selected (i.e. the pedal-level properties view — see `showForIndices()`):

1. **StyleKit dropdown** — a `juce::ComboBox`. Populate from the registry:
   "Default" (id "") + one item per `pf::StyleKitRegistry::kits()` using each
   kit's `getId()`. On change → set `canvas->styleKit` (NEW member, mirror the
   existing `colorwaySeed`/`colorwayMode` members already on ChassisCanvas at
   ~line 133-139) and `canvas->repaint()`. NOTE: ChassisCanvas currently has
   colorwaySeed/colorwayMode but NOT a styleKit member — add it + load in
   `loadDesign` (~line 2610) + save in `savePedalDesign` (~line 270), AND the
   per-control draw at ChassisCanvas paint (~line 460) currently passes
   `"default"` to StyleKitRegistry::draw — change to the canvas styleKit.

2. **Colorway swatch + mode toggle** — model on the existing chassis
   `colourSwatchBtn` (toolbar, ctor ~line 2423; `showColourPicker()` ~line
   2570 using `juce::ColourSelector` + `CallOutBox`). On pick → set
   `canvas->colorwaySeed = col.getARGB()` (as int64), default
   `colorwayMode=1` (Tint). Add a small "Tint/Semantic" toggle or just default
   to Tint for v1. A "clear" affordance should set seed=0 (no colorway).

### Wiring checklist
- ChassisCanvas: add `juce::String styleKit { "default" };` member.
- `loadDesign`: `canvas->styleKit = design.styleKit;` (already copies
  colorwaySeed/colorwayMode).
- `savePedalDesign`: `design.styleKit = styleKit;` (already saves the others).
- ChassisCanvas paint loop: pass `styleKit` not `"default"` to
  `pf::StyleKitRegistry::draw` for placed hardware.
- PropertiesPanel: declare the ComboBox + swatch members, init in its ctor,
  lay out in its `resized()`, and refresh their displayed values in
  `showForIndices()` (pedal-level view). The panel reaches the canvas via the
  `canvas` pointer set by `properties->setCanvas(...)`.

### Verify
Build `PedalForge_All`. Open a pedal in the designer; pick "Neon" from the
dropdown → knobs become flat glowing-arc live. Pick a blue colorway → controls
tint blue. Confirm it persists through save/reload (the underlying fields
already round-trip).

## Gotchas (learned this session)
- Controls are ~12px in the designer; use a BIG knob (110px) for any color/style
  A/B — small ones are indistinguishable. The **board thumbnail** (PedalPainter)
  is the most reliable real-pixel proof surface.
- Each render SURFACE must pass the real colorway/styleKit; several sites
  intentionally pass `"default"` + `pf::Colorway{}` (inventory thumbs, palette,
  drag preview) because they're not tied to a saved design — leave those.
- Verify every Edit landed (re-grep/rebuild); a few silently no-matched this
  session and one (`loadDesign` colorway copy) was caught only by a grep-count.

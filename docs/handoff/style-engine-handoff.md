# Style Engine — Session Handoff (2026-05-30)

Pick-up doc for continuing the **type × style × colorway** control engine.
Full design spec: [`docs/control-catalog.md`](../control-catalog.md). This file is
the *current state + next actions* only.

Base commit when work started: `7f07bff`. **All changes below are UNCOMMITTED in the
working tree** (the user commits manually — do not commit unless asked).

---

## 0. FIRST THING: fix / verify the MCP server, then visually verify

The whole reason for the fresh session: the **`computer-use` MCP disconnected
mid-session** and could not be reloaded (every `mcp__computer-use__*` call returned
"No such tool available"; ToolSearch found nothing). That blocked the documented way
to drive the in-app AI agent.

**Once `computer-use` is connected, do the visual verification that's still outstanding:**
1. Launch the app:
   `pkill -f "PedalForge.app/Contents/MacOS/PedalForge"; open build/PedalForge_artefacts/Release/Standalone/PedalForge.app`
2. Drive the **in-app AI agent** per [`CLAUDE.md` §12](../../CLAUDE.md) — Cmd-K focuses the
   bottom-bar input; type a request; Return sends; read the streamed transcript via screenshot.
3. Ask it to: *create a pedal "StyleTest" with two knobs, a fader, a toggle, a footswitch,
   an LED, a rotary selector, a VU meter, an XY pad and a joystick on the faceplate, then open the Pedal tab.*
4. Screenshot the designer → confirm **every existing control renders identically** to before
   the refactor (Phase 0 was meant to be visually lossless) and the **new xypad/joystick** render.
5. Have the agent `read_pedal_as_script` then re-`run_script` it → confirm the new
   `style` / `colorwaySeed` / `colorwayMode` / `guard` / `shiftBinding` fields round-trip.

> Do NOT substitute `osascript`/System Events pixel-driving for the in-app agent — the
> user explicitly rejected that. Use the agent via `computer-use`, or pause and ask.

Fallback if `computer-use` still won't load: ask the user to confirm it's enabled in the
Claude Code MCP/connectors settings, or to restart the host session.

---

## 1. Build / verify commands

```bash
# Build everything (bundles included — NOT just `--target PedalForge`, which is the static lib)
cmake --build build --target PedalForge_All --config Release
```
Every code step this session was verified green on `PedalForge_All`. The
`isDisplay` "set but not used" warning at `PedalDesign.h:582` is **pre-existing**, unrelated.

The 3 new files are **header-only** (`source/ui/ControlState.h`, `StyleColor.h`, `StyleKit.h`)
so **no CMakeLists.txt change is needed** — the target source list is explicit, not globbed,
but headers don't need listing.

---

## 2. What is DONE (uncommitted, build-verified)

### Phase 0 — type×style×colorway substrate (COMPLETE)
- `source/ui/ControlState.h` — unified carrier: `value, x,y,z, array, buffer, light{rgba+bright},
  text, status(enum off/on/blink/ack), zones`. Plus `buildControlState(id,value,data,texts,values)`.
- `source/ui/StyleColor.h` — `pf::Colorway`: emphasis+role → colour resolver (Apple-tinted-icon
  model), `Mode::Tint`/`Semantic`, `tintFromSeed`, HSB ramp lerp, contrast auto-correct.
- `source/ui/StyleKit.h` — `pf::StyleKit` interface + `DefaultStyleKit` (delegates to
  `HardwareDrawing` = pixel-identical) + `StyleKitRegistry` (per-type fallback to default).
- `PedalDesign` serialization: per-control `style`; per-pedal `styleKit` / `colorwaySeed` (int64 ARGB)
  / `colorwayMode` (0=Semantic,1=Tint). Omit-when-default in toJSON/fromJSON.
- **All 5 render call sites** route through `StyleKitRegistry::draw`: PedalPainter.cpp,
  CanvasOverlay.h, PedalDesignerComponent.cpp (×3: placedHardware, drag preview, palette thumb),
  InventoryOverlay.cpp (×2: grid, preview). No `drawForType` calls remain outside HardwareDrawing.h.

### Phase 1 — carriers wired (COMPLETE)
- `buildControlState` assembles value + array + buffer (from instance `controlData`) + text
  (from `controlTexts`) + x/y (from `controlValues`) in one place.
- Cross-cutting `Control` flags `guard` (0=none,1=cover,2=hold,3=keylock) + `shiftBinding` (String),
  serialized omit-when-default. **Data-only** — no interaction/render yet (that's Phase 3).
- `light` / `status` / `zones` carriers EXIST but are not yet populated — deferred until a
  producer node emits them (Phase 2/3).

### Phase 2a — XY pad + joystick RENDER half (COMPLETE)
- `drawXYPad` + `drawJoystick` in HardwareDrawing.h (read x,y; y measured bottom-up; joystick
  self-centres at 0.5,0.5).
- `"xypad"`/`"joystick"` cases in `drawForType` (single-float maps to both axes) AND
  `DefaultStyleKit::draw` (reads state.x/state.y).
- `buildControlState` fills x/y from `controlValues["<id>_x"/"<id>_y"]`, centre-default 0.5.
- Added to InventoryOverlay `parts[]` palette + PedalDesignerComponent sizeForType/widthForType
  (**these tables are in MILLIMETRES, real enclosure dims** — xypad=25mm, joystick=22mm).

---

## 3. What is NEXT (in priority order)

### Phase 2b — XY pad INTERACTION (task #9; the immediate next coding step)
The pad renders but isn't interactive or auto-spawned. Needed:
1. `XYPadNode` (`source/dsp/ControlNodeLibrary.h:333`): flip `isControlSurface()` → true,
   `getControlType()` → `"xypad"` (currently false/`{}`).
2. `ControlBinding::nodeTypeForFaceType` (`source/dsp/ControlSurfaceSync.h:14`): add
   `xypad`/`joystick` → `ctrl_xy`.
3. **The hard part:** `ControlSurfaceSync` is SINGLE-param — it binds only `params[0]`
   (see the `Snap` struct + `primaryParam`). A 2-axis control needs BOTH x and y params bound,
   two mappings, and the poller writing `controlValues["<id>_x"]` / `["<id>_y"]`.
   Engine poller that writes the maps: `AudioGraphEngine.cpp:~1102` (controlValues),
   `:~1164` (controlTexts); `controlData` written in PedalboardGrid.cpp:264 / PlayTabComponent.cpp:470.
4. 2D drag interaction in PedalComponent / PedalDesignerComponent updating both axes.
5. Decide joystick spring-return-to-centre behaviour (faceplate "feel", like WheelNode's springReturn).

### Rest of Phase 2 — more new controls (per catalog §4, by effort)
- Addressable **light layer** + annunciator (populates `light`/`status` carriers).
- **multislider → graphic EQ → step grid** (populates `array` carrier).
- **encoder / stepper / radio-segmented** (cheap scalar/discrete).
- **signal displays**: spectrum, goniometer, trend/sparkline (populate `buffer`).
- **scribble strip** (Companion foot-controllers).

### Phase 3 — cross-cutting behaviour
- `guard` (flip-cover/hold/keylock arm gesture) — render + interaction.
- `shiftBinding` page/mode layer ("one control, many params").

### Phase 4 — colorway UI/generator
- seed→ramp generator + contrast auto-correct, colorway picker, curated presets for `default` kit.
- Validate the third axis on `default` before any new kit.

### Phase 5 — first non-default StyleKit (LCARS or race) + on-theme colorways.

---

## 4. Landmines / gotchas learned this session
- **`source/dsp/PedalDesign.h` edits:** use UNIQUE multi-line Edit anchors. The bare line
  `std::vector<Control> controls;` is non-unique (also in CanvasPage). A python-script edit
  approach failed on a wrong fromJSON anchor (`d.chassisImage` vs the real `design.chassisImage`).
- **Designer size tables are millimetres**, not pixels — don't assume px values.
- **Verify edits landed:** several Edit calls silently no-matched this session because the
  assumed surrounding text was wrong; always re-grep/build after a batch of edits.
- **Two control-value map conventions:** scalars in `controlValues[id]`; 2D axes will use
  `controlValues[id + "_x"/"_y"]`; arrays/waveforms in `controlData[id]`; text in `controlTexts[id]`.
- CanvasPage controls have their OWN serialization block in PedalDesign.h that does NOT yet
  serialize the new style/guard/shiftBinding fields — only the main `controls` block does.
  If canvas-page controls need styling, mirror the edits there too (currently a known gap).

---

## 5. Memory pointers (auto-loaded each session)
- `project_control_type_style_engine.md` — full running state of this work.
- `project_asset_bundling_policy.md`, `project_knob_generation_pipeline.md`,
  `project_neutral_chassis_runtime_tint.md`, `project_musician_first_audience.md` — related decisions.

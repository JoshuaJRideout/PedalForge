# Handoff: Display Subsystem (Easy + Advanced displays)

> Read this + `CLAUDE.md` + your auto-memory to pick up the display work cold,
> without the originating conversation. Everything below is committed; nothing
> is lost. Build target is **`PedalForge_All`** (not `PedalForge` -- that only
> builds the static lib and leaves bundles stale).

## 1. Goal / locked design

Let users recreate real pedals' displays + menus. Two display types, sharing one
model:

- **Easy Display** -- a configurable **text-grid menu**, no code. User sets the
  grid (lines x cols), font, colours, then a navigable text menu.
- **Advanced Display** -- a **Canvas-2D drawing API** for custom GUI (chosen over
  Arduino/GFX). A `draw()` runs per frame with the display size + input values +
  nav state in scope; `ctx.fillRect/beginPath/arc/fillText/createLinearGradient/
  drawImage` backed by JUCE `Graphics`.

**The linchpin architecture:** one declarative **`ScreenDesign`** model (JSON,
stored on the display node, round-trips via `toJSON`/`fromJSON`), with THREE
producers -- the visual editor, the script API, and the AI agent -- and ONE
renderer (a `DisplayMode`, so the same screen draws on the faceplate AND on
secondary hardware). Get the model right and all three are doors into one room.

**Explicit ports (key decision):** menu items each EXPLICITLY declare their port,
not auto-derived:
- `readout` -> a **Control input** (reads another node's value to display)
- `value` / `toggle` / `trigger` / `list` -> a **Control output** you wire to params
- `submenu` -> no port (navigation)
Defaults per type, but every item's port is overridable; ports keyed by item id
so wiring survives reordering. `list` can be a single index/value output OR a
router with N live outputs (configured per item).

**Menus/pages:** a navigation state machine (cursor, current page, edit-mode)
driven by the existing encoder / select / back control nodes wired to the
display's nav inputs.

## 2. Status

DONE + bridge-verified (commits):
- **Slice 1 `4ac3667`** -- `EasyDisplayNode` (`disp_easy`) in
  `source/dsp/PeripheralNodeLibrary.h`. Carries the `ScreenDesign` JSON (`screen`
  member), persists it via `toJSON`/`fromJSON`, `rebuildPorts()` builds explicit
  ports from items, `process()` emits value-item outputs / captures readout
  inputs. `isDisplayNode()==true`, `getDisplayType()=="easy_display"`. Registered
  in `DSPGraph.h` `createNodeByType` + `NodeCatalog.h` (Nodes/Displays/Screens).
- **Slice 3 `1f3d9c9`** -- `setScreen(node, <ScreenDesign JSON>)` command added to
  `compileGraphBuilder` in `source/ui/ScriptingTabComponent.h` (JSON taken
  verbatim as the rest of the line; reuses the FX builder's tempGraph->swap
  commit). Documented in `getApiReference()` (FX section). Reachable by the AI via
  `run_script mode=fx`. Verified: agent ran `addNode("disp_easy")` + `setScreen`
  with 2 value items + 1 readout -> console "2 out / 1 in ports", config
  round-trips.
- **Slice 2 (this pass)** -- faceplate menu renderer + navigation. Done across
  the five spots below; bridge-verified visually (agent screenshotted the board
  tile and read back `> Gain 4.0 / Mode Plate / Mix 50% / In 0.00` with the
  cursor on line 1, list/value/readout/% formatting all correct).
  - `EasyDisplayNode` (`PeripheralNodeLibrary.h`): nav input ports
    (`nav_encoder` Control + `nav_select`/`nav_back` Gate) appended after the
    readout inputs at `navBase`; atomic `cursor`/`editMode`; item values moved to
    `std::vector<std::atomic<float>>` (out + readout) so audio writes / UI reads
    are race-free. `handleNav()` state machine in `process()`: encoder delta moves
    the cursor (nav) or edits the selected value/list (edit-mode); select
    enters/exits edit, flips a toggle, or one-shot-pulses a trigger; back leaves
    edit. `renderMenuText()` formats the active items into grid lines (cursor
    marker + live values, scroll window). Surfaced via a new base virtual
    `DSPNode::getDisplayText()`.
  - Pollers: `PlayTabComponent.cpp` + `PedalboardGrid.cpp` -- in the display-node
    branch, `getDisplayType()=="easy_display"` -> `controlTexts[id] =
    getDisplayText()`. **PedalboardGrid had no display polling at all** before;
    added the whole `isDisplayNode()` branch there (so meters/scopes now animate
    on the board tile too, not just the Play tab).
  - `PedalPainter.cpp`: new `easy_display` control case -> `drawTextScreen` with
    the rendered menu lines.
  - **Open question RESOLVED (auto-place, not editor-only):**
    `syncControlSurfaceNodes` (`ControlSurfaceSync.h`) now also auto-creates a
    faceplate `Control` (type `easy_display`, 130x90) + `Mapping`
    (`<id>_display`) for `disp_easy` nodes -- gated to that display type so
    existing user-placed LED/VU/scope gadgets are untouched. Also called from
    `compileGraphBuilder` after the graph commit (against the scripting tab's own
    `activePedal`, NOT the editor callback -- the headless/bridge path targets a
    pedal that is not the editor's focused one), so scripted/AI displays get a
    widget immediately.
  - GOTCHA found while verifying: the AI bridge has no `focus_pedal` tool, and
    `switch_tab tab=Pedal` does NOT re-point the Designer canvas at a scripted
    pedal -- the Designer shows the *editor's* `activePedal`. To see a scripted
    pedal render, `add_pedal_to_board` it and screenshot the **Board** tab (or
    `read_pedal_design` to confirm the control/mapping data).

So today the model + node + explicit ports + scriptable/AI authoring + faceplate
render + menu nav all work. What does NOT exist yet: the Advanced/Canvas-2D node
(slice 4) and the visual editor (slice 5). Nav is implemented but only
render-verified; driving the encoder/select inputs end-to-end (wire control nodes
-> nav inputs, rotate, confirm cursor moves) is still worth an interactive pass.

## 3. ScreenDesign JSON schema (v1)

```json
{
  "kind": "easy",
  "grid": { "lines": 4, "cols": 16 },
  "font": 12, "fg": "FFFFFFFF", "bg": "FF101010",
  "items": [
    { "id":"mix",   "type":"value",   "label":"Mix",  "port":"out",
      "min":0, "max":1, "step":0.01, "value":0.5, "fmt":"%.0f%%" },
    { "id":"lvl",   "type":"readout", "label":"In",   "port":"in", "fmt":"%.1f" },
    { "id":"mode",  "type":"list",    "label":"Mode", "port":"out",
      "options":["Hall","Plate"], "value":0 },
    { "id":"tap",   "type":"trigger", "label":"Tap",  "port":"out", "value":0 },
    { "id":"more",  "type":"submenu", "label":"More", "port":"none", "target":"page2" }
  ]
}
```
(Pages/submenus: the model should grow a `pages` tree for menu levels; slice 1
uses a flat `items` list. `target` on a submenu names the page to drill into.)

## 4. Remaining slices + exact integration points

### Slice 2 -- faceplate menu renderer + navigation (do first; it makes it VISIBLE)
Touches ~5 spots:
1. **Node** (`EasyDisplayNode`): add a menu-render method (e.g.
   `juce::String renderMenuText()` formatting the active page's items + cursor +
   live values into grid lines). Add nav state (cursor, page, editMode -- atomics,
   since the audio thread writes and the UI reads) + nav **input ports**
   (encoder / select / back) + the interaction state machine in `process()`
   (encoder moves cursor or, in edit-mode, changes the selected value item's
   output; select drills in / toggles edit; back pops a level).
2. **Display poller** -- the per-node loops that fill
   `instance.controlTexts / controlData / controlValues` from live display nodes.
   Lives in BOTH `source/ui/PedalboardGrid.cpp` (~L200-280) and
   `source/ui/PlayTabComponent.cpp` (~L400-500), and partly `PedalComponent.cpp`.
   Add `easy_display` handling -> `controlTexts[id] = node->renderMenuText()`.
3. **`source/ui/PedalPainter.cpp`** (~L134, the `text_screen` case): add an
   `easy_display` control type -> draw via `drawTextScreen` honouring the
   ScreenDesign grid/font/colours.
4. **Faceplate-widget creation** -- OPEN QUESTION: control-surface nodes
   auto-create a `PedalDesign::Control` via `source/dsp/ControlSurfaceSync.h`
   (`isControlSurface()` nodes, bound to `params[0]`). DISPLAY nodes do not go
   through that path -- they're placed in the editor. Decide: either extend the
   sync to auto-place display nodes, or rely on the visual editor (slice 5) to
   place the display widget. Until one exists, a `disp_easy` node has no faceplate
   widget to render into.
5. **Verify** via the `screenshot` agent tool (renders the editor offscreen) --
   read the PNG to confirm the menu draws.

### Slice 4 -- Canvas-2D draw context (Advanced display)
- New `disp_advanced` node carrying a draw script + declared input ports.
- A Canvas-2D -> JUCE `Graphics` interpreter: `fillRect/strokeRect/beginPath/
  moveTo/lineTo/arc/fill/stroke/fillStyle/strokeStyle/font/fillText/
  createLinearGradient/addColorStop/drawImage/save/restore/translate/...`.
- `draw()` called per frame with display size + node input values + nav state.
- OPEN: which engine runs the script -- extend `ExpressionVM` (source/dsp/
  ExpressionVM.h) with a draw API + `draw()` entry, or a small dedicated JS-ish
  interpreter. ExpressionVM is the on-brand choice if it can host stateful draw
  calls.

### Slice 5 -- visual editor (adapt the Pedal Designer)
- Adapt `source/ui/PedalDesignerComponent.cpp` (the faceplate canvas) to also edit
  a `ScreenDesign`: place display elements, add/navigate menu levels (pages), and
  per item set its type + **port** (in/out/none) + binding via an inspector.
- This is where a display gets placed + configured on the faceplate, so it
  unblocks slice 2's "open question" above. Biggest UI piece; do with the user
  (build-and-feel), verifying renders via screenshots.

## 5. Codebase map (what to read)
- `source/dsp/DSPNode.h` -- base node: `inputPorts/outputPorts` vectors,
  `addInput/addOutput`, `clearInputs/clearOutputs`, virtual `toJSON/fromJSON`,
  `isControlSurface/getControlType` (controls), `isDisplayNode/getDisplayType/
  getDisplayValue/getPixelData` (displays), float `params`.
- `source/dsp/DSPGraph.h` -- `createNodeByType` factory (register new node types
  here) + graph (de)serialization.
- `source/dsp/NodeCatalog.h` -- FX inventory metadata (id/name/category/desc).
- `source/dsp/ControlNodeLibrary.h` -- input controls (knob/fader/button/toggle/
  selector/encoder/pan/wheel/trim), each with physical-widget settings.
- `source/dsp/PeripheralNodeLibrary.h` -- display/IO nodes incl. `EasyDisplayNode`.
- `source/dsp/ControlSurfaceSync.h` -- graph->face control auto-creation.
- `source/ui/PedalPainter.cpp` -- draws `PedalDesign::Control`s from the
  controlValues/controlData/controlTexts maps.
- `source/ui/HardwareDrawing.h` -- the `drawKnob/drawTextScreen/...` primitives.
- `source/ui/PedalboardGrid.cpp` / `PlayTabComponent.cpp` -- the display poller.
- `source/peripherals/displays/DisplayMode.h` -- `renderInto(g,bounds,ctx)` mode
  abstraction (faceplate + Turing/HDMI hardware share it). This is the eventual
  unified render target for both display types.

## 6. Build + test loop
- Build: `cmake --build build --config Release --target PedalForge_All`
- UTF-8 guard runs in-build (`cmake/check_utf8_literals.py`): a bare non-ASCII
  string literal fails the build -- wrap it in `juce::CharPointer_UTF8(...)` (use
  `\xNN` escapes) or add a trailing `// utf8-ok`. `juce::String(const char*)`
  decodes ASCII/Latin-1, hence the rule.
- **Insider AI bridge** (autonomous testing, no clicking needed): write a prompt
  to `/tmp/pedalforge_ai_cmd.txt`; the running app's AiAssistantPanel timer
  consumes it, runs the real in-app agent (the user's Claude Code subscription),
  and writes `/tmp/pedalforge_ai_response.txt` (ends with `[turn complete]`).
  Tool-call audit log: `~/Library/PedalForge/logs/ai_audit.log`. The agent has a
  `screenshot` tool (renders the editor offscreen via createComponentSnapshot) ->
  use it to visually verify the display draws, and Read the PNG.
  - App Nap is disabled (`source/util/AppNap.mm`) so the bridge timer is reliable
    even backgrounded. Mic permission is stable (CMake auto-signs the Standalone
    with the dev Apple cert) so launches don't re-prompt. First launch of a build
    shows the onboarding modal once.
  - Test pattern: `killall PedalForge; open <Release Standalone>.app`, write the
    cmd file, poll the response file ~60-150s.

## 7. Gotchas
- Build `PedalForge_All`, never just `PedalForge`.
- Wrap non-ASCII literals in `CharPointer_UTF8` (guard enforces).
- `run_script mode=fx` rebuilds the WHOLE FX graph; `setScreen` operates inside it.
- The agent sometimes fumbles tool arg names (e.g. `pedal_id` vs `pedal_uuid`) and
  self-corrects -- not a bug in the feature under test.
- Nav state shared audio<->UI thread must be atomic.

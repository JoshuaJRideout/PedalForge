# Pedal Design Builder Script

Author a pedal's chassis, controls, and parameter mappings entirely from code. Picking **Pedal Design** in the Scripting tab's mode selector switches to this mode.

## At a glance

- **Where it lives in the UI:** Script tab → mode selector → **Pedal Design**
- **Underlying type(s):** `PedalDesign`, `PedalDesign::Control`, `PedalDesign::Mapping`
- **Persisted in:** `PedalDesign::scripts` on the active pedal (travels with the asset)
- **Scope:** Chassis + controls + mappings only. The DSP graph is left untouched — use [[graph-builder]] for that.
- **Related wiki:** [[pedal-design]], [[pedal-design-schema]], [[graph-builder]], [[pedalboard-builder]]

## How to use it

1. Select a pedal on the **Board** tab.
2. Open the **Script** tab and switch the mode dropdown to **Pedal Design**.
3. Write a script (see syntax below).
4. Hit **Compile**. The pedal's chassis, controls, and mappings are **replaced** with what the script declares.
5. Hit **← Graph** to round-trip the *current* pedal design back out as a script.

## Script API

| Function | Description |
|----------|-------------|
| `setMeta(name, author, category, description)` | Pedal metadata. All four positional, all string. |
| `setChassis(w, h, colorHex)` | Chassis dimensions in pixels + ARGB color (`0xFF8A8A94`). |
| `addKnob(id, x, y, label[, w, h])` | Add a knob. Defaults to 40×40 if `w`/`h` omitted. |
| `addSwitch(id, x, y, label[, w, h])` | Toggle / 2-pos switch. |
| `addFootswitch(id, x, y, label[, w, h])` | Bypass-style stomp switch. |
| `addLed(id, x, y, label[, w, h])` | Status LED. |
| `addFader(id, x, y, label[, w, h])` | Vertical slider. Default 30×120. |
| `addTextScreen(id, x, y, label[, w, h])` | Read-only text display. Default 120×40. |
| `mapControl(controlID, "nodeID_paramID")` | Wire a UI control to a DSP graph parameter. |

The `id` argument is the control's `controlID` — referenced from `mapControl` and from the engine's auto-mapping system.

## Examples

### Minimal overdrive faceplate

```
setMeta("My Drive", "User", "Drive", "A simple overdrive")
setChassis(200, 340, 0xFF8A8A94)

addKnob("drive", 60, 80, "Drive")
addKnob("tone",  140, 80, "Tone")
addKnob("level", 100, 160, "Level")
addLed("led",    100, 30, "")
addFootswitch("bypass", 80, 280, "")

mapControl("drive", "1_gain_db")
mapControl("tone",  "2_cutoff")
mapControl("level", "3_gain_db")
```

### Round-trip workflow

Build a pedal visually in the **Pedal** tab → switch to **Script / Pedal Design** → hit **← Graph** to emit the equivalent script → diff it against another pedal, share it, version-control it, edit it back.

## Gotchas

- **Compile is destructive.** Chassis, controls, and mappings are replaced. The DSP graph and canvas pages are kept. Hit **← Graph** first if you want a snapshot of the current state.
- **mapControl strings.** The `nodeParam` argument is the DSP graph's internal ID format: `<nodeID>_<paramID>`, e.g. `"1_gain_db"`. Easiest way to discover these is to wire a control visually once, then hit **← Graph** in Pedal Design mode and read the emitted `mapControl` lines.
- **Canvas pages and per-control styling not in v1.** Custom images, fonts, colors, font sizes, rotation ranges, and canvas overlay pages are preserved on round-trip but not exposed as script setters yet. Visual editor still required for those.
- **No FX wiring here.** This mode only touches `PedalDesign.controls`, `.mappings`, chassis, and metadata. To author the DSP graph from a script, switch to [[graph-builder]].

## See also

- [[graph-builder]] — Sibling mode for the DSP graph *inside* a pedal.
- [[pedalboard-builder]] — One level up: how to wire pedals together on a board.
- [[pedal-design-schema]] — Full JSON schema reference.

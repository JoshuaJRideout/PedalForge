# Control Surface Nodes

DSP-graph nodes that **spawn a matching control on the pedal face** when added. Drop a `ctrl_knob` in the FX graph and a knob appears on the chassis, already wired to the node's value parameter — no manual mapping required.

## At a glance

- **Where it lives in the UI:** Inventory → **Nodes** → **Controls** category
- **Underlying type(s):** `KnobNode`, `FaderNode`, `ButtonNode`, `ToggleNode`, `SelectorNode`, `FootswitchNode` in [ControlNodeLibrary.h](../../source/dsp/ControlNodeLibrary.h)
- **Bridge:** [`syncControlSurfaceNodes()`](../../source/dsp/ControlSurfaceSync.h) — runs whenever the FX graph changes
- **Persisted in:** `PedalDesign::controls` and `PedalDesign::mappings` with `controlID` prefixed `auto_node_<N>`
- **Related wiki:** [[graph-builder]], [[pedal-design]], [[pedal-design-builder]]

## How to use it

1. Open the **FX** tab on a pedal.
2. Open the Inventory overlay (Tab key) → **Nodes** → **Controls** category.
3. Drag a **Knob**, **Fader**, **Button**, **Toggle**, or **Selector** into the graph.
4. Switch to the **Pedal** tab — a matching control is now on the chassis, auto-positioned. Drag it where you want.
5. Back in the FX graph, wire the node's output to whatever DSP parameter it should control (a filter cutoff, a gain, etc.).
6. Moving the control on the pedal face now drives the DSP — no separate mapping step.

To remove the face control: delete the underlying node. The auto-managed control is cleared on the next graph change.

## Node ↔ face mapping

| Node | Face control | Primary param |
|------|--------------|---------------|
| `ctrl_knob`     | `knob`       | `value` (0..1) |
| `ctrl_fader`    | `fader`      | `value` (0..1) |
| `ctrl_button`   | `footswitch` | `pressed` (0/1, momentary) |
| `ctrl_toggle`   | `switch`     | `state` (0/1, latching) |
| `ctrl_selector` | `switch`     | `value` (integer step) |
| `footswitch`    | `footswitch` | (latching) |

The face control updates the node's primary parameter. The node's output is a steady control signal (0..1 or 0/1) you can wire to other DSP nodes.

## Auto vs. manual controls

The sync only manages controls whose `controlID` starts with `auto_node_`. User-placed controls on the chassis are completely untouched — you can mix:

- Auto-controls: spawned by adding nodes; deleted when nodes are deleted.
- Manual controls: added in the Pedal Designer the old way, with an explicit Mapping.

If you manually create a Mapping targeting a control-surface node's primary param, the sync sees the existing mapping and skips auto-creation. So the manual flow still works for users who want full control over chassis layout.

## Script API

The auto-creation happens regardless of how the node is added to the graph. From an [FX Graph Builder](graph-builder) script:

```
k = addNode("ctrl_knob")
filt = addNode("filter_svf")
connect(k, 0, filt, 1)        -- knob output → cutoff CV
```

After compile, the pedal face has a knob that controls the filter cutoff. No `addKnob(...)` line needed on the [Pedal Design Builder](pedal-design-builder) side.

## Gotchas

- **No XY-pad face control yet.** The `ctrl_xy` node exists for DSP routing but doesn't spawn a face control (HardwareDrawing has no XY-pad case). It's marked `isControlSurface() = false` for now.
- **`ctrl_selector` renders as a switch.** Multi-position rotary selectors aren't drawn by HardwareDrawing yet; the auto-control falls back to a binary switch until that lands.
- **Renaming an auto-control's `controlID` breaks the sync.** The system identifies auto-controls by the `auto_node_<N>` prefix — if you rename it, the sync treats it as a stranger and may spawn a duplicate.

## See also

- [[graph-builder]] — Programmatic node creation inside a pedal
- [[pedal-design]] — How the chassis + controls + mappings model works

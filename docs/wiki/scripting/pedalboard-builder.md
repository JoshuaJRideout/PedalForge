# Pedalboard Builder Script

Rebuild the entire pedalboard from a few lines of code. Picking **Pedalboard** in the Scripting tab's mode selector switches to this mode.

## At a glance

- **Where it lives in the UI:** Script tab → mode selector → **Pedalboard**
- **Underlying type(s):** `AudioGraphEngine`, `PedalRegistry`, `PedalInstance`
- **Persisted in:** `AudioGraphEngine::engineScripts` (engine-scoped, not per-pedal)
- **Related wiki:** [[engine]], [[graph-builder]], [[pedal-instance]]

## How to use it

1. Open the **Script** tab, set the mode dropdown to **Pedalboard**.
2. Write a script (see syntax below).
3. Hit **Compile**. The entire current board is **replaced** with what the script declares.
4. Hit **← Graph** to round-trip the *current* board back out as a script you can edit.
5. **Save** writes the script to engine state so it survives reload.

## Script API

| Function | Description |
|----------|-------------|
| `var = addPedal("Name")` | Add a factory pedal by display name. Position is auto-laid out. |
| `var = addPedal("Name", x, y)` | Same, with explicit board position in pixels. |
| `connect(src, srcCh, dst, dstCh)` | Wire one pedal's output channel to another's input. |
| `setPos(var, x, y)` | Reposition a pedal already declared. |
| `focus(var)` | Mark this pedal as the MIDI-learn target. |
| `clearBoard()` | Explicit clear (compile always clears anyway). |

The variable name on the left of `addPedal` is local to the script and re-used by later `connect/setPos/focus` calls.

## Examples

### Minimal chain

```
ts = addPedal("Hello Gain")
dly = addPedal("Delay Lab")
connect(ts, 0, dly, 0)
focus(ts)
```

### Explicit layout

```
in_amp = addPedal("Filter Sweep", 80, 200)
mid    = addPedal("Tremolo 101", 280, 200)
fx     = addPedal("Mini Synth", 480, 200)

connect(in_amp, 0, mid, 0)
connect(mid, 0, fx, 0)
```

## Gotchas

- **Compile is destructive.** The entire current board is removed before the script runs. Hit **← Graph** first if you want to save what's there.
- **Pedal names must match the registry exactly** (case-insensitive). Browse [[index]] under DSP Node Catalog or the Inventory overlay to find type names.
- **No MIDI / expression routing yet.** Only audio connections are scriptable. MIDI routing on the Route tab still has to be wired by hand. (Tracked in TODO.md §9.)
- **No live preview.** Unlike UI Script mode, board scripts don't render anything until you hit Compile.

## See also

- [[graph-builder]] — sibling for *inside* a pedal (DSP graph) instead of the board.
- [[engine]] — the underlying `AudioGraphEngine` that the script talks to.

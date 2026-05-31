# PedalForge Control Catalog & Themed Style-Kit Architecture

Status: **design / build checklist** (pre-implementation). This is the reference for
expanding PedalForge's programmatic control system from ~14 hand-drawn types into a
`type × style` engine with themed kits that ship their own on-theme controls.

> North star: controls are **code, not image assets** (see project memory:
> *asset-bundling policy*, *neutral chassis + runtime tint*). A "style kit" is a
> palette + a set of `draw()` routines, not a folder of PNGs. Image overrides
> (`CustomStyles`) still layer on top per-control for users who want them.

---

## 1. Where we are today

One idea repeated in three places, keyed by a single string `type`:

- **Behaviour** — a DSP node in [`ControlNodeLibrary.h`](../source/dsp/ControlNodeLibrary.h)
  (`KnobNode`, `FaderNode`, …) whose `getControlType()` returns `"knob"` etc.
- **Appearance** — a `draw*()` in [`HardwareDrawing.h`](../source/ui/HardwareDrawing.h),
  dispatched by `drawForType()`.
- **Data** — `PedalDesign::Control` ([`PedalDesign.h`](../source/dsp/PedalDesign.h)) — the
  serialized `type` + geometry + a `CustomStyles` bag (image/colour/font overrides).

Existing types: `knob`, `fader`, `switch`, `selector`, `footswitch`, `led`, `rgb_led`,
`indicator`, `7seg`, `display` (numeric), `vu_meter`, `oscilloscope`, `text_screen`,
`console`, `pixel_display`, `graphic`, `label`, `file_loader`/`library_loader`/`overlay_launcher`.

### Two structural limits this catalog removes
1. **Only one axis (`type`) where we need two (`type` × `style`).** No place for
   "Sci-Fi knob" vs "retro knob", or "render this whole pedal in the LCARS kit."
2. **Dispatch carries a single `float value`** — already lossy (`rgb_led` is faked by
   passing `value, value*0.5, 1-value`). Multi-value controls (XY, lit knobs, EQ,
   scopes) can't flow through it.

---

## 2. The 7 interaction archetypes

Every control across every domain we surveyed clusters by **value shape**. This is the
backbone: build the carriers once, and ~45 controls become "renderer + node-shape."

| # | Archetype | Value shape | Have | Gap |
|---|---|---|---|---|
| 1 | Continuous scalar | 1 float | knob, fader | encoder, stepper, gauge, ribbon, bezel |
| 2 | Discrete | 1 int | switch, selector, footswitch | radio/segmented, dropdown, multi-toggle, radial |
| 3 | **2D / spatial** | x,y(,z) | — | XY pad, joystick, vector/4-corner morph |
| 4 | **Vector / array** | float[] | — | multislider, graphic EQ, step grid, envelope, range, matrix, PIP |
| 5 | **Signal display** | buffer | scope, VU, 7seg | spectrum, goniometer, spectrogram, correlation, LUFS, trend/sparkline |
| 6 | Text / selection | string(s) | text_screen, label | scribble strip, number box, list/preset browser |
| 7 | **Addressable light** | RGBA+bright | rgb_led (faked) | LED ring, RGB pad — promoted to a cross-cutting layer (§5.2) |

---

## 3. `ControlState` — the unified carrier

Replaces the single `float value` in the dispatch. Build these fields once:

```
struct ControlState {
    float        value      = 0.0f;   // archetype 1/2 — primary scalar / index
    float        x, y, z    = 0.0f;   // archetype 3   — spatial axes
    const float* array      = nullptr;// archetype 4   — bars/steps/bands
    int          arrayLen   = 0;
    const float* buffer     = nullptr;// archetype 5   — waveform / history samples
    int          bufferLen  = 0;
    Light        light;               // archetype 7   — {r,g,b,brightness}
    juce::StringArray text;           // archetype 6   — lines / labels
    // cross-cutting (§5):
    Status       status     = Status::Off; // off/on/blink/ack  (annunciator)
    const float* zones      = nullptr;     // threshold bands (redline/alarm); pairs of [pos,hue]
    int          zonesLen   = 0;
};
```

`drawForType()` → `kit.draw(type, area, const ControlState&, custom)`.

---

## 4. The control catalog (~45 types)

Legend: **[have]** exists today · **[new]** to build · archetype number in parens.
"Node shape" = what the DSP control-surface node must output/consume.

### A. Continuous scalar (1)
- **knob** [have] — rotary, arc sweep. Node: 1 control out.
- **fader** [have] — linear slider (vert/horiz). Node: 1 control out.
- **encoder** [new] — endless rotary, no end-stop; the software-param standard. Node: relative delta → accumulated value. Pairs with LED ring (§5.2).
- **stepper / spinner** [new] — value + ▲▼ (Max `incdec`/`number`); precise + accessibility-friendly. Node: 1 control out, discrete step.
- **gauge / needle dial** [new] *(output)* — car-tach style readout with colored **redline/zone** arc. Uses `zones`. Node: 1 control **in**.
- **ribbon / touch strip** [new] — position-on-touch (+gate). Node: position out + gate out.
- **rotating bezel / digital crown** [new] — spin-the-frame / fine-detented edge wheel. Encoder variant, distinct feel.

### B. Discrete / selection (2)
- **switch** [have] · **footswitch** [have] · **selector** (rotary, N positions) [have].
- **radio bank / segmented** [new] — N exclusive options all visible (better than rotary for ≤6 modes). Node: 1 int out.
- **dropdown menu** [new] — long lists (Max `umenu`). Node: 1 int out.
- **multi-toggle / gate bank** [new] — row of independent on/offs (Max `gswitch`). Node: N gate outs.
- **radial / weapon wheel** [new] — hold-to-open ring, flick to select. Touch-first selector. Node: 1 int out.
- **engine-order-telegraph selector** [new] — chunky detented arc, named zones (Slow/Half/Full). Characterful stepped selector (also a *style* of `selector`).

### C. 2D / spatial (3) — biggest expressive unlock
- **XY pad** [new] — two params at once. Node: 2 control outs. (Max `pictslider` = slider OR 2D pad.)
- **joystick** [new] — self-centering XY (+optional Z-twist → up to 3 axes). Node: 2–3 outs.
- **vector / 4-corner morph pad** [new] — blend among 4 sources from one point. Node: 4 weight outs (or x,y + internal mix).

### D. Vector / array (4) — opens sequencing & spectral pedals
- **multislider** [new] — array of bars, draw a curve in one gesture. Node: float[] out.
- **graphic EQ / band display** [new] — labelled multislider. Node: float[] out (per-band gain).
- **step sequencer grid** [new] — per-step value/velocity/probability/param-lock. Node: stepped float[] + clock in.
- **envelope / breakpoint editor** [new] — draggable points + curved segments (ADSR-beyond). Node: shape → sampled control out.
- **range slider (two-thumb)** [new] — min/max span (Max `rslider`, JUCE TwoValue). Node: 2 control outs.
- **matrix / mod-matrix grid** [new] — crosspoint route-anything grid (Max `matrixctrl`). Node: NxM routing.
- **PIP / triangle distributor** [new] — distribute a fixed budget across 3–4 sinks (Elite Dangerous power triangle). Constrained multislider summing to 100%. Node: N normalized outs.

### E. Signal displays (5) *(output-only)*
- **7seg** [have] · **numeric display** [have] · **vu_meter** [have] · **oscilloscope** [have] · **pixel_display** [have].
- **spectrum analyzer** [new] — FFT bars. Buffer in.
- **spectrogram** [new] — colour = amplitude over time/freq. Buffer in.
- **goniometer / vectorscope** [new] — stereo image (Lissajous). Stereo buffer in.
- **correlation meter** [new] — −1..+1 phase. 1 control in.
- **LUFS / loudness + histogram** [new] — loudness over time. 1 control in.
- **gain-reduction meter** [new] — comp/limiter GR. 1 control in.
- **trend / sparkline strip** [new] — rolling history of any param (SCADA/medical). Uses `buffer` as ring history. 1 control in.
- **tape readout** [new] — vertical scrolling numeric tape (altimeter/speed), big-range alt to a knob readout.
- **compass / heading-bug strip** [new] — wrap-around scrolling tape with draggable target bug. 1 control in/out.

### F. Text / selection (6)
- **text_screen** [have] · **console** [have] · **label** [have].
- **scribble strip** [new] — per-footswitch label + colour + status (Helix/HeadRush); ideal for Companion foot controllers. Text + light + status.
- **number box** [new] — type/scrub a number (Max `number`). 1 control out.
- **list / preset browser** [new] — scrollable selectable list. 1 int out + names in.

### G. Loaders / launchers (existing utility)
- **file_loader / library_loader / overlay_launcher** [have] — keep as-is.

---

## 5. Three cross-cutting concepts (from cockpit/military/racing/SCADA)

These are **not widgets** — they're capabilities most controls can opt into. Cheap to
add as flags + a small state machine; high payoff.

### 5.1 Guarded / confirmed controls (safety interlock)
Aviation/military **guarded switch** (flip cover before toggle), keylock, detent.
- Model: per-control `guard` flag (`none | cover | hold | keylock`) + arm state.
- Render: cover overlay / "hold to confirm" ring / key icon.
- Use: panic-kill, preset-wipe, tuning-critical controls you don't want bumped live.

### 5.2 Addressable light layer (annunciator + lit controls)
Generalises today's faked `rgb_led` into a **light channel any control can expose**, plus
a status state machine from annunciator panels / master-caution / SCADA alarms.
- `Light {r,g,b,brightness}` driven by a graph-wired input.
- `Status { Off, On, Blink, Ack }` — latch + acknowledge behaviour.
- Hosts: **LED ring** around knob/encoder (position OR meter), **RGB-backlit pad/button**,
  **lamp / annunciator bank** (grid of named status lamps), **shift-light / RPM LED bar**
  (sequential strip, colour-shifts past threshold via `zones`).

### 5.3 Shift / page / bank layer ("one control, many params")
The biggest cross-domain lesson: F1 wheels drive 100+ params from a dozen controls; the
Source Audio pedal does 8 params on 4 knobs via an **ALT** button; MFD bezel **soft keys**
relabel by page; DSLR command dials change meaning by mode.
- Model: a control can be a **shift/page control** that re-targets other controls; other
  controls bind a value *per page*.
- Integrates with existing `overlay_launcher` + canvas pages.
- Payoff: small faceplates expose deep pedals — essential given limited pedal surface.

These extend `ControlState`/`Control` with: `Status status`, `zones[]`, `buffer` history,
and `Control` flags `guard`, `shiftBinding`.

---

## 6. Themed style kits

A **StyleKit** is the unit the user picks ("render this pedal in *LCARS*"). Crucially a kit
does **two** things:

1. **Restyles universal types** — its own `draw(knob/fader/switch/meter/...)`.
2. **Declares signature types** — on-theme controls it renders best and that define its
   character (the LCARS kit *wants* nested arcs; the race kit *wants* a shift-bar).

```
struct StyleKit {
    juce::String id;                       // "default", "lcars", "race", ...
    Palette      defaultColorway;          // the kit's "house" colours (a starting point)
    bool draws(const String& type) const;  // which types this kit implements
    void draw(type, area, state, colorway, custom); // per-type render — reads ROLES, not literals
    StringArray signatureTypes;            // on-theme controls to surface first
};
```

A kit **never emits colours** in `draw()`. Each painted element carries an **emphasis**
(`0..1`) and an optional **role tag**; the active **colorway** resolves that pair to an
actual colour at draw time (§6.5). This is what makes the colorway axis work — and lets
one kit render hundreds of colorways from a single seed, Apple-tinted-icon style.

**Fallback rule:** a kit may be **partial**. Any type it doesn't implement falls back to
the `default` kit *per type*, so kits can ship incrementally and still look coherent.

### Kit vocabularies (reference grammars)

| Kit | Restyle signature | Signature on-theme controls |
|---|---|---|
| **default** [have] | current dark/gradient look | the existing 14 — the fallback for all kits |
| **glass cockpit / avionics** | amber/cyan, flat, precise | attitude gauge, tape readout, heading-bug strip, annunciator bank, guarded switch |
| **LCARS / Trek** | nested arcs, swooshes, oval buttons, "Okudagram" labels, flat glowing segments | arc selector, swoosh bar, segmented block buttons |
| **sci-fi FUI / holo** | cyan glow, scan rings, brackets, **pulse-on-change** animation, reticles | scanning-ring knob, reticle selector, holo meter |
| **race / F1** | high-contrast LCD, group rotaries | RPM shift-bar, brake-bias readout, multi-position group rotary |
| **retro 80s hi-fi** | backlit needles, wood/brushed metal, paddle rockers | VU needle meter, slider EQ bank, paddle rocker switch |
| **heavy equipment / rugged** | chunky, high-vis, hazard stripes | rocker switch, proportional joystick, hour-meter, load gauge |
| **game HUD** | bold, animated, glowing | cooldown ring, segmented bar, radial/weapon wheel, diegetic readout |

> Per *musicians-first* (project memory): the **`type × style` refactor + new control
> types** is feature work to do now; **shipping the full stack of extra theme kits** is
> the part that was "parked." The refactor is the substrate that makes kits cheap later —
> build it now, ship `default` first, add kits incrementally.

---

## 6.5 Colorways — the third axis (`type × style × colorway`)

A **colorway** is an independent, user-pickable axis. One kit rendered under N colorways =
N visually distinct sets — *one kit becomes hundreds* with no new draw code and no image
assets. This generalises the runtime-tint principle (project memory) from "one accent" to
a full recolouring scheme.

### The core trick: kits emit EMPHASIS, colorways resolve it to colour
The mistake is to have `draw()` pick colours (literal or even named roles), because then
every meaningful colour distinction (redline, caution, status) has to be re-specified per
colorway and naive tinting flattens it. Instead — modelled on **Apple's tinted app
icons** — separate *structure* from *colour*:

- The kit `draw()` does **not** emit colours. It emits, per painted element, an
  **emphasis** value `0..1` (how hot/active/important this pixel-region is) plus an
  optional **role tag** (`chrome | accent | status | text | glow`).
- The **colorway** decides how `(emphasis, role)` becomes an actual `Colour`.

Because contrast is carried by **emphasis (lightness/intensity)**, not by hue, structure
survives any recolour. The user's example: a tach in a **blue** colorway is blue up to the
redline, and the redline is a **brighter blue** — "redline = danger" still reads, because
danger = high emphasis = the bright end of the ramp. Same draw code, any hue.

### Two resolution strategies a colorway can use
```
struct Colorway {
    Mode mode;                  // Tint | Semantic
    // Tint (Apple-style): one (or few) hues; emphasis -> position on a ramp
    Ramp  ramp;                 // hue(s) + lightness/sat curve; danger = bright end
    // Semantic: discrete colours per role/zone (literal "red = hot")
    Colour chrome, accent, text, glow;
    Colour ok, warn, danger;    // used only in Semantic mode
    Colour bg, surface, edge;   // chassis/panel/bevel (both modes)
};
```
- **Tint mode** → maximum multiplier: a single seed hue paints a whole coherent set;
  zones/status differentiate by brightness along the ramp. This is the headline feature.
- **Semantic mode** → for kits/users who want literal safety cues (cockpit amber, a real
  red redline). The kit's emphasis+role output feeds discrete colours instead of a ramp.

A kit author writes `draw()` **once** (emphasis + role); it works under both modes.

### The multiplier: generate a colorway from 1–3 seeds
The user shouldn't hand-pick a palette. A colorway is **derived**:
- User sets **1–3 seeds** (often just one accent hue, Apple-style).
- Tint mode builds the ramp from the seed (lightness/sat curve, bright end for high
  emphasis); Semantic mode fills remaining roles by HSV rotation / complementary schemes.
- **Contrast auto-correct** guarantees text-vs-bg legibility regardless of seed.
- Ship a few curated presets **per kit** as starting points ("Midnight", "Sunburst",
  "Mono", "Vaporwave", plus per-kit on-theme ones like LCARS amber/blue).

Catalogue size is `kits × colorways × types`, but only `kits × types` is *code* — colorways
are tiny data (often one seed). A user "set" = `{kit, colorway}`, serialized on the
pedal/board and shareable like any other design.

### Interaction with existing per-control overrides
Precedence, lowest → highest: **kit house colorway → pedal/board colorway → per-control
`CustomStyles.customColour`/image**. Existing single-colour and image overrides still win
locally, so nothing regresses; the colorway just sets the canvas everything starts from.

---

## 7. Build sequence

> **Phase 0 status: COMPLETE & build-verified** (`PedalForge_All`, exit 0).
> `ControlState` ([source/ui/ControlState.h](../source/ui/ControlState.h)),
> `Colorway` emphasis+role resolver ([source/ui/StyleColor.h](../source/ui/StyleColor.h)),
> `StyleKit`+`DefaultStyleKit`+registry ([source/ui/StyleKit.h](../source/ui/StyleKit.h)).
> `style` (per-control) + `styleKit`/`colorwaySeed`/`colorwayMode` (per-pedal)
> serialized in `PedalDesign` (omit-when-default). All five render call sites
> (PedalPainter, CanvasOverlay, PedalDesignerComponent ×3, InventoryOverlay ×2)
> route through `StyleKitRegistry`; the default kit delegates to HardwareDrawing so
> visuals are unchanged. **Next: Phase 1 (carriers wired end-to-end).**

**Phase 0 — behaviour-preserving refactor (verify before any visual change).**
- Introduce `ControlState`; convert `drawForType` → `StyleKit::draw`.
- Introduce the **emphasis+role → Colorway** resolution path and thread a colorway through
  `draw()`. Implement the `default` kit reproducing today's exact visuals by mapping its
  current colour literals → (emphasis, role) emissions resolved by a Semantic colorway
  that reproduces those exact colours (proves the indirection is lossless).
- Add `style` + `colorway` fields to `Control`/`PedalDesign` (default to `"default"` +
  the kit's house colorway) + pedal/board-level defaults.
- Verify via the in-app AI assistant script round-trip (`read_pedal_as_script` /
  `run_script`) — no visual diff expected.

**Phase 1 — carriers.** _DONE & build-verified._ Added `pf::buildControlState(id, value,
data*, texts*)` ([ControlState.h](../source/ui/ControlState.h)) that assembles
value + array + buffer (from the instance's `controlData`) + text (from `controlTexts`)
in one place; PedalPainter + CanvasOverlay kit branches now fill carriers through it
(no more per-site partial assembly). Added the two cross-cutting `Control` flags
`guard` (0=none,1=cover,2=hold,3=keylock) + `shiftBinding`, serialized omit-when-default.
_Deferred to their producers:_ `light`/`status`/`zones` carriers exist but are populated
when Phase 2/3 controls that emit them land (no current node produces them).

> **Phase 2 status (in progress).** XY pad + joystick **renderers** landed &
> build-verified: `drawXYPad`/`drawJoystick` in HardwareDrawing.h (read x,y),
> `"xypad"`/`"joystick"` cases in `drawForType` + `DefaultStyleKit::draw`,
> `buildControlState` now fills `x`/`y` from `controlValues["<id>_x"/"_y"]`
> (centre-default), both added to the InventoryOverlay "Controls" palette and the
> designer size table. _Remaining (Phase 2b):_ make `XYPadNode` a control surface
> + teach ControlSurfaceSync/poller the 2-param (x,y) binding + 2D drag — a
> larger interaction-layer change tracked separately.

**Phase 2 — priority new controls** (capability per unit effort):
1. XY pad + joystick (arch 3).
2. Addressable light layer + annunciator/shift-light (5.2).
3. Multislider → graphic EQ / step grid (arch 4).
4. Encoder + stepper + radio/segmented (arch 1/2).
5. Signal displays as data plumbing allows (arch 5).
6. Scribble strip (arch 6) — lands on Companion foot-controller plans.

**Phase 3 — cross-cutting concepts:** guard/confirm (5.1) + shift/page layer (5.3).

**Phase 4 — first non-default StyleKit** (pick one: LCARS or race) end-to-end, including
its signature controls — proves the kit abstraction before scaling to the rest.

---

## 8. Source domains surveyed
Audio: Max/MSP UI objects, JUCE sliders, Loopy Pro, Drambo, Bitwig, Ableton, HALion,
grooveboxes (param locks), Eurorack (joystick/ribbon/touchplate), Push/Maschine,
Helix/HeadRush/Quad Cortex, metering plugins. Cross-domain: glass cockpit (PFD/HSI,
annunciators, guarded switches), fighter HOTAS/MFD soft keys, car clusters (tach/redline),
F1 wheels (group rotaries, shift lights, ALT), heavy equipment (proportional joystick,
LMI), sci-fi FUI/LCARS, game HUDs (radial menus, cooldown rings, diegetic UI), SCADA HMI
(mimic, trend, alarm banner), smartwatch (activity rings, rotating bezel/crown), medical
monitors (waveform/trend/alarm-limit), DSLR (mode/command dials).

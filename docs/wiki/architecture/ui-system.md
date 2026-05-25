# UI System

PedalForge's UI is built with JUCE and organized around a tab-based architecture with overlay modals, a custom LookAndFeel, and expression-powered rendering.

## LookAndFeel

`PedalForgeLookAndFeel` (inherits `juce::LookAndFeel_V4`) provides a dark workshop theme.

### Color Palette

| Name | Hex | Usage |
|------|-----|-------|
| `bgDark` | `#0F0F14` | Deepest background |
| `bgMid` | `#1A1A24` | Panel backgrounds |
| `bgLight` | `#252535` | Elevated surfaces |
| `gridLine` | `#2A2A3A` | Grid lines, dividers |
| `accent` | `#6366F1` | Primary accent (Indigo) |
| `accentBright` | `#818CF8` | Hover/active accent |
| `textPrimary` | `#E2E8F0` | Main text |
| `textSecondary` | `#94A3B8` | Secondary text |
| `textMuted` | `#64748B` | Disabled/hint text |
| `danger` | `#EF4444` | Error states |
| `success` | `#22C55E` | Success states |
| `cableMono` | `#CBD5E1` | Mono audio cables |
| `cableStereo` | `#60A5FA` | Stereo audio cables |

### Custom Overrides
- `drawRotarySlider` — Rounded arc-style knobs
- `drawButtonBackground` — Rounded rectangle buttons with hover glow
- `drawComboBox` — Dark dropdown with accent border
- `drawPopupMenuItem` — Dark menu with highlight bar

## Tab Architecture

`PedalForgeEditor` manages all tabs. Each tab button belongs to a radio group so only one is active at a time.

```
Toolbar: [Play] [Board] [Route] [Pedal] [FX] [Script] [Wiki] [Library] [Store] [MIDI]
```

Tab switching in `buttonClicked()`:
1. Hide all tab components
2. Show the selected tab's component
3. Pass `activePedal` and engine references where needed

## Canvas Overlay System

`CanvasOverlay` provides full-screen overlay panels for detailed pedal editing. It renders the pedal's `CanvasPage` controls on a dedicated surface.

Key features:
- Launched by double-clicking a pedal or pressing an `overlay_launcher` control
- Each page has its own control layout (knobs, grids, screens)
- `DynamicDisplayComponent` renders controls using `ExpressionVM` for custom graphics
- Supports mouse interaction forwarding to controls

## Dynamic Display Component

`DynamicDisplayComponent` (~103KB, the largest UI file) handles rendering of all control types:
- Knobs with custom images and arcs
- Switches with multi-position states
- Text screens with scrolling
- Grid sequencers with step editing
- Pixel displays with shader support
- Live waveform scopes

It bridges the control values from `PedalInstance::controlValues` to the visual representation, and vice versa when the user interacts.

## Pedalboard Grid

`PedalboardGrid` is the Board tab's main component:
- Grid layout with configurable rows/columns
- Drag-and-drop pedal placement
- Snap-to-grid positioning
- Multi-page support with page navigation
- Pedal selection → updates `activePedal`

## Overlay System

| Overlay | Purpose |
|---------|---------|
| `InventoryOverlay` | Q-menu for adding pedals (press Q) |
| `LibraryOverlay` | File picker for NAM/IR assets |
| `CanvasOverlay` | Full-screen pedal editing overlay |
| `NotesOverlay` | Sticky notes on any tab |

## Node Graph Editor

`NodeGraphEditor` (FX tab) provides visual node editing:
- Add nodes from categorized palette
- Drag wire connections between ports
- Node selection, copy/paste, delete
- Parameter editing via node inspector
- Scroll/zoom canvas
- Sticky notes

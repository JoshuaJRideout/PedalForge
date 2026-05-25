# Color Palette & Theming

PedalForge uses a consistent dark-mode color palette inspired by modern design systems. All colors are defined in the `PedalForgeLookAndFeel` class.

## Core Colors

| Variable | Hex | Preview | Description |
|----------|-----|---------|-------------|
| `bgDark` | `#0F0F14` | ■ | App background / canvas |
| `bgPanel` | `#151520` | ■ | Sidebar / panel backgrounds |
| `bgCard` | `#1E1E2E` | ■ | Cards, code blocks, inputs |
| `bgElevated` | `#252535` | ■ | Elevated surfaces, dropdowns |
| `border` | `#2A2A3A` | ■ | Dividers, outlines, separators |

## Text Colors

| Variable | Hex | Preview | Description |
|----------|-----|---------|-------------|
| `textPrimary` | `#E2E8F0` | ■ | Primary text |
| `textSecondary` | `#94A3B8` | ■ | Secondary text, labels |
| `textMuted` | `#64748B` | ■ | Muted text, placeholders |
| `textDisabled` | `#475569` | ■ | Disabled / inactive text |

## Accent Colors

| Name | Hex | Preview | Usage |
|------|-----|---------|-------|
| Indigo | `#6366F1` | ■ | Primary accent — buttons, active states |
| Indigo Bright | `#818CF8` | ■ | Hover states, links, active sidebar items |
| Emerald | `#10B981` | ■ | Success, active (e.g. Test Sound button on) |
| Cyan | `#67E8F9` | ■ | Code text, highlights |
| Pink | `#EC4899` | ■ | Directives, special syntax |
| Green | `#34D399` | ■ | Variables, assignments |
| Yellow | `#FBBF24` | ■ | Warnings, caution states |
| Red | `#EF4444` | ■ | Errors, danger, clipping indicators |

## Transparency

| Usage | Value |
|-------|-------|
| Hover overlay | `0x15FFFFFF` |
| Active background | `0xFF6366F1` at 20% alpha |
| Alternating rows | `0x0AFFFFFF` |

## Component Styling

### Buttons

```
Background:   #1E1E2E (bgCard)
Border:       #2A2A3A (border)
Text:         #E2E8F0 (textPrimary)
Hover:        lighten border to #3A3A4A
Active/On:    #6366F1 (indigo)
```

### Tab Buttons (Toolbar)

```
Inactive:     bgCard background, textPrimary text
Active:       indigo accent background at 20% alpha, indigo bright text
Active bar:   3px indigo bar on left edge
```

### Input Fields

```
Background:   #1E1E2E
Border:       #2A2A3A
Text:         #E2E8F0
Placeholder:  #64748B (textMuted)
Focus border: #6366F1 (indigo)
```

### Knobs & Controls

Knobs use the pedal's `customColour` for their arc indicator. Default is red (`#FF0000`) but can be customized per-control in the [[pedal-design-schema|reference/pedal-design-schema]].

### Code / Monospace

```
Font:         System monospaced font, 13px
Background:   #1A1A28
Border:       #2A2A3A, rounded 6px
Comments:     #6B7280 (gray)
Directives:   #EC4899 (pink)
Assignments:  #34D399 (green)
Default:      #67E8F9 (cyan)
```

## Chassis Colors

Pedal chassis colors are customizable via `chassisColour`. Common presets:

| Name | Hex | Description |
|------|-----|-------------|
| Silver | `#8A8A94` | Default |
| Black | `#1A1A24` | Dark |
| White | `#E8E8F0` | Light |
| Vintage Green | `#3B6B4F` | Classic |
| Burst Orange | `#D97706` | Warm |
| Deep Purple | `#6B21A8` | Bold |

## Canvas Page Colors

Canvas pages (overlays) default to `#222222` but can be customized via `backgroundColour`.

## Working with Colors in Scripts

In [[ui-scripts|scripting/ui-scripts]], use:

```
-- Set color by hex value
colourHex(0xFF6366F1)       -- Indigo accent

-- Set color by RGB
colour(99, 102, 241)        -- Indigo accent

-- Set color with alpha
colour(99, 102, 241, 128)   -- 50% transparent indigo

-- Use opacity
opacity(0.5)                -- 50% opacity for next draw call
```

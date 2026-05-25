# UI Drawing Scripts

The ExpressionVM supports a rich set of drawing functions that can be used to create custom pedal UIs, shader displays, and canvas overlays.

## Canvas Coordinate System

- Origin `(0, 0)` is the top-left corner of the drawing surface
- X increases to the right, Y increases downward
- Coordinates are in pixels relative to the component bounds
- The variables `w` and `h` provide the current canvas width and height

## Drawing Functions

### Shapes

```
rect(x, y, width, height)          -- Filled rectangle
rectOutline(x, y, width, height)   -- Rectangle outline (stroke)
roundRect(x, y, w, h, radius)      -- Rounded rectangle
circle(cx, cy, radius)             -- Filled circle
circleOutline(cx, cy, radius)      -- Circle outline
ellipse(x, y, w, h)               -- Filled ellipse
arc(cx, cy, radius, startAngle, endAngle)  -- Arc stroke
pie(cx, cy, radius, startAngle, endAngle)  -- Pie/wedge fill
```

### Lines & Paths

```
line(x1, y1, x2, y2)              -- Line segment
lineWidth(pixels)                   -- Set stroke width
moveTo(x, y)                       -- Begin path at point
lineTo(x, y)                       -- Add line segment to path
closePath()                         -- Close current path
fillPath()                          -- Fill the current path
strokePath()                        -- Stroke the current path
```

### Text

```
text(str, x, y)                    -- Draw text at position
textCentered(str, x, y, w, h)     -- Draw text centered in rect
fontSize(size)                      -- Set font size (pixels)
fontBold()                          -- Set bold font style
fontNormal()                        -- Set normal font style
fontMono()                          -- Set monospaced font
```

### Color

```
colour(r, g, b)                    -- Set fill/stroke colour (0-255)
colour(r, g, b, a)                 -- Set colour with alpha
colourHex(hexValue)                -- Set colour from hex (e.g. 0xFF6366F1)
opacity(alpha)                      -- Set global opacity (0.0-1.0)
gradient(x1, y1, x2, y2, c1, c2)  -- Set linear gradient fill
```

### Images

```
image(path, x, y, w, h)           -- Draw image from asset path
imageRotated(path, x, y, w, h, angle)  -- Draw rotated image
```

### Transform

```
translate(dx, dy)                   -- Translate origin
rotate(angle)                       -- Rotate canvas (radians)
scale(sx, sy)                       -- Scale canvas
save()                              -- Save transform state
restore()                           -- Restore transform state
```

### Clipping

```
clip(x, y, w, h)                  -- Set rectangular clip region
clipRound(x, y, w, h, radius)     -- Set rounded rect clip region
resetClip()                         -- Clear clip region
```

## Built-in Variables

These variables are automatically available in UI scripts:

| Variable | Description |
|----------|-------------|
| `w` | Canvas width in pixels |
| `h` | Canvas height in pixels |
| `t` | Time in seconds since start (for animation) |
| `dt` | Delta time since last frame |
| `mx` | Mouse X position (relative to canvas) |
| `my` | Mouse Y position |
| `md` | Mouse down state (1.0 if pressed) |
| `v0`..`v15` | Control input values (0-1) from mapped controls |

## Example: Custom Knob Display

```
-- Knob with arc indicator and value readout
@param cutoff 0 1 0.5

-- Background
colourHex(0xFF1E1E2E)
circle(w/2, h/2, 40)

-- Arc indicator
lineWidth(4)
colourHex(0xFF6366F1)
arc(w/2, h/2, 35, -2.4, -2.4 + cutoff * 4.8)

-- Center dot
colourHex(0xFFE2E8F0)
circle(w/2, h/2, 6)

-- Value text
fontSize(12)
textCentered(floor(cutoff * 100) .. "%", w/2 - 20, h/2 + 45, 40, 16)
```

## Example: Animated VU Meter

```
-- Vertical VU meter with peak hold
@param level 0 1 0

-- Background
colourHex(0xFF0F0F14)
rect(0, 0, w, h)

-- Meter bar
barH = level * (h - 20)

-- Green zone
colourHex(0xFF10B981)
rect(10, h - 10 - barH, w - 20, min(barH, h * 0.6))

-- Yellow zone
if level > 0.6 then
  colourHex(0xFFFBBF24)
  rect(10, h - 10 - barH, w - 20, min(barH - h * 0.6, h * 0.2))
end

-- Red zone
if level > 0.8 then
  colourHex(0xFFEF4444)
  rect(10, h - 10 - barH, w - 20, barH - h * 0.8)
end

-- Border
colourHex(0xFF2A2A3A)
rectOutline(10, 10, w - 20, h - 20)
```

## Example: Shader Display Animation

```
-- Waveform visualization for disp_shader node
-- v0 receives amplitude from env_follower

colourHex(0xFF0F0F14)
rect(0, 0, w, h)

lineWidth(2)
colourHex(0xFF818CF8)

cx = w / 2
cy = h / 2
amp = v0 * 30

i = 0
while i < w do
  y = cy + sin((i / w) * 6.28 + t * 3) * amp
  if i == 0 then
    moveTo(i, y)
  else
    lineTo(i, y)
  end
  i = i + 2
end
strokePath()
```

## Canvas Pages

In [[pedal-design|architecture/pedal-design]], UI scripts are organized into **canvas pages**. Each page has:

- `name` — Display name shown in the page selector
- `script` — The ExpressionVM drawing code
- `width` / `height` — Canvas dimensions

Multiple pages let you create multi-screen pedal interfaces (e.g., "Main", "EQ", "Settings").

# ExpressionVM

The `ExpressionVM` is a lightweight bytecode virtual machine that powers custom DSP expressions and UI rendering in PedalForge. It compiles a simple expression language into bytecode and executes it on a stack machine.

## Language Syntax

The expression language supports variables, math, functions, and drawing commands:

```
-- Comments: use --, //, or #

-- Variables (auto-created on first use)
x = 3.14
y = sin(x) * 2.0

-- Math operators
result = (a + b) * c / d - e % f

-- Function calls
filtered = clamp(value, 0.0, 1.0)
mixed = lerp(dry, wet, 0.5)
```

## Directives

Directives configure node I/O (used in ExpressionNode DSP mode):

```
@inputs 2        -- Number of audio inputs
@outputs 1       -- Number of audio outputs
@parameters      -- Declare custom parameters:
  freq 20 20000 1000
  res 0 1 0.5
```

## Built-in Functions

### Math (1 argument)
| Function | Description |
|----------|-------------|
| `sin(x)` | Sine |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent |
| `asin(x)` | Arcsine (clamped -1..1) |
| `acos(x)` | Arccosine (clamped -1..1) |
| `atan(x)` | Arctangent |
| `abs(x)` | Absolute value |
| `sign(x)` | Sign (-1, 0, or 1) |
| `floor(x)` | Floor |
| `ceil(x)` | Ceiling |
| `sqrt(x)` | Square root (of abs) |
| `exp(x)` | e^x (clamped -80..80) |
| `log(x)` | Natural log |
| `log2(x)` | Log base 2 |
| `tanh(x)` | Hyperbolic tangent |

### Math (2 arguments)
| Function | Description |
|----------|-------------|
| `min(a, b)` | Minimum |
| `max(a, b)` | Maximum |
| `pow(a, b)` | Power |

### Math (3 arguments)
| Function | Description |
|----------|-------------|
| `clamp(x, lo, hi)` | Clamp x to [lo, hi] |
| `lerp(a, b, t)` | Linear interpolation |
| `cond(test, ifTrue, ifFalse)` | Conditional (if test > 0.5) |

### Comparison (return 0.0 or 1.0)
| Function | Description |
|----------|-------------|
| `gt(a, b)` | a > b |
| `ge(a, b)` | a >= b |
| `lt(a, b)` | a < b |
| `le(a, b)` | a <= b |
| `eq(a, b)` | a == b (within 1e-5) |
| `ne(a, b)` | a != b |

### Logic (return 0.0 or 1.0)
| Function | Description |
|----------|-------------|
| `and(a, b)` | Both > 0.5 |
| `or(a, b)` | Either > 0.5 |
| `not(a)` | a <= 0.5 |

### Drawing (UI Script mode)
| Function | Description |
|----------|-------------|
| `rect(x, y, w, h, color)` | Draw rectangle outline |
| `rectFill(x, y, w, h, color)` | Draw filled rectangle |
| `circle(x, y, r, color)` | Draw circle outline |
| `circleFill(x, y, r, color)` | Draw filled circle |
| `line(x1, y1, x2, y2, thickness, color)` | Draw line |
| `text(value, x, y, size, color)` | Draw text (numeric value) |
| `image(imgIdx, x, y, w, h)` | Draw image |

### Parameter Access
| Function | Description |
|----------|-------------|
| `getParam(idx)` | Get parameter value by index |
| `setParam(idx, val)` | Set parameter value by index |

## Built-in Variables

When used in DSP mode (ExpressionNode), these variables are available:

| Variable | Description |
|----------|-------------|
| `in0`, `in1`, ... | Input samples |
| `out0`, `out1`, ... | Output samples (write to these) |
| `sr` | Sample rate |
| `t` | Time in seconds (increments per sample) |
| `dt` | 1 / sample rate |
| `pi` | 3.14159... |

When used in UI mode, these mouse variables are available:

| Variable | Description |
|----------|-------------|
| `mouseX` | Mouse X position (0..width) |
| `mouseY` | Mouse Y position (0..height) |
| `mouseDown` | 1.0 if mouse is pressed |
| `width` | Canvas width |
| `height` | Canvas height |

## Bytecode Architecture

The VM uses a stack-based architecture:

```
Source Code → Compiler → Bytecode → Executor

Opcodes: OP_PUSH_CONST, OP_PUSH_VAR, OP_STORE_VAR,
         OP_ADD, OP_SUB, OP_MUL, OP_DIV, ...
         OP_DRAW_RECT, OP_FILL_RECT, ...
         OP_END
```

### Execution
- **Stack**: 256-element float stack
- **Variables**: 64 named variable slots
- **Constants**: Stored in a separate table
- **Drawing**: Accumulated into a `DrawCommand` list, rendered by the host

### Draw Commands

```cpp
struct DrawCommand {
    enum Type { Rect, FillRect, Circle, FillCircle, Line, Text, Image };
    Type type;
    float args[8];  // Command-specific arguments
};
```

The host component (e.g., `ScriptingTabComponent`'s preview canvas) iterates the draw commands and renders them using JUCE `Graphics`.

## Example: DSP Expression

```
@inputs 1
@outputs 1

-- Simple soft clipper with drive parameter
drive = getParam(0) * 10.0 + 1.0
x = in0 * drive
out0 = tanh(x) / tanh(drive)
```

## Example: UI Script

```
-- Animated spinning circle
angle = t * 2.0
cx = width * 0.5 + cos(angle) * 80.0
cy = height * 0.5 + sin(angle) * 80.0
circleFill(cx, cy, 20, 0xFF6366F1)

-- Interactive: highlight when mouse is near
dx = mouseX - cx
dy = mouseY - cy
dist = sqrt(dx * dx + dy * dy)
glow = cond(lt(dist, 40), 0xFFFFFFFF, 0x44FFFFFF)
circleFill(cx, cy, 25, glow)
```

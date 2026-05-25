<!-- AUTO-GENERATED from ExpressionVM::dumpFunctionsAsMarkdown(). Do not edit by hand. -->

# ExpressionVM — Built-in Functions

Every function callable from a UI / DSP / FX Graph script. Generated from the registry in [ExpressionVM.h](../../source/dsp/ExpressionVM.h).

## Math

| Function | Args | Description |
|----------|------|-------------|
| `sin(x)` | 1 | Sine of x (radians). |
| `cos(x)` | 1 | Cosine of x (radians). |
| `tan(x)` | 1 | Tangent of x (radians). |
| `asin(x)` | 1 | Arcsine, returns radians. |
| `acos(x)` | 1 | Arccosine, returns radians. |
| `atan(x)` | 1 | Arctangent, returns radians. |
| `abs(x)` | 1 | Absolute value. |
| `sign(x)` | 1 | -1, 0, or 1 depending on sign of x. |
| `floor(x)` | 1 | Largest integer <= x. |
| `ceil(x)` | 1 | Smallest integer >= x. |
| `sqrt(x)` | 1 | Square root. |
| `exp(x)` | 1 | e raised to x. |
| `log(x)` | 1 | Natural logarithm. |
| `log2(x)` | 1 | Base-2 logarithm. |
| `tanh(x)` | 1 | Hyperbolic tangent — common soft-clip. |
| `min(a, b)` | 2 | Minimum of a and b. |
| `max(a, b)` | 2 | Maximum of a and b. |
| `mod(a, b)` | 2 | Floating-point remainder a % b. |
| `pow(base, exp)` | 2 | Raise base to the power exp. |
| `clamp(x, lo, hi)` | 3 | Constrain x to [lo, hi]. |
| `lerp(a, b, t)` | 3 | Linear interpolation from a to b by t. |

## Logic

| Function | Args | Description |
|----------|------|-------------|
| `cond(test, ifTrue, ifFalse)` | 3 | Branchless select; like a ternary. |
| `gt(a, b)` | 2 | 1 if a > b, else 0. |
| `ge(a, b)` | 2 | 1 if a >= b, else 0. |
| `lt(a, b)` | 2 | 1 if a < b, else 0. |
| `le(a, b)` | 2 | 1 if a <= b, else 0. |
| `eq(a, b)` | 2 | 1 if a == b, else 0. |
| `ne(a, b)` | 2 | 1 if a != b, else 0. |
| `and(a, b)` | 2 | 1 if both non-zero. |
| `or(a, b)` | 2 | 1 if either non-zero. |
| `not(x)` | 1 | 1 if x is zero, else 0. |

## Drawing

| Function | Args | Description |
|----------|------|-------------|
| `rect(x, y, w, h, color)` | 5 | Stroke a rectangle outline. |
| `rectFill(x, y, w, h, color)` | 5 | Fill a rectangle. |
| `circle(x, y, r, color)` | 4 | Stroke a circle outline. |
| `circleFill(x, y, r, color)` | 4 | Fill a circle. |
| `line(x1, y1, x2, y2, thickness, color)` | 6 | Draw a line segment. |
| `text(value, x, y, size, color)` | 5 | Draw a numeric value as text. |
| `image(idx, x, y, w, h)` | 5 | Draw the host-supplied image #idx in the given rect. |

## Params

| Function | Args | Description |
|----------|------|-------------|
| `getParam(idx)` | 1 | Read a host-side parameter by index. |
| `setParam(idx, val)` | 2 | Write a host-side parameter by index. |


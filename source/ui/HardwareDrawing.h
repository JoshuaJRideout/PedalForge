#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace HardwareDrawing
{

struct CustomStyles
{
    juce::String imageMain;
    juce::String imageTrack;
    juce::String imageChassis;
    // Per-state images for stateful controls. Indexed by state 0,1,2…
    // The renderer picks the right entry based on the control's current
    // value. Empty / missing entries fall back to imageMain.
    juce::StringArray imageStates;
    juce::Colour customColour { juce::Colours::red };
    bool stretchImage = true;
    juce::String fontFamily = "Sans";
    int fontStyle = 1;
    float fontSize = 0.0f;
    float rotationRangeDeg = 270.0f;  // visual arc for knobs, in degrees
    int positions = 4;                // position count for rotary selector
};

/** Pick the image path for a given control value: imageStates[state] if
    populated for the right state index, else imageMain.
    @param value      current control value 0..1
    @param numStates  expected number of states (2 for switch/footswitch/
                      LED; N for selector). If imageStates has fewer than
                      this many entries we just use what's there.
*/
inline juce::String pickStateImagePath (const CustomStyles* custom, float value, int numStates)
{
    if (custom == nullptr) return {};
    const int n = custom->imageStates.size();
    if (n > 0 && numStates > 0)
    {
        const float v = juce::jlimit (0.0f, 1.0f, value);
        // Map [0,1] uniformly across numStates buckets. For numStates=2
        // this gives state 0 when value<0.5 and state 1 otherwise.
        int state = juce::jmin (numStates - 1, (int) (v * (float) numStates));
        if (state < n && custom->imageStates[state].isNotEmpty())
            return custom->imageStates[state];
    }
    return custom->imageMain;
}

inline void drawImageScaled (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> area, bool stretch)
{
    if (img.isNull()) return;
    juce::RectanglePlacement placement = stretch ? juce::RectanglePlacement::stretchToFit 
                                                 : (juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    g.setOpacity (1.0f);
    g.setColour (juce::Colours::white); // Reset brush colour and alpha so JUCE doesn't tint the image transparently
    g.drawImage (img, area, placement, false);
}

inline void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, float value = 0.5f, const CustomStyles* custom = nullptr)
{
    // Compute the total arc in radians from the configured degrees
    float arcRad = (custom ? custom->rotationRangeDeg : 270.0f) * (juce::MathConstants<float>::pi / 180.0f);
    float halfArc = arcRad * 0.5f;

    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull())
        {
            g.saveState();
            float angle = -halfArc + value * arcRad;
            auto transform = juce::AffineTransform::rotation (angle, area.getCentreX(), area.getCentreY());
            g.addTransform (transform);
            drawImageScaled (g, img, area, custom->stretchImage);
            g.restoreState();
            return;
        }
    }

    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.45f;

    // Outer ring
    g.setColour (juce::Colour (0xFF3A3A4A));
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    // Body
    g.setColour (juce::Colour (0xFF1E1E2E));
    g.fillEllipse (cx - r * 0.85f, cy - r * 0.85f, r * 1.7f, r * 1.7f);
    // Indicator line
    float angle = -halfArc + value * arcRad;
    float ix = cx + std::sin (angle) * r * 0.78f;
    float iy = cy - std::cos (angle) * r * 0.78f;
    float ix2 = cx + std::sin (angle) * r * 0.3f;
    float iy2 = cy - std::cos (angle) * r * 0.3f;
    g.setColour (juce::Colours::white);
    float lineW = juce::jmax (1.0f, r * 0.125f);
    g.drawLine (ix2, iy2, ix, iy, lineW);
    // Highlight
    g.setColour (juce::Colour (0x15FFFFFF));
    g.fillEllipse (cx - r * 0.5f, cy - r * 0.7f, r, r * 0.5f);
}

inline void drawSwitch (juce::Graphics& g, juce::Rectangle<float> area, float value = 0.0f, const CustomStyles* custom = nullptr)
{
    if (custom != nullptr)
    {
        const auto path = pickStateImagePath (custom, value, 2);
        if (path.isNotEmpty())
        {
            juce::Image img = juce::ImageCache::getFromFile (juce::File (path));
            if (!img.isNull())
            {
                drawImageScaled (g, img, area, custom->stretchImage);
                return;
            }
        }
    }

    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.35f;
    bool isOn = value > 0.5f;

    // Hex nut
    g.setColour (juce::Colour (0xFF5A5A6A));
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (juce::Colour (0xFF4A4A5A));
    g.fillEllipse (cx - r * 0.7f, cy - r * 0.7f, r * 1.4f, r * 1.4f);
    // Bat lever
    float leverY = isOn ? (cy - r * 1.4f) : (cy + r * 1.4f);
    g.setColour (juce::Colour (0xFFB0B0C0));
    float lineW = juce::jmax (1.0f, r * 0.15f);
    g.drawLine (cx, cy, cx, leverY, lineW);
    float batSize = juce::jmax (2.0f, r * 0.3f);
    g.fillEllipse (cx - batSize * 0.5f, leverY - batSize * 0.5f, batSize, batSize);
}

inline void drawLED (juce::Graphics& g, juce::Rectangle<float> area, float value = 1.0f, const CustomStyles* custom = nullptr)
{
    if (custom != nullptr)
    {
        const auto path = pickStateImagePath (custom, value, 2);
        if (path.isNotEmpty())
        {
            juce::Image img = juce::ImageCache::getFromFile (juce::File (path));
            if (!img.isNull())
            {
                drawImageScaled (g, img, area, custom->stretchImage);
                return;
            }
        }
    }

    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.3f;
    bool isOn = value > 0.5f;
    juce::Colour ledColor = custom ? custom->customColour : juce::Colours::red;

    // Bezel
    g.setColour (juce::Colour (0xFF3A3A4A));
    g.fillEllipse (cx - r * 1.3f, cy - r * 1.3f, r * 2.6f, r * 2.6f);

    if (isOn)
    {
        // LED glow
        g.setGradientFill (juce::ColourGradient (
            ledColor, cx, cy,
            ledColor.withAlpha(0.0f), cx, cy - r * 2.5f, true));
        g.fillEllipse (cx - r * 2, cy - r * 2, r * 4, r * 4);
        // LED body (lit)
        g.setColour (ledColor.brighter(0.2f));
    }
    else
    {
        g.setColour (ledColor.darker(0.8f));
    }
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    // Specular
    g.setColour (juce::Colour (0x66FFFFFF));
    g.fillEllipse (cx - r * 0.5f, cy - r * 0.5f, r, r);
}

/** Rotary selector — chickenhead-style pointer with one tick per position.
    `value` may be either a normalised 0..1 fraction OR a raw integer
    position index 0..(positions-1); both interpretations are accepted to
    match SelectorNode's "selection" parameter which is stored as a raw
    int (0..15). */
inline void drawSelector (juce::Graphics& g, juce::Rectangle<float> area,
                          float value = 0.0f, const CustomStyles* custom = nullptr)
{
    const int positions = juce::jlimit (2, 16, custom ? custom->positions : 4);

    // Coerce value to an integer position index. Heuristic: anything > 1.0
    // is treated as a raw index; otherwise as a 0..1 normalised fraction.
    int sel;
    if (value > 1.0f)
        sel = juce::jlimit (0, positions - 1, (int) std::floor (value + 0.5f));
    else
        sel = juce::jlimit (0, positions - 1, (int) std::floor (value * (float) positions));

    // Custom image override: pick state image if user supplied one per
    // position, else fall back to imageMain (still rotated to indicate
    // selection if it's a single image).
    if (custom != nullptr)
    {
        const auto path = pickStateImagePath (custom, value > 1.0f
                                                          ? (positions > 1 ? value / (float) (positions - 1) : 0.0f)
                                                          : value, positions);
        if (path.isNotEmpty())
        {
            juce::Image img = juce::ImageCache::getFromFile (juce::File (path));
            if (!img.isNull())
            {
                // If the user supplied per-state images we draw straight
                // (no rotation — each state is its own artwork).
                if (! custom->imageStates.isEmpty())
                {
                    drawImageScaled (g, img, area, custom->stretchImage);
                    return;
                }
                // Single image: rotate it to indicate selection.
                const float arc = (custom->rotationRangeDeg) * juce::MathConstants<float>::pi / 180.0f;
                const float halfArc = arc * 0.5f;
                const float frac = (positions > 1) ? (float) sel / (float) (positions - 1) : 0.5f;
                const float angle = -halfArc + frac * arc;
                g.saveState();
                auto t = juce::AffineTransform::rotation (angle, area.getCentreX(), area.getCentreY());
                g.addTransform (t);
                drawImageScaled (g, img, area, custom->stretchImage);
                g.restoreState();
                return;
            }
        }
    }

    // Default chickenhead-style render.
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();
    const auto r  = juce::jmin (area.getWidth(), area.getHeight()) * 0.42f;

    // Outer ring / bezel
    juce::ColourGradient ring (juce::Colour (0xFF26262E), cx, cy - r,
                               juce::Colour (0xFF0E0E14), cx, cy + r, false);
    g.setGradientFill (ring);
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);

    // Tick marks — one per position around the active arc
    const float arc = (custom ? custom->rotationRangeDeg : 270.0f) * juce::MathConstants<float>::pi / 180.0f;
    const float halfArc = arc * 0.5f;
    const float tickInner = r * 1.02f;
    const float tickOuter = r * 1.18f;
    for (int i = 0; i < positions; ++i)
    {
        const float frac = (positions > 1) ? (float) i / (float) (positions - 1) : 0.5f;
        const float a = -halfArc + frac * arc - juce::MathConstants<float>::halfPi;
        const float x0 = cx + tickInner * std::cos (a);
        const float y0 = cy + tickInner * std::sin (a);
        const float x1 = cx + tickOuter * std::cos (a);
        const float y1 = cy + tickOuter * std::sin (a);
        g.setColour (i == sel ? juce::Colour (0xFFE5E7EB)
                              : juce::Colour (0xFF4B5563));
        g.drawLine (x0, y0, x1, y1, juce::jmax (1.0f, r * 0.06f));
    }

    // Inner disc
    juce::ColourGradient disc (juce::Colour (0xFF6B7280), cx - r * 0.4f, cy - r * 0.7f,
                               juce::Colour (0xFF1F2937), cx + r * 0.4f, cy + r * 0.7f, false);
    g.setGradientFill (disc);
    g.fillEllipse (cx - r * 0.86f, cy - r * 0.86f, r * 1.72f, r * 1.72f);

    // Pointer
    const float frac = (positions > 1) ? (float) sel / (float) (positions - 1) : 0.5f;
    const float pa = -halfArc + frac * arc - juce::MathConstants<float>::halfPi;
    juce::Path pointer;
    const float baseWidth = r * 0.22f;
    const float tipLen    = r * 0.88f;
    pointer.startNewSubPath (-baseWidth * 0.5f, 0.0f);
    pointer.lineTo (baseWidth * 0.5f, 0.0f);
    pointer.lineTo (0.0f, -tipLen);
    pointer.closeSubPath();
    g.setColour (custom ? custom->customColour : juce::Colour (0xFFF59E0B));
    auto pt = juce::AffineTransform::rotation (pa).translated (cx, cy);
    g.fillPath (pointer, pt);

    // Center cap + specular
    g.setColour (juce::Colour (0xFF0A0F1A));
    g.fillEllipse (cx - r * 0.16f, cy - r * 0.16f, r * 0.32f, r * 0.32f);
    g.setColour (juce::Colour (0x33FFFFFF));
    g.fillEllipse (cx - r * 0.6f, cy - r * 0.82f, r * 1.2f, r * 0.45f);
}

inline void drawFootswitch (juce::Graphics& g, juce::Rectangle<float> area, float value = 0.0f, const CustomStyles* custom = nullptr)
{
    if (custom != nullptr)
    {
        const auto path = pickStateImagePath (custom, value, 2);
        if (path.isNotEmpty())
        {
            juce::Image img = juce::ImageCache::getFromFile (juce::File (path));
            if (!img.isNull())
            {
                drawImageScaled (g, img, area, custom->stretchImage);
                return;
            }
        }
    }

    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.4f;

    // Washer
    g.setColour (juce::Colours::lightgrey.darker());
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    // Button
    bool down = value > 0.5f;
    float pr = down ? r * 0.65f : r * 0.7f;
    g.setColour (juce::Colours::whitesmoke);
    g.fillEllipse (cx - pr, cy - pr, pr * 2, pr * 2);
    g.setColour (juce::Colours::black.withAlpha(0.3f));
    float lineW = juce::jmax (0.5f, r * 0.1f);
    g.drawEllipse (cx - pr, cy - pr, pr * 2, pr * 2, lineW);
}

inline void drawFader (juce::Graphics& g, juce::Rectangle<float> area, float value = 0.5f, const CustomStyles* custom = nullptr)
{
    // Draw track
    if (custom && custom->imageTrack.isNotEmpty())
    {
        juce::Image trackImg = juce::ImageCache::getFromFile (juce::File (custom->imageTrack));
        if (!trackImg.isNull())
            drawImageScaled (g, trackImg, area, custom->stretchImage);
    }
    else
    {
        auto b = area.reduced (area.getWidth() * 0.1f, area.getHeight() * 0.15f);
        // Track
        g.setColour (juce::Colour (0xFF2A2A3A));
        g.fillRoundedRectangle (b, b.getHeight() * 0.2f);
        // Slot
        float slotH = juce::jmax (1.0f, b.getHeight() * 0.1f);
        g.setColour (juce::Colour (0xFF0A0A14));
        g.fillRoundedRectangle (b.getX() + 4, b.getCentreY() - slotH/2, b.getWidth() - 8, slotH, slotH/2);
    }

    // Draw cap
    auto b = area.reduced (area.getWidth() * 0.1f, area.getHeight() * 0.15f);
    float capW = juce::jmax (3.0f, b.getWidth() * 0.15f);
    float capH = b.getHeight() * 0.5f;
    float travel = b.getWidth() - 8 - capW;
    float capX = b.getX() + 4 + travel * value;
    juce::Rectangle<float> capArea (capX, b.getCentreY() - capH/2, capW, capH);

    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image capImg = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!capImg.isNull())
        {
            // Center the custom image cap at the correct X value
            capArea = juce::Rectangle<float> (0, 0, capImg.getWidth(), capImg.getHeight())
                .withCentre (juce::Point<float> (capX + capW/2, b.getCentreY()));
            drawImageScaled (g, capImg, capArea, custom->stretchImage);
            return;
        }
    }

    g.setColour (juce::Colour (0xFFCCCCDD));
    float capCorner = juce::jmax(1.0f, capW * 0.2f);
    g.fillRoundedRectangle (capArea, capCorner);
}

inline void drawChassis (juce::Graphics& g, juce::Rectangle<float> area,
                         juce::Colour baseColour = juce::Colour (0xFF8A8A94), const CustomStyles* custom = nullptr)
{
    if (custom && custom->imageChassis.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageChassis));
        if (!img.isNull())
        {
            drawImageScaled (g, img, area, custom->stretchImage);
            return;
        }
    }

    float corner = juce::jmax (4.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.06f);

    // Body fill with beautiful vertical gradient (matches PedalPainter)
    auto bodyGrad = juce::ColourGradient (
        baseColour.brighter (0.18f), area.getX(), area.getY(),
        baseColour.darker (0.30f), area.getX(), area.getBottom(), false);
    g.setGradientFill (bodyGrad);
    g.fillRoundedRectangle (area, corner);

    // Premium edge bevel/highlight
    g.setColour (baseColour.brighter (0.25f).withAlpha (0.2f));
    g.drawRoundedRectangle (area, corner, 0.75f);
}

//==============================================================================
// ─── DISPLAY / GADGET DRAWING ───────────────────────────────────────────────
// Custom images replace the FRAME/BEZEL only. Functional overlays (glow,
// segments, bars, text, trace) always draw on top.
//==============================================================================

/** Helper: draw custom image as frame, or fall back to default dark bezel. */
inline bool drawFrameOrDefault (juce::Graphics& g, juce::Rectangle<float> area,
                                 const CustomStyles* custom,
                                 juce::Colour defaultBg = juce::Colour (0xFF0A0A0A),
                                 juce::Colour defaultBorder = juce::Colour (0xFF2A2A2A))
{
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull()) { drawImageScaled (g, img, area, custom->stretchImage); return true; }
    }
    float corner = juce::jmax(1.0f, area.getWidth() * 0.05f);
    g.setColour (defaultBg);
    g.fillRoundedRectangle (area, corner);
    g.setColour (defaultBorder);
    float borderW = juce::jmax(1.0f, corner * 0.3f);
    g.drawRoundedRectangle (area, corner, borderW);
    return false;
}

/**
 * 7-Segment digit helper — draws a single digit 0-9 in classic LED segment style.
 */
inline void drawSevenSegDigit (juce::Graphics& g, juce::Rectangle<float> area,
                                int digit, juce::Colour segColour)
{
    // Segment map: 0bGFEDCBA
    static constexpr uint8_t segs[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    uint8_t s = (digit >= 0 && digit <= 9) ? segs[digit] : 0;

    float x = area.getX(), y = area.getY();
    float w = area.getWidth(), h = area.getHeight();
    float t = juce::jmax (0.5f, w * 0.12f); // segment thickness
    float m = t * 0.3f; // margin

    juce::Colour off = segColour.withAlpha (0.08f);

    auto seg = [&](bool on) { return on ? segColour : off; };

    // A (top)
    g.setColour (seg (s & 0x01));
    g.fillRect (x + m + t, y + m, w - 2*m - 2*t, t);
    // B (top-right)
    g.setColour (seg (s & 0x02));
    g.fillRect (x + w - m - t, y + m + t, t, h/2 - m - t);
    // C (bottom-right)
    g.setColour (seg (s & 0x04));
    g.fillRect (x + w - m - t, y + h/2 + m, t, h/2 - m - t);
    // D (bottom)
    g.setColour (seg (s & 0x08));
    g.fillRect (x + m + t, y + h - m - t, w - 2*m - 2*t, t);
    // E (bottom-left)
    g.setColour (seg (s & 0x10));
    g.fillRect (x + m, y + h/2 + m, t, h/2 - m - t);
    // F (top-left)
    g.setColour (seg (s & 0x20));
    g.fillRect (x + m, y + m + t, t, h/2 - m - t);
    // G (middle)
    g.setColour (seg (s & 0x40));
    g.fillRect (x + m + t, y + h/2 - t/2, w - 2*m - 2*t, t);
}

/**
 * 7-Segment Display — draws multiple digits showing a number.
 */
inline void draw7Seg (juce::Graphics& g, juce::Rectangle<float> area, float value,
                      int numDigits = 3, juce::Colour segColour = juce::Colour (0xFFFF3333),
                      const CustomStyles* custom = nullptr)
{
    drawFrameOrDefault (g, area, custom);
    auto inner = area.reduced (3.0f);
    float digitW = inner.getWidth() / (float) numDigits;
    int intVal = juce::jlimit (0, (int)std::pow(10, numDigits) - 1, (int)std::abs(value));
    juce::Colour col = (custom && custom->customColour != juce::Colours::red) ? custom->customColour : segColour;
    for (int d = numDigits - 1; d >= 0; --d)
    {
        int digit = intVal % 10; intVal /= 10;
        drawSevenSegDigit (g, juce::Rectangle<float> (inner.getX() + d * digitW, inner.getY(),
                                                       digitW, inner.getHeight()).reduced (1.0f), digit, col);
    }
}

/**
 * Numeric Display — clean LCD-style numeric readout.
 */
inline void drawNumericDisplay (juce::Graphics& g, juce::Rectangle<float> area, float value,
                                const CustomStyles* custom = nullptr)
{
    drawFrameOrDefault (g, area, custom, juce::Colour (0xFF1A2A1A), juce::Colour (0xFF3A4A3A));
    float fontSize = area.getHeight() * 0.55f;
    juce::Colour textCol = (custom && custom->customColour != juce::Colours::red) ? custom->customColour : juce::Colour (0xFF33FF66);
    g.setColour (textCol);
    g.setFont (juce::FontOptions (fontSize).withStyle ("Bold"));
    g.drawText (juce::String (value, 1), area.reduced (4, 0), juce::Justification::centredRight);
}

/**
 * VU Meter — bar-graph level meter.
 */
inline void drawVUMeter (juce::Graphics& g, juce::Rectangle<float> area, float value,
                         const CustomStyles* custom = nullptr)
{
    drawFrameOrDefault (g, area, custom, juce::Colour (0xFF0E0E0E), juce::Colour (0xFF333333));

    auto inner = area.reduced (3.0f);
    float level = juce::jlimit (0.0f, 1.0f, value);
    int totalBars = juce::jmax (4, (int)(inner.getHeight() / 4.0f));
    float barH = (inner.getHeight() - (totalBars - 1)) / (float) totalBars;
    int litBars = (int)(level * totalBars);

    for (int i = 0; i < totalBars; ++i)
    {
        int fromBottom = totalBars - 1 - i;
        float barY = inner.getY() + i * (barH + 1.0f);
        bool lit = fromBottom < litBars;

        juce::Colour barCol;
        float frac = (float) fromBottom / (float) totalBars;
        if (frac > 0.85f) barCol = lit ? juce::Colour (0xFFFF3333) : juce::Colour (0xFF330A0A);
        else if (frac > 0.7f) barCol = lit ? juce::Colour (0xFFFFAA33) : juce::Colour (0xFF332200);
        else barCol = lit ? juce::Colour (0xFF33FF66) : juce::Colour (0xFF0A2A0A);

        g.setColour (barCol);
        g.fillRect (inner.getX(), barY, inner.getWidth(), barH);
    }
}

/**
 * Oscilloscope — draws a waveform trace in a screen area.
 */
inline void drawOscilloscope (juce::Graphics& g, juce::Rectangle<float> area,
                              const float* waveform, int numSamples,
                              const CustomStyles* custom = nullptr)
{
    drawFrameOrDefault (g, area, custom, juce::Colour (0xFF0A1A0A), juce::Colour (0xFF333333));

    // Grid lines
    g.setColour (juce::Colour (0xFF1A3A1A));
    for (int i = 1; i < 4; ++i)
    {
        float yy = area.getY() + area.getHeight() * i / 4.0f;
        g.drawHorizontalLine ((int) yy, area.getX() + 2, area.getRight() - 2);
    }
    for (int i = 1; i < 8; ++i)
    {
        float xx = area.getX() + area.getWidth() * i / 8.0f;
        g.drawVerticalLine ((int) xx, area.getY() + 2, area.getBottom() - 2);
    }

    // Centre line
    g.setColour (juce::Colour (0xFF2A4A2A));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX() + 2, area.getRight() - 2);

    if (!waveform || numSamples < 2) return;

    // Trace
    juce::Path trace;
    auto inner = area.reduced (2.0f);
    float midY = inner.getCentreY();
    float halfH = inner.getHeight() * 0.5f;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = inner.getX() + (float) i / (float)(numSamples - 1) * inner.getWidth();
        float y = midY - juce::jlimit (-1.0f, 1.0f, waveform[i]) * halfH;
        if (i == 0) trace.startNewSubPath (x, y);
        else trace.lineTo (x, y);
    }

    // Glow
    juce::Colour traceCol = (custom && custom->customColour != juce::Colours::red) ? custom->customColour : juce::Colour (0xFF33FF66);
    g.setColour (traceCol.withAlpha (0.15f));
    g.strokePath (trace, juce::PathStrokeType (3.0f));
    // Main trace
    g.setColour (traceCol);
    g.strokePath (trace, juce::PathStrokeType (1.5f));
}

/**
 * Indicator Light — auto-coloring green/yellow/red dot.
 */
inline void drawIndicator (juce::Graphics& g, juce::Rectangle<float> area, float value,
                           float yellowThresh = 0.6f, float redThresh = 0.85f,
                           const CustomStyles* custom = nullptr)
{
    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.35f;
    juce::Colour col;
    if (value > redThresh)      col = juce::Colour (0xFFFF3333);
    else if (value > yellowThresh) col = juce::Colour (0xFFFFAA33);
    else                         col = juce::Colour (0xFF33FF66);
    // Custom image = bezel, glow still on top
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull()) drawImageScaled (g, img, area, custom->stretchImage);
    }
    else
    {
        g.setColour (juce::Colour (0xFF3A3A4A));
        g.fillEllipse (cx - r * 1.2f, cy - r * 1.2f, r * 2.4f, r * 2.4f);
    }
    // Glow + dot always render
    g.setGradientFill (juce::ColourGradient (col, cx, cy, col.withAlpha(0.0f), cx, cy - r * 2, true));
    g.fillEllipse (cx - r * 1.8f, cy - r * 1.8f, r * 3.6f, r * 3.6f);
    g.setColour (col);
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (juce::Colour (0x55FFFFFF));
    g.fillEllipse (cx - r * 0.4f, cy - r * 0.5f, r * 0.8f, r * 0.6f);
}

/**
 * RGB LED — draws a colored LED circle using R, G, B values.
 */
inline void drawRGBLED (juce::Graphics& g, juce::Rectangle<float> area,
                        float r_val, float g_val, float b_val,
                        const CustomStyles* custom = nullptr)
{
    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.3f;
    auto col = juce::Colour::fromFloatRGBA (r_val, g_val, b_val, 1.0f);
    bool isOn = (r_val + g_val + b_val) > 0.05f;
    // Custom image = bezel
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull()) drawImageScaled (g, img, area, custom->stretchImage);
    }
    else
    {
        g.setColour (juce::Colour (0xFF3A3A4A));
        g.fillEllipse (cx - r * 1.3f, cy - r * 1.3f, r * 2.6f, r * 2.6f);
    }
    // Glow + LED always render on top
    if (isOn)
    {
        g.setGradientFill (juce::ColourGradient (col, cx, cy, col.withAlpha(0.0f), cx, cy - r * 2.5f, true));
        g.fillEllipse (cx - r * 2, cy - r * 2, r * 4, r * 4);
        g.setColour (col.brighter(0.2f));
    }
    else { g.setColour (juce::Colour (0xFF222222)); }
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (juce::Colour (0x55FFFFFF));
    g.fillEllipse (cx - r * 0.4f, cy - r * 0.5f, r * 0.8f, r * 0.6f);
}

/**
 * Text Screen — simple text display.
 */
inline void drawTextScreen (juce::Graphics& g, juce::Rectangle<float> area,
                            const juce::StringArray& lines, int highlightLine = -1,
                            const CustomStyles* custom = nullptr)
{
    drawFrameOrDefault (g, area, custom, juce::Colour (0xFF0A0A1A), juce::Colour (0xFF333344));
    auto inner = area.reduced (4.0f);
    int numLines = juce::jmax (1, lines.size());
    float lineH = inner.getHeight() / (float) numLines;
    float fontSize = lineH * 0.8f;
    if (custom && custom->fontSize > 0) fontSize = custom->fontSize;
    juce::Colour textCol = (custom && custom->customColour != juce::Colours::red) ? custom->customColour : juce::Colour (0xFF88BBFF);
    for (int i = 0; i < lines.size(); ++i)
    {
        auto lineRect = juce::Rectangle<float> (inner.getX(), inner.getY() + i * lineH, inner.getWidth(), lineH);
        if (i == highlightLine) { g.setColour (juce::Colour (0xFF334466)); g.fillRect (lineRect); }
        g.setColour (textCol);
        g.setFont (juce::FontOptions (fontSize));
        g.drawText (lines[i], lineRect.reduced(2, 0), juce::Justification::centredLeft);
    }
}

inline void drawGraphic (juce::Graphics& g, juce::Rectangle<float> area, const CustomStyles* custom)
{
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::File f (custom->imageMain);
        if (f.existsAsFile())
        {
            auto img = juce::ImageFileFormat::loadFrom (f);
            if (img.isValid())
            {
                if (custom->stretchImage)
                    g.drawImage (img, area);
                else
                    g.drawImageWithin (img, area.getX(), area.getY(), area.getWidth(), area.getHeight(), juce::RectanglePlacement::centred);
                return;
            }
        }
    }
    // Placeholder
    g.setColour (juce::Colours::grey.withAlpha(0.3f));
    g.drawRect (area, 1.0f);
    g.drawLine (area.getTopLeft().x, area.getTopLeft().y, area.getBottomRight().x, area.getBottomRight().y, 1.0f);
    g.drawLine (area.getTopRight().x, area.getTopRight().y, area.getBottomLeft().x, area.getBottomLeft().y, 1.0f);
}

/**
 * Pixel Display — draws a pixel grid.
 */
inline void drawPixelDisplay (juce::Graphics& g, juce::Rectangle<float> area,
                              const float* pixelData, int pw, int ph, bool colorMode = false,
                              const CustomStyles* custom = nullptr)
{
    drawFrameOrDefault (g, area, custom, juce::Colour (0xFF050505), juce::Colour (0xFF333333));
    if (!pixelData) return;
    auto inner = area.reduced (2.0f);
    float cellW = inner.getWidth() / (float) pw;
    float cellH = inner.getHeight() / (float) ph;
    juce::Colour pixCol = (custom && custom->customColour != juce::Colours::red) ? custom->customColour : juce::Colour (0xFF33FF66);
    for (int y = 0; y < ph; ++y)
    {
        for (int x = 0; x < pw; ++x)
        {
            float val = pixelData[y * pw + x];
            if (val < 0.01f) continue;
            float px = inner.getX() + x * cellW;
            float py = inner.getY() + y * cellH;
            if (colorMode) g.setColour (juce::Colour::fromHSV (val, 0.9f, 0.9f, 1.0f));
            else g.setColour (pixCol.withAlpha (juce::jlimit (0.0f, 1.0f, val)));
            g.fillRect (px, py, cellW - 0.5f, cellH - 0.5f);
        }
    }
}

//==============================================================================
/**
 * Text Label — A standalone text label placed on the chassis
 */
inline void drawTextLabel (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const CustomStyles* custom = nullptr)
{
    if (text.isEmpty()) return;

    juce::Colour textCol = (custom && custom->customColour != juce::Colours::red) ? custom->customColour : juce::Colour (0xFFE0E0E0);
    g.setColour (textCol);

    // Use imageMain as font style ("bold", "italic", "monospace")
    int styleFlags = juce::Font::plain;
    juce::String fontName = juce::Font::getDefaultSansSerifFontName();

    // Map fontName
    if (custom && custom->fontFamily == "Serif")           fontName = juce::Font::getDefaultSerifFontName();
    else if (custom && custom->fontFamily == "Monospace")  fontName = juce::Font::getDefaultMonospacedFontName();
    else if (custom && custom->fontFamily != "Sans" && custom->fontFamily.isNotEmpty()) fontName = custom->fontFamily;

    if (custom)
    {
        if (custom->fontStyle == 0) styleFlags = juce::Font::plain;
        else if (custom->fontStyle == 1) styleFlags = juce::Font::bold;
        else if (custom->fontStyle == 2) styleFlags = juce::Font::italic;
        else if (custom->fontStyle == 3) styleFlags = juce::Font::bold | juce::Font::italic;
    }

    // Determine size, using area height
    float fontSize = area.getHeight();
    juce::Font font (fontName, fontSize, styleFlags);
    g.setFont (font);

    // Draw the text
    g.drawText (text, area, juce::Justification::centred);
}

//==============================================================================
// Unified dispatch
//==============================================================================

inline void drawForType (juce::Graphics& g, const juce::String& type,
                         juce::Rectangle<float> area, float value = 0.5f, const CustomStyles* custom = nullptr)
{
    if (type == "knob")             drawKnob (g, area, value, custom);
    else if (type == "switch")      drawSwitch (g, area, value, custom);
    else if (type == "selector")    drawSelector (g, area, value, custom);
    else if (type == "led")         drawLED (g, area, value, custom);
    else if (type == "footswitch")  drawFootswitch (g, area, value, custom);
    else if (type == "fader")       drawFader (g, area, value, custom);
    // Display types
    else if (type == "7seg")        draw7Seg (g, area, value * 999.0f, 3, juce::Colour (0xFFFF3333), custom);
    else if (type == "display")     drawNumericDisplay (g, area, value, custom);
    else if (type == "vu_meter")    drawVUMeter (g, area, value, custom);
    else if (type == "indicator")   drawIndicator (g, area, value, 0.6f, 0.85f, custom);
    else if (type == "oscilloscope") drawOscilloscope (g, area, nullptr, 0, custom);
    else if (type == "rgb_led")     drawRGBLED (g, area, value, value * 0.5f, 1.0f - value, custom);
    else if (type == "text_screen") { juce::StringArray d {"Ready"}; drawTextScreen (g, area, d, -1, custom); }
    else if (type == "console")     { juce::StringArray d {"[log]"}; drawTextScreen (g, area, d, -1, custom); }
    else if (type == "pixel_display") drawPixelDisplay (g, area, nullptr, 32, 16, false, custom);
    else if (type == "file_loader" || type == "file_browser" || type == "plugin_browser") {
        g.setColour (juce::Colour(0xFF333333)); g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colour(0x55FFFFFF)); g.drawRoundedRectangle (area, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions(area.getHeight() * 0.4f).withStyle("Bold"));
        g.drawText("Load...", area, juce::Justification::centred);
    }
    else if (type == "library_loader") {
        g.setColour (juce::Colour(0xFF3A2A5A)); g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colour(0x88AAAAFF)); g.drawRoundedRectangle (area, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions(area.getHeight() * 0.4f).withStyle("Bold"));
        g.drawText("Library", area, juce::Justification::centred);
    }
    else if (type == "overlay_launcher") {
        g.setColour (juce::Colour(0xFF555555)); g.fillRoundedRectangle (area, 2.0f);
        g.setColour (juce::Colour(0x88FFFFFF)); g.drawRoundedRectangle (area, 2.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions(area.getHeight() * 0.4f).withStyle("Bold"));
        g.drawText("Press", area, juce::Justification::centred);
    }
    else if (type == "graphic")     drawGraphic (g, area, custom);
    else if (type == "label")       { /* In PedalPainter it draws its own label, but here we can draw it as a preview */ drawTextLabel (g, area, "LABEL", custom); }
    else { g.setColour (juce::Colours::grey); g.drawRect (area, 1.0f); }
}

} // namespace HardwareDrawing


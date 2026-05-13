#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace HardwareDrawing
{

struct CustomStyles
{
    juce::String imageMain;
    juce::String imageTrack;
    juce::String imageChassis;
    juce::Colour customColour { juce::Colours::red };
    bool stretchImage = true;
};

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
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull())
        {
            g.saveState();
            float angle = -2.35f + value * 4.7f;
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
    float angle = -2.35f + value * 4.7f;
    float ix = cx + std::sin (angle) * r * 0.78f;
    float iy = cy - std::cos (angle) * r * 0.78f;
    float ix2 = cx + std::sin (angle) * r * 0.3f;
    float iy2 = cy - std::cos (angle) * r * 0.3f;
    g.setColour (juce::Colours::white);
    g.drawLine (ix2, iy2, ix, iy, 2.5f);
    // Highlight
    g.setColour (juce::Colour (0x15FFFFFF));
    g.fillEllipse (cx - r * 0.5f, cy - r * 0.7f, r, r * 0.5f);
}

inline void drawSwitch (juce::Graphics& g, juce::Rectangle<float> area, float value = 0.0f, const CustomStyles* custom = nullptr)
{
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull())
        {
            drawImageScaled (g, img, area, custom->stretchImage);
            return;
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
    g.drawLine (cx, cy, cx, leverY, 3.0f);
    g.fillEllipse (cx - 3.0f, leverY - 3.0f, 6.0f, 6.0f);
}

inline void drawLED (juce::Graphics& g, juce::Rectangle<float> area, float value = 1.0f, const CustomStyles* custom = nullptr)
{
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull())
        {
            drawImageScaled (g, img, area, custom->stretchImage);
            return;
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

inline void drawFootswitch (juce::Graphics& g, juce::Rectangle<float> area, float value = 0.0f, const CustomStyles* custom = nullptr)
{
    if (custom && custom->imageMain.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (custom->imageMain));
        if (!img.isNull())
        {
            drawImageScaled (g, img, area, custom->stretchImage);
            return;
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
    g.drawEllipse (cx - pr, cy - pr, pr * 2, pr * 2, 2.0f);
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
        g.fillRoundedRectangle (b, 3.0f);
        // Slot
        float slotH = 3.0f;
        g.setColour (juce::Colour (0xFF0A0A14));
        g.fillRoundedRectangle (b.getX() + 4, b.getCentreY() - slotH/2, b.getWidth() - 8, slotH, 1.5f);
    }

    // Draw cap
    auto b = area.reduced (area.getWidth() * 0.1f, area.getHeight() * 0.15f);
    float capW = 10.0f, capH = b.getHeight() * 0.5f;
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
    g.fillRoundedRectangle (capArea, 3.0f);
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

    // Body fill
    g.setColour (baseColour);
    g.fillRoundedRectangle (area, 6.0f);
    // Edge bevel
    g.setColour (baseColour.darker (0.3f));
    g.drawRoundedRectangle (area, 6.0f, 2.0f);
    // Brushed texture lines
    g.setColour (juce::Colour (0x08FFFFFF));
    for (float yy = area.getY() + 3; yy < area.getBottom() - 3; yy += 4.0f)
        g.drawHorizontalLine ((int) yy, area.getX() + 4, area.getRight() - 4);
    // Screw holes
    g.setColour (baseColour.darker (0.6f));
    float sr = 4.0f, m = 10.0f;
    g.fillEllipse (area.getX()+m, area.getY()+m, sr*2, sr*2);
    g.fillEllipse (area.getRight()-m-sr*2, area.getY()+m, sr*2, sr*2);
    g.fillEllipse (area.getX()+m, area.getBottom()-m-sr*2, sr*2, sr*2);
    g.fillEllipse (area.getRight()-m-sr*2, area.getBottom()-m-sr*2, sr*2, sr*2);
}

//==============================================================================
// ─── DISPLAY / GADGET DRAWING ───────────────────────────────────────────────
//==============================================================================

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
    float t = juce::jmax (1.5f, w * 0.12f); // segment thickness
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
                      int numDigits = 3, juce::Colour segColour = juce::Colour (0xFFFF3333))
{
    // Dark bezel background
    g.setColour (juce::Colour (0xFF0A0A0A));
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (juce::Colour (0xFF2A2A2A));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    auto inner = area.reduced (3.0f);
    float digitW = inner.getWidth() / (float) numDigits;
    int intVal = juce::jlimit (0, (int)std::pow(10, numDigits) - 1, (int)std::abs(value));

    for (int d = numDigits - 1; d >= 0; --d)
    {
        int digit = intVal % 10;
        intVal /= 10;
        auto digitArea = juce::Rectangle<float> (inner.getX() + d * digitW, inner.getY(),
                                                  digitW, inner.getHeight());
        drawSevenSegDigit (g, digitArea.reduced (1.0f), digit, segColour);
    }
}

/**
 * Numeric Display — clean LCD-style numeric readout.
 */
inline void drawNumericDisplay (juce::Graphics& g, juce::Rectangle<float> area, float value,
                                const CustomStyles* = nullptr)
{
    // LCD background
    g.setColour (juce::Colour (0xFF1A2A1A));
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (juce::Colour (0xFF3A4A3A));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    float fontSize = juce::jmax (8.0f, area.getHeight() * 0.55f);
    g.setColour (juce::Colour (0xFF33FF66)); // green LCD text
    g.setFont (juce::FontOptions (fontSize).withStyle ("Bold"));
    g.drawText (juce::String (value, 1), area.reduced (4, 0), juce::Justification::centredRight);
}

/**
 * VU Meter — bar-graph level meter.
 */
inline void drawVUMeter (juce::Graphics& g, juce::Rectangle<float> area, float value,
                         const CustomStyles* = nullptr)
{
    // Background
    g.setColour (juce::Colour (0xFF0E0E0E));
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (juce::Colour (0xFF333333));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

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
                              const CustomStyles* = nullptr)
{
    // Screen background
    g.setColour (juce::Colour (0xFF0A1A0A));
    g.fillRoundedRectangle (area, 3.0f);

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
    g.setColour (juce::Colour (0xFF33FF66).withAlpha (0.15f));
    g.strokePath (trace, juce::PathStrokeType (3.0f));
    // Main trace
    g.setColour (juce::Colour (0xFF33FF66));
    g.strokePath (trace, juce::PathStrokeType (1.5f));

    // Bezel
    g.setColour (juce::Colour (0xFF333333));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);
}

/**
 * Indicator Light — auto-coloring green/yellow/red dot.
 */
inline void drawIndicator (juce::Graphics& g, juce::Rectangle<float> area, float value,
                           float yellowThresh = 0.6f, float redThresh = 0.85f)
{
    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.35f;

    juce::Colour col;
    if (value > redThresh)      col = juce::Colour (0xFFFF3333);
    else if (value > yellowThresh) col = juce::Colour (0xFFFFAA33);
    else                         col = juce::Colour (0xFF33FF66);

    // Bezel
    g.setColour (juce::Colour (0xFF3A3A4A));
    g.fillEllipse (cx - r * 1.2f, cy - r * 1.2f, r * 2.4f, r * 2.4f);
    // Glow
    g.setGradientFill (juce::ColourGradient (col, cx, cy,
                                              col.withAlpha(0.0f), cx, cy - r * 2, true));
    g.fillEllipse (cx - r * 1.8f, cy - r * 1.8f, r * 3.6f, r * 3.6f);
    // Dot
    g.setColour (col);
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    // Specular
    g.setColour (juce::Colour (0x55FFFFFF));
    g.fillEllipse (cx - r * 0.4f, cy - r * 0.5f, r * 0.8f, r * 0.6f);
}

/**
 * RGB LED — draws a colored LED circle using R, G, B values.
 */
inline void drawRGBLED (juce::Graphics& g, juce::Rectangle<float> area,
                        float r_val, float g_val, float b_val)
{
    auto cx = area.getCentreX(), cy = area.getCentreY();
    auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.3f;
    auto col = juce::Colour::fromFloatRGBA (r_val, g_val, b_val, 1.0f);
    bool isOn = (r_val + g_val + b_val) > 0.05f;

    // Bezel
    g.setColour (juce::Colour (0xFF3A3A4A));
    g.fillEllipse (cx - r * 1.3f, cy - r * 1.3f, r * 2.6f, r * 2.6f);

    if (isOn)
    {
        g.setGradientFill (juce::ColourGradient (col, cx, cy,
                                                  col.withAlpha(0.0f), cx, cy - r * 2.5f, true));
        g.fillEllipse (cx - r * 2, cy - r * 2, r * 4, r * 4);
        g.setColour (col.brighter(0.2f));
    }
    else
    {
        g.setColour (juce::Colour (0xFF222222));
    }
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (juce::Colour (0x55FFFFFF));
    g.fillEllipse (cx - r * 0.4f, cy - r * 0.5f, r * 0.8f, r * 0.6f);
}

/**
 * Text Screen — simple text display.
 */
inline void drawTextScreen (juce::Graphics& g, juce::Rectangle<float> area,
                            const juce::StringArray& lines, int highlightLine = -1)
{
    // LCD background
    g.setColour (juce::Colour (0xFF0A0A1A));
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (juce::Colour (0xFF333344));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    auto inner = area.reduced (4.0f);
    int numLines = juce::jmax (1, lines.size());
    float lineH = inner.getHeight() / (float) numLines;
    float fontSize = juce::jmax (7.0f, lineH * 0.8f);

    for (int i = 0; i < lines.size(); ++i)
    {
        auto lineRect = juce::Rectangle<float> (inner.getX(), inner.getY() + i * lineH,
                                                 inner.getWidth(), lineH);
        if (i == highlightLine)
        {
            g.setColour (juce::Colour (0xFF334466));
            g.fillRect (lineRect);
        }
        g.setColour (juce::Colour (0xFF88BBFF));
        g.setFont (juce::FontOptions (fontSize));
        g.drawText (lines[i], lineRect.reduced(2, 0), juce::Justification::centredLeft);
    }
}

/**
 * Pixel Display — draws a pixel grid.
 */
inline void drawPixelDisplay (juce::Graphics& g, juce::Rectangle<float> area,
                              const float* pixelData, int pw, int ph, bool colorMode = false)
{
    // Screen background
    g.setColour (juce::Colour (0xFF050505));
    g.fillRoundedRectangle (area, 3.0f);

    if (!pixelData) return;
    auto inner = area.reduced (2.0f);
    float cellW = inner.getWidth() / (float) pw;
    float cellH = inner.getHeight() / (float) ph;

    for (int y = 0; y < ph; ++y)
    {
        for (int x = 0; x < pw; ++x)
        {
            float val = pixelData[y * pw + x];
            if (val < 0.01f) continue;

            float px = inner.getX() + x * cellW;
            float py = inner.getY() + y * cellH;

            if (colorMode)
                g.setColour (juce::Colour::fromHSV (val, 0.9f, 0.9f, 1.0f));
            else
                g.setColour (juce::Colour (0xFF33FF66).withAlpha (juce::jlimit (0.0f, 1.0f, val)));

            g.fillRect (px, py, cellW - 0.5f, cellH - 0.5f);
        }
    }

    // Bezel
    g.setColour (juce::Colour (0xFF333333));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);
}

//==============================================================================
// Unified dispatch
//==============================================================================

inline void drawForType (juce::Graphics& g, const juce::String& type,
                         juce::Rectangle<float> area, float value = 0.5f, const CustomStyles* custom = nullptr)
{
    if (type == "knob")             drawKnob (g, area, value, custom);
    else if (type == "switch")      drawSwitch (g, area, value, custom);
    else if (type == "led")         drawLED (g, area, value, custom);
    else if (type == "footswitch")  drawFootswitch (g, area, value, custom);
    else if (type == "fader")       drawFader (g, area, value, custom);
    // Display types
    else if (type == "7seg")        draw7Seg (g, area, value * 999.0f);
    else if (type == "display")     drawNumericDisplay (g, area, value);
    else if (type == "vu_meter")    drawVUMeter (g, area, value);
    else if (type == "indicator")   drawIndicator (g, area, value);
    else if (type == "oscilloscope") drawOscilloscope (g, area, nullptr, 0);
    else if (type == "rgb_led")     drawRGBLED (g, area, value, value * 0.5f, 1.0f - value);
    else if (type == "text_screen") { juce::StringArray dummy {"Ready"}; drawTextScreen (g, area, dummy); }
    else if (type == "console")     { juce::StringArray dummy {"[log]"}; drawTextScreen (g, area, dummy); }
    else if (type == "pixel_display") drawPixelDisplay (g, area, nullptr, 32, 16);
    else { g.setColour (juce::Colours::grey); g.drawRect (area, 1.0f); }
}

} // namespace HardwareDrawing


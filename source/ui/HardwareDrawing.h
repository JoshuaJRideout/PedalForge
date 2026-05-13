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

inline void drawForType (juce::Graphics& g, const juce::String& type,
                         juce::Rectangle<float> area, float value = 0.5f, const CustomStyles* custom = nullptr)
{
    if (type == "knob")             drawKnob (g, area, value, custom);
    else if (type == "switch")      drawSwitch (g, area, value, custom);
    else if (type == "led")         drawLED (g, area, value, custom);
    else if (type == "footswitch")  drawFootswitch (g, area, value, custom);
    else if (type == "fader")       drawFader (g, area, value, custom);
    else { g.setColour (juce::Colours::grey); g.drawRect (area, 1.0f); }
}

} // namespace HardwareDrawing

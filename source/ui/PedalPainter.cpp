#include "PedalPainter.h"
#include "HardwareDrawing.h"

namespace PedalPainter
{

//==============================================================================
// Internal helpers
//==============================================================================

//==============================================================================
void paintDesign (juce::Graphics& g, juce::Rectangle<float> bounds,
                  const PedalDesign* design,
                  const std::map<juce::String, float>& controlValues,
                  bool bypassed, float alpha)
{
    if (bounds.getWidth() < 2 || bounds.getHeight() < 2)
        return;

    if (design == nullptr)
    {
        g.setColour (juce::Colour(0x20FFFFFF));
        g.drawRect (bounds, 1.0f);
        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (10.0f).withStyle("Bold"));
        g.drawText ("NO DESIGN", bounds, juce::Justification::centred);
        return;
    }

    float margin = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.04f;
    auto body = bounds.reduced (margin);

    // Scale from design coords → tile bounds
    float scaleX = body.getWidth() / design->chassisW;
    float scaleY = body.getHeight() / design->chassisH;
    float sc = juce::jmin (scaleX, scaleY);

    // Centre the chassis in bounds
    float drawW = design->chassisW * sc;
    float drawH = design->chassisH * sc;
    float offX = body.getX() + (body.getWidth() - drawW) * 0.5f;
    float offY = body.getY() + (body.getHeight() - drawH) * 0.5f;

    auto chassisRect = juce::Rectangle<float> (offX, offY, drawW, drawH);

    // Shadow
    g.setColour (juce::Colours::black.withAlpha (0.4f * alpha));
    float cornerR = juce::jmin (drawW, drawH) * 0.04f;
    g.fillRoundedRectangle (chassisRect.translated (1.5f, 2.0f), cornerR);

    // Chassis body with colour
    auto baseColour = design->chassisColour;
    auto bodyGrad = juce::ColourGradient (
        baseColour.brighter (0.18f), chassisRect.getX(), chassisRect.getY(),
        baseColour.darker (0.30f), chassisRect.getX(), chassisRect.getBottom(), false);
    g.setGradientFill (bodyGrad);
    g.setOpacity (alpha);
    g.fillRoundedRectangle (chassisRect, cornerR);

    // Custom chassis image
    if (design->chassisImage.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (design->chassisImage));
        if (!img.isNull())
        {
            g.drawImage (img, chassisRect);
        }
    }

    // Edge
    g.setColour (baseColour.brighter (0.25f).withAlpha (0.2f * alpha));
    g.drawRoundedRectangle (chassisRect, cornerR, 0.75f);

    // Brushed texture (skip if using custom image)
    if (design->chassisImage.isEmpty())
    {
        g.setColour (juce::Colour (0x08FFFFFF));
        for (float yy = chassisRect.getY() + 3; yy < chassisRect.getBottom() - 3; yy += 4.0f * sc)
            g.drawHorizontalLine ((int) yy, chassisRect.getX() + 2, chassisRect.getRight() - 2);
    }

    // Draw each control from the design
    for (const auto& ctrl : design->controls)
    {
        float cx = offX + ctrl.x * sc;
        float cy = offY + ctrl.y * sc;
        float cw = ctrl.width * sc;
        float ch = ctrl.height * sc;
        auto ctrlBounds = juce::Rectangle<float> (cx, cy, cw, ch);

        // Get value from controlValues map or use default
        float val = ctrl.defaultValue;
        auto it = controlValues.find (ctrl.controlID);
        if (it != controlValues.end()) val = it->second;

        HardwareDrawing::CustomStyles styles;
        styles.imageMain = ctrl.imageMain;
        styles.imageTrack = ctrl.imageTrack;
        styles.customColour = ctrl.customColour;
        styles.stretchImage = ctrl.stretchImage;

        if (ctrl.type == "knob")
            HardwareDrawing::drawKnob (g, ctrlBounds, val, &styles);
        else if (ctrl.type == "switch")
            HardwareDrawing::drawSwitch (g, ctrlBounds, val, &styles);
        else if (ctrl.type == "footswitch")
            HardwareDrawing::drawFootswitch (g, ctrlBounds, val, &styles);
        else if (ctrl.type == "led")
            HardwareDrawing::drawLED (g, ctrlBounds, val, &styles);
        else if (ctrl.type == "fader")
            HardwareDrawing::drawFader (g, ctrlBounds, val, &styles);
        // Display / Gadget types — custom image = frame, overlay still works
        else if (ctrl.type == "7seg")
            HardwareDrawing::draw7Seg (g, ctrlBounds, val * 999.0f, 3, juce::Colour (0xFFFF3333), &styles);
        else if (ctrl.type == "display")
            HardwareDrawing::drawNumericDisplay (g, ctrlBounds, val, &styles);
        else if (ctrl.type == "vu_meter")
            HardwareDrawing::drawVUMeter (g, ctrlBounds, val, &styles);
        else if (ctrl.type == "indicator")
            HardwareDrawing::drawIndicator (g, ctrlBounds, val, 0.6f, 0.85f, &styles);
        else if (ctrl.type == "oscilloscope")
            HardwareDrawing::drawOscilloscope (g, ctrlBounds, nullptr, 0, &styles);
        else if (ctrl.type == "rgb_led")
            HardwareDrawing::drawRGBLED (g, ctrlBounds, val, val * 0.5f, 1.0f - val, &styles);
        else if (ctrl.type == "text_screen" || ctrl.type == "console")
        {
            juce::StringArray lines { ctrl.label.isNotEmpty() ? ctrl.label : "Ready" };
            HardwareDrawing::drawTextScreen (g, ctrlBounds, lines, -1, &styles);
        }
        else if (ctrl.type == "pixel_display")
            HardwareDrawing::drawPixelDisplay (g, ctrlBounds, nullptr, 32, 16, false, &styles);
        else if (ctrl.type == "label")
            HardwareDrawing::drawTextLabel (g, ctrlBounds, ctrl.label, &styles);

        // Default label underneath control (skip for standalone text labels)
        if (ctrl.label.isNotEmpty() && sc > 0.3f && ctrl.type != "label")
        {
            float fontSize = juce::jmax (6.0f, 9.0f * sc);
            g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (0.8f * alpha));
            g.setFont (juce::FontOptions (fontSize));
            g.drawText (ctrl.label, cx, cy + ch + 1, cw, fontSize + 2,
                        juce::Justification::centredTop);
        }
    }

    // Name at bottom
    float nameFontSize = juce::jmax (6.0f, drawH * 0.06f);
    auto nameRect = juce::Rectangle<float> (
        chassisRect.getX() + drawW * 0.1f,
        chassisRect.getBottom() - drawH * 0.15f,
        drawW * 0.8f, drawH * 0.08f);
    g.setColour (juce::Colours::black.withAlpha (0.12f * alpha));
    g.fillRoundedRectangle (nameRect, 2.0f);
    g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (alpha));
    g.setFont (juce::FontOptions (nameFontSize).withStyle ("Bold"));
    g.drawText (design->name, nameRect, juce::Justification::centred, true);
}

} // namespace PedalPainter

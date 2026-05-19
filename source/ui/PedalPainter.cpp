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
                  const std::map<juce::String, juce::String>& controlTexts,
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
        styles.customColour = ctrl.customColour;
        styles.imageMain = ctrl.imageMain;
        styles.imageTrack = ctrl.imageTrack;
        styles.stretchImage = ctrl.stretchImage;
        styles.fontFamily = ctrl.fontFamily;
        styles.fontStyle = ctrl.fontStyle;
        styles.rotationRangeDeg = ctrl.rotationRange;

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
            juce::String txt = ctrl.label.isNotEmpty() ? ctrl.label : "Ready";
            auto textIt = controlTexts.find (ctrl.controlID);
            if (textIt != controlTexts.end() && textIt->second.isNotEmpty()) txt = textIt->second;
            juce::StringArray lines;
            lines.addLines (txt);
            
            int expectedLines = juce::jmax(1, ctrl.numLines);
            while (lines.size() < expectedLines)
                lines.add (""); // pad out lines
                
            styles.fontSize = ctrl.fontSize > 0 ? (ctrl.fontSize * sc) : 0.0f;
            HardwareDrawing::drawTextScreen (g, ctrlBounds, lines, -1, &styles);
        }
        else if (ctrl.type == "pixel_display")
            HardwareDrawing::drawPixelDisplay (g, ctrlBounds, nullptr, 32, 16, false, &styles);
        else if (ctrl.type == "label")
        {
            juce::String txt = ctrl.label;
            auto textIt = controlTexts.find (ctrl.controlID);
            if (textIt != controlTexts.end() && textIt->second.isNotEmpty()) txt = textIt->second;
            HardwareDrawing::drawTextLabel (g, ctrlBounds, txt, &styles);
        }
        else if (ctrl.type == "file_loader" || ctrl.type == "file_browser" || ctrl.type == "plugin_browser")
        {
            g.setColour (juce::Colour(0xFF333333).withAlpha(alpha));
            g.fillRoundedRectangle (ctrlBounds, 4.0f);
            g.setColour (juce::Colour(0x55FFFFFF).withAlpha(alpha));
            g.drawRoundedRectangle (ctrlBounds, 4.0f, 1.0f);
            
            float fontSize = 10.0f * sc;
            g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
            g.setFont (juce::FontOptions (fontSize).withStyle("Bold"));
            g.drawText (ctrl.label.isNotEmpty() ? ctrl.label : "Load...", ctrlBounds, juce::Justification::centred);
            
            continue; // Skip the default label underneath
        }
        else if (ctrl.type == "library_loader")
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.25f * alpha));
            g.fillRoundedRectangle (ctrlBounds, 4.0f);
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.8f * alpha));
            g.drawRoundedRectangle (ctrlBounds, 4.0f, 1.0f);

            float fontSize = 10.0f * sc;
            g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
            g.setFont (juce::FontOptions (fontSize).withStyle("Bold"));
            g.drawText (ctrl.label.isNotEmpty() ? ctrl.label : "Library", ctrlBounds, juce::Justification::centred);

            continue;
        }
        else if (ctrl.type == "overlay_launcher")
        {
            g.setColour (juce::Colour(0xFF555555).withAlpha(alpha));
            g.fillRoundedRectangle (ctrlBounds, 2.0f);
            g.setColour (juce::Colour(0x88FFFFFF).withAlpha(alpha));
            g.drawRoundedRectangle (ctrlBounds, 2.0f, 1.0f);
            
            float fontSize = 10.0f * sc;
            g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
            g.setFont (juce::FontOptions (fontSize).withStyle("Bold"));
            g.drawText (ctrl.label.isNotEmpty() ? ctrl.label : "Press", ctrlBounds, juce::Justification::centred);
            
            continue;
        }

        // Default label underneath control (skip for standalone text labels)
        if (ctrl.label.isNotEmpty() && ctrl.type != "label")
        {
            float fontSize = 9.0f * sc;
            g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (0.8f * alpha));
            g.setFont (juce::FontOptions (fontSize));
            float textMargin = cw * 0.5f;
            g.drawText (ctrl.label, cx - textMargin, cy + ch + 1, cw + textMargin * 2, fontSize + 2,
                        juce::Justification::centredTop);
        }
    }


}

} // namespace PedalPainter

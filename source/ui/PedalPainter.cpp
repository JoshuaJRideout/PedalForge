#include "PedalPainter.h"
#include "HardwareDrawing.h"
#include "StyleKit.h"

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
                  const std::map<juce::String, std::vector<float>>& controlData,
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

    // Chassis body with colour
    float cornerR = juce::jmax (4.0f, juce::jmin (drawW, drawH) * 0.06f);
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



    // Resolve the pedal's StyleKit + colorway once for this paint pass.
    // Phase 0: the default kit ignores the colorway, so this is inert until a
    // themed kit is registered — wiring it now means themed kits + colorways
    // apply with no further call-site changes.
    const juce::String pedalKit = design->styleKit;
    pf::Colorway colorway;
    if (design->colorwaySeed != 0)
    {
        juce::Colour seed ((juce::uint32) (juce::int64) design->colorwaySeed);
        if (design->colorwayMode == 1)
            colorway = pf::Colorway::tintFromSeed (seed);
        else
            { colorway.mode = pf::Colorway::Mode::Semantic; colorway.accent = seed; }
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
        styles.imageStates = ctrl.imageStates;
        styles.positions = ctrl.positions;
        styles.stretchImage = ctrl.stretchImage;
        styles.fontFamily = ctrl.fontFamily;
        styles.fontStyle = ctrl.fontStyle;
        styles.rotationRangeDeg = ctrl.rotationRange;

        // Core hardware + display controls route through the StyleKit engine.
        // Phase 0: the default kit is pixel-identical to the prior dispatch.
        // Text screens, loaders and labels keep their bespoke live rendering below.
        if (ctrl.type == "knob" || ctrl.type == "switch" || ctrl.type == "selector"
            || ctrl.type == "footswitch" || ctrl.type == "led" || ctrl.type == "fader"
            || ctrl.type == "xypad" || ctrl.type == "joystick"
            || ctrl.type == "7seg" || ctrl.type == "display" || ctrl.type == "vu_meter"
            || ctrl.type == "indicator" || ctrl.type == "oscilloscope" || ctrl.type == "rgb_led")
        {
            pf::ControlState st = pf::buildControlState (ctrl.controlID, val, &controlData, &controlTexts, &controlValues);
            const juce::String effKit = ctrl.style.isNotEmpty() ? ctrl.style : pedalKit;
            pf::StyleKitRegistry::draw (g, effKit, ctrl.type, ctrlBounds, st, colorway, &styles);
        }
        else if (ctrl.type == "easy_display")
        {
            // Easy Display: the node renders its menu (cursor + live values) into
            // grid text via renderMenuText(); the poller drops it in controlTexts.
            juce::String txt;
            auto textIt = controlTexts.find (ctrl.controlID);
            if (textIt != controlTexts.end()) txt = textIt->second;
            if (txt.isEmpty()) txt = "Easy Display";

            juce::StringArray lines;
            lines.addLines (txt);

            styles.fontSize = ctrl.fontSize > 0 ? (ctrl.fontSize * sc) : 0.0f;
            HardwareDrawing::drawTextScreen (g, ctrlBounds, lines, -1, &styles);
            continue; // menu text is drawn inside the screen — no label below
        }
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
            continue; // Text is already shown inside the screen — no label below
        }
        else if (ctrl.type == "pixel_display")
        {
            const float* pxData = nullptr;
            auto dataIt = controlData.find (ctrl.controlID);
            if (dataIt != controlData.end() && !dataIt->second.empty())
                pxData = dataIt->second.data();
            // Default 32x16 if not specified
            HardwareDrawing::drawPixelDisplay (g, ctrlBounds, pxData, 32, 16, false, &styles);
        }
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

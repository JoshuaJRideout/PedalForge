#include "PedalPainter.h"
#include "HardwareDrawing.h"

namespace PedalPainter
{

//==============================================================================
// Internal helpers
//==============================================================================

namespace
{
    /** Draw a single knob at the given centre/radius. */
    void drawKnob (juce::Graphics& g, float cx, float cy, float radius,
                   juce::Colour baseColour, float normValue, float alpha)
    {
        auto knobBounds = juce::Rectangle<float> (cx - radius, cy - radius,
                                                   radius * 2, radius * 2);

        // Knob shadow
        g.setColour (juce::Colours::black.withAlpha (0.35f * alpha));
        g.fillEllipse (knobBounds.translated (0.5f, 0.5f));

        // Knob body — metallic gradient
        auto knobGrad = juce::ColourGradient (
            juce::Colour (0xFF555555).brighter (0.2f), cx - radius, cy - radius,
            juce::Colour (0xFF222222),                 cx + radius, cy + radius,
            false);
        g.setGradientFill (knobGrad);
        g.setOpacity (alpha);
        g.fillEllipse (knobBounds);

        // Knob rim highlight
        g.setColour (juce::Colour (0xFF888888).withAlpha (0.3f * alpha));
        g.drawEllipse (knobBounds.reduced (0.5f), 0.5f);

        // Indicator line
        float angle = juce::MathConstants<float>::pi * 0.75f +
                      normValue * juce::MathConstants<float>::pi * 1.5f;
        float lineR = radius * 0.65f;
        float dotR  = juce::jmax (1.0f, radius * 0.12f);
        float dx = std::cos (angle);
        float dy = std::sin (angle);
        float indX = cx + dx * lineR;
        float indY = cy + dy * lineR;

        g.setColour (baseColour.brighter (0.6f).withAlpha (0.9f * alpha));
        g.fillEllipse (indX - dotR, indY - dotR, dotR * 2, dotR * 2);
    }

    /** Draw the stomp switch at the bottom. */
    void drawStompSwitch (juce::Graphics& g, float cx, float cy, float radius,
                          bool active, float alpha)
    {
        auto switchBounds = juce::Rectangle<float> (cx - radius, cy - radius,
                                                     radius * 2, radius * 2);

        // Shadow
        g.setColour (juce::Colours::black.withAlpha (0.3f * alpha));
        g.fillEllipse (switchBounds.translated (0.5f, 1.0f));

        // Switch body — chrome/brushed metal look
        auto switchGrad = juce::ColourGradient (
            juce::Colour (0xFF707070), cx, cy - radius,
            juce::Colour (0xFF3A3A3A), cx, cy + radius,
            false);
        g.setGradientFill (switchGrad);
        g.setOpacity (alpha);
        g.fillEllipse (switchBounds);

        // Ring
        g.setColour (juce::Colour (0xFF999999).withAlpha (0.25f * alpha));
        g.drawEllipse (switchBounds.reduced (1.0f), 1.0f);

        // Inner circle with slight press/active state
        float innerR = radius * 0.6f;
        auto innerBounds = juce::Rectangle<float> (cx - innerR, cy - innerR,
                                                    innerR * 2, innerR * 2);
        g.setColour (juce::Colour (active ? 0xFF505050 : 0xFF404040).withAlpha (alpha));
        g.fillEllipse (innerBounds);
    }

    /** Draw a 1/4" jack on the side. */
    void drawJack (juce::Graphics& g, float cx, float cy, float radius, float alpha)
    {
        // Outer ring
        g.setColour (juce::Colour (0xFF444444).withAlpha (alpha));
        g.fillEllipse (cx - radius, cy - radius, radius * 2, radius * 2);
        // Inner hole
        float holeR = radius * 0.55f;
        g.setColour (juce::Colour (0xFF1A1A1A).withAlpha (alpha));
        g.fillEllipse (cx - holeR, cy - holeR, holeR * 2, holeR * 2);
    }
} // anonymous namespace

//==============================================================================
void paint (juce::Graphics& g, juce::Rectangle<float> bounds,
            const PedalVisual& visual, float alpha)
{
    if (bounds.getWidth() < 2 || bounds.getHeight() < 2)
        return;

    auto baseColour = visual.colour.isTransparent()
                          ? PedalForgeLookAndFeel::bgLight
                          : visual.colour;

    float w = bounds.getWidth();
    float h = bounds.getHeight();
    float margin = juce::jmin (w, h) * 0.04f;

    // The enclosure body, inset slightly
    auto body = bounds.reduced (margin);
    float cornerR = juce::jmin (body.getWidth(), body.getHeight()) * 0.08f;

    //==========================================================================
    // Drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.4f * alpha));
    g.fillRoundedRectangle (body.translated (1.5f, 2.0f), cornerR);

    //==========================================================================
    // Enclosure body — top-down gradient
    auto bodyGrad = juce::ColourGradient (
        baseColour.brighter (0.18f), body.getX(), body.getY(),
        baseColour.darker (0.30f),   body.getX(), body.getBottom(),
        false);
    g.setGradientFill (bodyGrad);
    g.setOpacity (alpha);
    g.fillRoundedRectangle (body, cornerR);

    // Top edge bevel highlight
    auto topEdge = body.removeFromTop (juce::jmax (1.5f, body.getHeight() * 0.015f));
    g.setColour (juce::Colours::white.withAlpha (0.08f * alpha));
    g.fillRoundedRectangle (juce::Rectangle<float> (body.getX(), bounds.getY() + margin,
                                                     body.getWidth(), topEdge.getHeight()),
                            cornerR);

    // Restore body
    body = bounds.reduced (margin);

    // Bottom edge shadow
    g.setColour (juce::Colours::black.withAlpha (0.1f * alpha));
    g.fillRoundedRectangle (
        juce::Rectangle<float> (body.getX(), body.getBottom() - 2.0f,
                                 body.getWidth(), 2.0f),
        cornerR * 0.5f);

    // Border
    g.setColour (baseColour.brighter (0.25f).withAlpha (0.2f * alpha));
    g.drawRoundedRectangle (body, cornerR, 0.75f);

    //==========================================================================
    // Layout zones (proportional):
    //   Top 8%:     jack area
    //   8%-20%:     LED
    //   20%-50%:    knobs
    //   50%-65%:    name plate
    //   65%-90%:    stomp switch
    //   90%-100%:   bottom jacks
    //==========================================================================

    float bodyX = body.getX();
    float bodyY = body.getY();
    float bodyW = body.getWidth();
    float bodyH = body.getHeight();
    float centreX = body.getCentreX();

    //── LED ──
    float ledY = bodyY + bodyH * 0.13f;
    float ledSize = juce::jmin (bodyW * 0.08f, bodyH * 0.04f);
    ledSize = juce::jmax (ledSize, 2.5f);

    if (! visual.bypassed)
    {
        // LED glow
        float glowR = ledSize * 2.2f;
        g.setColour (PedalForgeLookAndFeel::success.withAlpha (0.2f * alpha));
        g.fillEllipse (centreX - glowR, ledY - glowR, glowR * 2, glowR * 2);
    }

    g.setColour ((visual.bypassed
                      ? PedalForgeLookAndFeel::danger.withAlpha (0.5f)
                      : PedalForgeLookAndFeel::success).withAlpha (alpha));
    g.fillEllipse (centreX - ledSize, ledY - ledSize, ledSize * 2, ledSize * 2);

    // LED bezel
    g.setColour (juce::Colour (0xFF333333).withAlpha (0.4f * alpha));
    g.drawEllipse (centreX - ledSize, ledY - ledSize, ledSize * 2, ledSize * 2, 0.5f);

    //── Knobs ──
    float knobZoneTop = bodyY + bodyH * 0.22f;
    float knobZoneBot = bodyY + bodyH * 0.52f;
    float knobZoneMid = (knobZoneTop + knobZoneBot) * 0.5f;

    int numKnobs = juce::jlimit (1, 6, visual.numKnobs);
    float maxKnobR = juce::jmin (bodyW * 0.14f, (knobZoneBot - knobZoneTop) * 0.35f);
    maxKnobR = juce::jmax (maxKnobR, 3.0f);

    if (numKnobs <= 3)
    {
        // Single row of knobs
        float spacing = bodyW / (float) (numKnobs + 1);
        for (int i = 0; i < numKnobs; ++i)
        {
            float kx = bodyX + spacing * (float) (i + 1);
            float normVal = (i < (int) visual.knobValues.size())
                                ? visual.knobValues[(size_t) i] : 0.5f;
            drawKnob (g, kx, knobZoneMid, maxKnobR, baseColour, normVal, alpha);
        }
    }
    else
    {
        // Two rows: top row gets ceil(n/2), bottom row gets floor(n/2)
        int topRow = (numKnobs + 1) / 2;
        int botRow = numKnobs - topRow;

        float topY = knobZoneTop + (knobZoneBot - knobZoneTop) * 0.3f;
        float botY = knobZoneTop + (knobZoneBot - knobZoneTop) * 0.75f;
        float smallR = maxKnobR * 0.85f;

        float topSpacing = bodyW / (float) (topRow + 1);
        for (int i = 0; i < topRow; ++i)
        {
            float kx = bodyX + topSpacing * (float) (i + 1);
            float normVal = (i < (int) visual.knobValues.size())
                                ? visual.knobValues[(size_t) i] : 0.5f;
            drawKnob (g, kx, topY, smallR, baseColour, normVal, alpha);
        }
        float botSpacing = bodyW / (float) (botRow + 1);
        for (int i = 0; i < botRow; ++i)
        {
            float kx = bodyX + botSpacing * (float) (i + 1);
            int idx = topRow + i;
            float normVal = (idx < (int) visual.knobValues.size())
                                ? visual.knobValues[(size_t) idx] : 0.5f;
            drawKnob (g, kx, botY, smallR, baseColour, normVal, alpha);
        }
    }

    //── Name plate ──
    float namePlateTop = bodyY + bodyH * 0.54f;
    float namePlateH   = bodyH * 0.12f;
    auto nameRect = juce::Rectangle<float> (bodyX + bodyW * 0.1f, namePlateTop,
                                             bodyW * 0.8f, namePlateH);

    // Subtle recessed plate background
    g.setColour (juce::Colours::black.withAlpha (0.12f * alpha));
    g.fillRoundedRectangle (nameRect, cornerR * 0.5f);

    // Name text
    float nameFontSize = juce::jmin (namePlateH * 0.55f, bodyW * 0.13f);
    nameFontSize = juce::jmax (nameFontSize, 5.0f);
    g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (alpha));
    g.setFont (juce::FontOptions (nameFontSize).withStyle ("Bold"));
    g.drawText (visual.name, nameRect, juce::Justification::centred, true);

    // Category (smaller, below name)
    float catFontSize = juce::jmax (4.0f, nameFontSize * 0.7f);
    float catY = namePlateTop + namePlateH;
    auto catRect = juce::Rectangle<float> (bodyX + bodyW * 0.1f, catY,
                                            bodyW * 0.8f, namePlateH * 0.7f);
    g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (0.4f * alpha));
    g.setFont (juce::FontOptions (catFontSize));
    g.drawText (visual.category, catRect, juce::Justification::centred, true);

    //── Stomp switch ──
    float stompY = bodyY + bodyH * 0.80f;
    float stompR = juce::jmin (bodyW * 0.15f, bodyH * 0.08f);
    stompR = juce::jmax (stompR, 3.5f);
    drawStompSwitch (g, centreX, stompY, stompR, ! visual.bypassed, alpha);

    //── Side jacks (input/output) ──
    float jackR = juce::jmin (bodyW * 0.06f, bodyH * 0.03f);
    jackR = juce::jmax (jackR, 2.0f);
    float jackY = body.getCentreY();

    drawJack (g, bodyX + jackR + 1.0f, jackY, jackR, alpha);
    drawJack (g, bodyX + bodyW - jackR - 1.0f, jackY, jackR, alpha);
}

//==============================================================================
void paintDesign (juce::Graphics& g, juce::Rectangle<float> bounds,
                  const PedalDesign& design,
                  const std::map<juce::String, float>& controlValues,
                  bool bypassed, float alpha)
{
    if (bounds.getWidth() < 2 || bounds.getHeight() < 2)
        return;

    float margin = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.04f;
    auto body = bounds.reduced (margin);

    // Scale from design coords → tile bounds
    float scaleX = body.getWidth() / design.chassisW;
    float scaleY = body.getHeight() / design.chassisH;
    float sc = juce::jmin (scaleX, scaleY);

    // Centre the chassis in bounds
    float drawW = design.chassisW * sc;
    float drawH = design.chassisH * sc;
    float offX = body.getX() + (body.getWidth() - drawW) * 0.5f;
    float offY = body.getY() + (body.getHeight() - drawH) * 0.5f;

    auto chassisRect = juce::Rectangle<float> (offX, offY, drawW, drawH);

    // Shadow
    g.setColour (juce::Colours::black.withAlpha (0.4f * alpha));
    float cornerR = juce::jmin (drawW, drawH) * 0.04f;
    g.fillRoundedRectangle (chassisRect.translated (1.5f, 2.0f), cornerR);

    // Chassis body with colour
    auto baseColour = design.chassisColour;
    auto bodyGrad = juce::ColourGradient (
        baseColour.brighter (0.18f), chassisRect.getX(), chassisRect.getY(),
        baseColour.darker (0.30f), chassisRect.getX(), chassisRect.getBottom(), false);
    g.setGradientFill (bodyGrad);
    g.setOpacity (alpha);
    g.fillRoundedRectangle (chassisRect, cornerR);

    // Custom chassis image
    if (design.chassisImage.isNotEmpty())
    {
        juce::Image img = juce::ImageCache::getFromFile (juce::File (design.chassisImage));
        if (!img.isNull())
        {
            g.drawImage (img, chassisRect);
        }
    }

    // Edge
    g.setColour (baseColour.brighter (0.25f).withAlpha (0.2f * alpha));
    g.drawRoundedRectangle (chassisRect, cornerR, 0.75f);

    // Brushed texture (skip if using custom image)
    if (design.chassisImage.isEmpty())
    {
        g.setColour (juce::Colour (0x08FFFFFF));
        for (float yy = chassisRect.getY() + 3; yy < chassisRect.getBottom() - 3; yy += 4.0f * sc)
            g.drawHorizontalLine ((int) yy, chassisRect.getX() + 2, chassisRect.getRight() - 2);
    }

    // Draw each control from the design
    for (const auto& ctrl : design.controls)
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

        // Label
        if (ctrl.label.isNotEmpty() && sc > 0.3f)
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
    g.drawText (design.name, nameRect, juce::Justification::centred, true);
}

} // namespace PedalPainter

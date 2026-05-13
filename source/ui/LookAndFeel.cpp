#include "LookAndFeel.h"

//==============================================================================
PedalForgeLookAndFeel::PedalForgeLookAndFeel()
{
    // Set default colours
    setColour (juce::ResizableWindow::backgroundColourId, bgDark);
    setColour (juce::PopupMenu::backgroundColourId,       bgMid);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::textColourId,             textPrimary);
    setColour (juce::PopupMenu::highlightedTextColourId,  textPrimary);
    setColour (juce::ComboBox::backgroundColourId,        bgLight);
    setColour (juce::ComboBox::outlineColourId,           gridLine);
    setColour (juce::ComboBox::textColourId,              textPrimary);
    setColour (juce::ComboBox::arrowColourId,             textSecondary);
    setColour (juce::TextButton::buttonColourId,          bgLight);
    setColour (juce::TextButton::textColourOffId,         textPrimary);
    setColour (juce::Label::textColourId,                 textPrimary);
}

//==============================================================================
void PedalForgeLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                               int x, int y, int width, int height,
                                               float sliderPos,
                                               float rotaryStartAngle,
                                               float rotaryEndAngle,
                                               juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;

    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Outer ring (track)
    g.setColour (bgLight);
    g.fillEllipse (rx, ry, rw, rw);

    // Value arc
    juce::Path valueArc;
    valueArc.addCentredArc (centreX, centreY, radius - 2.0f, radius - 2.0f,
                             0.0f, rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // Knob body
    auto knobRadius = radius * 0.7f;
    auto knobGradient = juce::ColourGradient (
        bgMid.brighter (0.15f), centreX, centreY - knobRadius,
        bgMid.darker (0.2f),    centreX, centreY + knobRadius, false);
    g.setGradientFill (knobGradient);
    g.fillEllipse (centreX - knobRadius, centreY - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f);

    // Knob outline
    g.setColour (gridLine);
    g.drawEllipse (centreX - knobRadius, centreY - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    // Pointer line
    juce::Path pointer;
    auto pointerLength = knobRadius * 0.7f;
    auto pointerThickness = 2.5f;
    pointer.addRectangle (-pointerThickness * 0.5f, -knobRadius + 4.0f,
                          pointerThickness, pointerLength);
    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                .translated (centreX, centreY));
    g.setColour (accent);
    g.fillPath (pointer);
}

//==============================================================================
void PedalForgeLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                   juce::Button& button,
                                                   const juce::Colour& backgroundColour,
                                                   bool shouldDrawButtonAsHighlighted,
                                                   bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto cornerRadius = 6.0f;

    auto baseColour = backgroundColour;
    if (shouldDrawButtonAsDown)
        baseColour = baseColour.darker (0.3f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter (0.1f);

    g.setColour (baseColour);
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (gridLine);
    g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
}

//==============================================================================
void PedalForgeLookAndFeel::drawComboBox (juce::Graphics& g,
                                           int width, int height,
                                           bool /*isButtonDown*/,
                                           int /*buttonX*/, int /*buttonY*/,
                                           int /*buttonW*/, int /*buttonH*/,
                                           juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    auto cornerRadius = 6.0f;

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerRadius, 1.0f);

    // Arrow
    auto arrowZone = juce::Rectangle<float> (width - 24.0f, 0.0f, 20.0f, (float) height);
    juce::Path arrow;
    arrow.addTriangle (arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getCentreX(),         arrowZone.getCentreY() + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

//==============================================================================
void PedalForgeLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                                const juce::Rectangle<int>& area,
                                                bool isSeparator, bool isActive,
                                                bool isHighlighted, bool isTicked,
                                                bool /*hasSubMenu*/,
                                                const juce::String& text,
                                                const juce::String& /*shortcutKeyText*/,
                                                const juce::Drawable* /*icon*/,
                                                const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        auto r = area.reduced (5, 0).toFloat();
        r = r.withY (r.getCentreY()).withHeight (1.0f);
        g.setColour (gridLine);
        g.fillRect (r);
        return;
    }

    auto r = area.reduced (1);

    if (isHighlighted && isActive)
    {
        g.setColour (accent.withAlpha (0.2f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }

    g.setColour (isActive ? textPrimary : textMuted);
    g.setFont (getPopupMenuFont());

    auto textArea = r.reduced (12, 0);
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

    if (isTicked)
    {
        g.setColour (accent);
        auto tick = r.removeFromRight (r.getHeight()).reduced (6);
        g.fillEllipse (tick.toFloat());
    }
}

//==============================================================================
juce::Font PedalForgeLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (14.0f);
}

juce::Font PedalForgeLookAndFeel::getPopupMenuFont()
{
    return juce::Font (14.0f);
}

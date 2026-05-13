#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * Custom LookAndFeel for PedalForge.
 * Dark theme with modern aesthetics — rounded controls, subtle gradients,
 * and a workshop/pedalboard atmosphere.
 */
class PedalForgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalForgeLookAndFeel();

    //==========================================================================
    // Colour palette
    static inline const juce::Colour bgDark        { 0xFF0F0F14 };
    static inline const juce::Colour bgMid          { 0xFF1A1A24 };
    static inline const juce::Colour bgLight        { 0xFF252535 };
    static inline const juce::Colour gridLine        { 0xFF2A2A3A };
    static inline const juce::Colour accent          { 0xFF6366F1 }; // Indigo
    static inline const juce::Colour accentBright    { 0xFF818CF8 };
    static inline const juce::Colour textPrimary     { 0xFFE2E8F0 };
    static inline const juce::Colour textSecondary   { 0xFF94A3B8 };
    static inline const juce::Colour textMuted       { 0xFF64748B };
    static inline const juce::Colour danger          { 0xFFEF4444 };
    static inline const juce::Colour success         { 0xFF22C55E };
    static inline const juce::Colour cableMono       { 0xFFCBD5E1 };
    static inline const juce::Colour cableStereo     { 0xFF60A5FA };

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g, int width, int height,
                       bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalForgeLookAndFeel)
};

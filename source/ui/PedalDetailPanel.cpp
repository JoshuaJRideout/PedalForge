#include "PedalDetailPanel.h"
#include "PedalPainter.h"
#include "LookAndFeel.h"

//==============================================================================
PedalDetailPanel::PedalDetailPanel()
{
    bypassButton.addListener (this);
    removeButton.addListener (this);
    closeButton.addListener (this);

    addAndMakeVisible (bypassButton);
    addAndMakeVisible (removeButton);
    addAndMakeVisible (closeButton);
}

PedalDetailPanel::~PedalDetailPanel()
{
    for (auto& entry : knobEntries)
        entry.knob->removeListener (this);
}

//==============================================================================
void PedalDetailPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);

    // Left border
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    if (selectedInstance == nullptr)
    {
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Select a pedal\nto edit", getLocalBounds(),
                    juce::Justification::centred);
        return;
    }

    //==========================================================================
    // Draw the pedal visual (large version, same as grid/palette)
    //==========================================================================
    auto bounds = getLocalBounds();
    auto pedalArea = bounds.reduced (12, 0);

    // Reserve bottom for buttons
    pedalArea.removeFromBottom (80);
    // Reserve top for close button
    pedalArea.removeFromTop (8);

    // The pedal visual should be centred and maintain a portrait aspect ratio
    float desiredRatio = 0.55f; // w/h (typical pedal proportion)
    float availW = (float) pedalArea.getWidth();
    float availH = (float) pedalArea.getHeight();

    float pedalW, pedalH;
    if (availW / availH > desiredRatio)
    {
        // Constrained by height
        pedalH = availH;
        pedalW = pedalH * desiredRatio;
    }
    else
    {
        // Constrained by width
        pedalW = availW;
        pedalH = pedalW / desiredRatio;
    }

    auto pedalRect = juce::Rectangle<float> (
        pedalArea.getCentreX() - pedalW * 0.5f,
        pedalArea.getY() + (availH - pedalH) * 0.5f,
        pedalW, pedalH);

    // Build the visual
    PedalPainter::PedalVisual visual;
    visual.name      = selectedInstance->name;
    visual.category  = selectedInstance->category;
    visual.colour    = selectedInstance->colour;
    visual.bypassed  = selectedInstance->bypassed;
    visual.numKnobs  = selectedInstance->numKnobs;

    // Get live knob values
    if (engineRef != nullptr)
    {
        if (auto* node = engineRef->getGraph().getNodeForId (selectedInstance->nodeID))
        {
            auto* proc = node->getProcessor();
            for (auto* param : proc->getParameters())
            {
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                    visual.knobValues.push_back (rp->getValue());
            }
        }
    }

    PedalPainter::paint (g, pedalRect, visual, 1.0f);

    //==========================================================================
    // Draw parameter labels below the pedal
    //==========================================================================
    if (engineRef != nullptr && ! knobEntries.empty())
    {
        float bodyY  = pedalRect.getY() + pedalRect.getHeight() * 0.04f;
        float bodyH  = pedalRect.getHeight() * 0.92f;
        float bodyX  = pedalRect.getX() + pedalRect.getWidth() * 0.04f;
        float bodyW  = pedalRect.getWidth() * 0.92f;

        float knobZoneTop = bodyY + bodyH * 0.22f;
        float knobZoneBot = bodyY + bodyH * 0.52f;

        int numKnobs = (int) knobEntries.size();

        auto* node = engineRef->getGraph().getNodeForId (selectedInstance->nodeID);
        if (node != nullptr)
        {
            auto* proc = node->getProcessor();

            if (numKnobs <= 3)
            {
                float knobMidY = (knobZoneTop + knobZoneBot) * 0.5f;
                float spacing = bodyW / (float) (numKnobs + 1);

                for (int i = 0; i < numKnobs; ++i)
                {
                    float kx = bodyX + spacing * (float) (i + 1);
                    float maxKnobR = juce::jmin (bodyW * 0.14f,
                                                  (knobZoneBot - knobZoneTop) * 0.35f);

                    // Parameter name above the knob
                    float labelW = spacing * 0.9f;
                    auto labelRect = juce::Rectangle<float> (
                        kx - labelW * 0.5f, knobMidY - maxKnobR - 14.0f,
                        labelW, 12.0f);
                    g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (0.5f));
                    g.setFont (juce::FontOptions (juce::jmax (7.0f, pedalW * 0.04f)));
                    g.drawText (knobEntries[(size_t) i].paramName, labelRect,
                                juce::Justification::centred, true);

                    // Value below the knob
                    for (auto* param : proc->getParameters())
                    {
                        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                        {
                            if (rp->getParameterID() == knobEntries[(size_t) i].paramId)
                            {
                                float val = rp->convertFrom0to1 (rp->getValue());
                                auto valRect = juce::Rectangle<float> (
                                    kx - labelW * 0.5f, knobMidY + maxKnobR + 2.0f,
                                    labelW, 12.0f);
                                g.setColour (PedalForgeLookAndFeel::textMuted);
                                g.setFont (juce::FontOptions (juce::jmax (7.0f, pedalW * 0.035f)));
                                g.drawText (juce::String (val, 1), valRect,
                                            juce::Justification::centred, true);
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                // Two rows of knobs — show labels similarly
                int topRow = (numKnobs + 1) / 2;
                int botRow = numKnobs - topRow;
                float topY = knobZoneTop + (knobZoneBot - knobZoneTop) * 0.3f;
                float botY = knobZoneTop + (knobZoneBot - knobZoneTop) * 0.75f;
                float smallR = juce::jmin (bodyW * 0.14f,
                                            (knobZoneBot - knobZoneTop) * 0.35f) * 0.85f;

                auto drawLabelsForRow = [&] (int startIdx, int count, float rowY, float rowSpacing)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        int idx = startIdx + i;
                        if (idx >= numKnobs) break;
                        float kx = bodyX + rowSpacing * (float) (i + 1);
                        float labelW = rowSpacing * 0.9f;

                        // Name
                        auto labelRect = juce::Rectangle<float> (
                            kx - labelW * 0.5f, rowY - smallR - 12.0f,
                            labelW, 11.0f);
                        g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (0.5f));
                        g.setFont (juce::FontOptions (juce::jmax (6.0f, pedalW * 0.035f)));
                        g.drawText (knobEntries[(size_t) idx].paramName, labelRect,
                                    juce::Justification::centred, true);

                        // Value
                        for (auto* param : proc->getParameters())
                        {
                            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                            {
                                if (rp->getParameterID() == knobEntries[(size_t) idx].paramId)
                                {
                                    float val = rp->convertFrom0to1 (rp->getValue());
                                    auto valRect = juce::Rectangle<float> (
                                        kx - labelW * 0.5f, rowY + smallR + 1.0f,
                                        labelW, 11.0f);
                                    g.setColour (PedalForgeLookAndFeel::textMuted);
                                    g.setFont (juce::FontOptions (juce::jmax (6.0f, pedalW * 0.03f)));
                                    g.drawText (juce::String (val, 1), valRect,
                                                juce::Justification::centred, true);
                                    break;
                                }
                            }
                        }
                    }
                };

                float topSpacing = bodyW / (float) (topRow + 1);
                float botSpacing = bodyW / (float) (botRow + 1);
                drawLabelsForRow (0, topRow, topY, topSpacing);
                drawLabelsForRow (topRow, botRow, botY, botSpacing);
            }
        }
    }
}

//==============================================================================
void PedalDetailPanel::resized()
{
    auto bounds = getLocalBounds().reduced (1, 0);

    // Close button (top right)
    closeButton.setBounds (bounds.getRight() - 32, 8, 24, 24);

    // Bypass + Remove buttons at bottom
    auto bottom = bounds.removeFromBottom (80).reduced (12, 8);
    bypassButton.setBounds (bottom.removeFromTop (32));
    bottom.removeFromTop (6);
    removeButton.setBounds (bottom.removeFromTop (28));

    //==========================================================================
    // Position invisible rotary sliders over the painted knob locations
    //==========================================================================
    if (selectedInstance != nullptr && ! knobEntries.empty())
    {
        auto pedalArea = bounds.reduced (12, 0);
        pedalArea.removeFromTop (8);

        float desiredRatio = 0.55f;
        float availW = (float) pedalArea.getWidth();
        float availH = (float) pedalArea.getHeight();

        float pedalW, pedalH;
        if (availW / availH > desiredRatio)
        {
            pedalH = availH;
            pedalW = pedalH * desiredRatio;
        }
        else
        {
            pedalW = availW;
            pedalH = pedalW / desiredRatio;
        }

        float pedalX = pedalArea.getCentreX() - pedalW * 0.5f;
        float pedalY = pedalArea.getY() + (availH - pedalH) * 0.5f;

        // Body dimensions (matching PedalPainter's layout)
        float margin = juce::jmin (pedalW, pedalH) * 0.04f;
        float bodyX = pedalX + margin;
        float bodyY = pedalY + margin;
        float bodyW = pedalW - margin * 2;
        float bodyH = pedalH - margin * 2;

        float knobZoneTop = bodyY + bodyH * 0.22f;
        float knobZoneBot = bodyY + bodyH * 0.52f;

        int numKnobs = (int) knobEntries.size();
        float maxKnobR = juce::jmin (bodyW * 0.14f, (knobZoneBot - knobZoneTop) * 0.35f);

        if (numKnobs <= 3)
        {
            float knobMidY = (knobZoneTop + knobZoneBot) * 0.5f;
            float spacing = bodyW / (float) (numKnobs + 1);

            for (int i = 0; i < numKnobs; ++i)
            {
                float kx = bodyX + spacing * (float) (i + 1);
                float hitR = maxKnobR * 1.4f; // Slightly larger hit area
                knobEntries[(size_t) i].knob->setBounds (
                    (int) (kx - hitR), (int) (knobMidY - hitR),
                    (int) (hitR * 2), (int) (hitR * 2));
            }
        }
        else
        {
            int topRow = (numKnobs + 1) / 2;
            int botRow = numKnobs - topRow;
            float topY = knobZoneTop + (knobZoneBot - knobZoneTop) * 0.3f;
            float botY = knobZoneTop + (knobZoneBot - knobZoneTop) * 0.75f;
            float smallR = maxKnobR * 0.85f;
            float hitR = smallR * 1.4f;

            float topSpacing = bodyW / (float) (topRow + 1);
            for (int i = 0; i < topRow; ++i)
            {
                float kx = bodyX + topSpacing * (float) (i + 1);
                knobEntries[(size_t) i].knob->setBounds (
                    (int) (kx - hitR), (int) (topY - hitR),
                    (int) (hitR * 2), (int) (hitR * 2));
            }
            float botSpacing = bodyW / (float) (botRow + 1);
            for (int i = 0; i < botRow; ++i)
            {
                int idx = topRow + i;
                float kx = bodyX + botSpacing * (float) (i + 1);
                knobEntries[(size_t) idx].knob->setBounds (
                    (int) (kx - hitR), (int) (botY - hitR),
                    (int) (hitR * 2), (int) (hitR * 2));
            }
        }
    }
}

//==============================================================================
void PedalDetailPanel::showPedal (PedalInstance& instance,
                                    AudioGraphEngine& engine)
{
    selectedInstance = &instance;
    engineRef = &engine;

    // Update bypass button
    bypassButton.setButtonText (instance.bypassed ? "ENABLE" : "BYPASS");
    bypassButton.setColour (juce::TextButton::buttonColourId,
                            instance.bypassed
                                ? PedalForgeLookAndFeel::success.darker (0.3f)
                                : PedalForgeLookAndFeel::danger.darker (0.3f));

    rebuildKnobs();
    resized();
    repaint();
}

void PedalDetailPanel::clearSelection()
{
    selectedInstance = nullptr;
    engineRef = nullptr;
    knobEntries.clear();
    repaint();
}

//==============================================================================
void PedalDetailPanel::rebuildKnobs()
{
    // Clean up old knobs
    for (auto& entry : knobEntries)
        entry.knob->removeListener (this);
    knobEntries.clear();

    if (selectedInstance == nullptr || engineRef == nullptr) return;

    auto* node = engineRef->getGraph().getNodeForId (selectedInstance->nodeID);
    if (node == nullptr) return;

    auto* proc = node->getProcessor();
    for (auto* param : proc->getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            KnobEntry entry;

            // Invisible rotary slider — the painted knob is drawn by PedalPainter
            entry.knob = std::make_unique<juce::Slider>();
            entry.knob->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            entry.knob->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            entry.knob->setRange (rangedParam->getNormalisableRange().start,
                                   rangedParam->getNormalisableRange().end,
                                   rangedParam->getNormalisableRange().interval);
            entry.knob->setValue (rangedParam->convertFrom0to1 (rangedParam->getValue()),
                                  juce::dontSendNotification);
            entry.knob->addListener (this);
            entry.knob->setName (rangedParam->getParameterID());

            // Make the slider nearly invisible — PedalPainter renders the knob,
            // but we need a small alpha so mouse events are intercepted.
            entry.knob->setAlpha (0.01f);
            entry.knob->setInterceptsMouseClicks (true, true);
            addAndMakeVisible (*entry.knob);

            entry.paramId = rangedParam->getParameterID();
            entry.paramName = rangedParam->getName (30);
            knobEntries.push_back (std::move (entry));
        }
    }
}

//==============================================================================
void PedalDetailPanel::sliderValueChanged (juce::Slider* slider)
{
    if (selectedInstance == nullptr || engineRef == nullptr) return;

    auto* node = engineRef->getGraph().getNodeForId (selectedInstance->nodeID);
    if (node == nullptr) return;

    auto* proc = node->getProcessor();
    for (auto* param : proc->getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (rp->getParameterID() == slider->getName())
            {
                rp->setValueNotifyingHost (
                    rp->getNormalisableRange().convertTo0to1 (
                        static_cast<float> (slider->getValue())));
                break;
            }
        }
    }

    // Repaint to update painted knob indicator + value text
    repaint();
}

void PedalDetailPanel::buttonClicked (juce::Button* button)
{
    if (button == &bypassButton && selectedInstance != nullptr)
    {
        selectedInstance->bypassed = ! selectedInstance->bypassed;
        bypassButton.setButtonText (selectedInstance->bypassed ? "ENABLE" : "BYPASS");
        bypassButton.setColour (juce::TextButton::buttonColourId,
                                selectedInstance->bypassed
                                    ? PedalForgeLookAndFeel::success.darker (0.3f)
                                    : PedalForgeLookAndFeel::danger.darker (0.3f));
        repaint();
    }
    else if (button == &removeButton && selectedInstance != nullptr)
    {
        auto nodeId = selectedInstance->nodeID;
        clearSelection();
        listeners.call ([nodeId] (Listener& l) { l.pedalRemoved (nodeId); });
    }
    else if (button == &closeButton)
    {
        clearSelection();
    }
}

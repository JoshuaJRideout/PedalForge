#include "PedalDetailPanel.h"
#include "PedalPainter.h"
#include "LookAndFeel.h"
#include "../dsp/GraphPedalProcessor.h"

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
    // Reserve top for close button and title
    pedalArea.removeFromTop (40);

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

    PedalPainter::paintDesign (g, pedalRect, selectedInstance->design.get(), selectedInstance->controlValues, selectedInstance->controlTexts, selectedInstance->bypassed, 1.0f);

    // Draw the title of the pedal at the top
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (16.0f).withStyle("Bold"));
    g.drawText (selectedInstance->name, 12, 10, getWidth() - 48, 24, juce::Justification::centredLeft);
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
    // Position invisible rotary sliders over the designed hardware locations
    //==========================================================================
    if (selectedInstance != nullptr && selectedInstance->design != nullptr && (!knobEntries.empty() || !fileLoaders.empty()))
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

        auto pedalRect = juce::Rectangle<float> (
            pedalArea.getCentreX() - pedalW * 0.5f,
            pedalArea.getY() + (availH - pedalH) * 0.5f,
            pedalW, pedalH);
            
        float margin = juce::jmin (pedalRect.getWidth(), pedalRect.getHeight()) * 0.04f;
        auto body = pedalRect.reduced (margin);
        
        float scaleX = body.getWidth() / selectedInstance->design->chassisW;
        float scaleY = body.getHeight() / selectedInstance->design->chassisH;
        float sc = juce::jmin (scaleX, scaleY);

        float drawW = selectedInstance->design->chassisW * sc;
        float drawH = selectedInstance->design->chassisH * sc;
        float offX = body.getX() + (body.getWidth() - drawW) * 0.5f;
        float offY = body.getY() + (body.getHeight() - drawH) * 0.5f;

        for (auto& entry : knobEntries)
        {
            // Find if this paramId is mapped to any control
            juce::String mappedControlID;
            for (const auto& mapping : selectedInstance->design->mappings)
            {
                if (mapping.nodeParam == entry.paramId)
                {
                    mappedControlID = mapping.controlID;
                    break;
                }
            }
            
            for (const auto& ctrl : selectedInstance->design->controls)
            {
                if (ctrl.controlID == mappedControlID)
                {
                    // Calculate bounds matching HardwareDrawing
                    float scaledX = offX + ctrl.x * sc;
                    float scaledY = offY + ctrl.y * sc;
                    float scaledW = ctrl.width * sc;
                    float scaledH = ctrl.height * sc;
                    
                    // We expand the bounds slightly for a better hit area
                    float hitMargin = juce::jmin (scaledW, scaledH) * 0.2f;
                    entry.knob->setBounds (
                        (int)(scaledX - hitMargin), 
                        (int)(scaledY - hitMargin), 
                        (int)(scaledW + hitMargin * 2), 
                        (int)(scaledH + hitMargin * 2)
                    );
                    break;
                }
            }
        }

        // Layout file loader buttons
        for (auto& fe : fileLoaders)
        {
            for (const auto& ctrl : selectedInstance->design->controls)
            {
                if (ctrl.controlID == fe.controlID)
                {
                    float scaledX = offX + ctrl.x * sc;
                    float scaledY = offY + ctrl.y * sc;
                    float scaledW = ctrl.width * sc;
                    float scaledH = ctrl.height * sc;
                    
                    fe.button->setBounds(
                        (int)scaledX,
                        (int)scaledY,
                        (int)scaledW,
                        (int)scaledH
                    );
                    break;
                }
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
    fileLoaders.clear();

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

            // Look up sensitivity from the mapped control in the design
            float sensitivity = 200.0f; // default
            if (selectedInstance->design != nullptr)
            {
                for (const auto& mapping : selectedInstance->design->mappings)
                {
                    if (mapping.nodeParam == rangedParam->getParameterID())
                    {
                        for (const auto& ctrl : selectedInstance->design->controls)
                        {
                            if (ctrl.controlID == mapping.controlID && ctrl.type == "knob")
                            {
                                sensitivity = ctrl.sensitivity;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            // Use velocity-based dragging so the knob responds to the configured
            // pixel distance rather than scaling with the parameter's numeric range.
            entry.knob->setVelocityBasedMode (true);
            entry.knob->setVelocityModeParameters (1.0, 1, 0.0, false,
                                                    juce::ModifierKeys::noModifiers);
            // setMouseDragSensitivity controls how many pixels = full range
            entry.knob->setMouseDragSensitivity ((int) sensitivity);

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

    if (selectedInstance->design != nullptr)
    {
        for (const auto& ctrl : selectedInstance->design->controls)
        {
            if (ctrl.type == "file_loader" || ctrl.type == "file_browser"
                || ctrl.type == "library_loader")
            {
                // Find what node this is mapped to
                juce::String nodeParamStr;
                for (const auto& mapping : selectedInstance->design->mappings)
                {
                    if (mapping.controlID == ctrl.controlID)
                    {
                        nodeParamStr = mapping.nodeParam;
                        break;
                    }
                }
                
                int targetNodeID = -1;
                if (nodeParamStr.isNotEmpty())
                    targetNodeID = nodeParamStr.upToFirstOccurrenceOf("_", false, false).getIntValue();

                if (targetNodeID >= 0)
                {
                    FileLoaderEntry fe;
                    fe.controlID = ctrl.controlID;
                    fe.targetNodeID = targetNodeID;
                    // Empty label — PedalPainter handles the visual rendering
                    fe.button = std::make_unique<juce::TextButton>("");
                    fe.button->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
                    fe.button->setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
                    fe.button->setAlpha(0.01f);
                    
                    if (ctrl.type == "library_loader")
                    {
                        // Library loader — trigger onOpenLibrary callback
                        fe.button->onClick = [this, targetNodeID]() {
                            if (onOpenLibrary)
                                onOpenLibrary("NAM", targetNodeID);
                        };
                    }
                    else
                    {
                        // Standard file loader — open OS file picker
                        auto safeEngineRef = engineRef;
                        auto safeNodeID = selectedInstance->nodeID;
                        fe.button->onClick = [this, safeEngineRef, safeNodeID, targetNodeID]() {
                            fileChooser = std::make_unique<juce::FileChooser> ("Select File", juce::File{}, "*.nam;*.wav;*.mp3;*.aif;*.flac");
                            auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                            fileChooser->launchAsync (chooserFlags, [this, safeEngineRef, safeNodeID, targetNodeID](const juce::FileChooser& fc) {
                                if (fc.getResult().existsAsFile()) {
                                    if (auto* node = safeEngineRef->getGraph().getNodeForId(safeNodeID)) {
                                        if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(node->getProcessor())) {
                                            graphProc->setNodeFilePath(targetNodeID, fc.getResult().getFullPathName());
                                        }
                                    }
                                }
                            });
                        };
                    }
                    
                    addAndMakeVisible(*fe.button);
                    fileLoaders.push_back(std::move(fe));
                }
            }
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

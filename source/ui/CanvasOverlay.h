#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"
#include "../midi/MidiLearn.h"
#include "HardwareDrawing.h"
#include "PedalPainter.h"
#include "../dsp/PluginHostNode.h"

class CanvasOverlay : public juce::Component, public juce::Timer
{
public:
    CanvasOverlay()
    {
        setVisible(false);
    }

    ~CanvasOverlay() override
    {
        stopTimer();
    }

    void showForPage(PedalInstance* inst, AudioGraphEngine* eng, MidiLearnManager* mlm, const juce::String& pageName)
    {
        // Clean up old editor if any
        if (activePluginEditor)
        {
            removeChildComponent(activePluginEditor.get());
            activePluginEditor.reset();
        }

        targetInstance = inst;
        engine = eng;
        midiLearn = mlm;
        if (!targetInstance || !targetInstance->design || !engine || !midiLearn) return;

        targetPage = nullptr;
        for (const auto& page : targetInstance->design->canvasPages)
        {
            if (page.pageName == pageName)
            {
                targetPage = &page;
                break;
            }
        }

        if (!targetPage) return;

        // Populate initial text/values just in case
        for (const auto& ctrl : targetPage->controls)
        {
            if (targetInstance->controlValues.find(ctrl.controlID) == targetInstance->controlValues.end())
                targetInstance->controlValues[ctrl.controlID] = ctrl.defaultValue;
            if (targetInstance->controlTexts.find(ctrl.controlID) == targetInstance->controlTexts.end())
                targetInstance->controlTexts[ctrl.controlID] = ctrl.label;

            // Handle plugin_editor components
            if (ctrl.type == "plugin_editor")
            {
                // We need to find the PluginHostNode to get its editor
                auto* node = engine->getGraph().getNodeForId(targetInstance->nodeID);
                if (node)
                {
                    if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(node->getProcessor()))
                    {
                        // Look for a PluginHostNode in the internal DSPGraph
                        // Control mapping might point to a specific node, but we can also just search for the first PluginHostNode
                        juce::String mappedNodeIDStr;
                        for (const auto& m : targetInstance->design->mappings)
                        {
                            if (m.controlID == ctrl.controlID)
                            {
                                mappedNodeIDStr = m.nodeParam.upToFirstOccurrenceOf("_", false, false);
                                break;
                            }
                        }

                        if (mappedNodeIDStr.isNotEmpty())
                        {
                            int mappedNodeID = mappedNodeIDStr.getIntValue();
                            if (auto* targetDSPNode = graphProc->getDSPGraph().getNode(mappedNodeID))
                            {
                                if (auto* hostNode = dynamic_cast<PluginHostNode*>(targetDSPNode))
                                {
                                    if (auto* pluginInst = hostNode->getPluginInstance())
                                    {
                                        if (pluginInst->hasEditor())
                                        {
                                            activePluginEditor.reset(pluginInst->createEditorIfNeeded());
                                            if (activePluginEditor)
                                            {
                                                addAndMakeVisible(activePluginEditor.get());
                                                // resized() will position it
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        resized(); // Force layout update for activePluginEditor
        setVisible(true);
        grabKeyboardFocus();
        startTimerHz(30);
    }

    void hide()
    {
        if (activePluginEditor)
        {
            removeChildComponent(activePluginEditor.get());
            activePluginEditor.reset();
        }
        setVisible(false);
        stopTimer();
        targetInstance = nullptr;
        targetPage = nullptr;
    }

    void timerCallback() override
    {
        if (!isVisible() || !targetInstance || !targetPage) return;

        auto* node = engine->getGraph().getNodeForId(targetInstance->nodeID);
        if (!node) return;
        auto* proc = node->getProcessor();
        if (!proc) return;

        bool needsRepaint = false;
        auto params = proc->getParameters();

        for (const auto& mapping : targetInstance->design->mappings)
        {
            // Only update controls that actually exist on this canvas page
            bool isOnPage = false;
            for (const auto& ctrl : targetPage->controls) {
                if (ctrl.controlID == mapping.controlID || mapping.controlID.startsWith(ctrl.controlID + ":")) {
                    isOnPage = true;
                    break;
                }
            }
            if (!isOnPage) continue;

            if (mapping.nodeParam.endsWith("_filepath"))
            {
                int targetNodeID = mapping.nodeParam.upToFirstOccurrenceOf("_", false, false).getIntValue();
                if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(proc))
                {
                    if (auto* targetNode = graphProc->getDSPGraph().getNode(targetNodeID))
                    {
                        juce::String path = targetNode->getFilePath();
                        juce::String text = "Empty";
                        if (path.isNotEmpty())
                            text = juce::File(path).getFileNameWithoutExtension();

                        juce::String baseControlID = mapping.controlID;
                        int lineIndex = -1;
                        if (baseControlID.containsChar(':'))
                        {
                            lineIndex = baseControlID.fromLastOccurrenceOf(":", false, false).getIntValue();
                            baseControlID = baseControlID.upToFirstOccurrenceOf(":", false, false);
                        }

                        if (lineIndex >= 0)
                        {
                            juce::StringArray lines;
                            lines.addLines(targetInstance->controlTexts[baseControlID]);
                            while (lines.size() <= lineIndex) lines.add("");
                            if (lines[lineIndex] != text)
                            {
                                lines.set(lineIndex, text);
                                targetInstance->controlTexts[baseControlID] = lines.joinIntoString("\n");
                                needsRepaint = true;
                            }
                        }
                        else
                        {
                            if (targetInstance->controlTexts[baseControlID] != text)
                            {
                                targetInstance->controlTexts[baseControlID] = text;
                                needsRepaint = true;
                            }
                        }
                    }
                }
            }
            else
            {
                for (auto* p : params)
                {
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                    {
                        if (ranged->getParameterID() == mapping.nodeParam)
                        {
                            float val = ranged->convertFrom0to1(ranged->getValue());
                            juce::String text = ranged->getText(ranged->getValue(), 32);

                            juce::String baseControlID = mapping.controlID;
                            int lineIndex = -1;
                            if (baseControlID.containsChar(':'))
                            {
                                lineIndex = baseControlID.fromLastOccurrenceOf(":", false, false).getIntValue();
                                baseControlID = baseControlID.upToFirstOccurrenceOf(":", false, false);
                            }

                            bool textChanged = false;
                            if (lineIndex >= 0)
                            {
                                juce::StringArray lines;
                                lines.addLines(targetInstance->controlTexts[baseControlID]);
                                while (lines.size() <= lineIndex) lines.add("");
                                if (lines[lineIndex] != text)
                                {
                                    lines.set(lineIndex, text);
                                    targetInstance->controlTexts[baseControlID] = lines.joinIntoString("\n");
                                    textChanged = true;
                                }
                            }
                            else
                            {
                                if (targetInstance->controlTexts[baseControlID] != text)
                                {
                                    targetInstance->controlTexts[baseControlID] = text;
                                    textChanged = true;
                                }
                            }

                            if (targetInstance->controlValues[baseControlID] != val || textChanged)
                            {
                                targetInstance->controlValues[baseControlID] = val;
                                needsRepaint = true;
                            }
                            break;
                        }
                    }
                }
            }
        }

        if (needsRepaint) repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Darkened backdrop
        g.fillAll(juce::Colours::black.withAlpha(0.7f));

        if (!targetInstance || !targetPage) return;

        auto r = getLocalBounds().reduced(getWidth() / 10, getHeight() / 10);
        
        // Scale to fit the page bounds inside our reduced area while maintaining aspect ratio
        float scaleX = r.getWidth() / targetPage->width;
        float scaleY = r.getHeight() / targetPage->height;
        float sc = juce::jmin(scaleX, scaleY);
        
        float drawW = targetPage->width * sc;
        float drawH = targetPage->height * sc;
        
        float offX = r.getX() + (r.getWidth() - drawW) * 0.5f;
        float offY = r.getY() + (r.getHeight() - drawH) * 0.5f;

        // Draw Canvas Background
        if (!targetPage->backgroundColour.isTransparent())
        {
            g.setColour(targetPage->backgroundColour);
            g.fillRoundedRectangle(offX, offY, drawW, drawH, 8.0f);
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawRoundedRectangle(offX, offY, drawW, drawH, 8.0f, 2.0f);
        }

        // Draw controls
        for (const auto& ctrl : targetPage->controls)
        {
            float val = targetInstance->controlValues[ctrl.controlID];
            juce::Rectangle<float> ctrlBounds(offX + ctrl.x * sc, offY + ctrl.y * sc, ctrl.width * sc, ctrl.height * sc);

            HardwareDrawing::CustomStyles styles;
            styles.customColour = ctrl.customColour;
            styles.imageMain = ctrl.imageMain;
            styles.imageTrack = ctrl.imageTrack;
            styles.stretchImage = ctrl.stretchImage;
            styles.fontFamily = ctrl.fontFamily;
            styles.fontStyle = ctrl.fontStyle;
            styles.fontSize = ctrl.fontSize > 0 ? (ctrl.fontSize * sc) : 0.0f;
            styles.rotationRangeDeg = ctrl.rotationRange;

            if (ctrl.type == "knob" || ctrl.type == "slider")
                HardwareDrawing::drawKnob(g, ctrlBounds, val, &styles);
            else if (ctrl.type == "fader")
                HardwareDrawing::drawFader(g, ctrlBounds, val, &styles);
            else if (ctrl.type == "switch" || ctrl.type == "footswitch" || ctrl.type == "led_toggle")
            {
                if (ctrl.type == "led_toggle")
                    HardwareDrawing::drawRGBLED(g, ctrlBounds, val > 0.5f ? 1.0f : 0.0f, val > 0.5f ? 1.0f : 0.0f, 0.0f, &styles);
                else if (ctrl.type == "footswitch")
                    HardwareDrawing::drawFootswitch(g, ctrlBounds, val > 0.5f, &styles);
                else
                    HardwareDrawing::drawSwitch(g, ctrlBounds, val > 0.5f, &styles);
            }
            else if (ctrl.type == "button" || ctrl.type == "file_loader" || ctrl.type == "library_loader" || ctrl.type == "overlay_launcher" || ctrl.type == "plugin_browser")
            {
                juce::String txt = ctrl.label;
                auto textIt = targetInstance->controlTexts.find(ctrl.controlID);
                if (textIt != targetInstance->controlTexts.end() && textIt->second.isNotEmpty()) txt = textIt->second;
                
                g.setColour(juce::Colours::grey);
                g.fillRoundedRectangle(ctrlBounds, 4.0f);
                g.setColour(juce::Colours::white);
                g.drawText(txt, ctrlBounds, juce::Justification::centred, true);
            }
            else if (ctrl.type == "rgb_led")
                HardwareDrawing::drawRGBLED(g, ctrlBounds, val, val * 0.5f, 1.0f - val, &styles);
            else if (ctrl.type == "text_screen" || ctrl.type == "console")
            {
                juce::String txt = ctrl.label.isNotEmpty() ? ctrl.label : "Ready";
                auto textIt = targetInstance->controlTexts.find(ctrl.controlID);
                if (textIt != targetInstance->controlTexts.end() && textIt->second.isNotEmpty()) txt = textIt->second;
                juce::StringArray lines;
                lines.addLines(txt);
                int expectedLines = juce::jmax(1, ctrl.numLines);
                while (lines.size() < expectedLines) lines.add("");
                HardwareDrawing::drawTextScreen(g, ctrlBounds, lines, -1, &styles);
            }
            else if (ctrl.type == "label")
            {
                juce::String txt = ctrl.label;
                auto textIt = targetInstance->controlTexts.find(ctrl.controlID);
                if (textIt != targetInstance->controlTexts.end() && textIt->second.isNotEmpty()) txt = textIt->second;
                HardwareDrawing::drawTextLabel(g, ctrlBounds, txt, &styles);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!targetInstance || !targetPage)
        {
            hide();
            return;
        }

        auto r = getLocalBounds().reduced(getWidth() / 10, getHeight() / 10);
        float scaleX = r.getWidth() / targetPage->width;
        float scaleY = r.getHeight() / targetPage->height;
        float sc = juce::jmin(scaleX, scaleY);
        float drawW = targetPage->width * sc;
        float drawH = targetPage->height * sc;
        float offX = r.getX() + (r.getWidth() - drawW) * 0.5f;
        float offY = r.getY() + (r.getHeight() - drawH) * 0.5f;

        juce::Rectangle<float> canvasBounds(offX, offY, drawW, drawH);
        if (!canvasBounds.contains(e.position))
        {
            hide();
            return;
        }

        float mx = (e.x - offX) / sc;
        float my = (e.y - offY) / sc;

        for (const auto& ctrl : targetPage->controls)
        {
            if (mx >= ctrl.x && mx <= ctrl.x + ctrl.width &&
                my >= ctrl.y && my <= ctrl.y + ctrl.height)
            {
                if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
                {
                    // Basic MIDI Learn hook
                    return;
                }

                if (ctrl.type == "switch" || ctrl.type == "footswitch" || ctrl.type == "led_toggle")
                {
                    float currentVal = targetInstance->controlValues[ctrl.controlID];
                    float newVal = currentVal > 0.5f ? 0.0f : 1.0f;
                    
                    if (auto* node = engine->getGraph().getNodeForId(targetInstance->nodeID))
                    {
                        auto* proc = node->getProcessor();
                        juce::String mappedParamID;
                        for (const auto& m : targetInstance->design->mappings)
                            if (m.controlID == ctrl.controlID) { mappedParamID = m.nodeParam; break; }

                        if (mappedParamID.isNotEmpty() && proc)
                        {
                            for (auto* p : proc->getParameters())
                            {
                                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                                {
                                    if (ranged->getParameterID() == mappedParamID)
                                    {
                                        ranged->setValueNotifyingHost(newVal);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    targetInstance->controlValues[ctrl.controlID] = newVal;
                    repaint();
                    return;
                }
                else if (ctrl.type == "knob" || ctrl.type == "slider" || ctrl.type == "fader")
                {
                    draggedKnobID = ctrl.controlID;
                    draggedKnobStartValue = targetInstance->controlValues[ctrl.controlID];
                    return;
                }
                // File loader and library loader omitted for brevity, but could be added
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggedKnobID.isNotEmpty() && targetInstance && targetPage)
        {
            float dragSensitivity = 200.0f;
            for (const auto& ctrl : targetPage->controls)
                if (ctrl.controlID == draggedKnobID) dragSensitivity = ctrl.sensitivity;

            float delta = (e.getDistanceFromDragStartY() - e.getDistanceFromDragStartX()) / dragSensitivity;
            float newVal = juce::jlimit(0.0f, 1.0f, draggedKnobStartValue - delta);
            
            targetInstance->controlValues[draggedKnobID] = newVal;
            
            if (auto* node = engine->getGraph().getNodeForId(targetInstance->nodeID))
            {
                auto* proc = node->getProcessor();
                juce::String mappedParamID;
                for (const auto& m : targetInstance->design->mappings)
                    if (m.controlID == draggedKnobID) { mappedParamID = m.nodeParam; break; }

                if (mappedParamID.isNotEmpty() && proc)
                {
                    for (auto* p : proc->getParameters())
                    {
                        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                        {
                            if (ranged->getParameterID() == mappedParamID)
                            {
                                ranged->setValueNotifyingHost(newVal);
                                break;
                            }
                        }
                    }
                }
            }
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        draggedKnobID = juce::String();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            hide();
            return true;
        }
        return false;
    }

    void resized() override
    {
        if (!targetInstance || !targetPage) return;

        auto r = getLocalBounds().reduced(getWidth() / 10, getHeight() / 10);
        float scaleX = r.getWidth() / targetPage->width;
        float scaleY = r.getHeight() / targetPage->height;
        float sc = juce::jmin(scaleX, scaleY);
        float drawW = targetPage->width * sc;
        float drawH = targetPage->height * sc;
        float offX = r.getX() + (r.getWidth() - drawW) * 0.5f;
        float offY = r.getY() + (r.getHeight() - drawH) * 0.5f;

        if (activePluginEditor)
        {
            for (const auto& ctrl : targetPage->controls)
            {
                if (ctrl.type == "plugin_editor")
                {
                    juce::Rectangle<int> bounds;
                    if (activePluginEditor->isResizable())
                    {
                        bounds = juce::Rectangle<int>(
                            (int)(offX + ctrl.x * sc), 
                            (int)(offY + ctrl.y * sc), 
                            (int)(ctrl.width * sc), 
                            (int)(ctrl.height * sc)
                        );
                    }
                    else
                    {
                        int cx = (int)(offX + ctrl.x * sc + (ctrl.width * sc - activePluginEditor->getWidth()) * 0.5f);
                        int cy = (int)(offY + ctrl.y * sc + (ctrl.height * sc - activePluginEditor->getHeight()) * 0.5f);
                        bounds = juce::Rectangle<int>(
                            cx, cy, 
                            activePluginEditor->getWidth(), 
                            activePluginEditor->getHeight()
                        );
                    }

                    if (activePluginEditor->getBounds() != bounds)
                        activePluginEditor->setBounds(bounds);
                    
                    break;
                }
            }
        }
    }

private:
    AudioGraphEngine* engine = nullptr;
    MidiLearnManager* midiLearn = nullptr;

    PedalInstance* targetInstance = nullptr;
    const PedalDesign::CanvasPage* targetPage = nullptr;

    juce::String draggedKnobID;
    float draggedKnobStartValue = 0.0f;
    
    std::unique_ptr<juce::AudioProcessorEditor> activePluginEditor;
};

#include "PlayTabComponent.h"
#include "LookAndFeel.h"
#include "ToastOverlay.h"
#include "../util/AppPaths.h"
#include "../pedals/PedalRegistry.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
class PlayTabComponent::PlaySlotWrapper : public juce::Component, public juce::DragAndDropTarget
{
public:
    PlaySlotWrapper (PlayTabComponent& owner, int idx, const juce::String& cat, const juce::String& lbl)
        : parent (owner), index (idx), category (cat), label (lbl)
    {
    }
    
    void setPedal (std::unique_ptr<PedalComponent> comp, const juce::String& name)
    {
        pedalComponent = std::move (comp);
        nameLabel = std::make_unique<juce::Label>();
        nameLabel->setText (name, juce::dontSendNotification);
        nameLabel->setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
        nameLabel->setJustificationType (juce::Justification::centred);
        nameLabel->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
        nameLabel->setInterceptsMouseClicks (false, false);

        addAndMakeVisible (pedalComponent.get());
        addAndMakeVisible (nameLabel.get());

        pedalComponent->addMouseListener (this, true); // Catch drags from pedal
    }

    PedalComponent* getPedalComponent() const { return pedalComponent.get(); }
    
    void resized() override
    {
        if (pedalComponent != nullptr)
        {
            nameLabel->setBounds (0, 0, getWidth(), 24);
            pedalComponent->setBounds (0, 30, getWidth(), getHeight() - 30);
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (pedalComponent != nullptr)
        {
            if (isDragHovering)
            {
                g.setColour (PedalForgeLookAndFeel::accent.withAlpha(0.3f));
                g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
            }
            return;
        }

        auto bounds = getLocalBounds().toFloat().reduced (5.0f);
        
        g.setColour (PedalForgeLookAndFeel::bgDark.brighter (0.1f));
        g.fillRoundedRectangle (bounds, 10.0f);
        
        juce::Path border;
        border.addRoundedRectangle (bounds, 10.0f);
        juce::PathStrokeType stroke (2.0f);
        float dashes[] = { 5.0f, 5.0f };
        stroke.createDashedStroke (border, border, dashes, 2);
        
        g.setColour (isDragHovering ? PedalForgeLookAndFeel::textPrimary : PedalForgeLookAndFeel::textSecondary.withAlpha(0.5f));
        g.strokePath (border, juce::PathStrokeType(2.0f));
        
        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (18.0f).withStyle("Bold"));
        g.drawText ("+ Add " + label, bounds, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool isPopup = e.mods.isRightButtonDown() || e.mods.isCtrlDown();

        if (isPopup)
        {
            juce::PopupMenu menu;
            if (pedalComponent != nullptr)
            {
                // Slot has a pedal — offer Remove (keep slot) and Delete Slot (remove both).
                menu.addItem (1, "Remove Pedal (keep slot)");
                menu.addItem (2, "Delete Slot");
            }
            else
            {
                // Empty slot — just Delete Slot.
                menu.addItem (2, "Delete Slot");
            }

            juce::Component::SafePointer<PlayTabComponent> sp (&parent);
            int idx = index;
            menu.showMenuAsync (juce::PopupMenu::Options(),
                                [sp, idx] (int result)
                                {
                                    if (sp == nullptr) return;
                                    if      (result == 1) sp->clearSlot  (idx);
                                    else if (result == 2) sp->removeSlot (idx);
                                });
            return;
        }

        // Left-click on an empty slot opens the inventory; left-click on a
        // populated slot lets the underlying PedalComponent handle it.
        if (pedalComponent == nullptr)
            parent.handleSlotClicked (index);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pedalComponent != nullptr)
        {
            if (pedalComponent->isDraggingKnob()) return;

            if (!e.source.hasMouseMovedSignificantlySincePressed()) return;

            if (auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor (this))
            {
                if (!dragContainer->isDragAndDropActive())
                {
                    auto bounds = pedalComponent->getBounds();
                    juce::Image dragImage (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
                    juce::Graphics g2 (dragImage);
                    pedalComponent->paintEntireComponent (g2, true);
                    
                    dragContainer->startDragging ("playslot:" + juce::String(index), e.eventComponent, dragImage, true);
                }
            }
        }
    }

    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        auto desc = details.description.toString();
        return desc.startsWith ("pedal:") || desc.startsWith ("playslot:");
    }
    
    void itemDragEnter (const SourceDetails&) override { isDragHovering = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { isDragHovering = false; repaint(); }
    
    void itemDropped (const SourceDetails& details) override
    {
        isDragHovering = false;
        repaint();
        
        auto desc = details.description.toString();
        juce::StringArray tok;
        tok.addTokens (desc, ":", "");
        
        if (tok.size() >= 2)
        {
            if (tok[0] == "pedal")
            {
                parent.handlePedalDropped (index, tok[1]);
            }
            else if (tok[0] == "playslot")
            {
                int sourceSlot = tok[1].getIntValue();
                if (sourceSlot != index)
                    parent.handleSlotSwapped (sourceSlot, index);
            }
        }
    }

private:
    PlayTabComponent& parent;
    int index;
    juce::String category;
    juce::String label;
    bool isDragHovering = false;
    
    std::unique_ptr<PedalComponent> pedalComponent;
    std::unique_ptr<juce::Label> nameLabel;
};

//==============================================================================
PlayTabComponent::PlayTabComponent (AudioGraphEngine& engine, InventoryOverlay& inventory, MidiLearnManager& midiLearn)
    : playEngine (engine), inventoryOverlay (inventory), playMidiLearn (midiLearn)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&container, false);
    viewport.setScrollBarsShown (false, true);
    
    auto addSlot = [this](const juce::String& cat, const juce::String& lbl) {
        auto s = std::make_unique<Slot>();
        s->recommendedCategory = cat;
        s->label = lbl;
        slots.push_back (std::move (s));
    };
    
    addSlot ("Dynamics", "Comp / Gate");
    addSlot ("Drive", "Drive / Fuzz");
    addSlot ("Amp", "Amp / Preamp");
    addSlot ("Modulation", "Modulation");
    addSlot ("Delay", "Delay");
    addSlot ("Reverb", "Reverb");

    addAndMakeVisible (presetMenu);
    presetMenu.setTextWhenNothingSelected ("Select a Preset...");
    presetMenu.onChange = [this] { loadPreset (presetMenu.getText()); };
    rebuildPresetMenu();   // populates built-ins + any user-saved presets

    addAndMakeVisible (btnSavePreset);
    btnSavePreset.setTooltip ("Save the current pedal chain as a reusable preset");
    btnSavePreset.onClick = [this] { saveCurrentChainAsPreset(); };

    addAndMakeVisible (addSlotButton);
    addSlotButton.onClick = [this] {
        auto s = std::make_unique<Slot>();
        s->recommendedCategory = "Any";
        s->label = "Pedal";
        slots.push_back (std::move (s));
        rebuildSlots();
    };

    startTimerHz (10);
    rebuildSlots();

    // First-launch onboarding: if we've never welcomed this user before
    // AND the chain is empty, drop the "Classic Rock" demo preset in so
    // they immediately have something to hear and tweak. The sentinel
    // file is created in the app support dir so this only fires once.
    {
        auto sentinel = pf::paths::getFirstRunSentinel();
        sentinel.getParentDirectory().createDirectory();
        const bool emptyChain = [this]
        {
            for (const auto& inst : playEngine.getPedalInstances())
                if (inst.boardId == "play_board") return false;
            return true;
        }();
        if (! sentinel.existsAsFile() && emptyChain)
        {
            sentinel.create();
            // Defer so the rest of the UI finishes building before the
            // preset load triggers slot rebuilds + the welcome toast.
            juce::MessageManager::callAsync ([this]
            {
                loadPreset ("Classic Rock");
                presetMenu.setText ("Classic Rock", juce::dontSendNotification);
                pf::toastInfo ("Welcome to PedalForge - loaded the Classic Rock demo. "
                               "Click any slot, or press Tab to swap pedals.");
            });
        }
    }

    notesOverlay.setNotes (playEngine.playNotes);
    addChildComponent (notesOverlay);
    btnNotes.setClickingTogglesState (true);
    btnNotes.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFF59E0B)); // amber
    btnNotes.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
    addAndMakeVisible (btnNotes);
    btnNotes.setTooltip ("Toggle Notes");
    btnNotes.onClick = [this] {
        NotesOverlay::globallyVisible = btnNotes.getToggleState();
        bool show = NotesOverlay::globallyVisible;
        notesOverlay.setVisible (show);
        if (show && playEngine.playNotes.empty())
            notesOverlay.addNote (120, 80);
    };
}

PlayTabComponent::~PlayTabComponent()
{
}

void PlayTabComponent::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark.darker(0.5f));

    auto toolbarArea = getLocalBounds().removeFromTop (36);
    g.setGradientFill (juce::ColourGradient (
        PedalForgeLookAndFeel::bgMid.darker (0.1f), 0, (float)toolbarArea.getY(),
        PedalForgeLookAndFeel::bgMid.darker (0.35f), 0, (float)toolbarArea.getBottom(), false));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float) getWidth());

    // Empty-chain hint — when nothing's been added yet, the field of dashed
    // empty cards is a bit cryptic for a new user. Drop a quiet caption above.
    bool anyPedal = false;
    for (const auto& s : slots) if (s->pedalId.uid != 0) { anyPedal = true; break; }
    if (! anyPedal)
    {
        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (13.0f));
        auto hintArea = getLocalBounds().removeFromTop (60).withTrimmedTop (38);
        g.drawText ("Pick a preset above, click a slot to add a pedal, or press Tab to browse.",
                    hintArea, juce::Justification::centred);
    }
}

void PlayTabComponent::resized()
{
    auto bounds = getLocalBounds();
    
    auto topBar = bounds.removeFromTop (36);
    topBar.reduce (8, 4);
    btnNotes.setBounds (topBar.removeFromLeft (60).withSizeKeepingCentre (60, 24));
    presetMenu.setBounds (topBar.removeFromLeft (200).withSizeKeepingCentre (200, 24));
    topBar.removeFromLeft (4);
    btnSavePreset.setBounds (topBar.removeFromLeft (80).withSizeKeepingCentre (80, 24));
    addSlotButton.setBounds (topBar.removeFromRight (120).withSizeKeepingCentre (120, 24));

    viewport.setBounds (bounds);
    notesOverlay.setBounds (bounds);
    
    // Layout slots horizontally
    int slotHeight = 350;
    int padding = 20;
    int emptySlotWidth = 160;
    
    int totalWidth = padding;
    for (int i = 0; i < slots.size(); ++i)
    {
        int w = emptySlotWidth;
        if (slots[i]->pedalId.uid != 0)
        {
            if (auto* inst = playEngine.getPedalInstance (slots[i]->pedalId))
            {
                if (inst->design != nullptr)
                {
                    float ratio = inst->design->chassisW / inst->design->chassisH;
                    w = (int)(slotHeight * ratio);
                }
            }
        }
        totalWidth += w + padding;
    }
    
    container.setBounds (0, 0, totalWidth, bounds.getHeight());
    
    int x = padding;
    int y = (bounds.getHeight() - slotHeight) / 2;
    
    for (int i = 0; i < slots.size(); ++i)
    {
        int w = emptySlotWidth;
        if (slots[i]->pedalId.uid != 0)
        {
            if (auto* inst = playEngine.getPedalInstance (slots[i]->pedalId))
            {
                if (inst->design != nullptr)
                {
                    float ratio = inst->design->chassisW / inst->design->chassisH;
                    w = (int)(slotHeight * ratio);
                }
            }
        }

        if (slots[i]->wrapper)
            slots[i]->wrapper->setBounds (x, y - 30, w, slotHeight + 30);
            
        x += w + padding;
    }
}

void PlayTabComponent::timerCallback()
{
    // Check if pedals count changed
    int currentCount = playEngine.getPedalInstances().size();
    if (currentCount != lastPedalCount)
    {
        rebuildSlots();
        lastPedalCount = currentCount;
    }

    bool needsRepaint = false;
    for (auto& s : slots)
    {
        if (s->wrapper != nullptr)
        {
            if (auto* pedalComp = s->wrapper->getPedalComponent())
            {
                auto& instance = pedalComp->getInstance();
                if (instance.design != nullptr)
                {
                    auto* node = playEngine.getGraph().getNodeForId (instance.nodeID);
                    if (node != nullptr)
                    {
                        if (auto* proc = node->getProcessor())
                        {
                            auto params = proc->getParameters();
                            for (const auto& mapping : instance.design->mappings)
                            {
                                if (mapping.nodeParam.endsWith("_filepath"))
                                {
                                    int targetNodeID = mapping.nodeParam.upToFirstOccurrenceOf("_", false, false).getIntValue();
                                    if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(proc))
                                    {
                                    auto getDSPNode = [&](int id) -> DSPNode* {
                                        if (auto* n = graphProc->getDSPGraph().getNode(id)) return n;
                                        if (auto* n = graphProc->getDSPGraph().getNode(id - 1)) return n;
                                        if (auto* n = graphProc->getDSPGraph().getNode(id + 1)) return n;
                                        return nullptr;
                                    };
                                    if (auto* targetNode = getDSPNode(targetNodeID))
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
                                                lines.addLines (instance.controlTexts[baseControlID]);
                                                while (lines.size() <= lineIndex) lines.add ("");
                                                if (lines[lineIndex] != text)
                                                {
                                                    lines.set (lineIndex, text);
                                                    instance.controlTexts[baseControlID] = lines.joinIntoString ("\n");
                                                    needsRepaint = true;
                                                }
                                            }
                                            else
                                            {
                                                if (instance.controlTexts[baseControlID] != text)
                                                {
                                                    instance.controlTexts[baseControlID] = text;
                                                    needsRepaint = true;
                                                }
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    bool wasDisplay = false;
                                    int targetNodeID = mapping.nodeParam.upToFirstOccurrenceOf("_", false, false).getIntValue();
                                    if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(proc))
                                    {
                                        auto getDSPNode = [&](int id) -> DSPNode* {
                                            if (auto* n = graphProc->getDSPGraph().getNode(id)) return n;
                                            if (auto* n = graphProc->getDSPGraph().getNode(id - 1)) return n;
                                            if (auto* n = graphProc->getDSPGraph().getNode(id + 1)) return n;
                                            return nullptr;
                                        };
                                        if (auto* targetNode = getDSPNode(targetNodeID))
                                        {
                                            if (targetNode->isDisplayNode())
                                            {
                                                wasDisplay = true;
                                                juce::String baseControlID = mapping.controlID;
                                                
                                                if (auto* px = targetNode->getPixelData())
                                                {
                                                    instance.controlData[baseControlID] = *px;
                                                    needsRepaint = true;
                                                }
                                                else
                                                {
                                                    float val = targetNode->getDisplayValue();
                                                    if (std::abs(instance.controlValues[baseControlID] - val) > 0.001f)
                                                    {
                                                        instance.controlValues[baseControlID] = val;
                                                        needsRepaint = true;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    
                                    if (!wasDisplay)
                                    {
                                        for (auto* p : params)
                                        {
                                            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                                            {
                                                if (matchMappingParam (mapping.nodeParam, ranged->getParameterID()))
                                                {
                                                    float val = ranged->getValue();
                                                    juce::String text = ranged->getText (ranged->getValue(), 32);

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
                                                        lines.addLines (instance.controlTexts[baseControlID]);
                                                        while (lines.size() <= lineIndex) lines.add ("");
                                                        if (lines[lineIndex] != text)
                                                        {
                                                            lines.set (lineIndex, text);
                                                            instance.controlTexts[baseControlID] = lines.joinIntoString ("\n");
                                                            textChanged = true;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        if (instance.controlTexts[baseControlID] != text)
                                                        {
                                                            instance.controlTexts[baseControlID] = text;
                                                            textChanged = true;
                                                        }
                                                    }

                                                    if (instance.controlValues[baseControlID] != val || textChanged)
                                                    {
                                                        instance.controlValues[baseControlID] = val;
                                                        needsRepaint = true;
                                                    }
                                                    break;
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
        }
    }

    if (needsRepaint)
        repaint();
}

void PlayTabComponent::rebuildSlots()
{
    container.removeAllChildren();
    
    for (int i = 0; i < slots.size(); ++i)
    {
        auto& s = slots[i];
        s->pedalId.uid = 0;
        s->wrapper.reset();
        
        s->wrapper = std::make_unique<PlaySlotWrapper> (*this, i, s->recommendedCategory, s->label);
        
        // Find if there's a pedal at boardX == i * 100.0f
        for (auto& inst : playEngine.getPedalInstances())
        {
            if (inst.onBoard && std::abs(inst.boardX - (i * 100.0f)) < 1.0f)
            {
                s->pedalId = inst.nodeID;
                auto comp = std::make_unique<PedalComponent> (const_cast<PedalInstance&>(inst), playEngine, playMidiLearn);
                if (onOpenLibrary)
                    comp->onOpenLibrary = onOpenLibrary;
                if (onOpenOverlay)
                {
                    comp->onOpenOverlay = [this, compPtr = comp.get()](const juce::String& pageName) {
                        if (onOpenOverlay)
                            onOpenOverlay(&compPtr->getInstance(), pageName);
                    };
                }
                s->wrapper->setPedal (std::move (comp), inst.name);
                break;
            }
        }
        
        container.addAndMakeVisible (s->wrapper.get());
    }
    
    rebuildRouting();
    resized();
    notesOverlay.repaint();
}

void PlayTabComponent::setOnOpenLibrary(std::function<void (const juce::String& category, std::function<void(const juce::File&)> onFileSelected)> cb)
{
    onOpenLibrary = cb;
    for (auto& s : slots)
    {
        if (s->wrapper != nullptr)
        {
            if (auto* pc = s->wrapper->getPedalComponent())
            {
                pc->onOpenLibrary = cb;
            }
        }
    }
}

void PlayTabComponent::setOnOpenOverlay(std::function<void (PedalInstance* instance, const juce::String& pageName)> cb)
{
    onOpenOverlay = cb;
    for (auto& s : slots)
    {
        if (s->wrapper != nullptr)
        {
            if (auto* pc = s->wrapper->getPedalComponent())
            {
                pc->onOpenOverlay = [cb, s=s.get()](const juce::String& pageName) {
                    if (cb && s->wrapper && s->wrapper->getPedalComponent()) {
                        cb(&s->wrapper->getPedalComponent()->getInstance(), pageName);
                    }
                };
            }
        }
    }
}

void PlayTabComponent::rebuildRouting()
{
    auto& graph = playEngine.getGraph();
    
    // Remove all current audio and MIDI connections to ensure a clean slate
    for (auto c : graph.getConnections())
    {
        graph.removeConnection (c);
    }
    
    auto inNode = playEngine.getAudioInputNodeID();
    auto outNode = playEngine.getAudioOutputNodeID();
    auto inMidiNode = playEngine.getMidiInputNodeID();
    auto outMidiNode = playEngine.getMidiOutputNodeID();
    
    AudioGraphEngine::NodeID lastNode = inNode;
    AudioGraphEngine::NodeID lastMidiNode = inMidiNode;
    
    for (auto& s : slots)
    {
        if (s->pedalId.uid != 0)
        {
            // Audio Routing
            graph.addConnection ({ { lastNode, 0 }, { s->pedalId, 0 } });
            graph.addConnection ({ { lastNode, 1 }, { s->pedalId, 1 } });
            lastNode = s->pedalId;
            
            // MIDI Routing
            graph.addConnection ({ { lastMidiNode, juce::AudioProcessorGraph::midiChannelIndex }, 
                                   { s->pedalId,   juce::AudioProcessorGraph::midiChannelIndex } });
            lastMidiNode = s->pedalId;
        }
    }
    
    // Connect last pedal to output
    graph.addConnection ({ { lastNode, 0 }, { outNode, 0 } });
    graph.addConnection ({ { lastNode, 1 }, { outNode, 1 } });
    
    graph.addConnection ({ { lastMidiNode, juce::AudioProcessorGraph::midiChannelIndex }, 
                           { outMidiNode,   juce::AudioProcessorGraph::midiChannelIndex } });
    
    // Passthrough extra channels (like FX Send/Return on 3-4). Guard the
    // node lookup — if the input node isn't present (e.g. engine mid-rebuild
    // or restored from an unexpected state) this used to deref null and crash
    // at startup.
    if (auto* inNodePtr = graph.getNodeForId (inNode))
    {
        if (auto* proc = inNodePtr->getProcessor())
        {
            const int totalChans = proc->getTotalNumOutputChannels();
            for (int ch = 2; ch < totalChans; ++ch)
                graph.addConnection ({ { inNode, ch }, { outNode, ch } });
        }
    }
}

void PlayTabComponent::handleSlotClicked (int slotIndex)
{
    activeSlotIndex = slotIndex;
    inventoryOverlay.onPedalClicked = [this](const juce::String& pedalName)
    {
        if (activeSlotIndex >= 0)
        {
            handlePedalDropped (activeSlotIndex, pedalName);
            inventoryOverlay.hide();
            inventoryOverlay.onPedalClicked = nullptr;
            activeSlotIndex = -1;
        }
    };
    
    inventoryOverlay.toggle();
    if (inventoryOverlay.isOpen())
        inventoryOverlay.grabKeyboardFocus();
}

void PlayTabComponent::handlePedalDropped (int slotIndex, const juce::String& pedalName)
{
    // Look up pedal in registry
    bool loaded = false;
    for (auto& info : getFactoryPedals())
    {
        if (info.name == pedalName)
        {
            // If there's an existing pedal in this slot, remove it
            if (slots[slotIndex]->pedalId.uid != 0)
            {
                playEngine.removePedal (slots[slotIndex]->pedalId);
            }

            auto processor = info.factory();
            auto nodeId = playEngine.addPedal (std::move (processor),
                                               "play_board", 0, // play_board is arbitrary
                                               slotIndex * 100.0f, 0.0f,
                                               info.gridW * 100.0f, info.gridH * 100.0f);
                                               
            if (auto* inst = playEngine.getPedalInstance (nodeId))
            {
                inst->colour = info.colour;
                inst->category = info.category;
                inst->numKnobs = info.numKnobs;

                if (auto* node = playEngine.getGraph().getNodeForId (nodeId))
                {
                    if (auto* proc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor()))
                    {
                        if (info.designFactory)
                        {
                            inst->design = info.designFactory();
                        }
                        else
                        {
                            inst->design = std::make_shared<PedalDesign>();
                            inst->design->name = info.name;
                            inst->design->category = info.category;
                            inst->design->chassisW = info.gridW * 100.0f;
                            inst->design->chassisH = info.gridH * 100.0f;
                            inst->design->chassisColour = info.colour;
                        }

                        inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                    }
                }
            }
            
            rebuildSlots();
            loaded = true;
            break;
        }
    }
    
    if (!loaded)
    {
        if (auto design = loadCustomPedalDesign (pedalName))
        {
            if (slots[slotIndex]->pedalId.uid != 0)
            {
                playEngine.removePedal (slots[slotIndex]->pedalId);
            }
            
            juce::String jsonGraph;
            if (!design->effectsGraph.isVoid())
                jsonGraph = juce::JSON::toString(design->effectsGraph);
                
            auto processor = std::make_unique<GraphPedalProcessor>(design->name, jsonGraph);
            
            
            auto nodeId = playEngine.addPedal (std::move (processor),
                                            "play_board", 0,
                                            slotIndex * 100.0f, 0.0f,
                                            design->chassisW, design->chassisH);
            
            if (auto* inst = playEngine.getPedalInstance (nodeId))
            {
                inst->colour = design->chassisColour;
                inst->category = design->category;
                inst->design = design;
                
                int numKnobs = 0;
                for (const auto& c : design->controls)
                    if (c.type == "knob") numKnobs++;
                inst->numKnobs = numKnobs;
            }
            rebuildSlots();
        }
    }
}

void PlayTabComponent::handleSlotSwapped (int sourceSlot, int targetSlot)
{
    if (sourceSlot < 0 || sourceSlot >= slots.size() || targetSlot < 0 || targetSlot >= slots.size())
        return;
        
    AudioGraphEngine::NodeID sourceNode = slots[sourceSlot]->pedalId;
    AudioGraphEngine::NodeID targetNode = slots[targetSlot]->pedalId;
    
    if (auto* sourceInst = playEngine.getPedalInstance (sourceNode))
        sourceInst->boardX = targetSlot * 100.0f;
        
    if (auto* targetInst = playEngine.getPedalInstance (targetNode))
        targetInst->boardX = sourceSlot * 100.0f;
        
    rebuildSlots();
}

void PlayTabComponent::removeSlot (int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < slots.size())
    {
        // If there's a pedal in this slot, remove it from the engine
        if (slots[slotIndex]->pedalId.uid != 0)
        {
            playEngine.removePedal (slots[slotIndex]->pedalId);
        }
        
        slots.erase (slots.begin() + slotIndex);
        
        // Update boardX for all subsequent pedals to keep them matching their slot index
        for (int i = 0; i < slots.size(); ++i)
        {
            if (slots[i]->pedalId.uid != 0)
            {
                if (auto* inst = playEngine.getPedalInstance (slots[i]->pedalId))
                {
                    inst->boardX = i * 100.0f;
                }
            }
        }
        
        rebuildSlots();
    }
}

void PlayTabComponent::loadPreset (const juce::String& presetName)
{
    // User-saved presets win — they may shadow a built-in name if the user
    // deliberately saved over one.
    auto userIt = userPresets.find (presetName);
    if (userIt != userPresets.end())
    {
        loadUserPreset (userIt->second);
        return;
    }

    // Clear existing pedals from playEngine
    auto instances = playEngine.getPedalInstances(); // Copy so we can iterate safely
    for (auto& inst : instances)
        if (inst.boardId == "play_board")
            playEngine.removePedal (inst.nodeID);

    auto addToSlot = [this](int slotIndex, const juce::String& pedalName) {
        handlePedalDropped (slotIndex, pedalName);
    };

    if (presetName == "Clean & Space")
    {
        addToSlot (0, "Compressor");
        addToSlot (3, "Chorus");
        addToSlot (4, "Delay");
        addToSlot (5, "Reverb");
    }
    else if (presetName == "Classic Rock")
    {
        addToSlot (0, "Noise Gate");
        addToSlot (1, "Overdrive");
        addToSlot (2, "Cabinet Sim");
        addToSlot (4, "Delay");
    }
    else if (presetName == "High Gain Lead")
    {
        addToSlot (0, "Noise Gate");
        addToSlot (1, "Distortion");
        addToSlot (3, "Phaser");
        addToSlot (4, "Delay");
        addToSlot (5, "Reverb");
    }
    else if (presetName == "Ambient Shimmer")
    {
        addToSlot (0, "Compressor");
        addToSlot (3, "Flanger");
        addToSlot (4, "Delay");
        addToSlot (5, "Reverb");
    }
    else if (presetName == "Blues Crunch")
    {
        addToSlot (0, "Compressor");
        addToSlot (1, "Overdrive");
        addToSlot (2, "Cabinet Sim");
        addToSlot (5, "Reverb");
    }
    else if (presetName == "Funk Wah Clean")
    {
        addToSlot (0, "Compressor");
        addToSlot (1, "Wah");
        addToSlot (3, "Chorus");
        addToSlot (5, "Reverb");
    }
    else if (presetName == "Worship Ambient")
    {
        addToSlot (0, "Compressor");
        addToSlot (1, "Overdrive");
        addToSlot (3, "Chorus");
        addToSlot (4, "Delay");
        addToSlot (5, "Reverb");
    }
    else if (presetName == "Country Slap")
    {
        addToSlot (0, "Compressor");
        addToSlot (2, "Cabinet Sim");
        addToSlot (4, "Delay");
        addToSlot (5, "Reverb");
    }

    rebuildSlots();
}

void PlayTabComponent::visibilityChanged()
{
    if (isVisible())
    {
        btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
        notesOverlay.setVisible (!playEngine.playNotes.empty());
    }
}

//==============================================================================
// User-preset save/load
//==============================================================================

juce::File PlayTabComponent::presetsDir()
{
    return pf::paths::getPlayPresetsDir();
}

void PlayTabComponent::rebuildPresetMenu()
{
    presetMenu.clear (juce::dontSendNotification);
    userPresets.clear();

    // Built-in presets first
    presetMenu.addItem ("Clean & Space",     1);
    presetMenu.addItem ("Classic Rock",      2);
    presetMenu.addItem ("High Gain Lead",    3);
    presetMenu.addItem ("Ambient Shimmer",   4);
    presetMenu.addItem ("Blues Crunch",      5);
    presetMenu.addItem ("Funk Wah Clean",    6);
    presetMenu.addItem ("Worship Ambient",   7);
    presetMenu.addItem ("Country Slap",      8);

    // User-saved presets — scan ~/Library/PedalForge/playpresets/
    auto dir = presetsDir();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        if (! files.isEmpty())
        {
            presetMenu.addSeparator();
            int idCounter = 100;
            for (const auto& f : files)
            {
                auto name = f.getFileNameWithoutExtension();
                userPresets[name] = f;
                presetMenu.addItem (name, idCounter++);
            }
        }
    }
}

void PlayTabComponent::clearSlot (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= (int) slots.size()) return;
    if (slots[(size_t) slotIndex]->pedalId.uid != 0)
    {
        playEngine.removePedal (slots[(size_t) slotIndex]->pedalId);
        slots[(size_t) slotIndex]->pedalId.uid = 0;
    }
    rebuildSlots();
}

void PlayTabComponent::saveCurrentChainAsPreset()
{
    // Default name based on the pedals on the chain.
    juce::String suggestedName = "My Preset";
    {
        juce::StringArray names;
        for (const auto& s : slots)
        {
            if (s->pedalId.uid != 0)
                if (auto* inst = playEngine.getPedalInstance (s->pedalId))
                    names.add (inst->name);
        }
        if (! names.isEmpty()) suggestedName = names.joinIntoString (" + ");
    }

    auto* alert = new juce::AlertWindow ("Save preset",
        "Name this pedal chain. It'll appear in the preset dropdown.",
        juce::AlertWindow::NoIcon);
    alert->addTextEditor ("name", suggestedName);
    alert->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PlayTabComponent> sp (this);
    alert->enterModalState (true, juce::ModalCallbackFunction::create (
        [sp, alert] (int result)
        {
            if (sp == nullptr || result != 1) return;
            auto name = alert->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            auto safe = name.replace ("/", "_").replace (":", "_");
            auto target = sp->presetsDir().getChildFile (safe + ".json");

            sp->writePresetFile (name, target);
            sp->rebuildPresetMenu();
            sp->presetMenu.setText (name, juce::dontSendNotification);
        }));
}

void PlayTabComponent::writePresetFile (const juce::String& name, const juce::File& target)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("name", name);
    root->setProperty ("formatVersion", 1);

    juce::Array<juce::var> slotArr;
    for (const auto& s : slots)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("category", s->recommendedCategory);
        obj->setProperty ("label",    s->label);
        juce::String pedalName;
        if (s->pedalId.uid != 0)
            if (auto* inst = playEngine.getPedalInstance (s->pedalId))
                pedalName = inst->name;
        obj->setProperty ("pedal", pedalName);   // empty if slot is unfilled
        slotArr.add (juce::var (obj));
    }
    root->setProperty ("slots", slotArr);

    target.replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

bool PlayTabComponent::loadUserPreset (const juce::File& file)
{
    auto json = juce::JSON::parse (file);
    if (! json.isObject()) return false;

    auto slotsArr = json.getProperty ("slots", juce::var());
    auto* arr = slotsArr.getArray();
    if (arr == nullptr) return false;

    // Clear current chain
    auto current = playEngine.getPedalInstances();   // copy
    for (auto& inst : current)
        if (inst.boardId == "play_board")
            playEngine.removePedal (inst.nodeID);

    // Rebuild slots vector from the preset
    slots.clear();
    for (const auto& sv : *arr)
    {
        auto s = std::make_unique<Slot>();
        s->recommendedCategory = sv.getProperty ("category", "Any").toString();
        s->label                = sv.getProperty ("label",    "Pedal").toString();
        slots.push_back (std::move (s));
    }

    rebuildSlots();

    // Now drop each named pedal into its slot. Doing this after rebuildSlots
    // so the slot wrappers exist for the drop targets.
    for (int i = 0; i < arr->size(); ++i)
    {
        auto pedalName = arr->getUnchecked (i).getProperty ("pedal", "").toString();
        if (pedalName.isNotEmpty())
            handlePedalDropped (i, pedalName);
    }

    return true;
}

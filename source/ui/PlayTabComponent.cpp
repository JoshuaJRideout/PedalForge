#include "PlayTabComponent.h"
#include "LookAndFeel.h"
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
        if (pedalComponent == nullptr)
        {
            if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
            {
                juce::PopupMenu menu;
                menu.addItem (1, "Delete Slot");
                
                juce::Component::SafePointer<PlayTabComponent> sp (&parent);
                int idx = index;
                menu.showMenuAsync (juce::PopupMenu::Options(),
                                    [sp, idx] (int result)
                                    {
                                        if (result == 1 && sp != nullptr)
                                        {
                                            sp->removeSlot (idx);
                                        }
                                    });
            }
            else
            {
                parent.handleSlotClicked (index);
            }
        }
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
    presetMenu.addItem ("Clean & Space", 1);
    presetMenu.addItem ("Classic Rock", 2);
    presetMenu.addItem ("High Gain Lead", 3);
    presetMenu.addItem ("Ambient Shimmer", 4);
    presetMenu.setTextWhenNothingSelected ("Select a Preset...");
    
    presetMenu.onChange = [this] {
        loadPreset (presetMenu.getText());
    };
    
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
}

PlayTabComponent::~PlayTabComponent()
{
}

void PlayTabComponent::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark.darker(0.5f));
    
    // Draw top bar for presets
    auto bounds = getLocalBounds();
    auto topBar = bounds.removeFromTop (44);
    g.setColour (PedalForgeLookAndFeel::bgDark);
    g.fillRect (topBar);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (43, 0.0f, (float) getWidth());
}

void PlayTabComponent::resized()
{
    auto bounds = getLocalBounds();
    
    auto topBar = bounds.removeFromTop (44);
    presetMenu.setBounds (topBar.reduced (10, 8).removeFromLeft (200));
    addSlotButton.setBounds (topBar.reduced (10, 8).removeFromRight (120));

    viewport.setBounds (bounds);
    
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
        
        // Find if there's a pedal at gridX == i
        for (auto& inst : playEngine.getPedalInstances())
        {
            if (inst.onBoard && inst.gridX == i)
            {
                s->pedalId = inst.nodeID;
                auto comp = std::make_unique<PedalComponent> (const_cast<PedalInstance&>(inst), playEngine, playMidiLearn);
                s->wrapper->setPedal (std::move (comp), inst.name);
                break;
            }
        }
        
        container.addAndMakeVisible (s->wrapper.get());
    }
    
    rebuildRouting();
    resized();
}

void PlayTabComponent::rebuildRouting()
{
    auto& graph = playEngine.getGraph();
    
    // Remove all current audio connections
    for (auto c : graph.getConnections())
    {
        if (c.source.channelIndex != juce::AudioProcessorGraph::midiChannelIndex)
            graph.removeConnection (c);
    }
    
    auto inNode = playEngine.getAudioInputNodeID();
    auto outNode = playEngine.getAudioOutputNodeID();
    
    AudioGraphEngine::NodeID lastNode = inNode;
    
    for (auto& s : slots)
    {
        if (s->pedalId.uid != 0)
        {
            graph.addConnection ({ { lastNode, 0 }, { s->pedalId, 0 } });
            graph.addConnection ({ { lastNode, 1 }, { s->pedalId, 1 } });
            lastNode = s->pedalId;
        }
    }
    
    graph.addConnection ({ { lastNode, 0 }, { outNode, 0 } });
    graph.addConnection ({ { lastNode, 1 }, { outNode, 1 } });
    
    // Passthrough extra channels (like FX Send/Return on 3-4)
    int totalChans = graph.getNodeForId(inNode)->getProcessor()->getTotalNumOutputChannels();
    for (int ch = 2; ch < totalChans; ++ch)
    {
        graph.addConnection ({ { inNode, ch }, { outNode, ch } });
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
                                               slotIndex, 0,
                                               info.gridW, info.gridH);
                                               
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
            break;
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
        sourceInst->gridX = targetSlot;
        
    if (auto* targetInst = playEngine.getPedalInstance (targetNode))
        targetInst->gridX = sourceSlot;
        
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
        
        // Update gridX for all subsequent pedals to keep them matching their slot index
        for (int i = 0; i < slots.size(); ++i)
        {
            if (slots[i]->pedalId.uid != 0)
            {
                if (auto* inst = playEngine.getPedalInstance (slots[i]->pedalId))
                {
                    inst->gridX = i;
                }
            }
        }
        
        rebuildSlots();
    }
}

void PlayTabComponent::loadPreset (const juce::String& presetName)
{
    // Clear existing pedals from playEngine
    auto instances = playEngine.getPedalInstances(); // Copy so we can iterate safely
    for (auto& inst : instances)
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
    
    rebuildSlots();
}

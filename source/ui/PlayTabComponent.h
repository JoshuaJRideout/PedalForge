#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "PedalComponent.h"
#include "InventoryOverlay.h"

class PlayTabComponent : public juce::Component, public juce::Timer
{
public:
    PlayTabComponent (AudioGraphEngine& engine, InventoryOverlay& inventory, MidiLearnManager& midiLearn);
    ~PlayTabComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void rebuildSlots();
    void rebuildRouting();
    void loadPreset (const juce::String& presetName);

    void handleSlotClicked (int slotIndex);
    void handlePedalDropped (int slotIndex, const juce::String& pedalName);
    void handleSlotSwapped (int sourceSlot, int targetSlot);
    
    void removeSlot (int slotIndex);

private:
    AudioGraphEngine& playEngine;
    InventoryOverlay& inventoryOverlay;
    MidiLearnManager& playMidiLearn;

    class PlaySlotWrapper;

    struct Slot
    {
        juce::String recommendedCategory;
        juce::String label;
        AudioGraphEngine::NodeID pedalId { 0 };
        std::unique_ptr<PlaySlotWrapper> wrapper;
    };

    std::vector<std::unique_ptr<Slot>> slots;

    juce::ComboBox presetMenu;
    juce::TextButton addSlotButton { "+ Add Slot" };
    juce::Viewport viewport;
    juce::Component container;
    
    int lastPedalCount = 0;
    int activeSlotIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayTabComponent)
};

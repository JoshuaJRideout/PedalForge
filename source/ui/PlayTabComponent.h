#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "PedalComponent.h"
#include "InventoryOverlay.h"
#include "NotesOverlay.h"

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
    void clearSlot  (int slotIndex);   // remove pedal but keep the slot
    void visibilityChanged() override;

    std::function<void (const juce::String& category, std::function<void(const juce::File&)> onFileSelected)> onOpenLibrary;
    void setOnOpenLibrary (std::function<void (const juce::String& category, std::function<void(const juce::File&)> onFileSelected)> cb);

    std::function<void (PedalInstance* instance, const juce::String& pageName)> onOpenOverlay;
    void setOnOpenOverlay (std::function<void (PedalInstance* instance, const juce::String& pageName)> cb);

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
    juce::TextButton btnSavePreset { "Save..." };
    juce::TextButton addSlotButton { "+ Add Slot" };
    juce::Viewport viewport;
    juce::Component container;

    // User-saved presets discovered on disk. Key = preset name, value = file path.
    // Loaded alongside the built-in presets and shown in the same dropdown.
    std::map<juce::String, juce::File> userPresets;

    void rebuildPresetMenu();
    void saveCurrentChainAsPreset();
    void writePresetFile (const juce::String& name, const juce::File& target);
    bool loadUserPreset (const juce::File& file);
    static juce::File presetsDir();
    
    int lastPedalCount = 0;
    int activeSlotIndex = -1;

    // ── Notes ──
    NotesOverlay notesOverlay;
    juce::TextButton btnNotes { "Notes" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayTabComponent)
};

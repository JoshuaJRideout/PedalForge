#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "PedalComponent.h"
#include "NotesOverlay.h"

class PlayTabComponent : public juce::Component,
                         public juce::Timer,
                         public juce::DragAndDropTarget
{
public:
    PlayTabComponent (AudioGraphEngine& engine, MidiLearnManager& midiLearn);
    ~PlayTabComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void rebuildSlots();
    void rebuildRouting();
    void loadPreset (const juce::String& presetName);

    //==========================================================================
    // Programmatic control surface for the AI agent's play_* tools.
    juce::StringArray getPresetNames();                                 // built-in + user
    juce::String      describeChain();                                  // current signal chain
    bool              addPedalToChain (const juce::String& pedalName);  // append (factory/custom)
    void              clearChain();                                     // remove all play pedals

    void handlePedalDropped (int slotIndex, const juce::String& pedalName);
    void handleSlotSwapped (int sourceSlot, int targetSlot);

    void removeSlot (int slotIndex);
    void visibilityChanged() override;

    // The Play area is the drop target for everything: pedals dragged down from
    // the shelf are inserted into the chain; pedals dragged within the chain are
    // reordered; a pedal dragged down below the row is removed.
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragMove (const SourceDetails& details) override;
    void itemDragExit (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

    std::function<void (const juce::String& category, std::function<void(const juce::File&)> onFileSelected)> onOpenLibrary;
    void setOnOpenLibrary (std::function<void (const juce::String& category, std::function<void(const juce::File&)> onFileSelected)> cb);

    std::function<void (PedalInstance* instance, const juce::String& pageName)> onOpenOverlay;
    void setOnOpenOverlay (std::function<void (PedalInstance* instance, const juce::String& pageName)> cb);

private:
    AudioGraphEngine& playEngine;
    MidiLearnManager& playMidiLearn;

    class PlaySlotWrapper;
    class PedalShelf;     // top strip of draggable pedal cards

    // One Slot per real pedal in the chain (signal order = boardX). No empty
    // placeholders — the chain is just the pedals you've added.
    struct Slot
    {
        juce::String recommendedCategory;   // legacy/preset metadata only
        juce::String label;
        AudioGraphEngine::NodeID pedalId { 0 };
        std::unique_ptr<PlaySlotWrapper> wrapper;
    };

    std::vector<std::unique_ptr<Slot>> slots;

    // The currently-selected chain pedal (highlighted; Delete/Backspace removes).
    AudioGraphEngine::NodeID selectedPedalId { 0 };

    std::unique_ptr<PedalShelf> shelf;
    juce::ComboBox presetMenu;
    juce::TextButton btnSavePreset { "Save..." };
    juce::Viewport viewport;
    juce::Component container;

    // User-saved presets discovered on disk. Key = preset name, value = file path.
    std::map<juce::String, juce::File> userPresets;

    void rebuildPresetMenu();
    void saveCurrentChainAsPreset();
    void writePresetFile (const juce::String& name, const juce::File& target);
    bool loadUserPreset (const juce::File& file);
    static juce::File presetsDir();

    // Add a pedal (factory or custom) to the chain at a given signal position
    // (boardX). Order is purely boardX, so callers append at maxBoardX+100 or
    // place at index*100 to slot into a specific spot.
    void addPedalByName (const juce::String& pedalName, float boardX);
    void selectPedal (AudioGraphEngine::NodeID id);   // highlight + remember
    float nextChainX() const;                         // boardX for an appended pedal

    // Drag-reorder / drag-off-to-remove state (set during a drag over the area).
    void  computeDrop (juce::Point<int> p, const juce::String& descriptor);
    float boardXForInsert (int gapIndex, AudioGraphEngine::NodeID excluding) const;
    bool  dragActive = false;
    bool  dropWillRemove = false;
    int   dropInsertIndex = 0;
    int   dropIndicatorX = 0;          // in this component's coords
    int   chainRowTop = 0, chainRowBottom = 0;

    int lastPedalCount = 0;

    // ── Notes ──
    NotesOverlay notesOverlay;
    juce::TextButton btnNotes { "Notes" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayTabComponent)
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "LookAndFeel.h"
#include "NotesOverlay.h"
#include "InventoryPanel.h"

class DSPGraph;
struct PedalDesign;

class PedalDesignerComponent : public juce::Component,
                               public juce::DragAndDropContainer,
                               public juce::ChangeListener
{
public:
    PedalDesignerComponent();
    ~PedalDesignerComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Connect to the Effects Forge graph so the param dropdown can list DSP params. */
    void setEffectsGraph (DSPGraph* graph);

    /** Load a PedalDesign's visual layout into the canvas. */
    void loadDesign (const PedalDesign& design);

    /** Get the current PedalDesign from the canvas. */
    PedalDesign getDesign() const;

    /** Clear the canvas for a new design. */
    void clearDesign();

    /** ChangeListener callback for colour picker. */
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void visibilityChanged() override;

private:

    class ChassisCanvas;
    class PropertiesPanel;
    class LayersPanel;
    class PagesPanel;

    std::unique_ptr<ChassisCanvas> canvas;
    // Docked "Add" inventory (left strip) — the parts toybox; replaces the
    // Q-menu on this tab. Phase 3 of the editor-shell unification.
    InventoryPanel inventoryPanel;
    std::unique_ptr<PropertiesPanel> properties;
    // The Properties panel can be taller than the window (especially the
    // chassis view with the style controls), so it lives inside a scrolling
    // viewport rather than directly as the tab content.
    std::unique_ptr<juce::Viewport> propertiesViewport;
    std::unique_ptr<LayersPanel> layersPanel;
    std::unique_ptr<PagesPanel> pagesPanel;
    std::unique_ptr<juce::TabbedComponent> rightTabs;
    DSPGraph* effectsGraph = nullptr;

    // Toolbar controls
    juce::TextButton colourSwatchBtn { "" };
    juce::ComboBox gridCombo;
    juce::TextButton btnZoomIn { "+" }, btnZoomOut { "-" }, btnFitView { "Fit" };
    juce::TextButton btnAlignLeft { "AL" }, btnAlignCenterH { "AC" }, btnAlignRight { "AR" };
    juce::TextButton btnDistributeH { "DH" }, btnDistributeV { "DV" };
    void showColourPicker();
    void alignSelected (int mode);
    void distributeSelected (bool horizontal);

    // ── Notes ──
    std::vector<StickyNote> designNotes;
    NotesOverlay notesOverlay;
    juce::TextButton btnNotes { "Notes" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalDesignerComponent)
};

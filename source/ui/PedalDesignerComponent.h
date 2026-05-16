#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "LookAndFeel.h"

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

private:

    class ChassisCanvas;
    class PropertiesPanel;

    std::unique_ptr<ChassisCanvas> canvas;
    std::unique_ptr<PropertiesPanel> properties;
    DSPGraph* effectsGraph = nullptr;

    // Toolbar colour picker
    juce::TextButton colourSwatchBtn { "" };
    void showColourPicker();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalDesignerComponent)
};

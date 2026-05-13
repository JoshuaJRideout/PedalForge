#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"

class DSPGraph;
struct PedalDesign;

class PedalDesignerComponent : public juce::Component,
                               public juce::DragAndDropContainer
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

    /** Clear the canvas for a new design. */
    void clearDesign();

private:
    class HardwarePalette;
    class ChassisCanvas;
    class PropertiesPanel;

    std::unique_ptr<HardwarePalette> palette;
    std::unique_ptr<ChassisCanvas> canvas;
    std::unique_ptr<PropertiesPanel> properties;
    DSPGraph* effectsGraph = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalDesignerComponent)
};

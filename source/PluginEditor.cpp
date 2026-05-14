#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/PedalDesign.h"

//==============================================================================
PedalForgeEditor::PedalForgeEditor (PedalForgeProcessor& proc)
    : AudioProcessorEditor (proc),
      processorRef (proc),
      grid (proc.getGraphEngine()),
      presetBrowser (proc.getPresetManager())
{
    setLookAndFeel (&lookAndFeel);

    // Title
    titleLabel.setText ("PedalForge", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    titleLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
    addAndMakeVisible (titleLabel);

    // Tabs
    tabPedalboard.setRadioGroupId (1);
    tabPedalboard.setClickingTogglesState (true);
    tabPedalboard.setToggleState (true, juce::dontSendNotification);
    tabPedalboard.addListener (this);
    addAndMakeVisible (tabPedalboard);

    tabForge.setRadioGroupId (1);
    tabForge.setClickingTogglesState (true);
    tabForge.addListener (this);
    addAndMakeVisible (tabForge);

    tabEffects.setRadioGroupId (1);
    tabEffects.setClickingTogglesState (true);
    tabEffects.addListener (this);
    addAndMakeVisible (tabEffects);

    tabStore.setRadioGroupId (1);
    tabStore.setClickingTogglesState (true);
    tabStore.addListener (this);
    addAndMakeVisible (tabStore);

    // Components
    addAndMakeVisible (palette);
    addAndMakeVisible (grid);
    addAndMakeVisible (presetBrowser);
    
    addChildComponent (pedalDesigner);    // Initially hidden
    addChildComponent (nodeGraphEditor);  // Initially hidden

    // Wire the Effects Forge graph to the Pedal Forge for parameter mapping
    pedalDesigner.setEffectsGraph (&nodeGraphEditor.getGraph());

    // Cross-tab: track which pedal is selected on the board
    grid.onPedalSelected = [this] (PedalInstance* inst)
    {
        activePedal = inst;
    };

    setSize (1200, 800);
    setResizable (true, true);
    setResizeLimits (900, 600, 2400, 1600);
}

PedalForgeEditor::~PedalForgeEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void PedalForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    // Toolbar background
    auto toolbar = getLocalBounds().removeFromTop (toolbarHeight);
    g.setColour (PedalForgeLookAndFeel::bgMid);
    g.fillRect (toolbar);

    // Toolbar bottom border
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (toolbarHeight - 1, 0.0f, (float) getWidth());

    // Palette right border (only draw if palette is visible)
    if (palette.isVisible())
        g.drawVerticalLine (paletteWidth, (float) toolbarHeight, (float) getHeight());
}

void PedalForgeEditor::resized()
{
    auto bounds = getLocalBounds();

    // Toolbar
    auto toolbar = bounds.removeFromTop (toolbarHeight);
    titleLabel.setBounds (toolbar.removeFromLeft (140).reduced (12, 0));

    // Preset browser
    toolbar.removeFromLeft (8);
    presetBrowser.setBounds (toolbar.removeFromLeft (350));

    // Tabs (right-aligned)
    tabStore.setBounds      (toolbar.removeFromRight (100).reduced (4, 6));
    tabEffects.setBounds    (toolbar.removeFromRight (100).reduced (4, 6));
    tabForge.setBounds      (toolbar.removeFromRight (100).reduced (4, 6));
    tabPedalboard.setBounds (toolbar.removeFromRight (100).reduced (4, 6));

    // Full-area views (below toolbar, stacked)
    pedalDesigner.setBounds (bounds);
    nodeGraphEditor.setBounds (bounds);

    // Palette sidebar
    palette.setBounds (bounds.removeFromLeft (paletteWidth));

    // Pedalboard grid fills the rest
    grid.setBounds (bounds);
}

//==============================================================================
void PedalForgeEditor::buttonClicked (juce::Button* button)
{
    if (button == &tabPedalboard || button == &tabForge
        || button == &tabEffects || button == &tabStore)
    {
        bool isPedalboard = tabPedalboard.getToggleState();
        bool isForge      = tabForge.getToggleState();
        bool isEffects    = tabEffects.getToggleState();

        // Pedalboard view
        grid.setVisible (isPedalboard);
        palette.setVisible (isPedalboard);
        presetBrowser.setVisible (isPedalboard);

        // Forge (Pedal Designer) — load active pedal's design if available
        pedalDesigner.setVisible (isForge);
        if (isForge && activePedal != nullptr && activePedal->design != nullptr)
        {
            pedalDesigner.loadDesign (*activePedal->design);
        }

        // Effects builder — load active pedal's effects graph if available
        nodeGraphEditor.setVisible (isEffects);
        if (isEffects && activePedal != nullptr && activePedal->design != nullptr)
        {
            nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);
        }

        // Re-wire graph pointer (it may have changed after load)
        pedalDesigner.setEffectsGraph (&nodeGraphEditor.getGraph());

        repaint();
    }
}

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

    // Tabs — all in one radio group
    for (auto* tab : { &tabBoard, &tabRoute, &tabForge, &tabFX, &tabLibrary, &tabStore })
    {
        tab->setRadioGroupId (1);
        tab->setClickingTogglesState (true);
        tab->addListener (this);
        addAndMakeVisible (*tab);
    }
    tabBoard.setToggleState (true, juce::dontSendNotification);

    // Components
    addAndMakeVisible (grid);
    addAndMakeVisible (presetBrowser);

    addChildComponent (routeView);
    addChildComponent (pedalDesigner);
    addChildComponent (nodeGraphEditor);
    addChildComponent (libraryView);

    // Inventory overlay (Q-menu style, initially hidden)
    addChildComponent (inventory);
    addKeyListener (&inventory);

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
    removeKeyListener (&inventory);
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
    tabStore.setBounds   (toolbar.removeFromRight (80).reduced (4, 6));
    tabLibrary.setBounds (toolbar.removeFromRight (80).reduced (4, 6));
    tabFX.setBounds      (toolbar.removeFromRight (60).reduced (4, 6));
    tabForge.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabRoute.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabBoard.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));

    // All full-area views share the same bounds
    grid.setBounds (bounds);
    routeView.setBounds (bounds);
    pedalDesigner.setBounds (bounds);
    nodeGraphEditor.setBounds (bounds);
    libraryView.setBounds (bounds);

    // Inventory overlay spans the full content area below toolbar
    inventory.setBounds (bounds);
}

//==============================================================================
void PedalForgeEditor::buttonClicked (juce::Button* button)
{
    // Only handle tab buttons
    if (button != &tabBoard && button != &tabRoute && button != &tabForge
        && button != &tabFX && button != &tabLibrary && button != &tabStore)
        return;

    bool wasForge   = pedalDesigner.isVisible();
    bool wasFX      = nodeGraphEditor.isVisible();

    bool isBoard    = tabBoard.getToggleState();
    bool isRoute    = tabRoute.getToggleState();
    bool isForge    = tabForge.getToggleState();
    bool isFX       = tabFX.getToggleState();
    bool isLibrary  = tabLibrary.getToggleState();

    // ── Save state when LEAVING a tab ───────────────────────────────
    if (wasForge && activePedal != nullptr && activePedal->design != nullptr)
    {
        auto updatedDesign = pedalDesigner.getDesign();
        *(activePedal->design) = updatedDesign;
        grid.refreshSelectedPedal();
    }

    if (wasFX && activePedal != nullptr && activePedal->design != nullptr)
    {
        activePedal->design->effectsGraph = nodeGraphEditor.getGraph().toJSON();
    }

    // ── Show/hide views ─────────────────────────────────────────────
    grid.setVisible (isBoard);
    presetBrowser.setVisible (isBoard || isRoute);
    routeView.setVisible (isRoute);
    libraryView.setVisible (isLibrary);

    pedalDesigner.setVisible (isForge);
    if (isForge && activePedal != nullptr && activePedal->design != nullptr)
        pedalDesigner.loadDesign (*activePedal->design);

    nodeGraphEditor.setVisible (isFX);
    if (isFX && activePedal != nullptr && activePedal->design != nullptr)
        nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);

    // Re-wire graph pointer
    pedalDesigner.setEffectsGraph (&nodeGraphEditor.getGraph());

    // ── Set Q-menu context for the active tab ───────────────────────
    if (isBoard)       inventory.setContext (InventoryOverlay::Context::Board);
    else if (isRoute)  inventory.setContext (InventoryOverlay::Context::Route);
    else if (isForge)  inventory.setContext (InventoryOverlay::Context::Forge);
    else if (isFX)     inventory.setContext (InventoryOverlay::Context::FX);

    // Close the inventory when switching tabs
    if (inventory.isOpen())
        inventory.hide();

    repaint();
}

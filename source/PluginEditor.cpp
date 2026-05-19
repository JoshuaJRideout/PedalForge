#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/PedalDesign.h"
#include "ui/PlayTabComponent.h"

//==============================================================================
PedalForgeEditor::PedalForgeEditor (PedalForgeProcessor& proc)
    : AudioProcessorEditor (proc),
      processorRef (proc),
      grid (proc.getGraphEngine(), proc.midiLearn),
      midiSettingsPanel (proc.getGraphEngine())
{
    setLookAndFeel (&lookAndFeel);

    // Title
    titleLabel.setText ("PedalForge", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    titleLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
    addAndMakeVisible (titleLabel);

    // Tabs — all in one radio group
    for (auto* tab : { &tabPlay, &tabBoard, &tabRoute, &tabPedal, &tabFX, &tabLibrary, &tabStore, &tabMidi })
    {
        tab->setRadioGroupId (1);
        tab->setClickingTogglesState (true);
        tab->addListener (this);
        addAndMakeVisible (*tab);
    }
    tabPlay.setToggleState (true, juce::dontSendNotification);

    // Components
    playTab = new PlayTabComponent (proc.getPlayGraphEngine(), inventory, proc.playMidiLearn);
    addChildComponent (playTab);

    addAndMakeVisible (grid);

    addChildComponent (pedalDesigner);
    addChildComponent (nodeGraphEditor);
    addChildComponent (libraryView);
    addChildComponent (midiSettingsPanel);
    midiSettingsPanel.setMidiLearnManagers (&proc.midiLearn, &proc.playMidiLearn);

    // Routing editor (needs engine reference)
    routingEditor = new RoutingGraphEditor (proc.getGraphEngine());
    addChildComponent (routingEditor);

    // Inventory overlay (Q-menu style, initially hidden)
    addChildComponent (inventory);
    addKeyListener (&inventory);
    
    // Library overlay
    addChildComponent (libraryOverlay);

    // Wire the Effects Forge graph to the Pedal Forge for parameter mapping
    pedalDesigner.setEffectsGraph (&nodeGraphEditor.getGraph());

    nodeGraphEditor.onGraphChanged = [this] {
        if (activePedal != nullptr && activePedal->design != nullptr)
        {
            activePedal->design->effectsGraph = nodeGraphEditor.getGraph().toJSON();
            auto newProc = std::make_unique<GraphPedalProcessor> (activePedal->name, activePedal->design->effectsGraph);
            processorRef.getGraphEngine().updatePedalProcessor (activePedal->nodeID, std::move (newProc));
        }
    };

    // Cross-tab: track which pedal is selected on the board
    grid.onPedalSelected = [this] (PedalInstance* inst)
    {
        activePedal = inst;
    };
    routingEditor->onPedalSelected = [this] (PedalInstance* inst)
    {
        activePedal = inst;
    };
    
    turingRenderer = std::make_unique<TuringRenderer> (processorRef.getGraphEngine());

    grid.onOpenInventory = [this]
    {
        inventory.onPedalClicked = [this] (const juce::String& itemID) {
            grid.addPedalAtGrid (itemID, -1, -1);
            inventory.hide();
        };
        inventory.toggle();
    };

    // File picker overlay handler
    libraryOverlay.onAssetSelected = [this] (const juce::File& file)
    {
        if (activeFileCallback)
        {
            activeFileCallback(file);
            activeFileCallback = nullptr;
        }
    };

    addChildComponent (canvasOverlay);
    
    // Wire PedalboardGrid's open-library callback
    grid.onOpenLibrary = [this] (const juce::String& category, std::function<void(const juce::File&)> cb)
    {
        activeFileCallback = cb;
        libraryOverlay.showForCategory(category);
    };

    grid.onOpenOverlay = [this] (PedalInstance* instance, const juce::String& pageName)
    {
        canvasOverlay.showForPage (instance, &processorRef.getGraphEngine(), &processorRef.midiLearn, pageName);
    };

    if (playTab)
    {
        playTab->setOnOpenLibrary ([this] (const juce::String& category, std::function<void(const juce::File&)> cb)
        {
            activeFileCallback = cb;
            libraryOverlay.showForCategory(category);
        });

        playTab->setOnOpenOverlay ([this] (PedalInstance* instance, const juce::String& pageName)
        {
            canvasOverlay.showForPage (instance, &processorRef.getPlayGraphEngine(), &processorRef.playMidiLearn, pageName);
        });
    }

    setSize (1200, 800);
    setResizable (true, true);
    setResizeLimits (900, 600, 2400, 1600);
}

PedalForgeEditor::~PedalForgeEditor()
{
    removeKeyListener (&inventory);
    delete routingEditor;
    delete playTab;
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



    // Tabs (right-aligned)
    tabStore.setBounds   (toolbar.removeFromRight (80).reduced (4, 6));
    tabLibrary.setBounds (toolbar.removeFromRight (80).reduced (4, 6));
    tabMidi.setBounds    (toolbar.removeFromRight (60).reduced (4, 6));
    tabFX.setBounds      (toolbar.removeFromRight (60).reduced (4, 6));
    tabPedal.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabRoute.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabBoard.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabPlay.setBounds    (toolbar.removeFromRight (70).reduced (4, 6));

    auto contentBounds = bounds;
    
    if (playTab) playTab->setBounds (contentBounds);
    grid.setBounds (contentBounds);
    if (routingEditor) routingEditor->setBounds (contentBounds);
    pedalDesigner.setBounds (contentBounds);
    nodeGraphEditor.setBounds (contentBounds);
    libraryView.setBounds (contentBounds);
    midiSettingsPanel.setBounds (contentBounds);
    
    inventory.setBounds (getLocalBounds());
    libraryOverlay.setBounds (getLocalBounds());
    canvasOverlay.setBounds (getLocalBounds());
}

//==============================================================================
void PedalForgeEditor::buttonClicked (juce::Button* button)
{
    // Only handle tab buttons
    if (button != &tabPlay && button != &tabBoard && button != &tabRoute && button != &tabPedal
        && button != &tabFX && button != &tabLibrary && button != &tabStore
        && button != &tabMidi)
        return;

    bool wasPedal   = pedalDesigner.isVisible();
    bool wasFX      = nodeGraphEditor.isVisible();

    bool isPlay     = tabPlay.getToggleState();
    bool isBoard    = tabBoard.getToggleState();
    bool isRoute    = tabRoute.getToggleState();
    bool isPedal    = tabPedal.getToggleState();
    bool isFX       = tabFX.getToggleState();
    bool isLibrary  = tabLibrary.getToggleState();
    bool isMidi     = tabMidi.getToggleState();

    // ── Save state when LEAVING a tab ───────────────────────────────
    if (wasPedal && activePedal != nullptr && activePedal->design != nullptr)
    {
        auto updatedDesign = pedalDesigner.getDesign();
        *(activePedal->design) = updatedDesign;
        grid.refreshSelectedPedal();
    }

    if (wasFX && activePedal != nullptr && activePedal->design != nullptr)
    {
        activePedal->design->effectsGraph = nodeGraphEditor.getGraph().toJSON();

        // Rebuild processor so changes actually affect audio and parameters
        auto newProc = std::make_unique<GraphPedalProcessor> (activePedal->name, activePedal->design->effectsGraph);
        processorRef.getGraphEngine().updatePedalProcessor (activePedal->nodeID, std::move (newProc));
    }

    // ── Show/hide views ─────────────────────────────────────────────
    processorRef.setPlayMode (isPlay);
    
    if (playTab)
    {
        playTab->setVisible (isPlay);
        if (isPlay) playTab->rebuildSlots();
    }
    
    grid.setVisible (isBoard);
    midiSettingsPanel.setVisible (isMidi);
    
    if (isBoard) grid.rebuildFromEngine();   // Refresh sidebar + grid in case pedals were added from Route tab
    if (routingEditor)
    {
        routingEditor->setVisible (isRoute);
        if (isRoute) routingEditor->syncFromEngine();
    }
    libraryView.setVisible (isLibrary);
    if (isLibrary) libraryView.refreshAssets();

    pedalDesigner.setVisible (isPedal);
    if (isPedal && activePedal != nullptr && activePedal->design != nullptr)
        pedalDesigner.loadDesign (*activePedal->design);

    nodeGraphEditor.setVisible (isFX);
    if (isFX && activePedal != nullptr && activePedal->design != nullptr)
        nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);

    // Re-wire graph pointer
    pedalDesigner.setEffectsGraph (&nodeGraphEditor.getGraph());

    // ── Set Q-menu context for the active tab ───────────────────────
    if (isPlay)
    {
        inventory.setContext (InventoryOverlay::Context::Board);
        // PlayTabComponent handles its own onPedalClicked when a slot is clicked
    }
    else if (isBoard)
    {
        inventory.setContext (InventoryOverlay::Context::Board);
        inventory.onPedalClicked = [this] (const juce::String& itemID) {
            grid.addPedalAtGrid (itemID, -1, -1);
            inventory.hide();
        };
    }
    else if (isRoute)
    {
        inventory.setContext (InventoryOverlay::Context::Route);
        inventory.onPedalClicked = [this] (const juce::String& itemID) {
            // Add pedal to graph at arbitrary position
            grid.addPedalAtGrid (itemID, -1, -1);
            inventory.hide();
            if (routingEditor) routingEditor->syncFromEngine();
        };
    }
    else if (isPedal)
    {
        inventory.setContext (InventoryOverlay::Context::Forge);
        inventory.onPedalClicked = nullptr;
    }
    else if (isFX)
    {
        inventory.setContext (InventoryOverlay::Context::FX);
        inventory.onPedalClicked = nullptr;
    }

    // Close the inventory when switching tabs
    if (inventory.isOpen())
        inventory.hide();

    repaint();
}

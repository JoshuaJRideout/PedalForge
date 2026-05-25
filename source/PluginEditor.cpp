#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/PedalDesign.h"
#include "ui/PlayTabComponent.h"

#if JucePlugin_Build_Standalone
extern void OpenStandaloneAudioSettingsDialog();
#endif

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
    for (auto* tab : { &tabPlay, &tabBoard, &tabRoute, &tabPedal, &tabFX, &tabScript, &tabWiki, &tabLibrary, &tabStore, &tabMidi })
    {
        tab->setRadioGroupId (1);
        tab->setClickingTogglesState (true);
        tab->addListener (this);
        addAndMakeVisible (*tab);
    }
    tabPlay.setToggleState (true, juce::dontSendNotification);

   #if JucePlugin_Build_Standalone
    btnSettings.addListener (this);
    addAndMakeVisible (btnSettings);
   #endif

    btnTestSound.setClickingTogglesState (true);
    btnTestSound.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF10B981)); // beautiful active emerald green
    btnTestSound.setColour (juce::TextButton::textColourOnId, juce::Colour (0xFFFFFFFF));
    btnTestSound.setToggleState (processorRef.isTestSoundActive(), juce::dontSendNotification);
    btnTestSound.addListener (this);
    addAndMakeVisible (btnTestSound);

    // Components
    playTab = new PlayTabComponent (proc.getPlayGraphEngine(), inventory, proc.playMidiLearn);
    addChildComponent (playTab);

    addAndMakeVisible (grid);

    addChildComponent (pedalDesigner);
    addChildComponent (nodeGraphEditor);
    addChildComponent (libraryView);
    addChildComponent (midiSettingsPanel);
    midiSettingsPanel.setMidiLearnManagers (&proc.midiLearn, &proc.playMidiLearn);

    addChildComponent (scriptingTab);
    scriptingTab.onGraphChanged = [this] {
        // Refresh the FX graph view if it was modified by the graph builder script
        if (activePedal != nullptr && activePedal->design != nullptr)
        {
            nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);
            grid.refreshSelectedPedal();
        }
    };

    // Wiki tab — load docs from the wiki directory beside the plugin binary
    addChildComponent (wikiTab);
    {
        auto wikiDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                           .getParentDirectory().getChildFile ("docs").getChildFile ("wiki");
        if (! wikiDir.isDirectory())
        {
            // Fallback: try the source tree location (development builds)
            auto devWikiDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                                  .getParentDirectory();
            // Walk up until we find docs/wiki or hit root
            for (int i = 0; i < 8; ++i)
            {
                auto candidate = devWikiDir.getChildFile ("docs").getChildFile ("wiki");
                if (candidate.isDirectory())
                {
                    wikiDir = candidate;
                    break;
                }
                devWikiDir = devWikiDir.getParentDirectory();
            }
        }
        wikiTab.loadFromDirectory (wikiDir);
    }

    // Routing editor (needs engine reference)
    routingEditor = new RoutingGraphEditor (proc.getGraphEngine());
    addChildComponent (routingEditor);

    // Inventory overlay (Q-menu style, initially hidden)
    addChildComponent (inventory);
    addKeyListener (&inventory);
    addKeyListener (this);
    
    // Library overlay
    addChildComponent (libraryOverlay);

    // Wire the Effects Forge graph to the Pedal Forge for parameter mapping
    pedalDesigner.setEffectsGraph (&nodeGraphEditor.getGraph());

    nodeGraphEditor.getEngineDSPGraph = [this] () -> DSPGraph*
    {
        if (activePedal != nullptr)
        {
            if (auto* node = processorRef.getGraphEngine().getGraph().getNodeForId (activePedal->nodeID))
            {
                if (auto* proc = node->getProcessor())
                {
                    if (auto* gProc = dynamic_cast<GraphPedalProcessor*> (proc))
                    {
                        return &(gProc->getDSPGraph());
                    }
                }
            }
        }
        return nullptr;
    };

    nodeGraphEditor.onGraphChanged = [this] {
        if (activePedal != nullptr && activePedal->design != nullptr)
        {
            activePedal->design->effectsGraph = nodeGraphEditor.getGraph().toJSON();
            auto newProc = std::make_unique<GraphPedalProcessor> (activePedal->name, juce::JSON::toString (activePedal->design->effectsGraph));
            processorRef.getGraphEngine().updatePedalProcessor (activePedal->nodeID, std::move (newProc));
            processorRef.getGraphEngine().saveUndoState();
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
            grid.addPedalAtGrid (itemID, -1.0f, -1.0f);
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
    
    canvasOverlay.onOpenLibrary = [this] (const juce::String& category, std::function<void(const juce::File&)> cb)
    {
        activeFileCallback = cb;
        libraryOverlay.showForCategory(category);
    };
    
    libraryView.onAssetSelected = [this] (const juce::File& file)
    {
        if (activePedal == nullptr || activePedal->design == nullptr)
            return;

        // Determine category based on file extension
        juce::String ext = file.getFileExtension().toLowerCase();
        juce::String category;
        if (ext == ".nam") category = "NAM";
        else if (ext == ".wav" || ext == ".ir") category = "IR";

        // Find a mapping that exposes a filepath parameter for this category
        int targetNodeID = -1;
        for (const auto& mapping : activePedal->design->mappings)
        {
            if (mapping.nodeParam.endsWith ("_filepath"))
            {
                // Verify if this control is a loader for our category
                for (const auto& ctrl : activePedal->design->controls)
                {
                    if (ctrl.controlID == mapping.controlID)
                    {
                        juce::String ctrlCat = ctrl.libraryCategory;
                        if (ctrlCat.isEmpty()) ctrlCat = "NAM"; // default to NAM
                        
                        if (category == "NAM" && ctrlCat == "NAM")
                        {
                            targetNodeID = mapping.nodeParam.upToFirstOccurrenceOf ("_", false, false).getIntValue();
                            break;
                        }
                        else if (category == "IR" && (ctrlCat == "IR" || ctrlCat == "IR_CAB"))
                        {
                            targetNodeID = mapping.nodeParam.upToFirstOccurrenceOf ("_", false, false).getIntValue();
                            break;
                        }
                    }
                }
                if (targetNodeID >= 0)
                    break;
            }
        }

        // If no specific loader mapping is found, fall back to the first _filepath mapping
        if (targetNodeID < 0)
        {
            for (const auto& mapping : activePedal->design->mappings)
            {
                if (mapping.nodeParam.endsWith ("_filepath"))
                {
                    targetNodeID = mapping.nodeParam.upToFirstOccurrenceOf ("_", false, false).getIntValue();
                    break;
                }
            }
        }

        if (targetNodeID >= 0)
        {
            auto& engine = tabPlay.getToggleState() ? processorRef.getPlayGraphEngine() : processorRef.getGraphEngine();
            if (auto* node = engine.getGraph().getNodeForId (activePedal->nodeID))
            {
                if (auto* graphProc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor()))
                {
                    graphProc->setNodeFilePath (targetNodeID, file.getFullPathName());
                    
                    // Update display
                    updateDisplayForFilePath (*activePedal->design, activePedal->controlTexts, targetNodeID, file.getFullPathName());
                    
                    // Save to effectsGraph JSON in design
                    activePedal->design->effectsGraph = graphProc->getDSPGraph().toJSON();
                    
                    engine.saveUndoState();
                    grid.repaint();
                    refreshAfterUndoRedo();
                }
            }
        }
    };
    
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
    removeKeyListener (this);
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
    
   #if JucePlugin_Build_Standalone
    btnSettings.setBounds (toolbar.removeFromLeft (80).reduced (4, 6));
   #endif
    btnTestSound.setBounds (toolbar.removeFromLeft (90).reduced (4, 6));
   
    // Tabs (right-aligned)
    tabStore.setBounds   (toolbar.removeFromRight (80).reduced (4, 6));
    tabLibrary.setBounds (toolbar.removeFromRight (80).reduced (4, 6));
    tabMidi.setBounds    (toolbar.removeFromRight (60).reduced (4, 6));
    tabWiki.setBounds    (toolbar.removeFromRight (60).reduced (4, 6));
    tabScript.setBounds  (toolbar.removeFromRight (70).reduced (4, 6));
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
    scriptingTab.setBounds (contentBounds);
    wikiTab.setBounds (contentBounds);
    
    inventory.setBounds (getLocalBounds());
    libraryOverlay.setBounds (getLocalBounds());
    canvasOverlay.setBounds (getLocalBounds());
}

//==============================================================================
void PedalForgeEditor::buttonClicked (juce::Button* button)
{
   #if JucePlugin_Build_Standalone
    if (button == &btnSettings)
    {
        OpenStandaloneAudioSettingsDialog();
        return;
    }
   #endif

    if (button == &btnTestSound)
    {
        processorRef.setTestSoundActive (btnTestSound.getToggleState());
        return;
    }

    // Only handle tab buttons
    if (button != &tabPlay && button != &tabBoard && button != &tabRoute && button != &tabPedal
        && button != &tabFX && button != &tabScript && button != &tabWiki && button != &tabLibrary
        && button != &tabStore && button != &tabMidi)
        return;

    bool wasPedal   = pedalDesigner.isVisible();
    bool wasFX      = nodeGraphEditor.isVisible();

    bool isPlay     = tabPlay.getToggleState();
    bool isBoard    = tabBoard.getToggleState();
    bool isRoute    = tabRoute.getToggleState();
    bool isPedal    = tabPedal.getToggleState();
    bool isFX       = tabFX.getToggleState();
    bool isScript   = tabScript.getToggleState();
    bool isWiki     = tabWiki.getToggleState();
    bool isLibrary  = tabLibrary.getToggleState();
    bool isMidi     = tabMidi.getToggleState();

    // ── Save state when LEAVING a tab ───────────────────────────────
    if (wasPedal && activePedal != nullptr && activePedal->design != nullptr)
    {
        auto updatedDesign = pedalDesigner.getDesign();
        if (juce::JSON::toString (updatedDesign.toJSON()) != juce::JSON::toString (activePedal->design->toJSON()))
        {
            *(activePedal->design) = updatedDesign;
            grid.refreshSelectedPedal();
            processorRef.getGraphEngine().saveUndoState();
        }
    }

    if (wasFX && activePedal != nullptr && activePedal->design != nullptr)
    {
        auto newGraph = nodeGraphEditor.getGraph().toJSON();
        auto newNotes = nodeGraphEditor.getNotes();

        bool graphChanged = (juce::JSON::toString (newGraph) != juce::JSON::toString (activePedal->design->effectsGraph));
        bool notesChanged = (juce::JSON::toString (StickyNoteData::toJSON (newNotes)) != juce::JSON::toString (StickyNoteData::toJSON (activePedal->design->fxNotes)));

        if (graphChanged || notesChanged)
        {
            activePedal->design->effectsGraph = newGraph;
            activePedal->design->fxNotes = newNotes;

            // Rebuild processor so changes actually affect audio and parameters
            auto newProc = std::make_unique<GraphPedalProcessor> (activePedal->name, juce::JSON::toString (activePedal->design->effectsGraph));
            processorRef.getGraphEngine().updatePedalProcessor (activePedal->nodeID, std::move (newProc));
            processorRef.getGraphEngine().saveUndoState();
        }
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
    {
        nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);
        pedalDesigner.loadDesign (*activePedal->design);
    }

    nodeGraphEditor.setVisible (isFX);
    if (isFX && activePedal != nullptr && activePedal->design != nullptr)
    {
        nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);
        nodeGraphEditor.loadNotes (activePedal->design->fxNotes);
    }

    scriptingTab.setVisible (isScript);
    if (isScript)
        scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());

    wikiTab.setVisible (isWiki);

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
            grid.addPedalAtGrid (itemID, -1.0f, -1.0f);
            inventory.hide();
        };
    }
    else if (isRoute)
    {
        inventory.setContext (InventoryOverlay::Context::Route);
        inventory.onPedalClicked = [this] (const juce::String& itemID) {
            // Add pedal to graph at arbitrary position
            grid.addPedalAtGrid (itemID, -1.0f, -1.0f);
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

//==============================================================================
bool PedalForgeEditor::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    (void) originatingComponent;

    // Check if keyboard focus is currently held by a text entry element.
    // If so, let them handle local undo/redo instead of triggering global undo/redo.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
    {
        if (dynamic_cast<juce::TextEditor*> (focused) != nullptr
            || dynamic_cast<juce::CodeEditorComponent*> (focused) != nullptr)
        {
            return false;
        }
    }

    bool isModDown = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    if (isModDown)
    {
        int code = key.getKeyCode();
        if (code == 'Z' || code == 'z')
        {
            if (key.getModifiers().isShiftDown())
                triggerRedo();
            else
                triggerUndo();
            return true;
        }
        else if (code == 'Y' || code == 'y')
        {
            triggerRedo();
            return true;
        }
    }
    return false;
}

void PedalForgeEditor::commitActiveTabState()
{
    if (pedalDesigner.isVisible() && activePedal != nullptr && activePedal->design != nullptr)
    {
        auto updatedDesign = pedalDesigner.getDesign();
        if (juce::JSON::toString (updatedDesign.toJSON()) != juce::JSON::toString (activePedal->design->toJSON()))
        {
            *(activePedal->design) = updatedDesign;
            grid.refreshSelectedPedal();
        }
    }
    if (nodeGraphEditor.isVisible() && activePedal != nullptr && activePedal->design != nullptr)
    {
        auto newGraph = nodeGraphEditor.getGraph().toJSON();
        auto newNotes = nodeGraphEditor.getNotes();

        bool graphChanged = (juce::JSON::toString (newGraph) != juce::JSON::toString (activePedal->design->effectsGraph));
        bool notesChanged = (juce::JSON::toString (StickyNoteData::toJSON (newNotes)) != juce::JSON::toString (StickyNoteData::toJSON (activePedal->design->fxNotes)));

        if (graphChanged || notesChanged)
        {
            activePedal->design->effectsGraph = newGraph;
            activePedal->design->fxNotes = newNotes;

            auto newProc = std::make_unique<GraphPedalProcessor> (activePedal->name, juce::JSON::toString (activePedal->design->effectsGraph));
            processorRef.getGraphEngine().updatePedalProcessor (activePedal->nodeID, std::move (newProc));
        }
    }
}

void PedalForgeEditor::triggerUndo()
{
    commitActiveTabState();
    if (processorRef.getGraphEngine().undo())
        refreshAfterUndoRedo();
}

void PedalForgeEditor::triggerRedo()
{
    commitActiveTabState();
    if (processorRef.getGraphEngine().redo())
        refreshAfterUndoRedo();
}

void PedalForgeEditor::refreshAfterUndoRedo()
{
    if (activePedal != nullptr)
    {
        auto oldId = activePedal->nodeID;
        activePedal = processorRef.getGraphEngine().getPedalInstance (oldId);
    }

    if (tabPlay.getToggleState())
    {
        if (playTab != nullptr) playTab->rebuildSlots();
    }
    else if (tabBoard.getToggleState())
    {
        grid.rebuildFromEngine();
    }
    else if (tabRoute.getToggleState())
    {
        if (routingEditor != nullptr) routingEditor->syncFromEngine();
    }
    else if (tabPedal.getToggleState())
    {
        if (activePedal != nullptr && activePedal->design != nullptr)
        {
            nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);
            pedalDesigner.loadDesign (*activePedal->design);
        }
    }
    else if (tabFX.getToggleState())
    {
        if (activePedal != nullptr && activePedal->design != nullptr)
        {
            nodeGraphEditor.loadDesign (activePedal->design->effectsGraph);
            nodeGraphEditor.loadNotes (activePedal->design->fxNotes);
        }
    }
    else if (tabScript.getToggleState())
    {
        scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());
    }
    repaint();
}



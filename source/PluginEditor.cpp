#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/PedalDesign.h"
#include "dsp/ControlSurfaceSync.h"
#include "pedals/PedalRegistry.h"
#include "ui/PlayTabComponent.h"
#include "peripherals/displays/modes/MidiMonitorMode.h"
#include "ai/AudioProbe.h"
#include <set>

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

    // Tabs — all in one radio group. The Store tab is hidden until §6
    // ships content; without backend or content it's a dead click that
    // confuses new users.
    for (auto* tab : { &tabPlay, &tabBoard, &tabRoute, &tabPedal, &tabFX, &tabScript, &tabWiki, &tabLibrary, &tabMidi })
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
    scriptingTab.onOpenWiki = [this] (const juce::String& pageId) {
        tabWiki.setToggleState (true, juce::sendNotification);
        wikiTab.navigateTo (pageId);
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
            // Auto-create / auto-remove pedal-face controls for control-surface
            // nodes in the graph (Knob, Fader, Toggle, etc.). Must run before
            // the design is serialised so the new controls travel with it.
            syncControlSurfaceNodes (*activePedal->design, nodeGraphEditor.getGraph());

            activePedal->design->effectsGraph = nodeGraphEditor.getGraph().toJSON();
            auto newProc = std::make_unique<GraphPedalProcessor> (activePedal->name, juce::JSON::toString (activePedal->design->effectsGraph));
            processorRef.getGraphEngine().updatePedalProcessor (activePedal->nodeID, std::move (newProc));
            processorRef.getGraphEngine().saveUndoState();

            // Re-render the pedal face / refresh detail panel since controls may have changed
            grid.refreshSelectedPedal();
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

    // New display subsystem. Attempts to open the Turing 3.5" V2 on
    // startup; reconnects on hot-plug. MIDI monitor is the default mode
    // until the per-display mode picker UI ships.
    displayManager = std::make_unique<DisplayManager> (processorRef.getGraphEngine());

    auto turing = std::make_unique<TuringDisplay>();
    const bool turingOpened = turing->startConnection();
    juce::ignoreUnused (turingOpened);
    const juce::String turingID = turing->getDisplayID();
    displayManager->attachDisplay (std::move (turing));

    displayManager->registerMode (std::make_unique<MidiMonitorMode>());
    displayManager->setActiveMode (turingID, "midi_monitor");

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
    // Toast overlay is always visible (it just paints nothing when empty)
    // and sits above every other tab so messages aren't obscured.
    addAndMakeVisible (toastOverlay);

    // Always-visible audio status strip at the bottom.
    audioStatusBar = std::make_unique<AudioStatusBar> (processorRef);
    addAndMakeVisible (*audioStatusBar);

    // AI assistant panel — sits above the status bar. Re-lay-out the editor
    // when it expands/collapses so the active tab shrinks to make room.
    addAndMakeVisible (aiPanel);
    aiPanel.onExpandedChanged = [this] { resized(); };

    // Crash recovery (#52): if a recovery file is sitting around it means
    // the previous run didn't shut down cleanly. Offer to restore.
    // Deferred so the editor finishes constructing before the modal pops.
    if (processorRef.hasPendingRecovery())
    {
        juce::MessageManager::callAsync ([this]
        {
            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::WarningIcon,
                "Recover unsaved work?",
                "PedalForge didn't shut down cleanly last time. A recovery file was found. "
                "Restore your last autosaved state?",
                "Restore",
                "Discard",
                this,
                juce::ModalCallbackFunction::create ([this] (int result)
                {
                    if (result == 1)
                    {
                        auto state = processorRef.loadRecoveryState();
                        if (state.isNotEmpty())
                        {
                            processorRef.getPresetManager().restoreState (state);
                            pf::toastInfo ("Restored last autosaved state.");
                        }
                    }
                    // Either way, clear the file so we don't prompt again next launch.
                    processorRef.clearRecoveryFile();
                }));
        });
    }
    
    canvasOverlay.onOpenLibrary = [this] (const juce::String& category, std::function<void(const juce::File&)> cb)
    {
        activeFileCallback = cb;
        libraryOverlay.showForCategory(category);
    };
    
    libraryView.onAssetSelected = [this] (const juce::File& file)
    {
        juce::String ext = file.getFileExtension().toLowerCase();

        // .pfboard — replace the current board with the one in the file.
        if (ext == ".pfboard")
        {
            juce::Component::SafePointer<PedalForgeEditor> sp (this);
            juce::File boardFile = file;
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                "Load Board?",
                "Loading \"" + boardFile.getFileNameWithoutExtension()
                    + "\" will replace your current pedalboard.\n\nContinue?",
                "Load", "Cancel", nullptr,
                juce::ModalCallbackFunction::create ([sp, boardFile] (int r)
                {
                    if (sp == nullptr || r == 0) return;
                    auto json = boardFile.loadFileAsString();
                    if (json.isEmpty()) return;
                    sp->processorRef.getGraphEngine().deserialise (json);
                    sp->grid.rebuildFromEngine();
                    sp->inventory.refresh();
                    sp->refreshAfterUndoRedo();
                }));
            return;
        }

        if (activePedal == nullptr || activePedal->design == nullptr)
            return;

        // Determine category based on file extension
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

bool PedalForgeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file (f);
        if (file.hasFileExtension ("pfpedal") || file.hasFileExtension ("pfboard"))
            return true;
    }
    return false;
}

void PedalForgeEditor::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    juce::StringArray importedPedals, failedPedals;
    juce::File boardFileToLoad;

    for (const auto& f : files)
    {
        juce::File src (f);
        if (src.hasFileExtension ("pfpedal"))
        {
            auto dest = importPedalDesignFile (src);
            if (dest != juce::File())
                importedPedals.add (dest.getFileNameWithoutExtension());
            else
                failedPedals.add (src.getFileName());
        }
        else if (src.hasFileExtension ("pfboard"))
        {
            // Only act on the first board file dropped; multi-board drop makes
            // no sense since each one would replace the previous.
            if (boardFileToLoad == juce::File())
                boardFileToLoad = src;
        }
    }

    if (! importedPedals.isEmpty())
    {
        inventory.refresh();
        juce::String msg = "Imported " + juce::String (importedPedals.size())
                         + " pedal" + (importedPedals.size() == 1 ? "" : "s") + ":\n"
                         + importedPedals.joinIntoString ("\n");
        if (! failedPedals.isEmpty())
            msg += "\n\nFailed:\n" + failedPedals.joinIntoString ("\n");
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                 "Pedal Import", msg);
    }
    else if (! failedPedals.isEmpty() && boardFileToLoad == juce::File())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Pedal Import Failed",
            "Could not import:\n" + failedPedals.joinIntoString ("\n")
            + "\n\nThe file(s) may not be valid PedalForge designs.");
    }

    if (boardFileToLoad != juce::File())
    {
        juce::Component::SafePointer<PedalForgeEditor> sp (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
            "Load Board?",
            "Loading \"" + boardFileToLoad.getFileNameWithoutExtension()
                + "\" will replace your current pedalboard.\n\nContinue?",
            "Load", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([sp, boardFileToLoad] (int r)
            {
                if (sp == nullptr || r == 0) return;

                auto json = boardFileToLoad.loadFileAsString();
                if (json.isEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                        "Board Import Failed", "Could not read:\n" + boardFileToLoad.getFullPathName());
                    return;
                }

                // Persist a copy in ~/Library/PedalForge/boards/ so it shows up
                // in the Library Boards category for later reuse.
                importBoardFile (boardFileToLoad);

                sp->processorRef.getGraphEngine().deserialise (json);
                sp->grid.rebuildFromEngine();
                sp->inventory.refresh();
                sp->refreshAfterUndoRedo();
            }));
    }
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
    // Store tab hidden until §6 ships content (see PluginEditor constructor).
    // tabStore.setBounds (toolbar.removeFromRight (80).reduced (4, 6));
    tabLibrary.setBounds (toolbar.removeFromRight (80).reduced (4, 6));
    tabMidi.setBounds    (toolbar.removeFromRight (60).reduced (4, 6));
    tabWiki.setBounds    (toolbar.removeFromRight (60).reduced (4, 6));
    tabScript.setBounds  (toolbar.removeFromRight (70).reduced (4, 6));
    tabFX.setBounds      (toolbar.removeFromRight (60).reduced (4, 6));
    tabPedal.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabRoute.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabBoard.setBounds   (toolbar.removeFromRight (70).reduced (4, 6));
    tabPlay.setBounds    (toolbar.removeFromRight (70).reduced (4, 6));

    // Reserve a slim strip at the bottom for the audio I/O status bar.
    if (audioStatusBar != nullptr)
        audioStatusBar->setBounds (bounds.removeFromBottom (audioStatusBarHeight));

    // AI assistant panel sits just above the status bar. Collapsed it's a
    // single input row; expanded it claims ~45% of the window height.
    {
        const int panelH = aiPanel.isExpanded()
                               ? juce::jmax (AiAssistantPanel::collapsedHeight + 120,
                                             (int) (getHeight() * 0.45))
                               : AiAssistantPanel::collapsedHeight;
        aiPanel.setBounds (bounds.removeFromBottom (panelH));
    }

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
    toastOverlay.setBounds (getLocalBounds());
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

    // Cmd/Ctrl-K focuses the AI assistant from anywhere (even from a text
    // editor), so it must be handled before the text-focus early-return.
    if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
        && (key.getKeyCode() == 'K' || key.getKeyCode() == 'k'))
    {
        aiPanel.focusInput();
        return true;
    }

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



//==============================================================================
// pf::ai::ToolHost — the AI assistant's window into the live app (#64).
//==============================================================================
namespace
{
    // Find a pedal instance by its design uuid across both engines. Returns
    // the engine it lives in (for writes) via engineOut.
    PedalInstance* findByUuid (PedalForgeProcessor& proc,
                               const juce::String& uuid,
                               AudioGraphEngine** engineOut)
    {
        AudioGraphEngine* engines[] = { &proc.getGraphEngine(), &proc.getPlayGraphEngine() };
        for (auto* eng : engines)
        {
            for (const auto& inst : eng->getPedalInstances())
            {
                if (inst.design != nullptr && inst.design->uuid == uuid)
                {
                    if (engineOut) *engineOut = eng;
                    return eng->getPedalInstance (inst.nodeID);
                }
            }
        }
        if (engineOut) *engineOut = nullptr;
        return nullptr;
    }
}

PedalInstance* PedalForgeEditor::findInstanceByUuid (const juce::String& uuid)
{
    return findByUuid (processorRef, uuid, nullptr);
}

juce::String PedalForgeEditor::readActiveTab()
{
    juce::String tab = "Play";
    if      (tabBoard.getToggleState())   tab = "Board";
    else if (tabRoute.getToggleState())   tab = "Route";
    else if (tabPedal.getToggleState())   tab = "Pedal Designer";
    else if (tabFX.getToggleState())      tab = "FX (DSP node graph)";
    else if (tabScript.getToggleState())  tab = "Scripting";
    else if (tabWiki.getToggleState())    tab = "Wiki";
    else if (tabLibrary.getToggleState()) tab = "Library";
    else if (tabMidi.getToggleState())    tab = "MIDI";

    auto* root = new juce::DynamicObject();
    root->setProperty ("activeTab", tab);
    if (activePedal != nullptr && activePedal->design != nullptr)
    {
        root->setProperty ("focusedPedalName", activePedal->name);
        root->setProperty ("focusedPedalUuid", activePedal->design->uuid);
    }
    return juce::JSON::toString (juce::var (root));
}

juce::String PedalForgeEditor::listPedals()
{
    juce::Array<juce::var> arr;
    AudioGraphEngine* engines[] = { &processorRef.getGraphEngine(), &processorRef.getPlayGraphEngine() };
    juce::StringArray seenUuids;
    for (auto* eng : engines)
    {
        for (const auto& inst : eng->getPedalInstances())
        {
            if (inst.design == nullptr) continue;
            if (seenUuids.contains (inst.design->uuid)) continue;
            seenUuids.add (inst.design->uuid);
            auto* o = new juce::DynamicObject();
            o->setProperty ("uuid", inst.design->uuid);
            o->setProperty ("name", inst.name);
            o->setProperty ("board", inst.boardId);
            arr.add (juce::var (o));
        }
    }
    return juce::JSON::toString (juce::var (arr));
}

juce::String PedalForgeEditor::readPedalDesign (const juce::String& uuid)
{
    if (auto* inst = findInstanceByUuid (uuid))
        if (inst->design != nullptr)
            return juce::JSON::toString (inst->design->toJSON());
    return {};
}

bool PedalForgeEditor::writePedalDesign (const juce::String& uuid,
                                         const juce::String& json,
                                         juce::String& errorOut)
{
    AudioGraphEngine* eng = nullptr;
    auto* inst = findByUuid (processorRef, uuid, &eng);
    if (inst == nullptr || inst->design == nullptr || eng == nullptr)
    {
        errorOut = "No pedal with uuid " + uuid;
        return false;
    }

    auto parsed = juce::JSON::parse (json);
    if (! parsed.isObject())
    {
        errorOut = "JSON did not parse to an object";
        return false;
    }

    eng->saveUndoState();

    auto newDesign = PedalDesign::fromJSON (parsed);
    newDesign.uuid = uuid;   // identity is immutable — never let the agent change it
    *(inst->design) = newDesign;

    // Rebuild the processor so any FX-graph changes carried in the design
    // become audible too.
    auto newProc = std::make_unique<GraphPedalProcessor> (
        inst->name, juce::JSON::toString (inst->design->effectsGraph));
    eng->updatePedalProcessor (inst->nodeID, std::move (newProc));

    grid.refreshSelectedPedal();
    grid.repaint();
    return true;
}

juce::String PedalForgeEditor::readFxGraph (const juce::String& pedalUuid)
{
    if (auto* inst = findInstanceByUuid (pedalUuid))
        if (inst->design != nullptr)
            return juce::JSON::toString (inst->design->effectsGraph);
    return {};
}

bool PedalForgeEditor::writeFxGraph (const juce::String& pedalUuid,
                                     const juce::String& json,
                                     juce::String& errorOut)
{
    AudioGraphEngine* eng = nullptr;
    auto* inst = findByUuid (processorRef, pedalUuid, &eng);
    if (inst == nullptr || inst->design == nullptr || eng == nullptr)
    {
        errorOut = "No pedal with uuid " + pedalUuid;
        return false;
    }

    auto parsed = juce::JSON::parse (json);
    if (! parsed.isObject() && ! parsed.isArray())
    {
        errorOut = "FX graph JSON did not parse";
        return false;
    }

    eng->saveUndoState();
    inst->design->effectsGraph = parsed;

    auto newProc = std::make_unique<GraphPedalProcessor> (
        inst->name, juce::JSON::toString (parsed));
    eng->updatePedalProcessor (inst->nodeID, std::move (newProc));

    // If this is the pedal currently open in the FX editor, refresh it.
    if (activePedal == inst)
        nodeGraphEditor.loadDesign (inst->design->effectsGraph);

    grid.refreshSelectedPedal();
    grid.repaint();
    return true;
}

void PedalForgeEditor::showToast (const juce::String& message)
{
    pf::toastInfo (message);
}

juce::String PedalForgeEditor::listFactoryPedals()
{
    juce::Array<juce::var> arr;
    for (const auto& info : getFactoryPedals())
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", info.factoryID());
        o->setProperty ("name", info.name);
        o->setProperty ("category", info.category);
        arr.add (juce::var (o));
    }
    return juce::JSON::toString (juce::var (arr));
}

juce::String PedalForgeEditor::addPedalToBoard (const juce::String& pedalId, juce::String& errorOut)
{
    // Snapshot existing design uuids so we can identify the new instance.
    juce::StringArray before;
    for (const auto& inst : processorRef.getGraphEngine().getPedalInstances())
        if (inst.design != nullptr) before.add (inst.design->uuid);

    // Reuse the same path the inventory drag-drop uses: auto-place (-1,-1),
    // which also auto-routes and saves undo state internally.
    grid.addPedalAtGrid (pedalId, -1.0f, -1.0f);

    // Find the newly added instance (uuid not present before).
    for (const auto& inst : processorRef.getGraphEngine().getPedalInstances())
    {
        if (inst.design == nullptr) continue;
        if (before.contains (inst.design->uuid)) continue;
        auto* o = new juce::DynamicObject();
        o->setProperty ("uuid", inst.design->uuid);
        o->setProperty ("name", inst.name);
        return juce::JSON::toString (juce::var (o));
    }

    errorOut = "Unknown pedal id '" + pedalId + "'. Call list_factory_pedals for valid ids.";
    return {};
}

juce::String PedalForgeEditor::createBlankPedal (const juce::String& name, juce::String& errorOut)
{
    // A fresh custom pedal = a minimal tutorial pedal (passthrough graph +
    // empty-ish design) that the agent then reshapes via pedal/fx scripts.
    juce::StringArray before;
    for (const auto& inst : processorRef.getGraphEngine().getPedalInstances())
        if (inst.design != nullptr) before.add (inst.design->uuid);

    grid.addPedalAtGrid ("factory:hello_gain", -1.0f, -1.0f);

    for (auto& inst : processorRef.getGraphEngine().getPedalInstances())
    {
        if (inst.design == nullptr) continue;
        if (before.contains (inst.design->uuid)) continue;
        // Found the new pedal — rename it if requested. (We mutate through a
        // const-ref's shared_ptr, which is allowed; see findByUuid note.)
        if (name.isNotEmpty())
        {
            auto* mut = processorRef.getGraphEngine().getPedalInstance (inst.nodeID);
            if (mut != nullptr)
            {
                mut->name = name;
                if (mut->design != nullptr) mut->design->name = name;
            }
        }
        grid.refreshSelectedPedal();
        grid.repaint();
        auto* o = new juce::DynamicObject();
        o->setProperty ("uuid", inst.design->uuid);
        o->setProperty ("name", name.isNotEmpty() ? name : inst.name);
        return juce::JSON::toString (juce::var (o));
    }

    errorOut = "Could not create a new pedal.";
    return {};
}

//==============================================================================
// Scripting-engine tools (#65). Each routes through the ScriptingTabComponent's
// existing compile logic via its headless entry points. Pedal-scoped scripts
// set the scripting tab's active pedal to the target first, then restore the
// editor's current selection afterward so the visible tab isn't left desynced.
//==============================================================================
juce::String PedalForgeEditor::getScriptApiReference()
{
    return ScriptingTabComponent::getApiReference();
}

juce::String PedalForgeEditor::runBoardScript (const juce::String& source)
{
    // Board scripts CLEAR and rebuild the board — every existing PedalInstance
    // is freed. So any pointer we hold into the old instances (activePedal,
    // the scripting tab's active pedal, the grid selection) is about to
    // dangle. Drop them BEFORE running, and do NOT restore activePedal
    // afterward (it would be a use-after-free).
    activePedal = nullptr;
    grid.deselectAll();
    scriptingTab.setActivePedal (nullptr, &processorRef.getGraphEngine());

    auto out = scriptingTab.runScriptHeadless (ScriptingTabComponent::ScriptMode::Board, source);

    grid.rebuildFromEngine();
    grid.repaint();
    return out;
}

juce::String PedalForgeEditor::runPedalScript (const juce::String& pedalUuid, const juce::String& source)
{
    auto* inst = findInstanceByUuid (pedalUuid);
    if (inst == nullptr) return "ERROR: no pedal with uuid " + pedalUuid;
    scriptingTab.setActivePedal (inst, &processorRef.getGraphEngine());
    auto out = scriptingTab.runScriptHeadless (ScriptingTabComponent::ScriptMode::Pedal, source);
    scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());
    grid.refreshSelectedPedal();
    grid.repaint();
    return out;
}

juce::String PedalForgeEditor::runFxScript (const juce::String& pedalUuid, const juce::String& source)
{
    auto* inst = findInstanceByUuid (pedalUuid);
    if (inst == nullptr) return "ERROR: no pedal with uuid " + pedalUuid;
    scriptingTab.setActivePedal (inst, &processorRef.getGraphEngine());
    auto out = scriptingTab.runScriptHeadless (ScriptingTabComponent::ScriptMode::GraphBuilder, source);
    scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());
    grid.refreshSelectedPedal();
    grid.repaint();
    return out;
}

juce::String PedalForgeEditor::runDspScript (const juce::String& pedalUuid, const juce::String& source)
{
    auto* inst = findInstanceByUuid (pedalUuid);
    if (inst == nullptr) return "ERROR: no pedal with uuid " + pedalUuid;
    scriptingTab.setActivePedal (inst, &processorRef.getGraphEngine());
    auto out = scriptingTab.runScriptHeadless (ScriptingTabComponent::ScriptMode::DSP, source);
    scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());
    grid.refreshSelectedPedal();
    grid.repaint();
    return out;
}

juce::String PedalForgeEditor::readBoardAsScript()
{
    scriptingTab.setActivePedal (nullptr, &processorRef.getGraphEngine());
    auto s = scriptingTab.emitScript (ScriptingTabComponent::ScriptMode::Board);
    scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());
    return s;
}

juce::String PedalForgeEditor::readPedalAsScript (const juce::String& pedalUuid)
{
    auto* inst = findInstanceByUuid (pedalUuid);
    if (inst == nullptr) return "ERROR: no pedal with uuid " + pedalUuid;
    scriptingTab.setActivePedal (inst, &processorRef.getGraphEngine());
    auto s = scriptingTab.emitScript (ScriptingTabComponent::ScriptMode::Pedal);
    scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());
    return s;
}

juce::String PedalForgeEditor::readFxAsScript (const juce::String& pedalUuid)
{
    auto* inst = findInstanceByUuid (pedalUuid);
    if (inst == nullptr) return "ERROR: no pedal with uuid " + pedalUuid;
    scriptingTab.setActivePedal (inst, &processorRef.getGraphEngine());
    auto s = scriptingTab.emitScript (ScriptingTabComponent::ScriptMode::GraphBuilder);
    scriptingTab.setActivePedal (activePedal, &processorRef.getGraphEngine());
    return s;
}

juce::String PedalForgeEditor::verifyPedal (const juce::String& pedalUuid)
{
    auto* inst = findInstanceByUuid (pedalUuid);
    if (inst == nullptr) return "ERROR: no pedal with uuid " + pedalUuid;

    auto* node = processorRef.getGraphEngine().getGraph().getNodeForId (inst->nodeID);
    auto* gproc = (node != nullptr) ? dynamic_cast<GraphPedalProcessor*> (node->getProcessor()) : nullptr;
    if (gproc == nullptr) return "ERROR: pedal '" + inst->name + "' has no DSP graph.";

    auto& dsp = gproc->getDSPGraph();
    const auto& nodes = dsp.getNodes();
    const auto& conns = dsp.getConnections();

    juce::String r;
    r << "Pedal \"" << inst->name << "\" DSP graph:\n";
    r << "Nodes (" << (int) nodes.size() << "):\n";
    std::vector<int> inputIds, outputIds;
    for (const auto& [id, n] : nodes)
    {
        if (n == nullptr) continue;
        r << "  #" << id << " " << n->getType() << "\n";
        if (n->getType() == "audio_input")  inputIds.push_back (id);
        if (n->getType() == "audio_output") outputIds.push_back (id);
    }

    r << "Connections (" << (int) conns.size() << "):\n";
    for (const auto& c : conns)
        r << "  #" << c.sourceNodeID << ":" << c.sourcePort
          << " -> #" << c.destNodeID << ":" << c.destPort << "\n";

    // Reachability: BFS from any audio_input to any audio_output.
    bool audioPathOk = false;
    if (! inputIds.empty() && ! outputIds.empty())
    {
        std::set<int> visited;
        std::vector<int> frontier = inputIds;
        while (! frontier.empty())
        {
            int cur = frontier.back(); frontier.pop_back();
            if (! visited.insert (cur).second) continue;
            for (auto out : outputIds) if (cur == out) { audioPathOk = true; break; }
            if (audioPathOk) break;
            for (const auto& c : conns)
                if (c.sourceNodeID == cur && ! visited.count (c.destNodeID))
                    frontier.push_back (c.destNodeID);
        }
    }

    // Orphans: nodes with no connection at all.
    juce::StringArray orphans;
    for (const auto& [id, n] : nodes)
    {
        bool used = false;
        for (const auto& c : conns)
            if (c.sourceNodeID == id || c.destNodeID == id) { used = true; break; }
        if (! used && n != nullptr) orphans.add ("#" + juce::String (id) + " " + n->getType());
    }

    r << "\nDIAGNOSIS:\n";
    if (inputIds.empty())  r << "  ! No audio_input node - add one.\n";
    if (outputIds.empty()) r << "  ! No audio_output node - add one.\n";
    r << (audioPathOk ? "  OK: audio flows audio_input -> audio_output.\n"
                      : "  ! BROKEN: no connected path from audio_input to audio_output. "
                        "Audio will be SILENT. Check your connect() calls.\n");
    if (! orphans.isEmpty())
        r << "  ! Orphaned (unconnected) nodes: " << orphans.joinIntoString (", ") << "\n";
    return r;
}

juce::String PedalForgeEditor::probePedal (const juce::String& pedalUuid)
{
    auto* inst = findInstanceByUuid (pedalUuid);
    if (inst == nullptr) return "ERROR: no pedal with uuid " + pedalUuid;

    auto* node = processorRef.getGraphEngine().getGraph().getNodeForId (inst->nodeID);
    auto* gproc = (node != nullptr) ? dynamic_cast<GraphPedalProcessor*> (node->getProcessor()) : nullptr;
    if (gproc == nullptr) return "ERROR: pedal '" + inst->name + "' has no DSP graph.";

    // Snapshot the live graph (atomic param reads + immutable-from-audio-thread
    // structure), then run the probe on a FRESH offline clone so the audio
    // thread is never touched. See AudioProbe.h.
    const auto json = gproc->saveGraph();
    return pf::ai::probeAudio (inst->name, json);
}

juce::String PedalForgeEditor::captureView (const juce::String& target)
{
    // MVP: snapshot the whole editor (exactly what the user currently sees on
    // the active tab). `target` is accepted for forward-compat (board / pedal:
    // <uuid>) but currently always renders the live editor view.
    juce::ignoreUnused (target);

    auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return {};

    auto img = createComponentSnapshot (bounds, true);   // renders all children
    if (! img.isValid()) return {};

    // Cap the longest edge so the base64 payload + vision token cost stay sane.
    const int maxEdge = 1400;
    const int w = img.getWidth(), h = img.getHeight();
    if (juce::jmax (w, h) > maxEdge)
    {
        const float scale = (float) maxEdge / (float) juce::jmax (w, h);
        img = img.rescaled (juce::roundToInt (w * scale), juce::roundToInt (h * scale),
                            juce::Graphics::highResamplingQuality);
    }

    juce::PNGImageFormat png;
    juce::MemoryOutputStream mos;
    if (! png.writeImageToStream (img, mos)) return {};
    return juce::Base64::toBase64 (mos.getData(), mos.getDataSize());
}

//==============================================================================
// PLAY TAB tools — the live performance rig (separate engine + presets).
juce::String PedalForgeEditor::listPlayPresets()
{
    if (playTab == nullptr) return "ERROR: Play tab unavailable.";
    return "Play-tab tone presets (built-in + saved):\n  "
           + playTab->getPresetNames().joinIntoString ("\n  ")
           + "\n(load one with load_play_preset, or build a chain with play_add_pedal.)";
}

juce::String PedalForgeEditor::loadPlayPreset (const juce::String& name)
{
    if (playTab == nullptr) return "ERROR: Play tab unavailable.";
    if (! playTab->getPresetNames().contains (name))
        return "No play preset named '" + name + "'. Available: "
               + playTab->getPresetNames().joinIntoString (", ");
    playTab->loadPreset (name);
    return "Loaded play preset '" + name + "'.\n" + playTab->describeChain();
}

juce::String PedalForgeEditor::readPlayChain()
{
    if (playTab == nullptr) return "ERROR: Play tab unavailable.";
    return playTab->describeChain();
}

juce::String PedalForgeEditor::playAddPedal (const juce::String& pedalName)
{
    if (playTab == nullptr) return "ERROR: Play tab unavailable.";
    if (! playTab->addPedalToChain (pedalName))
        return "Could not add '" + pedalName + "' to the play chain - not a known "
               "factory or saved pedal. Call list_factory_pedals for valid names.";
    return "Added '" + pedalName + "' to the play chain.\n" + playTab->describeChain();
}

juce::String PedalForgeEditor::playClear()
{
    if (playTab == nullptr) return "ERROR: Play tab unavailable.";
    playTab->clearChain();
    return "Cleared the play chain.";
}

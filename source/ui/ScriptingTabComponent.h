#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"
#include "../dsp/ExpressionVM.h"
#include "../dsp/DSPGraph.h"
#include "../dsp/ControlSurfaceSync.h"
#include "../pedals/PedalRegistry.h"
#include "ExpressionTokeniser.h"
#include "LookAndFeel.h"
#include "AutocompletePanelComponent.h"

//==============================================================================
/**
 * BlockColoredCodeEditor — A custom JUCE CodeEditorComponent that provides
 * Propeller-style visual code block separation by drawing subtle alternating
 * background colors behind logical sections of code.
 */
class BlockColoredCodeEditor : public juce::CodeEditorComponent,
                               private juce::CodeDocument::Listener
{
public:
    BlockColoredCodeEditor (juce::CodeDocument& doc, juce::CodeTokeniser* tokeniser = nullptr)
        : juce::CodeEditorComponent (doc, tokeniser), document (doc)
    {
        defaultBg = juce::Colour (0xFF0D1117);
        setColour (backgroundColourId, juce::Colours::transparentBlack);
        
        document.addListener (this);
        rebuildBlocks();
    }
    
    ~BlockColoredCodeEditor() override
    {
        document.removeListener (this);
    }
    
    /** Highlight a specific line as containing a compile error.
        Pass line=0 to clear. */
    void setErrorLine (int line, const juce::String& message = {})
    {
        errorLine = line;
        errorMessage = message;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // 1. Base background color
        g.fillAll (defaultBg);

        // 2. Draw block backgrounds for visible lines
        if (! lineToBlockColor.empty())
        {
            int lh = getLineHeight();
            int firstLine = getFirstLineOnScreen();

            auto clip = g.getClipBounds();
            int lastLine = juce::jmin ((int)lineToBlockColor.size() - 1, firstLine + clip.getBottom() / lh + 1);

            int gutterW = 0;
            int contentW = getWidth();

            for (int i = firstLine; i <= lastLine; ++i)
            {
                juce::Colour c = lineToBlockColor[(size_t)i];
                if (c.isTransparent()) continue;

                int y = (i - firstLine) * lh;
                g.setColour (c);
                g.fillRect (gutterW, y, contentW, lh);
            }
        }

        // 3. Draw text and gutter (base class)
        juce::CodeEditorComponent::paint (g);

        // 4. Overlay error-line marker (after the base class so it sits on top)
        if (errorLine > 0)
        {
            int lh = getLineHeight();
            int firstLine = getFirstLineOnScreen();
            int rowOnScreen = (errorLine - 1) - firstLine;
            if (rowOnScreen >= 0 && rowOnScreen * lh < getHeight())
            {
                int y = rowOnScreen * lh;
                g.setColour (juce::Colour (0xFFEF4444).withAlpha (0.18f));
                g.fillRect (0, y, getWidth(), lh);
                g.setColour (juce::Colour (0xFFEF4444));
                g.fillRect (0, y, 3, lh);
            }
        }
    }

private:
    juce::CodeDocument& document;
    juce::Colour defaultBg;
    std::vector<juce::Colour> lineToBlockColor;
    int errorLine = 0;
    juce::String errorMessage;
    
    // Subtle muted palette for separating blocks
    const juce::Colour palette[5] = {
        juce::Colour (0xFF0F1218), // deep navy
        juce::Colour (0xFF0F1614), // deep teal
        juce::Colour (0xFF14101A), // deep plum
        juce::Colour (0xFF101416), // deep slate
        juce::Colour (0xFF16120F)  // deep amber
    };
    
    void rebuildBlocks()
    {
        int numLines = document.getNumLines();
        lineToBlockColor.assign ((size_t)numLines, juce::Colours::transparentBlack);
        
        int currentBlockIdx = 0;
        bool inBlock = false;
        
        for (int i = 0; i < numLines; ++i)
        {
            juce::String line = document.getLine (i).trim();
            
            // Separator lines
            if (line.isEmpty() || line.startsWith ("-- ===") || line.startsWith ("// ===") || line.startsWith ("/* ==="))
            {
                if (inBlock)
                {
                    currentBlockIdx++;
                    inBlock = false;
                }
                lineToBlockColor[(size_t)i] = juce::Colours::transparentBlack;
            }
            else
            {
                // Directives or functions start a new block if already in one
                if ((line.startsWith ("@") || line.startsWith ("function ") || line.endsWith ("()")) && inBlock)
                {
                    currentBlockIdx++;
                }
                
                inBlock = true;
                lineToBlockColor[(size_t)i] = palette[currentBlockIdx % 5];
            }
        }
        repaint();
    }
    
    void codeDocumentTextInserted (const juce::String&, int) override { rebuildBlocks(); }
    void codeDocumentTextDeleted (int, int) override { rebuildBlocks(); }
};

//==============================================================================
/**
 * ScriptingTabComponent — A dedicated code workspace tab for PedalForge.
 *
 * Supports three modes:
 * 1. UI Script     — Draw custom UI overlays using ExpressionVM graphics opcodes
 * 2. DSP Expression — Edit ExpressionNode code for custom audio DSP
 * 3. FX Graph Builder — Programmatically construct DSPGraph node chains
 */
class ScriptingTabComponent : public juce::Component,
                              public juce::Button::Listener,
                              public juce::ComboBox::Listener,
                              public juce::KeyListener,
                              private juce::Timer
{
public:
    enum class ScriptMode { UI = 1, DSP = 2, GraphBuilder = 3, Board = 4, Pedal = 5 };

    ScriptingTabComponent () : codeDocument(), codeEditor(codeDocument, &tokeniser)
    {
        setOpaque (true);
        codeEditor.addKeyListener(this);

        // ── Mode Selector ──────────────────────────────────────────────
        modeSelector.addItem ("UI Script",        (int)ScriptMode::UI);
        modeSelector.addItem ("DSP Expression",   (int)ScriptMode::DSP);
        modeSelector.addItem ("FX Graph Builder", (int)ScriptMode::GraphBuilder);
        modeSelector.addItem ("Pedalboard",       (int)ScriptMode::Board);
        modeSelector.addItem ("Pedal Design",     (int)ScriptMode::Pedal);
        modeSelector.setSelectedId ((int)ScriptMode::UI, juce::dontSendNotification);
        modeSelector.addListener (this);
        modeSelector.setColour (juce::ComboBox::backgroundColourId,  juce::Colour (0xFF1F2937));
        modeSelector.setColour (juce::ComboBox::textColourId,        juce::Colour (0xFFE2E8F0));
        modeSelector.setColour (juce::ComboBox::outlineColourId,     juce::Colour (0xFF4B5563));
        addAndMakeVisible (modeSelector);

        // ── Toolbar Buttons ────────────────────────────────────────────
        for (auto* btn : { &btnCompile, &btnNew, &btnSave, &btnLoad, &btnImportGraph, &btnToggleSidebar })
        {
            btn->addListener (this);
            btn->setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF1F2937));
            btn->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible (*btn);
        }
        btnCompile.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF6366F1));

        // ── Script Name Editor ─────────────────────────────────────────
        scriptNameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1F2937));
        scriptNameEditor.setColour (juce::TextEditor::textColourId, juce::Colour (0xFFE2E8F0));
        scriptNameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF4B5563));
        scriptNameEditor.setFont (juce::FontOptions ("Sans", 13.0f, juce::Font::plain));
        scriptNameEditor.setText ("untitled", false);
        addAndMakeVisible (scriptNameEditor);

        // ── Code Editor ────────────────────────────────────────────────
        codeEditor.setColour (juce::CodeEditorComponent::backgroundColourId,  juce::Colour (0xFF0D1117));
        codeEditor.setColour (juce::CodeEditorComponent::defaultTextColourId, juce::Colour (0xFFE2E8F0));
        codeEditor.setColour (juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour (0xFF161B22));
        codeEditor.setColour (juce::CodeEditorComponent::lineNumberTextId, juce::Colour (0xFF6B7280));
        codeEditor.setColour (juce::CaretComponent::caretColourId, juce::Colour (0xFFEC4899));
        codeEditor.setFont (juce::FontOptions ("Menlo", 13.0f, juce::Font::plain));
        codeEditor.setTabSize (4, true);
        addAndMakeVisible (codeEditor);

        // ── Console Output ─────────────────────────────────────────────
        consoleOutput.setMultiLine (true);
        consoleOutput.setReadOnly (true);
        consoleOutput.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF0D1117));
        consoleOutput.setColour (juce::TextEditor::textColourId, juce::Colour (0xFF9CA3AF));
        consoleOutput.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF1F2937));
        consoleOutput.setFont (juce::FontOptions ("Menlo", 11.0f, juce::Font::plain));
        addAndMakeVisible (consoleOutput);

        // ── Status Label ───────────────────────────────────────────────
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF6B7280));
        statusLabel.setFont (juce::FontOptions ("Sans", 11.0f, juce::Font::plain));
        statusLabel.setText ("Ready", juce::dontSendNotification);
        addAndMakeVisible (statusLabel);

        // ── Pedal Info Label ───────────────────────────────────────────
        pedalInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF9CA3AF));
        pedalInfoLabel.setFont (juce::FontOptions ("Sans", 12.0f, juce::Font::italic));
        pedalInfoLabel.setText ("No pedal selected", juce::dontSendNotification);
        pedalInfoLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (pedalInfoLabel);

        // ── Autocomplete Sidebar ───────────────────────────────────────
        addChildComponent (autocompletePanel);
        autocompletePanel.onInsertCompletion = [this] (const juce::String& text) {
            codeEditor.insertTextAtCaret (text);
            codeEditor.grabKeyboardFocus();
        };
        autocompletePanel.onOpenWiki = [this] (const juce::String& link) {
            logToConsole ("Opening Wiki: " + link);
            if (onOpenWiki)
                onOpenWiki (link);
        };

        // ── Inline Popup ───────────────────────────────────────────────
        inlinePopup = std::make_unique<InlineAutocompletePopup>(autocompletePanel.getDatabase());
        addChildComponent (inlinePopup.get());
        
        inlinePopup->onInsertCompletion = [this] (const juce::String& text) {
            juce::String prefix = inlinePopup->getCurrentPrefix();
            if (prefix.isNotEmpty())
            {
                auto pos = codeEditor.getCaretPos();
                int endIndex = pos.getPosition();
                int startIndex = endIndex - prefix.length();
                codeEditor.setHighlightedRegion(juce::Range<int>(startIndex, endIndex));
                codeEditor.insertTextAtCaret("");
            }
            codeEditor.insertTextAtCaret (text);
            codeEditor.grabKeyboardFocus();
        };

        // Load default UI script
        loadDefaultScript (ScriptMode::UI);
    }

    ~ScriptingTabComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    // Tab activation — called from PluginEditor when the Script tab is selected

    void setActivePedal (PedalInstance* inst, AudioGraphEngine* eng)
    {
        activePedal = inst;
        engine = eng;

        if (activePedal)
            pedalInfoLabel.setText ("Pedal: " + activePedal->name, juce::dontSendNotification);
        else
            pedalInfoLabel.setText ("No pedal selected", juce::dontSendNotification);

        // Rebind VM callbacks for the new pedal context
        bindVMCallbacks();
    }

    //==========================================================================
    // Headless entry points for the AI agent (task #65). These reuse the exact
    // same compile/emit logic the Compile button uses; the widgets they touch
    // (console, code editor) exist whether or not the tab is visible, so
    // running them off-screen is safe. Always call on the message thread.

    /** Run a script of the given mode and return the console output (success
        summary or "ERROR line N: …"). For Pedal/DSP/GraphBuilder modes the
        caller must setActivePedal() to the target first. */
    juce::String runScriptHeadless (ScriptMode mode, const juce::String& source)
    {
        consoleOutput.clear();
        currentMode = mode;
        modeSelector.setSelectedId ((int) mode, juce::dontSendNotification);
        autocompletePanel.setDatabaseMode ((int) mode);
        codeDocument.replaceAllContent (source);
        compileAndRun();
        return consoleOutput.getText();
    }

    /** Emit the current live state as a script (round-trip) for the given
        mode, so the agent can read existing state as editable code. */
    juce::String emitScript (ScriptMode mode)
    {
        if      (mode == ScriptMode::Board)        importBoardAsScript();
        else if (mode == ScriptMode::Pedal)        importPedalAsScript();
        else if (mode == ScriptMode::GraphBuilder) importGraphAsScript();
        else return {};
        return codeDocument.getAllContent();
    }

    /** A concise reference of every script command + the ExpressionVM
        function list, for feeding to the AI agent so it writes valid code. */
    static juce::String getApiReference()
    {
        juce::String s;
        s << "PEDALFORGE SCRIPTING API\n"
             "========================\n\n"
             "HOW TO BUILD A BOARD (read this first):\n"
             "  - Pedals added with add_pedal_to_board / create_pedal are "
             "AUTOMATICALLY placed left-to-right and AUTO-WIRED into the signal "
             "chain. You do NOT need a board script to connect them.\n"
             "  - create_pedal and add_pedal_to_board RETURN {uuid,name}. Use "
             "that exact uuid in later calls. NEVER invent a uuid.\n"
             "  - Data dependency: a tool that needs a uuid from create_pedal "
             "must run in a LATER step, not batched in the same reply as the "
             "create_pedal call (the uuid doesn't exist until it returns).\n"
             "  - run_script mode=board CLEARS the whole board and rebuilds it "
             "from its addPedal() calls - it only adds FACTORY pedals and will "
             "DELETE custom pedals. Use it only to build a board entirely from "
             "factory pedals; do NOT use it after create_pedal.\n\n"
             "Typical 'board + custom pedal' flow:\n"
             "  1. add_pedal_to_board for each factory pedal (auto-wired in order)\n"
             "  2. create_pedal(name) -> returns {uuid}\n"
             "  3. run_script mode=pedal (chassis/controls) on that uuid\n"
             "  4. run_script mode=fx (DSP graph) on that uuid\n"
             "  (no board script needed - everything is auto-wired)\n\n"
             "Run scripts with the run_script tool:\n"
             "  run_script(args={ \"mode\":\"board|pedal|fx|dsp\", \"source\":\"<script>\", \"pedal_uuid\":\"<uuid>\" })\n"
             "  - mode \"board\": no pedal_uuid needed (operates on the whole board)\n"
             "  - mode \"pedal\"/\"fx\"/\"dsp\": pedal_uuid REQUIRED (the pedal to act on)\n"
             "  To make a brand-new custom pedal first call create_pedal(args={\"name\":\"...\"})\n"
             "  which returns {uuid,name}; then run_script mode=pedal (chassis/controls) and\n"
             "  mode=fx (DSP graph) against that uuid.\n\n"
             "BOARD script - builds/arranges the pedalboard (cleared first, then rebuilt):\n"
             "  v = addPedal(\"<name or factory id>\")        // add a pedal, returns a handle\n"
             "  addPedal(\"<name>\", x, y)                    // with explicit grid position\n"
             "  connect(src, srcCh, dst, dstCh)              // wire audio: handle, channel ints\n"
             "  setPos(v, x, y)                              // move a pedal\n"
             "  focus(v)                                     // set MIDI-learn focus\n\n"
             "PEDAL script - defines a pedal's chassis + face controls:\n"
             "  setMeta(\"name\",\"author\",\"category\",\"description\")\n"
             "  setChassis(width, height, \"RRGGBB\")\n"
             "  setStyleKit(\"name\")                              // per-pedal StyleKit\n"
             "  setColorway(seedARGB, mode)                         // 0=Semantic, 1=Tint\n"
             "  addKnob(\"id\", x, y, \"label\" [, w, h])\n"
             "  addSwitch / addFootswitch / addLed / addFader / addTextScreen(\"id\", x, y, \"label\")\n"
             "  addControl(\"type\", \"id\", x, y, \"label\" [, w, h])   // any type: selector,\n"
             "                                                      // xypad, joystick, vu_meter,\n"
             "                                                      // 7seg, display, oscilloscope, ...\n"
             "  setStyle(\"id\", \"styleName\")                    // per-control StyleKit override\n"
             "  setGuard(\"id\", n)                                // 0=none,1=cover,2=hold,3=keylock\n"
             "  setShift(\"id\", \"shiftBindingName\")             // page/mode shift binding\n"
             "  setDefault(\"id\", value)                          // per-control default 0.0-1.0\n"
             "  setPositions(\"id\", n)                            // selector/switch position count\n"
             "  mapControl(\"controlId\", \"<nodeID>_<paramID>\")   // bind a face control to a DSP param\n\n"
             "FX script - builds a pedal's DSP node graph (audible). It builds "
             "the COMPLETE graph from scratch each run:\n"
             "  in  = addNode(\"audio_input\")                // YOU must create these two -\n"
             "  out = addNode(\"audio_output\")               // they are NOT pre-existing.\n"
             "  n   = addNode(\"<type>\")                      // gain, softclip, tonestack, delay,\n"
             "                                               // reverb, lfo, oscillator, adsr, expression...\n"
             "  connect(src, srcPort, dst, dstPort)          // ALWAYS reference nodes by the VAR you\n"
             "                                               // assigned (e.g. connect(in,0,n,0)). NEVER\n"
             "                                               // write a bare type like connect(audio_input,...).\n"
             "                                               // ports are 0-based; audio nodes have L=0,R=1.\n"
             "  setParam(n, \"paramName\", value)\n"
             "  Wire a full path in->...->out or the pedal is SILENT. After building, the agent\n"
             "  should call verify_pedal to confirm the audio path connected.\n\n"
             "  EASY DISPLAY (menu screen): add a \"disp_easy\" node, then configure it with\n"
             "    d = addNode(\"disp_easy\")\n"
             "    setScreen(d, {\"kind\":\"easy\",\"grid\":{\"lines\":4,\"cols\":16},\"font\":12,\n"
             "                  \"fg\":\"FFFFFFFF\",\"bg\":\"FF101010\",\"items\":[\n"
             "      {\"id\":\"mix\",\"type\":\"value\",\"label\":\"Mix\",\"port\":\"out\",\"min\":0,\"max\":1,\"value\":0.5,\"fmt\":\"%.0f%%\"},\n"
             "      {\"id\":\"lvl\",\"type\":\"readout\",\"label\":\"In\",\"port\":\"in\",\"fmt\":\"%.1f\"} ]})\n"
             "    Write the JSON UNQUOTED as the 2nd arg. Each item declares its PORT: \"out\"\n"
             "    (value/toggle/trigger/list -> a Control output you wire to a param), \"in\"\n"
             "    (readout -> a Control input reading a node value to display), or \"none\".\n"
             "    The node grows one port per out/in item; wire them with connect().\n\n"
             "DSP script - per-sample expression for an Expression node on the pedal.\n\n"
             "ExpressionVM functions (for DSP/UI expressions):\n";
        s << ExpressionVM::dumpFunctionsAsMarkdown();
        return s;
    }

    void setVisible (bool shouldBeVisible) override
    {
        juce::Component::setVisible (shouldBeVisible);
        if (shouldBeVisible)
            startTimerHz (30);
        else
            stopTimer();
    }

    //==========================================================================
    // Layout

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF0F0F14));

        auto bounds = getLocalBounds();

        // ── Toolbar background ─────────────────────────────────────────
        g.setColour (juce::Colour (0xFF1A1A24));
        g.fillRect (bounds.removeFromTop (toolbarH));
        g.setColour (juce::Colour (0xFF2D2D3D));
        g.drawHorizontalLine (toolbarH - 1, 0.0f, (float) getWidth());

        // ── Status bar background ──────────────────────────────────────
        g.setColour (juce::Colour (0xFF1A1A24));
        g.fillRect (getLocalBounds().removeFromBottom (statusBarH));
        g.setColour (juce::Colour (0xFF2D2D3D));
        g.drawHorizontalLine (getHeight() - statusBarH, 0.0f, (float) getWidth());

        // ── Preview panel header ───────────────────────────────────────
        if (currentMode == ScriptMode::UI)
        {
            auto previewHeader = getPreviewBounds().removeFromTop (28);
            g.setColour (juce::Colour (0xFF161B22));
            g.fillRect (previewHeader);
            g.setColour (juce::Colour (0xFF9CA3AF));
            g.setFont (juce::FontOptions ("Sans", 11.0f, juce::Font::bold));
            g.drawText ("LIVE PREVIEW", previewHeader.reduced (8, 0), juce::Justification::centredLeft);
        }

        // ── Divider between editor and preview ─────────────────────────
        g.setColour (juce::Colour (0xFF2D2D3D));
        float divX = getWidth() * splitRatio;
        if (sidebarVisible) divX -= 250; // shift editor split if sidebar is open
        g.drawVerticalLine ((int) divX, (float) toolbarH, (float) (getHeight() - statusBarH));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // ── Toolbar ────────────────────────────────────────────────────
        auto toolbar = bounds.removeFromTop (toolbarH).reduced (6, 6);
        modeSelector.setBounds (toolbar.removeFromLeft (150));
        toolbar.removeFromLeft (8);
        scriptNameEditor.setBounds (toolbar.removeFromLeft (140));
        toolbar.removeFromLeft (8);
        btnCompile.setBounds (toolbar.removeFromLeft (80));
        toolbar.removeFromLeft (4);
        btnNew.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (4);
        btnSave.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (4);
        btnLoad.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (4);
        btnImportGraph.setBounds (toolbar.removeFromLeft (80));
        toolbar.removeFromLeft (8);
        btnToggleSidebar.setBounds(toolbar.removeFromLeft(80));

        pedalInfoLabel.setBounds (toolbar.removeFromRight (200));

        // ── Status bar ─────────────────────────────────────────────────
        auto statusBar = bounds.removeFromBottom (statusBarH).reduced (8, 0);
        statusLabel.setBounds (statusBar);

        // ── Content area ───────────────────────────────────────────────
        auto content = bounds;
        int editorW = (int) (content.getWidth() * splitRatio);
        if (sidebarVisible) editorW -= 250;

        auto editorArea = content.removeFromLeft (editorW);
        codeEditor.setBounds (editorArea);
        
        if (inlinePopup->isVisible())
        {
            // Position popup slightly below the caret
            // codeEditor.getLinePosition() doesn't exist natively for screen pos, but we can do a rough approx
            // We just let updateInlinePopup handle moving it.
        }

        if (sidebarVisible)
        {
            autocompletePanel.setBounds (content.removeFromLeft (250));
        }

        auto previewArea = content;
        if (currentMode == ScriptMode::UI)
        {
            // Preview canvas on top, console on bottom
            int consoleH = juce::jmax (80, previewArea.getHeight() / 5);
            consoleOutput.setBounds (previewArea.removeFromBottom (consoleH));
            // The preview canvas is painted directly (no child component needed)
        }
        else
        {
            // Full console panel for DSP and Graph modes
            consoleOutput.setBounds (previewArea);
        }
    }

    //==========================================================================
    // Button / ComboBox handlers

    void buttonClicked (juce::Button* button) override
    {
        if (button == &btnCompile)
        {
            compileAndRun();
        }
        else if (button == &btnNew)
        {
            loadDefaultScript (currentMode);
            scriptNameEditor.setText ("untitled", false);
            logToConsole ("New script created.");
        }
        else if (button == &btnSave)
        {
            saveScript();
        }
        else if (button == &btnLoad)
        {
            showLoadScriptMenu();
        }
        else if (button == &btnImportGraph)
        {
            if (currentMode == ScriptMode::Board)
                importBoardAsScript();
            else if (currentMode == ScriptMode::Pedal)
                importPedalAsScript();
            else
                importGraphAsScript();
        }
        else if (button == &btnToggleSidebar)
        {
            sidebarVisible = !sidebarVisible;
            autocompletePanel.setVisible(sidebarVisible);
            resized();
            repaint();
        }
    }
    
    // ── KeyListener ────────────────────────────────────────────────────
    using juce::Component::keyPressed;
    
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override
    {
        if (originatingComponent == &codeEditor)
        {
            if (inlinePopup->handleKeyPress(key))
                return true;
                
            if (key.isKeyCode(juce::KeyPress::backspaceKey) || key.isKeyCode(juce::KeyPress::deleteKey) ||
                 key.getTextCharacter() != 0)
            {
                juce::MessageManager::callAsync([this]() {
                    updateInlinePopup();
                });
            }
            else
            {
                juce::MessageManager::callAsync([this]() {
                    if (!inlinePopup->isVisible()) return;
                    updateInlinePopup();
                });
            }
        }
        return false;
    }
    
    void updateInlinePopup()
    {
        if (!codeEditor.hasKeyboardFocus(false))
        {
            inlinePopup->hide();
            return;
        }
        
        auto pos = codeEditor.getCaretPos();
        juce::String lineText = codeDocument.getLine(pos.getLineNumber());
        int col = pos.getIndexInLine();
        
        // Context-aware caret word tracking
        int wordStart = col;
        while (wordStart > 0 && (juce::CharacterFunctions::isLetterOrDigit(lineText[wordStart - 1]) || lineText[wordStart - 1] == '_'))
        {
            wordStart--;
        }
        int wordEnd = col;
        while (wordEnd < lineText.length() && (juce::CharacterFunctions::isLetterOrDigit(lineText[wordEnd]) || lineText[wordEnd] == '_'))
        {
            wordEnd++;
        }
        
        juce::String caretWord = lineText.substring (wordStart, wordEnd);
        if (caretWord.isNotEmpty() && sidebarVisible)
        {
            autocompletePanel.highlightCompletion (caretWord);
        }
        
        // Inline autocomplete popup prefix tracking
        int startCol = col - 1;
        while (startCol >= 0 && (juce::CharacterFunctions::isLetterOrDigit(lineText[startCol]) || lineText[startCol] == '_'))
        {
            startCol--;
        }
        startCol++;
        
        juce::String prefix = lineText.substring(startCol, col);
        
        if (prefix.length() >= 2)
        {
            inlinePopup->updateFilter(prefix);
            if (inlinePopup->isVisible())
            {
                auto prefixStartPos = pos;
                prefixStartPos.setLineAndIndex (pos.getLineNumber(), startCol);
                
                juce::Rectangle<int> prefixStartBounds = codeEditor.getCharacterBounds (prefixStartPos);
                
                int x = codeEditor.getX() + prefixStartBounds.getX();
                int y = codeEditor.getY() + prefixStartBounds.getBottom();
                
                inlinePopup->showAt (juce::Point<int> (x, y));
            }
        }
        else
        {
            inlinePopup->hide();
        }
    }

    void comboBoxChanged (juce::ComboBox* box) override
    {
        if (box == &modeSelector)
        {
            currentMode = (ScriptMode) modeSelector.getSelectedId();
            autocompletePanel.setDatabaseMode ((int)currentMode);
            loadDefaultScript (currentMode);
            resized();
            repaint();
        }
    }

    //==========================================================================
    // Timer — for live UI preview

    void timerCallback() override
    {
        if (currentMode == ScriptMode::UI)
            repaint();
    }

    //==========================================================================
    // Mouse handling — forwarded to the preview canvas for UI Script mode

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (currentMode == ScriptMode::UI)
        {
            auto canvas = getCanvasBounds();
            if (canvas.contains (e.position))
            {
                isMouseDownInCanvas = true;
                isMouseClickedInCanvas = true;
                lastCanvasMousePos = e.position - canvas.getPosition();
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isMouseDownInCanvas && currentMode == ScriptMode::UI)
        {
            auto canvas = getCanvasBounds();
            lastCanvasMousePos = e.position - canvas.getPosition();
            isMouseDraggedInCanvas = true;
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isMouseDownInCanvas = false;
        isMouseDraggedInCanvas = false;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (currentMode == ScriptMode::UI)
        {
            auto canvas = getCanvasBounds();
            lastCanvasMousePos = e.position - canvas.getPosition();
        }
    }

    //==========================================================================
    std::function<void()> onGraphChanged;
    std::function<void(const juce::String&)> onOpenWiki;

private:
    // ── Layout constants ───────────────────────────────────────────────
    static constexpr int toolbarH   = 40;
    static constexpr int statusBarH = 24;
    float splitRatio = 0.55f;

    // ── State ──────────────────────────────────────────────────────────
    ScriptMode currentMode = ScriptMode::UI;
    PedalInstance* activePedal = nullptr;
    AudioGraphEngine* engine = nullptr;

    // ── UI Components ──────────────────────────────────────────────────
    juce::ComboBox modeSelector;
    juce::TextEditor scriptNameEditor;
    juce::TextButton btnCompile { "Compile" };
    juce::TextButton btnNew { "New" };
    juce::TextButton btnSave { "Save" };
    juce::TextButton btnLoad { "Load" };
    juce::TextButton btnImportGraph { "\xe2\x86\x90 Graph" };  // ← Graph
    juce::TextButton btnToggleSidebar { "API Ref" };
    juce::Label statusLabel;
    juce::Label pedalInfoLabel;

    ExpressionTokeniser tokeniser;
    juce::CodeDocument codeDocument;
    BlockColoredCodeEditor codeEditor;
    juce::TextEditor consoleOutput;
    AutocompletePanelComponent autocompletePanel;
    bool sidebarVisible = false;
    std::unique_ptr<InlineAutocompletePopup> inlinePopup;

    // ── VM for UI Script mode ──────────────────────────────────────────
    ExpressionVM vm;
    bool vmCompiled = false;

    // ── Mouse state for canvas interaction ─────────────────────────────
    juce::Point<float> lastCanvasMousePos;
    bool isMouseDownInCanvas = false;
    bool isMouseClickedInCanvas = false;
    bool isMouseDraggedInCanvas = false;

    //==========================================================================
    // Helper: get the preview area bounds (right of the split)

    juce::Rectangle<float> getPreviewBounds() const
    {
        auto content = getLocalBounds();
        content.removeFromTop (toolbarH);
        content.removeFromBottom (statusBarH);
        int editorW = (int) (content.getWidth() * splitRatio);
        if (sidebarVisible) editorW -= 250;
        content.removeFromLeft (editorW);
        if (sidebarVisible) content.removeFromLeft (250);
        return content.toFloat();
    }

    juce::Rectangle<float> getCanvasBounds() const
    {
        auto preview = getPreviewBounds();
        preview.removeFromTop (28.0f);  // header

        // Remove console area from bottom
        int consoleH = juce::jmax (80, (int) preview.getHeight() / 5);
        preview.removeFromBottom ((float) consoleH);

        return preview.reduced (8.0f);
    }

    //==========================================================================
    // Paint — overridden to render the live preview canvas

    void paintOverChildren (juce::Graphics& g) override
    {
        if (currentMode != ScriptMode::UI || !vmCompiled)
            return;

        auto canvas = getCanvasBounds();

        // Draw canvas background frame
        g.setColour (juce::Colour (0xFF0D1117));
        g.fillRoundedRectangle (canvas, 4.0f);
        g.setColour (juce::Colour (0xFF6366F1).withAlpha (0.3f));
        g.drawRoundedRectangle (canvas, 4.0f, 1.0f);

        // Clip and render VM output
        {
            juce::Graphics::ScopedSaveState saveState (g);
            g.reduceClipRegion (canvas.toNearestInt());
            g.addTransform (juce::AffineTransform::translation (canvas.getX(), canvas.getY()));

            // Populate VM variables
            if (vm.getVarIndex ("w") >= 0)  vm.vars[vm.getVarIndex ("w")] = canvas.getWidth();
            if (vm.getVarIndex ("h") >= 0)  vm.vars[vm.getVarIndex ("h")] = canvas.getHeight();

            if (vm.getVarIndex ("mouse_x") >= 0) vm.vars[vm.getVarIndex ("mouse_x")] = lastCanvasMousePos.x;
            if (vm.getVarIndex ("mouse_y") >= 0) vm.vars[vm.getVarIndex ("mouse_y")] = lastCanvasMousePos.y;
            if (vm.getVarIndex ("mouse_down") >= 0)  vm.vars[vm.getVarIndex ("mouse_down")]  = isMouseDownInCanvas ? 1.0f : 0.0f;
            if (vm.getVarIndex ("mouse_click") >= 0) vm.vars[vm.getVarIndex ("mouse_click")] = isMouseClickedInCanvas ? 1.0f : 0.0f;
            if (vm.getVarIndex ("mouse_drag") >= 0)  vm.vars[vm.getVarIndex ("mouse_drag")]  = isMouseDraggedInCanvas ? 1.0f : 0.0f;
            if (vm.getVarIndex ("selected_track") >= 0) vm.vars[vm.getVarIndex ("selected_track")] = 0.0f;
            if (vm.getVarIndex ("playhead") >= 0) vm.vars[vm.getVarIndex ("playhead")] = 0.0f;
            if (vm.getVarIndex ("bpm") >= 0)  vm.vars[vm.getVarIndex ("bpm")]  = 120.0f;
            if (vm.getVarIndex ("run") >= 0)  vm.vars[vm.getVarIndex ("run")]  = 0.0f;

            float timeVal = (float)(juce::Time::getMillisecondCounter() % 100000) / 1000.0f;
            if (vm.getVarIndex ("time") >= 0) vm.vars[vm.getVarIndex ("time")] = timeVal;
            if (vm.getVarIndex ("t") >= 0)    vm.vars[vm.getVarIndex ("t")]    = timeVal;

            vm.currentGraphics = &g;
            vm.evaluate();

            isMouseClickedInCanvas = false;
        }
    }

    //==========================================================================
    // Compile and run the current script

    void compileAndRun()
    {
        juce::String source = codeDocument.getAllContent();

        if (currentMode == ScriptMode::UI)
        {
            compileUIScript (source);
        }
        else if (currentMode == ScriptMode::DSP)
        {
            compileDSPExpression (source);
        }
        else if (currentMode == ScriptMode::GraphBuilder)
        {
            compileGraphBuilder (source);
        }
        else if (currentMode == ScriptMode::Board)
        {
            compileBoardScript (source);
        }
        else if (currentMode == ScriptMode::Pedal)
        {
            compilePedalScript (source);
        }
    }

    void compileUIScript (const juce::String& source)
    {
        vm.clearVars();
        vm.registerVar ("w");
        vm.registerVar ("h");
        vm.registerVar ("mouse_x");
        vm.registerVar ("mouse_y");
        vm.registerVar ("mouse_down");
        vm.registerVar ("mouse_click");
        vm.registerVar ("mouse_drag");
        vm.registerVar ("selected_track");
        vm.registerVar ("playhead");
        vm.registerVar ("run");
        vm.registerVar ("bpm");
        vm.registerVar ("time");
        vm.registerVar ("t");

        bindVMCallbacks();

        bool ok = vm.compile (source);
        vmCompiled = ok;

        if (ok)
        {
            codeEditor.setErrorLine (0);
            setStatus ("Compile successful!", juce::Colour (0xFF10B981));
            logToConsole ("UI script compiled successfully.");
        }
        else
        {
            int line = vm.getErrorLine();
            codeEditor.setErrorLine (line, vm.getError());
            setStatus ("Error (line " + juce::String (line) + "): " + vm.getError(), juce::Colour (0xFFEF4444));
            logToConsole ("ERROR line " + juce::String (line) + ": " + vm.getError());
        }
        repaint();
    }

    void compileDSPExpression (const juce::String& source)
    {
        if (!activePedal || !engine)
        {
            setStatus ("No pedal selected", juce::Colour (0xFFFB923C));
            logToConsole ("ERROR: Select a pedal on the Board tab first.");
            return;
        }

        // Find the ExpressionNode in the active pedal's DSP graph
        auto* parentNode = engine->getGraph().getNodeForId (activePedal->nodeID);
        if (!parentNode)
        {
            setStatus ("Pedal node not found", juce::Colour (0xFFEF4444));
            return;
        }

        auto* graphProc = dynamic_cast<GraphPedalProcessor*> (parentNode->getProcessor());
        if (!graphProc)
        {
            setStatus ("Not a graph pedal", juce::Colour (0xFFEF4444));
            return;
        }

        // Find or report on ExpressionNode(s)
        DSPNode* exprNode = nullptr;
        for (auto& [id, node] : graphProc->getDSPGraph().getNodes())
        {
            if (node && node->getType() == "expression")
            {
                exprNode = node.get();
                break;
            }
        }

        if (!exprNode)
        {
            setStatus ("No Expression node found in pedal", juce::Colour (0xFFFB923C));
            logToConsole ("WARNING: This pedal has no Expression node. Add one in the FX tab first.");
            logToConsole ("The script will be validated but not applied.");

            // Still validate the script syntax
            ExpressionVM testVM;
            testVM.clearVars();
            bool ok = testVM.compile (source);
            if (ok)
            {
                codeEditor.setErrorLine (0);
                setStatus ("Syntax valid (no Expression node to apply to)", juce::Colour (0xFF10B981));
                logToConsole ("Syntax validation passed.");
            }
            else
            {
                int line = testVM.getErrorLine();
                codeEditor.setErrorLine (line, testVM.getError());
                setStatus ("Error (line " + juce::String (line) + "): " + testVM.getError(), juce::Colour (0xFFEF4444));
                logToConsole ("ERROR line " + juce::String (line) + ": " + testVM.getError());
            }
            return;
        }

        auto* exprNodeTyped = dynamic_cast<ExpressionNode*> (exprNode);
        if (exprNodeTyped)
        {
            bool ok = exprNodeTyped->setExpression (source);
            if (ok)
            {
                codeEditor.setErrorLine (0);
                setStatus ("DSP expression compiled & applied!", juce::Colour (0xFF10B981));
                logToConsole ("Expression applied to node ID " + juce::String (exprNode->getNodeID()));

                // Persist to the design's effects graph
                activePedal->design->effectsGraph = graphProc->getDSPGraph().toJSON();
                engine->saveUndoState();
            }
            else
            {
                // ExpressionNode doesn't surface a line number; re-compile through a
                // local VM purely to locate the offending line in the source.
                ExpressionVM probe;
                probe.clearVars();
                probe.compile (source);
                int line = probe.getErrorLine();
                codeEditor.setErrorLine (line, exprNodeTyped->getCompileError());
                setStatus ("Error (line " + juce::String (line) + "): " + exprNodeTyped->getCompileError(), juce::Colour (0xFFEF4444));
                logToConsole ("ERROR line " + juce::String (line) + ": " + exprNodeTyped->getCompileError());
            }
        }
    }

    void compileGraphBuilder (const juce::String& source)
    {
        if (!activePedal || !engine)
        {
            setStatus ("No pedal selected", juce::Colour (0xFFFB923C));
            logToConsole ("ERROR: Select a pedal on the Board tab first.");
            return;
        }

        logToConsole ("--- FX Graph Builder ---");

        // Build a temporary DSPGraph by interpreting the script
        DSPGraph tempGraph;
        std::map<juce::String, int> varToNodeID;

        juce::StringArray lines = juce::StringArray::fromLines (source);
        bool hasError = false;

        for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx)
        {
            juce::String line = lines[lineIdx].trim();

            // Skip empty lines and comments
            if (line.isEmpty() || line.startsWith ("--") || line.startsWith ("//") || line.startsWith ("#") || line.startsWith ("@"))
                continue;

            // Parse: varName = addNode("type")
            if (line.contains ("addNode"))
            {
                juce::String varName = line.upToFirstOccurrenceOf ("=", false, false).trim();
                juce::String call = line.fromFirstOccurrenceOf ("addNode", false, false).trim();

                // Extract the type string from ("type") or ("type")
                juce::String nodeType = call.fromFirstOccurrenceOf ("\"", false, false)
                                            .upToFirstOccurrenceOf ("\"", false, false).trim();

                if (nodeType.isEmpty())
                {
                    codeEditor.setErrorLine (lineIdx + 1, "addNode requires a quoted type string");
                    logToConsole ("ERROR line " + juce::String (lineIdx + 1) + ": addNode requires a quoted type string");
                    hasError = true;
                    break;
                }

                auto node = createNodeByType (nodeType);
                if (!node)
                {
                    codeEditor.setErrorLine (lineIdx + 1, "Unknown node type \"" + nodeType + "\"");
                    logToConsole ("ERROR line " + juce::String (lineIdx + 1) + ": Unknown node type \"" + nodeType + "\"");
                    hasError = true;
                    break;
                }

                int id = tempGraph.addNode (std::move (node));
                varToNodeID[varName] = id;
                logToConsole ("  Created node \"" + nodeType + "\" (ID " + juce::String (id) + ") as " + varName);
            }
            // Parse: connect(src, srcPort, dst, dstPort)
            else if (line.startsWith ("connect"))
            {
                juce::String args = line.fromFirstOccurrenceOf ("(", false, false)
                                        .upToLastOccurrenceOf (")", false, false);
                juce::StringArray parts;
                parts.addTokens (args, ",", "");
                parts.trim();

                if (parts.size() != 4)
                {
                    codeEditor.setErrorLine (lineIdx + 1, "connect requires 4 arguments");
                    logToConsole ("ERROR line " + juce::String (lineIdx + 1) + ": connect requires 4 arguments");
                    hasError = true;
                    break;
                }

                auto resolveNode = [&](const juce::String& s) -> int {
                    if (varToNodeID.count (s)) return varToNodeID[s];
                    return s.getIntValue();
                };

                int srcID   = resolveNode (parts[0]);
                int srcPort = parts[1].getIntValue();
                int dstID   = resolveNode (parts[2]);
                int dstPort = parts[3].getIntValue();

                bool ok = tempGraph.connect (srcID, srcPort, dstID, dstPort);
                if (ok)
                    logToConsole ("  Connected " + parts[0] + ":" + parts[1] + " -> " + parts[2] + ":" + parts[3]);
                else
                    logToConsole ("  WARNING: Connection failed on line " + juce::String (lineIdx + 1));
            }
            // Parse: setParam(node, "paramName", value)
            else if (line.startsWith ("setParam"))
            {
                juce::String args = line.fromFirstOccurrenceOf ("(", false, false)
                                        .upToLastOccurrenceOf (")", false, false);

                // Split carefully: first arg is node ref, second is quoted string, third is number
                juce::String nodeRef = args.upToFirstOccurrenceOf (",", false, false).trim();
                juce::String rest = args.fromFirstOccurrenceOf (",", false, false).trim();
                juce::String paramName = rest.fromFirstOccurrenceOf ("\"", false, false)
                                             .upToFirstOccurrenceOf ("\"", false, false);
                juce::String valueStr = rest.fromLastOccurrenceOf (",", false, false).trim();

                int nodeID = varToNodeID.count (nodeRef) ? varToNodeID[nodeRef] : nodeRef.getIntValue();
                float value = valueStr.getFloatValue();

                if (auto* node = tempGraph.getNode (nodeID))
                {
                    if (auto* param = node->getParam (paramName))
                    {
                        param->set (value);
                        logToConsole ("  Set " + nodeRef + "." + paramName + " = " + juce::String (value));
                    }
                    else
                        logToConsole ("  WARNING: Param \"" + paramName + "\" not found on node " + nodeRef);
                }
            }
            // Parse: setScreen(node, <ScreenDesign JSON>) — configure an Easy Display.
            // The JSON is taken verbatim as the rest of the line (write it UNQUOTED,
            // e.g. setScreen(disp, {"kind":"easy","grid":{"lines":4,"cols":16},"items":[...]}) ).
            else if (line.startsWith ("setScreen"))
            {
                juce::String args = line.fromFirstOccurrenceOf ("(", false, false)
                                        .upToLastOccurrenceOf (")", false, false);
                juce::String nodeRef = args.upToFirstOccurrenceOf (",", false, false).trim();
                juce::String jsonStr = args.fromFirstOccurrenceOf (",", false, false).trim();
                if (jsonStr.startsWithChar ('"') && jsonStr.endsWithChar ('"'))
                    jsonStr = jsonStr.unquoted();

                int nid = varToNodeID.count (nodeRef) ? varToNodeID[nodeRef] : nodeRef.getIntValue();
                if (auto* disp = dynamic_cast<EasyDisplayNode*> (tempGraph.getNode (nid)))
                {
                    disp->setScreenJSON (jsonStr);
                    logToConsole ("  Set screen on " + nodeRef + " ("
                                  + juce::String ((int) disp->getOutputPorts().size()) + " out / "
                                  + juce::String ((int) disp->getInputPorts().size()) + " in ports)");
                }
                else
                {
                    codeEditor.setErrorLine (lineIdx + 1, "setScreen target must be a disp_easy node");
                    logToConsole ("ERROR line " + juce::String (lineIdx + 1) + ": setScreen target must be a disp_easy node");
                    hasError = true;
                    break;
                }
            }
            else
            {
                logToConsole ("WARNING line " + juce::String (lineIdx + 1) + ": Unrecognized statement: " + line);
            }
        }

        if (hasError)
        {
            setStatus ("Graph build failed - see console", juce::Colour (0xFFEF4444));
            return;
        }

        codeEditor.setErrorLine (0);

        // Apply the built graph to the active pedal
        juce::var graphJSON = tempGraph.toJSON();
        logToConsole ("Graph built successfully with " + juce::String (tempGraph.getNodes().size()) + " nodes.");

        auto* parentNode = engine->getGraph().getNodeForId (activePedal->nodeID);
        if (parentNode)
        {
            if (auto* graphProc = dynamic_cast<GraphPedalProcessor*> (parentNode->getProcessor()))
            {
                // Update the pedal's effects graph
                activePedal->design->effectsGraph = graphJSON;
                auto newProc = std::make_unique<GraphPedalProcessor> (
                    activePedal->name, juce::JSON::toString (graphJSON));
                engine->updatePedalProcessor (activePedal->nodeID, std::move (newProc));

                // Auto-create / remove faceplate widgets for control-surface and
                // display "screen" nodes (e.g. Easy Display) the script declared,
                // so they appear on the pedal face immediately — the same
                // reconciliation the node-graph editor runs on every edit.
                syncControlSurfaceNodes (*activePedal->design, tempGraph);

                engine->saveUndoState();

                setStatus ("Graph applied to pedal!", juce::Colour (0xFF10B981));
                logToConsole ("FX graph applied to pedal \"" + activePedal->name + "\".");

                if (onGraphChanged)
                    onGraphChanged();
            }
        }
        else
        {
            setStatus ("Pedal not found in engine", juce::Colour (0xFFEF4444));
        }
    }

    //==========================================================================
    // Pedalboard script: addPedal / connect / setPos / focus / bypass / clearBoard.
    // Compiling REPLACES the entire current board with what the script declares,
    // so that the same script always reproduces the same board.

    void compileBoardScript (const juce::String& source)
    {
        if (!engine)
        {
            setStatus ("No engine", juce::Colour (0xFFEF4444));
            return;
        }

        logToConsole ("--- Pedalboard Builder ---");

        const auto& factory = getFactoryPedals();
        auto findFactory = [&] (juce::String name) -> const PedalInfo*
        {
            juce::String wanted = name.trim().unquoted();
            for (const auto& p : factory)
                if (p.name.equalsIgnoreCase (wanted)) return &p;
            return nullptr;
        };

        // First pass: validate before mutating engine state
        juce::StringArray lines = juce::StringArray::fromLines (source);
        struct AddOp { juce::String var, type; float x = -1, y = -1; int lineNum; };
        struct ConnOp { juce::String srcVar; int srcCh; juce::String dstVar; int dstCh; int lineNum; };
        struct PosOp { juce::String var; float x, y; int lineNum; };
        struct FocusOp { juce::String var; int lineNum; };
        std::vector<AddOp>   adds;
        std::vector<ConnOp>  conns;
        std::vector<PosOp>   poses;
        std::vector<FocusOp> focuses;
        bool hasError = false;

        auto stripQuotes = [] (juce::String s) {
            s = s.trim();
            if (s.startsWithChar ('"') && s.endsWithChar ('"'))
                s = s.substring (1, s.length() - 1);
            return s;
        };

        auto setLineErr = [&] (int n, const juce::String& msg) {
            codeEditor.setErrorLine (n, msg);
            logToConsole ("ERROR line " + juce::String (n) + ": " + msg);
            hasError = true;
        };

        std::set<juce::String> declared;

        for (int i = 0; i < lines.size(); ++i)
        {
            juce::String line = lines[i].trim();
            int lineNum = i + 1;
            if (line.isEmpty() || line.startsWith ("--") || line.startsWith ("//") || line.startsWith ("#") || line.startsWith ("@"))
                continue;

            // Pattern: var = addPedal("Name") | addPedal("Name", x, y)
            if (line.contains ("addPedal"))
            {
                juce::String var = line.upToFirstOccurrenceOf ("=", false, false).trim();
                if (var.isEmpty()) { setLineErr (lineNum, "addPedal needs a variable: ts = addPedal(\"Type\")"); break; }

                juce::String args = line.fromFirstOccurrenceOf ("(", false, false)
                                        .upToLastOccurrenceOf  (")", false, false);
                juce::StringArray parts;
                parts.addTokens (args, ",", "\"");
                parts.trim();
                if (parts.size() < 1) { setLineErr (lineNum, "addPedal requires a pedal type"); break; }

                AddOp op { var, stripQuotes (parts[0]), -1.0f, -1.0f, lineNum };
                if (parts.size() >= 3)
                {
                    op.x = parts[1].getFloatValue();
                    op.y = parts[2].getFloatValue();
                }
                if (!findFactory (op.type)) { setLineErr (lineNum, "Unknown pedal type \"" + op.type + "\""); break; }
                if (declared.count (var)) { setLineErr (lineNum, "Variable \"" + var + "\" redeclared"); break; }
                declared.insert (var);
                adds.push_back (op);
            }
            else if (line.startsWith ("connect"))
            {
                juce::String args = line.fromFirstOccurrenceOf ("(", false, false)
                                        .upToLastOccurrenceOf  (")", false, false);
                juce::StringArray parts;
                parts.addTokens (args, ",", "");
                parts.trim();
                if (parts.size() != 4) { setLineErr (lineNum, "connect needs 4 args: connect(src, srcCh, dst, dstCh)"); break; }
                if (!declared.count (parts[0])) { setLineErr (lineNum, "Unknown pedal \"" + parts[0] + "\""); break; }
                if (!declared.count (parts[2])) { setLineErr (lineNum, "Unknown pedal \"" + parts[2] + "\""); break; }
                conns.push_back ({ parts[0], parts[1].getIntValue(), parts[2], parts[3].getIntValue(), lineNum });
            }
            else if (line.startsWith ("setPos"))
            {
                juce::String args = line.fromFirstOccurrenceOf ("(", false, false)
                                        .upToLastOccurrenceOf  (")", false, false);
                juce::StringArray parts;
                parts.addTokens (args, ",", "");
                parts.trim();
                if (parts.size() != 3) { setLineErr (lineNum, "setPos needs 3 args: setPos(pedal, x, y)"); break; }
                if (!declared.count (parts[0])) { setLineErr (lineNum, "Unknown pedal \"" + parts[0] + "\""); break; }
                poses.push_back ({ parts[0], parts[1].getFloatValue(), parts[2].getFloatValue(), lineNum });
            }
            else if (line.startsWith ("focus"))
            {
                juce::String args = line.fromFirstOccurrenceOf ("(", false, false)
                                        .upToLastOccurrenceOf  (")", false, false).trim();
                if (!declared.count (args)) { setLineErr (lineNum, "Unknown pedal \"" + args + "\""); break; }
                focuses.push_back ({ args, lineNum });
            }
            else if (line.startsWith ("clearBoard"))
            {
                // No-op for the first pass — the apply pass always starts from empty.
            }
            else
            {
                logToConsole ("WARNING line " + juce::String (lineNum) + ": Unrecognized statement: " + line);
            }
        }

        if (hasError)
        {
            setStatus ("Board script failed - see console", juce::Colour (0xFFEF4444));
            return;
        }

        // Apply pass — clear the board, then create everything.
        {
            std::vector<AudioGraphEngine::NodeID> idsToRemove;
            for (const auto& inst : engine->getPedalInstances())
                idsToRemove.push_back (inst.nodeID);
            for (auto id : idsToRemove)
                engine->removePedal (id);
        }

        std::map<juce::String, AudioGraphEngine::NodeID> varToNode;
        for (const auto& op : adds)
        {
            const PedalInfo* info = findFactory (op.type);
            float x = (op.x >= 0) ? op.x : 80.0f + (float) varToNode.size() * 140.0f;
            float y = (op.y >= 0) ? op.y : 200.0f;
            float w = (float) info->gridW * 90.0f;
            float h = (float) info->gridH * 180.0f;
            auto id = engine->addPedal (info->factory(), "", 0, x, y, w, h);
            if (auto* inst = engine->getPedalInstance (id))
            {
                inst->name = info->name;
                inst->category = info->category;
                inst->colour = info->colour;
                inst->numKnobs = info->numKnobs;
                if (info->designFactory)
                    inst->design = info->designFactory();
            }
            varToNode[op.var] = id;
            logToConsole ("  Created \"" + op.type + "\" as " + op.var);
        }

        for (const auto& op : poses)
        {
            auto it = varToNode.find (op.var);
            if (it == varToNode.end()) continue;
            if (auto* inst = engine->getPedalInstance (it->second))
            {
                inst->boardX = op.x;
                inst->boardY = op.y;
            }
        }

        for (const auto& c : conns)
        {
            auto srcIt = varToNode.find (c.srcVar);
            auto dstIt = varToNode.find (c.dstVar);
            if (srcIt == varToNode.end() || dstIt == varToNode.end()) continue;
            if (!engine->connect (srcIt->second, c.srcCh, dstIt->second, c.dstCh))
                logToConsole ("  WARNING line " + juce::String (c.lineNum) + ": connect failed");
            else
                logToConsole ("  Connected " + c.srcVar + ":" + juce::String (c.srcCh)
                              + " -> " + c.dstVar + ":" + juce::String (c.dstCh));
        }

        for (const auto& f : focuses)
        {
            auto it = varToNode.find (f.var);
            if (it != varToNode.end()) engine->setFocusedPedal (it->second);
        }

        engine->saveUndoState();
        codeEditor.setErrorLine (0);
        setStatus ("Pedalboard rebuilt from script", juce::Colour (0xFF10B981));
        logToConsole ("Board script applied: " + juce::String ((int) adds.size()) + " pedals, "
                      + juce::String ((int) conns.size()) + " connections.");

        if (onGraphChanged) onGraphChanged();
    }

    //==========================================================================
    // Generate a Pedalboard script that reproduces the engine's current board.

    void importBoardAsScript()
    {
        if (!engine)
        {
            setStatus ("No engine", juce::Colour (0xFFEF4444));
            return;
        }

        const auto& instances = engine->getPedalInstances();

        std::map<juce::uint32, juce::String> nodeIdToVar;
        std::map<juce::String, int> typeCount;

        auto sanitize = [] (const juce::String& s) {
            juce::String out;
            for (auto c : s)
                if (juce::CharacterFunctions::isLetterOrDigit (c)) out << c;
                else if (c == ' ' || c == '-' || c == '_') out << '_';
            if (out.isEmpty() || juce::CharacterFunctions::isDigit (out[0])) out = "p_" + out;
            return out.toLowerCase();
        };

        juce::String script;
        script << "-- Generated board script\n";
        script << "-- " << juce::String ((int) instances.size()) << " pedals\n\n";

        for (const auto& inst : instances)
        {
            juce::String base = sanitize (inst.name);
            int n = ++typeCount[base];
            juce::String var = (n == 1) ? base : (base + juce::String (n));
            nodeIdToVar[inst.nodeID.uid] = var;

            script << var << " = addPedal(\"" << inst.name << "\", "
                   << juce::String (inst.boardX, 0) << ", " << juce::String (inst.boardY, 0) << ")\n";
        }
        script << "\n";

        for (const auto& conn : engine->getGraph().getConnections())
        {
            auto srcIt = nodeIdToVar.find (conn.source.nodeID.uid);
            auto dstIt = nodeIdToVar.find (conn.destination.nodeID.uid);
            if (srcIt == nodeIdToVar.end() || dstIt == nodeIdToVar.end()) continue;
            script << "connect(" << srcIt->second << ", " << conn.source.channelIndex
                   << ", "       << dstIt->second << ", " << conn.destination.channelIndex << ")\n";
        }

        auto focused = engine->getFocusedPedal();
        auto fIt = nodeIdToVar.find (focused.uid);
        if (fIt != nodeIdToVar.end())
            script << "\nfocus(" << fIt->second << ")\n";

        modeSelector.setSelectedId ((int) ScriptMode::Board, juce::sendNotification);
        codeDocument.replaceAllContent (script);
        scriptNameEditor.setText ("board_export", false);
        codeEditor.setErrorLine (0);
        setStatus ("Imported board as script", juce::Colour (0xFF10B981));
        logToConsole ("Imported board: " + juce::String ((int) instances.size()) + " pedals.");
    }

    //==========================================================================
    // Pedal Design script: setMeta / setChassis / addKnob / addSwitch / addLed /
    //   addFootswitch / addFader / addTextScreen / mapControl.
    // Compiling REPLACES the active pedal's chassis + controls + mappings.
    // The DSP graph is preserved untouched — use FX Graph Builder mode for that.

    void compilePedalScript (const juce::String& source)
    {
        if (!activePedal || !activePedal->design)
        {
            setStatus ("No pedal selected", juce::Colour (0xFFFB923C));
            logToConsole ("ERROR: Select a pedal on the Board tab first.");
            return;
        }

        logToConsole ("--- Pedal Design Builder ---");

        PedalDesign next = *activePedal->design;
        next.controls.clear();
        next.mappings.clear();

        bool hasError = false;

        auto stripQuotes = [] (juce::String s) {
            s = s.trim();
            if (s.startsWithChar ('"') && s.endsWithChar ('"'))
                s = s.substring (1, s.length() - 1);
            return s;
        };

        auto setLineErr = [&] (int n, const juce::String& msg) {
            codeEditor.setErrorLine (n, msg);
            logToConsole ("ERROR line " + juce::String (n) + ": " + msg);
            hasError = true;
        };

        auto splitArgs = [] (juce::String args) {
            juce::StringArray parts;
            parts.addTokens (args, ",", "\"");
            parts.trim();
            return parts;
        };

        // canonicalType maps both the friendly camelCase alias (Knob/Switch/...)
        // AND a raw canonical name passed verbatim to addControl("xypad", ...)
        // to the PedalDesign type string the renderer recognises.
        auto canonicalType = [] (const juce::String& t) -> juce::String
        {
            auto lower = t.toLowerCase();
            if (lower == "knob")        return "knob";
            if (lower == "switch")      return "switch";
            if (lower == "footswitch")  return "footswitch";
            if (lower == "led")         return "led";
            if (lower == "fader")       return "fader";
            if (lower == "textscreen" || lower == "text_screen") return "text_screen";
            if (lower == "selector")    return "selector";
            if (lower == "xypad")       return "xypad";
            if (lower == "joystick")    return "joystick";
            if (lower == "vu_meter" || lower == "vumeter" || lower == "vu") return "vu_meter";
            if (lower == "rgb_led" || lower == "rgbled")    return "rgb_led";
            if (lower == "indicator")   return "indicator";
            if (lower == "7seg")        return "7seg";
            if (lower == "display")     return "display";
            if (lower == "console")     return "console";
            if (lower == "pixel_display" || lower == "pixeldisplay") return "pixel_display";
            if (lower == "oscilloscope" || lower == "scope") return "oscilloscope";
            if (lower == "label")       return "label";
            if (lower == "graphic")     return "graphic";
            if (lower == "file_loader" || lower == "fileloader")       return "file_loader";
            if (lower == "plugin_browser" || lower == "pluginbrowser") return "plugin_browser";
            if (lower == "overlay_launcher" || lower == "overlaylauncher") return "overlay_launcher";
            if (lower == "library_loader" || lower == "libraryloader") return "library_loader";
            // Unknown — pass through as-is so an unrecognised type still serializes
            // round-trip (the renderer will fall back to a placeholder).
            return lower;
        };

        auto addControl = [&] (const juce::String& type, const juce::StringArray& parts, int lineNum)
        {
            // Common shape: addX(id, x, y, label[, w, h])
            if (parts.size() < 4) { setLineErr (lineNum, "add" + type + " needs at least (id, x, y, label)"); return; }
            PedalDesign::Control c;
            c.type      = canonicalType (type);
            c.controlID = stripQuotes (parts[0]);
            c.x         = parts[1].getFloatValue();
            c.y         = parts[2].getFloatValue();
            c.label     = stripQuotes (parts[3]);
            // Start from type-aware defaults so omitted w/h match what
            // PedalDesign::fromJSON would produce — then let explicit args win.
            c.width     = PedalDesign::defaultControlWidth  (c.type);
            c.height    = PedalDesign::defaultControlHeight (c.type);
            if (parts.size() >= 5)
            {
                float w = parts[4].getFloatValue();
                if (w > 0) c.width = w;
            }
            if (parts.size() >= 6)
            {
                float h = parts[5].getFloatValue();
                if (h > 0) c.height = h;
            }

            next.controls.push_back (c);
        };

        // Generic addControl("type", "id", x, y, "label" [, w, h]) — handles any
        // of the 23 canonical types in InventoryOverlay parts[] so the script
        // round-trip isn't limited to the 6 friendly aliases below.
        auto addControlGeneric = [&] (const juce::StringArray& parts, int lineNum)
        {
            if (parts.size() < 5) { setLineErr (lineNum, "addControl needs at least (\"type\", id, x, y, label)"); return; }
            juce::String type = stripQuotes (parts[0]);
            juce::StringArray rest;
            for (int i = 1; i < parts.size(); ++i) rest.add (parts[i]);
            addControl (type, rest, lineNum);
        };

        // Look up a previously-added control by ID so set* statements can
        // decorate the most recent addX(...) line.
        auto findControlByID = [&] (const juce::String& id) -> PedalDesign::Control*
        {
            for (auto& c : next.controls)
                if (c.controlID == id) return &c;
            return nullptr;
        };

        juce::StringArray lines = juce::StringArray::fromLines (source);
        for (int i = 0; i < lines.size(); ++i)
        {
            juce::String line = lines[i].trim();
            int lineNum = i + 1;
            if (line.isEmpty() || line.startsWith ("--") || line.startsWith ("//") || line.startsWith ("#") || line.startsWith ("@"))
                continue;

            // Strip optional `var = ` prefix — variables are decorative for pedal mode.
            juce::String rhs = line;
            if (line.contains ("="))
            {
                auto before = line.upToFirstOccurrenceOf ("=", false, false).trim();
                if (! before.contains ("("))
                    rhs = line.fromFirstOccurrenceOf ("=", false, false).trim();
            }

            juce::String args = rhs.fromFirstOccurrenceOf ("(", false, false)
                                   .upToLastOccurrenceOf  (")", false, false);
            auto parts = splitArgs (args);

            if      (rhs.startsWith ("setMeta"))
            {
                if (parts.size() >= 1) next.name        = stripQuotes (parts[0]);
                if (parts.size() >= 2) next.author      = stripQuotes (parts[1]);
                if (parts.size() >= 3) next.category    = stripQuotes (parts[2]);
                if (parts.size() >= 4) next.description = stripQuotes (parts[3]);
            }
            else if (rhs.startsWith ("setChassis"))
            {
                if (parts.size() < 2) { setLineErr (lineNum, "setChassis needs (w, h[, colorHex])"); break; }
                next.chassisW = parts[0].getFloatValue();
                next.chassisH = parts[1].getFloatValue();
                if (parts.size() >= 3)
                {
                    juce::String hex = parts[2].trim();
                    if (hex.startsWith ("0x") || hex.startsWith ("0X")) hex = hex.substring (2);
                    next.chassisColour = juce::Colour ((juce::uint32) hex.getHexValue64());
                }
            }
            else if (rhs.startsWith ("addControl"))     addControlGeneric (parts, lineNum);
            else if (rhs.startsWith ("addKnob"))        addControl ("Knob",        parts, lineNum);
            else if (rhs.startsWith ("addSwitch"))      addControl ("Switch",      parts, lineNum);
            else if (rhs.startsWith ("addFootswitch"))  addControl ("Footswitch",  parts, lineNum);
            else if (rhs.startsWith ("addLed"))         addControl ("Led",         parts, lineNum);
            else if (rhs.startsWith ("addFader"))       addControl ("Fader",       parts, lineNum);
            else if (rhs.startsWith ("addTextScreen"))  addControl ("TextScreen",  parts, lineNum);
            // Per-pedal style engine (docs/control-catalog.md): kit selection + colorway.
            else if (rhs.startsWith ("setStyleKit"))
            {
                if (parts.size() >= 1) next.styleKit = stripQuotes (parts[0]);
            }
            else if (rhs.startsWith ("setColorway"))
            {
                if (parts.size() < 1) { setLineErr (lineNum, "setColorway needs (seedARGB[, mode])"); break; }
                juce::String seedStr = parts[0].trim();
                juce::uint32 argb = 0;
                if (seedStr.startsWith ("0x") || seedStr.startsWith ("0X"))
                    argb = (juce::uint32) seedStr.substring (2).getHexValue64();
                else
                    argb = (juce::uint32) seedStr.getLargeIntValue();
                // Sign-extend through int32 so the stored int64 matches what the
                // JSON loader produces from "-16744961"-style signed values.
                next.colorwaySeed = (juce::int64) (juce::int32) argb;
                if (parts.size() >= 2) next.colorwayMode = parts[1].getIntValue();
            }
            // Per-control decorators — apply to the most recently-added control
            // matching the given id. Quiet no-op if id is unknown (lets a script
            // declare style/guard/shift before the control without erroring).
            else if (rhs.startsWith ("setStyle"))
            {
                if (parts.size() < 2) { setLineErr (lineNum, "setStyle needs (controlID, \"style\")"); break; }
                if (auto* c = findControlByID (stripQuotes (parts[0]))) c->style = stripQuotes (parts[1]);
            }
            else if (rhs.startsWith ("setGuard"))
            {
                if (parts.size() < 2) { setLineErr (lineNum, "setGuard needs (controlID, guardInt)"); break; }
                if (auto* c = findControlByID (stripQuotes (parts[0]))) c->guard = parts[1].getIntValue();
            }
            else if (rhs.startsWith ("setShift"))
            {
                if (parts.size() < 2) { setLineErr (lineNum, "setShift needs (controlID, \"shiftBinding\")"); break; }
                if (auto* c = findControlByID (stripQuotes (parts[0]))) c->shiftBinding = stripQuotes (parts[1]);
            }
            else if (rhs.startsWith ("setDefault"))
            {
                if (parts.size() < 2) { setLineErr (lineNum, "setDefault needs (controlID, value)"); break; }
                if (auto* c = findControlByID (stripQuotes (parts[0]))) c->defaultValue = parts[1].getFloatValue();
            }
            else if (rhs.startsWith ("setPositions"))
            {
                if (parts.size() < 2) { setLineErr (lineNum, "setPositions needs (controlID, n)"); break; }
                if (auto* c = findControlByID (stripQuotes (parts[0]))) c->positions = parts[1].getIntValue();
            }
            else if (rhs.startsWith ("mapControl"))
            {
                if (parts.size() != 2) { setLineErr (lineNum, "mapControl needs (controlID, \"nodeID_paramID\")"); break; }
                PedalDesign::Mapping m;
                m.controlID = stripQuotes (parts[0]);
                m.nodeParam = stripQuotes (parts[1]);
                next.mappings.push_back (m);
            }
            else
            {
                logToConsole ("WARNING line " + juce::String (lineNum) + ": Unrecognized statement: " + line);
            }

            if (hasError) break;
        }

        if (hasError)
        {
            setStatus ("Pedal script failed - see console", juce::Colour (0xFFEF4444));
            return;
        }

        *activePedal->design = next;
        if (engine) engine->saveUndoState();
        codeEditor.setErrorLine (0);
        setStatus ("Pedal design rebuilt from script", juce::Colour (0xFF10B981));
        logToConsole ("Pedal design applied: " + juce::String ((int) next.controls.size())
                      + " controls, " + juce::String ((int) next.mappings.size()) + " mappings.");

        if (onGraphChanged) onGraphChanged();
    }

    //==========================================================================
    // Generate a Pedal Design script reproducing the active pedal's chassis + controls + mappings.

    void importPedalAsScript()
    {
        if (!activePedal || !activePedal->design)
        {
            setStatus ("No pedal selected", juce::Colour (0xFFFB923C));
            return;
        }
        const auto& d = *activePedal->design;

        auto typeToFunc = [] (const juce::String& t) -> juce::String
        {
            if (t == "knob")        return "addKnob";
            if (t == "switch")      return "addSwitch";
            if (t == "footswitch")  return "addFootswitch";
            if (t == "led")         return "addLed";
            if (t == "fader")       return "addFader";
            if (t == "text_screen") return "addTextScreen";
            return juce::String();   // -> use generic addControl("type", ...)
        };

        juce::String s;
        s << "-- Generated pedal design script\n";
        s << "-- " << juce::String ((int) d.controls.size()) << " controls, "
          <<         juce::String ((int) d.mappings.size()) << " mappings\n\n";

        s << "setMeta(\"" << d.name << "\", \"" << d.author << "\", \""
          << d.category << "\", \"" << d.description << "\")\n";
        s << "setChassis(" << juce::String (d.chassisW, 0) << ", "
          <<                  juce::String (d.chassisH, 0) << ", 0x"
          <<                  juce::String::toHexString ((juce::int64) d.chassisColour.getARGB()).toUpperCase() << ")\n";

        // Per-pedal style engine — emit only when non-default so simple pedals
        // stay terse. Parsing side accepts these in any order before/after the
        // add* statements.
        if (d.styleKit.isNotEmpty() && d.styleKit != "default")
            s << "setStyleKit(\"" << d.styleKit << "\")\n";
        if (d.colorwaySeed != 0)
        {
            // colorwaySeed stores an ARGB in an int64 — emit the low 32 bits so
            // a seed like 0xFF007DFF doesn't sign-extend into 0xFFFFFFFFFF007DFF.
            const auto argb = (juce::uint32)(juce::int64) (d.colorwaySeed & 0xFFFFFFFFLL);
            s << "setColorway(0x" << juce::String::toHexString ((juce::int64) argb).toUpperCase()
              << ", " << juce::String (d.colorwayMode) << ")\n";
        }
        s << "\n";

        for (const auto& c : d.controls)
        {
            juce::String fn = typeToFunc (c.type);
            // Unknown-to-alias types fall back to generic addControl("type", ...)
            // — every PedalDesign control type round-trips losslessly now.
            if (fn.isEmpty())
            {
                s << "addControl(\"" << c.type << "\", \"" << c.controlID << "\", "
                  << juce::String (c.x, 0) << ", " << juce::String (c.y, 0)
                  << ", \"" << c.label << "\"";
                if (c.width != PedalDesign::defaultControlWidth (c.type)
                    || c.height != PedalDesign::defaultControlHeight (c.type))
                    s << ", " << juce::String (c.width, 0) << ", " << juce::String (c.height, 0);
                s << ")\n";
            }
            else
            {
                s << fn << "(\"" << c.controlID << "\", "
                  << juce::String (c.x, 0) << ", " << juce::String (c.y, 0)
                  << ", \"" << c.label << "\"";
                if (c.width != PedalDesign::defaultControlWidth (c.type)
                    || c.height != PedalDesign::defaultControlHeight (c.type))
                    s << ", " << juce::String (c.width, 0) << ", " << juce::String (c.height, 0);
                s << ")\n";
            }
            // Per-control decorators (emitted only when non-default so terse pedals
            // stay terse and diffs are clean).
            if (c.style.isNotEmpty() && c.style != "default")
                s << "setStyle(\"" << c.controlID << "\", \"" << c.style << "\")\n";
            if (c.guard != 0)
                s << "setGuard(\"" << c.controlID << "\", " << juce::String (c.guard) << ")\n";
            if (c.shiftBinding.isNotEmpty())
                s << "setShift(\"" << c.controlID << "\", \"" << c.shiftBinding << "\")\n";
            // Non-struct-default scalar fields the add* statements don't carry:
            // emit them only when they diverge so a typical knob stays a one-liner.
            if (c.defaultValue != 0.5f)
                s << "setDefault(\"" << c.controlID << "\", " << juce::String (c.defaultValue, 3) << ")\n";
            if ((c.type == "selector" || c.type == "switch") && c.positions != 4)
                s << "setPositions(\"" << c.controlID << "\", " << juce::String (c.positions) << ")\n";
        }

        if (! d.mappings.empty())
        {
            s << "\n";
            for (const auto& m : d.mappings)
                s << "mapControl(\"" << m.controlID << "\", \"" << m.nodeParam << "\")\n";
        }

        modeSelector.setSelectedId ((int) ScriptMode::Pedal, juce::sendNotification);
        codeDocument.replaceAllContent (s);
        scriptNameEditor.setText ("pedal_export", false);
        codeEditor.setErrorLine (0);
        setStatus ("Imported pedal design as script", juce::Colour (0xFF10B981));
        logToConsole ("Imported pedal: " + d.name + " (" + juce::String ((int) d.controls.size()) + " controls).");
    }

    //==========================================================================
    // Generate an FX-Graph-Builder script that reproduces the active pedal's
    // current DSPGraph. Round-trip companion to compileGraphBuilder().
    //
    // Switches mode to GraphBuilder, replaces the editor contents, and does NOT
    // compile (the script is identical to what's already running, so applying it
    // would be redundant — the user can edit then hit Compile).

    void importGraphAsScript()
    {
        if (!activePedal || !engine)
        {
            logToConsole ("WARNING: No pedal selected.");
            setStatus ("No pedal selected", juce::Colour (0xFFFB923C));
            return;
        }

        auto* parentNode = engine->getGraph().getNodeForId (activePedal->nodeID);
        if (!parentNode)
        {
            setStatus ("Pedal node not found", juce::Colour (0xFFEF4444));
            return;
        }
        auto* graphProc = dynamic_cast<GraphPedalProcessor*> (parentNode->getProcessor());
        if (!graphProc)
        {
            setStatus ("Not a graph pedal", juce::Colour (0xFFEF4444));
            return;
        }

        const auto& graph = graphProc->getDSPGraph();

        juce::String script;
        script << "-- Generated from \"" << activePedal->name << "\" on " << juce::Time::getCurrentTime().toString (true, true) << "\n";
        script << "-- " << juce::String ((int) graph.getNodes().size()) << " nodes, "
               <<            juce::String ((int) graph.getConnections().size()) << " connections\n\n";

        // Assign a unique variable name per node. Iterating in node-ID order keeps
        // the output stable and roughly matches the execution order.
        std::map<int, juce::String> nodeIDToVar;
        std::map<juce::String, int> typeCount;
        for (const auto& [id, nodePtr] : graph.getNodes())
        {
            if (!nodePtr) continue;
            juce::String type = nodePtr->getType();
            juce::String safeType = type.replace ("-", "_").replace (" ", "_");
            int n = ++typeCount[safeType];
            juce::String var = safeType + juce::String (n);
            nodeIDToVar[id] = var;
        }

        // Node creation
        for (const auto& [id, nodePtr] : graph.getNodes())
        {
            if (!nodePtr) continue;
            script << nodeIDToVar[id] << " = addNode(\"" << nodePtr->getType() << "\")\n";
        }
        script << "\n";

        // Non-default parameter values
        bool anyParam = false;
        for (const auto& [id, nodePtr] : graph.getNodes())
        {
            if (!nodePtr) continue;
            for (const auto& p : nodePtr->getParams())
            {
                float v = p.get();
                if (std::abs (v - p.defaultVal) < 1.0e-6f) continue;
                script << "setParam(" << nodeIDToVar[id] << ", \"" << p.id << "\", "
                       << juce::String (v, 4) << ")\n";
                anyParam = true;
            }
        }
        if (anyParam) script << "\n";

        // Connections
        for (const auto& c : graph.getConnections())
        {
            auto srcIt = nodeIDToVar.find (c.sourceNodeID);
            auto dstIt = nodeIDToVar.find (c.destNodeID);
            if (srcIt == nodeIDToVar.end() || dstIt == nodeIDToVar.end()) continue;
            script << "connect(" << srcIt->second << ", " << c.sourcePort
                   << ", "       << dstIt->second << ", " << c.destPort << ")\n";
        }

        // Switch mode and load
        modeSelector.setSelectedId ((int) ScriptMode::GraphBuilder, juce::sendNotification);
        codeDocument.replaceAllContent (script);
        scriptNameEditor.setText ("imported", false);
        codeEditor.setErrorLine (0);
        setStatus ("Imported graph as script", juce::Colour (0xFF10B981));
        logToConsole ("Imported FX graph: "
                      + juce::String ((int) graph.getNodes().size()) + " nodes, "
                      + juce::String ((int) graph.getConnections().size()) + " connections.");
    }

    //==========================================================================
    // VM callback binding

    void bindVMCallbacks()
    {
        vm.getParamCallback = [this](int idx) -> float {
            if (!activePedal || !engine) 
            {
                // Sensible defaults for standalone preview
                if (idx == 0) return 3.0f;
                if (idx == 2) return 64.0f;
                return 0.0f;
            }

            // Try to find GridSequencerNode
            auto* parentNode = engine->getGraph().getNodeForId (activePedal->nodeID);
            if (!parentNode) return 0.0f;
            auto* graphProc = dynamic_cast<GraphPedalProcessor*> (parentNode->getProcessor());
            if (!graphProc) return 0.0f;

            for (auto& [id, node] : graphProc->getDSPGraph().getNodes())
            {
                if (node && node->getType() == "grid_sequencer")
                {
                    juce::String tr = "tr0";
                    if (idx == 0) return node->getParam (tr + "_div")->get();
                    if (idx == 1) return node->getParam (tr + "_len")->get();
                    if (idx == 2) return node->getParam (tr + "_val1")->get();
                    if (idx == 3) return node->getParam (tr + "_val2")->get();
                }
            }
            return 0.0f;
        };

        vm.setParamCallback = [this](int, float) {
            // Param setting from standalone script is a no-op for now
        };

        vm.drawImageCallback = [this](juce::Graphics& g, float imgIdx, float x, float y, float w, float h) {
            g.setColour (juce::Colours::white);
            if (imgIdx == 0)
            {
                juce::Path p;
                p.addTriangle (x, y, x + w, y + h * 0.5f, x, y + h);
                g.fillPath (p);
            }
            else if (imgIdx == 1)
                g.fillRect (x, y, w, h);
            else if (imgIdx == 2)
                g.fillEllipse (x, y, w, h);
            else
                g.drawRect (x, y, w, h);
        };
    }

    //==========================================================================
    // Script storage routing:
    //   Modes UI/DSP/FX Graph (1-3) → per-pedal, lives on PedalDesign::scripts
    //   Mode  Board        (4)      → engine-scoped, lives on AudioGraphEngine::engineScripts

    std::vector<PedalDesign::Script>* getScriptStoreFor (ScriptMode m)
    {
        if (m == ScriptMode::Board)
            return engine ? &engine->engineScripts : nullptr;
        return (activePedal && activePedal->design) ? &activePedal->design->scripts : nullptr;
    }

    void saveScript()
    {
        auto* store = getScriptStoreFor (currentMode);
        if (!store)
        {
            const char* needed = (currentMode == ScriptMode::Board) ? "engine" : "pedal";
            logToConsole (juce::String ("WARNING: No ") + needed + " available. Script not saved.");
            setStatus (juce::String ("No ") + needed + " available", juce::Colour (0xFFFB923C));
            return;
        }

        juce::String name = scriptNameEditor.getText().trim();
        if (name.isEmpty()) name = "untitled";

        int mode = (int) currentMode;
        juce::String source = codeDocument.getAllContent();

        bool replaced = false;
        for (auto& s : *store)
        {
            if (s.name == name && s.mode == mode)
            {
                s.source = source;
                replaced = true;
                break;
            }
        }
        if (!replaced)
            store->push_back ({ name, mode, source });

        if (engine) engine->saveUndoState();

        juce::String modeLabel = modeName ((ScriptMode) mode);
        const char* where = (mode == (int) ScriptMode::Board) ? "engine" : "pedal design";
        setStatus ("Saved \"" + name + "\" (" + modeLabel + ")", juce::Colour (0xFF10B981));
        logToConsole (juce::String ("Script saved to ") + where + ": " + name + " [" + modeLabel + "]");
    }

    //==========================================================================
    // Show a popup menu of all scripts saved on the active pedal's design,
    // filtered to the current mode. Selecting one loads it into the editor.

    void showLoadScriptMenu()
    {
        // Build a combined list of scripts from both stores so the user can jump between modes.
        struct Entry { ScriptMode owner; int index; PedalDesign::Script* ref; };
        std::vector<Entry> entries;

        auto collectFrom = [&] (std::vector<PedalDesign::Script>* store, ScriptMode tag)
        {
            if (!store) return;
            for (size_t i = 0; i < store->size(); ++i)
                entries.push_back ({ tag, (int) i, & (*store)[i] });
        };

        // tag arg distinguishes which store the script came from; we use Board for engineScripts
        // and the pedal design for everything else.
        collectFrom (activePedal && activePedal->design ? &activePedal->design->scripts : nullptr, ScriptMode::UI);
        collectFrom (engine ? &engine->engineScripts : nullptr, ScriptMode::Board);

        if (entries.empty())
        {
            logToConsole ("No saved scripts available.");
            setStatus ("No saved scripts", juce::Colour (0xFFFB923C));
            return;
        }

        juce::PopupMenu menu;
        std::vector<int> entryIndexForMenuId;
        int menuId = 1;

        auto addSection = [&] (ScriptMode m)
        {
            bool hasAny = false;
            for (size_t i = 0; i < entries.size(); ++i)
            {
                if (entries[i].ref->mode != (int) m) continue;
                if (!hasAny)
                {
                    menu.addSectionHeader (modeName (m));
                    hasAny = true;
                }
                menu.addItem (menuId, entries[i].ref->name);
                entryIndexForMenuId.push_back ((int) i);
                ++menuId;
            }
        };

        addSection (currentMode);
        for (auto m : { ScriptMode::UI, ScriptMode::DSP, ScriptMode::GraphBuilder, ScriptMode::Board, ScriptMode::Pedal })
            if (m != currentMode) addSection (m);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (btnLoad),
            [this, entries, entryIndexForMenuId] (int chosen)
            {
                if (chosen <= 0) return;
                int entryIdx = entryIndexForMenuId[(size_t) (chosen - 1)];
                const auto& e = entries[(size_t) entryIdx];
                if (!e.ref) return;
                const auto& s = *e.ref;

                if ((int) currentMode != s.mode)
                    modeSelector.setSelectedId (s.mode, juce::sendNotification);
                codeDocument.replaceAllContent (s.source);
                scriptNameEditor.setText (s.name, false);
                setStatus ("Loaded \"" + s.name + "\"", juce::Colour (0xFF10B981));
                logToConsole ("Loaded script: " + s.name + " [" + modeName ((ScriptMode) s.mode) + "]");
            });
    }

    static juce::String modeName (ScriptMode m)
    {
        switch (m)
        {
            case ScriptMode::UI:           return "UI";
            case ScriptMode::DSP:          return "DSP";
            case ScriptMode::GraphBuilder: return "FX Graph";
            case ScriptMode::Board:        return "Board";
            case ScriptMode::Pedal:        return "Pedal";
        }
        return "?";
    }

    //==========================================================================
    // Default scripts for each mode

    void loadDefaultScript (ScriptMode mode)
    {
        juce::String script;

        if (mode == ScriptMode::UI)
        {
            script =
                "@inputs w h mouse_x mouse_y mouse_down mouse_click mouse_drag playhead selected_track\n"
                "@outputs p1 p2\n"
                "\n"
                "-- Interactive XY Pad & Multi-Wave Script --\n"
                "-- Click & drag inside the pad area to modulate parameters!\n"
                "\n"
                "-- 1. Draw background\n"
                "rectFill (0, 0, w, h, 1114908)\n"
                "\n"
                "-- 2. Draw XY Pad\n"
                "pad_w = w - 40\n"
                "pad_h = h - 100\n"
                "rectFill (20, 80, pad_w, pad_h, 2033957)\n"
                "rect (20, 80, pad_w, pad_h, 3223169)\n"
                "\n"
                "-- 3. Mouse interaction\n"
                "active_drag = cond (mouse_down, and (ge (mouse_x, 20), and (le (mouse_x, w - 20), and (ge (mouse_y, 80), le (mouse_y, h - 20)))), 0)\n"
                "norm_x = (mouse_x - 20) / pad_w\n"
                "norm_y = (mouse_y - 80) / pad_h\n"
                "\n"
                "setParam (0, cond (active_drag, clamp (norm_x * 7, 0, 7), getParam (0)))\n"
                "setParam (2, cond (active_drag, clamp (40 + norm_y * 48, 20, 127), getParam (2)))\n"
                "\n"
                "-- 4. Draw neon dot\n"
                "dot_x = 20 + (getParam (0) / 7.0) * pad_w\n"
                "dot_y = 80 + ((getParam (2) - 40) / 48.0) * pad_h\n"
                "circleFill (dot_x, dot_y, 10, 15485081)\n"
                "circle (dot_x, dot_y, 15, 16777215)\n"
                "\n"
                "-- 5. Decorative waveform\n"
                "line (20, 45, w - 20, 45, 1, 3223169)\n"
                "t_scaled = t * 6.28\n"
                "line (20, 45, w - 20, 45 + sin (t_scaled * 2.0) * 15, 1.5, 15485081)\n"
                "\n"
                "-- 6. Display values\n"
                "text (selected_track, 20, 15, 12, 16777215)\n"
                "text (getParam(0), 150, 15, 12, 16777215)\n"
                "text (getParam(2), 270, 15, 12, 16777215)\n";
        }
        else if (mode == ScriptMode::DSP)
        {
            script =
                "@inputs in in2\n"
                "@outputs out\n"
                "@parameters drive mix tone\n"
                "\n"
                "-- Simple Waveshaper --\n"
                "-- 'drive' controls distortion intensity\n"
                "-- 'mix' controls wet/dry blend\n"
                "-- 'tone' controls brightness\n"
                "\n"
                "-- Apply drive\n"
                "driven = in * (1 + drive * 10)\n"
                "\n"
                "-- Soft clip using tanh\n"
                "clipped = tanh (driven)\n"
                "\n"
                "-- Blend wet/dry\n"
                "out = lerp (in, clipped, mix)\n";
        }
        else if (mode == ScriptMode::GraphBuilder)
        {
            script =
                "@mode graph\n"
                "\n"
                "-- FX Graph Builder Script --\n"
                "-- Creates a simple distortion chain and applies it to the active pedal.\n"
                "-- Available functions:\n"
                "--   varName = addNode(\"type\")     Create a DSP node\n"
                "--   connect(src, srcPort, dst, dstPort)   Wire nodes together\n"
                "--   setParam(node, \"paramName\", value)    Set a node parameter\n"
                "\n"
                "-- Create nodes\n"
                "input = addNode(\"audio_input\")\n"
                "gain = addNode(\"gain\")\n"
                "clip = addNode(\"softclip\")\n"
                "tone = addNode(\"tonestack\")\n"
                "output = addNode(\"audio_output\")\n"
                "\n"
                "-- Wire the signal chain\n"
                "connect(input, 0, gain, 0)\n"
                "connect(gain, 0, clip, 0)\n"
                "connect(clip, 0, tone, 0)\n"
                "connect(tone, 0, output, 0)\n"
                "\n"
                "-- Set initial parameters\n"
                "setParam(gain, \"gain_db\", 20)\n"
                "setParam(tone, \"bass\", 0.6)\n"
                "setParam(tone, \"mid\", 0.5)\n"
                "setParam(tone, \"treble\", 0.4)\n";
        }
        else if (mode == ScriptMode::Pedal)
        {
            script =
                "-- Pedal Design Script --\n"
                "-- Rebuilds the active pedal's chassis + controls + mappings.\n"
                "-- The DSP graph is left untouched; use FX Graph Builder for that.\n"
                "-- Functions:\n"
                "--   setMeta(name, author, category, description)\n"
                "--   setChassis(w, h, colorHex)\n"
                "--   setStyleKit(name)                            -- per-pedal StyleKit\n"
                "--   setColorway(seedARGB, mode)                  -- 0=Semantic, 1=Tint\n"
                "--   addKnob(id, x, y, label[, w, h])\n"
                "--   addSwitch / addFootswitch / addLed / addFader / addTextScreen\n"
                "--   addControl(\"type\", id, x, y, label[, w, h])  -- any type (xypad, joystick,\n"
                "--                                                   vu_meter, selector, 7seg, ...)\n"
                "--   setStyle(id, \"styleName\")                  -- per-control StyleKit override\n"
                "--   setGuard(id, n)                              -- 0=none,1=cover,2=hold,3=keylock\n"
                "--   setShift(id, \"shiftBindingName\")           -- page/mode shift binding\n"
                "--   setDefault(id, value)                        -- per-control default 0.0-1.0\n"
                "--   setPositions(id, n)                          -- selector/switch position count\n"
                "--   mapControl(controlID, \"nodeID_paramID\")\n"
                "\n"
                "setMeta(\"My Drive\", \"User\", \"Drive\", \"A simple overdrive\")\n"
                "setChassis(200, 340, 0xFF8A8A94)\n"
                "\n"
                "addKnob(\"drive\", 60, 80, \"Drive\")\n"
                "addKnob(\"tone\",  140, 80, \"Tone\")\n"
                "addKnob(\"level\", 100, 160, \"Level\")\n"
                "addLed(\"led\",    100, 30, \"\")\n"
                "addFootswitch(\"bypass\", 80, 280, \"\")\n";
        }
        else if (mode == ScriptMode::Board)
        {
            script =
                "-- Pedalboard Script --\n"
                "-- Compiles to rebuild the entire board from scratch.\n"
                "-- Functions:\n"
                "--   var = addPedal(\"Pedal Name\")            -- auto-position\n"
                "--   var = addPedal(\"Pedal Name\", x, y)      -- explicit position\n"
                "--   connect(src, srcCh, dst, dstCh)           -- audio routing\n"
                "--   setPos(var, x, y)                         -- move a pedal\n"
                "--   focus(var)                                -- target for MIDI learn\n"
                "\n"
                "boost = addPedal(\"Clean Boost\")\n"
                "drive = addPedal(\"Overdrive\")\n"
                "delay = addPedal(\"Delay\")\n"
                "\n"
                "connect(boost, 0, drive, 0)\n"
                "connect(drive, 0, delay, 0)\n"
                "\n"
                "focus(drive)\n";
        }

        codeDocument.replaceAllContent (script);
    }

    //==========================================================================
    // Helpers

    void setStatus (const juce::String& msg, juce::Colour colour)
    {
        statusLabel.setText (msg, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, colour);
    }

    void logToConsole (const juce::String& msg)
    {
        consoleOutput.moveCaretToEnd();
        consoleOutput.insertTextAtCaret (msg + "\n");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptingTabComponent)
};

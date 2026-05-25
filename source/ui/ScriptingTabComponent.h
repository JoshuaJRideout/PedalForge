#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"
#include "../dsp/ExpressionVM.h"
#include "../dsp/DSPGraph.h"
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
    }

private:
    juce::CodeDocument& document;
    juce::Colour defaultBg;
    std::vector<juce::Colour> lineToBlockColor;
    
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
    enum class ScriptMode { UI = 1, DSP = 2, GraphBuilder = 3 };

    ScriptingTabComponent () : codeDocument(), codeEditor(codeDocument, &tokeniser)
    {
        setOpaque (true);
        codeEditor.addKeyListener(this);

        // ── Mode Selector ──────────────────────────────────────────────
        modeSelector.addItem ("UI Script",        (int)ScriptMode::UI);
        modeSelector.addItem ("DSP Expression",   (int)ScriptMode::DSP);
        modeSelector.addItem ("FX Graph Builder", (int)ScriptMode::GraphBuilder);
        modeSelector.setSelectedId ((int)ScriptMode::UI, juce::dontSendNotification);
        modeSelector.addListener (this);
        modeSelector.setColour (juce::ComboBox::backgroundColourId,  juce::Colour (0xFF1F2937));
        modeSelector.setColour (juce::ComboBox::textColourId,        juce::Colour (0xFFE2E8F0));
        modeSelector.setColour (juce::ComboBox::outlineColourId,     juce::Colour (0xFF4B5563));
        addAndMakeVisible (modeSelector);

        // ── Toolbar Buttons ────────────────────────────────────────────
        for (auto* btn : { &btnCompile, &btnNew, &btnSave, &btnToggleSidebar })
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
            setStatus ("Compile successful!", juce::Colour (0xFF10B981));
            logToConsole ("UI script compiled successfully.");
        }
        else
        {
            setStatus ("Error: " + vm.getError(), juce::Colour (0xFFEF4444));
            logToConsole ("ERROR: " + vm.getError());
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
                setStatus ("Syntax valid (no Expression node to apply to)", juce::Colour (0xFF10B981));
                logToConsole ("Syntax validation passed.");
            }
            else
            {
                setStatus ("Error: " + testVM.getError(), juce::Colour (0xFFEF4444));
                logToConsole ("ERROR: " + testVM.getError());
            }
            return;
        }

        auto* exprNodeTyped = dynamic_cast<ExpressionNode*> (exprNode);
        if (exprNodeTyped)
        {
            bool ok = exprNodeTyped->setExpression (source);
            if (ok)
            {
                setStatus ("DSP expression compiled & applied!", juce::Colour (0xFF10B981));
                logToConsole ("Expression applied to node ID " + juce::String (exprNode->getNodeID()));

                // Persist to the design's effects graph
                activePedal->design->effectsGraph = graphProc->getDSPGraph().toJSON();
                engine->saveUndoState();
            }
            else
            {
                setStatus ("Error: " + exprNodeTyped->getCompileError(), juce::Colour (0xFFEF4444));
                logToConsole ("ERROR: " + exprNodeTyped->getCompileError());
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
                    logToConsole ("ERROR line " + juce::String (lineIdx + 1) + ": addNode requires a quoted type string");
                    hasError = true;
                    break;
                }

                auto node = createNodeByType (nodeType);
                if (!node)
                {
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
            else
            {
                logToConsole ("WARNING line " + juce::String (lineIdx + 1) + ": Unrecognized statement: " + line);
            }
        }

        if (hasError)
        {
            setStatus ("Graph build failed — see console", juce::Colour (0xFFEF4444));
            return;
        }

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
    // Save script to pedal instance

    void saveScript()
    {
        if (!activePedal)
        {
            logToConsole ("WARNING: No pedal selected. Script not saved.");
            setStatus ("No pedal selected", juce::Colour (0xFFFB923C));
            return;
        }

        juce::String name = scriptNameEditor.getText().trim();
        if (name.isEmpty()) name = "untitled";

        juce::String prefix;
        if (currentMode == ScriptMode::UI)          prefix = "script_ui_";
        else if (currentMode == ScriptMode::DSP)    prefix = "script_dsp_";
        else                                        prefix = "script_graph_";

        juce::String key = prefix + name;
        activePedal->controlTexts[key] = codeDocument.getAllContent();

        if (engine) engine->saveUndoState();

        setStatus ("Saved as \"" + key + "\"", juce::Colour (0xFF10B981));
        logToConsole ("Script saved: " + key);
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

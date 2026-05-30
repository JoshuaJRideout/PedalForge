#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/GridSequencerNode.h"
#include "../dsp/ExpressionVM.h"
#include "../dsp/PedalDesign.h"
#include "../dsp/MidiEditorNode.h"

class DynamicDisplayComponent : public juce::Component, public juce::Timer, public juce::TextEditor::Listener
{
public:
    DynamicDisplayComponent (juce::Component* parentOverlay, PedalInstance* inst, AudioGraphEngine* eng, const juce::String& cid)
        : parent (parentOverlay), targetInstance (inst), engine (eng), controlID (cid)
    {
        startTimerHz (30); // 30Hz for smooth playhead and debugger updates
        setMouseCursor (juce::MouseCursor::NormalCursor);

        // 1. Script/Sequencer Mode Toggle Button
        toggleModeBtn = std::make_unique<juce::TextButton> ("Mode Toggle");
        toggleModeBtn->setButtonText ("SCRIPT EDITOR");
        toggleModeBtn->setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF312E81)); // indigo
        toggleModeBtn->onClick = [this]() { toggleScriptMode(); };
        addAndMakeVisible (toggleModeBtn.get());

        // 2. Script Editor Text Box
        scriptEditor = std::make_unique<juce::TextEditor>();
        scriptEditor->setMultiLine (true, true);
        scriptEditor->setReturnKeyStartsNewLine (true);
        scriptEditor->setTabKeyUsedAsCharacter (true);
        scriptEditor->setFont (juce::FontOptions ("Monospace", 12.0f, juce::Font::plain));
        scriptEditor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF0E0B16)); // extremely dark purple
        scriptEditor->setColour (juce::TextEditor::textColourId, juce::Colour (0xFF38BDF8)); // clean cyan text
        scriptEditor->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFF8B5CF6));
        scriptEditor->setVisible (false);
        addAndMakeVisible (scriptEditor.get());

        // 3. Compile Button
        compileBtn = std::make_unique<juce::TextButton> ("Compile");
        compileBtn->setButtonText ("COMPILE & RUN");
        compileBtn->setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF10B981)); // emerald green
        compileBtn->onClick = [this]() { compileScript(); };
        compileBtn->setVisible (false);
        addAndMakeVisible (compileBtn.get());

        // 4. Status Label
        statusLabel = std::make_unique<juce::Label> ("Status");
        statusLabel->setText ("Idle", juce::dontSendNotification);
        statusLabel->setFont (juce::FontOptions ("Sans", 11.0f, juce::Font::italic));
        statusLabel->setColour (juce::Label::textColourId, juce::Colour (0xFF9CA3AF));
        statusLabel->setVisible (false);
        addAndMakeVisible (statusLabel.get());

        // Load saved script or setup default
        if (controlID == "matrix_mixer_xl_display")
        {
            cellValueEditor = std::make_unique<juce::TextEditor>();
            cellValueEditor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1F2937));
            cellValueEditor->setColour (juce::TextEditor::textColourId, juce::Colours::white);
            cellValueEditor->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF4B5563));
            cellValueEditor->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFFEC4899));
            cellValueEditor->setFont (juce::FontOptions ("Sans", 10.0f, juce::Font::bold));
            cellValueEditor->addListener (this);
            cellValueEditor->setVisible (true);
            addAndMakeVisible (cellValueEditor.get());
            
            if (auto* node = getMatrixMixerXLNode())
                cellValueEditor->setText (juce::String (node->getGain (selectedRowMatrix, selectedColMatrix), 2), false);
        }

        if (targetInstance)
        {
            juce::String savedScript = targetInstance->controlTexts[controlID + "_script"];
            if (savedScript.isEmpty() || savedScript.contains (">=") || savedScript.contains ("<="))
            {
                savedScript = 
                    "@inputs w h mouse_x mouse_y mouse_down mouse_click mouse_drag playhead selected_track\n"
                    "@outputs p1 p2\n"
                    "\n"
                    "-- Interactive XY Pad & Multi-Wave Script --\n"
                    "-- Click & drag inside the pad area to modulate sequence parameters!\n"
                    "\n"
                    "-- 1. Draw a beautiful dark space background\n"
                    "rectFill (0, 0, w, h, 1114908)   -- Deep indigo (0xFF110B1C)\n"
                    "\n"
                    "-- 2. Draw Interactive XY Pad Border\n"
                    "pad_w = w - 40\n"
                    "pad_h = h - 100\n"
                    "rectFill (20, 80, pad_w, pad_h, 2033957) -- Slate grey (0xFF1F2937)\n"
                    "rect (20, 80, pad_w, pad_h, 3223169)     -- Light border (0xFF312E81)\n"
                    "\n"
                    "-- 3. Read Mouse Interaction & set DSP parameter values\n"
                    "active_drag = cond (mouse_down, and (ge (mouse_x, 20), and (le (mouse_x, w - 20), and (ge (mouse_y, 80), le (mouse_y, h - 20)))), 0)\n"
                    "\n"
                    "-- Modulate selected track clock division (param 0) and MIDI pitch (param 2)\n"
                    "norm_x = (mouse_x - 20) / pad_w\n"
                    "norm_y = (mouse_y - 80) / pad_h\n"
                    "\n"
                    "setParam (0, cond (active_drag, clamp (norm_x * 7, 0, 7), getParam (0)))\n"
                    "setParam (2, cond (active_drag, clamp (40 + norm_y * 48, 20, 127), getParam (2)))\n"
                    "\n"
                    "-- 4. Draw interactive neon dot at active coordinate\n"
                    "dot_x = 20 + (getParam (0) / 7.0) * pad_w\n"
                    "dot_y = 80 + ((getParam (2) - 40) / 48.0) * pad_h\n"
                    "circleFill (dot_x, dot_y, 10, 15485081)  -- Pink dot (0xFFEC4899)\n"
                    "circle (dot_x, dot_y, 15, 16777215)      -- White halo (0xFFFFFF)\n"
                    "\n"
                    "-- 5. Draw decorative visual waveforms\n"
                    "line (20, 45, w - 20, 45, 1, 3223169)\n"
                    "t_scaled = t * 6.28\n"
                    "line (20, 45, w - 20, 45 + sin (t_scaled * 2.0) * 15, 1.5, 15485081)\n"
                    "\n"
                    "-- 6. Display parameter titles and live values\n"
                    "text (selected_track, 20, 15, 12, 16777215)\n"
                    "text (getParam(0), 150, 15, 12, 16777215)\n"
                    "text (getParam(2), 270, 15, 12, 16777215)\n"
                    "text (mouse_x, 400, 15, 12, 16777215)\n"
                    "text (mouse_y, 480, 15, 12, 16777215)\n";

                targetInstance->controlTexts[controlID + "_script"] = savedScript;
            }
            scriptEditor->setText (savedScript, false);
            scriptCode = savedScript;
            compileScript();
        }
    }

    ~DynamicDisplayComponent() override
    {
        stopTimer();
    }

    juce::String getControlID() const { return controlID; }

    void timerCallback() override
    {
        repaint();
    }

    void toggleScriptMode()
    {
        editScriptMode = !editScriptMode;
        
        if (controlID == "matrix_mixer_xl_display")
            toggleModeBtn->setButtonText (editScriptMode ? "MATRIX GRID" : "SCRIPT EDITOR");
        else if (controlID == "midi_editor_display")
            toggleModeBtn->setButtonText (editScriptMode ? "MIDI EDITOR" : "SCRIPT EDITOR");
        else
            toggleModeBtn->setButtonText (editScriptMode ? "SEQUENCER GRID" : "SCRIPT EDITOR");
            
        toggleModeBtn->setColour (juce::TextButton::buttonColourId, 
            editScriptMode ? juce::Colour (0xFFEC4899) : juce::Colour (0xFF312E81)); // Pink vs Indigo

        scriptEditor->setVisible (editScriptMode);
        compileBtn->setVisible (editScriptMode);
        statusLabel->setVisible (editScriptMode);
        
        if (controlID == "matrix_mixer_xl_display" && cellValueEditor)
        {
            cellValueEditor->setVisible (!editScriptMode);
        }

        resized();
        repaint();
    }

    void compileScript()
    {
        scriptCode = scriptEditor->getText();
        if (targetInstance)
            targetInstance->controlTexts[controlID + "_script"] = scriptCode;

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

        // Bind parameter getters/setters
        vm.getParamCallback = [this](int idx) -> float {
            auto* node = getGridSequencerNode();
            if (!node)
            {
                // Return sensible defaults so XY pad dot is visible
                if (idx == 0) return 3.0f;   // div  — centers dot_x
                if (idx == 1) return 16.0f;  // len
                if (idx == 2) return 64.0f;  // val1 — centers dot_y
                if (idx == 3) return 100.0f; // val2
                return 0.0f;
            }
            juce::String tr = "tr" + juce::String (selectedTrack);
            if (idx == 0) return node->getParam (tr + "_div")->get();
            if (idx == 1) return node->getParam (tr + "_len")->get();
            if (idx == 2) return node->getParam (tr + "_val1")->get();
            if (idx == 3) return node->getParam (tr + "_val2")->get();
            return 0.0f;
        };

        vm.setParamCallback = [this](int idx, float val) {
            juce::String suffix;
            if (idx == 0) suffix = "div";
            else if (idx == 1) suffix = "len";
            else if (idx == 2) suffix = "val1";
            else if (idx == 3) suffix = "val2";
            
            if (suffix.isNotEmpty())
                setDSPParameter (suffix, selectedTrack, val);
        };

        vm.drawImageCallback = [this](juce::Graphics& g, float imgIdx, float x, float y, float w, float h) {
            g.setColour (juce::Colours::white);
            if (imgIdx == 0) // Play Icon
            {
                juce::Path p;
                p.addTriangle (x, y, x + w, y + h * 0.5f, x, y + h);
                g.fillPath (p);
            }
            else if (imgIdx == 1) // Pause/Stop Icon
            {
                g.fillRect (x, y, w, h);
            }
            else if (imgIdx == 2) // Settings / Circle Icon
            {
                g.fillEllipse (x, y, w, h);
            }
            else
            {
                g.drawRect (x, y, w, h);
            }
        };

        bool ok = vm.compile (scriptCode);
        if (ok)
        {
            statusLabel->setText ("Compile Successful!", juce::dontSendNotification);
            statusLabel->setColour (juce::Label::textColourId, juce::Colour (0xFF10B981)); // green
        }
        else
        {
            statusLabel->setText ("Error: " + vm.getError(), juce::dontSendNotification);
            statusLabel->setColour (juce::Label::textColourId, juce::Colour (0xFFEF4444)); // red
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        if (controlID == "midi_editor_display")
        {
            if (!editScriptMode)
            {
                paintMidiEditor (g);
                return;
            }
        }

        if (controlID == "matrix_mixer_xl_display")
        {
            if (!editScriptMode)
            {
                paintMatrixMixerXL (g);
                return;
            }
        }

        if (editScriptMode)
        {
            // Draw background for Editor Mode
            g.setColour (juce::Colour (0xFF0E0B16));
            g.fillAll();
            
            // Draw Transport Header
            g.setColour (juce::Colour (0xFF1A1726));
            g.fillRect (0.0f, 0.0f, bounds.getWidth(), 40.0f);
            g.setColour (juce::Colour (0xFF312E81));
            g.drawHorizontalLine (40, 0.0f, bounds.getWidth());

            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions ("Sans", 12.0f, juce::Font::bold));
            g.drawText ("LUA / EXPRESSION UI SCRIPT EDITOR", 15.0f, 8.0f, 300.0f, 28.0f, juce::Justification::centredLeft);

            // Draw Real-time Canvas Frame on the right
            float canvasX = bounds.getWidth() / 2 + 10.0f;
            float canvasY = 45.0f;
            float canvasW = bounds.getWidth() / 2 - 25.0f;
            float canvasH = bounds.getHeight() - 60.0f;
            
            juce::Rectangle<float> canvasRect (canvasX, canvasY, canvasW, canvasH);
            
            // Draw frame shadow
            g.setColour (juce::Colour (0xFF1E1B4B));
            g.fillRoundedRectangle (canvasRect, 6.0f);
            g.setColour (juce::Colour (0xFF8B5CF6).withAlpha (0.4f));
            g.drawRoundedRectangle (canvasRect, 6.0f, 1.5f);

            // Evaluate the Script in the real-time canvas
            if (vm.isCompiled())
            {
                // Clip drawing to canvas bounds
                juce::Graphics::ScopedSaveState saveState (g);
                g.reduceClipRegion (canvasRect.toNearestInt());
                
                // Offset Graphics so script draws relative to canvas top-left
                g.addTransform (juce::AffineTransform::translation (canvasX, canvasY));

                // Populate VM variables
                vm.vars[vm.getVarIndex("w")] = canvasW;
                vm.vars[vm.getVarIndex("h")] = canvasH;
                
                // Offset mouse relative to canvas
                float mx = (float)lastMousePos.x - canvasX;
                float my = (float)lastMousePos.y - canvasY;
                vm.vars[vm.getVarIndex("mouse_x")] = mx;
                vm.vars[vm.getVarIndex("mouse_y")] = my;
                
                vm.vars[vm.getVarIndex("mouse_down")]  = isMouseDownState ? 1.0f : 0.0f;
                vm.vars[vm.getVarIndex("mouse_click")] = isMouseClickedState ? 1.0f : 0.0f;
                vm.vars[vm.getVarIndex("mouse_drag")]  = isMouseDraggedState ? 1.0f : 0.0f;
                
                vm.vars[vm.getVarIndex("selected_track")] = (float)selectedTrack;
                
                auto* seqNode = getGridSequencerNode();
                vm.vars[vm.getVarIndex("playhead")] = seqNode ? (float)seqNode->getCurrentStep (selectedTrack) : 0.0f;
                vm.vars[vm.getVarIndex("bpm")]      = seqNode ? seqNode->getParam("bpm")->get() : 120.0f;
                vm.vars[vm.getVarIndex("run")]      = seqNode ? seqNode->getParam("run")->get() : 0.0f;
                
                vm.vars[vm.getVarIndex("time")] = (float)(juce::Time::getMillisecondCounter() % 100000) / 1000.0f;
                vm.vars[vm.getVarIndex("t")]    = vm.vars[vm.getVarIndex("time")];

                // Bind current graphics context
                vm.currentGraphics = &g;
                
                // Evaluate!
                vm.evaluate();
                
                // Reset click state after evaluation
                isMouseClickedState = false;
            }
            else
            {
                g.setColour (juce::Colour (0xFFEF4444));
                g.setFont (juce::FontOptions ("Sans", 12.0f, juce::Font::italic));
                g.drawText ("Script Not Compiled or Has Errors", canvasRect, juce::Justification::centred);
            }
        }
        else
        {
            paintSequencer (g);
        }
    }

    void paintSequencer (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw deep panel backdrop
        g.setColour (juce::Colour (0xFF110B1C)); // dark purple/indigo
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xFF312E81).withAlpha (0.5f)); // indigo border
        g.drawRoundedRectangle (bounds, 6.0f, 1.5f);

        // Fetch GridSequencerNode from engine
        auto* seqNode = getGridSequencerNode();
        if (!seqNode)
        {
            g.setColour (juce::Colours::white);
            g.drawText ("Step Sequencer Node Not Found", bounds, juce::Justification::centred);
            return;
        }

        // Draw transport and options bar at the top
        drawTransportBar (g);

        // Grid dimensions
        float labelW = 130.0f;
        float topBarH = 45.0f;
        float rowH = (bounds.getHeight() - topBarH - 5.0f) / 8.0f;
        float gridW = bounds.getWidth() - labelW - 10.0f;
        float colW = gridW / 32.0f;

        for (int t = 0; t < 8; ++t)
        {
            float rowY = topBarH + t * rowH;
            
            // Read parameters for track
            juce::String tr = "tr" + juce::String(t);
            float lenVal = seqNode->getParam (tr + "_len")->get();
            int loopLen = juce::jlimit (1, 32, (int)lenVal);
            int mode = juce::jlimit (0, 3, (int)seqNode->getParam (tr + "_mode")->get());
            int val1 = (int) seqNode->getParam (tr + "_val1")->get();
            int divIdx = juce::jlimit (0, 7, (int)seqNode->getParam (tr + "_div")->get());
            
            juce::String divStrings[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" };
            juce::String modeStrings[] = { "Note", "CC", "PC", "Expr" };

            // Highlight if selected track
            bool isSelected = (t == selectedTrack);
            g.setColour (isSelected ? juce::Colour (0xFF312E81).withAlpha (0.4f) : juce::Colours::transparentBlack);
            g.fillRect (0.0f, rowY, bounds.getWidth(), rowH);

            // Highlight hover on label parts
            bool isMouseOverLabel = (lastMousePos.x < labelW && lastMousePos.y >= topBarH);
            int hoveredTrack = isMouseOverLabel ? (int)((lastMousePos.y - topBarH) / rowH) : -1;

            if (hoveredTrack == t)
            {
                float relativeY = lastMousePos.y - rowY;
                bool isTopHalfHovered = (relativeY < rowH * 0.5f);
                g.setColour (juce::Colour (0xFF312E81).withAlpha (0.5f)); // subtle hover highlight
                
                if (isTopHalfHovered)
                    g.fillRoundedRectangle (6.0f, rowY + 3.0f, labelW - 12.0f, rowH * 0.5f - 4.0f, 4.0f);
                else
                    g.fillRoundedRectangle (6.0f, rowY + rowH * 0.5f + 1.0f, labelW - 12.0f, rowH * 0.5f - 4.0f, 4.0f);
            }

            // Draw Track Info Label
            g.setColour (isSelected ? juce::Colour (0xFFE879F9) : juce::Colour (0xFF9CA3AF)); // magenta highlight
            g.setFont (juce::FontOptions ("Sans", 11.0f, juce::Font::bold));
            
            juce::String trackInfo = "Tr " + juce::String (t + 1) + "  " + modeStrings[mode];
            if (mode == 0) // Note Mode
                trackInfo += " (" + getMidiNoteName(val1) + juce::String (juce::CharPointer_UTF8 (") \xe2\x96\xbc"));
            else if (mode == 1) // CC
                trackInfo += " (CC" + juce::String(val1) + juce::String (juce::CharPointer_UTF8 (") \xe2\x96\xbc"));
            else
                trackInfo += juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbc"));
            
            g.drawText (trackInfo, 10.0f, rowY, labelW - 15.0f, rowH * 0.5f, juce::Justification::bottomLeft);
            
            g.setColour (juce::Colour (0xFF6B7280));
            g.setFont (juce::FontOptions ("Sans", 9.0f, juce::Font::plain));
            g.drawText (divStrings[divIdx] + juce::String (juce::CharPointer_UTF8 (" div \xe2\x96\xbc  -  ")) + juce::String(loopLen) + " steps", 10.0f, rowY + rowH * 0.5f, labelW - 15.0f, rowH * 0.5f, juce::Justification::topLeft);

            // Draw Grid Cells
            int playheadStep = seqNode->getCurrentStep (t);

            for (int s = 0; s < 32; ++s)
            {
                float cellX = labelW + s * colW;
                juce::Rectangle<float> cellRect (cellX + 2.0f, rowY + 3.0f, colW - 4.0f, rowH - 6.0f);
                
                bool active = seqNode->getParam (tr + "_s" + juce::String(s))->get() > 0.5f;
                bool disabled = (s >= loopLen);

                if (disabled)
                {
                    // Greyed out disabled cell
                    g.setColour (juce::Colour (0xFF1F2937).withAlpha (0.3f));
                    g.fillRoundedRectangle (cellRect, 3.0f);
                    g.setColour (juce::Colour (0xFF374151).withAlpha (0.3f));
                    g.drawRoundedRectangle (cellRect, 3.0f, 1.0f);
                }
                else
                {
                    // Regular active/inactive cell
                    if (active)
                    {
                        // Color code based on beats (4 steps = 1 beat)
                        juce::Colour cellCol = ((s / 4) % 2 == 0) ? juce::Colour (0xFF8B5CF6) : juce::Colour (0xFF6366F1); // purple / indigo
                        g.setColour (cellCol);
                        g.fillRoundedRectangle (cellRect, 3.0f);
                        
                        // Active inner glow
                        g.setColour (juce::Colours::white.withAlpha (0.4f));
                        g.drawRoundedRectangle (cellRect.reduced(1.0f), 3.0f, 1.0f);
                    }
                    else
                    {
                        // Inactive cell
                        g.setColour (juce::Colour (0xFF1F2937));
                        g.fillRoundedRectangle (cellRect, 3.0f);
                        g.setColour (juce::Colour (0xFF374151));
                        g.drawRoundedRectangle (cellRect, 3.0f, 1.0f);
                    }

                    // Render Playhead highlight overlay
                    if (playheadStep == s)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.4f));
                        g.fillRoundedRectangle (cellRect, 3.0f);
                        g.setColour (juce::Colour (0xFFFCD34D)); // golden halo
                        g.drawRoundedRectangle (cellRect, 3.0f, 1.5f);
                    }
                }
            }

            // Draw sequence boundary vertical bracket line
            float boundaryX = labelW + loopLen * colW;
            g.setColour (juce::Colour (0xFFEC4899)); // vibrant pink
            g.drawVerticalLine ((int)boundaryX, rowY + 1.0f, rowY + rowH - 1.0f);
            
            // Draw visual resizing handle notches on the boundary line
            g.fillRect (boundaryX - 2.0f, rowY + rowH * 0.5f - 4.0f, 4.0f, 8.0f);
            
            // Draw horizontal row separator
            g.setColour (juce::Colour (0xFF312E81).withAlpha (0.3f));
            g.drawHorizontalLine ((int)(rowY + rowH), 10.0f, bounds.getWidth() - 10.0f);
        }
    }

    void drawTransportBar (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float topBarH = 45.0f;
        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;

        // Transport background
        g.setColour (juce::Colour (0xFF1E1B4B)); // deep dark indigo
        g.fillRect (0.0f, 0.0f, bounds.getWidth(), topBarH);
        g.setColour (juce::Colour (0xFF312E81));
        g.drawHorizontalLine ((int)topBarH, 0.0f, bounds.getWidth());

        // Read play and BPM state
        bool isRunning = seqNode->getParam ("run")->get() > 0.5f;
        float bpm = seqNode->getParam ("bpm")->get();

        // 1. Play Button
        playBtnRect = juce::Rectangle<float> (15.0f, 8.0f, 80.0f, 28.0f);
        g.setColour (isRunning ? juce::Colour (0xFF10B981) : juce::Colour (0xFF4B5563)); // emerald vs grey
        g.fillRoundedRectangle (playBtnRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions ("Sans", 11.0f, juce::Font::bold));
        g.drawText (isRunning ? "STOP" : "RUN", playBtnRect, juce::Justification::centred);

        // 2. BPM Slider/Drag Box
        bpmRect = juce::Rectangle<float> (110.0f, 8.0f, 100.0f, 28.0f);
        g.setColour (juce::Colour (0xFF312E81));
        g.fillRoundedRectangle (bpmRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText ("BPM: " + juce::String ((int)bpm), bpmRect, juce::Justification::centred);

        // 3. Global Output Mode Button
        modeBtnRect = juce::Rectangle<float> (225.0f, 8.0f, 120.0f, 28.0f);
        g.setColour (juce::Colour (0xFF312E81)); // indigo
        g.fillRoundedRectangle (modeBtnRect, 4.0f);
        g.setColour (juce::Colours::white);
        
        juce::String modeStrings[] = { "Mode: Note", "Mode: CC", "Mode: PC", "Mode: Expr" };
        int activeMode = juce::jlimit (0, 3, (int)seqNode->getParam ("tr0_mode")->get());
        g.drawText (modeStrings[activeMode] + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbc")), modeBtnRect, juce::Justification::centred);

        // 4. Clear Button
        clearBtnRect = juce::Rectangle<float> (360.0f, 8.0f, 90.0f, 28.0f);
        g.setColour (juce::Colour (0xFFEF4444).withAlpha (0.8f)); // soft red
        g.fillRoundedRectangle (clearBtnRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText ("CLEAR TR", clearBtnRect, juce::Justification::centred);

        // 5. Instructions/Tip Label
        g.setColour (juce::Colour (0xFF9CA3AF));
        g.setFont (juce::FontOptions ("Sans", 9.5f, juce::Font::italic));
        g.drawText ("Tip: Left-click labels for Note/Div. Right-click track for full menu.",
                    470.0f, 8.0f, bounds.getWidth() - 610.0f, 28.0f, juce::Justification::centredLeft, true);
    }

    void mouseInteraction (const juce::MouseEvent& e, bool down, bool click, bool drag)
    {
        lastMousePos = e.position;
        isMouseDownState = down;
        isMouseClickedState = click;
        isMouseDraggedState = drag;
        
        if (editScriptMode)
        {
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        mouseInteraction (e, true, true, false);
        if (editScriptMode) return;

        if (controlID == "midi_editor_display")
        {
            mouseDownMidiEditor (e);
            return;
        }
        if (controlID == "matrix_mixer_xl_display")
        {
            mouseDownMatrixMixerXL (e);
            return;
        }

        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;

        auto bounds = getLocalBounds().toFloat();
        float labelW = 130.0f;
        float topBarH = 45.0f;
        float rowH = (bounds.getHeight() - topBarH - 5.0f) / 8.0f;
        float gridW = bounds.getWidth() - labelW - 10.0f;
        float colW = gridW / 32.0f;

        // ── 1. Check Transport Bar Clicks ────────────────────────────────────
        if (playBtnRect.contains (e.position))
        {
            bool isRunning = seqNode->getParam ("run")->get() > 0.5f;
            setGeneralParameter ("run", isRunning ? 0.0f : 1.0f);
            repaint();
            return;
        }

        if (bpmRect.contains (e.position))
        {
            draggedBPM = true;
            draggedBPMStartVal = seqNode->getParam ("bpm")->get();
            return;
        }

        if (clearBtnRect.contains (e.position))
        {
            float currentClear = seqNode->getParam ("clear")->get();
            setGeneralParameter ("clear", currentClear > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }

        if (modeBtnRect.contains (e.position))
        {
            showGlobalModeMenu (e.position);
            return;
        }

        // ── 2. Check Row boundary dragging or track selection ────────────────
        if (e.x < labelW)
        {
            int rowIdx = (int)((e.y - topBarH) / rowH);
            if (rowIdx >= 0 && rowIdx < 8)
            {
                selectedTrack = rowIdx;
                setGeneralParameter ("sel_track", (float)selectedTrack);
                
                if (e.mods.isRightButtonDown())
                {
                    showTrackContextMenu (selectedTrack, e.position);
                }
                else
                {
                    float rowY = topBarH + rowIdx * rowH;
                    float relativeY = e.y - rowY;
                    
                    if (relativeY < rowH * 0.5f)
                    {
                        // Clicked top half: show Note / CC / PC selector
                        showTrackValue1Menu (selectedTrack, e.position);
                    }
                    else
                    {
                        // Clicked bottom half: show Division menu
                        showTrackDivisionMenu (selectedTrack, e.position);
                    }
                }
                
                repaint();
            }
            return;
        }

        // ── 3. Check Sequence length boundary resizing ───────────────────────
        int resizingTrack = hitTestResizingBoundary (e.position);
        if (resizingTrack >= 0)
        {
            draggedBoundaryTrack = resizingTrack;
            return;
        }

        // ── 4. Paint steps on dragging ───────────────────────────────────────
        int gridX = (int)((e.x - labelW) / colW);
        int gridY = (int)((e.y - topBarH) / rowH);
        
        if (gridX >= 0 && gridX < 32 && gridY >= 0 && gridY < 8)
        {
            juce::String tr = "tr" + juce::String(gridY);
            float lenVal = seqNode->getParam (tr + "_len")->get();
            if (gridX < (int)lenVal)
            {
                bool currentState = seqNode->getParam (tr + "_s" + juce::String (gridX))->get() > 0.5f;
                bool newState = !currentState;
                
                setDSPParameter ("s" + juce::String (gridX), gridY, newState ? 1.0f : 0.0f);
                
                paintDrawState = newState;
                draggedPaintTrack = gridY;
                draggedPaintLastCol = gridX;
                repaint();
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        mouseInteraction (e, true, false, true);
        if (editScriptMode) return;

        if (controlID == "midi_editor_display")
        {
            mouseDragMidiEditor (e);
            return;
        }
        if (controlID == "matrix_mixer_xl_display")
        {
            mouseDragMatrixMixerXL (e);
            return;
        }

        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;

        auto bounds = getLocalBounds().toFloat();
        float labelW = 130.0f;
        float topBarH = 45.0f;
        float rowH = (bounds.getHeight() - topBarH - 5.0f) / 8.0f;
        float gridW = bounds.getWidth() - labelW - 10.0f;
        float colW = gridW / 32.0f;

        // ── 1. Handle BPM Dragging ───────────────────────────────────────────
        if (draggedBPM)
        {
            float dragSensitivity = 1.5f;
            float delta = -e.getDistanceFromDragStartY() / dragSensitivity;
            float newBPM = juce::jlimit (20.0f, 300.0f, draggedBPMStartVal + delta);
            setGeneralParameter ("bpm", newBPM);
            repaint();
            return;
        }

        // ── 2. Handle Boundary Resizing Drag ─────────────────────────────────
        if (draggedBoundaryTrack >= 0)
        {
            int colIndex = (int) std::round ((e.x - labelW) / colW);
            int newLen = juce::jlimit (1, 32, colIndex);
            
            setDSPParameter ("len", draggedBoundaryTrack, (float)newLen);
            repaint();
            return;
        }

        // ── 3. Handle Drag-to-Paint Steps ────────────────────────────────────
        if (draggedPaintTrack >= 0)
        {
            int colIndex = (int)((e.x - labelW) / colW);
            int rowIndex = (int)((e.y - topBarH) / rowH);

            if (rowIndex == draggedPaintTrack && colIndex >= 0 && colIndex < 32 && colIndex != draggedPaintLastCol)
            {
                juce::String tr = "tr" + juce::String (draggedPaintTrack);
                float lenVal = seqNode->getParam (tr + "_len")->get();
                
                if (colIndex < (int)lenVal)
                {
                    setDSPParameter ("s" + juce::String (colIndex), draggedPaintTrack, paintDrawState ? 1.0f : 0.0f);
                    draggedPaintLastCol = colIndex;
                    repaint();
                }
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        mouseInteraction (e, false, false, false);
        if (editScriptMode) return;

        if (controlID == "midi_editor_display")
        {
            mouseUpMidiEditor (e);
            return;
        }
        if (controlID == "matrix_mixer_xl_display")
        {
            mouseUpMatrixMixerXL (e);
            return;
        }
        
        draggedBPM = false;
        draggedBoundaryTrack = -1;
        draggedPaintTrack = -1;
        draggedPaintLastCol = -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        lastMousePos = e.position;
        if (editScriptMode)
        {
            repaint();
            return;
        }

        if (controlID == "midi_editor_display")
        {
            mouseMoveMidiEditor (e);
            return;
        }
        if (controlID == "matrix_mixer_xl_display")
        {
            mouseMoveMatrixMixerXL (e);
            return;
        }

        int resTrack = hitTestResizingBoundary (e.position);
        if (resTrack >= 0)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        }
        else if (e.x < 130.0f && e.y >= 45.0f) // e.x < labelW && e.y >= topBarH
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        else
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
        repaint();
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        
        // Position Mode Toggle Button in the transport bar
        toggleModeBtn->setBounds (bounds.getWidth() - 135, 8, 120, 28);

        if (editScriptMode)
        {
            int editorW = bounds.getWidth() / 2 - 20;
            int editorH = bounds.getHeight() - 95;
            scriptEditor->setBounds (15, 45, editorW, editorH);
            compileBtn->setBounds (15, bounds.getHeight() - 40, 150, 28);
            statusLabel->setBounds (180, bounds.getHeight() - 40, editorW - 170, 28);
        }
    }

    std::function<void()> onParamChanged;

private:
    juce::Component* parent = nullptr;
    PedalInstance* targetInstance = nullptr;
    AudioGraphEngine* engine = nullptr;
    juce::String controlID;

    // Hit boundaries for controls
    juce::Rectangle<float> playBtnRect;
    juce::Rectangle<float> bpmRect;
    juce::Rectangle<float> clearBtnRect;
    juce::Rectangle<float> modeBtnRect;

    // Selected track index
    int selectedTrack = 0;

    // Interaction states
    bool draggedBPM = false;
    float draggedBPMStartVal = 120.0f;

    int draggedBoundaryTrack = -1;
    
    int draggedPaintTrack = -1;
    int draggedPaintLastCol = -1;
    bool paintDrawState = true; // paint ON or OFF

    // ── Scriptable Engine Extensions ──────────────────────────────────────────
    bool editScriptMode = false;
    juce::String scriptCode;
    ExpressionVM vm;
    juce::Point<float> lastMousePos;
    bool isMouseDownState = false;
    bool isMouseClickedState = false;
    bool isMouseDraggedState = false;

    std::unique_ptr<juce::TextButton> toggleModeBtn;
    std::unique_ptr<juce::TextEditor> scriptEditor;
    std::unique_ptr<juce::TextButton> compileBtn;
    std::unique_ptr<juce::Label> statusLabel;

    GridSequencerNode* getGridSequencerNode()
    {
        if (!targetInstance || !engine) return nullptr;
        
        auto* node = engine->getGraph().getNodeForId (targetInstance->nodeID);
        if (!node) return nullptr;
        
        auto* graphProc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor());
        if (!graphProc) return nullptr;
        
        for (const auto& [id, dspNode] : graphProc->getDSPGraph().getNodes())
        {
            if (dspNode && dspNode->getType() == "grid_sequencer")
                return dynamic_cast<GridSequencerNode*> (dspNode.get());
        }
        return nullptr;
    }

    MidiEditorNode* getMidiEditorNode()
    {
        if (!targetInstance || !engine) return nullptr;
        
        auto* node = engine->getGraph().getNodeForId (targetInstance->nodeID);
        if (!node) return nullptr;
        
        auto* graphProc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor());
        if (!graphProc) return nullptr;
        
        for (const auto& [id, dspNode] : graphProc->getDSPGraph().getNodes())
        {
            if (dspNode && dspNode->getType() == "midi_editor")
                return dynamic_cast<MidiEditorNode*> (dspNode.get());
        }
        return nullptr;
    }

    MatrixMixerXLNode* getMatrixMixerXLNode()
    {
        if (!targetInstance || !engine) return nullptr;
        
        auto* node = engine->getGraph().getNodeForId (targetInstance->nodeID);
        if (!node) return nullptr;
        
        auto* graphProc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor());
        if (!graphProc) return nullptr;
        
        for (const auto& [id, dspNode] : graphProc->getDSPGraph().getNodes())
        {
            if (dspNode && dspNode->getType() == "matrix_mixer_xl")
                return dynamic_cast<MatrixMixerXLNode*> (dspNode.get());
        }
        return nullptr;
    }

    void setMidiEditorParameter (const juce::String& paramName, float val)
    {
        auto* midiNode = getMidiEditorNode();
        if (!midiNode) return;
        
        juce::String targetParamID = juce::String (midiNode->getNodeID()) + "_" + paramName;
        
        if (targetInstance && engine)
        {
            if (auto* parentNode = engine->getGraph().getNodeForId (targetInstance->nodeID))
            {
                if (auto* proc = parentNode->getProcessor())
                {
                    for (auto* p : proc->getParameters())
                    {
                        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                        {
                            if (matchMappingParam (targetParamID, ranged->getParameterID()))
                            {
                                float normVal = ranged->getNormalisableRange().convertTo0to1 (val);
                                ranged->setValueNotifyingHost (normVal);
                                if (auto* np = midiNode->getParam (paramName))
                                    np->set (val);
                                return;
                            }
                        }
                    }
                }
            }
        }
        
        if (auto* p = midiNode->getParam (paramName))
            p->set (val);
    }

    void setDSPParameter (const juce::String& paramSuffix, int trackIdx, float val)
    {
        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;
        
        juce::String tr = "tr" + juce::String (trackIdx);
        juce::String targetParamID = juce::String (seqNode->getNodeID()) + "_" + tr + "_" + paramSuffix;
        
        if (targetInstance && engine)
        {
            if (auto* parentNode = engine->getGraph().getNodeForId (targetInstance->nodeID))
            {
                if (auto* proc = parentNode->getProcessor())
                {
                    for (auto* p : proc->getParameters())
                    {
                        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                        {
                            if (matchMappingParam (targetParamID, ranged->getParameterID()))
                            {
                                float normVal = ranged->getNormalisableRange().convertTo0to1 (val);
                                ranged->setValueNotifyingHost (normVal);
                                // Sync/Fallback immediately to avoid snapback race conditions
                                if (auto* np = seqNode->getParam (tr + "_" + paramSuffix))
                                    np->set (val);
                                return;
                            }
                        }
                    }
                }
            }
        }
        
        // Fallback
        if (auto* p = seqNode->getParam (tr + "_" + paramSuffix))
            p->set (val);
    }

    void setGeneralParameter (const juce::String& paramName, float val)
    {
        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;
        
        juce::String targetParamID = juce::String (seqNode->getNodeID()) + "_" + paramName;
        
        if (targetInstance && engine)
        {
            if (auto* parentNode = engine->getGraph().getNodeForId (targetInstance->nodeID))
            {
                if (auto* proc = parentNode->getProcessor())
                {
                    for (auto* p : proc->getParameters())
                    {
                        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                        {
                            if (matchMappingParam (targetParamID, ranged->getParameterID()))
                            {
                                float normVal = ranged->getNormalisableRange().convertTo0to1 (val);
                                ranged->setValueNotifyingHost (normVal);
                                // Sync/Fallback immediately to avoid snapback race conditions
                                if (auto* np = seqNode->getParam (paramName))
                                    np->set (val);
                                return;
                            }
                        }
                    }
                }
            }
        }
        
        // Fallback
        if (auto* p = seqNode->getParam (paramName))
            p->set (val);
    }

    int hitTestResizingBoundary (juce::Point<float> cp)
    {
        auto bounds = getLocalBounds().toFloat();
        float labelW = 130.0f;
        float topBarH = 45.0f;
        float rowH = (bounds.getHeight() - topBarH - 5.0f) / 8.0f;
        float gridW = bounds.getWidth() - labelW - 10.0f;
        float colW = gridW / 32.0f;

        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return -1;

        for (int t = 0; t < 8; ++t)
        {
            float rowY = topBarH + t * rowH;
            juce::String tr = "tr" + juce::String(t);
            float lenVal = seqNode->getParam (tr + "_len")->get();
            float boundaryX = labelW + lenVal * colW;

            // Comfortable 20px hit zone (+/- 10px) for grabbing vertical boundaries
            if (cp.x >= boundaryX - 10.0f && cp.x <= boundaryX + 10.0f &&
                cp.y >= rowY && cp.y <= rowY + rowH)
            {
                return t;
            }
        }
        return -1;
    }

    juce::String getMidiNoteName (int noteNum)
    {
        juce::String notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int octave = (noteNum / 12) - 1;
        int noteIdx = noteNum % 12;
        return notes[noteIdx] + juce::String (octave);
    }

    void showTrackContextMenu (int trackIdx, juce::Point<float> cp)
    {
        juce::PopupMenu menu;

        // 1. Division Submenu
        juce::PopupMenu divMenu;
        juce::String divStrings[] = { "1/1 Note", "1/2 Note", "1/4 Note (Quarter)", "1/8 Note (Eighth)", "1/16 Note (Sixteenth)", "1/32 Note", "1/64 Note", "1/128 Note" };
        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;
        juce::String tr = "tr" + juce::String (trackIdx);
        int activeDiv = (int) seqNode->getParam (tr + "_div")->get();
        for (int i = 0; i < 8; ++i)
            divMenu.addItem (10 + i, divStrings[i], true, i == activeDiv);
        menu.addSubMenu ("Change Clock Division", divMenu);

        // 2. Mode Submenu
        juce::PopupMenu modeMenu;
        juce::String modeStrings[] = { "Note Output (Plays Tone)", "CC Output (Controller modulation)", "PC Output (Program change)", "CV Gate Output (Expression/Pulsing)" };
        int activeMode = (int) seqNode->getParam (tr + "_mode")->get();
        for (int i = 0; i < 4; ++i)
            modeMenu.addItem (20 + i, modeStrings[i], true, i == activeMode);
        menu.addSubMenu ("Change Track Mode", modeMenu);

        // 3. Pitch / CC Number Submenu
        juce::PopupMenu pitchMenu;
        int activePitch = (int) seqNode->getParam (tr + "_val1")->get();
        
        int scaleNotes[] = { 48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84 };
        for (int i = 0; i < 22; ++i)
        {
            int noteNum = scaleNotes[i];
            pitchMenu.addItem (100 + noteNum, getMidiNoteName (noteNum) + " (" + juce::String (noteNum) + ")", true, noteNum == activePitch);
        }
        menu.addSubMenu ("Change MIDI Note / CC Pitch", pitchMenu);

        // Show the menu asynchronously
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> ((int)cp.x, (int)cp.y, 1, 1)),
            [this, trackIdx](int r) {
                if (r >= 10 && r < 18) // division selection
                {
                    setDSPParameter ("div", trackIdx, (float)(r - 10));
                }
                else if (r >= 20 && r < 24) // mode selection
                {
                    setDSPParameter ("mode", trackIdx, (float)(r - 20));
                }
                else if (r >= 100 && r < 227) // pitch selection
                {
                    setDSPParameter ("val1", trackIdx, (float)(r - 100));
                }
                repaint();
            });
    }

    void showTrackValue1Menu (int trackIdx, juce::Point<float> cp)
    {
        juce::PopupMenu menu;
        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;
        
        juce::String tr = "tr" + juce::String (trackIdx);
        int mode = juce::jlimit (0, 3, (int)seqNode->getParam (tr + "_mode")->get());
        int activeVal = (int) seqNode->getParam (tr + "_val1")->get();

        if (mode == 0) // Note Mode: Full Piano Grouped by Octave
        {
            for (int oct = -1; oct <= 9; ++oct)
            {
                juce::PopupMenu octMenu;
                int startNote = (oct + 1) * 12;
                int endNote = startNote + 11;
                
                for (int noteNum = startNote; noteNum <= endNote; ++noteNum)
                {
                    if (noteNum > 127) break;
                    
                    juce::String name = getMidiNoteName (noteNum) + " (" + juce::String (noteNum) + ")";
                    octMenu.addItem (100 + noteNum, name, true, noteNum == activeVal);
                }
                
                juce::String octTitle = "Octave " + juce::String (oct);
                if (oct == 4) octTitle += " (Middle C)";
                
                menu.addSubMenu (octTitle, octMenu);
            }
        }
        else if (mode == 1) // CC Mode: 0 - 127 in blocks of 16
        {
            for (int block = 0; block < 8; ++block)
            {
                juce::PopupMenu blockMenu;
                int startCC = block * 16;
                int endCC = startCC + 15;
                
                for (int ccNum = startCC; ccNum <= endCC; ++ccNum)
                {
                    blockMenu.addItem (100 + ccNum, "CC " + juce::String (ccNum), true, ccNum == activeVal);
                }
                
                juce::String blockTitle = "CCs " + juce::String (startCC) + " - " + juce::String (endCC);
                menu.addSubMenu (blockTitle, blockMenu);
            }
        }
        else if (mode == 2) // PC Mode: 0 - 127 in blocks of 16
        {
            for (int block = 0; block < 8; ++block)
            {
                juce::PopupMenu blockMenu;
                int startPC = block * 16;
                int endPC = startPC + 15;
                
                for (int pcNum = startPC; pcNum <= endPC; ++pcNum)
                {
                    blockMenu.addItem (100 + pcNum, "Prog " + juce::String (pcNum), true, pcNum == activeVal);
                }
                
                juce::String blockTitle = "Progs " + juce::String (startPC) + " - " + juce::String (endPC);
                menu.addSubMenu (blockTitle, blockMenu);
            }
        }
        else
        {
            return;
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> ((int)cp.x, (int)cp.y, 1, 1)),
            [this, trackIdx](int r) {
                if (r >= 100 && r < 228)
                {
                    setDSPParameter ("val1", trackIdx, (float)(r - 100));
                }
                repaint();
            });
    }

    void showGlobalModeMenu (juce::Point<float> cp)
    {
        juce::PopupMenu menu;
        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;
        
        int activeMode = (int) seqNode->getParam ("tr0_mode")->get();
        juce::String modeStrings[] = { "Note Output (Plays Tone)", "CC Output (Controller modulation)", "PC Output (Program change)", "CV Gate Output (Expression/Pulsing)" };
        for (int i = 0; i < 4; ++i)
            menu.addItem (1 + i, modeStrings[i], true, i == activeMode);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> ((int)cp.x, (int)cp.y, 1, 1)),
            [this](int r) {
                if (r >= 1 && r <= 4)
                {
                    int newMode = r - 1;
                    for (int t = 0; t < 8; ++t)
                        setDSPParameter ("mode", t, (float)newMode);
                    repaint();
                }
            });
    }

    void showTrackDivisionMenu (int trackIdx, juce::Point<float> cp)
    {
        juce::PopupMenu menu;
        auto* seqNode = getGridSequencerNode();
        if (!seqNode) return;
        
        juce::String tr = "tr" + juce::String (trackIdx);
        int activeDiv = (int) seqNode->getParam (tr + "_div")->get();

        juce::String divStrings[] = { "1/1 Note", "1/2 Note", "1/4 Note (Quarter)", "1/8 Note (Eighth)", "1/16 Note (Sixteenth)", "1/32 Note", "1/64 Note", "1/128 Note" };
        for (int i = 0; i < 8; ++i)
            menu.addItem (10 + i, divStrings[i], true, i == activeDiv);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> ((int)cp.x, (int)cp.y, 1, 1)),
            [this, trackIdx](int r) {
                if (r >= 10 && r < 18) // division selection
                {
                    setDSPParameter ("div", trackIdx, (float)(r - 10));
                }
                repaint();
            });
    }

    double getSnapBeats() const
    {
        auto* node = const_cast<DynamicDisplayComponent*>(this)->getMidiEditorNode();
        if (!node) return 0.25; // default 1/16
        int snapIdx = (int)node->getParam("snap")->get();
        if (snapIdx == 0) return 0.01; // fine snap
        if (snapIdx == 1) return 1.0;  // 1/4
        if (snapIdx == 2) return 0.5;  // 1/8
        if (snapIdx == 3) return 0.25; // 1/16
        if (snapIdx == 4) return 0.125;// 1/32
        if (snapIdx == 5) return 0.0625;// 1/64
        return 0.25;
    }

    // ── Matrix Mixer XL Drawing & Mouse Interaction ──────────────────────────
    void paintMatrixMixerXL (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto* node = getMatrixMixerXLNode();
        if (!node)
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions ("Sans", 16.0f, juce::Font::bold));
            g.drawText ("Matrix Mixer XL Node Not Found", bounds, juce::Justification::centred);
            return;
        }

        // Draw deep panel backdrop (Dark premium space theme)
        g.setColour (juce::Colour (0xFF0E0B16)); // extremely dark purple/black
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xFF312E81).withAlpha (0.4f)); // subtle glowing indigo border
        g.drawRoundedRectangle (bounds, 6.0f, 1.5f);

        // Header Background
        float topBarH = 45.0f;
        g.setColour (juce::Colour (0xFF1A1726)); // deep slate-indigo
        g.fillRect (0.0f, 0.0f, bounds.getWidth(), topBarH);
        g.setColour (juce::Colour (0xFF312E81)); // separator
        g.drawHorizontalLine ((int)topBarH, 0.0f, bounds.getWidth());

        // 1. Draw Title
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions ("Sans", 13.0f, juce::Font::bold));
        g.drawText ("MATRIX MIXER XL", 15.0f, 8.0f, 180.0f, 28.0f, juce::Justification::centredLeft);

        int activeSize = juce::jlimit (1, 32, (int) node->getParam ("size")->get());
        float masterVol = node->getParam ("master_vol")->get();

        // 2. Draw Resizing Segment Buttons: 4x4, 8x8, 16x16, 32x32
        g.setFont (juce::FontOptions ("Sans", 10.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xFF9CA3AF));
        g.drawText ("SIZE:", 190.0f, 8.0f, 35.0f, 28.0f, juce::Justification::centredLeft);

        xlBtn4x4 = juce::Rectangle<float> (230.0f, 8.0f, 38.0f, 28.0f);
        xlBtn8x8 = juce::Rectangle<float> (272.0f, 8.0f, 38.0f, 28.0f);
        xlBtn16x16 = juce::Rectangle<float> (314.0f, 8.0f, 44.0f, 28.0f);
        xlBtn32x32 = juce::Rectangle<float> (362.0f, 8.0f, 44.0f, 28.0f);

        auto drawBtn = [&](const juce::Rectangle<float>& r, int sizeVal, const juce::String& text) {
            bool active = (activeSize == sizeVal);
            g.setColour (active ? juce::Colour (0xFFEC4899) : juce::Colour (0xFF312E81)); // active pink vs idle indigo
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions ("Sans", 9.0f, juce::Font::bold));
            g.drawText (text, r, juce::Justification::centred);
        };

        drawBtn (xlBtn4x4, 4, "4x4");
        drawBtn (xlBtn8x8, 8, "8x8");
        drawBtn (xlBtn16x16, 16, "16x16");
        drawBtn (xlBtn32x32, 32, "32x32");

        // 3. Action Buttons
        btnSetDiagonal = juce::Rectangle<float> (420.0f, 8.0f, 95.0f, 28.0f);
        btnClearAll = juce::Rectangle<float> (520.0f, 8.0f, 75.0f, 28.0f);

        auto drawActionBtn = [&](const juce::Rectangle<float>& r, const juce::String& text, bool isDanger) {
            g.setColour (isDanger ? juce::Colour (0xFFEF4444).withAlpha (0.8f) : juce::Colour (0xFF10B981).withAlpha (0.8f)); // red vs green
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions ("Sans", 9.0f, juce::Font::bold));
            g.drawText (text, r, juce::Justification::centred);
        };

        drawActionBtn (btnSetDiagonal, "RESET DIAGONAL", false);
        drawActionBtn (btnClearAll, "CLEAR ALL", true);

        // 4. Master Volume Slider
        g.setColour (juce::Colour (0xFF9CA3AF));
        g.setFont (juce::FontOptions ("Sans", 10.0f, juce::Font::bold));
        g.drawText ("MASTER VOL:", 610.0f, 8.0f, 80.0f, 28.0f, juce::Justification::centredLeft);

        masterVolRect = juce::Rectangle<float> (695.0f, 15.0f, 100.0f, 14.0f);
        g.setColour (juce::Colour (0xFF1F2937)); // bar backdrop
        g.fillRoundedRectangle (masterVolRect, 4.0f);
        
        float masterFillW = (masterVol / 2.0f) * masterVolRect.getWidth();
        juce::Rectangle<float> masterFillRect (masterVolRect.getX(), masterVolRect.getY(), masterFillW, masterVolRect.getHeight());
        g.setColour (juce::Colour (0xFF10B981)); // green fill
        g.fillRoundedRectangle (masterFillRect, 4.0f);

        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (masterVolRect, 4.0f, 1.0f);
        g.setFont (juce::FontOptions ("Sans", 8.0f, juce::Font::bold));
        g.drawText (juce::String (masterVol, 2) + "x", masterVolRect, juce::Justification::centred);

        // Position editor
        if (cellValueEditor)
        {
            cellValueEditor->setBounds (805, 8, 45, 28);
            cellValueEditor->setVisible (true);
        }

        // 5. Draw Row & Column labels and grid
        float gridX = leftMargin;
        float gridY = topBarH + topMargin;
        float gridW = bounds.getWidth() - leftMargin - rightMargin;
        float gridH = bounds.getHeight() - gridY - bottomMargin;
        
        float rowH = gridH / (float) activeSize;
        float colW = gridW / (float) activeSize;

        g.setFont (juce::FontOptions ("Sans", 9.0f, juce::Font::bold));
        
        // Draw column labels
        for (int c = 0; c < activeSize; ++c)
        {
            float cx = gridX + c * colW;
            juce::Rectangle<float> colLabelRect (cx, topBarH + 5.0f, colW, 15.0f);
            
            bool highlight = (c == hoverColMatrix || c == selectedColMatrix);
            g.setColour (highlight ? juce::Colour (0xFFEC4899) : juce::Colour (0xFF9CA3AF));
            
            juce::String labelStr = juce::String (c + 1);
            if (activeSize <= 16) labelStr = "OUT " + labelStr;
            g.drawText (labelStr, colLabelRect, juce::Justification::centred);
            
            if (c == hoverColMatrix)
            {
                g.setColour (juce::Colour (0xFFEC4899).withAlpha (0.05f));
                g.fillRect (cx, gridY, colW, gridH);
            }
        }

        // Draw row labels
        for (int r = 0; r < activeSize; ++r)
        {
            float cy = gridY + r * rowH;
            juce::Rectangle<float> rowLabelRect (10.0f, cy, leftMargin - 15.0f, rowH);
            
            bool highlight = (r == hoverRowMatrix || r == selectedRowMatrix);
            g.setColour (highlight ? juce::Colour (0xFFEC4899) : juce::Colour (0xFF9CA3AF));
            
            juce::String labelStr = juce::String (r + 1);
            if (activeSize <= 16) labelStr = "IN " + labelStr;
            g.drawText (labelStr, rowLabelRect, juce::Justification::centredRight);
            
            if (r == hoverRowMatrix)
            {
                g.setColour (juce::Colour (0xFFEC4899).withAlpha (0.05f));
                g.fillRect (gridX, cy, gridW, rowH);
            }
        }

        // Draw grid cells
        for (int r = 0; r < activeSize; ++r)
        {
            for (int c = 0; c < activeSize; ++c)
            {
                float cx = gridX + c * colW;
                float cy = gridY + r * rowH;
                juce::Rectangle<float> cellRect (cx + 1.0f, cy + 1.0f, colW - 2.0f, rowH - 2.0f);
                
                float gain = node->getGain (r, c);
                
                juce::Colour cellColour;
                if (gain <= 0.001f)
                {
                    cellColour = juce::Colour (0xFF1E2937).withAlpha (0.4f);
                }
                else if (gain <= 1.0f)
                {
                    float t = gain;
                    cellColour = juce::Colour (
                        (juce::uint8) (0x1E + t * (0x63 - 0x1E)),
                        (juce::uint8) (0x29 + t * (0x66 - 0x29)),
                        (juce::uint8) (0x37 + t * (0xF1 - 0x37))
                    );
                }
                else
                {
                    float t = (gain - 1.0f);
                    cellColour = juce::Colour (
                        (juce::uint8) (0x63 + t * (0xEC - 0x63)),
                        (juce::uint8) (0x66 + t * (0x48 - 0x66)),
                        (juce::uint8) (0xF1 + t * (0x99 - 0xF1))
                    );
                }
                
                g.setColour (cellColour);
                g.fillRoundedRectangle (cellRect, 2.0f);
                
                if (r == c && gain > 0.9f && gain < 1.1f)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.4f));
                    g.fillEllipse (cellRect.getCentreX() - 1.5f, cellRect.getCentreY() - 1.5f, 3.0f, 3.0f);
                }

                if (activeSize <= 8)
                {
                    g.setColour (gain > 0.5f ? juce::Colours::white : juce::Colour (0xFF9CA3AF));
                    g.setFont (juce::FontOptions ("Sans", 8.0f, juce::Font::bold));
                    g.drawText (juce::String (gain, 2), cellRect, juce::Justification::centred);
                }

                if (r == selectedRowMatrix && c == selectedColMatrix)
                {
                    g.setColour (juce::Colour (0xFFEC4899));
                    g.drawRoundedRectangle (cellRect, 2.0f, 1.5f);
                }
            }
        }

        // Draw grid lines
        g.setColour (juce::Colour (0xFF374151).withAlpha (0.5f));
        for (int i = 0; i <= activeSize; ++i)
        {
            float cx = gridX + i * colW;
            g.drawVerticalLine ((int)cx, gridY, gridY + gridH);
            
            float cy = gridY + i * rowH;
            g.drawHorizontalLine ((int)cy, gridX, gridX + gridW);
        }

        // Tooltip
        juce::String tooltipStr = "Selected: IN " + juce::String (selectedRowMatrix + 1) + " -> OUT " + juce::String (selectedColMatrix + 1) + ": " + juce::String (node->getGain (selectedRowMatrix, selectedColMatrix), 2);
        if (hoverRowMatrix >= 0 && hoverColMatrix >= 0)
        {
            tooltipStr = "Hover: IN " + juce::String (hoverRowMatrix + 1) + " -> OUT " + juce::String (hoverColMatrix + 1) + ": " + juce::String (node->getGain (hoverRowMatrix, hoverColMatrix), 2);
        }
        g.setFont (juce::FontOptions ("Sans", 10.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xFFEC4899));
        g.drawText (tooltipStr, gridX, gridY + gridH + 1.0f, gridW, 14.0f, juce::Justification::centredRight);
    }

    void mouseDownMatrixMixerXL (const juce::MouseEvent& e)
    {
        auto* node = getMatrixMixerXLNode();
        if (!node) return;
        int activeSize = juce::jlimit (1, 32, (int) node->getParam ("size")->get());

        if (xlBtn4x4.contains (e.position)) { setMatrixSize (4); return; }
        if (xlBtn8x8.contains (e.position)) { setMatrixSize (8); return; }
        if (xlBtn16x16.contains (e.position)) { setMatrixSize (16); return; }
        if (xlBtn32x32.contains (e.position)) { setMatrixSize (32); return; }

        if (btnSetDiagonal.contains (e.position))
        {
            for (int r = 0; r < 32; ++r)
                for (int c = 0; c < 32; ++c)
                    node->setGain (r, c, (r == c) ? 1.0f : 0.0f);
            updateEditorValue();
            if (onParamChanged) onParamChanged();
            repaint();
            return;
        }

        if (btnClearAll.contains (e.position))
        {
            for (int r = 0; r < 32; ++r)
                for (int c = 0; c < 32; ++c)
                    node->setGain (r, c, 0.0f);
            updateEditorValue();
            if (onParamChanged) onParamChanged();
            repaint();
            return;
        }

        if (masterVolRect.contains (e.position))
        {
            isDraggingMasterVol = true;
            dragStartMasterVolVal = node->getParam ("master_vol")->get();
            dragStartMatrixPos = e.position;
            return;
        }

        float leftMargin = 55.0f;
        float topMargin = 25.0f;
        float topBarH = 45.0f;
        float rightMargin = 15.0f;
        float bottomMargin = 15.0f;
        
        auto bounds = getLocalBounds().toFloat();
        float gridX = leftMargin;
        float gridY = topBarH + topMargin;
        float gridW = bounds.getWidth() - leftMargin - rightMargin;
        float gridH = bounds.getHeight() - gridY - bottomMargin;

        if (e.x >= gridX && e.x <= gridX + gridW && e.y >= gridY && e.y <= gridY + gridH)
        {
            float rowH = gridH / (float) activeSize;
            float colW = gridW / (float) activeSize;

            int r = (int) ((e.y - gridY) / rowH);
            int c = (int) ((e.x - gridX) / colW);

            r = juce::jlimit (0, activeSize - 1, r);
            c = juce::jlimit (0, activeSize - 1, c);

            selectedRowMatrix = r;
            selectedColMatrix = c;

            isDraggingMatrixGain = true;
            dragStartGainVal = node->getGain (r, c);
            dragStartMatrixPos = e.position;

            updateEditorValue();
            if (cellValueEditor)
            {
                cellValueEditor->grabKeyboardFocus();
                cellValueEditor->selectAll();
            }
            repaint();
        }
    }

    void mouseDragMatrixMixerXL (const juce::MouseEvent& e)
    {
        auto* node = getMatrixMixerXLNode();
        if (!node) return;

        if (isDraggingMasterVol)
        {
            float dragSensitivity = 100.0f;
            float deltaY = dragStartMatrixPos.y - e.position.y;
            float newVal = dragStartMasterVolVal + (deltaY / dragSensitivity);
            newVal = juce::jlimit (0.0f, 2.0f, newVal);

            juce::String targetParamID = juce::String (node->getNodeID()) + "_master_vol";
            if (targetInstance && engine)
            {
                if (auto* parentNode = engine->getGraph().getNodeForId (targetInstance->nodeID))
                {
                    if (auto* proc = parentNode->getProcessor())
                    {
                        for (auto* p : proc->getParameters())
                        {
                            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                            {
                                if (matchMappingParam (targetParamID, ranged->getParameterID()))
                                {
                                    ranged->setValueNotifyingHost (ranged->convertTo0to1 (newVal));
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            repaint();
            return;
        }

        if (isDraggingMatrixGain)
        {
            float dragSensitivity = 150.0f;
            float deltaY = dragStartMatrixPos.y - e.position.y;
            float newVal = dragStartGainVal + (deltaY / dragSensitivity);
            newVal = juce::jlimit (0.0f, 2.0f, newVal);

            node->setGain (selectedRowMatrix, selectedColMatrix, newVal);
            updateEditorValue();
            repaint();
        }
    }

    void mouseUpMatrixMixerXL (const juce::MouseEvent& e)
    {
        juce::ignoreUnused (e);
        if (isDraggingMatrixGain || isDraggingMasterVol)
        {
            isDraggingMatrixGain = false;
            isDraggingMasterVol = false;
            if (onParamChanged) onParamChanged();
            repaint();
        }
    }

    void mouseMoveMatrixMixerXL (const juce::MouseEvent& e)
    {
        auto* node = getMatrixMixerXLNode();
        if (!node) return;
        int activeSize = juce::jlimit (1, 32, (int) node->getParam ("size")->get());

        float leftMargin = 55.0f;
        float topMargin = 25.0f;
        float topBarH = 45.0f;
        float rightMargin = 15.0f;
        float bottomMargin = 15.0f;
        
        auto bounds = getLocalBounds().toFloat();
        float gridX = leftMargin;
        float gridY = topBarH + topMargin;
        float gridW = bounds.getWidth() - leftMargin - rightMargin;
        float gridH = bounds.getHeight() - gridY - bottomMargin;

        if (e.x >= gridX && e.x <= gridX + gridW && e.y >= gridY && e.y <= gridY + gridH)
        {
            float rowH = gridH / (float) activeSize;
            float colW = gridW / (float) activeSize;

            int r = (int) ((e.y - gridY) / rowH);
            int c = (int) ((e.x - gridX) / colW);

            hoverRowMatrix = juce::jlimit (0, activeSize - 1, r);
            hoverColMatrix = juce::jlimit (0, activeSize - 1, c);
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
        else if (xlBtn4x4.contains (e.position) || xlBtn8x8.contains (e.position) ||
                 xlBtn16x16.contains (e.position) || xlBtn32x32.contains (e.position) ||
                 btnSetDiagonal.contains (e.position) || btnClearAll.contains (e.position))
        {
            hoverRowMatrix = -1;
            hoverColMatrix = -1;
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        else if (masterVolRect.contains (e.position))
        {
            hoverRowMatrix = -1;
            hoverColMatrix = -1;
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        }
        else
        {
            hoverRowMatrix = -1;
            hoverColMatrix = -1;
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
        repaint();
    }

    void setMatrixSize (int sizeVal)
    {
        auto* node = getMatrixMixerXLNode();
        if (!node) return;
        
        juce::String targetParamID = juce::String (node->getNodeID()) + "_size";
        if (targetInstance && engine)
        {
            if (auto* parentNode = engine->getGraph().getNodeForId (targetInstance->nodeID))
            {
                if (auto* proc = parentNode->getProcessor())
                {
                    for (auto* p : proc->getParameters())
                    {
                        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                        {
                            if (matchMappingParam (targetParamID, ranged->getParameterID()))
                            {
                                ranged->setValueNotifyingHost (ranged->convertTo0to1 ((float) sizeVal));
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        selectedRowMatrix = juce::jlimit (0, sizeVal - 1, selectedRowMatrix);
        selectedColMatrix = juce::jlimit (0, sizeVal - 1, selectedColMatrix);
        updateEditorValue();
        if (onParamChanged) onParamChanged();
        repaint();
    }

    void updateEditorValue()
    {
        if (controlID != "matrix_mixer_xl_display" || !cellValueEditor) return;
        auto* node = getMatrixMixerXLNode();
        if (!node) return;
        
        cellValueEditor->setText (juce::String (node->getGain (selectedRowMatrix, selectedColMatrix), 2), false);
    }

    void updateSelectedCellFromEditor()
    {
        if (controlID != "matrix_mixer_xl_display" || !cellValueEditor) return;
        auto* node = getMatrixMixerXLNode();
        if (!node) return;

        float val = cellValueEditor->getText().getFloatValue();
        val = juce::jlimit (0.0f, 2.0f, val);
        node->setGain (selectedRowMatrix, selectedColMatrix, val);
        if (targetInstance)
            targetInstance->controlTexts[controlID + "_gain_" + juce::String (selectedRowMatrix) + "_" + juce::String (selectedColMatrix)] = juce::String (val, 2);
        
        if (onParamChanged) onParamChanged();
        repaint();
    }

    void textEditorReturnKeyPressed (juce::TextEditor& editor) override
    {
        if (&editor == cellValueEditor.get())
        {
            updateSelectedCellFromEditor();
        }
    }

    void textEditorFocusLost (juce::TextEditor& editor) override
    {
        if (&editor == cellValueEditor.get())
        {
            updateSelectedCellFromEditor();
        }
    }

    void paintMidiEditor (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        
        auto* midiNode = getMidiEditorNode();
        if (!midiNode)
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions ("Sans", 14.0f, juce::Font::bold));
            g.drawText ("MIDI Editor Node Not Found", bounds, juce::Justification::centred);
            return;
        }

        // 1. Draw transport bar background
        float topBarH = 45.0f;
        g.setColour (juce::Colour (0xFF1E1B4B)); // deep dark indigo
        g.fillRect (0.0f, 0.0f, bounds.getWidth(), topBarH);
        g.setColour (juce::Colour (0xFF312E81));
        g.drawHorizontalLine ((int)topBarH, 0.0f, bounds.getWidth());

        // Read states
        bool isRunning = midiNode->getParam ("run")->get() > 0.5f;
        float bpm = midiNode->getParam ("bpm")->get();
        int snapIdx = (int)midiNode->getParam ("snap")->get();
        int chan = (int)midiNode->getParam ("chan")->get();
        double playheadBeat = midiNode->getPlayheadBeat();

        // 2. Draw Transport Bar Buttons
        // Play/Stop
        playBtnRect = juce::Rectangle<float> (15.0f, 8.0f, 75.0f, 28.0f);
        g.setColour (isRunning ? juce::Colour (0xFF10B981) : juce::Colour (0xFF4B5563));
        g.fillRoundedRectangle (playBtnRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions ("Sans", 11.0f, juce::Font::bold));
        g.drawText (isRunning ? "STOP" : "RUN", playBtnRect, juce::Justification::centred);

        // BPM
        bpmRect = juce::Rectangle<float> (100.0f, 8.0f, 90.0f, 28.0f);
        g.setColour (juce::Colour (0xFF312E81));
        g.fillRoundedRectangle (bpmRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText ("BPM: " + juce::String ((int)bpm), bpmRect, juce::Justification::centred);

        // Snap
        juce::String snapStrings[] = { "Snap: None", "Snap: 1/4", "Snap: 1/8", "Snap: 1/16", "Snap: 1/32", "Snap: 1/64" };
        juce::Rectangle<float> midiSnapRect (200.0f, 8.0f, 100.0f, 28.0f);
        g.setColour (juce::Colour (0xFF312E81));
        g.fillRoundedRectangle (midiSnapRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText (snapStrings[juce::jlimit (0, 5, snapIdx)] + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbc")), midiSnapRect, juce::Justification::centred);

        // Channel
        juce::Rectangle<float> midiChanRect (310.0f, 8.0f, 80.0f, 28.0f);
        g.setColour (juce::Colour (0xFF312E81));
        g.fillRoundedRectangle (midiChanRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText ("Chan: " + juce::String (chan) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbc")), midiChanRect, juce::Justification::centred);

        // Octave Down / Up
        juce::Rectangle<float> midiOctDownRect (400.0f, 8.0f, 50.0f, 28.0f);
        g.setColour (juce::Colour (0xFF312E81));
        g.fillRoundedRectangle (midiOctDownRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("OCT \xe2\x96\xbc")), midiOctDownRect, juce::Justification::centred);

        juce::Rectangle<float> midiOctUpRect (455.0f, 8.0f, 50.0f, 28.0f);
        g.setColour (juce::Colour (0xFF312E81));
        g.fillRoundedRectangle (midiOctUpRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("OCT \xe2\x96\xb2")), midiOctUpRect, juce::Justification::centred);

        // Clear
        juce::Rectangle<float> midiClearRect (515.0f, 8.0f, 80.0f, 28.0f);
        g.setColour (juce::Colour (0xFFEF4444).withAlpha (0.8f));
        g.fillRoundedRectangle (midiClearRect, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText ("CLEAR", midiClearRect, juce::Justification::centred);

        // Title Info
        g.setColour (juce::Colour (0xFF9CA3AF));
        g.setFont (juce::FontOptions ("Sans", 10.0f, juce::Font::italic));
        int currentOctave = (midiBaseNote / 12) - 1;
        g.drawText ("Octave: C" + juce::String(currentOctave) + " - B" + juce::String(currentOctave + 1) + " | Click grid to add, drag to move, drag edge to resize, double-click to delete.",
                    605.0f, 8.0f, bounds.getWidth() - 615.0f, 28.0f, juce::Justification::centredLeft, true);

        // 3. Grid Coordinates
        float keyboardW = 75.0f;
        float rightMargin = 10.0f;
        float velocityH = 90.0f;
        float gridY = topBarH;
        float gridH = bounds.getHeight() - topBarH - velocityH - 15.0f;
        float gridW = bounds.getWidth() - keyboardW - rightMargin;
        float rowH = gridH / 24.0f;

        // Draw deep panel backdrop for the piano roll area
        g.setColour (juce::Colour (0xFF110B1C)); // dark purple
        g.fillRect (keyboardW, gridY, gridW, gridH);

        // 4. Draw Rows & Keys
        for (int r = 0; r < 24; ++r)
        {
            float rowY = gridY + r * rowH;
            int noteNum = midiBaseNote + (23 - r);
            int noteInOctave = noteNum % 12;
            bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);

            // Draw grid row background highlight if black key
            if (isBlackKey)
            {
                g.setColour (juce::Colour (0xFF1E1B4B).withAlpha (0.4f));
                g.fillRect (keyboardW, rowY, gridW, rowH);
            }

            // Draw horizontal row separator
            g.setColour (juce::Colour (0xFF312E81).withAlpha (0.15f));
            g.drawHorizontalLine ((int)(rowY + rowH), keyboardW, bounds.getWidth() - rightMargin);

            // Check if key is currently active
            bool keyIsActive = false;
            {
                std::lock_guard<std::mutex> lock (midiNode->getMutex());
                for (const auto& note : midiNode->getNotes())
                {
                    if (note.noteNumber == noteNum && playheadBeat >= note.startBeat && playheadBeat < note.startBeat + note.duration && isRunning)
                    {
                        keyIsActive = true;
                        break;
                    }
                }
            }

            // Draw keyboard key
            if (isBlackKey)
            {
                g.setColour (keyIsActive ? juce::Colour (0xFFEC4899) : juce::Colour (0xFF151324));
                g.fillRect (0.0f, rowY + 1.0f, keyboardW * 0.7f, rowH - 2.0f);
                g.setColour (juce::Colour (0xFF312E81));
                g.drawRect (0.0f, rowY + 1.0f, keyboardW * 0.7f, rowH - 2.0f);
            }
            else
            {
                g.setColour (keyIsActive ? juce::Colour (0xFFFCD34D) : juce::Colour (0xFFE5E7EB));
                g.fillRect (0.0f, rowY + 1.0f, keyboardW, rowH - 2.0f);
                g.setColour (juce::Colour (0xFF9CA3AF));
                g.drawRect (0.0f, rowY + 1.0f, keyboardW, rowH - 2.0f);
            }

            // Label key (C notes, e.g. C4, C5, or if hovered/active)
            if (noteInOctave == 0 || r == 0 || r == 23)
            {
                g.setColour (isBlackKey ? juce::Colours::white.withAlpha (0.7f) : juce::Colours::black.withAlpha (0.7f));
                g.setFont (juce::FontOptions ("Sans", 9.0f, juce::Font::bold));
                g.drawText (getMidiNoteName (noteNum), 5.0f, rowY, keyboardW - 10.0f, rowH, juce::Justification::centredLeft);
            }
        }

        // 5. Draw Vertical Snap Columns
        g.setColour (juce::Colour (0xFF312E81).withAlpha (0.3f));
        
        // Draw 16th note subdivisions
        for (int step = 1; step < 16; ++step)
        {
            float subdivX = keyboardW + (step / 16.0f) * gridW;
            g.drawVerticalLine ((int)subdivX, gridY, gridY + gridH);
        }

        // Draw main beat lines (thicker)
        g.setColour (juce::Colour (0xFF8B5CF6).withAlpha (0.6f));
        for (int beat = 1; beat < 4; ++beat)
        {
            float beatX = keyboardW + (beat / 4.0f) * gridW;
            g.drawVerticalLine ((int)beatX, gridY, gridY + gridH);
        }

        // 6. Draw Note Blocks
        {
            std::lock_guard<std::mutex> lock (midiNode->getMutex());
            const auto& noteList = midiNode->getNotes();
            
            for (size_t i = 0; i < noteList.size(); ++i)
            {
                const auto& note = noteList[i];
                if (note.noteNumber >= midiBaseNote && note.noteNumber < midiBaseNote + 24)
                {
                    float noteX = keyboardW + (note.startBeat / 4.0f) * gridW;
                    float noteW = (note.duration / 4.0f) * gridW;
                    float noteY = gridY + (23 - (note.noteNumber - midiBaseNote)) * rowH;
                    float noteH = rowH;

                    juce::Rectangle<float> noteRect (noteX + 1.0f, noteY + 2.0f, noteW - 2.0f, noteH - 4.0f);
                    
                    bool isPlaying = playheadBeat >= note.startBeat && playheadBeat < note.startBeat + note.duration && isRunning;
                    
                    // Note block gradient
                    juce::Colour colourStart = isPlaying ? juce::Colour (0xFFFCD34D) : juce::Colour (0xFFEC4899); // gold glow vs pink
                    juce::Colour colourEnd = isPlaying ? juce::Colour (0xFFFBBF24) : juce::Colour (0xFF8B5CF6);   // yellow vs purple
                    
                    juce::ColourGradient grad (colourStart, noteRect.getX(), noteRect.getY(), 
                                               colourEnd, noteRect.getRight(), noteRect.getBottom(), false);
                    g.setGradientFill (grad);
                    g.fillRoundedRectangle (noteRect, 3.0f);
                    
                    // Borders
                    g.setColour (isPlaying ? juce::Colours::white : juce::Colours::white.withAlpha (0.6f));
                    g.drawRoundedRectangle (noteRect, 3.0f, 1.2f);

                    // Note label text
                    g.setColour (juce::Colours::white);
                    g.setFont (juce::FontOptions ("Sans", 8.5f, juce::Font::bold));
                    g.drawText (getMidiNoteName (note.noteNumber), noteRect.reduced(3.0f, 0.0f), juce::Justification::centredLeft, true);

                    // Right edge resize handle notches
                    if (noteRect.getWidth() > 12.0f)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.6f));
                        float handleX = noteRect.getRight() - 3.0f;
                        g.drawVerticalLine ((int)handleX, noteRect.getY() + 3.0f, noteRect.getBottom() - 3.0f);
                    }
                }
            }
        }

        // 7. Draw Velocity Lane
        float laneTopY = gridY + gridH + 10.0f;
        float laneBottomY = laneTopY + velocityH;
        
        // Background
        g.setColour (juce::Colour (0xFF0E0B16));
        g.fillRect (keyboardW, laneTopY, gridW, velocityH);
        g.setColour (juce::Colour (0xFF312E81).withAlpha (0.5f));
        g.drawRect (keyboardW, laneTopY, gridW, velocityH);

        // Sidebar label
        g.setColour (juce::Colour (0xFF4B5563));
        g.fillRect (0.0f, laneTopY, keyboardW, velocityH);
        g.setColour (juce::Colour (0xFF312E81).withAlpha (0.5f));
        g.drawRect (0.0f, laneTopY, keyboardW, velocityH);
        g.setColour (juce::Colour (0xFF9CA3AF));
        g.setFont (juce::FontOptions ("Sans", 10.0f, juce::Font::bold));
        g.drawText ("VELOCITY", 5.0f, laneTopY, keyboardW - 10.0f, velocityH, juce::Justification::centred);

        // Stalks
        {
            std::lock_guard<std::mutex> lock (midiNode->getMutex());
            const auto& noteList = midiNode->getNotes();
            
            for (size_t i = 0; i < noteList.size(); ++i)
            {
                const auto& note = noteList[i];
                float stalkX = keyboardW + (note.startBeat / 4.0f) * gridW;
                float stalkH = (note.velocity / 127.0f) * (velocityH - 15.0f);
                float stalkY = laneBottomY - stalkH;

                // Highlight stalk if note is within visible keyboard pitch
                bool isVisible = (note.noteNumber >= midiBaseNote && note.noteNumber < midiBaseNote + 24);
                
                g.setColour (isVisible ? juce::Colour (0xFF06B6D4) : juce::Colour (0xFF06B6D4).withAlpha (0.3f)); // Cyan
                g.drawVerticalLine ((int)stalkX, stalkY, laneBottomY - 2.0f);
                
                g.fillEllipse (stalkX - 3.0f, stalkY - 3.0f, 6.0f, 6.0f);
            }
        }

        // 8. Playhead golden line
        float playheadX = keyboardW + (playheadBeat / 4.0f) * gridW;
        g.setColour (juce::Colour (0xFFFCD34D)); // Gold
        g.drawVerticalLine ((int)playheadX, gridY, laneBottomY);
        
        // Playhead triangle handle at the top of the grid
        juce::Path headPath;
        headPath.addTriangle (playheadX - 6.0f, gridY, playheadX + 6.0f, gridY, playheadX, gridY + 8.0f);
        g.fillPath (headPath);
    }

    void mouseDownMidiEditor (const juce::MouseEvent& e)
    {
        auto* midiNode = getMidiEditorNode();
        if (!midiNode) return;

        auto bounds = getLocalBounds().toFloat();
        float topBarH = 45.0f;
        float keyboardW = 75.0f;
        float rightMargin = 10.0f;
        float velocityH = 90.0f;
        float gridY = topBarH;
        float gridH = bounds.getHeight() - topBarH - velocityH - 15.0f;
        float gridW = bounds.getWidth() - keyboardW - rightMargin;
        float rowH = gridH / 24.0f;
        float laneTopY = gridY + gridH + 10.0f;
        float laneBottomY = laneTopY + velocityH;

        int chan = (int)midiNode->getParam ("chan")->get();

        // ── 1. Check Transport Bar ───────────────────────────────────────────
        if (playBtnRect.contains (e.position))
        {
            bool isRunning = midiNode->getParam ("run")->get() > 0.5f;
            setMidiEditorParameter ("run", isRunning ? 0.0f : 1.0f);
            repaint();
            return;
        }

        if (bpmRect.contains (e.position))
        {
            draggedMidiBPM = true;
            draggedMidiBPMStartVal = midiNode->getParam ("bpm")->get();
            dragStartPos = e.position;
            return;
        }

        juce::Rectangle<float> midiSnapRect (200.0f, 8.0f, 100.0f, 28.0f);
        if (midiSnapRect.contains (e.position))
        {
            juce::PopupMenu menu;
            juce::String snapStrings[] = { "None", "1/4 Note", "1/8 Note", "1/16 Note", "1/32 Note", "1/64 Note" };
            int currentSnap = (int)midiNode->getParam ("snap")->get();
            for (int i = 0; i < 6; ++i)
                menu.addItem (1 + i, snapStrings[i], true, i == currentSnap);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> ((int)e.x, (int)e.y, 1, 1)),
                [this](int r) {
                    if (r >= 1 && r <= 6)
                        setMidiEditorParameter ("snap", (float)(r - 1));
                    repaint();
                });
            return;
        }

        juce::Rectangle<float> midiChanRect (310.0f, 8.0f, 80.0f, 28.0f);
        if (midiChanRect.contains (e.position))
        {
            juce::PopupMenu menu;
            for (int c = 1; c <= 16; ++c)
                menu.addItem (c, "Channel " + juce::String (c), true, c == chan);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> ((int)e.x, (int)e.y, 1, 1)),
                [this](int r) {
                    if (r >= 1 && r <= 16)
                        setMidiEditorParameter ("chan", (float)r);
                    repaint();
                });
            return;
        }

        juce::Rectangle<float> midiOctDownRect (400.0f, 8.0f, 50.0f, 28.0f);
        if (midiOctDownRect.contains (e.position))
        {
            midiBaseNote = juce::jlimit (12, 96, midiBaseNote - 12);
            repaint();
            return;
        }

        juce::Rectangle<float> midiOctUpRect (455.0f, 8.0f, 50.0f, 28.0f);
        if (midiOctUpRect.contains (e.position))
        {
            midiBaseNote = juce::jlimit (12, 96, midiBaseNote + 12);
            repaint();
            return;
        }

        juce::Rectangle<float> midiClearRect (515.0f, 8.0f, 80.0f, 28.0f);
        if (midiClearRect.contains (e.position))
        {
            setMidiEditorParameter ("clear", 1.0f);
            repaint();
            return;
        }

        // ── 2. Check Keyboard Sidebar clicks ──────────────────────────────────
        if (e.x < keyboardW && e.y >= gridY && e.y < gridY + gridH)
        {
            int row = (int)((e.y - gridY) / rowH);
            int noteNum = midiBaseNote + (23 - row);
            if (noteNum >= 0 && noteNum <= 127)
            {
                midiNode->triggerNotePreview (noteNum, true, 100, chan);
                activePreviewNote = noteNum;
            }
            return;
        }

        // ── 3. Check Velocity Lane clicks ─────────────────────────────────────
        if (e.y >= laneTopY && e.y <= laneBottomY && e.x >= keyboardW && e.x <= bounds.getWidth() - rightMargin)
        {
            isEditingVelocity = true;
            dragStartPos = e.position;
            
            // Find note closest to X coordinate
            double clickBeat = ((e.x - keyboardW) / gridW) * 4.0;
            
            std::lock_guard<std::mutex> lock (midiNode->getMutex());
            auto& notes = midiNode->getNotes();
            
            int closestIdx = -1;
            double minDistance = 0.25; // max snap distance
            
            for (size_t i = 0; i < notes.size(); ++i)
            {
                double dist = std::abs (notes[i].startBeat - clickBeat);
                if (dist < minDistance)
                {
                    minDistance = dist;
                    closestIdx = (int)i;
                }
            }
            
            noteWithActiveVelocityDrag = closestIdx;
            
            if (closestIdx >= 0)
            {
                float newVel = ((laneBottomY - e.y) / (velocityH - 15.0f)) * 127.0f;
                notes[closestIdx].velocity = juce::jlimit (1, 127, (int)newVel);
                repaint();
            }
            return;
        }

        // ── 4. Check Grid Clicks ──────────────────────────────────────────────
        if (e.x >= keyboardW && e.x <= bounds.getWidth() - rightMargin && e.y >= gridY && e.y < gridY + gridH)
        {
            selectedNoteIndex = -1;
            isResizingNote = false;
            isDraggingNote = false;

            double clickBeat = ((e.x - keyboardW) / gridW) * 4.0;
            int clickRow = (int)((e.y - gridY) / rowH);
            int clickNote = midiBaseNote + (23 - clickRow);

            std::lock_guard<std::mutex> lock (midiNode->getMutex());
            auto& notes = midiNode->getNotes();

            // Hit test existing notes
            for (size_t i = 0; i < notes.size(); ++i)
            {
                auto& note = notes[i];
                if (note.noteNumber == clickNote && clickBeat >= note.startBeat && clickBeat <= note.startBeat + note.duration)
                {
                    selectedNoteIndex = (int)i;
                    dragStartBeat = note.startBeat;
                    dragStartNoteNumber = note.noteNumber;
                    dragStartDuration = note.duration;
                    dragStartPos = e.position;

                    // Double click to delete
                    if (e.getNumberOfClicks() >= 2)
                    {
                        notes.erase (notes.begin() + i);
                        selectedNoteIndex = -1;
                        repaint();
                        return;
                    }

                    // Check if clicked the right edge (within 8 pixels)
                    float noteRightX = keyboardW + ((note.startBeat + note.duration) / 4.0f) * gridW;
                    if (e.x >= noteRightX - 8.0f)
                    {
                        isResizingNote = true;
                    }
                    else
                    {
                        isDraggingNote = true;
                    }
                    
                    // Preview note on selection
                    midiNode->triggerNotePreview (note.noteNumber, true, 100, chan);
                    activePreviewNote = note.noteNumber;
                    
                    repaint();
                    return;
                }
            }

            // Clicked empty cell: Add new note!
            double snapBeats = getSnapBeats();
            double snappedBeat = std::round (clickBeat / snapBeats) * snapBeats;
            snappedBeat = juce::jlimit (0.0, 4.0 - snapBeats, snappedBeat);

            MidiEditorNote note;
            note.startBeat = (float)snappedBeat;
            note.duration = (float)snapBeats;
            note.noteNumber = clickNote;
            note.velocity = 100;
            note.channel = chan;

            notes.push_back (note);
            selectedNoteIndex = (int)(notes.size() - 1);
            
            // Preview
            midiNode->triggerNotePreview (note.noteNumber, true, 100, chan);
            activePreviewNote = note.noteNumber;
            
            repaint();
        }
    }

    void mouseDragMidiEditor (const juce::MouseEvent& e)
    {
        auto* midiNode = getMidiEditorNode();
        if (!midiNode) return;

        auto bounds = getLocalBounds().toFloat();
        float topBarH = 45.0f;
        float keyboardW = 75.0f;
        float rightMargin = 10.0f;
        float velocityH = 90.0f;
        float gridY = topBarH;
        float gridH = bounds.getHeight() - topBarH - velocityH - 15.0f;
        float gridW = bounds.getWidth() - keyboardW - rightMargin;
        float rowH = gridH / 24.0f;
        float laneTopY = gridY + gridH + 10.0f;
        float laneBottomY = laneTopY + velocityH;
        int chan = (int)midiNode->getParam ("chan")->get();

        // ── 1. Drag BPM ───────────────────────────────────────────────────────
        if (draggedMidiBPM)
        {
            float dragSensitivity = 1.5f;
            float delta = -e.getDistanceFromDragStartY() / dragSensitivity;
            float newBPM = juce::jlimit (20.0f, 300.0f, draggedMidiBPMStartVal + delta);
            setMidiEditorParameter ("bpm", newBPM);
            repaint();
            return;
        }

        // ── 2. Drag Keyboard ──────────────────────────────────────────────────
        if (activePreviewNote >= 0 && e.x < keyboardW && e.y >= gridY && e.y < gridY + gridH)
        {
            int row = (int)((e.y - gridY) / rowH);
            int noteNum = midiBaseNote + (23 - row);
            if (noteNum >= 0 && noteNum <= 127 && noteNum != activePreviewNote)
            {
                midiNode->triggerNotePreview (activePreviewNote, false, 0, chan);
                midiNode->triggerNotePreview (noteNum, true, 100, chan);
                activePreviewNote = noteNum;
                repaint();
            }
            return;
        }

        // ── 3. Drag Velocity ──────────────────────────────────────────────────
        if (isEditingVelocity && noteWithActiveVelocityDrag >= 0)
        {
            std::lock_guard<std::mutex> lock (midiNode->getMutex());
            auto& notes = midiNode->getNotes();
            if (noteWithActiveVelocityDrag < (int)notes.size())
            {
                float newVel = ((laneBottomY - e.y) / (velocityH - 15.0f)) * 127.0f;
                notes[noteWithActiveVelocityDrag].velocity = juce::jlimit (1, 127, (int)newVel);
                repaint();
            }
            return;
        }

        // ── 4. Drag Note Block ────────────────────────────────────────────────
        if (selectedNoteIndex >= 0)
        {
            std::lock_guard<std::mutex> lock (midiNode->getMutex());
            auto& notes = midiNode->getNotes();
            if (selectedNoteIndex >= (int)notes.size()) return;
            auto& note = notes[selectedNoteIndex];

            double snapBeats = getSnapBeats();
            float deltaX = e.position.x - dragStartPos.x;
            double deltaBeats = (deltaX / gridW) * 4.0;

            if (isResizingNote)
            {
                double newDuration = dragStartDuration + deltaBeats;
                newDuration = std::round (newDuration / snapBeats) * snapBeats;
                newDuration = juce::jmax (snapBeats, newDuration);
                if (note.startBeat + newDuration > 4.0)
                    newDuration = 4.0 - note.startBeat;
                
                note.duration = (float)newDuration;
            }
            else if (isDraggingNote)
            {
                double newStart = dragStartBeat + deltaBeats;
                newStart = std::round (newStart / snapBeats) * snapBeats;
                newStart = juce::jlimit (0.0, 4.0 - note.duration, newStart);

                float deltaY = e.position.y - dragStartPos.y;
                int deltaRows = (int)std::round (deltaY / rowH);
                int newNoteNum = dragStartNoteNumber - deltaRows;
                newNoteNum = juce::jlimit (0, 127, newNoteNum);

                if (newNoteNum != note.noteNumber)
                {
                    midiNode->triggerNotePreview (note.noteNumber, false, 0, chan);
                    midiNode->triggerNotePreview (newNoteNum, true, 100, chan);
                    activePreviewNote = newNoteNum;
                }

                note.startBeat = (float)newStart;
                note.noteNumber = newNoteNum;
            }
            repaint();
        }
    }

    void mouseUpMidiEditor (const juce::MouseEvent& e)
    {
        juce::ignoreUnused (e);
        auto* midiNode = getMidiEditorNode();
        if (!midiNode) return;
        int chan = (int)midiNode->getParam ("chan")->get();

        // Release preview note
        if (activePreviewNote >= 0)
        {
            midiNode->triggerNotePreview (activePreviewNote, false, 0, chan);
            activePreviewNote = -1;
        }

        draggedMidiBPM = false;
        isEditingVelocity = false;
        noteWithActiveVelocityDrag = -1;
        isResizingNote = false;
        isDraggingNote = false;
        repaint();
    }

    void mouseMoveMidiEditor (const juce::MouseEvent& e)
    {
        auto bounds = getLocalBounds().toFloat();
        float topBarH = 45.0f;
        float keyboardW = 75.0f;
        float rightMargin = 10.0f;
        float velocityH = 90.0f;
        float gridY = topBarH;
        float gridH = bounds.getHeight() - topBarH - velocityH - 15.0f;
        float gridW = bounds.getWidth() - keyboardW - rightMargin;
        float rowH = gridH / 24.0f;

        if (e.x >= keyboardW && e.x <= bounds.getWidth() - rightMargin && e.y >= gridY && e.y < gridY + gridH)
        {
            auto* midiNode = getMidiEditorNode();
            if (midiNode)
            {
                double hoverBeat = ((e.x - keyboardW) / gridW) * 4.0;
                int hoverRow = (int)((e.y - gridY) / rowH);
                int hoverNote = midiBaseNote + (23 - hoverRow);

                std::lock_guard<std::mutex> lock (midiNode->getMutex());
                for (const auto& note : midiNode->getNotes())
                {
                    if (note.noteNumber == hoverNote && hoverBeat >= note.startBeat && hoverBeat <= note.startBeat + note.duration)
                    {
                        float noteRightX = keyboardW + ((note.startBeat + note.duration) / 4.0f) * gridW;
                        if (e.x >= noteRightX - 8.0f)
                        {
                            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
                            return;
                        }
                        else
                        {
                            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                            return;
                        }
                    }
                }
            }
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
        else if (e.x < keyboardW && e.y >= gridY && e.y < gridY + gridH)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        else
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
    }

private:
    // ── MIDI Editor State ─────────────────────────────────────────────────────
    int midiBaseNote = 48; // Default C3
    int selectedNoteIndex = -1;
    bool isResizingNote = false;
    bool isDraggingNote = false;
    double dragStartBeat = 0.0;
    int dragStartNoteNumber = 60;
    double dragStartDuration = 1.0;
    juce::Point<float> dragStartPos;
    bool draggedMidiBPM = false;
    float draggedMidiBPMStartVal = 120.0f;
    bool isEditingVelocity = false;
    int noteWithActiveVelocityDrag = -1;
    int activePreviewNote = -1;

    // ── Matrix Mixer XL State ────────────────────────────────────────────────
    int selectedRowMatrix = 0;
    int selectedColMatrix = 0;
    bool isDraggingMatrixGain = false;
    float dragStartGainVal = 0.0f;
    juce::Point<float> dragStartMatrixPos;
    int hoverRowMatrix = -1;
    int hoverColMatrix = -1;
    
    std::unique_ptr<juce::TextEditor> cellValueEditor;
    
    juce::Rectangle<float> xlBtn4x4;
    juce::Rectangle<float> xlBtn8x8;
    juce::Rectangle<float> xlBtn16x16;
    juce::Rectangle<float> xlBtn32x32;
    
    juce::Rectangle<float> masterVolRect;
    bool isDraggingMasterVol = false;
    float dragStartMasterVolVal = 1.0f;
    
    juce::Rectangle<float> btnClearAll;
    juce::Rectangle<float> btnSetDiagonal;
    
    float leftMargin = 55.0f;
    float topMargin = 25.0f;
    float rightMargin = 15.0f;
    float bottomMargin = 15.0f;
};

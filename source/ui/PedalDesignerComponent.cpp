#include "PedalDesignerComponent.h"
#include "HardwareDrawing.h"
#include "../dsp/DSPGraph.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
struct PlacedHardware
{
    juce::String type;
    float x, y;
    float width, height;
    juce::String label;
    juce::String parameterID;      // mapped effect parameter (empty until connected)
    float value = 0.5f;           // visual state: 0-1 for knobs/faders, 0/1 for switches
    juce::String controlID;       // unique ID for effects builder to reference this control

    // Custom UI properties
    juce::String imageMain;       // Path to custom main image (knob body, switch, LED, fader thumb)
    juce::String imageTrack;      // Path to track/background image (fader slot)
    juce::Colour customColour { juce::Colours::red }; // Color for LED glow
    bool stretchImage = true;     // Whether to stretch image or keep aspect ratio
    juce::String fontFamily = "Sans";
    int fontStyle = 1;            // 0=Plain, 1=Bold, 2=Italic, 3=BoldItalic
};

//==============================================================================
class HardwareItem : public juce::Component
{
public:
    HardwareItem (const juce::String& t, const juce::String& displayName)
        : type (t), name (displayName) {}

    void paint (juce::Graphics& g) override
    {
        HardwareDrawing::drawForType (g, type, getLocalBounds().toFloat());
        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (name, 0, getHeight() - 14, getWidth(), 14, juce::Justification::centredBottom);
    }

    void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragStarted && e.getDistanceFromDragStart() > 5)
        {
            dragStarted = true;
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            {
                float ratioX = e.getMouseDownX() / (float) getWidth();
                float ratioY = e.getMouseDownY() / (float) getHeight();
                juce::String desc = "hardware:" + type + ":" + juce::String(ratioX) + ":" + juce::String(ratioY);
                juce::Image emptyImage (juce::Image::ARGB, 1, 1, true);
                container->startDragging (desc, this, emptyImage, false);
            }
        }
    }

    juce::String getType() const { return type; }

private:
    juce::String type, name;
    bool dragStarted = false;
};



//==============================================================================
class PedalDesignerComponent::ChassisCanvas : public juce::Component,
                                              public juce::DragAndDropTarget
{
public:
    std::function<void()> onSelectionChanged;

    void notifySelectionChanged()
    {
        if (onSelectionChanged) onSelectionChanged();
    }

    // Grid presets: { size, label }
    static constexpr float gridSizes[] = { 10.0f, 20.0f, 40.0f };
    static constexpr int numGridSizes = 3;
    inline static const char* gridLabels[] = { "Fine", "Med", "Coarse" };

    // Chassis presets (name, width, height)
    struct ChassisPreset { const char* name; float w; float h; };
    inline static const ChassisPreset chassisPresets[] = {
        { "1590A",  120.0f, 200.0f },
        { "1590B",  200.0f, 340.0f },
        { "1590BB", 240.0f, 380.0f },
        { "125B",   280.0f, 420.0f },
    };
    static constexpr int numChassisPresets = 4;
    int chassisPresetIndex = 1; // default 1590B
    float chassisW = 200.0f, chassisH = 340.0f;

    // Chassis paint colours
    inline static const juce::Colour chassisColours[] = {
        juce::Colour (0xFF8A8A94), // Silver (default)
        juce::Colour (0xFF2A2A3A), // Black
        juce::Colour (0xFFCC3333), // Red
        juce::Colour (0xFF2266AA), // Blue
        juce::Colour (0xFF33AA55), // Green
        juce::Colour (0xFFDDAA22), // Gold
        juce::Colour (0xFFEEEEEE), // White
        juce::Colour (0xFFDD6600), // Orange
        juce::Colour (0xFF8844CC), // Purple
    };
    static constexpr int numChassisColours = 9;
    int chassisColourIndex = 0;
    juce::Colour chassisColour { 0xFF8A8A94 };

    // Component sizes per type (aligned to grid)
    static float sizeForType (const juce::String& type)
    {
        if (type == "fader") return 40.0f;
        if (type == "led" || type == "rgb_led" || type == "indicator") return 20.0f;
        // Screens — need height
        if (type == "7seg") return 30.0f;
        if (type == "display") return 28.0f;
        if (type == "text_screen" || type == "console") return 50.0f;
        if (type == "pixel_display") return 40.0f;
        if (type == "label") return 20.0f;
        if (type == "vu_meter") return 60.0f;
        if (type == "oscilloscope") return 50.0f;
        return 40.0f; // knob, switch, footswitch
    }
    static float widthForType (const juce::String& type)
    {
        if (type == "fader") return 100.0f;
        // Screens are wider than tall
        if (type == "7seg") return 70.0f;
        if (type == "display") return 70.0f;
        if (type == "text_screen" || type == "console") return 80.0f;
        if (type == "pixel_display") return 80.0f;
        if (type == "label") return 80.0f;
        if (type == "vu_meter") return 20.0f; // tall and narrow
        if (type == "oscilloscope") return 80.0f;
        return sizeForType (type);
    }

    float snapToGrid (float val) const
    {
        if (! snapEnabled) return val;
        float g = gridSizes[gridSizeIndex];
        return std::round (val / g) * g;
    }

    ChassisCanvas()
    {
        setWantsKeyboardFocus (true);
        addAndMakeVisible (btnReset);
        btnReset.onClick = [this] { scale = 1.0f; panX = panY = 0.0f; rotation = 0.0f; repaint(); };
        addAndMakeVisible (btnRotate);
        btnRotate.onClick = [this] { rotation += juce::MathConstants<float>::halfPi; repaint(); };
        addAndMakeVisible (btnGrid);
        btnGrid.onClick = [this] { gridSizeIndex = (gridSizeIndex + 1) % numGridSizes; updateGridLabel(); repaint(); };
        addAndMakeVisible (btnSnap);
        btnSnap.setClickingTogglesState (true);
        btnSnap.setToggleState (true, juce::dontSendNotification);
        btnSnap.onClick = [this] { snapEnabled = btnSnap.getToggleState(); repaint(); };

        // Chassis size selector
        for (int i = 0; i < numChassisPresets; ++i)
            chassisCombo.addItem (chassisPresets[i].name, i + 1);
        chassisCombo.setSelectedId (chassisPresetIndex + 1, juce::dontSendNotification);
        chassisCombo.onChange = [this]
        {
            chassisPresetIndex = chassisCombo.getSelectedId() - 1;
            chassisW = chassisPresets[chassisPresetIndex].w;
            chassisH = chassisPresets[chassisPresetIndex].h;
            repaint();
        };
        addAndMakeVisible (chassisCombo);

        // Colour button
        addAndMakeVisible (btnColour);
        btnColour.onClick = [this]
        {
            chassisColourIndex = (chassisColourIndex + 1) % numChassisColours;
            chassisColour = chassisColours[chassisColourIndex];
            chassisImage = ""; // Clear image when cycling colors
            repaint();
        };

        addAndMakeVisible (btnSave);
        btnSave.onClick = [this] { savePedalDesign(); };
        updateGridLabel();
    }

    /** Callback: parent sets this to inject the effects graph into the PedalDesign. */
    std::function<juce::var()> onGetEffectsGraph;

    juce::String pedalName = "My Pedal";
    juce::String pedalAuthor = "User";
    juce::String pedalDescription = "";
    juce::String pedalCategory = "Custom";

    void savePedalDesign()
    {
        if (pedalName.isEmpty()) pedalName = "Untitled Pedal";
        
        PedalDesign design = buildPedalDesign();
        
        // Save to designs directory
        auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("PedalForge").getChildFile ("designs");
        designsDir.createDirectory();

        auto file = designsDir.getChildFile (pedalName.replace (" ", "_") + ".json");
        design.saveToFile (file);

        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                 "Saved!",
                                                 "Pedal design saved to:\n" + file.getFullPathName());
    }

    PedalDesign buildPedalDesign() const
    {
        PedalDesign design;
        design.name = pedalName;
        design.author = pedalAuthor;
        design.description = pedalDescription;
        design.category = pedalCategory;
        design.chassisW = chassisW;
        design.chassisH = chassisH;
        design.chassisColour = chassisColour;
        design.chassisImage = chassisImage;

        // Controls
        for (const auto& hw : placedHardware)
        {
            PedalDesign::Control ctrl;
            ctrl.type = hw.type;
            ctrl.x = hw.x;
            ctrl.y = hw.y;
            ctrl.width = hw.width;
            ctrl.height = hw.height;
            ctrl.label = hw.label;
            ctrl.controlID = hw.controlID;
            ctrl.defaultValue = hw.value;
            ctrl.imageMain = hw.imageMain;
            ctrl.imageTrack = hw.imageTrack;
            ctrl.customColour = hw.customColour;
            ctrl.stretchImage = hw.stretchImage;
            ctrl.fontFamily = hw.fontFamily;
            ctrl.fontStyle = hw.fontStyle;
            design.controls.push_back (ctrl);

            // Mapping
            if (hw.parameterID.isNotEmpty())
            {
                PedalDesign::Mapping m;
                m.controlID = hw.controlID;
                m.nodeParam = hw.parameterID;
                design.mappings.push_back (m);
            }
        }

        // Effects graph from the NodeGraphEditor
        if (onGetEffectsGraph)
            design.effectsGraph = onGetEffectsGraph();

        return design;
    }

    void updateGridLabel()
    {
        btnGrid.setButtonText (juce::String ("Grid: ") + gridLabels[gridSizeIndex]);
    }

    void resized() override
    {
        btnReset.setBounds  (10, 10, 80, 24);
        btnRotate.setBounds (100, 10, 80, 24);
        btnGrid.setBounds   (190, 10, 100, 24);
        btnSnap.setBounds   (300, 10, 60, 24);
        chassisCombo.setBounds (370, 10, 90, 24);
        btnColour.setBounds (470, 10, 70, 24);
        btnSave.setBounds   (getWidth() - 90, 10, 80, 24);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (PedalForgeLookAndFeel::bgDark);
        g.saveState();

        juce::AffineTransform t = juce::AffineTransform::rotation (rotation, getWidth() / 2.0f, getHeight() / 2.0f)
                                  .scaled (scale, scale, getWidth() / 2.0f, getHeight() / 2.0f)
                                  .translated (panX, panY);
        g.addTransform (t);

        // Draw chassis
        float cx = getWidth() / 2.0f - chassisW / 2.0f;
        float cy = getHeight() / 2.0f - chassisH / 2.0f;

        g.addTransform (juce::AffineTransform::translation (cx, cy));

        HardwareDrawing::CustomStyles chassisStyles;
        chassisStyles.imageChassis = chassisImage;
        HardwareDrawing::drawChassis (g, { 0, 0, chassisW, chassisH }, chassisColour, &chassisStyles);

        // Draw grid overlay on chassis
        float gs = gridSizes[gridSizeIndex];
        g.setColour (juce::Colour (0x18FFFFFF));
        for (float gx = 0; gx <= chassisW; gx += gs)
            g.drawVerticalLine ((int) gx, 0, chassisH);
        for (float gy = 0; gy <= chassisH; gy += gs)
            g.drawHorizontalLine ((int) gy, 0, chassisW);

        // Draw placed hardware with values
        for (int i = 0; i < (int) placedHardware.size(); ++i)
        {
            const auto& hw = placedHardware[i];
            HardwareDrawing::CustomStyles styles;
            styles.imageMain = hw.imageMain;
            styles.imageTrack = hw.imageTrack;
            styles.customColour = hw.customColour;
            styles.stretchImage = hw.stretchImage;

            HardwareDrawing::drawForType (g, hw.type, { hw.x, hw.y, hw.width, hw.height }, hw.value, &styles);

            if (selectedIndices.count(i))
            {
                g.setColour (PedalForgeLookAndFeel::accent);
                g.drawRoundedRectangle (hw.x - 3, hw.y - 3, hw.width + 6, hw.height + 6, 6.0f, 2.5f);
                float hs = 6.0f;
                g.setColour (juce::Colours::white);
                g.fillRoundedRectangle (hw.x - hs/2 - 1, hw.y - hs/2 - 1, hs, hs, 2.0f);
                g.fillRoundedRectangle (hw.x + hw.width - hs/2 + 1, hw.y - hs/2 - 1, hs, hs, 2.0f);
                g.fillRoundedRectangle (hw.x - hs/2 - 1, hw.y + hw.height - hs/2 + 1, hs, hs, 2.0f);
                g.fillRoundedRectangle (hw.x + hw.width - hs/2 + 1, hw.y + hw.height - hs/2 + 1, hs, hs, 2.0f);
            }

            if (hw.label.isNotEmpty())
            {
                g.setColour (PedalForgeLookAndFeel::textPrimary);
                g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
                g.drawText (hw.label, (int)(hw.x - 20), (int)(hw.y + hw.height + 2),
                           (int)(hw.width + 40), 16, juce::Justification::centredTop);
            }
        }

        if (selectedIndices.empty())
        {
            float hs = 6.0f;
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (0 - hs/2 - 1, 0 - hs/2 - 1, hs, hs, 2.0f);
            g.fillRoundedRectangle (chassisW - hs/2 + 1, 0 - hs/2 - 1, hs, hs, 2.0f);
            g.fillRoundedRectangle (0 - hs/2 - 1, chassisH - hs/2 + 1, hs, hs, 2.0f);
            g.fillRoundedRectangle (chassisW - hs/2 + 1, chassisH - hs/2 + 1, hs, hs, 2.0f);
        }

        if (showDragPreview)
        {
            float pw = widthForType (dragPreviewType), ph = sizeForType (dragPreviewType);
            float sx = snapToGrid (dragPreviewCanvasX);
            float sy = snapToGrid (dragPreviewCanvasY);
            g.setOpacity (0.5f);
            HardwareDrawing::drawForType (g, dragPreviewType, { sx, sy, pw, ph });
            g.setOpacity (1.0f);
        }

        if (isMarquee)
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.3f));
            g.fillRect (marqueeRect);
            g.setColour (PedalForgeLookAndFeel::accent);
            g.drawRect (marqueeRect, 1.0f);
        }

        g.restoreState();
    }

    // --- Mouse / camera ---
    juce::Point<float> getAbsoluteCanvasPosForPoint (juce::Point<float> p)
    {
        auto t = juce::AffineTransform::rotation (rotation, getWidth() / 2.0f, getHeight() / 2.0f)
                 .scaled (scale, scale, getWidth() / 2.0f, getHeight() / 2.0f)
                 .translated (panX, panY).inverted();
        t.transformPoint (p.x, p.y);
        return p;
    }

    juce::Point<float> getChassisRelativeCanvasPosForPoint (juce::Point<float> p)
    {
        auto absP = getAbsoluteCanvasPosForPoint (p);
        float cx = getWidth() / 2.0f - chassisW / 2.0f;
        float cy = getHeight() / 2.0f - chassisH / 2.0f;
        return { absP.x - cx, absP.y - cy };
    }

    juce::Point<float> getAbsoluteCanvasPosForEvent (const juce::MouseEvent& e) { return getAbsoluteCanvasPosForPoint(e.position); }
    juce::Point<float> getCanvasPosForEvent (const juce::MouseEvent& e) { return getChassisRelativeCanvasPosForPoint(e.position); }

    int hitTestHardware (juce::Point<float> p)
    {
        for (int i = (int) placedHardware.size() - 1; i >= 0; --i)
        {
            auto& hw = placedHardware[i];
            if (juce::Rectangle<float> (hw.x, hw.y, hw.width, hw.height).contains (p))
                return i;
        }
        return -1;
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            deleteSelected();
            return true;
        }
        if (key.getKeyCode() == 'C' && key.getModifiers().isCommandDown())
        {
            clipboard.clear();
            for (int idx : selectedIndices)
                clipboard.push_back (placedHardware[idx]);
            return true;
        }
        if (key.getKeyCode() == 'V' && key.getModifiers().isCommandDown())
        {
            selectedIndices.clear();
            for (auto hw : clipboard)
            {
                hw.x += 20.0f; // offset slightly
                hw.y += 20.0f;
                hw.label = hw.type.substring(0,1).toUpperCase() + hw.type.substring(1) + " " + juce::String(placedHardware.size() + 1);
                hw.parameterID = ""; // unlink parameter by default on paste
                hw.controlID = hw.type + "_" + juce::String(placedHardware.size() + 1);
                placedHardware.push_back (hw);
                selectedIndices.insert (placedHardware.size() - 1);
            }
            // update clipboard to offset further next time
            for (auto& hw : clipboard) { hw.x += 20.0f; hw.y += 20.0f; }
            notifySelectionChanged();
            return true;
        }
        // Select All
        if (key.getKeyCode() == 'A' && key.getModifiers().isCommandDown())
        {
            selectedIndices.clear();
            for (int i=0; i < placedHardware.size(); ++i) selectedIndices.insert(i);
            notifySelectionChanged();
            return true;
        }
        return false;
    }

    int hitTestHandle (juce::Point<float> p, int& outHandle)
    {
        float hs = 16.0f; // larger hit size
        for (int idx : selectedIndices)
        {
            const auto& hw = placedHardware[idx];
            if (juce::Rectangle<float>(hw.x - hs, hw.y - hs, hs*2, hs*2).contains(p)) { outHandle = 0; return idx; }
            if (juce::Rectangle<float>(hw.x + hw.width - hs, hw.y - hs, hs*2, hs*2).contains(p)) { outHandle = 1; return idx; }
            if (juce::Rectangle<float>(hw.x - hs, hw.y + hw.height - hs, hs*2, hs*2).contains(p)) { outHandle = 2; return idx; }
            if (juce::Rectangle<float>(hw.x + hw.width - hs, hw.y + hw.height - hs, hs*2, hs*2).contains(p)) { outHandle = 3; return idx; }
        }
        if (selectedIndices.empty())
        {
            if (juce::Rectangle<float>(0 - hs, 0 - hs, hs*2, hs*2).contains(p)) { outHandle = 0; return -2; }
            if (juce::Rectangle<float>(chassisW - hs, 0 - hs, hs*2, hs*2).contains(p)) { outHandle = 1; return -2; }
            if (juce::Rectangle<float>(0 - hs, chassisH - hs, hs*2, hs*2).contains(p)) { outHandle = 2; return -2; }
            if (juce::Rectangle<float>(chassisW - hs, chassisH - hs, hs*2, hs*2).contains(p)) { outHandle = 3; return -2; }
        }
        return -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto cp = getCanvasPosForEvent (e);
        int handle = -1;
        if (hitTestHandle (cp, handle) >= 0)
        {
            if (handle == 0 || handle == 3) setMouseCursor (juce::MouseCursor::TopLeftCornerResizeCursor);
            else setMouseCursor (juce::MouseCursor::TopRightCornerResizeCursor);
        }
        else if (hitTestHardware (cp) >= 0)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        else
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        auto cp = getCanvasPosForEvent (e);
        isDraggingHardware = false;
        interactingControl = -1;
        isResizingHardware = false;
        isMarquee = false;

        // Check resize handles first
        int handle = -1;
        int hitHandle = hitTestHandle (cp, handle);
        if (hitHandle != -1)
        {
            if (hitHandle >= 0)
            {
                if (!selectedIndices.count(hitHandle))
                    setSelection({hitHandle});

                resizeStarts.clear();
                for (int idx : selectedIndices)
                    resizeStarts.push_back({idx, {placedHardware[idx].x, placedHardware[idx].y, placedHardware[idx].width, placedHardware[idx].height}});
            }
            isResizingHardware = true;
            resizeHandleId = handle;
            resizeStartPos = cp;
            return;
        }

        int hit = hitTestHardware (cp);
        if (hit >= 0)
        {
            auto& hw = placedHardware[hit];

            // Double-click toggles switches/footswitches/LEDs
            if (e.getNumberOfClicks() >= 2)
            {
                if (hw.type == "switch" || hw.type == "footswitch" || hw.type == "led")
                {
                    hw.value = hw.value > 0.5f ? 0.0f : 1.0f;
                    repaint();
                    return;
                }
            }

            // Command/Ctrl-click to interact with knob/fader value instead of moving (if we had control interactions)
            if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            {
                if (hw.type == "knob" || hw.type == "fader")
                {
                    interactingControl = hit;
                    interactStartValue = hw.value;
                    interactStartY = cp.y;
                    if (!selectedIndices.count(hit)) setSelection ({hit});
                    return;
                }
            }

            if (e.mods.isShiftDown())
            {
                toggleSelection (hit);
                if (! selectedIndices.count(hit)) return; // Don't drag if just deselected
            }
            else if (! selectedIndices.count(hit))
            {
                setSelection ({ hit });
            }

            if (e.mods.isAltDown()) // Option drag to duplicate
            {
                std::set<int> newSelection;
                for (int idx : selectedIndices)
                {
                    auto dup = placedHardware[idx];
                    dup.label = dup.type.substring(0,1).toUpperCase() + dup.type.substring(1) + " " + juce::String(placedHardware.size() + 1);
                    dup.parameterID = "";
                    dup.controlID = dup.type + "_" + juce::String(placedHardware.size() + 1);
                    placedHardware.push_back(dup);
                    newSelection.insert((int)placedHardware.size() - 1);
                }
                setSelection(newSelection);
            }

            dragStarts.clear();
            for (int idx : selectedIndices)
                dragStarts.push_back({idx, placedHardware[idx].x, placedHardware[idx].y});
            isDraggingHardware = true;
            hardwareDragStartPos = cp;
        }
        else
        {
            if (! e.mods.isShiftDown() && ! e.mods.isCommandDown())
                setSelection ({});

            if (e.mods.isMiddleButtonDown() || (e.mods.isLeftButtonDown() && e.mods.isAltDown()))
            {
                dragStartPan = { panX, panY };
            }
            else
            {
                isMarquee = true;
                marqueeStart = cp;
                marqueeRect = juce::Rectangle<float> (marqueeStart, marqueeStart);
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isResizingHardware)
        {
            if (resizeStarts.empty()) // Chassis resize
            {
                auto cp = getAbsoluteCanvasPosForEvent (e);
                float snapX = snapToGrid (cp.x);
                float snapY = snapToGrid (cp.y);

                float cx = getWidth() / 2.0f;
                float cy = getHeight() / 2.0f;

                float newW = chassisW;
                float newH = chassisH;

                if (resizeHandleId == 0) // TL
                {
                    newW = (cx - snapX) * 2.0f;
                    newH = (cy - snapY) * 2.0f;
                }
                else if (resizeHandleId == 1) // TR
                {
                    newW = (snapX - cx) * 2.0f;
                    newH = (cy - snapY) * 2.0f;
                }
                else if (resizeHandleId == 2) // BL
                {
                    newW = (cx - snapX) * 2.0f;
                    newH = (snapY - cy) * 2.0f;
                }
                else if (resizeHandleId == 3) // BR
                {
                    newW = (snapX - cx) * 2.0f;
                    newH = (snapY - cy) * 2.0f;
                }

                if (newW > 50.0f) chassisW = newW;
                if (newH > 50.0f) chassisH = newH;
                
                notifyHardwareChanged();
                return;
            }
            else // Hardware multi-resize
            {
                auto cp = getCanvasPosForEvent (e);
                float dx = cp.x - resizeStartPos.x;
                float dy = cp.y - resizeStartPos.y;

                for (auto& start : resizeStarts)
                {
                    auto& hw = placedHardware[start.idx];
                    float newX = start.rect.getX();
                    float newY = start.rect.getY();
                    float newR = start.rect.getRight();
                    float newB = start.rect.getBottom();

                    if (resizeHandleId == 0) // TL
                    {
                        newX = snapToGrid(start.rect.getX() + dx);
                        newY = snapToGrid(start.rect.getY() + dy);
                    }
                    else if (resizeHandleId == 1) // TR
                    {
                        newR = snapToGrid(start.rect.getRight() + dx);
                        newY = snapToGrid(start.rect.getY() + dy);
                    }
                    else if (resizeHandleId == 2) // BL
                    {
                        newX = snapToGrid(start.rect.getX() + dx);
                        newB = snapToGrid(start.rect.getBottom() + dy);
                    }
                    else if (resizeHandleId == 3) // BR
                    {
                        newR = snapToGrid(start.rect.getRight() + dx);
                        newB = snapToGrid(start.rect.getBottom() + dy);
                    }

                    if (newR - newX > 10) { hw.x = newX; hw.width = newR - newX; }
                    if (newB - newY > 10) { hw.y = newY; hw.height = newB - newY; }
                }
            }

            repaint();
            return;
        }
        else if (interactingControl >= 0)
        {
            auto cp = getCanvasPosForEvent (e);
            auto& hw = placedHardware[interactingControl];
            float deltaY = (interactStartY - cp.y) / 150.0f; // drag up = increase
            hw.value = juce::jlimit (0.0f, 1.0f, interactStartValue + deltaY);
            repaint();
            return;
        }
        else if (isDraggingHardware)
        {
            auto cp = getCanvasPosForEvent (e);
            float dx = cp.x - hardwareDragStartPos.x;
            float dy = cp.y - hardwareDragStartPos.y;

            for (auto& start : dragStarts)
            {
                placedHardware[start.idx].x = snapToGrid(start.startX + dx);
                placedHardware[start.idx].y = snapToGrid(start.startY + dy);
            }
            repaint();
            return;
        }
        else if (isMarquee)
        {
            auto cp = getCanvasPosForEvent (e);
            marqueeRect = juce::Rectangle<float> (marqueeStart, cp);
            repaint();
            return;
        }
        else
        {
            panX = dragStartPan.x + (float) e.getOffsetFromDragStart().x;
            panY = dragStartPan.y + (float) e.getOffsetFromDragStart().y;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (isMarquee)
        {
            isMarquee = false;
            for (int i = 0; i < (int)placedHardware.size(); ++i)
            {
                auto& hw = placedHardware[i];
                if (marqueeRect.intersects (juce::Rectangle<float>(hw.x, hw.y, hw.width, hw.height)))
                {
                    selectedIndices.insert(i);
                }
            }
            notifySelectionChanged();
            repaint();
            return;
        }

        interactingControl = -1;
        isDraggingHardware = false;
        isResizingHardware = false;
        resizeStarts.clear();
        dragStarts.clear();
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        float z = 1.0f + w.deltaY * 2.0f;
        if (z > 0) { scale = juce::jlimit (0.1f, 10.0f, scale * z); repaint(); }
    }

    // --- Drag & Drop target ---
    bool isInterestedInDragSource (const SourceDetails& d) override
    { return d.description.toString().startsWith ("hardware:"); }

    void itemDragEnter (const SourceDetails& d) override
    { showDragPreview = true; itemDragMove (d); }

    void itemDragMove (const SourceDetails& d) override
    {
        if (! showDragPreview) return;
        juce::StringArray tok; tok.addTokens (d.description.toString(), ":", "");
        if (tok.size() == 4)
        {
            dragPreviewType = tok[1];
            auto cp = getChassisRelativeCanvasPosForPoint (d.localPosition.toFloat());
            float pw = widthForType (tok[1]), ph = sizeForType (tok[1]);
            dragPreviewCanvasX = cp.x - tok[2].getFloatValue() * pw;
            dragPreviewCanvasY = cp.y - tok[3].getFloatValue() * ph;
            repaint();
        }
    }

    void itemDragExit (const SourceDetails&) override { showDragPreview = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        showDragPreview = false;
        juce::StringArray tok; tok.addTokens (d.description.toString(), ":", "");
        if (tok.size() == 4 && tok[0] == "hardware")
        {
            auto cp = getChassisRelativeCanvasPosForPoint (d.localPosition.toFloat());
            float pw = widthForType (tok[1]), ph = sizeForType (tok[1]);
            float cx = snapToGrid (cp.x - tok[2].getFloatValue() * pw);
            float cy = snapToGrid (cp.y - tok[3].getFloatValue() * ph);
            juce::String lbl = tok[1].substring(0,1).toUpperCase() + tok[1].substring(1)
                             + " " + juce::String ((int)placedHardware.size() + 1);
            // Default values: knobs/faders at 0.5, displays at 0.5 (shows mid), switches/lights off
            float defVal = 0.0f;
            if (tok[1] == "knob" || tok[1] == "fader" || tok[1] == "display"
                || tok[1] == "7seg" || tok[1] == "vu_meter")
                defVal = 0.5f;
            juce::String ctrlID = tok[1] + "_" + juce::String ((int)placedHardware.size() + 1);
            placedHardware.push_back ({ tok[1], cx, cy, pw, ph, lbl, "", defVal, ctrlID });
            setSelection ({(int) placedHardware.size() - 1});
            repaint();
        }
    }

    // --- Public API ---
    const std::set<int>& getSelectedIndices() const { return selectedIndices; }
    
    PlacedHardware* getSelectedHardware()
    {
        if (selectedIndices.size() == 1)
        {
            int idx = *selectedIndices.begin();
            if (idx >= 0 && idx < (int)placedHardware.size())
                return &placedHardware[idx];
        }
        return nullptr;
    }

    void deleteSelected()
    {
        if (selectedIndices.empty()) return;
        auto it = selectedIndices.rbegin();
        while (it != selectedIndices.rend())
        {
            if (*it >= 0 && *it < (int)placedHardware.size())
                placedHardware.erase (placedHardware.begin() + *it);
            ++it;
        }
        setSelection ({});
        repaint();
    }
    
    void notifyHardwareChanged() { repaint(); }

    friend class PedalDesignerComponent;

    juce::TextButton btnReset { "Reset View" }, btnRotate { "Rotate" };
    juce::TextButton btnGrid { "Grid: Med" }, btnSnap { "Snap" };
    juce::ComboBox chassisCombo;
    juce::TextButton btnColour { "Colour" };
    juce::TextButton btnSave { "Save" };
    int gridSizeIndex = 1;
    bool snapEnabled = true;
    float scale = 1.0f, panX = 0.0f, panY = 0.0f, rotation = 0.0f;
    juce::String chassisImage;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Point<float> dragStartPan;
    bool isDraggingHardware = false;
    juce::Point<float> hardwareDragStartPos;
    struct DragStart { int idx; float startX; float startY; };
    std::vector<DragStart> dragStarts;

    bool isResizingHardware = false;
    int resizeHandleId = -1;
    juce::Point<float> resizeStartPos;
    struct ResizeStart { int idx; juce::Rectangle<float> rect; };
    std::vector<ResizeStart> resizeStarts;

    int interactingControl = -1;
    float interactStartValue = 0.0f;
    float interactStartY = 0.0f;
    bool showDragPreview = false;
    juce::String dragPreviewType;
    float dragPreviewCanvasX = 0.0f, dragPreviewCanvasY = 0.0f;

    std::set<int> selectedIndices;
    std::vector<PlacedHardware> clipboard;
    std::vector<PlacedHardware> placedHardware;

    bool isMarquee = false;
    juce::Point<float> marqueeStart;
    juce::Rectangle<float> marqueeRect;

    void setSelection (const std::set<int>& newSel)
    {
        if (selectedIndices != newSel) { selectedIndices = newSel; if (onSelectionChanged) onSelectionChanged (); repaint(); }
    }

    void toggleSelection (int idx)
    {
        if (selectedIndices.count(idx)) selectedIndices.erase(idx);
        else selectedIndices.insert(idx);
        if (onSelectionChanged) onSelectionChanged();
        repaint();
    }
};

//==============================================================================
class PedalDesignerComponent::PropertiesPanel : public juce::Component,
                                                public juce::TextEditor::Listener,
                                                public juce::ComboBox::Listener
{
public:
    std::function<void()> onDeleteClicked;
    DSPGraph* effectsGraph = nullptr;

    PropertiesPanel()
    {
        auto setupEditor = [this] (juce::TextEditor& ed) {
            ed.setColour (juce::TextEditor::backgroundColourId, PedalForgeLookAndFeel::bgLight);
            ed.setColour (juce::TextEditor::textColourId, PedalForgeLookAndFeel::textPrimary);
            ed.setColour (juce::TextEditor::outlineColourId, PedalForgeLookAndFeel::gridLine);
            ed.setFont (juce::FontOptions (14.0f));
            ed.addListener (this);
            addChildComponent (ed);
        };
        setupEditor (labelEditor);
        setupEditor (wEditor);
        setupEditor (hEditor);
        setupEditor (nameEditor);
        setupEditor (authorEditor);
        
        descEditor.setMultiLine (true);
        descEditor.setReturnKeyStartsNewLine (true);
        setupEditor (descEditor);

        paramCombo.setColour (juce::ComboBox::backgroundColourId, PedalForgeLookAndFeel::bgLight);
        paramCombo.setColour (juce::ComboBox::textColourId, PedalForgeLookAndFeel::textPrimary);
        paramCombo.setColour (juce::ComboBox::outlineColourId, PedalForgeLookAndFeel::gridLine);
        paramCombo.addListener (this);
        addChildComponent (paramCombo);

        auto setupCombo = [this] (juce::ComboBox& cb) {
            cb.setColour (juce::ComboBox::backgroundColourId, PedalForgeLookAndFeel::bgLight);
            cb.setColour (juce::ComboBox::textColourId, PedalForgeLookAndFeel::textPrimary);
            cb.setColour (juce::ComboBox::outlineColourId, PedalForgeLookAndFeel::gridLine);
            cb.addListener (this);
            addChildComponent (cb);
        };
        
        setupCombo (fontStyleCombo);
        fontStyleCombo.addItem ("Normal", 1);
        fontStyleCombo.addItem ("Bold", 2);
        fontStyleCombo.addItem ("Italic", 3);
        fontStyleCombo.addItem ("Bold Italic", 4);

        setupCombo (fontFamilyCombo);
        fontFamilyCombo.addItem ("Sans Serif", 1);
        fontFamilyCombo.addItem ("Serif", 2);
        fontFamilyCombo.addItem ("Monospace", 3);

        setupCombo (colourCombo);
        colourCombo.addItem ("Default", 1);
        colourCombo.addItem ("White", 2);
        colourCombo.addItem ("Black", 3);
        colourCombo.addItem ("Red", 4);
        colourCombo.addItem ("Green", 5);
        colourCombo.addItem ("Blue", 6);
        colourCombo.addItem ("Yellow", 7);
        colourCombo.addItem ("Grey", 8);

        auto setupButton = [this] (juce::TextButton& btn, auto callback) {
            btn.onClick = callback;
            addChildComponent (btn);
        };
        setupButton (deleteButton, [this] { if (onDeleteClicked) onDeleteClicked(); });
        deleteButton.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::danger.withAlpha (0.3f));
        deleteButton.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::danger);

        setupButton (btnImageMain, [this] { pickImage (true); });
        setupButton (btnImageTrack, [this] { pickImage (false); });
        setupButton (btnClearImage, [this] { clearImage(); });
    }

    void pickImage (bool isMain)
    {
        if (canvas == nullptr) return;

        fileChooser = std::make_unique<juce::FileChooser> (
            "Select an image file...", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.png;*.jpg;*.jpeg");

        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync (chooserFlags, [this, isMain] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                auto* hw = canvas->getSelectedHardware();
                if (hw)
                {
                    if (isMain) hw->imageMain = file.getFullPathName();
                    else hw->imageTrack = file.getFullPathName();
                }
                else
                {
                    canvas->chassisImage = file.getFullPathName();
                }
                canvas->notifyHardwareChanged();
            }
        });
    }

    void clearImage()
    {
        if (canvas == nullptr) return;
        auto* hw = canvas->getSelectedHardware();
        if (hw)
        {
            hw->imageMain = "";
            hw->imageTrack = "";
        }
        else
        {
            canvas->chassisImage = "";
        }
        canvas->notifyHardwareChanged();
    }

    void setCanvas (ChassisCanvas* c) { canvas = c; }

    void showForIndices()
    {
        if (canvas == nullptr) return;
        auto& sel = canvas->getSelectedIndices();
        if (sel.size() == 1)
        {
            auto* hw = canvas->getSelectedHardware(); // wait, we need to add getSelectedHardware!
            // I'll define a helper here
            PlacedHardware* hwPtr = nullptr;
            if (*sel.begin() >= 0 && *sel.begin() < (int)canvas->placedHardware.size())
                hwPtr = &canvas->placedHardware[*sel.begin()];
            
            if (hwPtr)
            {
                labelEditor.setText (hwPtr->label, juce::dontSendNotification);
                wEditor.setText (juce::String (hwPtr->width), juce::dontSendNotification);
                hEditor.setText (juce::String (hwPtr->height), juce::dontSendNotification);
                rebuildParamCombo (hwPtr);

                bool showImages = (hwPtr->type != "led");
                bool showTrack = (hwPtr->type == "fader");
                bool isLabel = (hwPtr->type == "label");

                // Update combo boxes based on state
                if (isLabel)
                {
                    int fs = hwPtr->fontStyle;
                    if (fs == 0) fontStyleCombo.setSelectedId (1, juce::dontSendNotification);
                    else if (fs == 1) fontStyleCombo.setSelectedId (2, juce::dontSendNotification);
                    else if (fs == 2) fontStyleCombo.setSelectedId (3, juce::dontSendNotification);
                    else if (fs == 3) fontStyleCombo.setSelectedId (4, juce::dontSendNotification);

                    juce::String fam = hwPtr->fontFamily;
                    if (fam == "Sans") fontFamilyCombo.setSelectedId (1, juce::dontSendNotification);
                    else if (fam == "Serif") fontFamilyCombo.setSelectedId (2, juce::dontSendNotification);
                    else if (fam == "Monospace") fontFamilyCombo.setSelectedId (3, juce::dontSendNotification);
                    else fontFamilyCombo.setSelectedId (1, juce::dontSendNotification);
                }

                auto c = hwPtr->customColour;
                if (c == juce::Colours::red) colourCombo.setSelectedId (1, juce::dontSendNotification);
                else if (c == juce::Colours::white) colourCombo.setSelectedId (2, juce::dontSendNotification);
                else if (c == juce::Colours::black) colourCombo.setSelectedId (3, juce::dontSendNotification);
                else if (c == juce::Colour(0xFFFF3333)) colourCombo.setSelectedId (4, juce::dontSendNotification);
                else if (c == juce::Colour(0xFF33FF66)) colourCombo.setSelectedId (5, juce::dontSendNotification);
                else if (c == juce::Colour(0xFF3366FF)) colourCombo.setSelectedId (6, juce::dontSendNotification);
                else if (c == juce::Colour(0xFFFFDD33)) colourCombo.setSelectedId (7, juce::dontSendNotification);
                else if (c == juce::Colours::grey) colourCombo.setSelectedId (8, juce::dontSendNotification);
                else colourCombo.setSelectedId (1, juce::dontSendNotification);

                labelEditor.setVisible (true);
                wEditor.setVisible (true);
                hEditor.setVisible (true);
                paramCombo.setVisible (true);
                deleteButton.setVisible (true);

                btnImageMain.setVisible (!isLabel);
                btnImageMain.setButtonText ("Set Image...");
                btnImageTrack.setVisible (showTrack);
                btnClearImage.setVisible (!isLabel);

                fontStyleCombo.setVisible (isLabel);
                fontFamilyCombo.setVisible (isLabel);
                colourCombo.setVisible (true);

                nameEditor.setVisible (false);
                authorEditor.setVisible (false);
                descEditor.setVisible (false);
            }
        }
        else if (sel.empty())
        {
            // Chassis selected (no hardware selected)
            labelEditor.setVisible (false);
            wEditor.setVisible (true);
            hEditor.setVisible (true);
            wEditor.setText (juce::String (canvas->chassisW), juce::dontSendNotification);
            hEditor.setText (juce::String (canvas->chassisH), juce::dontSendNotification);
            
            nameEditor.setVisible (true);
            nameEditor.setText (canvas->pedalName, juce::dontSendNotification);
            authorEditor.setVisible (true);
            authorEditor.setText (canvas->pedalAuthor, juce::dontSendNotification);
            descEditor.setVisible (true);
            descEditor.setText (canvas->pedalDescription, juce::dontSendNotification);

            paramCombo.setVisible (false);
            deleteButton.setVisible (false);

            btnImageMain.setVisible (true);
            btnImageMain.setButtonText ("Set Chassis Image...");
            btnImageTrack.setVisible (false);
            btnClearImage.setVisible (true);

            fontStyleCombo.setVisible (false);
            fontFamilyCombo.setVisible (false);
            colourCombo.setVisible (false);
        }
        else
        {
            // Multiple items selected
            labelEditor.setVisible (false);
            wEditor.setVisible (false);
            hEditor.setVisible (false);
            paramCombo.setVisible (false);
            btnImageMain.setVisible (false);
            btnImageTrack.setVisible (false);
            btnClearImage.setVisible (false);
            fontStyleCombo.setVisible (false);
            fontFamilyCombo.setVisible (false);
            colourCombo.setVisible (false);
            nameEditor.setVisible (false);
            authorEditor.setVisible (false);
            descEditor.setVisible (false);
            deleteButton.setVisible (true);
        }
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (PedalForgeLookAndFeel::bgDark);
        g.setColour (PedalForgeLookAndFeel::gridLine);
        g.drawVerticalLine (0, 0, (float) getHeight());

        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
        g.drawText ("PROPERTIES", getLocalBounds().withTrimmedTop(10), juce::Justification::centredTop);

        if (canvas == nullptr) return;
        auto& sel = canvas->getSelectedIndices();
        
        if (sel.size() == 1)
        {
            PlacedHardware* hw = nullptr;
            if (*sel.begin() >= 0 && *sel.begin() < (int)canvas->placedHardware.size())
                hw = &canvas->placedHardware[*sel.begin()];
                
            if (hw)
            {
                int y = 50, m = 16;
                g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
                g.drawText ("TYPE", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft); y += 16;
                g.setColour (PedalForgeLookAndFeel::textPrimary); g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
                g.drawText (hw->type.substring(0,1).toUpperCase() + hw->type.substring(1), m, y, getWidth()-m*2, 22, juce::Justification::centredLeft); y += 34;

                g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
                g.drawText ("POSITION & SIZE", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft); y += 16;
                g.setColour (PedalForgeLookAndFeel::textSecondary); g.setFont (juce::FontOptions (13.0f));
                g.drawText ("X: " + juce::String((int)hw->x) + "   Y: " + juce::String((int)hw->y), m, y, getWidth()-m*2, 20, juce::Justification::centredLeft);
                g.drawText ("W:", m, y + 26, 20, 24, juce::Justification::centredLeft);
                g.drawText ("H:", m + 80, y + 26, 20, 24, juce::Justification::centredLeft);
                y += 60;

                // Control ID (read-only)
                g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
                g.drawText ("CONTROL ID", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft); y += 16;
                g.setColour (PedalForgeLookAndFeel::accent); g.setFont (juce::FontOptions (12.0f));
                g.drawText (hw->controlID, m, y, getWidth()-m*2, 18, juce::Justification::centredLeft); y += 26;

                g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
                g.drawText ("LABEL", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
                y = labelEditor.getBottom() + 12;

                g.drawText ("MAP TO PARAMETER", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
                y = paramCombo.getBottom() + 16;

                g.drawText ("CUSTOM RENDERING", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
                y += 20;

                if (hw->imageMain.isNotEmpty())
                {
                    g.setColour (PedalForgeLookAndFeel::textSecondary);
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText ("Main: " + juce::File(hw->imageMain).getFileName(), m, btnImageMain.getBottom() + 2, getWidth()-m*2, 14, juce::Justification::centredLeft);
                }
                if (hw->imageTrack.isNotEmpty())
                {
                    g.setColour (PedalForgeLookAndFeel::textSecondary);
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText ("Track: " + juce::File(hw->imageTrack).getFileName(), m, btnImageTrack.getBottom() + 2, getWidth()-m*2, 14, juce::Justification::centredLeft);
                }
            }
        }
        else if (sel.empty())
        {
            int y = 50, m = 16;
            g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
            g.drawText ("TYPE", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft); y += 16;
            g.setColour (PedalForgeLookAndFeel::textPrimary); g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
            g.drawText ("Pedal Chassis", m, y, getWidth()-m*2, 22, juce::Justification::centredLeft); y += 34;

            g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
            g.drawText ("CHASSIS SIZE", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft); y += 16;
            g.setColour (PedalForgeLookAndFeel::textSecondary); g.setFont (juce::FontOptions (13.0f));
            g.drawText ("W:", m, y + 26, 20, 24, juce::Justification::centredLeft);
            g.drawText ("H:", m + 80, y + 26, 20, 24, juce::Justification::centredLeft);
            y += 60;

            g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
            g.drawText ("PEDAL NAME", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
            y = nameEditor.getBottom() + 12;

            g.drawText ("AUTHOR", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
            y = authorEditor.getBottom() + 12;

            g.drawText ("DESCRIPTION", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
            y = descEditor.getBottom() + 16;

            g.drawText ("CUSTOM RENDERING", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
            y += 20;

            if (canvas->chassisImage.isNotEmpty())
            {
                g.setColour (PedalForgeLookAndFeel::textSecondary);
                g.setFont (juce::FontOptions (10.0f));
                g.drawText ("Image: " + juce::File(canvas->chassisImage).getFileName(), m, btnImageMain.getBottom() + 2, getWidth()-m*2, 14, juce::Justification::centredLeft);
            }
        }
        else
        {
            int y = 50, m = 16;
            g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
            g.drawText ("SELECTION", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft); y += 16;
            g.setColour (PedalForgeLookAndFeel::textPrimary); g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
            g.drawText (juce::String(sel.size()) + " Components Selected", m, y, getWidth()-m*2, 22, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        int m = 16;
        int y = 142; // After X/Y
        wEditor.setBounds (m + 24, y, 48, 24);
        hEditor.setBounds (m + 104, y, 48, 24);

        if (canvas == nullptr) return;
        auto& sel = canvas->getSelectedIndices();
        if (sel.empty())
        {
            // Chassis mode sizes
            y = 192;
            nameEditor.setBounds (m, y, getWidth()-m*2, 28);
            
            y = nameEditor.getBottom() + 28;
            authorEditor.setBounds (m, y, getWidth()-m*2, 28);
            
            y = authorEditor.getBottom() + 28;
            descEditor.setBounds (m, y, getWidth()-m*2, 100);
            
            y = descEditor.getBottom() + 36;
            btnImageMain.setBounds (m, y, getWidth()-m*2, 24);
            y += 30;
            btnClearImage.setBounds (m, y, getWidth()-m*2, 24);
        }
        else if (sel.size() == 1)
        {
            auto* hw = canvas->getSelectedHardware();
            bool isLabel = hw && hw->type == "label";

            // Component mode sizes
            y = 234;
            labelEditor.setBounds (m, y, getWidth()-m*2, 28);
            
            y = labelEditor.getBottom() + 28;
            paramCombo.setBounds   (m, y, getWidth()-m*2, 28);

            y = paramCombo.getBottom() + 36;
            if (isLabel)
            {
                fontFamilyCombo.setBounds (m, y, getWidth()-m*2, 28); y += 36;
                fontStyleCombo.setBounds (m, y, getWidth()-m*2, 28); y += 36;
                colourCombo.setBounds (m, y, getWidth()-m*2, 28);
            }
            else
            {
                btnImageMain.setBounds (m, y, getWidth()-m*2, 24); y += 30;
                btnImageTrack.setBounds (m, y, getWidth()-m*2, 24); y += 30;
                colourCombo.setBounds (m, y, getWidth()-m*2, 28); y += 36;
                btnClearImage.setBounds (m, y, getWidth()-m*2, 24);
            }

            deleteButton.setBounds (m, getHeight()-50, getWidth()-m*2, 32);
        }
        else
        {
            // Multi select sizes
            deleteButton.setBounds (m, getHeight()-50, getWidth()-m*2, 32);
        }
    }

    void textEditorTextChanged (juce::TextEditor& editor) override
    {
        if (canvas == nullptr) return;
        auto& sel = canvas->getSelectedIndices();
        if (sel.empty())
        {
            // Edit chassis properties
            if (&editor == &wEditor) { canvas->chassisW = juce::jmax(50.0f, wEditor.getText().getFloatValue()); canvas->notifyHardwareChanged(); }
            else if (&editor == &hEditor) { canvas->chassisH = juce::jmax(50.0f, hEditor.getText().getFloatValue()); canvas->notifyHardwareChanged(); }
            else if (&editor == &nameEditor) { canvas->pedalName = nameEditor.getText(); }
            else if (&editor == &authorEditor) { canvas->pedalAuthor = authorEditor.getText(); }
            else if (&editor == &descEditor) { canvas->pedalDescription = descEditor.getText(); }
            return;
        }

        if (sel.size() == 1)
        {
            PlacedHardware* hw = nullptr;
            if (*sel.begin() >= 0 && *sel.begin() < (int)canvas->placedHardware.size())
                hw = &canvas->placedHardware[*sel.begin()];
                
            if (hw)
            {
                if (&editor == &labelEditor) { hw->label = labelEditor.getText(); canvas->notifyHardwareChanged(); }
                else if (&editor == &wEditor) { hw->width = juce::jmax(10.0f, wEditor.getText().getFloatValue()); canvas->notifyHardwareChanged(); }
                else if (&editor == &hEditor) { hw->height = juce::jmax(10.0f, hEditor.getText().getFloatValue()); canvas->notifyHardwareChanged(); }
            }
        }
    }

    void comboBoxChanged (juce::ComboBox* box) override
    {
        if (canvas == nullptr) return;
        auto& sel = canvas->getSelectedIndices();
        if (sel.size() == 1)
        {
            PlacedHardware* hw = nullptr;
            if (*sel.begin() >= 0 && *sel.begin() < (int)canvas->placedHardware.size())
                hw = &canvas->placedHardware[*sel.begin()];
                
            if (hw)
            {
                if (box == &paramCombo)
                {
                    int s = paramCombo.getSelectedId();
                    if (s == 1) // "(none)"
                        hw->parameterID = "";
                    else
                    {
                        // Extract the [fullID] from the display string
                        juce::String txt = paramCombo.getText();
                        int start = txt.lastIndexOfChar ('[');
                        int end = txt.lastIndexOfChar (']');
                        if (start >= 0 && end > start)
                            hw->parameterID = txt.substring (start + 1, end);
                    }
                }
                else if (box == &fontStyleCombo)
                {
                    int s = fontStyleCombo.getSelectedId();
                    if (s == 1) hw->fontStyle = 0; // Normal
                    else if (s == 2) hw->fontStyle = 1; // Bold
                    else if (s == 3) hw->fontStyle = 2; // Italic
                    else if (s == 4) hw->fontStyle = 3; // Bold Italic
                    
                    canvas->notifyHardwareChanged();
                }
                else if (box == &fontFamilyCombo)
                {
                    int s = fontFamilyCombo.getSelectedId();
                    if (s == 1) hw->fontFamily = "Sans";
                    else if (s == 2) hw->fontFamily = "Serif";
                    else if (s == 3) hw->fontFamily = "Monospace";
                    
                    canvas->notifyHardwareChanged();
                }
                else if (box == &colourCombo)
                {
                    int s = colourCombo.getSelectedId();
                    if (s == 2) hw->customColour = juce::Colours::white;
                    else if (s == 3) hw->customColour = juce::Colours::black;
                    else if (s == 4) hw->customColour = juce::Colour(0xFFFF3333);
                    else if (s == 5) hw->customColour = juce::Colour(0xFF33FF66);
                    else if (s == 6) hw->customColour = juce::Colour(0xFF3366FF);
                    else if (s == 7) hw->customColour = juce::Colour(0xFFFFDD33);
                    else if (s == 8) hw->customColour = juce::Colours::grey;
                    else hw->customColour = juce::Colours::red; // Default
                    canvas->notifyHardwareChanged();
                }
            }
        }
    }

private:
    ChassisCanvas* canvas = nullptr;
    juce::TextEditor labelEditor;
    juce::TextEditor wEditor, hEditor;
    juce::TextEditor nameEditor, authorEditor, descEditor;
    juce::ComboBox paramCombo, fontStyleCombo, fontFamilyCombo, colourCombo;
    juce::TextButton deleteButton { "Delete Component" };
    juce::TextButton btnImageMain { "Set Image..." };
    juce::TextButton btnImageTrack { "Set Track Image..." };
    juce::TextButton btnClearImage { "Clear Images" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    void rebuildParamCombo (PlacedHardware* hw)
    {
        paramCombo.clear (juce::dontSendNotification);
        paramCombo.addItem ("(none)", 1);

        int itemID = 2;
        if (effectsGraph != nullptr)
        {
            for (const auto& [nodeID, node] : effectsGraph->getNodes())
            {
                if (node->getType() == "audio_input" || node->getType() == "audio_output")
                    continue;
                for (const auto& param : node->getParams())
                {
                    juce::String fullID = juce::String (nodeID) + "_" + param.id;
                    juce::String display = node->getName() + " : " + param.name;
                    paramCombo.addItem (display + "  [" + fullID + "]", itemID);
                    if (hw && hw->parameterID == fullID)
                        paramCombo.setSelectedId (itemID, juce::dontSendNotification);
                    itemID++;
                }
            }
        }

        if (hw && hw->parameterID.isEmpty())
            paramCombo.setSelectedId (1, juce::dontSendNotification);
    }
};

//==============================================================================
 PedalDesignerComponent::PedalDesignerComponent()
{
    canvas = std::make_unique<ChassisCanvas>();    addAndMakeVisible (*canvas);
    properties = std::make_unique<PropertiesPanel>(); addAndMakeVisible (*properties);

    properties->setCanvas (canvas.get());
    canvas->onSelectionChanged = [this] () { properties->showForIndices (); };
    properties->onDeleteClicked = [this] { canvas->deleteSelected(); };

    // Initialize properties panel to chassis immediately
    properties->showForIndices();
}

PedalDesignerComponent::~PedalDesignerComponent() = default;

void PedalDesignerComponent::paint (juce::Graphics&) {}

void PedalDesignerComponent::resized()
{
    auto area = getLocalBounds();
    properties->setBounds (area.removeFromRight (250));
    canvas->setBounds (area);
}

void PedalDesignerComponent::setEffectsGraph (DSPGraph* graph)
{
    effectsGraph = graph;
    if (properties)
        properties->effectsGraph = graph;
    if (canvas)
        canvas->onGetEffectsGraph = [graph]() -> juce::var {
            return graph ? graph->toJSON() : juce::var();
        };
}

void PedalDesignerComponent::loadDesign (const PedalDesign& design)
{
    if (canvas)
    {
        canvas->placedHardware.clear();
        canvas->chassisW = design.chassisW;
        canvas->chassisH = design.chassisH;
        canvas->chassisColour = design.chassisColour;
        canvas->chassisImage = design.chassisImage;
        canvas->pedalName = design.name;
        canvas->pedalAuthor = design.author;
        canvas->pedalDescription = design.description;
        canvas->pedalCategory = design.category;

        // Find matching preset index
        for (int i = 0; i < canvas->numChassisPresets; ++i)
        {
            if (std::abs (canvas->chassisPresets[i].w - design.chassisW) < 1.0f &&
                std::abs (canvas->chassisPresets[i].h - design.chassisH) < 1.0f)
            {
                canvas->chassisPresetIndex = i;
                canvas->chassisCombo.setSelectedId (i + 1, juce::dontSendNotification);
                break;
            }
        }

        // Load controls
        for (const auto& ctrl : design.controls)
        {
            PlacedHardware hw;
            hw.type = ctrl.type;
            hw.x = ctrl.x;
            hw.y = ctrl.y;
            hw.width = ctrl.width;
            hw.height = ctrl.height;
            hw.label = ctrl.label;
            hw.parameterID = "";
            hw.value = ctrl.defaultValue;
            hw.controlID = ctrl.controlID;
            hw.imageMain = ctrl.imageMain;
            hw.imageTrack = ctrl.imageTrack;
            hw.customColour = ctrl.customColour;
            hw.stretchImage = ctrl.stretchImage;
            hw.fontFamily = ctrl.fontFamily;
            hw.fontStyle = ctrl.fontStyle;

            // Find parameterID from mappings
            for (const auto& m : design.mappings)
                if (m.controlID == ctrl.controlID)
                    hw.parameterID = m.nodeParam;

            canvas->placedHardware.push_back (hw);
        }

        canvas->repaint();
    }
    if (properties)
        properties->showForIndices ();
}

void PedalDesignerComponent::clearDesign()
{
    if (canvas)
    {
        canvas->placedHardware.clear();
        canvas->chassisW = 200.0f;
        canvas->chassisH = 340.0f;
        canvas->chassisColour = juce::Colour (0xFF8A8A94);
        canvas->chassisPresetIndex = 1;
        canvas->chassisCombo.setSelectedId (2, juce::dontSendNotification);
        canvas->chassisColourIndex = 0;
        canvas->repaint();
    }
    if (properties)
        properties->showForIndices ();
}

PedalDesign PedalDesignerComponent::getDesign() const
{
    if (canvas)
        return canvas->buildPedalDesign();
    return PedalDesign();
}

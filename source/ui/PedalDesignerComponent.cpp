#include "PedalDesignerComponent.h"
#include "HardwareDrawing.h"
#include "StyleKit.h"
#include "../dsp/DSPGraph.h"
#include "../dsp/PedalDesign.h"
#include "../util/AppPaths.h"

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
    juce::StringArray imageStates;// Per-state images (state 0/1/2/…); falls back to imageMain
    juce::Colour customColour { juce::Colours::red }; // Color for LED glow
    bool stretchImage = true;     // Whether to stretch image or keep aspect ratio
    juce::String fontFamily = "Sans";
    int fontStyle = 1;            // 0=Plain, 1=Bold, 2=Italic, 3=BoldItalic

    // Knob visual/interaction properties
    float rotationRange = 270.0f; // visual arc in degrees
    float sensitivity = 200.0f;   // pixels of drag for full sweep
    int   positions = 4;          // number of positions for rotary selector (2-16)
    
    bool isLocked = false;        // prevents dragging and selection on canvas
    juce::String overlayPage;     // target page for overlay_launcher
};

//==============================================================================
/** Context for a single overlay page (or the pedal face). */
struct PageContext
{
    juce::String pageName;                         // "" = pedal face
    float width = 800.0f, height = 600.0f;
    juce::Colour bgColour { 0xFF222222 };
    std::vector<PlacedHardware> hardware;
};

//==============================================================================
class HardwareItem : public juce::Component
{
public:
    HardwareItem (const juce::String& t, const juce::String& displayName)
        : type (t), name (displayName) {}

    void paint (juce::Graphics& g) override
    {
        pf::StyleKitRegistry::draw (g, "default", type, getLocalBounds().toFloat(),
                                    pf::ControlState (0.5f), pf::Colorway{}, nullptr);
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

    // Snap grid size in mm
    float gridSize = 1.0f; // 1mm snap default

    // Chassis presets — real-world enclosure dimensions in millimeters
    struct ChassisPreset { const char* name; float w; float h; };
    inline static const ChassisPreset chassisPresets[] = {
        { "1590A",   50.0f,  90.0f },   // ~50 x 90 mm
        { "1590B",   60.0f, 112.0f },   // ~60 x 112 mm
        { "1590BB",  72.0f, 120.0f },   // ~72 x 120 mm
        { "125B",    80.0f, 120.0f },   // ~80 x 120 mm
        { "1032L",  125.0f,  80.0f },   // ~125 x 80 mm (wide)
        { "1590DD", 145.0f, 121.0f },   // ~145 x 121 mm (dual)
    };
    static constexpr int numChassisPresets = 6;
    int chassisPresetIndex = 1; // default 1590B
    float chassisW = 60.0f, chassisH = 112.0f;  // default 1590B in mm

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

    // Style engine: the design's colorway, so the designer canvas previews the
    // same tint the live faceplate (PedalPainter) renders. 0 seed = inactive =
    // default look. Built into a pf::Colorway in paint().
    juce::int64 colorwaySeed = 0;
    int         colorwayMode = 0;   // 0 = Semantic, 1 = Tint

    // Component sizes per type in mm
    static float sizeForType (const juce::String& type)
    {
        if (type == "fader") return 12.0f;
        if (type == "led" || type == "rgb_led" || type == "indicator") return 5.0f;
        if (type == "7seg") return 10.0f;
        if (type == "display") return 8.0f;
        if (type == "text_screen" || type == "console") return 15.0f;
        if (type == "pixel_display") return 12.0f;
        if (type == "label") return 6.0f;
        if (type == "vu_meter") return 18.0f;
        if (type == "oscilloscope") return 15.0f;
        if (type == "xypad") return 25.0f;
        if (type == "joystick") return 22.0f;
        if (type == "file_loader" || type == "plugin_browser" || type == "overlay_launcher") return 8.0f;
        return 12.0f; // knob, switch, footswitch
    }
    static float widthForType (const juce::String& type)
    {
        if (type == "fader") return 30.0f;
        if (type == "7seg") return 22.0f;
        if (type == "display") return 22.0f;
        if (type == "text_screen" || type == "console") return 25.0f;
        if (type == "pixel_display") return 25.0f;
        if (type == "label") return 25.0f;
        if (type == "vu_meter") return 6.0f;
        if (type == "oscilloscope") return 25.0f;
        if (type == "xypad") return 25.0f;
        if (type == "joystick") return 22.0f;
        if (type == "file_loader" || type == "plugin_browser" || type == "overlay_launcher") return 22.0f;
        return sizeForType (type);
    }

    float snapToGrid (float val) const
    {
        return std::round (val / gridSize) * gridSize;
    }

    ChassisCanvas()
    {
        setWantsKeyboardFocus (true);

        addAndMakeVisible (btnSave);
        btnSave.onClick = [this] { savePedalDesign(); };

        addAndMakeVisible (btnExport);
        btnExport.onClick = [this] { exportPedalDesign(); };
    }

    juce::var cachedEffectsGraph;

    juce::String pedalUuid;        // empty = fresh design; otherwise preserved across saves
    juce::String pedalName = "My Pedal";
    juce::String pedalAuthor = "User";
    juce::String pedalDescription = "";
    juce::String pedalCategory = "Custom";
    juce::StringArray pedalTags;

    void savePedalDesign()
    {
        if (pedalName.isEmpty()) pedalName = "Untitled Pedal";

        PedalDesign design = buildPedalDesign (pedalFaceBackupPtr);

        // Save to designs directory
        auto designsDir = pf::paths::getDesignsDir();

        auto file = designsDir.getChildFile (pedalName.replace (" ", "_") + ".json");
        design.saveToFile (file);

        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                 "Saved!",
                                                 "Pedal design saved to:\n" + file.getFullPathName());
    }

    /** Export the current pedal as a .pfpedal file to a user-chosen location.
        The file is just a PedalDesign JSON serialisation; .pfpedal is a friendly
        extension so users (and Finder) know what it is when shared. */
    void exportPedalDesign()
    {
        if (pedalName.isEmpty()) pedalName = "Untitled Pedal";

        auto suggested = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                             .getChildFile (pedalName.replace (" ", "_") + ".pfpedal");

        exportChooser = std::make_unique<juce::FileChooser> (
            "Export pedal as...", suggested, "*.pfpedal");

        int flags = juce::FileBrowserComponent::saveMode
                  | juce::FileBrowserComponent::canSelectFiles
                  | juce::FileBrowserComponent::warnAboutOverwriting;

        exportChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;

            // Ensure the file has a .pfpedal extension even if the OS dialog
            // didn't add it (Linux/Windows behaviour varies).
            if (! file.hasFileExtension ("pfpedal"))
                file = file.withFileExtension ("pfpedal");

            PedalDesign design = buildPedalDesign (pedalFaceBackupPtr);
            if (design.saveToFile (file))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                    "Exported", "Pedal saved to:\n" + file.getFullPathName());
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                    "Export Failed", "Could not write to:\n" + file.getFullPathName());
            }
        });
    }

    PedalDesign buildPedalDesign (const std::vector<PlacedHardware>* pedalFaceBackup = nullptr) const
    {
        PedalDesign design;
        // Preserve the existing design's UUID across saves; the default ctor
        // already generated one if this canvas was loaded fresh.
        if (pedalUuid.isNotEmpty()) design.uuid = pedalUuid;
        design.name = pedalName;
        design.author = pedalAuthor;
        design.description = pedalDescription;
        design.category = pedalCategory;
        design.tags = pedalTags;
        design.chassisW = chassisW;
        design.chassisH = chassisH;
        design.chassisColour = chassisColour;
    design.colorwaySeed = colorwaySeed;
    design.colorwayMode = colorwayMode;
        design.chassisImage = chassisImage;

        // Helper: convert PlacedHardware → PedalDesign::Control + Mapping
        auto convertHardware = [&design](const std::vector<PlacedHardware>& hardware)
        {
            std::vector<PedalDesign::Control> controls;
            for (const auto& hw : hardware)
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
                ctrl.imageStates = hw.imageStates;
                ctrl.positions = hw.positions;
                ctrl.customColour = hw.customColour;
                ctrl.stretchImage = hw.stretchImage;
                ctrl.fontFamily = hw.fontFamily;
                ctrl.fontStyle = hw.fontStyle;
                ctrl.rotationRange = hw.rotationRange;
                ctrl.sensitivity = hw.sensitivity;
                ctrl.overlayPage = hw.overlayPage;
                controls.push_back (ctrl);

                if (hw.parameterID.isNotEmpty())
                {
                    PedalDesign::Mapping m;
                    m.controlID = hw.controlID;
                    m.nodeParam = hw.parameterID;
                    design.mappings.push_back (m);
                }
            }
            return controls;
        };

        // If editing an overlay page, placedHardware is the page's controls
        // and the pedal face is stashed in pedalFaceBackup.
        if (activePageIndex >= 0 && pedalFaceBackup != nullptr)
        {
            design.controls = convertHardware (*pedalFaceBackup);

            // Build overlay pages, substituting placedHardware for the active page
            for (int i = 0; i < (int)overlayPages.size(); ++i)
            {
                PedalDesign::CanvasPage cp;
                cp.pageName = overlayPages[i].pageName;
                cp.width = overlayPages[i].width;
                cp.height = overlayPages[i].height;
                cp.backgroundColour = overlayPages[i].bgColour;
                cp.controls = convertHardware (i == activePageIndex ? placedHardware : overlayPages[i].hardware);
                design.canvasPages.push_back (cp);
            }
        }
        else
        {
            // Not editing an overlay page — placedHardware IS the pedal face
            design.controls = convertHardware (placedHardware);

            for (const auto& page : overlayPages)
            {
                PedalDesign::CanvasPage cp;
                cp.pageName = page.pageName;
                cp.width = page.width;
                cp.height = page.height;
                cp.backgroundColour = page.bgColour;
                cp.controls = convertHardware (page.hardware);
                design.canvasPages.push_back (cp);
            }
        }

        // Preserve the effects graph that was loaded
        design.effectsGraph = cachedEffectsGraph;

        return design;
    }

    void resized() override
    {
        btnSave.setBounds   (getWidth() - 90,  10, 80, 24);
        btnExport.setBounds (getWidth() - 180, 10, 85, 24);
    }

    void paint (juce::Graphics& g) override
    {
        // ── Checkerboard artboard background ──
        {
            int tileSize = 12;
            juce::Colour c1 (0xFF101018);
            juce::Colour c2 (0xFF141420);
            for (int y = 0; y < getHeight(); y += tileSize)
                for (int x = 0; x < getWidth(); x += tileSize)
                {
                    g.setColour (((x / tileSize + y / tileSize) & 1) ? c2 : c1);
                    g.fillRect (x, y, tileSize, tileSize);
                }
        }
        g.saveState();

        juce::AffineTransform t = juce::AffineTransform::rotation (rotation, getWidth() / 2.0f, getHeight() / 2.0f)
                                  .scaled (scale, scale, getWidth() / 2.0f, getHeight() / 2.0f)
                                  .translated (panX, panY);
        g.addTransform (t);

        float drawW, drawH;
        juce::Colour drawBg;
        getActivePageDimensions (drawW, drawH, drawBg);

        float cx = getWidth() / 2.0f - drawW / 2.0f;
        float cy = getHeight() / 2.0f - drawH / 2.0f;
        g.addTransform (juce::AffineTransform::translation (cx, cy));

        // ── Drop shadow ──
        {
            float sh = 8.0f;
            juce::Colour shadowCol (0x40000000);
            for (float i = sh; i > 0; i -= 2.0f)
            {
                g.setColour (shadowCol.withMultipliedAlpha (1.0f - i / sh));
                g.fillRoundedRectangle (-i, -i + 2, drawW + i * 2, drawH + i * 2, 6.0f + i * 0.5f);
            }
        }

        // ── Draw chassis or overlay page background ──
        if (isEditingOverlayPage())
        {
            g.setColour (drawBg);
            g.fillRoundedRectangle (0, 0, drawW, drawH, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.drawRoundedRectangle (0, 0, drawW, drawH, 6.0f, 1.5f);
        }
        else
        {
            HardwareDrawing::CustomStyles chassisStyles;
            chassisStyles.imageChassis = chassisImage;
            HardwareDrawing::drawChassis (g, { 0, 0, drawW, drawH }, chassisColour, &chassisStyles);
        }

        // ── Grid overlay — major/minor lines ──
        if (gridSize >= 0.5f)
        {
            float gs = gridSize;
            float majorEvery = (gs <= 1.0f) ? 10.0f : (gs <= 2.0f) ? 10.0f : (gs <= 5.0f) ? 5.0f : 1.0f;
            float majorStep = gs * majorEvery;

            // Minor grid lines
            g.setColour (juce::Colour (0x0CFFFFFF));
            for (float gx = gs; gx < drawW; gx += gs)
                g.drawLine (gx, 0, gx, drawH, 0.5f);
            for (float gy = gs; gy < drawH; gy += gs)
                g.drawLine (0, gy, drawW, gy, 0.5f);

            // Major grid lines
            g.setColour (juce::Colour (0x22FFFFFF));
            for (float gx = majorStep; gx < drawW; gx += majorStep)
                g.drawLine (gx, 0, gx, drawH, 1.0f);
            for (float gy = majorStep; gy < drawH; gy += majorStep)
                g.drawLine (0, gy, drawW, gy, 1.0f);

            // Centre crosshair
            g.setColour (juce::Colour (0x18FF6366));
            float midX = drawW / 2.0f, midY = drawH / 2.0f;
            g.drawLine (midX, 0, midX, drawH, 0.5f);
            g.drawLine (0, midY, drawW, midY, 0.5f);
        }

        // Build the design's colorway once so the canvas previews the same tint
        // the live faceplate (PedalPainter) renders. Inactive when no seed set.
        pf::Colorway canvasColorway;
        if (colorwaySeed != 0)
        {
            juce::Colour seed ((juce::uint32) (juce::int64) colorwaySeed);
            if (colorwayMode == 1)
                canvasColorway = pf::Colorway::tintFromSeed (seed);
            else
                { canvasColorway.mode = pf::Colorway::Mode::Semantic; canvasColorway.accent = seed; canvasColorway.active = true; }
        }

        // ── Draw placed hardware ──
        for (int i = 0; i < (int) placedHardware.size(); ++i)
        {
            const auto& hw = placedHardware[i];
            HardwareDrawing::CustomStyles styles;
            styles.imageMain = hw.imageMain;
            styles.imageTrack = hw.imageTrack;
            styles.imageStates = hw.imageStates;
            styles.positions = hw.positions;
            styles.customColour = hw.customColour;
            styles.stretchImage = hw.stretchImage;

            // Dim locked items slightly
            if (hw.isLocked && !selectedIndices.count(i))
                g.setOpacity (0.55f);

            pf::StyleKitRegistry::draw (g, "default", hw.type, { hw.x, hw.y, hw.width, hw.height },
                                        pf::ControlState (hw.value), canvasColorway, &styles);
            g.setOpacity (1.0f);

            // ── Selection visuals ──
            if (selectedIndices.count(i))
            {
                // Accent glow
                g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.15f));
                g.fillRoundedRectangle (hw.x - 5, hw.y - 5, hw.width + 10, hw.height + 10, 6.0f);

                // Dashed selection border
                g.setColour (PedalForgeLookAndFeel::accent);
                float dashLen[] = { 4.0f, 3.0f };
                juce::Path selPath;
                selPath.addRoundedRectangle (hw.x - 2, hw.y - 2, hw.width + 4, hw.height + 4, 4.0f);
                juce::PathStrokeType stroke (1.5f);
                stroke.createDashedStroke (selPath, selPath, dashLen, 2);
                g.strokePath (selPath, juce::PathStrokeType (1.5f));

                // Corner handles
                float hs = 7.0f;
                auto drawHandle = [&](float hx, float hy) {
                    g.setColour (PedalForgeLookAndFeel::accent);
                    g.fillRoundedRectangle (hx - hs/2, hy - hs/2, hs, hs, 2.0f);
                    g.setColour (juce::Colours::white);
                    g.fillRoundedRectangle (hx - hs/2 + 1, hy - hs/2 + 1, hs - 2, hs - 2, 1.5f);
                };
                drawHandle (hw.x, hw.y);
                drawHandle (hw.x + hw.width, hw.y);
                drawHandle (hw.x, hw.y + hw.height);
                drawHandle (hw.x + hw.width, hw.y + hw.height);
            }

            // ── Lock badge on locked items ──
            if (hw.isLocked)
            {
                float bx = hw.x + hw.width - 6, by = hw.y - 2;
                g.setColour (juce::Colour (0xCC222233));
                g.fillRoundedRectangle (bx, by, 10, 10, 3.0f);
                g.setColour (juce::Colour (0xFFFF9944));
                g.setFont (juce::FontOptions (8.0f).withStyle ("Bold"));
                g.drawText ("L", (int)bx, (int)by, 10, 10, juce::Justification::centred);
            }

            // ── Label below component ──
            if (hw.label.isNotEmpty() && hw.type != "label" && hw.type != "graphic")
            {
                g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (0.85f));
                g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
                g.drawText (hw.label, (int)(hw.x - 20), (int)(hw.y + hw.height + 2),
                           (int)(hw.width + 40), 14, juce::Justification::centredTop);
            }
        }

        // ── Snap guide lines (drawn when single-item drag is snapping) ──
        if (isDraggingHardware && selectedIndices.size() == 1)
        {
            int idx = *selectedIndices.begin();
            const auto& hw = placedHardware[idx];
            g.setColour (juce::Colour (0xAAFF4488));
            for (int i = 0; i < (int) placedHardware.size(); ++i)
            {
                if (i == idx) continue;
                const auto& other = placedHardware[i];
                float snapDist = 2.0f;

                // Vertical guides
                if (std::abs(hw.x - other.x) < 0.1f)
                    g.drawLine (hw.x, juce::jmin(hw.y, other.y) - 10, hw.x, juce::jmax(hw.y + hw.height, other.y + other.height) + 10, 0.5f);
                if (std::abs((hw.x + hw.width/2) - (other.x + other.width/2)) < 0.1f)
                { float mx = hw.x + hw.width / 2; g.drawLine (mx, juce::jmin(hw.y, other.y) - 10, mx, juce::jmax(hw.y + hw.height, other.y + other.height) + 10, 0.5f); }
                if (std::abs((hw.x + hw.width) - (other.x + other.width)) < 0.1f)
                { float rx = hw.x + hw.width; g.drawLine (rx, juce::jmin(hw.y, other.y) - 10, rx, juce::jmax(hw.y + hw.height, other.y + other.height) + 10, 0.5f); }

                // Horizontal guides
                if (std::abs(hw.y - other.y) < 0.1f)
                    g.drawLine (juce::jmin(hw.x, other.x) - 10, hw.y, juce::jmax(hw.x + hw.width, other.x + other.width) + 10, hw.y, 0.5f);
                if (std::abs((hw.y + hw.height/2) - (other.y + other.height/2)) < 0.1f)
                { float my = hw.y + hw.height / 2; g.drawLine (juce::jmin(hw.x, other.x) - 10, my, juce::jmax(hw.x + hw.width, other.x + other.width) + 10, my, 0.5f); }
                if (std::abs((hw.y + hw.height) - (other.y + other.height)) < 0.1f)
                { float by = hw.y + hw.height; g.drawLine (juce::jmin(hw.x, other.x) - 10, by, juce::jmax(hw.x + hw.width, other.x + other.width) + 10, by, 0.5f); }
            }
        }

        // ── Chassis resize handles ──
        if (selectedIndices.empty())
        {
            float hs = 7.0f;
            auto drawHandle = [&](float hx, float hy) {
                g.setColour (PedalForgeLookAndFeel::accent);
                g.fillRoundedRectangle (hx - hs/2, hy - hs/2, hs, hs, 2.0f);
                g.setColour (juce::Colours::white);
                g.fillRoundedRectangle (hx - hs/2 + 1, hy - hs/2 + 1, hs - 2, hs - 2, 1.5f);
            };
            drawHandle (0, 0);
            drawHandle (chassisW, 0);
            drawHandle (0, chassisH);
            drawHandle (chassisW, chassisH);
        }

        // ── Drag preview ──
        if (showDragPreview)
        {
            float pw = widthForType (dragPreviewType), ph = sizeForType (dragPreviewType);
            float sx = snapToGrid (dragPreviewCanvasX);
            float sy = snapToGrid (dragPreviewCanvasY);
            g.setOpacity (0.45f);
            pf::StyleKitRegistry::draw (g, "default", dragPreviewType, { sx, sy, pw, ph },
                                        pf::ControlState (0.5f), pf::Colorway{}, nullptr);
            g.setOpacity (1.0f);
            // Preview crosshair
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.4f));
            g.drawLine (sx + pw / 2, 0, sx + pw / 2, chassisH, 0.5f);
            g.drawLine (0, sy + ph / 2, chassisW, sy + ph / 2, 0.5f);
        }

        // ── Marquee selection ──
        if (isMarquee)
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.12f));
            g.fillRect (marqueeRect);
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.7f));
            float dashLen[] = { 5.0f, 3.0f };
            juce::Path marquee;
            marquee.addRectangle (marqueeRect);
            juce::PathStrokeType stroke (1.0f);
            stroke.createDashedStroke (marquee, marquee, dashLen, 2);
            g.strokePath (marquee, juce::PathStrokeType (1.0f));
        }

        g.restoreState();

        // ── HUD: zoom level + coordinates ──
        {
            juce::String hudText = juce::String ((int)(scale * 100)) + "%";
            if (!selectedIndices.empty() && selectedIndices.size() == 1)
            {
                auto* hw = &placedHardware[*selectedIndices.begin()];
                hudText += "    X:" + juce::String (hw->x, 1) + "  Y:" + juce::String (hw->y, 1)
                        + "  W:" + juce::String (hw->width, 1) + "  H:" + juce::String (hw->height, 1);
            }
            g.setColour (juce::Colour (0xCC0A0A14));
            g.fillRoundedRectangle (8.0f, getHeight() - 26.0f, g.getCurrentFont().getStringWidthFloat(hudText) + 60.0f, 20.0f, 4.0f);
            g.setColour (PedalForgeLookAndFeel::textSecondary);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (hudText, 16, getHeight() - 26, getWidth(), 20, juce::Justification::centredLeft);
        }
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
        float dw, dh; juce::Colour dbg;
        getActivePageDimensions (dw, dh, dbg);
        float cx = getWidth() / 2.0f - dw / 2.0f;
        float cy = getHeight() / 2.0f - dh / 2.0f;
        return { absP.x - cx, absP.y - cy };
    }

    juce::Point<float> getAbsoluteCanvasPosForEvent (const juce::MouseEvent& e) { return getAbsoluteCanvasPosForPoint(e.position); }
    juce::Point<float> getCanvasPosForEvent (const juce::MouseEvent& e) { return getChassisRelativeCanvasPosForPoint(e.position); }

    int hitTestHardware (juce::Point<float> p)
    {
        for (int i = (int) placedHardware.size() - 1; i >= 0; --i)
        {
            auto& hw = placedHardware[i];
            if (hw.isLocked) continue;
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
            if (hw.isLocked) continue;
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

            if (selectedIndices.size() == 1)
            {
                int movingIdx = *selectedIndices.begin();
                auto& hw = placedHardware[movingIdx];
                float targetX = dragStarts[0].startX + dx;
                float targetY = dragStarts[0].startY + dy;

                float snappedX = targetX;
                float snappedY = targetY;

                bool snapped = false;
                for (int i = 0; i < placedHardware.size(); ++i)
                {
                    if (i == movingIdx) continue;
                    const auto& other = placedHardware[i];
                    float snapDist = 2.0f; // 2mm snapping radius
                    
                    if (std::abs(targetX - other.x) < snapDist) { snappedX = other.x; snapped = true; }
                    else if (std::abs((targetX + hw.width/2) - (other.x + other.width/2)) < snapDist) { snappedX = other.x + other.width/2 - hw.width/2; snapped = true; }
                    else if (std::abs((targetX + hw.width) - (other.x + other.width)) < snapDist) { snappedX = other.x + other.width - hw.width; snapped = true; }

                    if (std::abs(targetY - other.y) < snapDist) { snappedY = other.y; snapped = true; }
                    else if (std::abs((targetY + hw.height/2) - (other.y + other.height/2)) < snapDist) { snappedY = other.y + other.height/2 - hw.height/2; snapped = true; }
                    else if (std::abs((targetY + hw.height) - (other.y + other.height)) < snapDist) { snappedY = other.y + other.height - hw.height; snapped = true; }
                }

                if (snapped)
                {
                    hw.x = snappedX;
                    hw.y = snappedY;
                }
                else
                {
                    hw.x = snapToGrid (targetX);
                    hw.y = snapToGrid (targetY);
                }
            }
            else
            {
                for (auto& start : dragStarts)
                {
                    placedHardware[start.idx].x = snapToGrid(start.startX + dx);
                    placedHardware[start.idx].y = snapToGrid(start.startY + dy);
                }
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
                if (hw.isLocked) continue;
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
            {
                if (!placedHardware[*it].isLocked)
                    placedHardware.erase (placedHardware.begin() + *it);
            }
            ++it;
        }
        setSelection ({});
        notifyHardwareChanged();
    }
    
    std::function<void()> onHardwareListChanged;
    void notifyHardwareChanged() { 
        if (onHardwareListChanged) onHardwareListChanged();
        repaint(); 
    }

    friend class PedalDesignerComponent;

    juce::TextButton btnSave   { "Save" };
    juce::TextButton btnExport { "Export..." };
    std::unique_ptr<juce::FileChooser> exportChooser;
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
    std::vector<PlacedHardware> placedHardware;   // the ACTIVE page's hardware

    // ── Overlay Pages ──
    std::vector<PageContext> overlayPages;         // index 0..N = overlay pages (pedal face is NOT stored here)
    int activePageIndex = -1;                      // -1 = pedal face, 0..N = overlayPages[i]
    std::vector<PlacedHardware>* pedalFaceBackupPtr = nullptr;  // set by PagesPanel so save works correctly
    std::function<void()> onPageChanged;

    /** Store the current placedHardware back to the correct page. */
    void stashActivePage()
    {
        if (activePageIndex >= 0 && activePageIndex < (int)overlayPages.size())
            overlayPages[activePageIndex].hardware = placedHardware;
    }

    /** Switch to editing a different page. -1 = pedal face. */
    void switchToPage (int pageIdx, std::vector<PlacedHardware>& pedalFaceHardware)
    {
        stashActivePage();

        // If currently on the pedal face, stash it
        if (activePageIndex < 0)
            pedalFaceHardware = placedHardware;

        activePageIndex = pageIdx;

        if (pageIdx < 0)
        {
            placedHardware = pedalFaceHardware;
        }
        else if (pageIdx < (int)overlayPages.size())
        {
            placedHardware = overlayPages[pageIdx].hardware;
        }

        setSelection ({});
        repaint();
        if (onPageChanged) onPageChanged();
    }

    /** Get the dimensions of the active page (or chassis). */
    void getActivePageDimensions (float& w, float& h, juce::Colour& bg) const
    {
        if (activePageIndex >= 0 && activePageIndex < (int)overlayPages.size())
        {
            w  = overlayPages[activePageIndex].width;
            h  = overlayPages[activePageIndex].height;
            bg = overlayPages[activePageIndex].bgColour;
        }
        else
        {
            w  = chassisW;
            h  = chassisH;
            bg = chassisColour;
        }
    }

    bool isEditingOverlayPage() const { return activePageIndex >= 0; }

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
        setupEditor (categoryEditor);
        setupEditor (tagsEditor);
        
        descEditor.setMultiLine (true);
        descEditor.setReturnKeyStartsNewLine (true);
        setupEditor (descEditor);

        // NOTE: control→parameter binding is no longer done here. The Pedal tab
        // is purely about a widget's appearance + behaviour; binding happens by
        // spawning the control's node and wiring it in the FX tab.

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
        setupButton (btnImageOff,   [this] { pickStateImage (0); });
        setupButton (btnImageOn,    [this] { pickStateImage (1); });
        setupButton (btnClearImage, [this] { clearImage(); });

        // Knob-specific editors
        setupEditor (rotationEditor);
        setupEditor (sensitivityEditor);

        setupCombo (overlayPageCombo);
    }

    // Default starting location for image pickers — the user's already-
    // imported Library/Images folder. Falls back to home if it doesn't
    // exist yet (first-run case).
    static juce::File imagePickerStartDir()
    {
        auto lib = pf::paths::getImagesDir();
        return lib.isDirectory() ? lib
                                 : juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    }

    void pickImage (bool isMain)
    {
        if (canvas == nullptr) return;

        fileChooser = std::make_unique<juce::FileChooser> (
            "Select an image file...", imagePickerStartDir(),
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
            hw->imageStates.clear();
        }
        else
        {
            canvas->chassisImage = "";
        }
        canvas->notifyHardwareChanged();
    }

    void pickStateImage (int stateIndex)
    {
        if (canvas == nullptr) return;
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select an image for state " + juce::String (stateIndex) + "...",
            imagePickerStartDir(),
            "*.png;*.jpg;*.jpeg");

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync (flags, [this, stateIndex] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (! file.existsAsFile()) return;
            auto* hw = canvas->getSelectedHardware();
            if (hw == nullptr) return;
            while (hw->imageStates.size() <= stateIndex) hw->imageStates.add ("");
            hw->imageStates.set (stateIndex, file.getFullPathName());
            canvas->notifyHardwareChanged();
        });
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

                bool isOverlayLauncher = (hwPtr->type == "overlay_launcher");
                overlayPageCombo.setVisible (isOverlayLauncher);
                if (isOverlayLauncher)
                    rebuildOverlayPageCombo (hwPtr);

                labelEditor.setVisible (true);
                wEditor.setVisible (true);
                hEditor.setVisible (true);
                deleteButton.setVisible (true);

                btnImageMain.setVisible (!isLabel);
                btnImageMain.setButtonText ("Set Image...");
                btnImageTrack.setVisible (showTrack);
                const bool isBinaryStateful = (hwPtr->type == "switch"
                                            || hwPtr->type == "footswitch"
                                            || hwPtr->type == "led");
                btnImageOff.setVisible (isBinaryStateful);
                btnImageOn .setVisible (isBinaryStateful);
                btnClearImage.setVisible (!isLabel);

                fontStyleCombo.setVisible (isLabel);
                fontFamilyCombo.setVisible (isLabel);
                colourCombo.setVisible (true);

                nameEditor.setVisible (false);
                authorEditor.setVisible (false);
                descEditor.setVisible (false);
                categoryEditor.setVisible (false);
                tagsEditor.setVisible (false);

                bool isKnob = (hwPtr->type == "knob");
                rotationEditor.setVisible (isKnob);
                sensitivityEditor.setVisible (isKnob);
                if (isKnob)
                {
                    rotationEditor.setText (juce::String (hwPtr->rotationRange, 0), juce::dontSendNotification);
                    sensitivityEditor.setText (juce::String (hwPtr->sensitivity, 0), juce::dontSendNotification);
                }
            }
        }
        else if (sel.empty())
        {
            // Chassis selected (no hardware selected)
            overlayPageCombo.setVisible (false);
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

            categoryEditor.setVisible (true);
            categoryEditor.setText (canvas->pedalCategory, juce::dontSendNotification);
            tagsEditor.setVisible (true);
            tagsEditor.setText (canvas->pedalTags.joinIntoString (", "), juce::dontSendNotification);

            deleteButton.setVisible (false);

            btnImageMain.setVisible (true);
            btnImageMain.setButtonText ("Set Chassis Image...");
            btnImageTrack.setVisible (false);
            btnImageOff.setVisible (false);
            btnImageOn.setVisible (false);
            btnClearImage.setVisible (true);

            fontStyleCombo.setVisible (false);
            fontFamilyCombo.setVisible (false);
            colourCombo.setVisible (false);
            rotationEditor.setVisible (false);
            sensitivityEditor.setVisible (false);
        }
        else
        {
            // Multiple items selected
            overlayPageCombo.setVisible (false);
            labelEditor.setVisible (false);
            wEditor.setVisible (false);
            hEditor.setVisible (false);
            btnImageMain.setVisible (false);
            btnImageTrack.setVisible (false);
            btnImageOff.setVisible (false);
            btnImageOn.setVisible (false);
            btnClearImage.setVisible (false);
            fontStyleCombo.setVisible (false);
            fontFamilyCombo.setVisible (false);
            colourCombo.setVisible (false);
            nameEditor.setVisible (false);
            authorEditor.setVisible (false);
            descEditor.setVisible (false);
            categoryEditor.setVisible (false);
            tagsEditor.setVisible (false);
            deleteButton.setVisible (true);
            rotationEditor.setVisible (false);
            sensitivityEditor.setVisible (false);
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

                bool isOverlayLauncher = (hw->type == "overlay_launcher");
                if (isOverlayLauncher)
                {
                    g.drawText ("TARGET PAGE", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
                    y = overlayPageCombo.getBottom() + 16;
                }
                else
                {
                    y = labelEditor.getBottom() + 16;
                }

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

                // Knob-specific labels
                if (hw->type == "knob")
                {
                    int ky = rotationEditor.getY() - 18;
                    g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
                    g.drawText ("KNOB BEHAVIOUR", m, ky, getWidth()-m*2, 16, juce::Justification::centredLeft);
                    g.setColour (PedalForgeLookAndFeel::textSecondary); g.setFont (juce::FontOptions (11.0f));
                    g.drawText (juce::CharPointer_UTF8 ("Arc \xc2\xb0"), m, rotationEditor.getY(), 32, 24, juce::Justification::centredLeft); // \xc2\xb0 = U+00B0 DEGREE; bare UTF-8 literal would mojibake as "Â°"
                    g.drawText ("Sens", m + 90, sensitivityEditor.getY(), 32, 24, juce::Justification::centredLeft);
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
            y = descEditor.getBottom() + 12;

            g.drawText ("CATEGORY", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
            y = categoryEditor.getBottom() + 12;

            g.drawText ("TAGS", m, y, getWidth()-m*2, 16, juce::Justification::centredLeft);
            g.setColour (PedalForgeLookAndFeel::textSecondary.withAlpha (0.5f)); g.setFont (juce::FontOptions (9.0f));
            g.drawText ("(comma separated)", m + 30, y, getWidth()-m*2-30, 16, juce::Justification::centredLeft);
            g.setColour (PedalForgeLookAndFeel::textMuted); g.setFont (juce::FontOptions (11.0f));
            y = tagsEditor.getBottom() + 16;

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
            descEditor.setBounds (m, y, getWidth()-m*2, 80);
            
            y = descEditor.getBottom() + 28;
            categoryEditor.setBounds (m, y, getWidth()-m*2, 28);

            y = categoryEditor.getBottom() + 28;
            tagsEditor.setBounds (m, y, getWidth()-m*2, 28);

            y = tagsEditor.getBottom() + 36;
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
            bool isOverlayLauncher = hw && hw->type == "overlay_launcher";
            if (isOverlayLauncher)
            {
                overlayPageCombo.setBounds (m, y, getWidth()-m*2, 28);
                y = overlayPageCombo.getBottom() + 36;
            }
            else
            {
                y = labelEditor.getBottom() + 36;
            }
            if (isLabel)
            {
                fontFamilyCombo.setBounds (m, y, getWidth()-m*2, 28); y += 36;
                fontStyleCombo.setBounds (m, y, getWidth()-m*2, 28); y += 36;
                colourCombo.setBounds (m, y, getWidth()-m*2, 28);
            }
            else
            {
                btnImageMain.setBounds (m, y, getWidth()-m*2, 24); y += 30;
                btnImageTrack.setBounds (m, y, getWidth()-m*2, 24);
                if (btnImageTrack.isVisible()) y += 30;
                if (btnImageOff.isVisible() || btnImageOn.isVisible())
                {
                    const int half = (getWidth() - m*2 - 4) / 2;
                    btnImageOff.setBounds (m,            y, half, 24);
                    btnImageOn .setBounds (m + half + 4, y, half, 24);
                    y += 30;
                }
                colourCombo.setBounds (m, y, getWidth()-m*2, 28); y += 36;
                btnClearImage.setBounds (m, y, getWidth()-m*2, 24); y += 32;

                // Knob-specific: Rotation Range + Sensitivity
                if (hw && hw->type == "knob")
                {
                    y += 4;
                    rotationEditor.setBounds (m + 34, y, 48, 24);
                    sensitivityEditor.setBounds (m + 124, y, 48, 24);
                }
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
            else if (&editor == &categoryEditor) { canvas->pedalCategory = categoryEditor.getText(); }
            else if (&editor == &tagsEditor)
            {
                juce::StringArray parsed;
                parsed.addTokens (tagsEditor.getText(), ",", "");
                for (auto& t : parsed) t = t.trim();
                parsed.removeEmptyStrings();
                canvas->pedalTags = parsed;
            }
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
                else if (&editor == &rotationEditor) { hw->rotationRange = juce::jlimit(10.0f, 360.0f, rotationEditor.getText().getFloatValue()); canvas->notifyHardwareChanged(); }
                else if (&editor == &sensitivityEditor) { hw->sensitivity = juce::jmax(20.0f, sensitivityEditor.getText().getFloatValue()); canvas->notifyHardwareChanged(); }
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
                if (box == &overlayPageCombo)
                {
                    int s = overlayPageCombo.getSelectedId();
                    if (s == 1) // "(none)"
                        hw->overlayPage = "";
                    else if (s == 2) // "Close"
                        hw->overlayPage = "Close";
                    else
                        hw->overlayPage = overlayPageCombo.getText();

                    canvas->notifyHardwareChanged();
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
    juce::TextEditor nameEditor, authorEditor, descEditor, categoryEditor, tagsEditor;
    juce::ComboBox fontStyleCombo, fontFamilyCombo, colourCombo, overlayPageCombo;
    juce::TextButton deleteButton { "Delete Component" };
    juce::TextButton btnImageMain { "Set Image..." };
    juce::TextButton btnImageTrack { "Set Track Image..." };
    juce::TextButton btnImageOff   { "Set OFF Image..." };
    juce::TextButton btnImageOn    { "Set ON Image..." };
    juce::TextButton btnClearImage { "Clear Images" };
    juce::TextEditor rotationEditor, sensitivityEditor;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void rebuildOverlayPageCombo (PlacedHardware* hw)
    {
        overlayPageCombo.clear (juce::dontSendNotification);
        overlayPageCombo.addItem ("(none)", 1);
        overlayPageCombo.addItem ("Close Overlay", 2);
        
        int selectId = 1;
        if (hw && hw->overlayPage == "Close") selectId = 2;

        int itemId = 3;
        if (canvas)
        {
            for (const auto& page : canvas->overlayPages)
            {
                overlayPageCombo.addItem (page.pageName, itemId);
                if (hw && hw->overlayPage == page.pageName)
                    selectId = itemId;
                itemId++;
            }
        }
        overlayPageCombo.setSelectedId (selectId, juce::dontSendNotification);
    }
};

//==============================================================================
class PedalDesignerComponent::LayersPanel : public juce::Component, public juce::ListBoxModel
{
public:
    ChassisCanvas* canvas = nullptr;
    juce::ListBox listBox;
    juce::TextButton btnUp {"Up"}, btnDown {"Down"}, btnLock {"Lock"};

    LayersPanel()
    {
        listBox.setModel (this);
        listBox.setMultipleSelectionEnabled (true);
        listBox.setColour (juce::ListBox::backgroundColourId, PedalForgeLookAndFeel::bgDark);
        addAndMakeVisible (listBox);

        auto setupBtn = [this](juce::TextButton& b, auto cb) { 
            b.onClick = cb; 
            b.setColour(juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
            b.setColour(juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textPrimary);
            addAndMakeVisible(b); 
        };
        setupBtn (btnUp, [this] { moveSelected(-1); });
        setupBtn (btnDown, [this] { moveSelected(1); });
        setupBtn (btnLock, [this] { toggleLock(); });
        btnUp.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb2")));
        btnDown.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbc")));
        btnLock.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92")));
        listBox.setRowHeight (28);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (PedalForgeLookAndFeel::bgDark);
        // Header
        g.setColour (PedalForgeLookAndFeel::bgMid);
        g.fillRect (0, 0, getWidth(), 24);
        g.setColour (PedalForgeLookAndFeel::gridLine);
        g.drawHorizontalLine (23, 0, (float)getWidth());
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText ("  LAYERS  (" + juce::String(getNumRows()) + ")", 0, 0, getWidth(), 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop (24); // header
        auto row = bounds.removeFromBottom(32).reduced(4, 2);
        int bw = row.getWidth() / 3;
        btnUp.setBounds(row.removeFromLeft(bw).reduced(2));
        btnDown.setBounds(row.removeFromLeft(bw).reduced(2));
        btnLock.setBounds(row.reduced(2));
        listBox.setBounds(bounds);
    }

    int getNumRows() override { return canvas ? (int)canvas->placedHardware.size() : 0; }
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int w, int h, bool rowIsSelected) override
    {
        if (!(canvas && rowNumber >= 0 && rowNumber < (int)canvas->placedHardware.size())) return;
        auto& hw = canvas->placedHardware[rowNumber];

        // Alternating row bg
        g.fillAll ((rowNumber & 1) ? PedalForgeLookAndFeel::bgDark : PedalForgeLookAndFeel::bgDark.brighter(0.03f));

        // Selection highlight
        if (rowIsSelected)
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.25f));
            g.fillRect (0, 0, w, h);
            g.setColour (PedalForgeLookAndFeel::accent);
            g.fillRect (0, 0, 3, h); // accent bar on left
        }

        // Type badge
        juce::String typeShort = hw.type.substring(0, 3).toUpperCase();
        juce::Colour badgeCol = PedalForgeLookAndFeel::accent;
        if (hw.type == "knob" || hw.type == "fader" || hw.type == "switch") badgeCol = juce::Colour (0xFF22C55E);
        else if (hw.type == "led" || hw.type == "rgb_led") badgeCol = juce::Colour (0xFFEAB308);
        else if (hw.type == "label" || hw.type == "graphic") badgeCol = juce::Colour (0xFF8B5CF6);
        else if (hw.type == "footswitch") badgeCol = juce::Colour (0xFFEF4444);

        g.setColour (badgeCol.withAlpha (0.2f));
        g.fillRoundedRectangle (8.0f, (h - 16) / 2.0f, 30.0f, 16.0f, 4.0f);
        g.setColour (badgeCol);
        g.setFont (juce::FontOptions (9.0f).withStyle ("Bold"));
        g.drawText (typeShort, 8, (h - 16) / 2, 30, 16, juce::Justification::centred);

        // Label text
        g.setColour (hw.isLocked ? PedalForgeLookAndFeel::textMuted : PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (12.0f));
        juce::String text = hw.label.isNotEmpty() ? hw.label : hw.type;
        g.drawText (text, 44, 0, w - 68, h, juce::Justification::centredLeft, true);

        // Lock icon on right
        if (hw.isLocked)
        {
            g.setColour (juce::Colour (0xFFFF9944));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92")), w - 24, 0, 20, h, juce::Justification::centred);
        }

        // Bottom separator
        g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.5f));
        g.drawHorizontalLine (h - 1, 8.0f, (float)(w - 8));
    }
    
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (!canvas) return;
        std::set<int> newSel;
        for (int i=0; i < listBox.getNumSelectedRows(); ++i)
            newSel.insert (listBox.getSelectedRow(i));
        
        isUpdatingSelection = true;
        canvas->setSelection (newSel);
        isUpdatingSelection = false;
    }

    bool isUpdatingSelection = false;

    void updateSelectionFromCanvas()
    {
        if (isUpdatingSelection || !canvas) return;
        listBox.deselectAllRows();
        for (int idx : canvas->getSelectedIndices())
            listBox.selectRow (idx, false, false);
    }

    void moveSelected(int delta)
    {
        if (!canvas || canvas->selectedIndices.empty()) return;
        std::vector<int> sel (canvas->selectedIndices.begin(), canvas->selectedIndices.end());
        if (delta < 0) std::sort(sel.begin(), sel.end()); 
        else std::sort(sel.rbegin(), sel.rend()); 

        for (int idx : sel)
        {
            int newIdx = idx + delta;
            if (newIdx >= 0 && newIdx < canvas->placedHardware.size())
            {
                std::swap (canvas->placedHardware[idx], canvas->placedHardware[newIdx]);
            }
        }
        
        std::set<int> newSel;
        for (int idx : sel)
        {
            int newIdx = juce::jlimit(0, (int)canvas->placedHardware.size() - 1, idx + delta);
            newSel.insert(newIdx);
        }
        canvas->selectedIndices = newSel;
        
        listBox.updateContent();
        updateSelectionFromCanvas();
        canvas->repaint();
    }

    void toggleLock()
    {
        if (!canvas) return;
        for (int idx : canvas->selectedIndices)
            canvas->placedHardware[idx].isLocked = !canvas->placedHardware[idx].isLocked;
        listBox.updateContent();
        canvas->repaint();
    }
};

//==============================================================================
class PedalDesignerComponent::PagesPanel : public juce::Component,
                                            public juce::ListBoxModel,
                                            public juce::TextEditor::Listener
{
public:
    ChassisCanvas* canvas = nullptr;
    std::vector<PlacedHardware> pedalFaceHardware;   // stash when switching away from pedal face

    juce::ListBox listBox;
    juce::TextButton btnAddPage { "+ New Page" };
    juce::TextButton btnDeletePage { "Delete" };

    // Page property editors
    juce::TextEditor nameEditor, widthEditor, heightEditor;
    juce::TextButton  btnBgColour { "" };

    std::function<void()> onPageSwitched;

    PagesPanel()
    {
        listBox.setModel (this);
        listBox.setMultipleSelectionEnabled (false);
        listBox.setColour (juce::ListBox::backgroundColourId, PedalForgeLookAndFeel::bgDark);
        listBox.setRowHeight (30);
        addAndMakeVisible (listBox);

        auto setupBtn = [this](juce::TextButton& b) {
            b.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
            b.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textPrimary);
            addAndMakeVisible (b);
        };
        setupBtn (btnAddPage);
        setupBtn (btnDeletePage);

        btnAddPage.onClick = [this] { addPage(); };
        btnDeletePage.onClick = [this] { deletePage(); };

        auto setupEditor = [this](juce::TextEditor& ed) {
            ed.setColour (juce::TextEditor::backgroundColourId, PedalForgeLookAndFeel::bgLight);
            ed.setColour (juce::TextEditor::textColourId, PedalForgeLookAndFeel::textPrimary);
            ed.setColour (juce::TextEditor::outlineColourId, PedalForgeLookAndFeel::gridLine);
            ed.setFont (juce::FontOptions (13.0f));
            ed.addListener (this);
            addChildComponent (ed);
        };
        setupEditor (nameEditor);
        setupEditor (widthEditor);
        setupEditor (heightEditor);

        btnBgColour.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF222222));
        btnBgColour.onClick = [this] { pickBgColour(); };
        addChildComponent (btnBgColour);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (PedalForgeLookAndFeel::bgDark);

        g.setColour (PedalForgeLookAndFeel::bgMid);
        g.fillRect (0, 0, getWidth(), 24);
        g.setColour (PedalForgeLookAndFeel::gridLine);
        g.drawHorizontalLine (23, 0, (float)getWidth());
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText ("  PAGES", 0, 0, getWidth(), 24, juce::Justification::centredLeft);

        // Show property section header when a page is selected
        if (canvas && canvas->activePageIndex >= 0)
        {
            int propY = getHeight() - 130;
            g.setColour (PedalForgeLookAndFeel::gridLine);
            g.drawHorizontalLine (propY, 0, (float)getWidth());
            g.setColour (PedalForgeLookAndFeel::textMuted);
            g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
            g.drawText ("  PAGE PROPERTIES", 0, propY + 2, getWidth(), 16, juce::Justification::centredLeft);

            int m = 10;
            int y = propY + 22;
            g.setColour (PedalForgeLookAndFeel::textSecondary);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("Name", m, y, 50, 20, juce::Justification::centredLeft);
            g.drawText ("Size", m, y + 28, 50, 20, juce::Justification::centredLeft);
            g.drawText ("BG Colour", m, y + 56, 60, 20, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop (24);

        auto buttonRow = bounds.removeFromTop (32).reduced (4, 4);
        btnAddPage.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (2, 0));
        btnDeletePage.setBounds (buttonRow.reduced (2, 0));

        // Page property editors at bottom (visible only when overlay page selected)
        bool showProps = (canvas && canvas->activePageIndex >= 0);
        int propH = showProps ? 130 : 0;
        auto propArea = bounds.removeFromBottom (propH);

        listBox.setBounds (bounds);

        if (showProps)
        {
            int m = 10;
            int y = propArea.getY() + 22;
            int edX = m + 55;
            int edW = propArea.getWidth() - edX - m;
            nameEditor.setBounds (edX, y, edW, 22);
            widthEditor.setBounds (edX, y + 28, edW / 2 - 2, 22);
            heightEditor.setBounds (edX + edW / 2 + 2, y + 28, edW / 2 - 2, 22);
            btnBgColour.setBounds (edX, y + 56, 30, 22);
        }

        nameEditor.setVisible (showProps);
        widthEditor.setVisible (showProps);
        heightEditor.setVisible (showProps);
        btnBgColour.setVisible (showProps);
    }

    // ListBox
    int getNumRows() override
    {
        return 1 + (canvas ? (int)canvas->overlayPages.size() : 0); // +1 for pedal face
    }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool rowIsSelected) override
    {
        g.fillAll ((row & 1) ? PedalForgeLookAndFeel::bgDark : PedalForgeLookAndFeel::bgDark.brighter (0.03f));

        if (rowIsSelected)
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.25f));
            g.fillRect (0, 0, w, h);
            g.setColour (PedalForgeLookAndFeel::accent);
            g.fillRect (0, 0, 3, h);
        }

        // Active indicator
        int pageIdx = row - 1; // -1 = pedal face
        bool isActive = (canvas && canvas->activePageIndex == pageIdx);
        g.setColour (isActive ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (isActive ? "\u25CF" : "\u25CB", 6, 0, 14, h, juce::Justification::centredLeft);

        juce::String label;
        if (row == 0)
            label = "Pedal Face (main)";
        else if (canvas && (row - 1) < (int)canvas->overlayPages.size())
            label = canvas->overlayPages[row - 1].pageName;

        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (label, 24, 0, w - 30, h, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        switchToPageRow (row);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        switchToPageRow (row);
    }

    void switchToPageRow (int row)
    {
        if (!canvas) return;
        int pageIdx = row - 1;
        canvas->switchToPage (pageIdx, pedalFaceHardware);
        refreshPageProperties();
        listBox.repaint();
        resized();
        repaint();
        if (onPageSwitched) onPageSwitched();
    }

    void addPage()
    {
        if (!canvas) return;
        PageContext page;
        page.pageName = "Page " + juce::String (canvas->overlayPages.size() + 1);
        page.width = 800.0f;
        page.height = 600.0f;
        page.bgColour = juce::Colour (0xFF222222);
        canvas->overlayPages.push_back (page);
        listBox.updateContent();
        repaint();

        // Switch to the new page
        switchToPageRow ((int)canvas->overlayPages.size()); // row = overlayPages.size() because row 0 = pedal face
    }

    void deletePage()
    {
        if (!canvas || canvas->activePageIndex < 0) return;
        int idx = canvas->activePageIndex;

        // Switch back to pedal face first
        canvas->switchToPage (-1, pedalFaceHardware);

        if (idx >= 0 && idx < (int)canvas->overlayPages.size())
            canvas->overlayPages.erase (canvas->overlayPages.begin() + idx);

        listBox.updateContent();
        refreshPageProperties();
        resized();
        repaint();
        if (onPageSwitched) onPageSwitched();
    }

    void refreshPageProperties()
    {
        if (!canvas || canvas->activePageIndex < 0 || canvas->activePageIndex >= (int)canvas->overlayPages.size())
        {
            nameEditor.setVisible (false);
            widthEditor.setVisible (false);
            heightEditor.setVisible (false);
            btnBgColour.setVisible (false);
            return;
        }
        auto& page = canvas->overlayPages[canvas->activePageIndex];
        nameEditor.setText (page.pageName, false);
        widthEditor.setText (juce::String ((int)page.width), false);
        heightEditor.setText (juce::String ((int)page.height), false);
        btnBgColour.setColour (juce::TextButton::buttonColourId, page.bgColour);
    }

    void textEditorReturnKeyPressed (juce::TextEditor& ed) override
    {
        textEditorFocusLost (ed);
    }

    void textEditorFocusLost (juce::TextEditor& ed) override
    {
        if (!canvas || canvas->activePageIndex < 0 || canvas->activePageIndex >= (int)canvas->overlayPages.size())
            return;
        auto& page = canvas->overlayPages[canvas->activePageIndex];

        if (&ed == &nameEditor)
        {
            page.pageName = nameEditor.getText().trim();
            if (page.pageName.isEmpty()) page.pageName = "Untitled";
            listBox.repaint();
        }
        else if (&ed == &widthEditor)
        {
            page.width = juce::jmax (100.0f, widthEditor.getText().getFloatValue());
            canvas->repaint();
        }
        else if (&ed == &heightEditor)
        {
            page.height = juce::jmax (100.0f, heightEditor.getText().getFloatValue());
            canvas->repaint();
        }
    }

    void pickBgColour()
    {
        if (!canvas || canvas->activePageIndex < 0) return;
        auto& page = canvas->overlayPages[canvas->activePageIndex];

        auto* picker = new juce::ColourSelector (
            juce::ColourSelector::showColourAtTop
            | juce::ColourSelector::showSliders
            | juce::ColourSelector::showColourspace);
        picker->setCurrentColour (page.bgColour);
        picker->setSize (240, 280);
        picker->addChangeListener (new BgColourListener (*this));
        juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (picker),
                                                btnBgColour.getScreenBounds(), nullptr);
    }

    struct BgColourListener : public juce::ChangeListener
    {
        PagesPanel& panel;
        BgColourListener (PagesPanel& p) : panel (p) {}
        void changeListenerCallback (juce::ChangeBroadcaster* source) override
        {
            if (auto* picker = dynamic_cast<juce::ColourSelector*> (source))
            {
                if (panel.canvas && panel.canvas->activePageIndex >= 0
                    && panel.canvas->activePageIndex < (int)panel.canvas->overlayPages.size())
                {
                    panel.canvas->overlayPages[panel.canvas->activePageIndex].bgColour = picker->getCurrentColour();
                    panel.btnBgColour.setColour (juce::TextButton::buttonColourId, picker->getCurrentColour());
                    panel.canvas->repaint();
                }
            }
        }
    };
};

 PedalDesignerComponent::PedalDesignerComponent()
{
    canvas = std::make_unique<ChassisCanvas>();    addAndMakeVisible (*canvas);
    properties = std::make_unique<PropertiesPanel>(); 
    layersPanel = std::make_unique<LayersPanel>(); 
    pagesPanel = std::make_unique<PagesPanel>();

    rightTabs = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::TabsAtTop);
    rightTabs->setTabBarDepth(30);
    rightTabs->addTab ("Properties", PedalForgeLookAndFeel::bgDark, properties.get(), false);
    rightTabs->addTab ("Layers", PedalForgeLookAndFeel::bgDark, layersPanel.get(), false);
    rightTabs->addTab ("Pages", PedalForgeLookAndFeel::bgDark, pagesPanel.get(), false);
    rightTabs->setOutline(0);
    addAndMakeVisible (*rightTabs);

    properties->setCanvas (canvas.get());
    layersPanel->canvas = canvas.get();
    pagesPanel->canvas = canvas.get();
    canvas->pedalFaceBackupPtr = &pagesPanel->pedalFaceHardware;

    canvas->onSelectionChanged = [this] () { 
        properties->showForIndices (); 
        layersPanel->updateSelectionFromCanvas();
    };
    canvas->onHardwareListChanged = [this] () {
        layersPanel->listBox.updateContent();
    };
    canvas->onPageChanged = [this] () {
        layersPanel->listBox.updateContent();
        properties->showForIndices();
    };
    pagesPanel->onPageSwitched = [this] () {
        layersPanel->listBox.updateContent();
        properties->showForIndices();
    };
    properties->onDeleteClicked = [this] { canvas->deleteSelected(); };

    // ── Toolbar: Colour swatch ──
    colourSwatchBtn.setColour (juce::TextButton::buttonColourId, canvas->chassisColour);
    colourSwatchBtn.setButtonText ("");
    colourSwatchBtn.onClick = [this] { showColourPicker(); };
    addAndMakeVisible (colourSwatchBtn);

    // ── Toolbar: Grid combo ──
    gridCombo.setColour (juce::ComboBox::backgroundColourId, PedalForgeLookAndFeel::bgLight);
    gridCombo.setColour (juce::ComboBox::textColourId, PedalForgeLookAndFeel::textPrimary);
    gridCombo.setColour (juce::ComboBox::outlineColourId, PedalForgeLookAndFeel::gridLine);
    gridCombo.addItem ("Off", 1);
    gridCombo.addItem ("1mm", 2);
    gridCombo.addItem ("2mm", 3);
    gridCombo.addItem ("5mm", 4);
    gridCombo.addItem ("10mm", 5);
    gridCombo.setSelectedId (2, juce::dontSendNotification);
    gridCombo.onChange = [this] {
        int s = gridCombo.getSelectedId();
        if (s == 1) canvas->gridSize = 0.1f;
        else if (s == 2) canvas->gridSize = 1.0f;
        else if (s == 3) canvas->gridSize = 2.0f;
        else if (s == 4) canvas->gridSize = 5.0f;
        else if (s == 5) canvas->gridSize = 10.0f;
        canvas->repaint();
    };
    addAndMakeVisible (gridCombo);

    // ── Toolbar: Zoom ──
    auto setupToolBtn = [this](juce::TextButton& btn) {
        btn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
        btn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textPrimary);
        addAndMakeVisible (btn);
    };
    setupToolBtn (btnZoomIn);
    setupToolBtn (btnZoomOut);
    setupToolBtn (btnFitView);
    btnZoomIn.onClick = [this]  { canvas->scale = juce::jmin (10.0f, canvas->scale * 1.25f); canvas->repaint(); };
    btnZoomOut.onClick = [this] { canvas->scale = juce::jmax (0.1f, canvas->scale / 1.25f); canvas->repaint(); };
    btnFitView.onClick = [this] {
        float sw = (float)(canvas->getWidth()) / (canvas->chassisW + 40.0f);
        float sh = (float)(canvas->getHeight()) / (canvas->chassisH + 40.0f);
        canvas->scale = juce::jmin (sw, sh);
        canvas->panX = 0; canvas->panY = 0;
        canvas->repaint();
    };

    // ── Toolbar: Align ──
    setupToolBtn (btnAlignLeft);
    setupToolBtn (btnAlignCenterH);
    setupToolBtn (btnAlignRight);
    setupToolBtn (btnDistributeH);
    setupToolBtn (btnDistributeV);
    btnAlignLeft.onClick    = [this] { alignSelected(0); };
    btnAlignCenterH.onClick = [this] { alignSelected(1); };
    btnAlignRight.onClick   = [this] { alignSelected(2); };
    btnDistributeH.onClick  = [this] { distributeSelected(true); };
    btnDistributeV.onClick  = [this] { distributeSelected(false); };

    // ── Notes ──
    notesOverlay.setNotes (designNotes);
    addChildComponent (notesOverlay);
    setupToolBtn (btnNotes);
    btnNotes.setClickingTogglesState (true);
    btnNotes.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFF59E0B)); // amber
    btnNotes.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
    btnNotes.setTooltip ("Toggle Notes");
    btnNotes.onClick = [this] {
        NotesOverlay::globallyVisible = btnNotes.getToggleState();
        bool show = NotesOverlay::globallyVisible;
        notesOverlay.setVisible (show);
        if (show && designNotes.empty())
            notesOverlay.addNote (120, 80);
    };

    properties->showForIndices();
}

PedalDesignerComponent::~PedalDesignerComponent() = default;

void PedalDesignerComponent::paint (juce::Graphics& g)
{
    auto toolbarArea = getLocalBounds().removeFromTop (36);
    // Toolbar gradient background
    g.setGradientFill (juce::ColourGradient (
        PedalForgeLookAndFeel::bgMid.darker (0.1f), 0, (float)toolbarArea.getY(),
        PedalForgeLookAndFeel::bgMid.darker (0.35f), 0, (float)toolbarArea.getBottom(), false));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float)getWidth());

    // Toolbar labels
    g.setColour (PedalForgeLookAndFeel::textMuted);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("COLOUR", colourSwatchBtn.getX() - 42, colourSwatchBtn.getY(), 40, 24, juce::Justification::centredRight);
    g.drawText ("GRID", gridCombo.getX() - 32, gridCombo.getY(), 30, 24, juce::Justification::centredRight);

    // Separators
    auto drawSep = [&](int x) {
        g.setColour (PedalForgeLookAndFeel::gridLine);
        g.drawVerticalLine (x, 8, 28);
    };
    drawSep (colourSwatchBtn.getRight() + 8);
    drawSep (gridCombo.getRight() + 8);
    drawSep (btnFitView.getRight() + 8);
}

void PedalDesignerComponent::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (36);
    int m = 8;
    int bw = 28, bh = 24;

    // Colour swatch
    toolbar.removeFromLeft (m);
    auto colourLabel = toolbar.removeFromLeft (46);
    colourSwatchBtn.setBounds (toolbar.removeFromLeft (bw).withSizeKeepingCentre (24, 24));
    toolbar.removeFromLeft (16); // separator gap

    // Grid combo
    toolbar.removeFromLeft (32); // label
    gridCombo.setBounds (toolbar.removeFromLeft (70).withSizeKeepingCentre (70, bh));
    toolbar.removeFromLeft (16);

    // Zoom buttons
    btnZoomOut.setBounds (toolbar.removeFromLeft (bw).withSizeKeepingCentre (bw, bh));
    btnZoomIn.setBounds (toolbar.removeFromLeft (bw).withSizeKeepingCentre (bw, bh));
    btnFitView.setBounds (toolbar.removeFromLeft (36).withSizeKeepingCentre (36, bh));
    toolbar.removeFromLeft (16);

    // Align buttons
    int abw = 36;
    btnAlignLeft.setBounds (toolbar.removeFromLeft (abw).withSizeKeepingCentre (abw, bh));
    btnAlignCenterH.setBounds (toolbar.removeFromLeft (abw).withSizeKeepingCentre (abw, bh));
    btnAlignRight.setBounds (toolbar.removeFromLeft (abw).withSizeKeepingCentre (abw, bh));
    toolbar.removeFromLeft (4);
    btnDistributeH.setBounds (toolbar.removeFromLeft (abw).withSizeKeepingCentre (abw, bh));
    btnDistributeV.setBounds (toolbar.removeFromLeft (abw).withSizeKeepingCentre (abw, bh));
    toolbar.removeFromLeft (12);
    btnNotes.setBounds (toolbar.removeFromLeft (50).withSizeKeepingCentre (50, bh));

    rightTabs->setBounds (area.removeFromRight (260));
    canvas->setBounds (area);
    notesOverlay.setBounds (canvas->getBounds());
}

void PedalDesignerComponent::showColourPicker()
{
    auto* picker = new juce::ColourSelector (
        juce::ColourSelector::showColourAtTop
        | juce::ColourSelector::showSliders
        | juce::ColourSelector::showColourspace);
    picker->setCurrentColour (canvas->chassisColour);
    picker->setSize (260, 300);
    picker->addChangeListener (this);
    juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (picker),
                                            colourSwatchBtn.getScreenBounds(),
                                            nullptr);
}

void PedalDesignerComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (auto* picker = dynamic_cast<juce::ColourSelector*> (source))
    {
        auto col = picker->getCurrentColour();
        canvas->chassisColour = col;
        canvas->chassisImage = ""; // clear custom image when colour changes
        colourSwatchBtn.setColour (juce::TextButton::buttonColourId, col);
        canvas->repaint();
    }
}

void PedalDesignerComponent::setEffectsGraph (DSPGraph* graph)
{
    effectsGraph = graph;
    if (properties)
        properties->effectsGraph = graph;
}

void PedalDesignerComponent::loadDesign (const PedalDesign& design)
{
    if (canvas)
    {
        // Always switch back to pedal face before loading
        if (pagesPanel)
            canvas->switchToPage (-1, pagesPanel->pedalFaceHardware);

        canvas->placedHardware.clear();
        canvas->overlayPages.clear();
        canvas->chassisW = design.chassisW;
        canvas->chassisH = design.chassisH;
        canvas->chassisColour = design.chassisColour;
        canvas->chassisImage = design.chassisImage;
        canvas->colorwaySeed = design.colorwaySeed;
        canvas->colorwayMode = design.colorwayMode;
        canvas->pedalUuid = design.uuid;
        canvas->pedalName = design.name;
        canvas->pedalAuthor = design.author;
        canvas->pedalDescription = design.description;
        canvas->pedalCategory = design.category;
        canvas->pedalTags = design.tags;
        canvas->cachedEffectsGraph = design.effectsGraph;

        // Find matching preset index
        for (int i = 0; i < canvas->numChassisPresets; ++i)
        {
            if (std::abs (canvas->chassisPresets[i].w - design.chassisW) < 1.0f &&
                std::abs (canvas->chassisPresets[i].h - design.chassisH) < 1.0f)
            {
                canvas->chassisPresetIndex = i;
                break;
            }
        }

        // Helper: convert controls + mappings → PlacedHardware
        auto convertControls = [&design](const std::vector<PedalDesign::Control>& controls)
        {
            std::vector<PlacedHardware> result;
            for (const auto& ctrl : controls)
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
                hw.imageStates = ctrl.imageStates;
                hw.positions = ctrl.positions;
                hw.customColour = ctrl.customColour;
                hw.stretchImage = ctrl.stretchImage;
                hw.fontFamily = ctrl.fontFamily;
                hw.fontStyle = ctrl.fontStyle;
                hw.rotationRange = ctrl.rotationRange;
                hw.sensitivity = ctrl.sensitivity;
                hw.overlayPage = ctrl.overlayPage;

                for (const auto& m : design.mappings)
                    if (m.controlID == ctrl.controlID)
                        hw.parameterID = m.nodeParam;

                result.push_back (hw);
            }
            return result;
        };

        // Load pedal face controls
        canvas->placedHardware = convertControls (design.controls);

        // Load overlay pages
        for (const auto& cp : design.canvasPages)
        {
            PageContext page;
            page.pageName = cp.pageName;
            page.width = cp.width;
            page.height = cp.height;
            page.bgColour = cp.backgroundColour;
            page.hardware = convertControls (cp.controls);
            canvas->overlayPages.push_back (page);
        }

        canvas->repaint();
    }
    if (properties)
        properties->showForIndices ();
    if (pagesPanel)
        pagesPanel->listBox.updateContent();

    designNotes = design.designNotes;
    notesOverlay.setNotes (designNotes);
    notesOverlay.setVisible (!designNotes.empty());
    btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
}

void PedalDesignerComponent::visibilityChanged()
{
    if (isVisible())
    {
        btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
        notesOverlay.setVisible (!designNotes.empty());
    }
}

void PedalDesignerComponent::clearDesign()
{
    if (canvas)
    {
        if (pagesPanel)
            canvas->switchToPage (-1, pagesPanel->pedalFaceHardware);

        canvas->placedHardware.clear();
        canvas->overlayPages.clear();
        canvas->chassisW = 60.0f;
        canvas->chassisH = 112.0f;
        canvas->chassisColour = juce::Colour (0xFF8A8A94);
        canvas->chassisPresetIndex = 1;
        canvas->chassisColourIndex = 0;
        canvas->repaint();
    }
    if (properties)
        properties->showForIndices ();
    if (pagesPanel)
        pagesPanel->listBox.updateContent();

    designNotes.clear();
    notesOverlay.setVisible (false);
}

PedalDesign PedalDesignerComponent::getDesign() const
{
    if (canvas)
    {
        PedalDesign design = canvas->buildPedalDesign (pagesPanel ? &pagesPanel->pedalFaceHardware : nullptr);
        design.designNotes = designNotes;
        return design;
    }
    return PedalDesign();
}

void PedalDesignerComponent::alignSelected (int mode)
{
    if (!canvas || canvas->selectedIndices.size() < 2) return;
    auto& hw = canvas->placedHardware;
    auto& sel = canvas->selectedIndices;

    if (mode == 0) // Align Left
    {
        float minX = std::numeric_limits<float>::max();
        for (int i : sel) minX = juce::jmin (minX, hw[i].x);
        for (int i : sel) hw[i].x = minX;
    }
    else if (mode == 1) // Align Center H
    {
        float sumCX = 0;
        for (int i : sel) sumCX += hw[i].x + hw[i].width / 2.0f;
        float avgCX = sumCX / (float) sel.size();
        for (int i : sel) hw[i].x = avgCX - hw[i].width / 2.0f;
    }
    else if (mode == 2) // Align Right
    {
        float maxR = -std::numeric_limits<float>::max();
        for (int i : sel) maxR = juce::jmax (maxR, hw[i].x + hw[i].width);
        for (int i : sel) hw[i].x = maxR - hw[i].width;
    }
    canvas->repaint();
}

void PedalDesignerComponent::distributeSelected (bool horizontal)
{
    if (!canvas || canvas->selectedIndices.size() < 3) return;
    auto& hw = canvas->placedHardware;

    std::vector<int> sorted (canvas->selectedIndices.begin(), canvas->selectedIndices.end());
    if (horizontal)
        std::sort (sorted.begin(), sorted.end(), [&](int a, int b) { return hw[a].x < hw[b].x; });
    else
        std::sort (sorted.begin(), sorted.end(), [&](int a, int b) { return hw[a].y < hw[b].y; });

    int n = (int) sorted.size();
    if (horizontal)
    {
        float first = hw[sorted.front()].x;
        float last = hw[sorted.back()].x;
        float step = (last - first) / (float)(n - 1);
        for (int i = 0; i < n; ++i)
            hw[sorted[i]].x = first + step * (float)i;
    }
    else
    {
        float first = hw[sorted.front()].y;
        float last = hw[sorted.back()].y;
        float step = (last - first) / (float)(n - 1);
        for (int i = 0; i < n; ++i)
            hw[sorted[i]].y = first + step * (float)i;
    }
    canvas->repaint();
}

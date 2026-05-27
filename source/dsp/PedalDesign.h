#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "../engine/StickyNoteData.h"

//==============================================================================
// Pedal kind — distinguishes audio pedals from companion (no-audio) ones.
// Companion pedals (displays, foot controllers, etc.) are not inserted into
// the JUCE AudioProcessorGraph; they only participate in the UI, mapping,
// library and scripting surfaces.
enum class PedalKind
{
    Audio = 0,     // default — processes audio
    Midi  = 1,     // pure MIDI router / transformer, no audio I/O
    Companion = 2  // no signal flow at all (Turing display, HDMI display, foot controller)
};

//==============================================================================
/**
 * The complete, unified definition of a pedal.
 *
 * A PedalDesign bundles three things:
 *   1. pedalForge  — The visual layout (chassis + controls)
 *   2. effectsForge — The DSP graph (nodes + connections)
 *   3. mappings    — Links between UI controls and DSP parameters
 *
 * Both factory pedals and user-created pedals use this format.
 * Serialized to JSON for saving/loading.
 */
struct PedalDesign
{
    //==========================================================================
    // Metadata
    juce::String uuid;            // stable identity — survives renames; used by
                                  // the inventory to deduplicate factory vs.
                                  // user designs and to round-trip imports.
                                  // Generated on first construction / load.
    juce::String name = "My Pedal";
    juce::String author = "User";
    juce::String description = "";
    juce::String category = "Custom";
    juce::StringArray tags;       // free-form tags, e.g. {"drive", "beginner", "tutorial"}
    int version = 1;
    bool isFactory = false;       // true = read-only, can't overwrite
    PedalKind kind = PedalKind::Audio; // see enum above

    PedalDesign() : uuid (juce::Uuid().toString()) {}

    //==========================================================================
    // Pedal Forge — Visual Layout
    struct Control
    {
        juce::String type;        // "knob", "switch", "footswitch", "led", "fader"
        float x = 0, y = 0;
        float width = 40, height = 40;
        juce::String label;
        juce::String controlID;   // unique ID within this design
        float defaultValue = 0.5f;

        // For library_loader
        juce::String libraryCategory;

        // For overlay_launcher
        juce::String overlayPage;

        // Knob visual/interaction properties
        float rotationRange = 270.0f;   // visual arc in degrees (e.g. 270 = 7-o'clock to 5-o'clock)
        float sensitivity = 200.0f;     // pixels of vertical drag for a full 0→1 sweep

        // Custom UI properties
        juce::String imageMain;
        juce::String imageTrack;
        juce::Colour customColour { juce::Colours::red };
        bool stretchImage = true;
        
        // Font properties (mostly used by 'label' and 'text_screen')
        juce::String fontFamily = "Sans"; // "Sans", "Serif", "Monospace", or specific font name
        int fontStyle = 1;                // 0=Plain, 1=Bold, 2=Italic, 3=BoldItalic
        float fontSize = 0.0f;            // 0 = auto scale
        int numLines = 1;                 // for multi-line displays
        bool isLocked = false;            // if true, ignores canvas interaction
    };

    float chassisW = 200.0f;
    float chassisH = 340.0f;
    juce::Colour chassisColour { 0xFF8A8A94 };  // Silver default
    juce::String chassisImage;                  // Custom background image
    std::vector<Control> controls;

    //==========================================================================
    // Canvas Overlays — Secondary full-screen or large pop-up UI panels
    struct CanvasPage
    {
        juce::String pageName;
        float width = 800.0f;
        float height = 600.0f;
        juce::Colour backgroundColour { 0xFF222222 };
        std::vector<Control> controls;
    };
    std::vector<CanvasPage> canvasPages;

    //==========================================================================
    // Routing Ports — MIDI and Expression ports visible in the Routing Tab.
    // These are cross-pedal connections that live outside the AudioProcessorGraph.
    struct RoutingPort
    {
        enum class Kind { MidiIn, MidiOut, ExpressionIn, ExpressionOut };
        Kind kind = Kind::MidiIn;
        juce::String id;     // unique within this design, e.g. "midi_in", "expr_in_1"
        juce::String label;  // displayed in routing tab, e.g. "MIDI In", "Expr In 1"
    };
    std::vector<RoutingPort> routingPorts;

    //==========================================================================
    // Effects Forge — DSP Graph (stored as raw JSON var)
    // This is the serialized DSPGraph output from DSPGraph::toJSON()
    juce::var effectsGraph;

    //==========================================================================
    // Parameter Mappings — Links UI controls to DSP parameters
    struct Mapping
    {
        juce::String controlID;   // references a Control.controlID
        juce::String nodeParam;   // "nodeID.paramID" in the DSP graph
    };

    std::vector<Mapping> mappings;

    //==========================================================================
    // Sticky Notes
    std::vector<StickyNote> designNotes; // Notes on the Pedal Builder tab
    std::vector<StickyNote> fxNotes;     // Notes on the Code/DSP tab

    //==========================================================================
    // User-authored scripts attached to this pedal design.
    // Mode mirrors ScriptingTabComponent::ScriptMode (1=UI, 2=DSP, 3=GraphBuilder, 4=Board).
    // Scripts live on the design (not the instance) so they travel with the asset
    // when the pedal is exported, shared, or instantiated on another board.
    // Mode 4 (Board) is engine-scoped and lives on AudioGraphEngine::engineScripts
    // rather than per-pedal — it appears here only so the same struct can be reused.
    struct Script
    {
        juce::String name;
        int mode = 1;            // 1=UI, 2=DSP, 3=GraphBuilder, 4=Board
        juce::String source;
    };
    std::vector<Script> scripts;

    //==========================================================================
    // Serialization

    juce::var toJSON() const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("uuid",        uuid);
        root->setProperty ("name",        name);
        root->setProperty ("author",      author);
        root->setProperty ("description", description);
        root->setProperty ("category",    category);

        // Tags — stored as a JSON array of strings
        juce::Array<juce::var> tagArr;
        for (const auto& t : tags)
            tagArr.add (t);
        root->setProperty ("tags", tagArr);

        root->setProperty ("version",     version);
        root->setProperty ("isFactory",   isFactory);
        root->setProperty ("kind",        (int) kind);
        root->setProperty ("chassisW",  chassisW);
        root->setProperty ("chassisH",  chassisH);
        root->setProperty ("chassisColour", (juce::int64) chassisColour.getARGB());
        root->setProperty ("chassisImage", chassisImage);

        // Controls
        juce::Array<juce::var> ctrlArr;
        for (const auto& c : controls)
        {
            auto* co = new juce::DynamicObject();
            co->setProperty ("type",         c.type);
            co->setProperty ("x",            c.x);
            co->setProperty ("y",            c.y);
            co->setProperty ("width",        c.width);
            co->setProperty ("height",       c.height);
            co->setProperty ("label",        c.label);
            co->setProperty ("controlID",    c.controlID);
            co->setProperty ("defaultValue", c.defaultValue);
            co->setProperty ("imageMain",    c.imageMain);
            co->setProperty ("imageTrack",   c.imageTrack);
            co->setProperty ("customColour", (juce::int64) c.customColour.getARGB());
            if (c.stretchImage != true)
                co->setProperty ("stretchImage", c.stretchImage);
            if (c.fontFamily != "Sans")
                co->setProperty ("fontFamily", c.fontFamily);
            if (c.fontStyle != 1)
                co->setProperty ("fontStyle", c.fontStyle);
            if (std::abs (c.rotationRange - 270.0f) > 0.01f)
                co->setProperty ("rotationRange", c.rotationRange);
            if (std::abs (c.sensitivity - 200.0f) > 0.01f)
                co->setProperty ("sensitivity", c.sensitivity);
            co->setProperty ("isLocked", c.isLocked);
            if (c.overlayPage.isNotEmpty())
                co->setProperty ("overlayPage", c.overlayPage);

            ctrlArr.add (juce::var (co));
        }
        root->setProperty ("controls", ctrlArr);

        // Effects graph (already a var)
        root->setProperty ("effectsGraph", effectsGraph);

        // Mappings
        juce::Array<juce::var> mapArr;
        for (const auto& m : mappings)
        {
            auto* mo = new juce::DynamicObject();
            mo->setProperty ("controlID", m.controlID);
            mo->setProperty ("nodeParam", m.nodeParam);
            mapArr.add (juce::var (mo));
        }
        root->setProperty ("mappings", mapArr);

        root->setProperty ("designNotes", StickyNoteData::toJSON (designNotes));
        root->setProperty ("fxNotes", StickyNoteData::toJSON (fxNotes));

        // Routing ports
        juce::Array<juce::var> rpArr;
        for (const auto& rp : routingPorts)
        {
            auto* rpo = new juce::DynamicObject();
            rpo->setProperty ("kind",  (int) rp.kind);
            rpo->setProperty ("id",    rp.id);
            rpo->setProperty ("label", rp.label);
            rpArr.add (juce::var (rpo));
        }
        root->setProperty ("routingPorts", rpArr);

        // Canvas overlay pages
        juce::Array<juce::var> cpArr;
        for (const auto& cp : canvasPages)
        {
            auto* cpo = new juce::DynamicObject();
            cpo->setProperty ("pageName", cp.pageName);
            cpo->setProperty ("width",    cp.width);
            cpo->setProperty ("height",   cp.height);
            cpo->setProperty ("backgroundColour", (juce::int64) cp.backgroundColour.getARGB());

            juce::Array<juce::var> pageCtrlArr;
            for (const auto& c : cp.controls)
            {
                auto* co = new juce::DynamicObject();
                co->setProperty ("type",         c.type);
                co->setProperty ("x",            c.x);
                co->setProperty ("y",            c.y);
                co->setProperty ("width",        c.width);
                co->setProperty ("height",       c.height);
                co->setProperty ("label",        c.label);
                co->setProperty ("controlID",    c.controlID);
                co->setProperty ("defaultValue", c.defaultValue);
                co->setProperty ("imageMain",    c.imageMain);
                co->setProperty ("imageTrack",   c.imageTrack);
                co->setProperty ("customColour", (juce::int64) c.customColour.getARGB());
                if (c.stretchImage != true)
                    co->setProperty ("stretchImage", c.stretchImage);
                if (c.fontFamily != "Sans")
                    co->setProperty ("fontFamily", c.fontFamily);
                if (c.fontStyle != 1)
                    co->setProperty ("fontStyle", c.fontStyle);
                if (std::abs (c.rotationRange - 270.0f) > 0.01f)
                    co->setProperty ("rotationRange", c.rotationRange);
                if (std::abs (c.sensitivity - 200.0f) > 0.01f)
                    co->setProperty ("sensitivity", c.sensitivity);
                co->setProperty ("isLocked", c.isLocked);
                if (c.overlayPage.isNotEmpty())
                    co->setProperty ("overlayPage", c.overlayPage);
                pageCtrlArr.add (juce::var (co));
            }
            cpo->setProperty ("controls", pageCtrlArr);
            cpArr.add (juce::var (cpo));
        }
        root->setProperty ("canvasPages", cpArr);

        // Scripts
        juce::Array<juce::var> scriptArr;
        for (const auto& s : scripts)
        {
            auto* so = new juce::DynamicObject();
            so->setProperty ("name",   s.name);
            so->setProperty ("mode",   s.mode);
            so->setProperty ("source", s.source);
            scriptArr.add (juce::var (so));
        }
        root->setProperty ("scripts", scriptArr);

        return juce::var (root);
    }

    static PedalDesign fromJSON (const juce::var& json)
    {
        PedalDesign design;
        if (auto* root = json.getDynamicObject())
        {
            // Older designs predate the UUID field — generate one on first load.
            // The migration persists on the next save (saveToFile preserves uuid).
            auto persisted = root->getProperty ("uuid").toString();
            if (persisted.isNotEmpty()) design.uuid = persisted;
            // else: keep the freshly-generated default from the constructor.

            design.name        = root->getProperty ("name").toString();
            design.author      = root->getProperty ("author").toString();
            if (root->hasProperty("description")) design.description = root->getProperty ("description").toString();
            design.category    = root->getProperty ("category").toString();

            // Tags
            if (auto* tagArr = root->getProperty ("tags").getArray())
            {
                for (const auto& tv : *tagArr)
                    design.tags.add (tv.toString());
            }

            design.version     = (int) root->getProperty ("version");
            design.isFactory   = (bool) root->getProperty ("isFactory");
            if (root->hasProperty ("kind"))
                design.kind = (PedalKind) (int) root->getProperty ("kind");
            design.chassisW    = (float)(double) root->getProperty ("chassisW");
            design.chassisH    = (float)(double) root->getProperty ("chassisH");
            if (root->hasProperty ("chassisColour"))
                design.chassisColour = juce::Colour ((juce::uint32)(juce::int64) root->getProperty ("chassisColour"));
            design.chassisImage = root->getProperty ("chassisImage").toString();

            // Controls
            if (auto* arr = root->getProperty ("controls").getArray())
            {
                for (const auto& cv : *arr)
                {
                    if (auto* co = cv.getDynamicObject())
                    {
                        Control c;
                        c.type         = co->getProperty ("type").toString();
                        c.x            = (float)(double) co->getProperty ("x");
                        c.y            = (float)(double) co->getProperty ("y");
                        c.width        = (float)(double) co->getProperty ("width");
                        c.height       = (float)(double) co->getProperty ("height");
                        c.label        = co->getProperty ("label").toString();
                        c.controlID    = co->getProperty ("controlID").toString();
                        c.defaultValue = (float)(double) co->getProperty ("defaultValue");
                        if (co->hasProperty("imageMain"))    c.imageMain    = co->getProperty ("imageMain").toString();
                        if (co->hasProperty("imageTrack"))   c.imageTrack   = co->getProperty ("imageTrack").toString();
                        if (co->hasProperty("customColour")) c.customColour = juce::Colour ((juce::uint32)(juce::int64) co->getProperty ("customColour"));
                        if (co->hasProperty("stretchImage")) c.stretchImage = (bool) co->getProperty ("stretchImage");
                        if (co->hasProperty("fontFamily"))   c.fontFamily   = co->getProperty ("fontFamily").toString();
                        if (co->hasProperty("fontStyle"))    c.fontStyle    = (int) co->getProperty ("fontStyle");
                        if (co->hasProperty("rotationRange")) c.rotationRange = (float)(double) co->getProperty ("rotationRange");
                        if (co->hasProperty("sensitivity"))   c.sensitivity   = (float)(double) co->getProperty ("sensitivity");
                        if (co->hasProperty("isLocked"))      c.isLocked      = (bool) co->getProperty ("isLocked");
                        if (co->hasProperty("overlayPage"))   c.overlayPage   = co->getProperty ("overlayPage").toString();
                        design.controls.push_back (c);
                    }
                }
            }

            // Effects graph
            design.effectsGraph = root->getProperty ("effectsGraph");

            // Mappings
            if (auto* arr = root->getProperty ("mappings").getArray())
            {
                for (const auto& mv : *arr)
                {
                    if (auto* mo = mv.getDynamicObject())
                    {
                        Mapping m;
                        m.controlID = mo->getProperty ("controlID").toString();
                        m.nodeParam = mo->getProperty ("nodeParam").toString();
                        design.mappings.push_back (m);
                    }
                }
            }

            if (root->hasProperty ("designNotes"))
                design.designNotes = StickyNoteData::fromJSON (root->getProperty ("designNotes"));
            if (root->hasProperty ("fxNotes"))
                design.fxNotes = StickyNoteData::fromJSON (root->getProperty ("fxNotes"));

            // Routing ports
            if (auto* arr = root->getProperty ("routingPorts").getArray())
            {
                for (const auto& rv : *arr)
                {
                    if (auto* ro = rv.getDynamicObject())
                    {
                        RoutingPort rp;
                        rp.kind  = (RoutingPort::Kind)(int) ro->getProperty ("kind");
                        rp.id    = ro->getProperty ("id").toString();
                        rp.label = ro->getProperty ("label").toString();
                        design.routingPorts.push_back (rp);
                    }
                }
            }

            // Canvas overlay pages
            if (auto* arr = root->getProperty ("canvasPages").getArray())
            {
                for (const auto& cpv : *arr)
                {
                    if (auto* cpo = cpv.getDynamicObject())
                    {
                        CanvasPage cp;
                        cp.pageName = cpo->getProperty ("pageName").toString();
                        cp.width    = (float)(double) cpo->getProperty ("width");
                        cp.height   = (float)(double) cpo->getProperty ("height");
                        if (cpo->hasProperty ("backgroundColour"))
                            cp.backgroundColour = juce::Colour ((juce::uint32)(juce::int64) cpo->getProperty ("backgroundColour"));

                        if (auto* ctrlArr = cpo->getProperty ("controls").getArray())
                        {
                            for (const auto& cv : *ctrlArr)
                            {
                                if (auto* co = cv.getDynamicObject())
                                {
                                    Control c;
                                    c.type         = co->getProperty ("type").toString();
                                    c.x            = (float)(double) co->getProperty ("x");
                                    c.y            = (float)(double) co->getProperty ("y");
                                    c.width        = (float)(double) co->getProperty ("width");
                                    c.height       = (float)(double) co->getProperty ("height");
                                    c.label        = co->getProperty ("label").toString();
                                    c.controlID    = co->getProperty ("controlID").toString();
                                    c.defaultValue = (float)(double) co->getProperty ("defaultValue");
                                    if (co->hasProperty("imageMain"))    c.imageMain    = co->getProperty ("imageMain").toString();
                                    if (co->hasProperty("imageTrack"))   c.imageTrack   = co->getProperty ("imageTrack").toString();
                                    if (co->hasProperty("customColour")) c.customColour = juce::Colour ((juce::uint32)(juce::int64) co->getProperty ("customColour"));
                                    if (co->hasProperty("stretchImage")) c.stretchImage = (bool) co->getProperty ("stretchImage");
                                    if (co->hasProperty("fontFamily"))   c.fontFamily   = co->getProperty ("fontFamily").toString();
                                    if (co->hasProperty("fontStyle"))    c.fontStyle    = (int) co->getProperty ("fontStyle");
                                    if (co->hasProperty("rotationRange")) c.rotationRange = (float)(double) co->getProperty ("rotationRange");
                                    if (co->hasProperty("sensitivity"))   c.sensitivity   = (float)(double) co->getProperty ("sensitivity");
                                    if (co->hasProperty("isLocked"))      c.isLocked      = (bool) co->getProperty ("isLocked");
                                    if (co->hasProperty("overlayPage"))   c.overlayPage   = co->getProperty ("overlayPage").toString();
                                    cp.controls.push_back (c);
                                }
                            }
                        }
                        design.canvasPages.push_back (cp);
                    }
                }
            }

            // Scripts
            if (auto* arr = root->getProperty ("scripts").getArray())
            {
                for (const auto& sv : *arr)
                {
                    if (auto* so = sv.getDynamicObject())
                    {
                        Script s;
                        s.name   = so->getProperty ("name").toString();
                        s.mode   = (int) so->getProperty ("mode");
                        s.source = so->getProperty ("source").toString();
                        if (s.mode < 1 || s.mode > 4) s.mode = 1;
                        design.scripts.push_back (s);
                    }
                }
            }
        }
        return design;
    }

    /** Load from a .json file. */
    static PedalDesign loadFromFile (const juce::File& file)
    {
        auto json = juce::JSON::parse (file.loadFileAsString());
        return fromJSON (json);
    }

    /** Save to a .json file. */
    bool saveToFile (const juce::File& file) const
    {
        return file.replaceWithText (juce::JSON::toString (toJSON()));
    }
};

// Helper function to match visual mappings to actual DSP parameters, bridging any off-by-one offsets.
inline bool matchMappingParam (const juce::String& mappingParam, const juce::String& actualParam)
{
    if (mappingParam == actualParam) return true;
    
    int mapUnderscore = mappingParam.indexOfChar ('_');
    int actUnderscore = actualParam.indexOfChar ('_');
    if (mapUnderscore > 0 && actUnderscore > 0)
    {
        juce::String mapSuffix = mappingParam.substring (mapUnderscore);
        juce::String actSuffix = actualParam.substring (actUnderscore);
        if (mapSuffix == actSuffix)
        {
            int mapNodeID = mappingParam.substring (0, mapUnderscore).getIntValue();
            int actNodeID = actualParam.substring (0, actUnderscore).getIntValue();
            int diff = mapNodeID - actNodeID;
            if (diff >= -1 && diff <= 1)
                return true;
        }
    }
    return false;
}

/**
 * After loading a file into a DSP node, call this to update any text_screen
 * displays that are mapped to the filepath parameter.
 *
 * Handles the "controlID:lineIndex" mapping convention, e.g.
 *   mapping { "nam_display:1", "4_filepath" }
 * will update line 1 of the "nam_display" text_screen with the filename.
 */
inline void updateDisplayForFilePath (const PedalDesign& design,
                                      std::map<juce::String, juce::String>& controlTexts,
                                      int targetNodeID,
                                      const juce::String& filePath)
{
    juce::String filepathParam = juce::String (targetNodeID) + "_filepath";
    juce::String displayName = juce::File (filePath).getFileNameWithoutExtension();

    for (const auto& mapping : design.mappings)
    {
        if (mapping.nodeParam != filepathParam)
            continue;

        // Check if this mapping's controlID has a ":lineIndex" suffix
        juce::String baseControlID = mapping.controlID;
        int lineIndex = -1;

        int colonPos = mapping.controlID.lastIndexOfChar (':');
        if (colonPos > 0)
        {
            juce::String suffix = mapping.controlID.substring (colonPos + 1);
            if (suffix.containsOnly ("0123456789"))
            {
                baseControlID = mapping.controlID.substring (0, colonPos);
                lineIndex = suffix.getIntValue();
            }
        }

        // Only update text_screen controls, not file_loader/library_loader
        bool isDisplay = false;
        for (const auto& ctrl : design.controls)
        {
            if (ctrl.controlID == baseControlID
                && (ctrl.type == "text_screen" || ctrl.type == "console"))
            {
                isDisplay = true;

                if (lineIndex >= 0)
                {
                    // Update a specific line of the display
                    juce::String currentText = ctrl.label;
                    auto it = controlTexts.find (baseControlID);
                    if (it != controlTexts.end() && it->second.isNotEmpty())
                        currentText = it->second;

                    juce::StringArray lines;
                    lines.addLines (currentText);

                    // Pad if needed
                    while (lines.size() <= lineIndex)
                        lines.add ("");

                    lines.set (lineIndex, displayName);
                    controlTexts[baseControlID] = lines.joinIntoString ("\n");
                }
                else
                {
                    // Replace entire display text
                    controlTexts[baseControlID] = displayName;
                }
                break;
            }
        }
    }
}
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

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
    juce::String name = "My Pedal";
    juce::String author = "User";
    juce::String description = "";
    juce::String category = "Custom";
    int version = 1;
    bool isFactory = false;       // true = read-only, can't overwrite

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

        // Knob visual/interaction properties
        float rotationRange = 270.0f;   // visual arc in degrees (e.g. 270 = 7-o'clock to 5-o'clock)
        float sensitivity = 200.0f;     // pixels of vertical drag for a full 0→1 sweep

        // Custom UI properties
        juce::String imageMain;
        juce::String imageTrack;
        juce::Colour customColour { juce::Colours::red };
        bool stretchImage = true;
        
        // Font properties (mostly used by 'label')
        juce::String fontFamily = "Sans"; // "Sans", "Serif", "Monospace", or specific font name
        int fontStyle = 1;                // 0=Plain, 1=Bold, 2=Italic, 3=BoldItalic
    };

    float chassisW = 200.0f;
    float chassisH = 340.0f;
    juce::Colour chassisColour { 0xFF8A8A94 };  // Silver default
    juce::String chassisImage;                  // Custom background image
    std::vector<Control> controls;

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
    // Serialization

    juce::var toJSON() const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("name",        name);
        root->setProperty ("author",      author);
        root->setProperty ("description", description);
        root->setProperty ("category",    category);
        root->setProperty ("version",     version);
        root->setProperty ("isFactory",   isFactory);
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

        return juce::var (root);
    }

    static PedalDesign fromJSON (const juce::var& json)
    {
        PedalDesign design;
        if (auto* root = json.getDynamicObject())
        {
            design.name        = root->getProperty ("name").toString();
            design.author      = root->getProperty ("author").toString();
            if (root->hasProperty("description")) design.description = root->getProperty ("description").toString();
            design.category    = root->getProperty ("category").toString();
            design.version     = (int) root->getProperty ("version");
            design.isFactory   = (bool) root->getProperty ("isFactory");
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

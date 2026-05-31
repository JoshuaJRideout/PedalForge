#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include "../pedals/PedalRegistry.h"   // PedalInfo, getFactoryPedals()
#include "../dsp/PedalDesign.h"
#include "../dsp/NodeCatalog.h"
#include "../util/AppPaths.h"

//==============================================================================
// Shared inventory model. The Q-menu overlay and the docked InventoryPanel (and
// later the radial quick-menu) all draw their items from here, so there is ONE
// source of truth for "what can be placed" — pedals, hardware parts, DSP nodes.
//==============================================================================
namespace pf::inv
{
    /** Which workspace the inventory is feeding. Determines which top-level
        categories are shown (the item DB itself is context-agnostic). */
    enum class Context { Board, Route, Forge, FX };

    /** One placeable thing in the inventory. */
    struct Item
    {
        juce::String id;           // unique identifier (e.g. "knob", a pedal uuid)
        juce::String displayName;  // what shows in the grid
        juce::String category;     // sub-category: "Controls", "Lights", …
        juce::String mainCategory; // "Pedals" | "Parts" | "Nodes" | "Effects"
        juce::String description;  // shown in the preview
        juce::StringArray tags;    // free-form tags for search
        bool isFactory = true;

        // For pedal items
        PedalInfo pedalInfo;
        std::shared_ptr<PedalDesign> pedalDesign;

        // For hardware / node items
        juce::String hardwareType; // "knob", "switch", a node type, …
    };

    /** Build the full, context-agnostic item database (factory + user pedals,
        hardware parts, DSP nodes). Filtering by context/category happens in the UI. */
    inline std::vector<Item> buildItems()
    {
        std::vector<Item> all;

        // ── Pedals: factory ────────────────────────────────────────────────
        for (auto& info : getFactoryPedals())
        {
            Item item;
            item.id = info.factoryID();
            item.displayName = info.name;
            item.category = info.category;
            item.mainCategory = "Pedals";
            item.description = info.category + " pedal with " + juce::String (info.numKnobs) + " knob(s).";
            item.isFactory = true;
            item.pedalInfo = info;
            if (info.designFactory)
            {
                item.pedalDesign = info.designFactory();
                item.tags = item.pedalDesign->tags;
            }
            all.push_back (std::move (item));
        }

        // ── Pedals: user-designed ──────────────────────────────────────────
        auto designsDir = pf::paths::getDesignsDir();
        if (designsDir.isDirectory())
        {
            for (const auto& file : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
            {
                auto design = std::make_shared<PedalDesign> (PedalDesign::loadFromFile (file));
                if (design->name.isEmpty()) continue;

                Item item;
                item.id = design->uuid;
                item.displayName = design->name;
                item.category = design->category.isNotEmpty() ? design->category : juce::String ("Custom");
                item.mainCategory = "Pedals";
                item.tags = design->tags;
                item.description = design->tags.size() > 0
                    ? "Custom pedal. Tags: " + design->tags.joinIntoString (", ")
                    : juce::String ("Custom user-designed pedal.");
                item.isFactory = false;
                item.pedalDesign = design;

                item.pedalInfo.name = design->name;
                item.pedalInfo.category = item.category;
                item.pedalInfo.gridW = 1;
                item.pedalInfo.gridH = 2;
                item.pedalInfo.numKnobs = 0;
                for (const auto& c : design->controls)
                    if (c.type == "knob") item.pedalInfo.numKnobs++;
                item.pedalInfo.colour = design->chassisColour;

                all.push_back (std::move (item));
            }
        }

        // ── Parts (hardware) ───────────────────────────────────────────────
        struct PartDef { const char* type; const char* name; const char* category; const char* desc; };
        static const PartDef parts[] = {
            { "knob",        "Knob",        "Controls",    "Rotary potentiometer control. Maps to a continuous parameter (0-1)." },
            { "switch",      "Switch",      "Controls",    "Toggle switch. Maps to a binary on/off parameter." },
            { "selector",    "Selector",    "Controls",    "Rotary multi-position selector. Pairs with a Selector Node; defaults to 4 positions." },
            { "fader",       "Fader",       "Controls",    "Linear slider control. Maps to a continuous parameter (0-1)." },
            { "xypad",       "XY Pad",      "Controls",    "Two-axis touch surface. Pairs with an XY Pad Node; outputs X and Y (0-1 each)." },
            { "joystick",    "Joystick",    "Controls",    "Self-centering two-axis stick. Pairs with an XY Pad Node; centre = 0.5,0.5." },
            { "footswitch",  "Stomp",       "Controls",    "3PDT footswitch for bypass/engage control." },
            { "led",         "LED",         "Lights",      "Single-color LED indicator." },
            { "rgb_led",     "RGB LED",     "Lights",      "Full-color RGB LED indicator." },
            { "indicator",   "Indicator",   "Lights",      "Status indicator light." },
            { "7seg",        "7-Seg",       "Screens",     "7-segment numeric display." },
            { "display",     "Display",     "Screens",     "Small graphical display panel." },
            { "text_screen", "Text",        "Screens",     "Text-based screen for status messages." },
            { "console",     "Console",     "Screens",     "Debug console / text output." },
            { "file_loader", "File Loader", "Controls",    "Button that opens a file browser to load files." },
            { "plugin_browser", "Plugin Browser", "Controls", "Button that opens a popup menu to select installed VST/AU plugins." },
            { "overlay_launcher", "Overlay Switch", "Controls", "Button that opens a visual overlay page or closes it." },
            { "label",       "Label",       "Decoration",  "Custom text label for the pedal face." },
            { "graphic",     "Graphic",     "Decoration",  "Custom image layer (supports transparency)." },
            { "vu_meter",    "VU Meter",    "Instruments", "Analog-style VU level meter." },
            { "oscilloscope","Scope",       "Instruments", "Mini oscilloscope waveform display." },
            { "pixel_display","Pixel Grid", "Screens",     "Pixel matrix display. Maps to a control signal driving each pixel." },
            { "library_loader","Library",   "Controls",    "Button that opens the Library overlay to select a NAM/IR/Image asset." },
        };
        for (auto& p : parts)
        {
            Item item;
            item.id = p.type;
            item.displayName = p.name;
            item.category = p.category;
            item.mainCategory = "Parts";
            item.description = p.desc;
            item.isFactory = true;
            item.hardwareType = p.type;
            all.push_back (std::move (item));
        }

        // ── Nodes (DSP blocks) — single source: NodeCatalog ────────────────
        for (const auto& e : NodeCatalog::getEntries())
        {
            if (! e.inInventory) continue;

            auto segs = juce::StringArray::fromTokens (e.menuPath, "/", {});
            segs.removeEmptyStrings();
            if (segs.isEmpty()) continue;

            juce::String mainCat = segs[0];
            segs.remove (0);
            juce::String subCat = segs.joinIntoString (" / ");

            Item item;
            item.id           = e.type;
            item.displayName  = e.displayName;
            item.category     = subCat.isNotEmpty() ? subCat : juce::String ("General");
            item.mainCategory = mainCat;
            item.description  = e.description;
            item.isFactory    = true;
            item.hardwareType = e.type;
            all.push_back (std::move (item));
        }

        return all;
    }

    /** The top-level categories visible in a given workspace. */
    inline juce::StringArray mainCategoriesForContext (Context ctx)
    {
        switch (ctx)
        {
            case Context::Board:
            case Context::Route:  return { "Pedals" };
            case Context::Forge:  return { "Parts" };
            case Context::FX:     return { "Nodes", "Effects" };
        }
        return {};
    }

    /** The drag-and-drop descriptor string a canvas parses on drop. (rx,ry) is
        the grab point as a 0-1 fraction of the dragged cell. */
    inline juce::String dragDescriptor (const Item& item, float rx, float ry)
    {
        const juce::String pos = ":" + juce::String (rx) + ":" + juce::String (ry);
        if (item.mainCategory == "Pedals") return "pedal:" + item.pedalInfo.name + pos;
        if (item.mainCategory == "Parts")  return "hardware:" + item.hardwareType + pos;
        if (item.mainCategory == "Nodes" || item.mainCategory == "Effects")
            return "node:" + item.hardwareType + pos;
        return {};
    }
}

#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

struct StickyNote
{
    juce::String text;
    juce::Rectangle<int> bounds { 100, 100, 200, 120 };
    juce::Colour colour { 0xFFFFEB3B }; // Kept for file compatibility, not used for drawing
};

class StickyNoteData
{
public:
    static juce::var toJSON (const std::vector<StickyNote>& notes)
    {
        juce::Array<juce::var> arr;
        for (const auto& note : notes)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("text", note.text);
            obj->setProperty ("x", note.bounds.getX());
            obj->setProperty ("y", note.bounds.getY());
            obj->setProperty ("w", note.bounds.getWidth());
            obj->setProperty ("h", note.bounds.getHeight());
            obj->setProperty ("colour", (juce::int64) note.colour.getARGB());
            arr.add (juce::var (obj));
        }
        return arr;
    }

    static std::vector<StickyNote> fromJSON (const juce::var& json)
    {
        std::vector<StickyNote> result;
        if (auto* arr = json.getArray())
        {
            for (auto& item : *arr)
            {
                if (auto* obj = item.getDynamicObject())
                {
                    StickyNote n;
                    n.text = obj->getProperty("text").toString();
                    n.bounds = { (int) obj->getProperty("x"), (int) obj->getProperty("y"),
                                 (int) obj->getProperty("w"), (int) obj->getProperty("h") };
                    if (obj->hasProperty("colour"))
                        n.colour = juce::Colour ((juce::uint32)(juce::int64) obj->getProperty ("colour"));
                    result.push_back (n);
                }
            }
        }
        return result;
    }
};

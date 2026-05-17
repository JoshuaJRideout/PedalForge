#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LibraryComponent.h"

class LibraryOverlay : public juce::Component
{
public:
    LibraryOverlay()
    {
        addAndMakeVisible (libraryComponent);
        libraryComponent.setSidebarVisible (false);
        
        // When an asset is selected, we want to close the overlay
        // The parent will set its own callback on `onAssetSelected`
        libraryComponent.onAssetSelected = [this] (const juce::File& f)
        {
            if (onAssetSelected)
                onAssetSelected(f);
            hide();
        };
        
        setVisible(false);
    }

    void showForCategory(const juce::String& category)
    {
        libraryComponent.selectCategory(category);
        libraryComponent.refreshAssets();
        
        setVisible(true);
        grabKeyboardFocus();
    }

    void hide()
    {
        setVisible(false);
    }

    void paint (juce::Graphics& g) override
    {
        // Darkened backdrop
        g.fillAll (juce::Colours::black.withAlpha (0.7f));
    }

    void resized() override
    {
        // Center the library component, maybe 80% width/height
        auto r = getLocalBounds().reduced(getWidth() / 10, getHeight() / 10);
        libraryComponent.setBounds(r);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // If clicking outside the libraryComponent, hide the overlay
        if (!libraryComponent.getBounds().contains(e.getPosition()))
            hide();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            hide();
            return true;
        }
        return false;
    }

    std::function<void(const juce::File&)> onAssetSelected;

private:
    LibraryComponent libraryComponent;
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/PluginHostNode.h"
#include "PluginScannerWindow.h"

class PluginBrowserWindow : public juce::DocumentWindow,
                            public juce::TableListBoxModel
{
public:
    PluginBrowserWindow(std::function<void(const juce::PluginDescription&)> onPluginSelected) 
        : juce::DocumentWindow("Select Plugin", juce::Colours::darkgrey, juce::DocumentWindow::closeButton),
          onSelect(onPluginSelected)
    {
        setUsingNativeTitleBar(true);
        
        refreshList();
        
        table.setModel(this);
        table.setRowHeight(24);
        table.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        
        auto& header = table.getHeader();
        header.addColumn("Type", 1, 60, 40, 100, juce::TableHeaderComponent::defaultFlags);
        header.addColumn("Manufacturer", 2, 160, 100, 400, juce::TableHeaderComponent::defaultFlags);
        header.addColumn("Plugin Name", 3, 250, 150, 600, juce::TableHeaderComponent::defaultFlags);
        
        header.setSortColumnId(3, true); // Default sort by Name
        sortPlugins(3, true);
        
        // Add a scan button
        scanButton.setButtonText("Scan for Plugins...");
        scanButton.onClick = [this]() {
            scanPlugins();
        };

        // Add a Load button
        loadButton.setButtonText("Load Selected");
        loadButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
        loadButton.onClick = [this]() {
            int row = table.getSelectedRow();
            if (row >= 0 && row < plugins.size() && onSelect)
            {
                onSelect(plugins[row]);
                closeButtonPressed();
            }
        };

        contentPanel.addAndMakeVisible(table);
        contentPanel.addAndMakeVisible(scanButton);
        contentPanel.addAndMakeVisible(loadButton);
        
        contentPanel.onResized = [this]() {
            auto b = contentPanel.getLocalBounds();
            auto bottom = b.removeFromBottom(40).reduced(4);
            scanButton.setBounds(bottom.removeFromLeft(bottom.getWidth() / 2).reduced(2));
            loadButton.setBounds(bottom.reduced(2));
            table.setBounds(b);
        };

        setContentNonOwned(&contentPanel, true);
        centreWithSize(600, 600);
        setVisible(true);
    }
    
    void scanPlugins()
    {
        new PluginScannerWindow();
    }

    void refreshList()
    {
        plugins = PluginHostNode::getKnownPluginList().getTypes();
    }
    
    void sortPlugins(int columnId, bool forwards)
    {
        std::sort(plugins.begin(), plugins.end(), [columnId, forwards](const juce::PluginDescription& a, const juce::PluginDescription& b) {
            juce::String s1, s2;
            if (columnId == 1) { s1 = a.pluginFormatName; s2 = b.pluginFormatName; }
            else if (columnId == 2) { s1 = a.manufacturerName; s2 = b.manufacturerName; }
            else if (columnId == 3) { s1 = a.name; s2 = b.name; }
            
            int res = s1.compareIgnoreCase(s2);
            if (res == 0) res = a.name.compareIgnoreCase(b.name);
            return forwards ? (res < 0) : (res > 0);
        });
        table.updateContent();
    }
    
    void sortOrderChanged (int newSortColumnId, bool isForwards) override
    {
        sortPlugins(newSortColumnId, isForwards);
    }

    int getNumRows() override { return plugins.size(); }
    
    void paintRowBackground (juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll (juce::Colours::orange.withAlpha(0.3f));
        else if (rowNumber % 2)
            g.fillAll (juce::Colours::black.withAlpha(0.2f));
    }

    void paintCell (juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < plugins.size())
        {
            g.setColour (juce::Colours::white);
            g.setFont (14.0f);
            
            const auto& p = plugins.getReference(rowNumber);
            juce::String text;
            
            if (columnId == 1) {
                juce::String fmt = p.pluginFormatName;
                if (fmt == "AudioUnit") text = "AU";
                else if (fmt == "VST3") text = "VST3";
                else text = fmt;
            }
            else if (columnId == 2) {
                text = p.manufacturerName;
            }
            else if (columnId == 3) {
                text = p.name;
            }
            
            g.drawText (text, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
        }
    }
    
    void cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent& e) override
    {
        if (rowNumber >= 0 && rowNumber < plugins.size() && onSelect)
        {
            onSelect(plugins[rowNumber]);
            closeButtonPressed();
        }
    }

    void closeButtonPressed() override
    {
        delete this;
    }

private:
    std::function<void(const juce::PluginDescription&)> onSelect;
    juce::Array<juce::PluginDescription> plugins;
    
    struct ContentPanel : public juce::Component {
        std::function<void()> onResized;
        void resized() override { if (onResized) onResized(); }
    } contentPanel;
    
    juce::TableListBox table;
    juce::TextButton scanButton;
    juce::TextButton loadButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginBrowserWindow)
};

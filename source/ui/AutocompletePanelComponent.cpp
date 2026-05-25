#include "AutocompletePanelComponent.h"
#include "../dsp/NodeCatalog.h"

CompletionDatabase::CompletionDatabase()
{
}

void CompletionDatabase::loadUIDatabase()
{
    items.clear();
    items.push_back({"fillAll(colour)", "Function", "Fills the entire background with a hex color.", "scripting/ui-scripts"});
    items.push_back({"setColour(colour)", "Function", "Sets the current drawing color.", "scripting/ui-scripts"});
    items.push_back({"fillRect(x, y, w, h)", "Function", "Draws a filled rectangle.", "scripting/ui-scripts"});
    items.push_back({"drawRect(x, y, w, h, thickness)", "Function", "Draws a rectangle outline.", "scripting/ui-scripts"});
    items.push_back({"fillRoundedRect(x, y, w, h, radius)", "Function", "Draws a filled rounded rectangle.", "scripting/ui-scripts"});
    items.push_back({"drawText(text, x, y, w, h)", "Function", "Draws text within the specified bounds.", "scripting/ui-scripts"});
    items.push_back({"setFont(fontName, height, style)", "Function", "Sets the current font.", "scripting/ui-scripts"});
    items.push_back({"w", "Variable", "Width of the canvas.", "scripting/ui-scripts"});
    items.push_back({"h", "Variable", "Height of the canvas.", "scripting/ui-scripts"});
    items.push_back({"mouse_x", "Variable", "Current mouse X position.", "scripting/ui-scripts"});
    items.push_back({"mouse_y", "Variable", "Current mouse Y position.", "scripting/ui-scripts"});
    items.push_back({"mouse_down", "Variable", "1 if mouse button is held, 0 otherwise.", "scripting/ui-scripts"});
    items.push_back({"mouse_click", "Variable", "1 on the frame the mouse was clicked.", "scripting/ui-scripts"});
    items.push_back({"time", "Variable", "Current time in seconds.", "scripting/ui-scripts"});
}

void CompletionDatabase::loadDSPDatabase()
{
    items.clear();
    items.push_back({"in1", "Variable", "Input signal 1.", "scripting/dsp-expressions"});
    items.push_back({"in2", "Variable", "Input signal 2.", "scripting/dsp-expressions"});
    items.push_back({"out1", "Variable", "Output signal 1.", "scripting/dsp-expressions"});
    items.push_back({"out2", "Variable", "Output signal 2.", "scripting/dsp-expressions"});
    items.push_back({"sin(x)", "Function", "Sine function.", "scripting/dsp-expressions"});
    items.push_back({"cos(x)", "Function", "Cosine function.", "scripting/dsp-expressions"});
    items.push_back({"tan(x)", "Function", "Tangent function.", "scripting/dsp-expressions"});
    items.push_back({"abs(x)", "Function", "Absolute value.", "scripting/dsp-expressions"});
    items.push_back({"min(a, b)", "Function", "Minimum of two values.", "scripting/dsp-expressions"});
    items.push_back({"max(a, b)", "Function", "Maximum of two values.", "scripting/dsp-expressions"});
    items.push_back({"pow(base, exp)", "Function", "Power function.", "scripting/dsp-expressions"});
    items.push_back({"clip(x, min, max)", "Function", "Clips value between min and max.", "scripting/dsp-expressions"});
}

void CompletionDatabase::loadGraphBuilderDatabase()
{
    items.clear();
    items.push_back ({"addNode(\"type\")",                              "Function", "Adds a new node to the graph.",         "scripting/graph-builder"});
    items.push_back ({"connect(srcNode, srcPort, dstNode, dstPort)",    "Function", "Connects two nodes.",                    "scripting/graph-builder"});
    items.push_back ({"setParam(node, \"paramName\", value)",           "Function", "Sets a parameter on a node.",            "scripting/graph-builder"});

    // Every node type the graph knows about — driven by the same catalog the
    // right-click menu and the inventory use. Surfaced as quoted strings since
    // that's how they appear inside addNode(...) calls.
    for (const auto& e : NodeCatalog::getEntries())
    {
        items.push_back ({ "\"" + e.type + "\"",
                           "Node",
                           e.description,
                           "dsp-nodes/index" });
    }
}

std::vector<CompletionItem> CompletionDatabase::getCompletions(const juce::String& prefix)
{
    std::vector<CompletionItem> results;
    juce::String p = prefix.toLowerCase();
    
    for (const auto& item : items)
    {
        if (p.isEmpty() || item.text.toLowerCase().contains(p))
            results.push_back(item);
    }
    return results;
}

//==============================================================================
AutocompletePanelComponent::AutocompletePanelComponent()
{
    searchBox.setTextToShowWhenEmpty("Search API...", juce::Colours::grey);
    searchBox.addListener(this);
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF161B22));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2D2D3D));
    addAndMakeVisible(searchBox);
    
    listBox.setModel(this);
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF0D1117));
    listBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xFF2D2D3D));
    addAndMakeVisible(listBox);
    
    titleLabel.setFont(juce::FontOptions("Sans", 14.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFE2E8F0));
    addAndMakeVisible(titleLabel);
    
    typeLabel.setFont(juce::FontOptions("Sans", 12.0f, juce::Font::italic));
    typeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF9CA3AF));
    addAndMakeVisible(typeLabel);
    
    descriptionBox.setMultiLine(true);
    descriptionBox.setReadOnly(true);
    descriptionBox.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    descriptionBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    descriptionBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFD1D5DB));
    addAndMakeVisible(descriptionBox);
    
    wikiButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1F2937));
    wikiButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    wikiButton.addListener(this);
    addAndMakeVisible(wikiButton);
    
    setDatabaseMode(1); // Default to UI
}

AutocompletePanelComponent::~AutocompletePanelComponent()
{
}

void AutocompletePanelComponent::setDatabaseMode(int mode)
{
    if (mode == 1) database.loadUIDatabase();
    else if (mode == 2) database.loadDSPDatabase();
    else if (mode == 3) database.loadGraphBuilderDatabase();
    
    updateFilter("");
}

void AutocompletePanelComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF161B22));
    
    // Draw separator line for detail pane
    auto bounds = getLocalBounds();
    int detailH = 150;
    g.setColour(juce::Colour(0xFF2D2D3D));
    g.drawHorizontalLine(bounds.getHeight() - detailH, 0.0f, (float)bounds.getWidth());
}

void AutocompletePanelComponent::resized()
{
    auto bounds = getLocalBounds();
    
    searchBox.setBounds(bounds.removeFromTop(30).reduced(4));
    
    int detailH = 150;
    auto detailBounds = bounds.removeFromBottom(detailH).reduced(8);
    
    listBox.setBounds(bounds);
    
    titleLabel.setBounds(detailBounds.removeFromTop(24));
    
    auto row2 = detailBounds.removeFromTop(24);
    typeLabel.setBounds(row2.removeFromLeft(120));
    wikiButton.setBounds(row2.removeFromRight(80).reduced(0, 2));
    
    detailBounds.removeFromTop(4); // spacing
    descriptionBox.setBounds(detailBounds);
}

int AutocompletePanelComponent::getNumRows()
{
    return (int)filteredItems.size();
}

void AutocompletePanelComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredItems.size()) return;
    
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xFF2D2D3D));
    
    const auto& item = filteredItems[(size_t)rowNumber];
    
    g.setColour(juce::Colour(0xFFE2E8F0));
    g.setFont(juce::FontOptions("Menlo", 12.0f, juce::Font::plain));
    g.drawText(item.text, 8, 0, width - 16, height, juce::Justification::centredLeft, true);
    
    // Type badge (optional, right aligned)
    g.setColour(juce::Colour(0xFF9CA3AF));
    g.setFont(juce::FontOptions("Sans", 10.0f, juce::Font::italic));
    g.drawText(item.type, 8, 0, width - 16, height, juce::Justification::centredRight, true);
}

void AutocompletePanelComponent::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    selectedIndex = row;
    updateDetailPane();
}

void AutocompletePanelComponent::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int)filteredItems.size())
    {
        if (onInsertCompletion)
            onInsertCompletion(filteredItems[(size_t)row].text);
    }
}

void AutocompletePanelComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &searchBox)
    {
        updateFilter(searchBox.getText());
    }
}

void AutocompletePanelComponent::buttonClicked(juce::Button* b)
{
    if (b == &wikiButton)
    {
        if (selectedIndex >= 0 && selectedIndex < (int)filteredItems.size())
        {
            if (onOpenWiki)
                onOpenWiki(filteredItems[(size_t)selectedIndex].wikiLink);
        }
    }
}

void AutocompletePanelComponent::updateFilter(const juce::String& text)
{
    filteredItems = database.getCompletions(text);
    listBox.updateContent();
    
    if (filteredItems.empty())
    {
        selectedIndex = -1;
    }
    else
    {
        selectedIndex = 0;
        listBox.selectRow(0, true, true);
    }
    updateDetailPane();
}

void AutocompletePanelComponent::highlightCompletion(const juce::String& word)
{
    if (word.isEmpty()) return;
    
    juce::String lowerWord = word.toLowerCase();
    for (int i = 0; i < (int)filteredItems.size(); ++i)
    {
        juce::String itemText = filteredItems[(size_t)i].text.toLowerCase();
        int parenIndex = itemText.indexOfChar('(');
        juce::String cleanText = (parenIndex != -1) ? itemText.substring(0, parenIndex) : itemText;
        
        if (cleanText == lowerWord || cleanText.startsWith(lowerWord))
        {
            selectedIndex = i;
            listBox.selectRow(i, true, false);
            listBox.scrollToEnsureRowIsOnscreen(i);
            updateDetailPane();
            break;
        }
    }
}

void AutocompletePanelComponent::updateDetailPane()
{
    if (selectedIndex >= 0 && selectedIndex < (int)filteredItems.size())
    {
        const auto& item = filteredItems[(size_t)selectedIndex];
        titleLabel.setText(item.text, juce::dontSendNotification);
        typeLabel.setText(item.type, juce::dontSendNotification);
        descriptionBox.setText(item.description, false);
        wikiButton.setEnabled(true);
    }
    else
    {
        titleLabel.setText("", juce::dontSendNotification);
        typeLabel.setText("", juce::dontSendNotification);
        descriptionBox.setText("", false);
        wikiButton.setEnabled(false);
    }
}

//==============================================================================
InlineAutocompletePopup::InlineAutocompletePopup(CompletionDatabase& db)
    : database(db)
{
    setOpaque(true);
    listBox.setModel(this);
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF161B22));
    listBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xFF2D2D3D));
    addAndMakeVisible(listBox);
}

InlineAutocompletePopup::~InlineAutocompletePopup()
{
}

void InlineAutocompletePopup::updateFilter(const juce::String& prefix)
{
    currentPrefix = prefix;
    filteredItems = database.getCompletions(prefix);
    
    if (filteredItems.empty())
    {
        setVisible(false);
        selectedIndex = -1;
    }
    else
    {
        listBox.updateContent();
        selectedIndex = 0;
        listBox.selectRow(0, true, true);
        
        int h = juce::jmin(200, (int)filteredItems.size() * listBox.getRowHeight() + 2);
        setSize(250, h);
    }
}

void InlineAutocompletePopup::moveSelection(int delta)
{
    if (filteredItems.empty()) return;
    
    selectedIndex += delta;
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= (int)filteredItems.size()) selectedIndex = (int)filteredItems.size() - 1;
    
    listBox.selectRow(selectedIndex, true, true);
}

bool InlineAutocompletePopup::handleKeyPress(const juce::KeyPress& key)
{
    if (!isVisible() || filteredItems.empty())
        return false;

    if (key == juce::KeyPress::downKey)
    {
        moveSelection(1);
        return true;
    }
    else if (key == juce::KeyPress::upKey)
    {
        moveSelection(-1);
        return true;
    }
    else if (key == juce::KeyPress::returnKey || key == juce::KeyPress::tabKey)
    {
        if (selectedIndex >= 0 && selectedIndex < (int)filteredItems.size())
        {
            if (onInsertCompletion)
                onInsertCompletion(filteredItems[(size_t)selectedIndex].text);
        }
        setVisible(false);
        return true;
    }
    else if (key == juce::KeyPress::escapeKey)
    {
        setVisible(false);
        return true;
    }

    return false; // let editor handle typing
}

void InlineAutocompletePopup::showAt(juce::Point<int> pos)
{
    setTopLeftPosition(pos);
    setVisible(true);
}

void InlineAutocompletePopup::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF2D2D3D)); // border
}

void InlineAutocompletePopup::resized()
{
    listBox.setBounds(getLocalBounds().reduced(1));
}

int InlineAutocompletePopup::getNumRows()
{
    return (int)filteredItems.size();
}

void InlineAutocompletePopup::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredItems.size()) return;
    
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xFF2D2D3D));
    else
        g.fillAll(juce::Colour(0xFF161B22));
    
    const auto& item = filteredItems[(size_t)rowNumber];
    
    g.setColour(juce::Colour(0xFFE2E8F0));
    g.setFont(juce::FontOptions("Menlo", 12.0f, juce::Font::plain));
    g.drawText(item.text, 8, 0, width - 16, height, juce::Justification::centredLeft, true);
    
    g.setColour(juce::Colour(0xFF9CA3AF));
    g.setFont(juce::FontOptions("Sans", 10.0f, juce::Font::italic));
    g.drawText(item.type, 8, 0, width - 16, height, juce::Justification::centredRight, true);
}

void InlineAutocompletePopup::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    selectedIndex = row;
}

void InlineAutocompletePopup::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int)filteredItems.size())
    {
        if (onInsertCompletion)
            onInsertCompletion(filteredItems[(size_t)row].text);
        setVisible(false);
    }
}

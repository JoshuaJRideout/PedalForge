#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

struct CompletionItem
{
    juce::String text;
    juce::String type;
    juce::String description;
    juce::String wikiLink;
};

class CompletionDatabase
{
public:
    CompletionDatabase();
    
    void loadUIDatabase();
    void loadDSPDatabase();
    void loadGraphBuilderDatabase();
    
    std::vector<CompletionItem> getCompletions(const juce::String& prefix);
    
    std::vector<CompletionItem> items;
};

class AutocompletePanelComponent : public juce::Component,
                                   public juce::ListBoxModel,
                                   public juce::TextEditor::Listener,
                                   public juce::Button::Listener
{
public:
    AutocompletePanelComponent();
    ~AutocompletePanelComponent() override;

    void setDatabaseMode (int mode);

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void textEditorTextChanged (juce::TextEditor& editor) override;
    void buttonClicked (juce::Button* b) override;
    
    std::function<void(const juce::String&)> onInsertCompletion;
    std::function<void(const juce::String&)> onOpenWiki;

    void updateFilter (const juce::String& text);
    void highlightCompletion (const juce::String& word);

    CompletionDatabase& getDatabase() { return database; }

private:
    CompletionDatabase database;
    std::vector<CompletionItem> filteredItems;
    
    juce::TextEditor searchBox;
    juce::ListBox listBox;
    
    juce::Label titleLabel;
    juce::Label typeLabel;
    juce::TextEditor descriptionBox;
    juce::TextButton wikiButton { "Read Wiki" };
    
    int selectedIndex = -1;
    void updateDetailPane();
};

class InlineAutocompletePopup : public juce::Component,
                                public juce::ListBoxModel
{
public:
    InlineAutocompletePopup(CompletionDatabase& db);
    ~InlineAutocompletePopup() override;

    void updateFilter (const juce::String& prefix);
    void moveSelection (int delta);
    
    // Returns true if handled
    bool handleKeyPress (const juce::KeyPress& key);

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    
    std::function<void(const juce::String&)> onInsertCompletion;

    juce::String getCurrentPrefix() const { return currentPrefix; }
    void hide() { setVisible(false); }
    void showAt (juce::Point<int> pos);

private:
    CompletionDatabase& database;
    std::vector<CompletionItem> filteredItems;
    juce::ListBox listBox;
    juce::String currentPrefix;
    int selectedIndex = -1;
};

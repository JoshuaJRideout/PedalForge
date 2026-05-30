#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <vector>

//==============================================================================
/**
 * In-app wiki documentation browser.
 *
 * Renders markdown wiki pages from docs/wiki/ with a sidebar navigation,
 * search, and internal page linking ([[page-name]]).
 */
class WikiTabComponent : public juce::Component,
                         public juce::TextEditor::Listener
{
public:
    WikiTabComponent()
    {
        // --- Sidebar ---
        addAndMakeVisible (sidebar);
        sidebar.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xFF151520));

        // --- Search bar ---
        addAndMakeVisible (searchBox);
        searchBox.setMultiLine (false);
        searchBox.setTextToShowWhenEmpty ("Search wiki...", juce::Colour (0xFF64748B));
        searchBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1E1E2E));
        searchBox.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xFF2A2A3A));
        searchBox.setColour (juce::TextEditor::textColourId,       juce::Colour (0xFFE2E8F0));
        searchBox.addListener (this);

        // --- Content viewport ---
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&contentArea, false);
        viewport.setScrollBarsShown (true, false);
        contentArea.setVisible (true);

        // --- Back / Forward ---
        addAndMakeVisible (backBtn);
        addAndMakeVisible (forwardBtn);
        backBtn.setButtonText ("<");
        forwardBtn.setButtonText (">");
        backBtn.onClick = [this] { navigateBack(); };
        forwardBtn.onClick = [this] { navigateForward(); };
        styleNavButton (backBtn);
        styleNavButton (forwardBtn);

        // --- Breadcrumb ---
        addAndMakeVisible (breadcrumb);
        breadcrumb.setColour (juce::Label::textColourId, juce::Colour (0xFF94A3B8));
        breadcrumb.setFont (juce::Font (13.0f));

        // --- Title ---
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Wiki", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (15.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFE2E8F0));
    }

    //==========================================================================
    /** Load wiki content from a directory of markdown files. */
    void loadFromDirectory (const juce::File& wikiDir)
    {
        wikiRoot = wikiDir;
        pages.clear();
        sidebarItems.clear();

        if (! wikiDir.isDirectory())
            return;

        // Recursively find all .md files
        auto files = wikiDir.findChildFiles (juce::File::findFiles, true, "*.md");
        files.sort();

        for (const auto& f : files)
        {
            auto relativePath = f.getRelativePathFrom (wikiDir);
            auto pageId = relativePath.replace (".md", "").replace (juce::File::getSeparatorString(), "/");

            WikiPage page;
            page.id = pageId;
            page.file = f;
            page.title = extractTitle (f);
            page.content = f.loadFileAsString();
            page.section = f.getParentDirectory().getFileName();

            pages[pageId] = page;
        }

        buildSidebar();

        // Navigate to index
        if (pages.count ("index"))
            navigateTo ("index");
        else if (! pages.empty())
            navigateTo (pages.begin()->first);
    }

    //==========================================================================
    // AI agent text access (reading markdown is far cheaper than screenshots).
    juce::StringArray getPageList() const
    {
        juce::StringArray out;
        for (const auto& [id, page] : pages)
            out.add (id + "  (" + page.title + ")");
        out.sort (true);
        return out;
    }

    /** Raw markdown for a page id (partial-match like navigateTo), or "". */
    juce::String getPageContent (const juce::String& pageId) const
    {
        auto it = pages.find (pageId);
        if (it == pages.end())
            for (const auto& [id, page] : pages)
                if (id == pageId || id.endsWith ("/" + pageId)) { it = pages.find (id); break; }
        return it != pages.end() ? it->second.content : juce::String();
    }

    //==========================================================================
    void navigateTo (const juce::String& pageId)
    {
        auto it = pages.find (pageId);
        if (it == pages.end())
        {
            // Try partial match (e.g., "expression-vm" → "scripting/expression-vm")
            for (auto& [id, page] : pages)
            {
                if (id.endsWith ("/" + pageId) || id == pageId)
                {
                    it = pages.find (id);
                    break;
                }
            }
            if (it == pages.end()) return;
        }

        // History management
        if (currentPage.isNotEmpty())
        {
            if (historyIndex < (int) history.size() - 1)
                history.erase (history.begin() + historyIndex + 1, history.end());
            history.push_back (currentPage);
            historyIndex = (int) history.size() - 1;
        }

        currentPage = it->first;
        renderPage (it->second);
        updateNavState();
    }

    //==========================================================================
    void resized() override
    {
        auto bounds = getLocalBounds();

        // Sidebar: 220px wide
        int sidebarWidth = 220;
        auto sidebarArea = bounds.removeFromLeft (sidebarWidth);

        // Sidebar header with title
        auto sidebarHeader = sidebarArea.removeFromTop (40);
        titleLabel.setBounds (sidebarHeader.reduced (12, 8));

        // Search box
        searchBox.setBounds (sidebarArea.removeFromTop (32).reduced (8, 4));

        // Sidebar list
        sidebar.setBounds (sidebarArea.reduced (0, 4));

        // Content area
        auto contentBounds = bounds;

        // Toolbar with back/forward + breadcrumb
        auto toolbar = contentBounds.removeFromTop (36);
        backBtn.setBounds (toolbar.removeFromLeft (32).reduced (4));
        forwardBtn.setBounds (toolbar.removeFromLeft (32).reduced (4));
        breadcrumb.setBounds (toolbar.reduced (8, 6));

        // Content viewport
        viewport.setBounds (contentBounds);
        contentArea.setSize (contentBounds.getWidth() - 16, 
                            juce::jmax (contentBounds.getHeight(), renderedHeight));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF0F0F14));

        // Sidebar background
        g.setColour (juce::Colour (0xFF151520));
        g.fillRect (0, 0, 220, getHeight());

        // Divider
        g.setColour (juce::Colour (0xFF2A2A3A));
        g.drawVerticalLine (220, 0.0f, (float) getHeight());

        // Toolbar divider
        g.drawHorizontalLine (36, 220.0f, (float) getWidth());
    }

    //==========================================================================
    // TextEditor::Listener
    void textEditorTextChanged (juce::TextEditor& editor) override
    {
        if (&editor == &searchBox)
            filterSidebar (searchBox.getText());
    }

private:
    //==========================================================================
    struct WikiPage
    {
        juce::String id;
        juce::String title;
        juce::String content;
        juce::String section;
        juce::File file;
    };

    //==========================================================================
    // Parsed markdown blocks
    struct MdBlock
    {
        enum Type { Heading, Paragraph, CodeBlock, Table, ListItem, HorizontalRule, Empty };
        Type type = Paragraph;
        juce::String text;
        int headingLevel = 0;
        juce::StringArray tableRows;
        juce::String language;  // for code blocks
    };

    //==========================================================================
    // Content rendering component
    class ContentArea : public juce::Component
    {
    public:
        WikiTabComponent* owner = nullptr;
        std::vector<MdBlock> blocks;

        void paint (juce::Graphics& g) override
        {
            if (!owner) return;

            int y = 20;
            int maxWidth = getWidth() - 60;
            int x = 30;

            for (const auto& block : blocks)
            {
                switch (block.type)
                {
                    case MdBlock::Heading:
                        y += drawHeading (g, block, x, y, maxWidth);
                        break;
                    case MdBlock::Paragraph:
                        y += drawParagraph (g, block, x, y, maxWidth);
                        break;
                    case MdBlock::CodeBlock:
                        y += drawCodeBlock (g, block, x, y, maxWidth);
                        break;
                    case MdBlock::Table:
                        y += drawTable (g, block, x, y, maxWidth);
                        break;
                    case MdBlock::ListItem:
                        y += drawListItem (g, block, x, y, maxWidth);
                        break;
                    case MdBlock::HorizontalRule:
                        y += 8;
                        g.setColour (juce::Colour (0xFF2A2A3A));
                        g.drawHorizontalLine (y, (float) x, (float) (x + maxWidth));
                        y += 16;
                        break;
                    case MdBlock::Empty:
                        y += 8;
                        break;
                }
            }

            owner->renderedHeight = y + 40;
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (!owner) return;

            // Check if click is on a [[link]]
            int y = 20;
            int maxWidth = getWidth() - 60;

            for (const auto& block : blocks)
            {
                if (block.type == MdBlock::Paragraph || block.type == MdBlock::ListItem)
                {
                    // Check for wiki links in the text
                    auto text = block.text;
                    int linkStart = text.indexOf ("[[");
                    while (linkStart >= 0)
                    {
                        int linkEnd = text.indexOf (linkStart, "]]");
                        if (linkEnd > linkStart)
                        {
                            auto linkText = text.substring (linkStart + 2, linkEnd);
                            auto parts = juce::StringArray::fromTokens (linkText, "|", "");
                            auto pageId = parts.size() > 1 ? parts[1].trim() : parts[0].trim().toLowerCase().replace (" ", "-");

                            // Simple hit test: if click is in this block's area, navigate
                            auto blockHeight = estimateBlockHeight (block, maxWidth);
                            if (e.y >= y && e.y < y + blockHeight)
                            {
                                owner->navigateTo (pageId);
                                return;
                            }
                        }
                        linkStart = text.indexOf (linkEnd + 2, "[[");
                    }
                }

                y += estimateBlockHeight (block, maxWidth);
            }
        }

    private:
        //----------------------------------------------------------------------
        int drawHeading (juce::Graphics& g, const MdBlock& block, int x, int y, int maxW)
        {
            float fontSize = 24.0f - (block.headingLevel - 1) * 3.0f;
            fontSize = juce::jmax (fontSize, 14.0f);

            g.setFont (juce::Font (fontSize, juce::Font::bold));

            if (block.headingLevel == 1)
                g.setColour (juce::Colour (0xFFE2E8F0));
            else if (block.headingLevel == 2)
                g.setColour (juce::Colour (0xFF818CF8));  // accent bright
            else
                g.setColour (juce::Colour (0xFF94A3B8));

            auto text = stripMarkdown (block.text);
            auto textArea = juce::Rectangle<int> (x, y, maxW, 200);
            g.drawFittedText (text, textArea, juce::Justification::topLeft, 5);

            int lines = juce::jmax (1, (int) std::ceil ((float) g.getCurrentFont().getStringWidth (text) / maxW));
            int height = (int) (lines * (fontSize + 2)) + 8;

            // Underline for h1/h2
            if (block.headingLevel <= 2)
            {
                g.setColour (juce::Colour (0xFF2A2A3A));
                g.drawHorizontalLine (y + height - 2, (float) x, (float) (x + maxW));
                height += 6;
            }

            return height + 4;
        }

        //----------------------------------------------------------------------
        int drawParagraph (juce::Graphics& g, const MdBlock& block, int x, int y, int maxW)
        {
            g.setFont (juce::Font (14.0f));
            g.setColour (juce::Colour (0xFFCBD5E1));

            auto text = stripMarkdown (block.text);
            auto textArea = juce::Rectangle<int> (x, y, maxW, 2000);

            int lines = juce::jmax (1, (int) std::ceil ((float) g.getCurrentFont().getStringWidth (text) / maxW));
            int height = lines * 19 + 8;

            g.drawFittedText (text, textArea, juce::Justification::topLeft, lines + 2);
            return height;
        }

        //----------------------------------------------------------------------
        int drawCodeBlock (juce::Graphics& g, const MdBlock& block, int x, int y, int maxW)
        {
            // Background
            auto bgRect = juce::Rectangle<int> (x - 4, y, maxW + 8, 0);
            auto lines = juce::StringArray::fromLines (block.text);
            int lineHeight = 16;
            int blockHeight = (int) lines.size() * lineHeight + 20;
            bgRect.setHeight (blockHeight);

            g.setColour (juce::Colour (0xFF1A1A28));
            g.fillRoundedRectangle (bgRect.toFloat(), 6.0f);
            g.setColour (juce::Colour (0xFF2A2A3A));
            g.drawRoundedRectangle (bgRect.toFloat(), 6.0f, 1.0f);

            // Code text
            g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            g.setColour (juce::Colour (0xFF67E8F9));  // cyan

            for (int i = 0; i < lines.size(); ++i)
            {
                // Basic syntax coloring
                auto line = lines[i];
                auto lineColor = getCodeLineColor (line);
                g.setColour (lineColor);
                g.drawText (line, x + 8, y + 8 + i * lineHeight, maxW - 16, lineHeight,
                           juce::Justification::centredLeft);
            }

            return blockHeight + 8;
        }

        //----------------------------------------------------------------------
        int drawTable (juce::Graphics& g, const MdBlock& block, int x, int y, int maxW)
        {
            if (block.tableRows.isEmpty()) return 0;

            g.setFont (juce::Font (13.0f));
            int rowHeight = 26;
            int totalHeight = 0;

            for (int rowIdx = 0; rowIdx < block.tableRows.size(); ++rowIdx)
            {
                auto row = block.tableRows[rowIdx];

                // Skip separator rows (---|---|---)
                if (row.containsOnly ("-| "))
                    continue;

                auto cells = juce::StringArray::fromTokens (row, "|", "");

                // Remove empty first/last from leading/trailing pipes
                while (cells.size() > 0 && cells[0].trim().isEmpty())
                    cells.remove (0);
                while (cells.size() > 0 && cells[cells.size() - 1].trim().isEmpty())
                    cells.remove (cells.size() - 1);

                if (cells.isEmpty()) continue;

                int colWidth = maxW / juce::jmax (1, cells.size());
                bool isHeader = (rowIdx == 0);

                // Row background
                if (isHeader)
                {
                    g.setColour (juce::Colour (0xFF1E1E2E));
                    g.fillRoundedRectangle ((float) x, (float) (y + totalHeight), 
                                           (float) maxW, (float) rowHeight, 4.0f);
                }
                else if (rowIdx % 2 == 0)
                {
                    g.setColour (juce::Colour (0x0AFFFFFF));
                    g.fillRect (x, y + totalHeight, maxW, rowHeight);
                }

                // Cell text
                for (int col = 0; col < cells.size(); ++col)
                {
                    if (isHeader)
                    {
                        g.setFont (juce::Font (13.0f, juce::Font::bold));
                        g.setColour (juce::Colour (0xFFE2E8F0));
                    }
                    else
                    {
                        g.setFont (juce::Font (13.0f));
                        g.setColour (juce::Colour (0xFFCBD5E1));
                    }

                    auto cellText = stripMarkdown (cells[col].trim());
                    g.drawText (cellText, x + col * colWidth + 8, y + totalHeight,
                               colWidth - 16, rowHeight, juce::Justification::centredLeft);
                }

                totalHeight += rowHeight;
            }

            // Border
            g.setColour (juce::Colour (0xFF2A2A3A));
            g.drawRoundedRectangle ((float) x, (float) y, (float) maxW, 
                                   (float) totalHeight, 4.0f, 1.0f);

            return totalHeight + 12;
        }

        //----------------------------------------------------------------------
        int drawListItem (juce::Graphics& g, const MdBlock& block, int x, int y, int maxW)
        {
            g.setFont (juce::Font (14.0f));
            g.setColour (juce::Colour (0xFFCBD5E1));

            auto text = stripMarkdown (block.text);

            // Detect indent level
            int indent = 0;
            auto raw = block.text;
            while (raw.isNotEmpty() && raw[0] == ' ')
            {
                indent++;
                raw = raw.substring (1);
            }
            int indentPx = (indent / 2) * 16;

            // Bullet
            g.setColour (juce::Colour (0xFF6366F1));
            g.fillEllipse ((float) (x + indentPx), (float) (y + 6), 5.0f, 5.0f);

            // Text
            g.setColour (juce::Colour (0xFFCBD5E1));

            // Check for [[links]]
            if (text.contains ("[["))
            {
                // Render with link highlighting
                drawTextWithLinks (g, text, x + indentPx + 14, y, maxW - indentPx - 14);
            }
            else
            {
                g.drawText (text, x + indentPx + 14, y, maxW - indentPx - 14, 20,
                           juce::Justification::centredLeft);
            }

            int lines = juce::jmax (1, (int) std::ceil ((float) g.getCurrentFont().getStringWidth (text) / (maxW - indentPx - 14)));
            return lines * 20 + 2;
        }

        //----------------------------------------------------------------------
        void drawTextWithLinks (juce::Graphics& g, const juce::String& text, int x, int y, int maxW)
        {
            int cx = x;
            int pos = 0;
            auto remaining = text;

            while (remaining.isNotEmpty())
            {
                int linkStart = remaining.indexOf ("[[");
                if (linkStart < 0)
                {
                    g.setColour (juce::Colour (0xFFCBD5E1));
                    g.drawText (remaining, cx, y, maxW - (cx - x), 20, juce::Justification::centredLeft);
                    break;
                }

                // Text before link
                if (linkStart > 0)
                {
                    auto before = remaining.substring (0, linkStart);
                    g.setColour (juce::Colour (0xFFCBD5E1));
                    g.drawText (before, cx, y, maxW - (cx - x), 20, juce::Justification::centredLeft);
                    cx += g.getCurrentFont().getStringWidth (before);
                }

                // Link
                int linkEnd = remaining.indexOf (linkStart, "]]");
                if (linkEnd > linkStart)
                {
                    auto linkContent = remaining.substring (linkStart + 2, linkEnd);
                    auto parts = juce::StringArray::fromTokens (linkContent, "|", "");
                    auto displayText = parts[0].trim();

                    g.setColour (juce::Colour (0xFF818CF8));  // accent bright
                    g.drawText (displayText, cx, y, maxW - (cx - x), 20, juce::Justification::centredLeft);

                    // Underline
                    int textW = g.getCurrentFont().getStringWidth (displayText);
                    g.drawHorizontalLine (y + 17, (float) cx, (float) (cx + textW));
                    cx += textW;

                    remaining = remaining.substring (linkEnd + 2);
                }
                else
                {
                    break;
                }
            }
        }

        //----------------------------------------------------------------------
    public:
        int estimateBlockHeight (const MdBlock& block, int maxW) const
        {
            switch (block.type)
            {
                case MdBlock::Heading:
                {
                    float fontSize = 24.0f - (block.headingLevel - 1) * 3.0f;
                    int height = (int) (fontSize + 2) + 12;
                    if (block.headingLevel <= 2) height += 6;
                    return height;
                }
                case MdBlock::Paragraph:
                {
                    juce::Font f (14.0f);
                    int lines = juce::jmax (1, (int) std::ceil ((float) f.getStringWidth (stripMarkdown (block.text)) / maxW));
                    return lines * 19 + 8;
                }
                case MdBlock::CodeBlock:
                {
                    auto lines = juce::StringArray::fromLines (block.text);
                    return (int) lines.size() * 16 + 28;
                }
                case MdBlock::Table:
                {
                    int rows = 0;
                    for (const auto& r : block.tableRows)
                        if (! r.containsOnly ("-| ")) rows++;
                    return rows * 26 + 12;
                }
                case MdBlock::ListItem:
                {
                    juce::Font f (14.0f);
                    int lines = juce::jmax (1, (int) std::ceil ((float) f.getStringWidth (stripMarkdown (block.text)) / maxW));
                    return lines * 20 + 2;
                }
                case MdBlock::HorizontalRule:
                    return 24;
                case MdBlock::Empty:
                    return 8;
                default:
                    return 20;
            }
        }

        //----------------------------------------------------------------------
        static juce::Colour getCodeLineColor (const juce::String& line)
        {
            auto trimmed = line.trim();
            if (trimmed.startsWith ("--") || trimmed.startsWith ("//") || trimmed.startsWith ("#"))
                return juce::Colour (0xFF6B7280);  // comment gray
            if (trimmed.startsWith ("@"))
                return juce::Colour (0xFFEC4899);  // directive pink
            if (trimmed.contains ("=") && !trimmed.contains ("=="))
                return juce::Colour (0xFF34D399);  // variable green
            return juce::Colour (0xFF67E8F9);      // default cyan
        }

        //----------------------------------------------------------------------
        static juce::String stripMarkdown (const juce::String& text)
        {
            auto result = text;

            // Remove bold markers **text**
            while (result.contains ("**"))
            {
                int start = result.indexOf ("**");
                int end = result.indexOf (start + 2, "**");
                if (end > start)
                    result = result.substring (0, start) 
                           + result.substring (start + 2, end)
                           + result.substring (end + 2);
                else
                    break;
            }

            // Remove inline code `text`
            while (result.contains ("`"))
            {
                int start = result.indexOf ("`");
                int end = result.indexOf (start + 1, "`");
                if (end > start)
                    result = result.substring (0, start)
                           + result.substring (start + 1, end)
                           + result.substring (end + 1);
                else
                    break;
            }

            // Remove [[link]] markers — keep display text
            while (result.contains ("[["))
            {
                int start = result.indexOf ("[[");
                int end = result.indexOf (start, "]]");
                if (end > start)
                {
                    auto linkContent = result.substring (start + 2, end);
                    auto parts = juce::StringArray::fromTokens (linkContent, "|", "");
                    result = result.substring (0, start)
                           + parts[0].trim()
                           + result.substring (end + 2);
                }
                else
                    break;
            }

            // Remove list markers
            result = result.trimStart();
            if (result.startsWith ("- ")) result = result.substring (2);
            if (result.startsWith ("* ")) result = result.substring (2);
            if (result.length() > 2 && result[0] >= '0' && result[0] <= '9' && result[1] == '.')
                result = result.substring (2).trimStart();

            return result.trim();
        }
    };

    //==========================================================================
    // Sidebar list component
    class SidebarList : public juce::ListBox,
                        public juce::ListBoxModel
    {
    public:
        WikiTabComponent* owner = nullptr;

        struct SidebarEntry
        {
            juce::String pageId;
            juce::String displayName;
            juce::String section;
            int indent = 0;
            bool isSectionHeader = false;
        };

        std::vector<SidebarEntry> entries;
        std::vector<SidebarEntry> filteredEntries;

        SidebarList()
        {
            setModel (this);
            setRowHeight (28);
            setColour (juce::ListBox::backgroundColourId, juce::Colour (0xFF151520));
            setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        }

        int getNumRows() override { return (int) filteredEntries.size(); }

        void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (rowNumber < 0 || rowNumber >= (int) filteredEntries.size()) return;

            const auto& entry = filteredEntries[rowNumber];
            bool isActive = owner && owner->currentPage == entry.pageId;

            // Background
            if (isActive)
            {
                g.setColour (juce::Colour (0xFF6366F1).withAlpha (0.2f));
                g.fillRoundedRectangle (4.0f, 2.0f, (float) width - 8.0f, (float) height - 4.0f, 4.0f);
            }
            else if (rowIsSelected)
            {
                g.setColour (juce::Colour (0x15FFFFFF));
                g.fillRoundedRectangle (4.0f, 2.0f, (float) width - 8.0f, (float) height - 4.0f, 4.0f);
            }

            int indent = entry.indent * 12 + 12;

            if (entry.isSectionHeader)
            {
                g.setFont (juce::Font (11.0f, juce::Font::bold));
                g.setColour (juce::Colour (0xFF64748B));
                g.drawText (entry.displayName.toUpperCase(), indent, 0, width - indent - 8, height,
                           juce::Justification::centredLeft);
            }
            else
            {
                g.setFont (juce::Font (13.0f));
                g.setColour (isActive ? juce::Colour (0xFF818CF8) : juce::Colour (0xFFCBD5E1));

                // Active indicator
                if (isActive)
                {
                    g.setColour (juce::Colour (0xFF6366F1));
                    g.fillRoundedRectangle (2.0f, 6.0f, 3.0f, (float) height - 12.0f, 1.5f);
                    g.setColour (juce::Colour (0xFF818CF8));
                }

                g.drawText (entry.displayName, indent, 0, width - indent - 8, height,
                           juce::Justification::centredLeft);
            }
        }

        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (row >= 0 && row < (int) filteredEntries.size() && !filteredEntries[row].isSectionHeader)
            {
                if (owner)
                    owner->navigateTo (filteredEntries[row].pageId);
            }
        }

        void filter (const juce::String& query)
        {
            filteredEntries.clear();
            for (const auto& e : entries)
            {
                if (query.isEmpty() || 
                    e.displayName.containsIgnoreCase (query) ||
                    e.pageId.containsIgnoreCase (query) ||
                    e.isSectionHeader)
                {
                    filteredEntries.push_back (e);
                }
            }
            updateContent();
            repaint();
        }
    };

    //==========================================================================
    void buildSidebar()
    {
        sidebar.entries.clear();
        sidebar.owner = this;

        // Group by section
        std::map<juce::String, std::vector<juce::String>> sections;
        juce::StringArray sectionOrder;

        for (const auto& [id, page] : pages)
        {
            if (id == "index") continue;  // Index goes at top

            auto section = page.section;
            if (section == "wiki") section = "";

            if (sections.find (section) == sections.end())
                sectionOrder.add (section);
            sections[section].push_back (id);
        }

        // Index first
        if (pages.count ("index"))
        {
            SidebarList::SidebarEntry entry;
            entry.pageId = "index";
            entry.displayName = "Home";
            entry.indent = 0;
            sidebar.entries.push_back (entry);
        }

        // Ordered sections
        juce::StringArray preferredOrder = { "getting-started", "architecture", "dsp-nodes", "scripting", "reference" };

        for (const auto& sec : preferredOrder)
        {
            if (sections.find (sec) == sections.end()) continue;

            // Section header
            SidebarList::SidebarEntry header;
            header.isSectionHeader = true;
            header.displayName = sec.replace ("-", " ");
            header.displayName = header.displayName.substring (0, 1).toUpperCase() + header.displayName.substring (1);
            sidebar.entries.push_back (header);

            // Pages in section
            auto& pageIds = sections[sec];
            std::sort (pageIds.begin(), pageIds.end());

            for (const auto& id : pageIds)
            {
                SidebarList::SidebarEntry entry;
                entry.pageId = id;
                entry.section = sec;
                entry.indent = 1;

                // Use page title or derive from filename
                if (pages.count (id))
                    entry.displayName = pages[id].title;
                else
                    entry.displayName = id.fromLastOccurrenceOf ("/", false, false).replace ("-", " ");

                sidebar.entries.push_back (entry);
            }
        }

        sidebar.filteredEntries = sidebar.entries;
        sidebar.updateContent();
    }

    //==========================================================================
    void renderPage (const WikiPage& page)
    {
        contentArea.owner = this;
        contentArea.blocks = parseMarkdown (page.content);

        // Update breadcrumb
        auto path = page.id.replace ("/", " > ");
        breadcrumb.setText (path, juce::dontSendNotification);

        // Estimate total height
        int totalHeight = 60;
        int maxW = viewport.getWidth() - 60;
        if (maxW < 100) maxW = 600;

        for (const auto& block : contentArea.blocks)
            totalHeight += contentArea.estimateBlockHeight (block, maxW);

        renderedHeight = totalHeight;
        contentArea.setSize (viewport.getWidth() - 16, juce::jmax (viewport.getHeight(), renderedHeight));

        viewport.setViewPosition (0, 0);
        contentArea.repaint();
        sidebar.repaint();
    }

    //==========================================================================
    std::vector<MdBlock> parseMarkdown (const juce::String& markdown)
    {
        std::vector<MdBlock> blocks;
        auto lines = juce::StringArray::fromLines (markdown);
        bool inCodeBlock = false;
        juce::String codeAccumulator;
        juce::String codeLang;

        for (int i = 0; i < lines.size(); ++i)
        {
            auto line = lines[i];

            // Code fences
            if (line.trimStart().startsWith ("```"))
            {
                if (inCodeBlock)
                {
                    MdBlock block;
                    block.type = MdBlock::CodeBlock;
                    block.text = codeAccumulator;
                    block.language = codeLang;
                    blocks.push_back (block);
                    codeAccumulator.clear();
                    inCodeBlock = false;
                }
                else
                {
                    codeLang = line.trimStart().substring (3).trim();
                    inCodeBlock = true;
                }
                continue;
            }

            if (inCodeBlock)
            {
                if (codeAccumulator.isNotEmpty())
                    codeAccumulator += "\n";
                codeAccumulator += line;
                continue;
            }

            // Empty line
            if (line.trim().isEmpty())
            {
                blocks.push_back ({ MdBlock::Empty });
                continue;
            }

            // Horizontal rule
            if (line.trim() == "---" || line.trim() == "***" || line.trim() == "___")
            {
                blocks.push_back ({ MdBlock::HorizontalRule });
                continue;
            }

            // Headings
            auto trimmed = line.trimStart();
            if (trimmed.startsWith ("# "))
            {
                MdBlock block;
                block.type = MdBlock::Heading;
                block.headingLevel = 1;
                block.text = trimmed.substring (2);
                blocks.push_back (block);
                continue;
            }
            if (trimmed.startsWith ("## "))
            {
                MdBlock block;
                block.type = MdBlock::Heading;
                block.headingLevel = 2;
                block.text = trimmed.substring (3);
                blocks.push_back (block);
                continue;
            }
            if (trimmed.startsWith ("### "))
            {
                MdBlock block;
                block.type = MdBlock::Heading;
                block.headingLevel = 3;
                block.text = trimmed.substring (4);
                blocks.push_back (block);
                continue;
            }
            if (trimmed.startsWith ("#### "))
            {
                MdBlock block;
                block.type = MdBlock::Heading;
                block.headingLevel = 4;
                block.text = trimmed.substring (5);
                blocks.push_back (block);
                continue;
            }

            // Tables (lines starting with |)
            if (trimmed.startsWith ("|"))
            {
                MdBlock block;
                block.type = MdBlock::Table;
                block.tableRows.add (line);

                // Gather consecutive table rows
                while (i + 1 < lines.size() && lines[i + 1].trimStart().startsWith ("|"))
                {
                    ++i;
                    block.tableRows.add (lines[i]);
                }
                blocks.push_back (block);
                continue;
            }

            // List items
            if (trimmed.startsWith ("- ") || trimmed.startsWith ("* ") ||
                (trimmed.length() > 2 && trimmed[0] >= '0' && trimmed[0] <= '9' && trimmed[1] == '.'))
            {
                MdBlock block;
                block.type = MdBlock::ListItem;
                block.text = line;
                blocks.push_back (block);
                continue;
            }

            // Default: paragraph
            MdBlock block;
            block.type = MdBlock::Paragraph;
            block.text = line;
            blocks.push_back (block);
        }

        return blocks;
    }

    //==========================================================================
    static juce::String extractTitle (const juce::File& file)
    {
        auto content = file.loadFileAsString();
        auto lines = juce::StringArray::fromLines (content);

        for (const auto& line : lines)
        {
            auto trimmed = line.trimStart();
            if (trimmed.startsWith ("# "))
                return trimmed.substring (2).trim();
        }

        // Fallback: use filename
        return file.getFileNameWithoutExtension().replace ("-", " ");
    }

    //==========================================================================
    void navigateBack()
    {
        if (historyIndex > 0)
        {
            historyIndex--;
            currentPage = history[historyIndex];
            if (pages.count (currentPage))
                renderPage (pages[currentPage]);
            updateNavState();
        }
    }

    void navigateForward()
    {
        if (historyIndex < (int) history.size() - 1)
        {
            historyIndex++;
            currentPage = history[historyIndex];
            if (pages.count (currentPage))
                renderPage (pages[currentPage]);
            updateNavState();
        }
    }

    void updateNavState()
    {
        backBtn.setEnabled (historyIndex > 0);
        forwardBtn.setEnabled (historyIndex < (int) history.size() - 1);

        backBtn.setAlpha (backBtn.isEnabled() ? 1.0f : 0.3f);
        forwardBtn.setAlpha (forwardBtn.isEnabled() ? 1.0f : 0.3f);
    }

    void filterSidebar (const juce::String& query)
    {
        sidebar.filter (query);
    }

    void styleNavButton (juce::TextButton& btn)
    {
        btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF1E1E2E));
        btn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE2E8F0));
    }

    //==========================================================================
    juce::File wikiRoot;
    std::map<juce::String, WikiPage> pages;
    std::vector<SidebarList::SidebarEntry> sidebarItems;

    SidebarList sidebar;
    juce::TextEditor searchBox;
    juce::Viewport viewport;
    ContentArea contentArea;
    juce::TextButton backBtn, forwardBtn;
    juce::Label breadcrumb;
    juce::Label titleLabel;

    juce::String currentPage;
    std::vector<juce::String> history;
    int historyIndex = -1;
    int renderedHeight = 800;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WikiTabComponent)
};

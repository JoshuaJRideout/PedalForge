#include "BoardComponent.h"
#include "PedalboardGrid.h"
#include "LookAndFeel.h"
#include "../pedals/PedalRegistry.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/PedalDesign.h"
#include "BoardCanvas.h"

//==============================================================================
BoardComponent::BoardComponent (BoardConfig& cfg, AudioGraphEngine& eng, MidiLearnManager& midiMgr, PedalboardGrid* grid)
    : config (cfg), engine (eng), midiLearn (midiMgr), parentGrid (grid)
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText (config.name, juce::dontSendNotification);
    
    lastActivePage = config.activePage;
    startTimerHz(10);
    titleLabel.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    titleLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setInterceptsMouseClicks(false, false);
    
    addAndMakeVisible (btnMenu);
    btnMenu.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    
    
    btnMenu.onClick = [this] {
        juce::PopupMenu menu;
        
        // Full screen
        menu.addItem ("Go Full Screen", [this] { 
            if (parentGrid)
                parentGrid->getCanvas()->setBoardFullscreen(this, true);
        });
        menu.addSeparator();
        
        // Pages
        juce::PopupMenu pageMenu;
        for (int i = 0; i < config.numPages; ++i)
        {
            pageMenu.addItem (juce::String("Page ") + juce::String(i+1), true, config.activePage == i, [this, i] {
                config.activePage = i;
                rebuildPedals();
            });
        }
        pageMenu.addSeparator();
        pageMenu.addItem ("Add Page", [this] {
            config.numPages++;
            config.activePage = config.numPages - 1;
            rebuildPedals();
        });
        
        if (config.numPages > 1)
        {
            pageMenu.addItem ("Remove Current Page", [this] {
                int pageToRemove = config.activePage;
                
                // Collect pedals to remove
                std::vector<AudioGraphEngine::NodeID> toRemove;
                for (const auto& inst : engine.getPedalInstances())
                {
                    if (inst.boardId == config.id && inst.pageIndex == pageToRemove)
                        toRemove.push_back (inst.nodeID);
                }
                
                // Remove the pedals from the engine
                for (auto id : toRemove)
                    engine.removePedal (id);
                    
                // Shift subsequent pages down
                for (const auto& inst : engine.getPedalInstances())
                {
                    if (inst.boardId == config.id && inst.pageIndex > pageToRemove)
                    {
                        if (auto* mutableInst = engine.getPedalInstance (inst.nodeID))
                            mutableInst->pageIndex--;
                    }
                }
                
                config.numPages--;
                if (config.activePage >= config.numPages)
                    config.activePage = config.numPages - 1;
                    
                rebuildPedals();
            });
        }
        
        menu.addSubMenu ("Page", pageMenu);
        menu.addSeparator();
        
        // Displays
        juce::PopupMenu displayMenu;
        displayMenu.addItem ("Main Display", true, config.displayIndex == -1, [this] { 
            config.displayIndex = -1; 
            if (parentGrid) parentGrid->getCanvas()->setBoardFullscreen(this, false);
        });
        
        auto displays = juce::Desktop::getInstance().getDisplays().displays;
        for (int i = 0; i < displays.size(); ++i)
        {
            displayMenu.addItem ("Display " + juce::String(i+1), true, config.displayIndex == i, [this, i] { 
                config.displayIndex = i; 
                if (parentGrid) parentGrid->getCanvas()->setBoardFullscreen(this, true);
            });
        }
        menu.addSubMenu ("Assign Display", displayMenu);
        menu.addSeparator();
        
        // Turing
        juce::PopupMenu turingMenu;
        turingMenu.addItem ("Enable Output", true, config.assignToTuring, [this] {
            config.assignToTuring = !config.assignToTuring;
        });
        turingMenu.addItem ("Orientation: Horizontal", true, config.turingHorizontal, [this] {
            config.turingHorizontal = true;
        });
        turingMenu.addItem ("Orientation: Vertical", true, !config.turingHorizontal, [this] {
            config.turingHorizontal = false;
        });
        menu.addSubMenu ("Turing LCD", turingMenu);
        
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btnMenu));
    };
    
    rebuildPedals();
}

BoardComponent::~BoardComponent()
{
}

void BoardComponent::updateHeader()
{

}

void BoardComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Board background
    g.setColour (PedalForgeLookAndFeel::bgLight.withAlpha (0.6f));
    g.fillRoundedRectangle (bounds, 6.0f);
    
    // Header background
    g.setColour (PedalForgeLookAndFeel::bgMid.darker(0.2f));
    g.fillRoundedRectangle (bounds.withHeight (getHeaderHeight()), 6.0f);
    g.fillRect (0.0f, getHeaderHeight() - 6.0f, bounds.getWidth(), 6.0f); // square bottom corners of header
    
    // Grid overlay — major/minor lines
    if (config.snapGridSize >= 0.5f)
    {
        float gs = config.snapGridSize;
        float majorEvery = std::max(1.0f, std::round(100.0f / gs));
        float majorStep = gs * majorEvery;
        float bw = getBoardWidth();
        float bh = getBoardHeight();

        // Minor grid lines
        g.setColour (juce::Colour (0x0CFFFFFF));
        for (float gx = gs; gx < bw; gx += gs)
            g.drawLine (gx, getHeaderHeight(), gx, getHeaderHeight() + bh, 0.5f);
        for (float gy = gs; gy < bh; gy += gs)
            g.drawLine (0, getHeaderHeight() + gy, bw, getHeaderHeight() + gy, 0.5f);

        // Major grid lines
        g.setColour (juce::Colour (0x22FFFFFF));
        for (float gx = majorStep; gx < bw; gx += majorStep)
            g.drawLine (gx, getHeaderHeight(), gx, getHeaderHeight() + bh, 1.0f);
        for (float gy = majorStep; gy < bh; gy += majorStep)
            g.drawLine (0, getHeaderHeight() + gy, bw, getHeaderHeight() + gy, 1.0f);
    }
        
    // Resize handle (bottom right)
    g.setColour (PedalForgeLookAndFeel::textMuted.withAlpha (0.5f));
    float rs = 10.0f;
    juce::Path resizeHandle;
    resizeHandle.addTriangle (bounds.getRight() - rs, bounds.getBottom(),
                              bounds.getRight(), bounds.getBottom(),
                              bounds.getRight(), bounds.getBottom() - rs);
    g.fillPath (resizeHandle);
                    
    // Drag preview
    if (isDragHovering)
    {
        auto hoverCol = dragHoverValid ? PedalForgeLookAndFeel::success : PedalForgeLookAndFeel::danger;
        g.setColour (hoverCol.withAlpha (0.2f));
        g.fillRect (dragHoverBoardX,
                    getHeaderHeight() + dragHoverBoardY,
                    dragHoverBoardW,
                    dragHoverBoardH);
        
        g.setColour (hoverCol.withAlpha (0.6f));
        g.drawRect (dragHoverBoardX,
                    getHeaderHeight() + dragHoverBoardY,
                    dragHoverBoardW,
                    dragHoverBoardH, 2.0f);
    }

    // Border
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.2f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.5f);
}

int BoardComponent::getHeaderHeight() const
{
    return 40;
}

void BoardComponent::resized()
{
    auto header = getLocalBounds().removeFromTop (getHeaderHeight());
    
    btnMenu.setBounds (header.removeFromLeft (40).reduced (4, 4));
    
    if (config.cols < 3)
    {
        titleLabel.setVisible (false);
    }
    else
    {
        titleLabel.setVisible (true);
        // Reduce right by 40 to perfectly center the text across the full width
        auto titleArea = header;
        titleArea.removeFromRight (40);
        titleLabel.setBounds (titleArea);
    }
    for (auto& comp : pedalComponents)
    {
        comp->setBounds (comp->getInstance().boardX, getHeaderHeight() + comp->getInstance().boardY, 
                         comp->getInstance().boardW, comp->getInstance().boardH);
    }
}

void BoardComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.y < getHeaderHeight())
    {
        isDraggingBoard = true;
        dragStartPos = e.getEventRelativeTo (getParentComponent()).getPosition();
        boardStartPos = getPosition();
    }
    else if (e.x > getWidth() - 20 && e.y > getHeight() - 20)
    {
        isResizingBoard = true;
        dragStartPos = e.getScreenPosition();
        startCols = config.cols;
        startRows = config.rows;
    }
    else
    {
        if (auto* parent = getParentComponent())
            parent->mouseDown (e.getEventRelativeTo (parent));
    }
}

void BoardComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (isDraggingBoard)
    {
        auto pos = e.getEventRelativeTo (getParentComponent()).getPosition();
        float scale = getTransform().mat00 > 0 ? getTransform().mat00 : 1.0f;
        int dx = std::round ((pos.x - dragStartPos.x) / scale);
        int dy = std::round ((pos.y - dragStartPos.y) / scale);
        
        // Snap to parent canvas grid (e.g. 20px)
        int nx = std::round((boardStartPos.x + dx) / 20.0f) * 20;
        int ny = std::round((boardStartPos.y + dy) / 20.0f) * 20;
        
        config.canvasX = nx;
        config.canvasY = ny;
        setTopLeftPosition (nx, ny);
        
        // Repaint parent to clear any floating title trails
        if (getParentComponent() != nullptr)
            getParentComponent()->repaint();
    }
    else if (isResizingBoard)
    {
        float dx = (e.getScreenPosition().x - dragStartPos.x) / getTransform().mat00;
        float dy = (e.getScreenPosition().y - dragStartPos.y) / getTransform().mat11;
        
        int dCols = std::round (dx / 100.0f);
        int dRows = std::round (dy / 100.0f);
        
        int newCols = juce::jmax (1, startCols + dCols);
        int newRows = juce::jmax (1, startRows + dRows);
        
        if (newCols != config.cols || newRows != config.rows)
        {
            config.cols = newCols;
            config.rows = newRows;
            setSize (getBoardWidth(), getHeaderHeight() + getBoardHeight());
            getParentComponent()->repaint(); // Update connections
        }
    }
    else
    {
        if (auto* parent = getParentComponent())
            parent->mouseDrag (e.getEventRelativeTo (parent));
    }
}

void BoardComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! isDraggingBoard && ! isResizingBoard)
    {
        if (auto* parent = getParentComponent())
            parent->mouseUp (e.getEventRelativeTo (parent));
    }
    isDraggingBoard = false;
    isResizingBoard = false;
}

//==============================================================================
juce::Point<float> BoardComponent::snapToGrid (float px, float py) const
{
    float gs = config.snapGridSize;
    if (gs <= 0.0f) return { px, py };
    return { std::round(px / gs) * gs, std::round(py / gs) * gs };
}

bool BoardComponent::isGridRectFree (float bx, float by, float bw, float bh, AudioGraphEngine::NodeID ignoreNodeId) const
{
    juce::Rectangle<float> rect(bx, by, bw, bh);
    if (bx < 0.0f || by < 0.0f || rect.getRight() > getBoardWidth() || rect.getBottom() > getBoardHeight())
        return false;

    for (auto& inst : engine.getPedalInstances())
    {
        if (! inst.onBoard || inst.boardId != config.id || inst.pageIndex != config.activePage)
            continue;
        if (inst.nodeID == ignoreNodeId) continue;
        
        juce::Rectangle<float> other(inst.boardX, inst.boardY, inst.boardW, inst.boardH);
        if (rect.intersects(other))
            return false;
    }
    return true;
}

void BoardComponent::rebuildPedals()
{
    lastActivePage = config.activePage;
    pedalComponents.clear();

    for (auto& inst : engine.getPedalInstances())
    {
        if (! inst.onBoard || inst.boardId != config.id || inst.pageIndex != config.activePage)
            continue;

        auto comp = std::make_unique<PedalComponent> (const_cast<PedalInstance&> (inst), engine, midiLearn);
        comp->setGrid (this);

        // Wire library_loader callback through the grid
        if (parentGrid != nullptr)
        {
            comp->onOpenLibrary = [this] (const juce::String& category, std::function<void(const juce::File&)> cb)
            {
                if (parentGrid && parentGrid->onOpenLibrary)
                    parentGrid->onOpenLibrary (category, cb);
            };
            
            comp->onOpenOverlay = [this, compPtr = comp.get()] (const juce::String& pageName)
            {
                if (parentGrid && parentGrid->onOpenOverlay)
                    parentGrid->onOpenOverlay (&compPtr->getInstance(), pageName);
            };
        }

        comp->setBounds (inst.boardX, getHeaderHeight() + inst.boardY, inst.boardW, inst.boardH);

        addAndMakeVisible (comp.get());
        pedalComponents.push_back (std::move (comp));
    }
    resized();
    repaint();
}

void BoardComponent::timerCallback()
{
    if (config.activePage != lastActivePage)
    {
        rebuildPedals();
    }
}

// Drag & Drop
bool BoardComponent::isInterestedInDragSource (const SourceDetails& details)
{
    auto desc = details.description.toString();
    return desc.startsWith ("pedal:") || desc.startsWith ("active_pedal:");
}

void BoardComponent::itemDragEnter (const SourceDetails& details)
{
    isDragHovering = true;
    itemDragMove (details);
}

void BoardComponent::itemDragMove (const SourceDetails& details)
{
    auto desc = details.description.toString();
    juce::StringArray tok;
    tok.addTokens (desc, ":", "");
    
    float bw = 100.0f, bh = 200.0f;
    if (tok.size() >= 2 && tok[0] == "pedal")
    {
        for (auto& info : getFactoryPedals())
        {
            if (info.name == tok[1])
            {
                bw = info.gridW * 100.0f;
                bh = info.gridH * 100.0f;
                break;
            }
        }
    }
    else if (tok.size() >= 2 && tok[0] == "active_pedal")
    {
        auto nodeId = AudioGraphEngine::NodeID { static_cast<juce::uint32> (tok[1].getLargeIntValue()) };
        if (auto* inst = engine.getPedalInstance(nodeId))
        {
            bw = inst->boardW;
            bh = inst->boardH;
        }
    }

    auto localPos = details.localPosition;
    auto snap = snapToGrid (localPos.x - bw / 2.0f, localPos.y - getHeaderHeight() - bh / 2.0f);
    
    dragHoverBoardX = snap.x;
    dragHoverBoardY = snap.y;
    dragHoverBoardW = bw;
    dragHoverBoardH = bh;
    dragHoverValid = isGridRectFree (snap.x, snap.y, bw, bh);
    
    repaint();
}

void BoardComponent::itemDragExit (const SourceDetails& details)
{
    isDragHovering = false;
    repaint();
}

void BoardComponent::itemDropped (const SourceDetails& details)
{
    auto desc = details.description.toString();
    juce::StringArray tok;
    tok.addTokens (desc, ":", "");
    if (tok.size() < 2) return;

    auto localPos = details.localPosition;
    auto snap = snapToGrid (localPos.x - dragHoverBoardW / 2.0f, localPos.y - getHeaderHeight() - dragHoverBoardH / 2.0f);

    if (tok[0] == "pedal")
    {
        auto type = tok[1];
        bool loaded = false;
        for (auto& info : getFactoryPedals())
        {
            if (info.name == type)
            {
                auto processor = info.factory();
                auto nodeId = engine.addPedal (std::move (processor),
                                                config.id, config.activePage,
                                                snap.x, snap.y,
                                                info.gridW * 100.0f, info.gridH * 100.0f);
                                                
                if (auto* inst = engine.getPedalInstance (nodeId))
                {
                    inst->colour = info.colour;
                    inst->category = info.category;
                    inst->numKnobs = info.numKnobs;

                    // If it's a DSP pedal (e.g. Memory Node), load its default chassis/wiring design
                    if (auto* node = engine.getGraph().getNodeForId (nodeId))
                    {
                        if (auto* proc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor()))
                        {
                            if (info.designFactory)
                            {
                                inst->design = info.designFactory();
                            }
                            else
                            {
                                inst->design = std::make_shared<PedalDesign>();
                                inst->design->name = info.name;
                                inst->design->category = info.category;
                                inst->design->chassisW = info.gridW * 100.0f;
                                inst->design->chassisH = info.gridH * 100.0f;
                                inst->design->chassisColour = info.colour;
                            }

                            // Serialize the factory graph so we can edit it later
                            inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                        }
                    }
                }
                engine.autoRoutePedal(nodeId);
                loaded = true;
                break;
            }
        }
        
        if (!loaded)
        {
            if (auto design = loadCustomPedalDesign (type))
            {
                juce::String jsonGraph;
                if (!design->effectsGraph.isVoid())
                    jsonGraph = juce::JSON::toString(design->effectsGraph);
                    
                auto processor = std::make_unique<GraphPedalProcessor>(design->name, jsonGraph);
                
                float bw = std::max(100.0f, design->chassisW);
                float bh = std::max(100.0f, design->chassisH);

                auto nodeId = engine.addPedal (std::move (processor),
                                                config.id, config.activePage,
                                                snap.x, snap.y,
                                                bw, bh);
                
                if (auto* inst = engine.getPedalInstance (nodeId))
                {
                    inst->colour = design->chassisColour;
                    inst->category = design->category;
                    inst->design = design;
                    
                    int numKnobs = 0;
                    for (const auto& c : design->controls)
                        if (c.type == "knob") numKnobs++;
                    inst->numKnobs = numKnobs;
                }
                engine.autoRoutePedal(nodeId);
            }
        }
    }
    else if (tok[0] == "active_pedal")
    {
        juce::AudioProcessorGraph::NodeID nodeId { (juce::uint32) tok[1].getLargeIntValue() };
        if (auto* inst = engine.getPedalInstance (nodeId))
        {
            inst->onBoard = true;
            inst->boardId = config.id;
            inst->pageIndex = config.activePage;
            inst->boardX = snap.x;
            inst->boardY = snap.y;
        }
    }
    
    if (parentGrid)
        parentGrid->rebuildFromEngine();
    else
        rebuildPedals();
}

void BoardComponent::rebuildFromEngine()
{
    rebuildPedals();
}

void BoardComponent::removePedal (AudioGraphEngine::NodeID nodeId)
{
    engine.removePedal(nodeId);
    if (parentGrid)
        parentGrid->rebuildFromEngine();
    else
        rebuildPedals();
}


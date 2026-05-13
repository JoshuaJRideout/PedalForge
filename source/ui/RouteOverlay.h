#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include <vector>
#include <set>
#include <map>
#include <queue>

//==============================================================================
/**
 * Tile-based routing overlay.
 *
 * Renders semi-transparent directional arrow tiles along grid channels
 * between connected pedals. Routes auto-compute using BFS pathfinding
 * on the grid, avoiding occupied cells.
 *
 * Visibility modes:
 *   - Hidden (default)
 *   - Shown when "Show Routing" is toggled on
 *   - Shown while dragging a connector
 */
class RouteOverlay : public juce::Component,
                     public juce::Timer
{
public:
    RouteOverlay (AudioGraphEngine& engine);

    void paint (juce::Graphics& g) override;
    
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    //==========================================================================
    /** Recompute all routes from the engine's current connections. */
    void rebuild (int cellSize, int originX, int originY,
                  int gridCols, int gridRows);

    /** Toggle routing visibility. */
    void setRoutingVisible (bool shouldBeVisible);
    bool isRoutingVisible() const { return routingVisible; }

    /** Set whether we're in drag mode (always show). */
    void setDragMode (bool isDragging) { dragActive = isDragging; repaint(); }

    /** Set the signal flow animation phase. */
    void timerCallback() override;

    //==========================================================================
    // A single grid coordinate
    struct Cell
    {
        int x, y;
        bool operator== (const Cell& o) const { return x == o.x && y == o.y; }
        bool operator<  (const Cell& o) const { return x < o.x || (x == o.x && y < o.y); }
    };

    /** Set a temporary route being drawn by the user. */
    void setDragRoute (Cell start, Cell end);
    void clearDragRoute();

    bool isValidDropTarget (Cell end, juce::AudioProcessorGraph::NodeID sourceNode) const;

private:
    //==========================================================================
    // Direction of travel through a cell
    enum class Dir { Right, Left, Down, Up, None };

    // A complete route between two nodes
    struct Route
    {
        std::vector<Cell> path; // Sequence of intersection nodes
        juce::Colour colour;
    };

    struct PedalBounds
    {
        int x, y, w, h;
    };

    //==========================================================================
    /** BFS pathfinding from start to end, avoiding pedal interiors. */
    std::vector<Cell> findPath (Cell start, Cell end,
                                const std::vector<PedalBounds>& pedals,
                                int cols, int rows) const;

    /** Draw animated chevrons along a path. */
    void drawChevrons (juce::Graphics& g, const std::vector<Cell>& path, 
                       float alpha, float animPhase) const;

    /** Draw an arrow chevron at a position pointing in a direction. */
    void drawChevron (juce::Graphics& g, float cx, float cy,
                      Dir dir, float size, float alpha) const;

    AudioGraphEngine& engine;

    std::vector<Route> routes;
    Route dragRoute;
    bool hasDragRoute = false;
    juce::AudioProcessorGraph::NodeID dragSourceNodeID;
    Cell dragStartCell;

    bool routingVisible = false;
    bool dragActive = false;
    float animPhase = 0.0f;

    int cellSize = 60;
    int originX  = 0;
    int originY  = 0;
    int gridCols = 10;
    int gridRows = 7;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RouteOverlay)
};

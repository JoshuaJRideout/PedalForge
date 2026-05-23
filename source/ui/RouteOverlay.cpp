#include "RouteOverlay.h"
#include "LookAndFeel.h"

//==============================================================================
RouteOverlay::RouteOverlay (AudioGraphEngine& eng)
    : engine (eng)
{
    setInterceptsMouseClicks (false, false); // Click-through
    startTimerHz (30); // Animation at 30fps
}

//==============================================================================
void RouteOverlay::timerCallback()
{
    if (routingVisible || dragActive)
    {
        animPhase += 0.04f;
        if (animPhase > 1.0f) animPhase -= 1.0f;
        repaint();
    }
}

//==============================================================================
void RouteOverlay::setRoutingVisible (bool shouldBeVisible)
{
    routingVisible = shouldBeVisible;
    setInterceptsMouseClicks (routingVisible, false);
    repaint();
}


//==============================================================================
void RouteOverlay::rebuild (int cs, int ox, int oy,
                            int gridCols, int gridRows)
{
    cellSize = cs;
    originX  = ox;
    originY  = oy;
    this->gridCols = gridCols;
    this->gridRows = gridRows;
    routes.clear();

    // Build the list of pedal bounds
    std::vector<PedalBounds> pedals;
    for (auto& inst : engine.getPedalInstances())
    {
        pedals.push_back ({ (int)std::round(inst.boardX / 100.0f), 
                            (int)std::round(inst.boardY / 100.0f), 
                            (int)std::max(1.0f, std::round(inst.boardW / 100.0f)), 
                            (int)std::max(1.0f, std::round(inst.boardH / 100.0f)) });
    }

    // Get the actual connections
    auto connections = engine.getGraph().getConnections();
    if (connections.empty())
    {
        repaint();
        return;
    }

    // Group connections by unique pairs of nodes (so stereo is drawn as one cable)
    std::set<std::pair<juce::AudioProcessorGraph::NodeID, juce::AudioProcessorGraph::NodeID>> uniquePairs;
    for (auto& conn : connections)
        uniquePairs.insert ({ conn.source.nodeID, conn.destination.nodeID });

    auto getCellForNode = [&] (juce::AudioProcessorGraph::NodeID nodeId, bool isOutput) -> Cell
    {
        if (nodeId == engine.getAudioInputNodeID())
            return { -1, gridRows / 2 };
        if (nodeId == engine.getAudioOutputNodeID())
            return { gridCols + 1, gridRows / 2 };

        if (auto* inst = engine.getPedalInstance (nodeId))
        {
            int gx = (int)std::round(inst->boardX / 100.0f);
            int gy = (int)std::round(inst->boardY / 100.0f);
            int gw = (int)std::max(1.0f, std::round(inst->boardW / 100.0f));
            int gh = (int)std::max(1.0f, std::round(inst->boardH / 100.0f));
            
            if (isOutput)
                return { gx + gw, gy + gh / 2 };
            else
                return { gx, gy + gh / 2 };
        }
        return { 0, 0 };
    };

    for (auto& pair : uniquePairs)
    {
        Cell start = getCellForNode (pair.first, true);
        Cell end   = getCellForNode (pair.second, false);

        // Don't draw internal feedback loops or invalid paths
        if (start == end) continue;

        auto path = findPath (start, end, pedals, gridCols, gridRows);
        
        if (! path.empty())
        {
            Route r;
            r.path   = path;
            r.colour = PedalForgeLookAndFeel::accent;
            routes.push_back (std::move (r));
        }
    }
    repaint();
}

//==============================================================================
void RouteOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! routingVisible) return;

    int gx = (int) std::round ((e.x - originX) / (float) cellSize);
    int gy = (int) std::round ((e.y - originY) / (float) cellSize);

    // Check main IN
    if ((gx == 0 || gx == -1) && gy == gridRows / 2)
    {
        dragSourceNodeID = engine.getAudioInputNodeID();
        dragStartCell = { -1, gy };
        return;
    }

    // Check pedal outputs
    for (auto& inst : engine.getPedalInstances())
    {
        int igx = (int)std::round(inst.boardX / 100.0f);
        int igy = (int)std::round(inst.boardY / 100.0f);
        int igw = (int)std::max(1.0f, std::round(inst.boardW / 100.0f));
        int igh = (int)std::max(1.0f, std::round(inst.boardH / 100.0f));
        
        if (gx == igx + igw && gy == igy + igh / 2)
        {
            dragSourceNodeID = inst.nodeID;
            dragStartCell = { gx, gy };
            return;
        }
    }

    // Did we click on an existing connection? Delete it.
    // (A simple heuristic: if we click on an input jack, disconnect anything going to it)
    if ((gx == gridCols || gx == gridCols + 1) && gy == gridRows / 2)
    {
        engine.disconnectAll (engine.getAudioOutputNodeID());
        rebuild (cellSize, originX, originY, gridCols, gridRows);
        return;
    }

    for (auto& inst : engine.getPedalInstances())
    {
        int igx = (int)std::round(inst.boardX / 100.0f);
        int igy = (int)std::round(inst.boardY / 100.0f);
        int igh = (int)std::max(1.0f, std::round(inst.boardH / 100.0f));
        
        if (gx == igx && gy == igy + igh / 2)
        {
            // We clicked an input jack, disconnect it
            auto conns = engine.getGraph().getConnections();
            for (auto& c : conns)
            {
                if (c.destination.nodeID == inst.nodeID)
                    engine.getGraph().removeConnection (c);
            }
            rebuild (cellSize, originX, originY, gridCols, gridRows);
            return;
        }
    }

    dragSourceNodeID = juce::AudioProcessorGraph::NodeID();
}

void RouteOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (! routingVisible || dragSourceNodeID.uid == 0) return;

    int gx = (int) std::round ((e.x - originX) / (float) cellSize);
    int gy = (int) std::round ((e.y - originY) / (float) cellSize);
    
    // Clamp
    gx = juce::jlimit (0, gridCols, gx);
    gy = juce::jlimit (0, gridRows, gy);

    setDragRoute (dragStartCell, { gx, gy });
}

void RouteOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (! routingVisible || dragSourceNodeID.uid == 0) return;

    int gx = (int) std::round ((e.x - originX) / (float) cellSize);
    int gy = (int) std::round ((e.y - originY) / (float) cellSize);

    clearDragRoute();

    juce::AudioProcessorGraph::NodeID destNodeID;
    
    // Check main OUT
    if ((gx == gridCols || gx == gridCols + 1) && gy == gridRows / 2)
    {
        destNodeID = engine.getAudioOutputNodeID();
    }
    else
    {
        // Check pedal inputs
        for (auto& inst : engine.getPedalInstances())
        {
            int igx = (int)std::round(inst.boardX / 100.0f);
            int igy = (int)std::round(inst.boardY / 100.0f);
            int igh = (int)std::max(1.0f, std::round(inst.boardH / 100.0f));
            
            if (gx == igx && gy == igy + igh / 2)
            {
                destNodeID = inst.nodeID;
                break;
            }
        }
    }

    if (destNodeID.uid != 0 && destNodeID != dragSourceNodeID)
    {
        // Connect both stereo channels
        engine.connect (dragSourceNodeID, 0, destNodeID, 0);
        engine.connect (dragSourceNodeID, 1, destNodeID, 1);
        rebuild (cellSize, originX, originY, gridCols, gridRows);
    }

    dragSourceNodeID = juce::AudioProcessorGraph::NodeID();
}

//==============================================================================
void RouteOverlay::setDragRoute (Cell start, Cell end)
{
    hasDragRoute = true;
    dragRoute.path.clear();

    std::vector<PedalBounds> pedals;
    for (auto& inst : engine.getPedalInstances())
    {
        pedals.push_back ({ (int)std::round(inst.boardX / 100.0f), 
                            (int)std::round(inst.boardY / 100.0f), 
                            (int)std::max(1.0f, std::round(inst.boardW / 100.0f)), 
                            (int)std::max(1.0f, std::round(inst.boardH / 100.0f)) });
    }

    auto path = findPath (start, end, pedals, gridCols, gridRows);
    if (! path.empty())
    {
        dragRoute.path = path;
        if (isValidDropTarget (end, dragSourceNodeID))
            dragRoute.colour = PedalForgeLookAndFeel::success.brighter();
        else
            dragRoute.colour = PedalForgeLookAndFeel::danger.brighter();
    }
    repaint();
}

//==============================================================================
bool RouteOverlay::isValidDropTarget (Cell end, juce::AudioProcessorGraph::NodeID sourceNode) const
{
    // Main OUT
    if ((end.x == gridCols || end.x == gridCols + 1) && end.y == gridRows / 2)
        return sourceNode != engine.getAudioOutputNodeID();

    // Pedal inputs
    for (auto& inst : engine.getPedalInstances())
    {
        int igx = (int)std::round(inst.boardX / 100.0f);
        int igy = (int)std::round(inst.boardY / 100.0f);
        int igh = (int)std::max(1.0f, std::round(inst.boardH / 100.0f));
        
        if (end.x == igx && end.y == igy + igh / 2)
            return inst.nodeID != sourceNode;
    }

    return false;
}

void RouteOverlay::clearDragRoute()
{
    hasDragRoute = false;
    repaint();
}

//==============================================================================
void RouteOverlay::paint (juce::Graphics& g)
{
    if (! routingVisible && ! dragActive) return;

    float baseAlpha = dragActive ? 0.5f : 0.35f;

    for (auto& route : routes)
    {
        if (route.path.size() < 2) continue;

        juce::Path p;
        p.startNewSubPath (originX + route.path[0].x * cellSize,
                           originY + route.path[0].y * cellSize);

        for (size_t i = 1; i < route.path.size(); ++i)
        {
            p.lineTo (originX + route.path[i].x * cellSize,
                      originY + route.path[i].y * cellSize);
        }

        g.setColour (route.colour.withAlpha (baseAlpha));
        g.strokePath (p, juce::PathStrokeType (juce::jmax(2.0f, cellSize * 0.08f),
                                               juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));

        drawChevrons (g, route.path, baseAlpha * 1.8f, animPhase);
    }

    if (hasDragRoute && dragRoute.path.size() >= 2)
    {
        juce::Path p;
        p.startNewSubPath (originX + dragRoute.path[0].x * cellSize,
                           originY + dragRoute.path[0].y * cellSize);

        for (size_t i = 1; i < dragRoute.path.size(); ++i)
        {
            p.lineTo (originX + dragRoute.path[i].x * cellSize,
                      originY + dragRoute.path[i].y * cellSize);
        }

        g.setColour (dragRoute.colour.withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (juce::jmax(2.0f, cellSize * 0.08f),
                                               juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));

        drawChevrons (g, dragRoute.path, 1.0f, std::fmod (animPhase * 2.0f, 1.0f));
    }

    // Draw connection jacks (nodes) to show where things can connect
    if (routingVisible)
    {
        auto drawJack = [&](int gx, int gy, bool isInput) {
            float cx = originX + gx * cellSize;
            float cy = originY + gy * cellSize;
            g.setColour (PedalForgeLookAndFeel::bgDark);
            g.fillEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
            g.setColour (isInput ? PedalForgeLookAndFeel::textMuted : PedalForgeLookAndFeel::accent);
            g.drawEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, 2.0f);
            g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
        };

        // Main IN and OUT
        drawJack (-1, gridRows / 2, false); // Board IN is an output for paths
        drawJack (gridCols + 1, gridRows / 2, true); // Board OUT is an input for paths

        // Pedal jacks
        for (auto& inst : engine.getPedalInstances())
        {
            int igx = (int)std::round(inst.boardX / 100.0f);
            int igy = (int)std::round(inst.boardY / 100.0f);
            int igw = (int)std::max(1.0f, std::round(inst.boardW / 100.0f));
            int igh = (int)std::max(1.0f, std::round(inst.boardH / 100.0f));
            
            drawJack (igx, igy + igh / 2, true);             // Input
            drawJack (igx + igw, igy + igh / 2, false); // Output
        }
    }


    // INPUT / OUTPUT labels
    if (routingVisible)
    {
        float labelAlpha = baseAlpha * 0.9f;
        g.setColour (PedalForgeLookAndFeel::textMuted.withAlpha (labelAlpha));
        g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
        
        g.drawText ("INPUT", originX - cellSize * 1.5f, originY + (gridRows / 2) * cellSize - 10,
                    60, 20, juce::Justification::centredLeft);
        
        g.drawText ("OUTPUT", originX + (gridCols + 1) * cellSize - 45, originY + (gridRows / 2) * cellSize - 10,
                    60, 20, juce::Justification::centredRight);
    }
}

//==============================================================================
void RouteOverlay::drawChevrons (juce::Graphics& g, const std::vector<Cell>& path, 
                                 float alpha, float animPhase) const
{
    if (path.size() < 2) return;

    // We can place chevrons on the segments
    int chevronSpacing = juce::jmax (2, (int) path.size() / 6);
    int offset = (int) (animPhase * chevronSpacing);

    for (size_t i = offset; i + 1 < path.size(); i += chevronSpacing)
    {
        auto& c1 = path[i];
        auto& c2 = path[i + 1];

        float cx = originX + (c1.x + c2.x) * 0.5f * cellSize;
        float cy = originY + (c1.y + c2.y) * 0.5f * cellSize;

        Dir dir = Dir::None;
        if (c2.x > c1.x) dir = Dir::Right;
        else if (c2.x < c1.x) dir = Dir::Left;
        else if (c2.y > c1.y) dir = Dir::Down;
        else if (c2.y < c1.y) dir = Dir::Up;

        drawChevron (g, cx, cy, dir, cellSize * 0.28f, alpha);
    }
}

//==============================================================================
void RouteOverlay::drawChevron (juce::Graphics& g, float cx, float cy,
                                 Dir dir, float size, float alpha) const
{
    if (dir == Dir::None) return;

    juce::Path chevron;
    float hs = size * 0.5f;

    switch (dir)
    {
        case Dir::Right:
            chevron.addTriangle (cx - hs * 0.6f, cy - hs,
                                 cx + hs * 0.6f, cy,
                                 cx - hs * 0.6f, cy + hs);
            break;
        case Dir::Left:
            chevron.addTriangle (cx + hs * 0.6f, cy - hs,
                                 cx - hs * 0.6f, cy,
                                 cx + hs * 0.6f, cy + hs);
            break;
        case Dir::Down:
            chevron.addTriangle (cx - hs, cy - hs * 0.6f,
                                 cx,      cy + hs * 0.6f,
                                 cx + hs, cy - hs * 0.6f);
            break;
        case Dir::Up:
            chevron.addTriangle (cx - hs, cy + hs * 0.6f,
                                 cx,      cy - hs * 0.6f,
                                 cx + hs, cy + hs * 0.6f);
            break;
        default: break;
    }

    g.setColour (PedalForgeLookAndFeel::accent.withAlpha (alpha));
    g.fillPath (chevron);
}

//==============================================================================
std::vector<RouteOverlay::Cell> RouteOverlay::findPath (
    Cell start, Cell end,
    const std::vector<PedalBounds>& pedals,
    int cols, int rows) const
{
    // Dijkstra's algorithm on the grid intersections
    struct PQNode {
        int x, y, d;
        int cost;
        int parentIdx; // index in visited list
        bool operator> (const PQNode& o) const { return cost > o.cost; }
    };

    struct VisitedNode {
        Cell cell;
        int parent;
    };

    struct StateKey {
        int x, y, d;
        bool operator< (const StateKey& o) const {
            if (x != o.x) return x < o.x;
            if (y != o.y) return y < o.y;
            return d < o.d;
        }
    };

    auto isValidNode = [&] (int x, int y) -> bool {
        return x >= -1 && x <= cols + 1 && y >= -1 && y <= rows + 1;
    };

    auto isEdgeBlocked = [&] (int x1, int y1, int x2, int y2) -> bool {
        for (auto& p : pedals) {
            // Check if horizontal edge (y1 == y2) is strictly inside the pedal
            if (y1 == y2) {
                if (y1 > p.y && y1 < p.y + p.h) {
                    int minX = std::min(x1, x2);
                    if (minX >= p.x && minX < p.x + p.w) return true;
                }
            }
            // Check if vertical edge (x1 == x2) is strictly inside the pedal
            else if (x1 == x2) {
                if (x1 > p.x && x1 < p.x + p.w) {
                    int minY = std::min(y1, y2);
                    if (minY >= p.y && minY < p.y + p.h) return true;
                }
            }
        }
        return false;
    };

    std::vector<VisitedNode> visited;
    std::map<StateKey, int> minCost;
    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> pq;

    // Start with direction 4 (none)
    pq.push ({ start.x, start.y, 4, 0, -1 });
    minCost[{ start.x, start.y, 4 }] = 0;

    static constexpr int dx[] = { 1, -1, 0, 0 };
    static constexpr int dy[] = { 0, 0, 1, -1 };

    int foundIdx = -1;

    while (! pq.empty())
    {
        auto cur = pq.top();
        pq.pop();

        StateKey curKey { cur.x, cur.y, cur.d };
        if (cur.cost > minCost[curKey]) continue;

        int idx = (int) visited.size();
        visited.push_back ({ { cur.x, cur.y }, cur.parentIdx });

        if (cur.x == end.x && cur.y == end.y)
        {
            foundIdx = idx;
            break;
        }

        for (int nd = 0; nd < 4; ++nd)
        {
            int nx = cur.x + dx[nd];
            int ny = cur.y + dy[nd];

            if (! isValidNode (nx, ny)) continue;
            if (isEdgeBlocked (cur.x, cur.y, nx, ny)) continue;

            int moveCost = 10;

            // Penalty for turning
            if (cur.d != 4 && cur.d != nd)
            {
                // Forbid U-turns
                if ((cur.d == 0 && nd == 1) || (cur.d == 1 && nd == 0) ||
                    (cur.d == 2 && nd == 3) || (cur.d == 3 && nd == 2))
                    continue;

                moveCost += 100; // Big penalty for bending
            }

            int nextCost = cur.cost + moveCost;
            StateKey nextKey { nx, ny, nd };

            if (minCost.find (nextKey) == minCost.end() || nextCost < minCost[nextKey])
            {
                minCost[nextKey] = nextCost;
                pq.push ({ nx, ny, nd, nextCost, idx });
            }
        }
    }

    std::vector<Cell> path;
    if (foundIdx != -1)
    {
        int curr = foundIdx;
        while (curr != -1)
        {
            path.push_back (visited[curr].cell);
            curr = visited[curr].parent;
        }
        std::reverse (path.begin(), path.end());
    }

    return path;
}


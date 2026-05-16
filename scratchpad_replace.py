import sys
with open('source/ui/PedalboardGrid.cpp', 'r') as f:
    lines = f.readlines()

new_lines = lines[:161]
new_content = """
void PedalboardGrid::selectPedal (PedalComponent* comp)
{
    if (selectedComponent == comp)
        return;

    selectedComponent = comp;

    if (selectedComponent != nullptr)
    {
        selectedComponent->toFront (false);
        detailPanel.showPedal (selectedComponent->getInstance(), engine);
    }
    else
    {
        detailPanel.clearSelection();
    }

    resized();
    repaint();
}

void PedalboardGrid::deselectAll()
{
    selectedComponent = nullptr;
    detailPanel.clearSelection();
    resized();
    repaint();
}

void PedalboardGrid::refreshSelectedPedal()
{
    if (selectedComponent != nullptr)
    {
        selectedComponent->getInstance().gridW = std::max(1, (int)std::round (selectedComponent->getInstance().design->chassisW / 100.0f));
        selectedComponent->getInstance().gridH = std::max(1, (int)std::round (selectedComponent->getInstance().design->chassisH / 100.0f));
        
        selectedComponent->repaint();
        detailPanel.showPedal (selectedComponent->getInstance(), engine);
        resized();
        repaint();
    }
}

void PedalboardGrid::addPedalCopy (const PedalInstance& srcInst, int gridX, int gridY)
{
    // Implementation not supported yet
}

bool PedalboardGrid::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (auto* inst = getSelectedInstance())
        {
            removePedal (inst->nodeID);
            return true;
        }
    }
    
    return false;
}

void PedalboardGrid::removePedal (AudioGraphEngine::NodeID nodeId)
{
    engine.removePedal (nodeId);
    deselectAll();
    rebuildFromEngine();
}

//==============================================================================
void PedalboardGrid::rebuildFromEngine()
{
    activePedalsList.refresh();
    if (boardCanvas)
        boardCanvas->rebuildBoards();
}
"""
new_lines.extend([line + '\n' for line in new_content.strip().split('\n')])

# find where ActivePedalsList begins
idx = 0
for i, line in enumerate(lines):
    if "ActivePedalsList::paint" in line:
        idx = i
        break

# back up to the comment block
while idx > 0 and "// ActivePedalsList" not in lines[idx-1] and "//=" not in lines[idx-1]:
    idx -= 1
if idx > 0 and "//=" in lines[idx-1]:
    idx -= 1
if idx > 0 and "//=" in lines[idx-1]:
    idx -= 1

new_lines.append('\n')
new_lines.extend(lines[idx:])

with open('source/ui/PedalboardGrid.cpp', 'w') as f:
    f.writelines(new_lines)

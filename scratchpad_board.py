import sys
with open('source/ui/BoardComponent.cpp', 'r') as f:
    lines = f.read()

# Replace empty drag functions
to_replace = """// Drag & Drop
bool BoardComponent::isInterestedInDragSource (const SourceDetails& details)
{
    auto desc = details.description.toString();
    return desc.startsWith ("pedal:") || desc.startsWith ("active_pedal:");
}

void BoardComponent::itemDragEnter (const SourceDetails& details)
{
    // ...
}

void BoardComponent::itemDragMove (const SourceDetails& details)
{
    // ...
}

void BoardComponent::itemDragExit (const SourceDetails& details)
{
    // ...
}

void BoardComponent::itemDropped (const SourceDetails& details)
{
    // ...
}"""

replacement = """// Drag & Drop
bool BoardComponent::isInterestedInDragSource (const SourceDetails& details)
{
    auto desc = details.description.toString();
    return desc.startsWith ("pedal:") || desc.startsWith ("active_pedal:");
}

void BoardComponent::itemDragEnter (const SourceDetails& details)
{
    // Optional: Add drag preview logic inside BoardComponent
}

void BoardComponent::itemDragMove (const SourceDetails& details)
{
}

void BoardComponent::itemDragExit (const SourceDetails& details)
{
}

void BoardComponent::itemDropped (const SourceDetails& details)
{
    auto desc = details.description.toString();
    juce::StringArray tok;
    tok.addTokens (desc, ":", "");
    if (tok.size() < 2) return;

    auto localPos = details.localPosition;
    auto gp = pixelToGrid (localPos.x, localPos.y);

    if (tok[0] == "pedal")
    {
        auto type = tok[1];
        for (auto& info : getFactoryPedals())
        {
            if (info.name == type)
            {
                auto processor = info.factory();
                auto nodeId = engine.addPedal (std::move (processor),
                                                config.id, config.activePage,
                                                gp.x, gp.y,
                                                info.gridW, info.gridH);
                                                
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
                            if (auto* dsp = proc->getDSPGraph())
                            {
                                inst->design = std::make_shared<PedalDesign>();
                                inst->design->name = info.name;
                                inst->design->category = info.category;
                                inst->design->chassisW = info.gridW * 100.0f;
                                inst->design->chassisH = info.gridH * 100.0f;
                                inst->design->chassisColour = info.colour;

                                // Serialize the factory graph so we can edit it later
                                inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                                
                                float x = 20, y = 40;
                                for (auto* param : proc->getParameters())
                                {
                                    if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
                                    {
                                        PedalDesign::Control ctrl;
                                        ctrl.type = "knob";
                                        ctrl.label = pf->name;
                                        ctrl.controlID = "knob_" + juce::String (inst->design->controls.size() + 1);
                                        ctrl.x = x;
                                        ctrl.y = y;
                                        ctrl.width = 40;
                                        ctrl.height = 40;
                                        inst->design->controls.push_back (ctrl);

                                        PedalDesign::Mapping m;
                                        m.controlID = ctrl.controlID;
                                        m.nodeParam = pf->paramID;
                                        inst->design->mappings.push_back (m);

                                        x += 50;
                                        if (x > 150) { x = 20; y += 60; }
                                    }
                                }
                            }
                        }
                    }
                }
                break;
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
            inst->gridX = gp.x;
            inst->gridY = gp.y;
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
"""
lines = lines.replace(to_replace, replacement)

with open('source/ui/BoardComponent.cpp', 'w') as f:
    f.write(lines)

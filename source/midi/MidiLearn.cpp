#include "MidiLearn.h"
#include "../engine/AudioGraphEngine.h"

//==============================================================================
MidiLearnManager::MidiLearnManager (AudioGraphEngine& eng)
    : targetEngine (eng)
{
}

//==============================================================================
void MidiLearnManager::startLearning (const juce::String& paramId)
{
    learningParamId = paramId;
    learning = true;
}

void MidiLearnManager::cancelLearning()
{
    learning = false;
    learningParamId = {};
}

void MidiLearnManager::removeMapping (const juce::String& paramId)
{
    auto it = mappings.find (paramId);
    if (it != mappings.end())
    {
        // Remove reverse lookup
        int key = it->second.ccNumber * 17 + it->second.channel;
        ccToParam.erase (key);
        mappings.erase (it);
    }
}

void MidiLearnManager::clearAllMappings()
{
    mappings.clear();
    ccToParam.clear();
}

void MidiLearnManager::setMapping (const juce::String& paramId, int ccNumber, int channel)
{
    if (ccNumber < 0 || ccNumber > 127) return;
    if (channel < 0 || channel > 16)    return;

    // Clear any prior mapping for this param.
    removeMapping (paramId);

    // If something else already owns this CC/channel slot, evict it so the
    // user's manual edit takes precedence (and doesn't leave a ghost).
    const int key = ccNumber * 17 + channel;
    auto existing = ccToParam.find (key);
    if (existing != ccToParam.end() && existing->second != paramId)
    {
        mappings.erase (existing->second);
        // ccToParam[key] will be overwritten below
    }

    MidiMapping m;
    m.ccNumber = ccNumber;
    m.channel  = channel;
    m.paramId  = paramId;
    mappings[paramId] = m;
    ccToParam[key] = paramId;
}

int MidiLearnManager::getMappedCC (const juce::String& paramId) const
{
    auto it = mappings.find (paramId);
    return it != mappings.end() ? it->second.ccNumber : -1;
}

//==============================================================================
void MidiLearnManager::processMidi (const juce::MidiBuffer& midiBuffer)
{
    for (const auto metadata : midiBuffer)
    {
        auto msg = metadata.getMessage();

        if (msg.isController())
        {
            int cc      = msg.getControllerNumber();
            int channel = msg.getChannel();
            float value = msg.getControllerValue() / 127.0f;

            if (learning)
            {
                // Capture this CC as the mapping for the learning parameter
                MidiMapping mapping;
                mapping.ccNumber = cc;
                mapping.channel  = 0; // Omni
                mapping.paramId  = learningParamId;

                // Remove any existing mapping for the param we're (re)learning.
                removeMapping (learningParamId);

                // If THIS CC was already bound to some OTHER param, also
                // clear that old binding so we don't leave an orphan.
                // Previously this case left the old mapping in `mappings`
                // even though `ccToParam` would be overwritten — the user
                // saw a "re-learn" that silently broke the other binding.
                const int key = cc * 17; // channel 0 omni
                auto existing = ccToParam.find (key);
                if (existing != ccToParam.end() && existing->second != learningParamId)
                    mappings.erase (existing->second);

                mappings[learningParamId] = mapping;
                ccToParam[key] = learningParamId;

                learning = false;
                learningParamId = {};
            }
            else
            {
                // Apply mapped CC values
                applyCC (cc, channel, value);
            }
        }
    }
}

void MidiLearnManager::applyCC (int ccNumber, int /*channel*/, float value)
{
    // Check omni mapping first (channel 0)
    int key = ccNumber * 17;
    auto it = ccToParam.find (key);

    if (it != ccToParam.end())
    {
        auto mapIt = mappings.find (it->second);
        if (mapIt == mappings.end()) return;
        
        auto& mapping = mapIt->second;

        // The stored paramId uses the format "nodeUID:parameterID" (e.g. "1027:3_gain")
        // Parse the target node UID and actual parameter ID
        auto fullId = it->second;
        juce::AudioProcessorGraph::NodeID targetNodeId;
        juce::String actualParamId;

        if (fullId.contains (":"))
        {
            targetNodeId.uid = static_cast<juce::uint32> (fullId.upToFirstOccurrenceOf (":", false, false).getIntValue());
            actualParamId = fullId.fromFirstOccurrenceOf (":", false, false);
        }
        else
        {
            // Legacy mapping without instance prefix — scan all pedals
            actualParamId = fullId;
        }

        auto& graphEngine = targetEngine;

        for (auto& instance : graphEngine.getPedalInstances())
        {
            // If we have a target node, skip non-matching instances
            if (targetNodeId.uid != 0 && instance.nodeID != targetNodeId)
                continue;

            auto* node = graphEngine.getGraph().getNodeForId (instance.nodeID);
            if (node == nullptr) continue;

            auto* proc = node->getProcessor();
            for (auto* param : proc->getParameters())
            {
                if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    if (rangedParam->getParameterID() == actualParamId)
                    {
                        float currentVirtual = rangedParam->getValue();
                        
                        // If the virtual parameter was moved by GUI, unlatch!
                        if (mapping.isLatched && mapping.lastVirtualValue >= 0.0f)
                        {
                            if (std::abs(currentVirtual - mapping.lastVirtualValue) > 0.001f)
                                mapping.isLatched = false;
                        }
                        
                        // Pick-up logic
                        if (!mapping.isLatched)
                        {
                            if (mapping.lastPhysicalValue >= 0.0f)
                            {
                                // Check if we crossed the virtual value, or got close enough
                                bool crossedUp = (mapping.lastPhysicalValue <= currentVirtual && value >= currentVirtual);
                                bool crossedDown = (mapping.lastPhysicalValue >= currentVirtual && value <= currentVirtual);
                                bool closeEnough = std::abs (value - currentVirtual) < 0.05f;

                                if (crossedUp || crossedDown || closeEnough)
                                    mapping.isLatched = true;
                            }
                            mapping.lastPhysicalValue = value;
                        }

                        if (mapping.isLatched)
                        {
                            rangedParam->setValueNotifyingHost (value);
                            mapping.lastVirtualValue = value;
                        }
                        
                        return;
                    }
                }
            }
        }
    }
}

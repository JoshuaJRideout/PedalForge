#include "MidiLearn.h"
#include "../PluginProcessor.h"

//==============================================================================
MidiLearnManager::MidiLearnManager (PedalForgeProcessor& proc)
    : processor (proc)
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

                // Remove any existing mapping for this param
                removeMapping (learningParamId);

                mappings[learningParamId] = mapping;
                ccToParam[cc * 17] = learningParamId; // Channel 0 = omni

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
        // Find the parameter in the graph and set it
        auto& graphEngine = processor.getGraphEngine();

        for (auto& instance : graphEngine.getPedalInstances())
        {
            auto* node = graphEngine.getGraph().getNodeForId (instance.nodeID);
            if (node == nullptr) continue;

            auto* proc = node->getProcessor();
            for (auto* param : proc->getParameters())
            {
                if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    if (rangedParam->getParameterID() == it->second)
                    {
                        rangedParam->setValueNotifyingHost (value);
                        return;
                    }
                }
            }
        }
    }
}

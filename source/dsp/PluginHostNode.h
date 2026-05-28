#pragma once

#include "DSPNode.h"
#include "../util/AppPaths.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PluginHostNode : public DSPNode
{
public:
    PluginHostNode() : DSPNode ("plugin_host", "VST/AU Host")
    {
        addInput ("in_l");
        addInput ("in_r");
        addOutput ("out_l");
        addOutput ("out_r");
    }

    static juce::AudioPluginFormatManager& getFormatManager()
    {
        static juce::AudioPluginFormatManager formatManager;
        static bool initialized = false;
        if (!initialized)
        {
            formatManager.addDefaultFormats();
            initialized = true;
        }
        return formatManager;
    }

    static juce::KnownPluginList& getKnownPluginList()
    {
        static juce::KnownPluginList pluginList;
        static bool initialized = false;
        if (!initialized)
        {
            juce::File listFile = pf::paths::getRoot().getChildFile ("KnownPlugins.xml");
            if (auto xml = juce::parseXML(listFile))
                pluginList.recreateFromXml(*xml);
            initialized = true;
        }
        return pluginList;
    }

    static void saveKnownPluginList()
    {
        juce::File listFile = pf::paths::getRoot().getChildFile ("KnownPlugins.xml");
        listFile.getParentDirectory().createDirectory();
        if (auto xml = getKnownPluginList().createXml())
            xml->writeTo(listFile);
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);

        // Pre-allocate tempBuffer large enough for any reasonable plugin.
        // This avoids any heap allocation on the audio thread.
        tempBuffer.setSize (maxTempChannels, maxBlockSize);

        juce::SpinLock::ScopedLockType sl (processLock);
        if (pluginInstance)
            pluginInstance->prepareToPlay (sampleRate, maxBlockSize);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        // Use try_lock so the audio thread NEVER blocks.
        // If the message thread is swapping the plugin, we output silence for one block instead of glitching.
        const juce::SpinLock::ScopedTryLockType sl (processLock);

        if (!sl.isLocked() || !pluginInstance)
        {
            // Pass through (or silence) if no plugin loaded or lock is held by message thread
            for (int i = 0; i < numOut; ++i)
            {
                if (out[i])
                {
                    if (i < numIn && in[i])
                        std::copy (in[i], in[i] + n, out[i]);
                    else
                        std::fill (out[i], out[i] + n, 0.0f);
                }
            }
            return;
        }

        int pluginIns  = pluginInstance->getTotalNumInputChannels();
        int pluginOuts = pluginInstance->getTotalNumOutputChannels();
        int maxCh = juce::jmax (pluginIns, pluginOuts);

        // Safety: if a plugin needs more channels than we pre-allocated, cap it.
        maxCh = juce::jmin (maxCh, maxTempChannels);
        pluginIns  = juce::jmin (pluginIns, maxTempChannels);
        pluginOuts = juce::jmin (pluginOuts, maxTempChannels);

        for (int i = 0; i < pluginIns; ++i)
        {
            if (i < numIn && in[i])
                std::copy (in[i], in[i] + n, tempBuffer.getWritePointer(i));
            else
                tempBuffer.clear (i, 0, n);
        }

        for (int i = pluginIns; i < maxCh; ++i)
            tempBuffer.clear (i, 0, n);

        juce::MidiBuffer midi;
        if (midiBuffer) midi = *midiBuffer;

        juce::AudioBuffer<float> aliasBuffer (tempBuffer.getArrayOfWritePointers(),
                                              maxCh,
                                              n);

        pluginInstance->processBlock (aliasBuffer, midi);

        if (midiBuffer) *midiBuffer = midi;

        // Copy plugin outputs to DSP graph outputs
        for (int i = 0; i < pluginOuts; ++i)
        {
            if (i < numOut && out[i])
                std::copy (tempBuffer.getReadPointer(i), tempBuffer.getReadPointer(i) + n, out[i]);
        }

        // Handle channel mismatch: mono plugin → stereo host
        if (pluginOuts == 1 && numOut > 1 && out[0])
        {
            for (int i = 1; i < numOut; ++i)
            {
                if (out[i])
                    std::copy (out[0], out[0] + n, out[i]);
            }
        }
        else
        {
            // Silence any unused host output channels
            for (int i = pluginOuts; i < numOut; ++i)
            {
                if (out[i])
                    std::fill (out[i], out[i] + n, 0.0f);
            }
        }
    }

    void reset() override
    {
        juce::SpinLock::ScopedLockType sl (processLock);
        if (pluginInstance)
            pluginInstance->reset();
    }

    void setFilePath (const juce::String& path) override
    {
        if (path == currentPath) return;
        currentPath = path;

        juce::PluginDescription desc;
        bool found = false;
        for (const auto& type : getKnownPluginList().getTypes())
        {
            if (type.fileOrIdentifier == path)
            {
                desc = type;
                found = true;
                break;
            }
        }

        if (!found)
            desc.fileOrIdentifier = path;

        getFormatManager().createPluginInstanceAsync (
            desc, sr, blockSize,
            [this] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& errorMsg)
            {
                if (instance)
                {
                    instance->prepareToPlay (sr, blockSize);
                    std::unique_ptr<juce::AudioPluginInstance> oldInstance;
                    {
                        juce::SpinLock::ScopedLockType sl (processLock);
                        oldInstance = std::move (pluginInstance);
                        pluginInstance = std::move (instance);
                    }
                    // oldInstance is deleted here, outside the lock, on the message thread
                }
                else
                {
                    juce::Logger::writeToLog ("Failed to load plugin: " + errorMsg);
                }
            });
    }

    juce::String getFilePath() const override { return currentPath; }

    juce::AudioPluginInstance* getPluginInstance() const { return pluginInstance.get(); }

private:
    static constexpr int maxTempChannels = 8;  // Pre-allocate for up to 8 channels (covers stereo, surround, etc.)
    juce::SpinLock processLock;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::String currentPath;
    juce::AudioBuffer<float> tempBuffer;
};

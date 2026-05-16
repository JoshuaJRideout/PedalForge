#pragma once

#include "DSPNode.h"
#include <NAM/get_dsp.h>
#include <memory>
#include <atomic>
#include <vector>
#include <NAM/model_config.h>
#include <NAM/wavenet/model.h>
#include <NAM/lstm.h>
#include <NAM/convnet.h>
#include <NAM/dsp.h>

class NAMNode : public DSPNode
{
public:
    NAMNode(const juce::String& id, const juce::String& name)
        : DSPNode(id, name)
    {
        addInput("in");
        addOutput("out");
        addParam("gain", "Gain", -24.0f, 24.0f, 0.0f);
        addParam("out_level", "Out Level", -24.0f, 24.0f, 0.0f);

        // Force explicit registration of NAM architectures since static initializers
        // inside the statically linked library might be stripped by the linker.
        static bool namParsersRegistered = false;
        if (!namParsersRegistered) {
            try { nam::ConfigParserRegistry::instance().registerParser("WaveNet", nam::wavenet::create_config); } catch (...) {}
            try { nam::ConfigParserRegistry::instance().registerParser("LSTM", nam::lstm::create_config); } catch (...) {}
            try { nam::ConfigParserRegistry::instance().registerParser("ConvNet", nam::convnet::create_config); } catch (...) {}
            try { nam::ConfigParserRegistry::instance().registerParser("Linear", nam::linear::create_config); } catch (...) {}
            namParsersRegistered = true;
        }
    }

    ~NAMNode() override
    {
        // Clean up the active model
        if (activeModel != nullptr)
            delete activeModel;
        // Clean up any staged model
        delete stagedModel.exchange(nullptr);
        // Clean up any trashed model
        delete trashModel.exchange(nullptr);
    }

    void setFilePath(const juce::String& path) override
    {
        DSPNode::setFilePath(path);
        loadModel(path);
    }

    void loadModel(const juce::String& path)
    {
        // Clean up any old models the audio thread discarded
        collectTrash();

        juce::Logger::writeToLog("NAMNode: Attempting to load model from " + path);
        try {
            auto model = nam::get_dsp(std::filesystem::path(path.toStdString()));
            if (model) {
                juce::Logger::writeToLog("NAMNode: Successfully parsed model. Prewarming...");
                model->ResetAndPrewarm(lastSampleRate.load(), lastBlockSize.load());
                juce::Logger::writeToLog("NAMNode: Prewarm complete. Swapping into audio thread.");
            } else {
                juce::Logger::writeToLog("NAMNode: get_dsp returned nullptr for " + path);
                return;
            }

            // Lock-free swap: place the new model into the staging pointer.
            // The audio thread will pick it up on its next process() call.
            nam::DSP* newModel = model.release();
            nam::DSP* old = stagedModel.exchange(newModel);
            // If there was already a staged model nobody picked up, delete it here
            // (we're on the message thread, so delete is safe)
            if (old != nullptr)
                delete old;

        } catch (const std::exception& e) {
            juce::Logger::writeToLog("NAMNode: Exception loading model: " + juce::String(e.what()));
        } catch (...) {
            juce::Logger::writeToLog("NAMNode: Unknown exception loading model!");
        }
    }

    void prepare(double sampleRate, int samplesPerBlock) override
    {
        DSPNode::prepare(sampleRate, samplesPerBlock);
        lastSampleRate.store(sampleRate);
        lastBlockSize.store(samplesPerBlock);
        inBuffer.resize((size_t)samplesPerBlock, 0.0f);
        outBuffer.resize((size_t)samplesPerBlock, 0.0f);

        // If we already have a model, reset it for the new sample rate
        // (prepare is called from the message thread before audio starts)
        if (activeModel != nullptr) {
            try {
                activeModel->ResetAndPrewarm(sampleRate, samplesPerBlock);
            } catch (...) {
                delete activeModel;
                activeModel = nullptr;
            }
        }
    }

    void process(const float** in, int numIn, float** out, int numOut, int n) override
    {
        // Check for a newly staged model (lock-free pointer swap)
        nam::DSP* staged = stagedModel.exchange(nullptr);
        if (staged != nullptr)
        {
            // Swap in the new model; stash the old one for deferred deletion
            nam::DSP* old = activeModel;
            activeModel = staged;
            // Stash old model for the message thread to clean up later.
            // If there's already one waiting, it leaks — acceptable tradeoff
            // vs calling delete/free on the RT thread.
            if (old != nullptr)
                trashModel.store(old);
        }

        if (activeModel == nullptr || numIn == 0 || numOut == 0)
        {
            // Pass through or silence
            if (numOut > 0 && numIn > 0)
                std::copy(in[0], in[0] + n, out[0]);
            else if (numOut > 0)
                std::fill(out[0], out[0] + n, 0.0f);
            return;
        }

        // Apply input gain
        float inGain = juce::Decibels::decibelsToGain(getParam("gain")->get());
        for (int i = 0; i < n; ++i)
            inBuffer[(size_t)i] = in[0][i] * inGain;

        // Run the NAM model — wrapped in try/catch so a model error
        // never crashes the audio thread.
        float* inPtrs[] = { inBuffer.data() };
        float* outPtrs[] = { outBuffer.data() };

        try {
            activeModel->process(inPtrs, outPtrs, n);
        } catch (...) {
            // Model failed during processing — pass through dry signal
            std::copy(in[0], in[0] + n, out[0]);
            // Stash the broken model for deferred cleanup
            nam::DSP* broken = activeModel;
            activeModel = nullptr;
            trashModel.store(broken);
            return;
        }

        // Apply output gain and copy to output
        float outGain = juce::Decibels::decibelsToGain(getParam("out_level")->get());
        for (int i = 0; i < n; ++i)
            out[0][i] = outBuffer[(size_t)i] * outGain;
    }

private:
    // Lock-free model management:
    // - stagedModel: set by the UI thread when a new model is ready
    // - activeModel: owned by the audio thread, used for processing
    // - trashModel: old models stashed by the audio thread for deferred deletion
    std::atomic<nam::DSP*> stagedModel{nullptr};
    nam::DSP* activeModel = nullptr; // Only touched by audio thread
    std::atomic<nam::DSP*> trashModel{nullptr};

    std::vector<float> inBuffer;
    std::vector<float> outBuffer;
    std::atomic<double> lastSampleRate{44100.0};
    std::atomic<int> lastBlockSize{512};

    /** Call from message thread to clean up any models the audio thread discarded. */
    void collectTrash()
    {
        nam::DSP* trash = trashModel.exchange(nullptr);
        if (trash != nullptr)
            delete trash;
    }
};

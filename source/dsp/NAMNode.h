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

        juce::Logger::writeToLog("NAMNode: Attempting to load model from: " + path);
        try {
            auto model = nam::get_dsp(std::filesystem::path(path.toStdString()));
            if (model) {
                juce::Logger::writeToLog("NAMNode: Successfully parsed model.");
                juce::Logger::writeToLog("  Expected sample rate: " + juce::String(model->GetExpectedSampleRate()) + " Hz");
                juce::Logger::writeToLog("  Number of inputs:     " + juce::String(model->NumInputChannels()));
                juce::Logger::writeToLog("  Number of outputs:    " + juce::String(model->NumOutputChannels()));
                
                if (model->HasInputLevel())
                    juce::Logger::writeToLog("  Input level:          " + juce::String(model->GetInputLevel()) + " dBu");
                else
                    juce::Logger::writeToLog("  Input level:          Not defined in model metadata");
                    
                if (model->HasLoudness())
                    juce::Logger::writeToLog("  Loudness:             " + juce::String(model->GetLoudness()) + " dB");
                else
                    juce::Logger::writeToLog("  Loudness:             Not defined in model metadata");
                    
                if (model->HasOutputLevel())
                    juce::Logger::writeToLog("  Output level:         " + juce::String(model->GetOutputLevel()) + " dBu");
                else
                    juce::Logger::writeToLog("  Output level:         Not defined in model metadata");

                double currentSR = lastSampleRate.load();
                int currentBlock = lastBlockSize.load();
                juce::Logger::writeToLog("NAMNode: Resetting and prewarming with active SR=" + juce::String(currentSR) + ", BlockSize=" + juce::String(currentBlock));
                
                model->ResetAndPrewarm(currentSR, currentBlock);
                juce::Logger::writeToLog("NAMNode: Prewarm complete. Swapping new model into audio thread.");
            } else {
                juce::Logger::writeToLog("NAMNode: get_dsp returned nullptr for path: " + path);
                return;
            }

            // Lock-free swap: place the new model into the staging pointer.
            // The audio thread will pick it up on its next process() call.
            nam::DSP* newModel = model.release();
            nam::DSP* old = stagedModel.exchange(newModel);
            // If there was already a staged model nobody picked up, delete it here
            // (we're on the message thread, so delete is safe)
            if (old != nullptr)
            {
                juce::Logger::writeToLog("NAMNode: Warning - Discarding an existing staged model that was never processed by the audio thread.");
                delete old;
            }

        } catch (const std::exception& e) {
            juce::Logger::writeToLog("NAMNode: C++ std::exception loading model: " + juce::String(e.what()));
        } catch (...) {
            juce::Logger::writeToLog("NAMNode: Unknown catch-all exception loading model!");
        }
    }

    void prepare(double sampleRate, int samplesPerBlock) override
    {
        DSPNode::prepare(sampleRate, samplesPerBlock);
        lastSampleRate.store(sampleRate);
        lastBlockSize.store(samplesPerBlock);

        // Dynamically resize internal process buffers to a safe size
        int safeBlockSize = std::max(samplesPerBlock, 4096);
        inBuffer.resize((size_t)safeBlockSize, 0.0f);
        outBuffer.resize((size_t)safeBlockSize, 0.0f);
        dummyBuffer.resize((size_t)safeBlockSize, 0.0f);

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
            
            // Reset NaN logging so we can log at least once for the new model
            loggedNanForCurrentModel.store(false);
            
            // Stash old model for the message thread to clean up later.
            if (old != nullptr)
                trashModel.store(old);

            juce::Logger::writeToLog("NAMNode::process - Audio thread swapped to new active model at address: " + juce::String((uint64_t)activeModel));
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

        // Ensure buffers are large enough for the current block size
        if (inBuffer.size() < (size_t)n)
            inBuffer.resize((size_t)n, 0.0f);
        if (outBuffer.size() < (size_t)n)
            outBuffer.resize((size_t)n, 0.0f);
        if (dummyBuffer.size() < (size_t)n)
            dummyBuffer.resize((size_t)n, 0.0f);

        // Apply input gain - null-check the parameter
        float inGain = 1.0f;
        if (auto* pGain = getParam("gain"))
        {
            inGain = juce::Decibels::decibelsToGain(pGain->get());
        }
        else
        {
            static std::atomic<bool> warnedGain{false};
            if (!warnedGain.exchange(true))
                juce::Logger::writeToLog("NAMNode::process - Warning: 'gain' parameter is nullptr!");
        }

        for (int i = 0; i < n; ++i)
            inBuffer[(size_t)i] = in[0][i] * inGain;

        // Run the NAM model — wrapped in try/catch so a model error
        // never crashes the audio thread.
        int numModelInputs = activeModel->NumInputChannels();
        int numModelOutputs = activeModel->NumOutputChannels();

        std::vector<float*> modelInPtrs((size_t)numModelInputs, nullptr);
        for (int i = 0; i < numModelInputs; ++i)
            modelInPtrs[(size_t)i] = (i == 0) ? inBuffer.data() : dummyBuffer.data();

        std::vector<float*> modelOutPtrs((size_t)numModelOutputs, nullptr);
        for (int i = 0; i < numModelOutputs; ++i)
            modelOutPtrs[(size_t)i] = (i == 0) ? outBuffer.data() : dummyBuffer.data();

        try {
            activeModel->process(modelInPtrs.data(), modelOutPtrs.data(), n);
        } catch (const std::exception& e) {
            juce::Logger::writeToLog("NAMNode::process - Standard C++ exception during activeModel->process: " + juce::String(e.what()));
            // Model failed during processing — pass through dry signal
            std::copy(in[0], in[0] + n, out[0]);
            // Stash the broken model for deferred cleanup
            nam::DSP* broken = activeModel;
            activeModel = nullptr;
            trashModel.store(broken);
            return;
        } catch (...) {
            juce::Logger::writeToLog("NAMNode::process - Unknown exception during activeModel->process!");
            // Model failed during processing — pass through dry signal
            std::copy(in[0], in[0] + n, out[0]);
            // Stash the broken model for deferred cleanup
            nam::DSP* broken = activeModel;
            activeModel = nullptr;
            trashModel.store(broken);
            return;
        }

        // Apply output gain and copy to output, adding NaN/Infinity checks
        float outGain = 1.0f;
        if (auto* pOutLevel = getParam("out_level"))
        {
            outGain = juce::Decibels::decibelsToGain(pOutLevel->get());
        }
        else
        {
            static std::atomic<bool> warnedOutLevel{false};
            if (!warnedOutLevel.exchange(true))
                juce::Logger::writeToLog("NAMNode::process - Warning: 'out_level' parameter is nullptr!");
        }

        bool hasNaN = false;
        int firstNaNIndex = -1;
        float nanVal = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float val = outBuffer[(size_t)i];
            if (std::isnan(val) || std::isinf(val))
            {
                hasNaN = true;
                firstNaNIndex = i;
                nanVal = val;
                break;
            }
        }

        if (hasNaN)
        {
            if (!loggedNanForCurrentModel.exchange(true))
            {
                juce::Logger::writeToLog("NAMNode::process - NaN/Infinity detected at sample index " 
                                         + juce::String(firstNaNIndex) + " with value: " + juce::String(nanVal) 
                                         + ". Falling back to dry pass-through.");
            }
            // Model produced invalid output — fallback to dry pass-through
            std::copy(in[0], in[0] + n, out[0]);
            return;
        }

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
    std::vector<float> dummyBuffer;
    std::atomic<double> lastSampleRate{44100.0};
    std::atomic<int> lastBlockSize{512};
    std::atomic<bool> loggedNanForCurrentModel{false};

    /** Call from message thread to clean up any models the audio thread discarded. */
    void collectTrash()
    {
        nam::DSP* trash = trashModel.exchange(nullptr);
        if (trash != nullptr)
            delete trash;
    }
};

#include "source/dsp/NAMNode.h"
#include <iostream>

class ConsoleLogger : public juce::Logger
{
protected:
    void logMessage (const juce::String& message) override
    {
        std::cout << "[LOG] " << message.toStdString() << std::endl;
    }
};

int main()
{
    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    std::cout << "Creating NAMNode..." << std::endl;
    NAMNode node ("nam", "NAM Amp");

    std::cout << "Preparing node with 128 samples..." << std::endl;
    node.prepare (44100.0, 128);

    std::cout << "Setting file path to model..." << std::endl;
    node.setFilePath ("/Users/joshua/Downloads/NAM_models-main/Tudor N [Suhr RL] Input@HI_Dep@6_Gir@6.5_Pres@8.5_Bas@6.5_Mid@4_Tre@10_Mas@8_Gain@5_CHAR@mid_ERA@rgt_EDG@mid_FEL@up ESR_0.0109_normalized-6dB.nam");

    // Call process twice to simulate real-time loop where staged model is swapped in
    std::cout << "Processing first block of size 128..." << std::endl;
    float inData[128];
    for (int i = 0; i < 128; ++i)
        inData[i] = std::sin (2.0f * 3.14159f * 1000.0f * (float)i / 44100.0f) * 0.5f;

    const float* inPtrs[] = { inData };
    float outData[128] = { 0.0f };
    float* outPtrs[] = { outData };

    node.process (inPtrs, 1, outPtrs, 1, 128);

    std::cout << "First 5 processed output samples of block 1:" << std::endl;
    for (int i = 0; i < 5; ++i)
        std::cout << "  Sample " << i << ": " << outData[i] << std::endl;

    std::cout << "Processing second block..." << std::endl;
    node.process (inPtrs, 1, outPtrs, 1, 128);

    std::cout << "First 5 processed output samples of block 2:" << std::endl;
    for (int i = 0; i < 5; ++i)
        std::cout << "  Sample " << i << ": " << outData[i] << std::endl;

    juce::Logger::setCurrentLogger (nullptr);
    return 0;
}

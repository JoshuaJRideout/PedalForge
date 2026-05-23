#include "TuringRenderer.h"
#include "PedalPainter.h"

TuringRenderer::TuringRenderer (AudioGraphEngine& eng) : engine(eng)
{
    // Run at 15 FPS
    startTimerHz(15);
}

TuringRenderer::~TuringRenderer()
{
}

void TuringRenderer::timerCallback()
{
    BoardConfig* turingBoard = nullptr;
    for (auto& b : engine.getBoards())
    {
        if (b.assignToTuring)
        {
            turingBoard = &b;
            break;
        }
    }
    
    if (!turingBoard) return;
    
    std::vector<const PedalInstance*> boardPedals;
    for (const auto& inst : engine.getPedalInstances())
    {
        if (inst.onBoard && inst.boardId == turingBoard->id)
            boardPedals.push_back(&inst);
    }
            
    if (boardPedals.empty()) return;
    
    std::sort(boardPedals.begin(), boardPedals.end(), [](const PedalInstance* a, const PedalInstance* b) {
        if (a->pageIndex != b->pageIndex) return a->pageIndex < b->pageIndex;
        if (std::abs(a->boardY - b->boardY) > 1.0f) return a->boardY < b->boardY;
        return a->boardX < b->boardX;
    });
    
    if (turingBoard->turingPedalIndex >= boardPedals.size()) turingBoard->turingPedalIndex = 0;
    if (turingBoard->turingPedalIndex < 0) turingBoard->turingPedalIndex = boardPedals.size() - 1;
    
    const auto* activePedal = boardPedals[turingBoard->turingPedalIndex];
    
    juce::String currentHash;
    for (auto& val : activePedal->controlValues)
        currentHash += juce::String(val.second) + "_";
    currentHash += activePedal->bypassed ? "B" : "A";
    
    if (lastPedalNodeId == activePedal->nodeID.uid && 
        lastValuesHash == currentHash && 
        lastOrientation == turingBoard->turingHorizontal)
    {
        return;
    }
        
    lastPedalNodeId = activePedal->nodeID.uid;
    lastValuesHash = currentHash;
    lastOrientation = turingBoard->turingHorizontal;
    
    int w = turingBoard->turingHorizontal ? 480 : 320;
    int h = turingBoard->turingHorizontal ? 320 : 480;
    
    juce::Image img (juce::Image::RGB, w, h, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colours::black);
    
    float pw = activePedal->boardW * 1.5f;
    float ph = activePedal->boardH * 1.5f;
    
    float scale = juce::jmin (w / pw, h / ph) * 0.85f;
    pw *= scale;
    ph *= scale;
    
    // Move up slightly to make room for text
    juce::Rectangle<float> bounds ((w - pw) * 0.5f, (h - ph) * 0.5f - 15.0f, pw, ph);
    
    PedalPainter::paintDesign (g, bounds, activePedal->design.get(), 
                               activePedal->controlValues, activePedal->controlTexts, 
                               {}, activePedal->bypassed, 1.0f);
                               
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawText (activePedal->name, 0, h - 40, w, 40, juce::Justification::centred);
    
    // Add page indicator
    g.setFont (14.0f);
    g.setColour (juce::Colours::grey);
    juce::String pageText = "Page " + juce::String(activePedal->pageIndex + 1);
    g.drawText (pageText, 10, h - 30, w, 30, juce::Justification::bottomLeft);
    
    juce::String indexText = juce::String(turingBoard->turingPedalIndex + 1) + "/" + juce::String(boardPedals.size());
    g.drawText (indexText, -10, h - 30, w, 30, juce::Justification::bottomRight);
    
    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pedalforge_turing.png");
    juce::FileOutputStream out (tempFile);
    out.setPosition(0);
    out.truncate();
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
}

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "../pedals/PedalRegistry.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/PedalDesign.h"
#include "NotesOverlay.h"

//==============================================================================
/**
 * Node-graph style routing editor for the Route tab.
 *
 * Displays pedals as nodes with typed ports (Audio, MIDI, Expression).
 * Users wire connections by dragging between ports.
 * Changes immediately update the AudioGraphEngine.
 *
 * Layout: scrollable/zoomable canvas (left) + properties panel (right).
 */
class RoutingGraphEditor : public juce::Component, public juce::Timer
{
public:
    explicit RoutingGraphEditor (AudioGraphEngine& engine);
    ~RoutingGraphEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Rebuild the visual graph from the engine's current state. */
    void syncFromEngine();

    void timerCallback() override;

    /** Callback fired when a pedal is selected/deselected. */
    std::function<void(PedalInstance*)> onPedalSelected;

private:
    //==========================================================================
    // Port type determines cable colour and compatibility
    //==========================================================================
    enum class PortType { AudioStereo, MIDI, Expression };

    struct RoutingPort
    {
        juce::String name;
        PortType type;
        bool isOutput;
        int engineChannel = 0;      // maps to the AudioProcessorGraph channel index (audio only)
        juce::String routingPortId; // for MIDI/Expression ports — matches PedalDesign::RoutingPort::id
        float glowLevel = 0.0f;
    };

    //==========================================================================
    // Visual representation of a node (pedal, I/O endpoint, etc.)
    //==========================================================================
    struct RoutingNode
    {
        AudioGraphEngine::NodeID engineNodeId {};
        juce::String name;
        float x = 0, y = 0;
        float width = 180, height = 80;
        bool selected = false;
        bool isIONode = false;    // true for Audio In/Out, MIDI In/Out
        bool isHwMidi = false;    // true for physical hardware MIDI device nodes
        juce::String hwMidiName;  // the system device name
        bool hwMidiIsInput = true;

        std::vector<RoutingPort> inputs;
        std::vector<RoutingPort> outputs;
    };

    struct RoutingConnection
    {
        int sourceNodeIdx = -1, sourcePortIdx = -1;
        int destNodeIdx = -1, destPortIdx = -1;
        float glowLevel = 0.0f;
    };

    struct PortHit
    {
        int nodeIdx = -1;
        bool isOutput = false;
        int portIdx = -1;
    };

    //==========================================================================
    // Canvas — handles drawing and interaction
    //==========================================================================
    class RoutingCanvas : public juce::Component,
                          public juce::DragAndDropTarget
    {
    public:
        RoutingCanvas (RoutingGraphEditor& owner);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

        // DragAndDropTarget
        bool isInterestedInDragSource (const SourceDetails& details) override;
        void itemDropped (const SourceDetails& details) override;

        std::function<void(int)> onNodeSelected;

    private:
        RoutingGraphEditor& editor;

        float scale = 1.0f, panX = 0.0f, panY = 0.0f;
        juce::Point<float> dragStartPan;
        int draggingNodeIdx = -1;
        juce::Point<float> nodeDragOffset;
        bool draggingWire = false;
        bool isPanning = false;
        PortHit wireStart;
        float wireEndX = 0, wireEndY = 0;

        juce::Point<float> screenToCanvas (float sx, float sy) const;
        PortHit hitTestPort (juce::Point<float> cp) const;
        int hitTestNode (juce::Point<float> cp) const;
        int hitTestConnection (juce::Point<float> cp) const;

        void drawNode (juce::Graphics& g, int idx, const RoutingNode& node) const;
        void drawConnection (juce::Graphics& g, const RoutingConnection& conn) const;
        void drawWirePreview (juce::Graphics& g) const;
    };

    //==========================================================================
    // Properties panel (right side)
    //==========================================================================
    class PropertiesPanel : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
        void showNode (const RoutingNode* node);
        void clearSelection();

        static juce::Colour getPortColour (PortType type);

    private:
        const RoutingNode* currentNode = nullptr;
    };

    //==========================================================================
    // Data
    //==========================================================================
    AudioGraphEngine& engine;

    std::vector<RoutingNode> nodes;
    std::vector<RoutingConnection> connections;
    int selectedNodeIdx = -1;
    int lastKnownInputChannels = -1;
    int lastKnownOutputChannels = -1;

    RoutingCanvas canvas;
    PropertiesPanel propertiesPanel;

    static constexpr float nodeW = 160.0f;
    static constexpr float headerH = 28.0f;
    static constexpr float portR = 6.0f;
    static constexpr float portSpacing = 22.0f;
    static constexpr int propertiesWidth = 220;
    static constexpr float gridSize = 20.0f;

    static float snapToGrid (float v) { return std::round (v / gridSize) * gridSize; }

    juce::Point<float> getPortPosition (int nodeIdx, bool isOutput, int portIdx) const;
    juce::Colour getPortColour (PortType type) const;
    float computeNodeHeight (const RoutingNode& node) const;

    int findNodeByEngineId (AudioGraphEngine::NodeID id) const;
    void selectNode (int idx);
    void addPedalToRoute (const juce::String& pedalName, float canvasX, float canvasY);

    // ── Notes ──
    NotesOverlay notesOverlay;
    juce::TextButton btnNotes { "Notes" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingGraphEditor)
};

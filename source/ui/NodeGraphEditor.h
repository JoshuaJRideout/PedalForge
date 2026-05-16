#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/DSPGraph.h"

//==============================================================================
/**
 * Visual node graph editor for the Effects Forge.
 *
 * Displays DSP nodes as draggable boxes with input/output ports.
 * Users connect ports by dragging wires between them.
 * Right-click context menu to add new node types.
 * Right panel shows properties for the selected node.
 */
class NodeGraphEditor : public juce::Component
{
public:
    NodeGraphEditor();
    ~NodeGraphEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Get the graph (for serialization / wiring to PedalDesign). */
    DSPGraph& getGraph() { return graph; }

    /** Load a design's effects graph. */
    void loadDesign (const juce::var& effectsGraphJSON);

    /** Reset to default empty graph (AudioIn → AudioOut). */
    void clearGraph();

private:
    //==========================================================================
    // Visual layout for each node
    struct NodeVisual
    {
        float x = 100, y = 100;
        float width = 160;
        float height = 80;
        bool selected = false;
    };

    struct PortHit
    {
        int nodeID = -1;
        bool isOutput = false;
        int portIndex = -1;
    };

    //==========================================================================
    /** The canvas where nodes and wires are drawn and interacted with. */
    class GraphCanvas : public juce::Component,
                        public juce::DragAndDropTarget
    {
    public:
        GraphCanvas (NodeGraphEditor& owner);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
        bool keyPressed (const juce::KeyPress& key) override;

        bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDropped (const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;

        std::function<void(int)> onNodeSelected;

    private:
        friend class NodeGraphEditor;
        NodeGraphEditor& editor;

        float scale = 1.0f, panX = 0.0f, panY = 0.0f;
        juce::Point<float> dragStartPan;
        int draggingNodeID = -1;
        juce::Point<float> nodeDragOffset;
        bool draggingWire = false;
        PortHit wireStart;
        float wireEndX = 0, wireEndY = 0;

        juce::Point<float> screenToCanvas (float sx, float sy) const;
        PortHit hitTestPort (juce::Point<float> cp) const;
        int hitTestNode (juce::Point<float> cp) const;
        int hitTestConnection (juce::Point<float> cp) const;

        void drawNode (juce::Graphics& g, int nodeID, DSPNode* node, const NodeVisual& visual) const;
        void drawConnection (juce::Graphics& g, const NodeConnection& conn, bool highlighted) const;
        void drawWirePreview (juce::Graphics& g) const;

        void showAddNodeMenu (juce::Point<float> canvasPos);
    };

    //==========================================================================
    /** Right panel showing properties of the selected node. */
    class NodePropertiesPanel : public juce::Component,
                                 public juce::Slider::Listener
    {
    public:
        NodePropertiesPanel();
        void paint (juce::Graphics& g) override;
        void resized() override;
        void sliderValueChanged (juce::Slider* slider) override;

        void showNode (int nodeID, DSPNode* node);
        void clearSelection();

        std::function<void(int)> onDeleteNode;
        std::function<void()> onParamChanged;

    private:
        int currentNodeID = -1;
        DSPNode* currentNode = nullptr;

        struct ParamSlider
        {
            std::unique_ptr<juce::Slider> slider;
            std::unique_ptr<juce::Label> label;
            NodeParam* param = nullptr;
        };

        std::vector<ParamSlider> paramSliders;
        juce::TextButton deleteButton { "Delete Node" };
        juce::Viewport viewport;
        juce::Component contentArea;

        // Expression editor (visible only for ExpressionNode)
        juce::TextEditor expressionEditor;
        juce::Label expressionError;
        bool isExpressionNode = false;

        juce::OwnedArray<juce::TextButton> fileLoaders;
        juce::OwnedArray<juce::Label> fileLabels;
        std::unique_ptr<juce::FileChooser> fileChooser;

        void rebuildSliders();
        void setupExpressionEditor();
    };

    //==========================================================================
    DSPGraph graph;
    std::map<int, NodeVisual> nodeVisuals;
    int selectedNodeID = -1;

    struct ClipboardNode {
        juce::String type;
        std::vector<std::pair<juce::String, float>> params;
    };
    std::unique_ptr<ClipboardNode> clipboardNode;

    GraphCanvas canvas;
    NodePropertiesPanel propertiesPanel;

    static constexpr float nodeW = 160.0f;
    static constexpr float headerH = 26.0f;
    static constexpr float portR = 5.0f;
    static constexpr float portSpacing = 20.0f;
    static constexpr float paramRowH = 18.0f;
    static constexpr int propertiesWidth = 240;
    static constexpr float gridSize = 20.0f;

    static float snapToGrid (float v) { return std::round (v / gridSize) * gridSize; }

    float computeNodeHeight (int nodeID) const;
    juce::Point<float> getPortPosition (int nodeID, bool isOutput, int portIndex) const;
    juce::Colour getNodeColour (const juce::String& type) const;

    void selectNode (int nodeID);
    void addNodeAt (const juce::String& type, float cx, float cy);
    void deleteNode (int nodeID);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeGraphEditor)
};

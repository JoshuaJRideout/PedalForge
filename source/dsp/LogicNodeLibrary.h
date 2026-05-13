#pragma once
#include "DSPNode.h"

//==============================================================================
// NAND
class NANDGateNode : public DSPNode {
public:
    NANDGateNode() : DSPNode("nand_gate", "NAND Gate") {
        addInput("a", NodePort::Gate); addInput("b", NodePort::Gate);
        addOutput("out", NodePort::Gate);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool a = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            bool b = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            out[0][i] = !(a && b) ? 1.0f : 0.0f;
        }
    }
};

// NOR
class NORGateNode : public DSPNode {
public:
    NORGateNode() : DSPNode("nor_gate", "NOR Gate") {
        addInput("a", NodePort::Gate); addInput("b", NodePort::Gate);
        addOutput("out", NodePort::Gate);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool a = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            bool b = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            out[0][i] = !(a || b) ? 1.0f : 0.0f;
        }
    }
};

// XNOR
class XNORGateNode : public DSPNode {
public:
    XNORGateNode() : DSPNode("xnor_gate", "XNOR Gate") {
        addInput("a", NodePort::Gate); addInput("b", NodePort::Gate);
        addOutput("out", NodePort::Gate);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool a = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            bool b = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            out[0][i] = (a == b) ? 1.0f : 0.0f;
        }
    }
};

// Buffer (passes signal through — useful as a logic buffer or signal repeater)
class BufferNode : public DSPNode {
public:
    BufferNode() : DSPNode("buffer", "Buffer") {
        addInput("in", NodePort::Control); addOutput("out", NodePort::Control);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) out[0][i] = (numIn>0 && in[0]) ? in[0][i] : 0.0f;
    }
};

// Pulse (Outputs 1 for a set duration when triggered by a rising edge)
class PulseNode : public DSPNode {
public:
    PulseNode() : DSPNode("pulse", "Pulse") {
        addInput("trig", NodePort::Gate);
        addOutput("out", NodePort::Gate);
        addParam("duration", "Duration (s)", 0.001f, 10.0f, 0.1f);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        float durSamples = getParam("duration")->get() * sr;
        for (int i=0; i<n; ++i) {
            bool trig = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            if (trig && !lastTrig) pulseTimer = durSamples;
            lastTrig = trig;
            
            if (pulseTimer > 0) { out[0][i] = 1.0f; pulseTimer--; }
            else out[0][i] = 0.0f;
        }
    }
private:
    bool lastTrig = false;
    float pulseTimer = 0;
};

// Controlled Buffer / Gate (passes signal only when gate is high)
class GateBufferNode : public DSPNode {
public:
    GateBufferNode() : DSPNode("gate_buffer", "Controlled Buffer") {
        addInput("in", NodePort::Control); addInput("gate", NodePort::Gate);
        addOutput("out", NodePort::Control);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool gate = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            out[0][i] = gate ? ((numIn>0 && in[0]) ? in[0][i] : 0.0f) : 0.0f;
        }
    }
};

// SR Latch
class SRLatchNode : public DSPNode {
public:
    SRLatchNode() : DSPNode("sr_latch", "SR Latch") {
        addInput("s", NodePort::Gate); addInput("r", NodePort::Gate);
        addOutput("q", NodePort::Gate);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool s = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            bool r = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            if (r) state = false;
            else if (s) state = true;
            out[0][i] = state ? 1.0f : 0.0f;
        }
    }
private:
    bool state = false;
};

// D Latch
class DLatchNode : public DSPNode {
public:
    DLatchNode() : DSPNode("d_latch", "D Latch") {
        addInput("d", NodePort::Control); addInput("en", NodePort::Gate);
        addOutput("q", NodePort::Control);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool en = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            if (en) state = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = state;
        }
    }
private:
    float state = 0.0f;
};

// D Flip-Flop
class DFlipFlopNode : public DSPNode {
public:
    DFlipFlopNode() : DSPNode("d_ff", "D Flip-Flop") {
        addInput("d", NodePort::Control); addInput("clk", NodePort::Gate);
        addOutput("q", NodePort::Gate);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool clk = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            if (clk && !lastClk) state = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            lastClk = clk;
            out[0][i] = state;
        }
    }
private:
    bool lastClk = false;
    float state = 0.0f;
};

// T Flip-Flop
class TFlipFlopNode : public DSPNode {
public:
    TFlipFlopNode() : DSPNode("t_ff", "T Flip-Flop") {
        addInput("t", NodePort::Gate); addInput("clk", NodePort::Gate);
        addOutput("q", NodePort::Gate);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool t = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            bool clk = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            if (clk && !lastClk && t) state = !state;
            lastClk = clk;
            out[0][i] = state ? 1.0f : 0.0f;
        }
    }
private:
    bool lastClk = false;
    bool state = false;
};

// JK Flip-Flop
class JKFlipFlopNode : public DSPNode {
public:
    JKFlipFlopNode() : DSPNode("jk_ff", "JK Flip-Flop") {
        addInput("j", NodePort::Gate); addInput("k", NodePort::Gate); addInput("clk", NodePort::Gate);
        addOutput("q", NodePort::Gate);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            bool j = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            bool k = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            bool clk = (numIn > 2 && in[2]) && in[2][i] > 0.5f;
            if (clk && !lastClk) {
                if (j && k) state = !state;
                else if (j) state = true;
                else if (k) state = false;
            }
            lastClk = clk;
            out[0][i] = state ? 1.0f : 0.0f;
        }
    }
private:
    bool lastClk = false;
    bool state = false;
};

// Demux
class DemuxNode : public DSPNode {
public:
    DemuxNode() : DSPNode("demux", "Demux") {
        addInput("in", NodePort::Control); addInput("sel", NodePort::Control);
        addOutput("out0", NodePort::Control); addOutput("out1", NodePort::Control);
        addOutput("out2", NodePort::Control); addOutput("out3", NodePort::Control);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        for (int i=0; i<n; ++i) {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            int sel = (numIn > 1 && in[1]) ? (int)std::round(in[1][i]) : 0;
            if (numOut > 0 && out[0]) out[0][i] = (sel == 0) ? val : 0.0f;
            if (numOut > 1 && out[1]) out[1][i] = (sel == 1) ? val : 0.0f;
            if (numOut > 2 && out[2]) out[2][i] = (sel == 2) ? val : 0.0f;
            if (numOut > 3 && out[3]) out[3][i] = (sel == 3) ? val : 0.0f;
        }
    }
};

// Priority
class PriorityNode : public DSPNode {
public:
    PriorityNode() : DSPNode("priority", "Priority") {
        addInput("in1", NodePort::Control); addInput("in2", NodePort::Control);
        addInput("in3", NodePort::Control); addInput("in4", NodePort::Control);
        addOutput("out", NodePort::Control);
    }
    void process(const float** in, int numIn, float** out, int numOut, int n) override {
        if (numOut == 0) return;
        for (int i=0; i<n; ++i) {
            float val = 0.0f;
            for (int j=0; j<4; ++j) {
                if (numIn > j && in[j] && in[j][i] != 0.0f) {
                    val = in[j][i];
                    break;
                }
            }
            out[0][i] = val;
        }
    }
};

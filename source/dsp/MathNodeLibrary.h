#pragma once

#include "DSPNode.h"
#include <cmath>
#include <algorithm>

//==============================================================================
class SubtractNode : public DSPNode
{
public:
    SubtractNode() : DSPNode ("subtract", "Subtract")
    {
        addInput ("a", NodePort::Control);
        addInput ("b", NodePort::Control);
        addOutput ("out", NodePort::Control);
        addParam ("value", "Value", -10.0f, 10.0f, 0.0f);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float val = getParam("value")->get();
        for (int i = 0; i < n; ++i)
        {
            float a = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            float b = (numIn > 1 && in[1]) ? in[1][i] : val;
            out[0][i] = a - b;
        }
    }
};

class RoundNode : public DSPNode
{
public:
    RoundNode() : DSPNode ("round", "Round")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
            out[0][i] = (numIn > 0 && in[0]) ? std::round(in[0][i]) : 0.0f;
    }
};

class FloorNode : public DSPNode
{
public:
    FloorNode() : DSPNode ("floor", "Floor")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
            out[0][i] = (numIn > 0 && in[0]) ? std::floor(in[0][i]) : 0.0f;
    }
};

class CeilingNode : public DSPNode
{
public:
    CeilingNode() : DSPNode ("ceiling", "Ceiling")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
            out[0][i] = (numIn > 0 && in[0]) ? std::ceil(in[0][i]) : 0.0f;
    }
};

class SquareRootNode : public DSPNode
{
public:
    SquareRootNode() : DSPNode ("sqrt", "Square Root")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
            out[0][i] = (numIn > 0 && in[0]) ? std::sqrt(std::max(0.0f, in[0][i])) : 0.0f;
    }
};

class PowerNode : public DSPNode
{
public:
    PowerNode() : DSPNode ("power", "Power")
    {
        addInput ("base", NodePort::Control);
        addInput ("exp", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float b = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            float e = (numIn > 1 && in[1]) ? in[1][i] : 0.0f;
            out[0][i] = std::pow(b, e);
        }
    }
};

class MinNode : public DSPNode
{
public:
    MinNode() : DSPNode ("min", "Min")
    {
        addInput ("a", NodePort::Control);
        addInput ("b", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float a = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            float b = (numIn > 1 && in[1]) ? in[1][i] : 0.0f;
            out[0][i] = std::min(a, b);
        }
    }
};

class MaxNode : public DSPNode
{
public:
    MaxNode() : DSPNode ("max", "Max")
    {
        addInput ("a", NodePort::Control);
        addInput ("b", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float a = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            float b = (numIn > 1 && in[1]) ? in[1][i] : 0.0f;
            out[0][i] = std::max(a, b);
        }
    }
};

class SignNode : public DSPNode
{
public:
    SignNode() : DSPNode ("sign", "Sign")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = (val > 0.0f) ? 1.0f : ((val < 0.0f) ? -1.0f : 0.0f);
        }
    }
};

class ReciprocalNode : public DSPNode
{
public:
    ReciprocalNode() : DSPNode ("reciprocal", "Reciprocal")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = (val != 0.0f) ? 1.0f / val : 0.0f;
        }
    }
};

class IncrementNode : public DSPNode
{
public:
    IncrementNode() : DSPNode ("increment", "Increment")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = val + 1.0f;
        }
    }
};

class DecrementNode : public DSPNode
{
public:
    DecrementNode() : DSPNode ("decrement", "Decrement")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = val - 1.0f;
        }
    }
};

class AverageNode : public DSPNode
{
public:
    AverageNode() : DSPNode ("average", "Average")
    {
        addInput ("a", NodePort::Control);
        addInput ("b", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float a = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            float b = (numIn > 1 && in[1]) ? in[1][i] : 0.0f;
            out[0][i] = (a + b) * 0.5f;
        }
    }
};

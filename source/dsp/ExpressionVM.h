#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <functional>

//==============================================================================
/**
 * ExpressionVM — A lightweight bytecode virtual machine for audio DSP and interactive UIs.
 */
class ExpressionVM
{
public:
    //==========================================================================
    // Opcodes
    enum Op : uint8_t
    {
        OP_PUSH_CONST,  // followed by float constant index
        OP_PUSH_VAR,    // followed by var index
        OP_STORE_VAR,   // pop → store into var
        OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG,
        OP_POW,
        // 1-arg functions
        OP_SIN, OP_COS, OP_TAN, OP_ASIN, OP_ACOS, OP_ATAN,
        OP_ABS, OP_SIGN, OP_FLOOR, OP_CEIL, OP_SQRT,
        OP_EXP, OP_LOG, OP_LOG2, OP_TANH,
        // 2-arg functions
        OP_MIN, OP_MAX,
        // 3-arg functions
        OP_CLAMP, OP_LERP,
        
        // Custom UI & Logic Extension Opcodes
        OP_COND,          // 3-arg cond(test, if_true, if_false)
        OP_DRAW_RECT,     // 5-arg rect(x, y, w, h, color)
        OP_FILL_RECT,     // 5-arg rectFill(x, y, w, h, color)
        OP_DRAW_CIRCLE,   // 4-arg circle(x, y, r, color)
        OP_FILL_CIRCLE,   // 4-arg circleFill(x, y, r, color)
        OP_DRAW_LINE,     // 6-arg line(x1, y1, x2, y2, thickness, color)
        OP_DRAW_TEXT,     // 5-arg text(val, x, y, size, color)
        OP_DRAW_IMAGE,    // 5-arg image(imgIdx, x, y, w, h)
        OP_GET_PARAM,     // 1-arg getParam(idx)
        OP_SET_PARAM,     // 2-arg setParam(idx, val)
        
        // Comparison & Logic Opcodes
        OP_GT, OP_GE, OP_LT, OP_LE, OP_EQ, OP_NE,
        OP_AND, OP_OR, OP_NOT,
        
        OP_END
    };

    //==========================================================================
    // UI Hooks / Bindings
    juce::Graphics* currentGraphics = nullptr;
    std::function<void(juce::Graphics&, float, float, float, float, float)> drawImageCallback;
    std::function<float(int)> getParamCallback;
    std::function<void(int, float)> setParamCallback;

    //==========================================================================
    std::vector<float> vars;
    std::map<std::string, int> varMap;

    int registerVar(const std::string& name)
    {
        if (varMap.count(name)) return varMap[name];
        int idx = (int)vars.size();
        if (idx > 250) return -1;
        varMap[name] = idx;
        vars.push_back(0.0f);
        return idx;
    }

    int getVarIndex(const std::string& name) const
    {
        auto it = varMap.find(name);
        return it != varMap.end() ? it->second : -1;
    }

    void clearVars()
    {
        varMap.clear();
        vars.clear();
        registerVar("sr");
        registerVar("t");
        registerVar("dt");
    }

    void resetState()
    {
        for (auto& v : vars) v = 0.0f;
    }

    bool compile (const juce::String& source)
    {
        bytecode.clear();
        constants.clear();
        errorMessage.clear();
        errorPos = 0;

        src = source.toStdString();
        pos = 0;
        skipWhitespace();

        while (pos < src.size())
        {
            // Skip empty statements (consecutive newlines, semicolons, or general whitespace)
            while (pos < src.size() && (src[pos] == ';' || src[pos] == '\n' || src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r'))
            {
                if (src[pos] == ';' || src[pos] == '\n')
                {
                    pos++;
                    skipWhitespace();
                }
                else
                {
                    skipWhitespace();
                }
            }

            if (pos >= src.size())
                break;

            if (! parseStatement())
                return false;

            skipWhitespace();
            if (pos < src.size() && (src[pos] == ';' || src[pos] == '\n'))
                pos++;
            skipWhitespace();
        }

        bytecode.push_back (OP_END);
        compiled = true;
        return true;
    }

    float evaluate()
    {
        if (! compiled) return 0.0f;

        int sp = 0; 
        float stack[64];
        float lastVal = 0.0f;

        for (int pc = 0; pc < (int)bytecode.size(); )
        {
            Op op = (Op) bytecode[pc++];
            switch (op)
            {
                case OP_PUSH_CONST: stack[sp++] = constants[bytecode[pc++]]; break;
                case OP_PUSH_VAR:   stack[sp++] = vars[bytecode[pc++]]; break;
                case OP_STORE_VAR:  vars[bytecode[pc++]] = stack[sp - 1]; break;
                case OP_ADD: { float b = stack[--sp], a = stack[--sp]; stack[sp++] = a + b; } break;
                case OP_SUB: { float b = stack[--sp], a = stack[--sp]; stack[sp++] = a - b; } break;
                case OP_MUL: { float b = stack[--sp], a = stack[--sp]; stack[sp++] = a * b; } break;
                case OP_DIV: { float b = stack[--sp], a = stack[--sp]; stack[sp++] = (std::abs(b) > 1e-15f) ? a / b : 0.0f; } break;
                case OP_MOD: { float b = stack[--sp], a = stack[--sp]; stack[sp++] = (std::abs(b) > 1e-15f) ? std::fmod(a,b) : 0.0f; } break;
                case OP_NEG: stack[sp-1] = -stack[sp-1]; break;
                case OP_POW: { float b = stack[--sp], a = stack[--sp]; stack[sp++] = std::pow(a,b); } break;
                case OP_SIN:   stack[sp-1] = std::sin(stack[sp-1]); break;
                case OP_COS:   stack[sp-1] = std::cos(stack[sp-1]); break;
                case OP_TAN:   stack[sp-1] = std::tan(stack[sp-1]); break;
                case OP_ASIN:  stack[sp-1] = std::asin(juce::jlimit(-1.0f,1.0f,stack[sp-1])); break;
                case OP_ACOS:  stack[sp-1] = std::acos(juce::jlimit(-1.0f,1.0f,stack[sp-1])); break;
                case OP_ATAN:  stack[sp-1] = std::atan(stack[sp-1]); break;
                case OP_ABS:   stack[sp-1] = std::abs(stack[sp-1]); break;
                case OP_SIGN:  stack[sp-1] = (stack[sp-1] > 0) ? 1.0f : (stack[sp-1] < 0) ? -1.0f : 0.0f; break;
                case OP_FLOOR: stack[sp-1] = std::floor(stack[sp-1]); break;
                case OP_CEIL:  stack[sp-1] = std::ceil(stack[sp-1]); break;
                case OP_SQRT:  stack[sp-1] = std::sqrt(std::abs(stack[sp-1])); break;
                case OP_EXP:   stack[sp-1] = std::exp(juce::jlimit(-80.0f, 80.0f, stack[sp-1])); break;
                case OP_LOG:   stack[sp-1] = std::log(std::abs(stack[sp-1]) + 1e-15f); break;
                case OP_LOG2:  stack[sp-1] = std::log2(std::abs(stack[sp-1]) + 1e-15f); break;
                case OP_TANH:  stack[sp-1] = std::tanh(stack[sp-1]); break;
                case OP_MIN:   { float b = stack[--sp]; stack[sp-1] = std::min(stack[sp-1], b); } break;
                case OP_MAX:   { float b = stack[--sp]; stack[sp-1] = std::max(stack[sp-1], b); } break;
                case OP_CLAMP: { float hi = stack[--sp]; float lo = stack[--sp]; stack[sp-1] = juce::jlimit(lo, hi, stack[sp-1]); } break;
                case OP_LERP:  { float t = stack[--sp]; float b = stack[--sp]; float a = stack[--sp]; stack[sp++] = a + (b-a)*t; } break;

                // ─── UI & logic Opcodes ──────────────────────────────────────
                case OP_COND: {
                    float b = stack[--sp];
                    float a = stack[--sp];
                    stack[sp-1] = (stack[sp-1] > 0.5f) ? a : b;
                } break;

                case OP_DRAW_RECT: {
                    float color = stack[--sp];
                    float h = stack[--sp];
                    float w = stack[--sp];
                    float y = stack[--sp];
                    float x = stack[--sp];
                    if (currentGraphics)
                    {
                        currentGraphics->setColour (juce::Colour ((uint32_t)color | 0xFF000000));
                        currentGraphics->drawRoundedRectangle (x, y, w, h, 4.0f, 1.2f);
                    }
                    stack[sp++] = 0.0f;
                } break;

                case OP_FILL_RECT: {
                    float color = stack[--sp];
                    float h = stack[--sp];
                    float w = stack[--sp];
                    float y = stack[--sp];
                    float x = stack[--sp];
                    if (currentGraphics)
                    {
                        currentGraphics->setColour (juce::Colour ((uint32_t)color | 0xFF000000));
                        currentGraphics->fillRoundedRectangle (x, y, w, h, 4.0f);
                    }
                    stack[sp++] = 0.0f;
                } break;

                case OP_DRAW_CIRCLE: {
                    float color = stack[--sp];
                    float r = stack[--sp];
                    float y = stack[--sp];
                    float x = stack[--sp];
                    if (currentGraphics)
                    {
                        currentGraphics->setColour (juce::Colour ((uint32_t)color | 0xFF000000));
                        currentGraphics->drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.2f);
                    }
                    stack[sp++] = 0.0f;
                } break;

                case OP_FILL_CIRCLE: {
                    float color = stack[--sp];
                    float r = stack[--sp];
                    float y = stack[--sp];
                    float x = stack[--sp];
                    if (currentGraphics)
                    {
                        currentGraphics->setColour (juce::Colour ((uint32_t)color | 0xFF000000));
                        currentGraphics->fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
                    }
                    stack[sp++] = 0.0f;
                } break;

                case OP_DRAW_LINE: {
                    float color = stack[--sp];
                    float thick = stack[--sp];
                    float y2 = stack[--sp];
                    float x2 = stack[--sp];
                    float y1 = stack[--sp];
                    float x1 = stack[--sp];
                    if (currentGraphics)
                    {
                        currentGraphics->setColour (juce::Colour ((uint32_t)color | 0xFF000000));
                        currentGraphics->drawLine (x1, y1, x2, y2, thick);
                    }
                    stack[sp++] = 0.0f;
                } break;

                case OP_DRAW_TEXT: {
                    float color = stack[--sp];
                    float size = stack[--sp];
                    float y = stack[--sp];
                    float x = stack[--sp];
                    float val = stack[--sp];
                    if (currentGraphics)
                    {
                        currentGraphics->setColour (juce::Colour ((uint32_t)color | 0xFF000000));
                        currentGraphics->setFont (juce::FontOptions ("Sans", size, juce::Font::plain));
                        juce::String txt = (val == (int)val) ? juce::String((int)val) : juce::String(val, 2);
                        currentGraphics->drawText (txt, (int)x, (int)y, 250, (int)size + 6, juce::Justification::topLeft);
                    }
                    stack[sp++] = 0.0f;
                } break;

                case OP_DRAW_IMAGE: {
                    float h = stack[--sp];
                    float w = stack[--sp];
                    float y = stack[--sp];
                    float x = stack[--sp];
                    float imgIdx = stack[--sp];
                    if (currentGraphics && drawImageCallback)
                    {
                        drawImageCallback (*currentGraphics, imgIdx, x, y, w, h);
                    }
                    stack[sp++] = 0.0f;
                } break;

                case OP_GET_PARAM: {
                    float paramIdx = stack[sp-1];
                    if (getParamCallback)
                        stack[sp-1] = getParamCallback ((int)paramIdx);
                    else
                        stack[sp-1] = 0.0f;
                } break;

                case OP_SET_PARAM: {
                    float value = stack[--sp];
                    float paramIdx = stack[sp-1];
                    if (setParamCallback)
                        setParamCallback ((int)paramIdx, value);
                    stack[sp-1] = value;
                } break;
                
                // ─── Comparison & Logic Opcodes ─────────────────────────────
                case OP_GT: { float b = stack[--sp]; stack[sp-1] = (stack[sp-1] > b) ? 1.0f : 0.0f; } break;
                case OP_GE: { float b = stack[--sp]; stack[sp-1] = (stack[sp-1] >= b) ? 1.0f : 0.0f; } break;
                case OP_LT: { float b = stack[--sp]; stack[sp-1] = (stack[sp-1] < b) ? 1.0f : 0.0f; } break;
                case OP_LE: { float b = stack[--sp]; stack[sp-1] = (stack[sp-1] <= b) ? 1.0f : 0.0f; } break;
                case OP_EQ: { float b = stack[--sp]; stack[sp-1] = (std::abs(stack[sp-1] - b) < 1e-5f) ? 1.0f : 0.0f; } break;
                case OP_NE: { float b = stack[--sp]; stack[sp-1] = (std::abs(stack[sp-1] - b) >= 1e-5f) ? 1.0f : 0.0f; } break;
                case OP_AND: { float b = stack[--sp]; stack[sp-1] = (stack[sp-1] > 0.5f && b > 0.5f) ? 1.0f : 0.0f; } break;
                case OP_OR: { float b = stack[--sp]; stack[sp-1] = (stack[sp-1] > 0.5f || b > 0.5f) ? 1.0f : 0.0f; } break;
                case OP_NOT: { stack[sp-1] = (stack[sp-1] <= 0.5f) ? 1.0f : 0.0f; } break;

                case OP_END: goto done;
            }
            if (sp > 60) sp = 60; // safety
            if (sp < 0) sp = 0;
        }
        done:
        lastVal = (sp > 0) ? stack[sp - 1] : 0.0f;
        return lastVal;
    }

    bool isCompiled() const { return compiled; }
    const juce::String& getError() const { return errorMessage; }
    const juce::String& getSource() const { return sourceCode; }

    /** 1-based line number where the last compile error occurred. 0 if no error. */
    int getErrorLine() const
    {
        if (errorMessage.isEmpty()) return 0;
        size_t at = juce::jmin (errorPos, src.size());
        int line = 1;
        for (size_t i = 0; i < at; ++i)
            if (src[i] == '\n') ++line;
        return line;
    }

    bool compile_and_store (const juce::String& source)
    {
        sourceCode = source;
        return compile (source);
    }

private:
    std::vector<uint8_t> bytecode;
    std::vector<float> constants;
    std::string src;
    size_t pos = 0;
    bool compiled = false;
    juce::String errorMessage;
    juce::String sourceCode;
    size_t errorPos = 0;

    void setError (const juce::String& msg)
    {
        errorMessage = msg;
        errorPos = pos;
    }

    void skipWhitespace()
    {
        while (pos < src.size())
        {
            if (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r')
            {
                pos++;
            }
            else if (src[pos] == '-' && pos + 1 < src.size() && src[pos + 1] == '-')
            {
                pos += 2;
                while (pos < src.size() && src[pos] != '\n')
                    pos++;
            }
            else if (src[pos] == '/' && pos + 1 < src.size() && src[pos + 1] == '/')
            {
                pos += 2;
                while (pos < src.size() && src[pos] != '\n')
                    pos++;
            }
            else if (src[pos] == '#')
            {
                pos++;
                while (pos < src.size() && src[pos] != '\n')
                    pos++;
            }
            else
            {
                break;
            }
        }
    }

    bool match (char c)
    {
        skipWhitespace();
        if (pos < src.size() && src[pos] == c) { pos++; return true; }
        return false;
    }

    std::string parseIdent()
    {
        skipWhitespace();
        size_t start = pos;
        while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_'))
            pos++;
        return src.substr(start, pos - start);
    }

    float parseNumber()
    {
        skipWhitespace();
        size_t start = pos;
        if (pos < src.size() && (src[pos] == '-' || src[pos] == '+')) pos++;
        while (pos < src.size() && (std::isdigit(src[pos]) || src[pos] == '.'))
            pos++;
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E'))
        {
            pos++;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) pos++;
            while (pos < src.size() && std::isdigit(src[pos])) pos++;
        }
        return std::stof(src.substr(start, pos - start));
    }

    int resolveVar (const std::string& name)
    {
        if (name == "pi")  { emitConst(juce::MathConstants<float>::pi); return -1; }
        if (name == "e")   { emitConst(std::exp(1.0f)); return -1; }
        if (name == "twopi") { emitConst(juce::MathConstants<float>::twoPi); return -1; }

        int idx = registerVar(name);
        if (idx == -1) { setError ("Too many variables"); return -2; }
        return idx;
    }

public:
    // Single source of truth for the built-in function list.
    // The wiki API reference page is generated from this same registry — see
    // dumpFunctionsAsMarkdown().
    struct FunctionInfo
    {
        const char* name;
        int         opcode;
        int         argCount;
        const char* category;     // "Math", "Drawing", "Logic", "Params"
        const char* signature;    // human-readable arg list
        const char* description;
    };

    static const std::vector<FunctionInfo>& getFunctionRegistry()
    {
        static const std::vector<FunctionInfo> registry =
        {
            // Math — 1-arg
            { "sin",   OP_SIN,   1, "Math", "sin(x)",   "Sine of x (radians)." },
            { "cos",   OP_COS,   1, "Math", "cos(x)",   "Cosine of x (radians)." },
            { "tan",   OP_TAN,   1, "Math", "tan(x)",   "Tangent of x (radians)." },
            { "asin",  OP_ASIN,  1, "Math", "asin(x)",  "Arcsine, returns radians." },
            { "acos",  OP_ACOS,  1, "Math", "acos(x)",  "Arccosine, returns radians." },
            { "atan",  OP_ATAN,  1, "Math", "atan(x)",  "Arctangent, returns radians." },
            { "abs",   OP_ABS,   1, "Math", "abs(x)",   "Absolute value." },
            { "sign",  OP_SIGN,  1, "Math", "sign(x)",  "-1, 0, or 1 depending on sign of x." },
            { "floor", OP_FLOOR, 1, "Math", "floor(x)", "Largest integer <= x." },
            { "ceil",  OP_CEIL,  1, "Math", "ceil(x)",  "Smallest integer >= x." },
            { "sqrt",  OP_SQRT,  1, "Math", "sqrt(x)",  "Square root." },
            { "exp",   OP_EXP,   1, "Math", "exp(x)",   "e raised to x." },
            { "log",   OP_LOG,   1, "Math", "log(x)",   "Natural logarithm." },
            { "log2",  OP_LOG2,  1, "Math", "log2(x)",  "Base-2 logarithm." },
            { "tanh",  OP_TANH,  1, "Math", "tanh(x)",  "Hyperbolic tangent - common soft-clip." },

            // Math — 2-arg / 3-arg
            { "min",   OP_MIN,   2, "Math", "min(a, b)",        "Minimum of a and b." },
            { "max",   OP_MAX,   2, "Math", "max(a, b)",        "Maximum of a and b." },
            { "mod",   OP_MOD,   2, "Math", "mod(a, b)",        "Floating-point remainder a % b." },
            { "pow",   OP_POW,   2, "Math", "pow(base, exp)",   "Raise base to the power exp." },
            { "clamp", OP_CLAMP, 3, "Math", "clamp(x, lo, hi)", "Constrain x to [lo, hi]." },
            { "lerp",  OP_LERP,  3, "Math", "lerp(a, b, t)",    "Linear interpolation from a to b by t." },

            // Conditionals
            { "cond",  OP_COND,  3, "Logic", "cond(test, ifTrue, ifFalse)", "Branchless select; like a ternary." },

            // Drawing
            { "rect",       OP_DRAW_RECT,  5, "Drawing", "rect(x, y, w, h, color)",       "Stroke a rectangle outline." },
            { "rectFill",   OP_FILL_RECT,  5, "Drawing", "rectFill(x, y, w, h, color)",   "Fill a rectangle." },
            { "circle",     OP_DRAW_CIRCLE,4, "Drawing", "circle(x, y, r, color)",        "Stroke a circle outline." },
            { "circleFill", OP_FILL_CIRCLE,4, "Drawing", "circleFill(x, y, r, color)",    "Fill a circle." },
            { "line",       OP_DRAW_LINE,  6, "Drawing", "line(x1, y1, x2, y2, thickness, color)", "Draw a line segment." },
            { "text",       OP_DRAW_TEXT,  5, "Drawing", "text(value, x, y, size, color)", "Draw a numeric value as text." },
            { "image",      OP_DRAW_IMAGE, 5, "Drawing", "image(idx, x, y, w, h)",        "Draw the host-supplied image #idx in the given rect." },

            // Param hooks
            { "getParam", OP_GET_PARAM, 1, "Params", "getParam(idx)",       "Read a host-side parameter by index." },
            { "setParam", OP_SET_PARAM, 2, "Params", "setParam(idx, val)",  "Write a host-side parameter by index." },

            // Comparison
            { "gt", OP_GT, 2, "Logic", "gt(a, b)", "1 if a > b, else 0." },
            { "ge", OP_GE, 2, "Logic", "ge(a, b)", "1 if a >= b, else 0." },
            { "lt", OP_LT, 2, "Logic", "lt(a, b)", "1 if a < b, else 0." },
            { "le", OP_LE, 2, "Logic", "le(a, b)", "1 if a <= b, else 0." },
            { "eq", OP_EQ, 2, "Logic", "eq(a, b)", "1 if a == b, else 0." },
            { "ne", OP_NE, 2, "Logic", "ne(a, b)", "1 if a != b, else 0." },

            // Logical
            { "and", OP_AND, 2, "Logic", "and(a, b)", "1 if both non-zero." },
            { "or",  OP_OR,  2, "Logic", "or(a, b)",  "1 if either non-zero." },
            { "not", OP_NOT, 1, "Logic", "not(x)",    "1 if x is zero, else 0." },
        };
        return registry;
    }

    /** Render the function registry as a Markdown reference page. */
    static juce::String dumpFunctionsAsMarkdown()
    {
        const auto& reg = getFunctionRegistry();

        // Collect categories in insertion order
        std::vector<juce::String> categoryOrder;
        std::map<juce::String, std::vector<const FunctionInfo*>> byCategory;
        for (const auto& f : reg)
        {
            juce::String cat (f.category);
            if (! byCategory.count (cat)) categoryOrder.push_back (cat);
            byCategory[cat].push_back (&f);
        }

        juce::String md;
        md << "<!-- AUTO-GENERATED from ExpressionVM::dumpFunctionsAsMarkdown(). Do not edit by hand. -->\n\n";
        md << "# ExpressionVM - Built-in Functions\n\n";
        md << "Every function callable from a UI / DSP / FX Graph script. Generated from the registry in [ExpressionVM.h](../../source/dsp/ExpressionVM.h).\n\n";
        for (const auto& cat : categoryOrder)
        {
            md << "## " << cat << "\n\n";
            md << "| Function | Args | Description |\n";
            md << "|----------|------|-------------|\n";
            for (auto* f : byCategory[cat])
                md << "| `" << f->signature << "` | " << f->argCount << " | " << f->description << " |\n";
            md << "\n";
        }
        return md;
    }

private:
    int resolveFunc (const std::string& name)
    {
        for (const auto& f : getFunctionRegistry())
            if (name == f.name) return f.opcode;
        return -1;
    }

    int funcArgCount (int opcode)
    {
        for (const auto& f : getFunctionRegistry())
            if (f.opcode == opcode) return f.argCount;
        return 1;
    }

    void emitOp (uint8_t op) { bytecode.push_back(op); }
    void emitConst (float val)
    {
        bytecode.push_back(OP_PUSH_CONST);
        bytecode.push_back((uint8_t)constants.size());
        constants.push_back(val);
    }
    void emitVar (int varIdx)
    {
        bytecode.push_back(OP_PUSH_VAR);
        bytecode.push_back((uint8_t)varIdx);
    }
    void emitStore (int varIdx)
    {
        bytecode.push_back(OP_STORE_VAR);
        bytecode.push_back((uint8_t)varIdx);
    }

    bool parseStatement()
    {
        skipWhitespace();
        if (pos >= src.size()) return true;

        if (src[pos] == '@') {
            while (pos < src.size() && src[pos] != '\n') pos++;
            return true;
        }

        size_t saved = pos;
        std::string ident = parseIdent();
        skipWhitespace();

        if (!ident.empty() && pos < src.size() && src[pos] == '=' && (pos+1 >= src.size() || src[pos+1] != '='))
        {
            pos++; 
            int varIdx = resolveVar(ident);
            if (varIdx == -2) return false;
            if (varIdx >= 0)
            {
                if (! parseExpr()) return false;
                emitStore(varIdx);
                return true;
            }
        }

        pos = saved;
        return parseExpr();
    }

    bool parseExpr()
    {
        if (! parseTerm()) return false;
        skipWhitespace();
        while (pos < src.size() && (src[pos] == '+' || src[pos] == '-'))
        {
            char op = src[pos++];
            if (! parseTerm()) return false;
            emitOp(op == '+' ? OP_ADD : OP_SUB);
            skipWhitespace();
        }
        return true;
    }

    bool parseTerm()
    {
        if (! parseFactor()) return false;
        skipWhitespace();
        while (pos < src.size() && (src[pos] == '*' || src[pos] == '/' || src[pos] == '%'))
        {
            char op = src[pos++];
            if (! parseFactor()) return false;
            emitOp(op == '*' ? OP_MUL : (op == '/' ? OP_DIV : OP_MOD));
            skipWhitespace();
        }
        return true;
    }

    bool parseFactor()
    {
        skipWhitespace();
        if (pos < src.size() && src[pos] == '-')
        {
            pos++;
            if (! parseAtom()) return false;
            emitOp(OP_NEG);
            return true;
        }
        return parseAtom();
    }

    bool parseAtom()
    {
        skipWhitespace();
        if (pos >= src.size()) { setError ("Unexpected end of expression"); return false; }

        if (std::isdigit(src[pos]) || (src[pos] == '.' && pos+1 < src.size() && std::isdigit(src[pos+1])))
        {
            emitConst(parseNumber());
            return true;
        }

        if (src[pos] == '(')
        {
            pos++;
            if (! parseExpr()) return false;
            if (! match(')')) { setError ("Expected ')'"); return false; }
            return true;
        }

        if (std::isalpha(src[pos]) || src[pos] == '_')
        {
            std::string ident = parseIdent();
            skipWhitespace();

            if (pos < src.size() && src[pos] == '(')
            {
                int funcOp = resolveFunc(ident);
                if (funcOp < 0) { setError ("Unknown function: " + juce::String(ident)); return false; }

                pos++; // skip '('
                int expectedArgs = funcArgCount(funcOp);

                if (! parseExpr()) return false;
                for (int a = 1; a < expectedArgs; ++a)
                {
                    if (! match(',')) { setError ("Expected ',' in function " + juce::String(ident)); return false; }
                    if (! parseExpr()) return false;
                }
                if (! match(')')) { setError ("Expected ')' after function " + juce::String(ident)); return false; }

                emitOp((uint8_t)funcOp);
                return true;
            }

            int varIdx = resolveVar(ident);
            if (varIdx == -1) return true; 
            if (varIdx == -2) return false;
            emitVar(varIdx);
            return true;
        }

        setError ("Unexpected character: '" + juce::String(juce::String::charToString(src[pos])) + "'");
        return false;
    }
};

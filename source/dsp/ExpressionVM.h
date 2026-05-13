#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <cmath>
#include <map>

//==============================================================================
/**
 * ExpressionVM — A lightweight bytecode virtual machine for audio DSP.
 *
 * Inspired by Wiremod's Expression2 chip from Garry's Mod.
 * Compiles simple math expressions into opcodes, then evaluates
 * them per-sample at audio rate with zero allocation.
 *
 * Built-in variables:
 *   in, in2     — input signals (from node ports)
 *   out         — output signal
 *   sr          — sample rate
 *   t           — time (seconds, auto-incrementing)
 *   dt          — 1/sr (time step)
 *   p1..p4      — user parameters (from sliders)
 *   x1..x8      — state variables (persist between samples)
 *
 * Built-in functions:
 *   sin, cos, tan, asin, acos, atan
 *   abs, sign, floor, ceil, sqrt, exp, log, log2
 *   tanh, pow, min, max, clamp, lerp
 *   mod (same as %)
 *
 * Syntax:
 *   x1 = x1 * 0.999 + abs(in) * 0.001;
 *   out = tanh(in * p1 * (1.0 + x1 * p2))
 *
 * The last expression value becomes 'out' if not explicitly assigned.
 */
class ExpressionVM
{
public:
    //==========================================================================
    // Variable indices
    enum VarID
    {
        VAR_IN = 0, VAR_IN2, VAR_OUT,
        VAR_SR, VAR_T, VAR_DT,
        VAR_P1, VAR_P2, VAR_P3, VAR_P4,
        VAR_X1, VAR_X2, VAR_X3, VAR_X4,
        VAR_X5, VAR_X6, VAR_X7, VAR_X8,
        NUM_VARS
    };

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
        OP_END
    };

    //==========================================================================
    float vars[NUM_VARS] = {};

    /** Reset state variables (x1-x8) and time. */
    void resetState()
    {
        for (int i = VAR_X1; i <= VAR_X8; ++i) vars[i] = 0.0f;
        vars[VAR_T] = 0.0f;
    }

    /** Compile an expression string. Returns true on success. */
    bool compile (const juce::String& source)
    {
        bytecode.clear();
        constants.clear();
        errorMessage.clear();

        src = source.toStdString();
        pos = 0;
        skipWhitespace();

        while (pos < src.size())
        {
            if (! parseStatement())
                return false;
            skipWhitespace();
            // Skip statement separators
            if (pos < src.size() && (src[pos] == ';' || src[pos] == '\n'))
                pos++;
            skipWhitespace();
        }

        bytecode.push_back (OP_END);
        compiled = true;
        return true;
    }

    /** Evaluate the compiled expression. Call once per sample. */
    float evaluate()
    {
        if (! compiled) return 0.0f;

        int sp = 0; // stack pointer
        float stack[64];
        float lastVal = 0.0f;

        for (int pc = 0; pc < (int)bytecode.size(); )
        {
            Op op = (Op) bytecode[pc++];
            switch (op)
            {
                case OP_PUSH_CONST:
                    stack[sp++] = constants[bytecode[pc++]];
                    break;
                case OP_PUSH_VAR:
                    stack[sp++] = vars[bytecode[pc++]];
                    break;
                case OP_STORE_VAR:
                    vars[bytecode[pc++]] = stack[sp - 1];
                    break;
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
                case OP_END: goto done;
            }
            if (sp > 60) sp = 60; // safety
            if (sp < 0) sp = 0;
        }
        done:
        lastVal = (sp > 0) ? stack[sp - 1] : 0.0f;

        // Auto-assign to out if not explicitly stored
        // (the compiled code handles explicit assignments via OP_STORE_VAR)
        return lastVal;
    }

    bool isCompiled() const { return compiled; }
    const juce::String& getError() const { return errorMessage; }

    /** Get the expression source for serialization */
    const juce::String& getSource() const { return sourceCode; }

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

    //==========================================================================
    // Lexer helpers

    void skipWhitespace()
    {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r'))
            pos++;
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
        // Handle scientific notation
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
        if (name == "in")  return VAR_IN;
        if (name == "in2") return VAR_IN2;
        if (name == "out") return VAR_OUT;
        if (name == "sr")  return VAR_SR;
        if (name == "t")   return VAR_T;
        if (name == "dt")  return VAR_DT;
        if (name == "p1")  return VAR_P1;
        if (name == "p2")  return VAR_P2;
        if (name == "p3")  return VAR_P3;
        if (name == "p4")  return VAR_P4;
        if (name == "x1")  return VAR_X1;
        if (name == "x2")  return VAR_X2;
        if (name == "x3")  return VAR_X3;
        if (name == "x4")  return VAR_X4;
        if (name == "x5")  return VAR_X5;
        if (name == "x6")  return VAR_X6;
        if (name == "x7")  return VAR_X7;
        if (name == "x8")  return VAR_X8;
        // Constants
        if (name == "pi")  { emitConst(juce::MathConstants<float>::pi); return -1; }
        if (name == "e")   { emitConst(std::exp(1.0f)); return -1; }
        if (name == "twopi") { emitConst(juce::MathConstants<float>::twoPi); return -1; }
        return -2; // unknown
    }

    int resolveFunc (const std::string& name)
    {
        if (name == "sin")   return OP_SIN;
        if (name == "cos")   return OP_COS;
        if (name == "tan")   return OP_TAN;
        if (name == "asin")  return OP_ASIN;
        if (name == "acos")  return OP_ACOS;
        if (name == "atan")  return OP_ATAN;
        if (name == "abs")   return OP_ABS;
        if (name == "sign")  return OP_SIGN;
        if (name == "floor") return OP_FLOOR;
        if (name == "ceil")  return OP_CEIL;
        if (name == "sqrt")  return OP_SQRT;
        if (name == "exp")   return OP_EXP;
        if (name == "log")   return OP_LOG;
        if (name == "log2")  return OP_LOG2;
        if (name == "tanh")  return OP_TANH;
        if (name == "min")   return OP_MIN;
        if (name == "max")   return OP_MAX;
        if (name == "clamp") return OP_CLAMP;
        if (name == "lerp")  return OP_LERP;
        if (name == "mod")   return OP_MOD;
        if (name == "pow")   return OP_POW;
        return -1;
    }

    int funcArgCount (int opcode)
    {
        switch (opcode) {
            case OP_CLAMP: case OP_LERP: return 3;
            case OP_MIN: case OP_MAX: case OP_MOD: case OP_POW: return 2;
            default: return 1;
        }
    }

    //==========================================================================
    // Emitters

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

    //==========================================================================
    // Parser (recursive descent)

    bool parseStatement()
    {
        skipWhitespace();
        if (pos >= src.size()) return true;

        // Look ahead for assignment: ident = expr
        size_t saved = pos;
        std::string ident = parseIdent();
        skipWhitespace();

        if (!ident.empty() && pos < src.size() && src[pos] == '=' && (pos+1 >= src.size() || src[pos+1] != '='))
        {
            pos++; // skip '='
            int varIdx = resolveVar(ident);
            if (varIdx == -2) { errorMessage = "Unknown variable: " + juce::String(ident); return false; }
            if (varIdx >= 0)
            {
                if (! parseExpr()) return false;
                emitStore(varIdx);
                return true;
            }
        }

        // Not an assignment — parse as expression
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
        if (pos >= src.size()) { errorMessage = "Unexpected end of expression"; return false; }

        // Number
        if (std::isdigit(src[pos]) || (src[pos] == '.' && pos+1 < src.size() && std::isdigit(src[pos+1])))
        {
            emitConst(parseNumber());
            return true;
        }

        // Parenthesized expression
        if (src[pos] == '(')
        {
            pos++;
            if (! parseExpr()) return false;
            if (! match(')')) { errorMessage = "Expected ')'"; return false; }
            return true;
        }

        // Identifier (variable or function)
        if (std::isalpha(src[pos]) || src[pos] == '_')
        {
            std::string ident = parseIdent();
            skipWhitespace();

            // Function call?
            if (pos < src.size() && src[pos] == '(')
            {
                int funcOp = resolveFunc(ident);
                if (funcOp < 0) { errorMessage = "Unknown function: " + juce::String(ident); return false; }

                pos++; // skip '('
                int expectedArgs = funcArgCount(funcOp);

                // Parse arguments
                if (! parseExpr()) return false;
                for (int a = 1; a < expectedArgs; ++a)
                {
                    if (! match(',')) { errorMessage = "Expected ',' in function " + juce::String(ident); return false; }
                    if (! parseExpr()) return false;
                }
                if (! match(')')) { errorMessage = "Expected ')' after function " + juce::String(ident); return false; }

                emitOp((uint8_t)funcOp);
                return true;
            }

            // Variable
            int varIdx = resolveVar(ident);
            if (varIdx == -1) return true; // constant already emitted (pi, e, twopi)
            if (varIdx == -2) { errorMessage = "Unknown variable: " + juce::String(ident); return false; }
            emitVar(varIdx);
            return true;
        }

        errorMessage = "Unexpected character: '" + juce::String(juce::String::charToString(src[pos])) + "'";
        return false;
    }
};

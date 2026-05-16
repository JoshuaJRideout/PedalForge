#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <cmath>
#include <map>

//==============================================================================
/**
 * ExpressionVM — A lightweight bytecode virtual machine for audio DSP.
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
        OP_END
    };

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

        src = source.toStdString();
        pos = 0;
        skipWhitespace();

        while (pos < src.size())
        {
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
        if (idx == -1) { errorMessage = "Too many variables"; return -2; }
        return idx;
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
        if (pos >= src.size()) { errorMessage = "Unexpected end of expression"; return false; }

        if (std::isdigit(src[pos]) || (src[pos] == '.' && pos+1 < src.size() && std::isdigit(src[pos+1])))
        {
            emitConst(parseNumber());
            return true;
        }

        if (src[pos] == '(')
        {
            pos++;
            if (! parseExpr()) return false;
            if (! match(')')) { errorMessage = "Expected ')'"; return false; }
            return true;
        }

        if (std::isalpha(src[pos]) || src[pos] == '_')
        {
            std::string ident = parseIdent();
            skipWhitespace();

            if (pos < src.size() && src[pos] == '(')
            {
                int funcOp = resolveFunc(ident);
                if (funcOp < 0) { errorMessage = "Unknown function: " + juce::String(ident); return false; }

                pos++; // skip '('
                int expectedArgs = funcArgCount(funcOp);

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

            int varIdx = resolveVar(ident);
            if (varIdx == -1) return true; 
            if (varIdx == -2) return false;
            emitVar(varIdx);
            return true;
        }

        errorMessage = "Unexpected character: '" + juce::String(juce::String::charToString(src[pos])) + "'";
        return false;
    }
};

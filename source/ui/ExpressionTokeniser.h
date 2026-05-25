#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <set>

//==============================================================================
/**
 * Syntax tokeniser for the PedalForge expression language.
 *
 * Recognises comments (-- // #), directives (@inputs etc.),
 * built-in function names, numbers (int, float, 0x hex),
 * operators, string literals, and variable identifiers.
 */
class ExpressionTokeniser : public juce::CodeTokeniser
{
public:
    //==========================================================================
    enum TokenType
    {
        tokenType_error = 0,
        tokenType_comment,
        tokenType_directive,
        tokenType_function,
        tokenType_number,
        tokenType_operator,
        tokenType_variable,
        tokenType_string
    };

    //==========================================================================
    ExpressionTokeniser() = default;

    //==========================================================================
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override
    {
        juce::CodeEditorComponent::ColourScheme scheme;

        scheme.set ("Error",     juce::Colour (0xFFEF4444));  // red
        scheme.set ("Comment",   juce::Colour (0xFF6B7280));  // gray
        scheme.set ("Directive", juce::Colour (0xFFEC4899));  // pink
        scheme.set ("Function",  juce::Colour (0xFF67E8F9));  // cyan
        scheme.set ("Number",    juce::Colour (0xFFFB923C));  // orange
        scheme.set ("Operator",  juce::Colour (0xFFE2E8F0));  // light gray
        scheme.set ("Variable",  juce::Colour (0xFF34D399));  // green
        scheme.set ("String",    juce::Colour (0xFFFBBF24));  // yellow

        return scheme;
    }

    //==========================================================================
    int readNextToken (juce::CodeDocument::Iterator& source) override
    {
        source.skipWhitespace();

        if (source.isEOF())
            return tokenType_error;

        auto firstChar = source.peekNextChar();

        // ------------------------------------------------------------------
        // Comments: --, //, or # to end of line
        // ------------------------------------------------------------------
        if (firstChar == '#')
        {
            skipToEndOfLine (source);
            return tokenType_comment;
        }

        if (firstChar == '-')
        {
            source.skip();
            if (source.peekNextChar() == '-')
            {
                skipToEndOfLine (source);
                return tokenType_comment;
            }
            // Single '-' is an operator (or negative sign — treated as operator)
            return tokenType_operator;
        }

        if (firstChar == '/')
        {
            source.skip();
            if (source.peekNextChar() == '/')
            {
                skipToEndOfLine (source);
                return tokenType_comment;
            }
            // Single '/' is the division operator
            return tokenType_operator;
        }

        // ------------------------------------------------------------------
        // Directives: @word
        // ------------------------------------------------------------------
        if (firstChar == '@')
        {
            source.skip();
            while (! source.isEOF() && isIdentifierChar (source.peekNextChar()))
                source.skip();
            return tokenType_directive;
        }

        // ------------------------------------------------------------------
        // String literals: "..." or '...'
        // ------------------------------------------------------------------
        if (firstChar == '"' || firstChar == '\'')
        {
            auto quote = firstChar;
            source.skip();
            while (! source.isEOF())
            {
                auto c = source.nextChar();
                if (c == quote)
                    break;
                if (c == '\\' && ! source.isEOF())
                    source.skip(); // skip escaped character
            }
            return tokenType_string;
        }

        // ------------------------------------------------------------------
        // Numbers: integers, floats, hex (0x / 0X prefix)
        // ------------------------------------------------------------------
        if (juce::CharacterFunctions::isDigit (firstChar)
            || (firstChar == '.' && juce::CharacterFunctions::isDigit (peekCharAt (source, 1))))
        {
            if (firstChar == '0')
            {
                source.skip();
                auto next = source.peekNextChar();
                if (next == 'x' || next == 'X')
                {
                    source.skip();
                    while (! source.isEOF() && isHexDigitChar (source.peekNextChar()))
                        source.skip();
                    return tokenType_number;
                }
            }

            // Consume integer / float digits
            if (firstChar != '0') // '0' already consumed above when it was non-hex
                source.skip();

            while (! source.isEOF() && juce::CharacterFunctions::isDigit (source.peekNextChar()))
                source.skip();

            // Decimal point
            if (! source.isEOF() && source.peekNextChar() == '.')
            {
                source.skip();
                while (! source.isEOF() && juce::CharacterFunctions::isDigit (source.peekNextChar()))
                    source.skip();
            }

            // Exponent (e / E)
            if (! source.isEOF())
            {
                auto e = source.peekNextChar();
                if (e == 'e' || e == 'E')
                {
                    source.skip();
                    if (! source.isEOF() && (source.peekNextChar() == '+' || source.peekNextChar() == '-'))
                        source.skip();
                    while (! source.isEOF() && juce::CharacterFunctions::isDigit (source.peekNextChar()))
                        source.skip();
                }
            }

            return tokenType_number;
        }

        // ------------------------------------------------------------------
        // Identifiers / function keywords
        // ------------------------------------------------------------------
        if (isIdentifierStart (firstChar))
        {
            juce::String word;
            while (! source.isEOF() && isIdentifierChar (source.peekNextChar()))
                word += juce::String::charToString (source.nextChar());

            if (getFunctionNames().count (word) > 0)
                return tokenType_function;

            return tokenType_variable;
        }

        // ------------------------------------------------------------------
        // Operators: + - * / % = ( ) , .
        // ------------------------------------------------------------------
        if (isOperatorChar (firstChar))
        {
            source.skip();
            return tokenType_operator;
        }

        // ------------------------------------------------------------------
        // Anything else is an error token
        // ------------------------------------------------------------------
        source.skip();
        return tokenType_error;
    }

private:
    //==========================================================================
    static void skipToEndOfLine (juce::CodeDocument::Iterator& source)
    {
        while (! source.isEOF())
        {
            auto c = source.peekNextChar();
            if (c == '\r' || c == '\n')
                break;
            source.skip();
        }
    }

    //==========================================================================
    // Peek at the character n positions ahead without advancing the iterator.
    static juce::juce_wchar peekCharAt (juce::CodeDocument::Iterator source, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            if (source.isEOF()) return 0;
            source.skip();
        }
        return source.isEOF() ? 0 : source.peekNextChar();
    }

    //==========================================================================
    static bool isIdentifierStart (juce::juce_wchar c)
    {
        return juce::CharacterFunctions::isLetter (c) || c == '_';
    }

    static bool isIdentifierChar (juce::juce_wchar c)
    {
        return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
    }

    static bool isHexDigitChar (juce::juce_wchar c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    static bool isOperatorChar (juce::juce_wchar c)
    {
        return c == '+' || c == '-' || c == '*' || c == '/'
            || c == '%' || c == '=' || c == '(' || c == ')'
            || c == ',' || c == '.' || c == '<' || c == '>'
            || c == '!' || c == '&' || c == '|' || c == '^'
            || c == '[' || c == ']' || c == '{' || c == '}'
            || c == ';' || c == ':';
    }

    //==========================================================================
    static const std::set<juce::String>& getFunctionNames()
    {
        static const std::set<juce::String> names
        {
            // Math
            "sin", "cos", "tan", "asin", "acos", "atan",
            "abs", "sign", "floor", "ceil", "sqrt",
            "exp", "log", "log2", "tanh",
            "min", "max", "clamp", "lerp", "mod", "pow",
            "cond",
            // Drawing
            "rect", "rectFill", "circle", "circleFill",
            "line", "text", "image",
            // Parameters
            "getParam", "setParam",
            // Comparison / logic
            "gt", "ge", "lt", "le", "eq", "ne",
            "and", "or", "not",
            // Node graph
            "addNode", "connect", "setNodeParam"
        };
        return names;
    }

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExpressionTokeniser)
};

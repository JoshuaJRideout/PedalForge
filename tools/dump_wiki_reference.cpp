// Standalone tool: writes the auto-generated ExpressionVM function reference
// to docs/wiki/reference/expression-vm-functions.md.
//
// Usage:  dump_wiki_reference <output-path>
//
// Driven by a CMake target so the wiki page can't drift from the C++ registry.

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>  // ExpressionVM.h transitively needs juce::Graphics
#include "../source/dsp/ExpressionVM.h"

#include <cstdio>
#include <cstdlib>

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "Usage: %s <output-path>\n", argv[0]);
        return 2;
    }

    juce::String md = ExpressionVM::dumpFunctionsAsMarkdown();
    juce::String outPath (argv[1]);
    juce::File out (outPath);

    // Only write if content actually differs, to avoid polluting git status
    // with no-op rewrites on every build.
    if (out.existsAsFile() && out.loadFileAsString() == md)
    {
        std::printf ("dump_wiki_reference: %s already up to date\n", argv[1]);
        return 0;
    }

    if (! out.replaceWithText (md))
    {
        std::fprintf (stderr, "dump_wiki_reference: failed to write %s\n", argv[1]);
        return 1;
    }
    std::printf ("dump_wiki_reference: wrote %s\n", argv[1]);
    return 0;
}

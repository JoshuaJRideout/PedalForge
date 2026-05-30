#include "SecureStore.h"
#include "../util/AppPaths.h"

#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
 #include <sys/stat.h>
#endif

namespace pf::secure
{
namespace
{
    juce::File secretsFile()
    {
        return pf::paths::getRoot().getChildFile ("secrets.json");
    }

    // Device-derived obfuscation key. Not cryptographic — just keeps the
    // value from being readable plaintext if the file leaks. The 0600
    // perms below are the real guard.
    juce::String obfuscationKey()
    {
        auto id = juce::SystemStats::getUniqueDeviceID();
        if (id.isEmpty()) id = "pedalforge-fallback-key";
        return "pf::" + id + "::ai";
    }

    juce::String xorCycle (const juce::String& input, const juce::String& key)
    {
        auto in  = input.toRawUTF8();
        const auto inLen = (int) input.getNumBytesAsUTF8();
        auto k = key.toRawUTF8();
        const auto kLen = juce::jmax (1, (int) key.getNumBytesAsUTF8());

        juce::MemoryBlock out;
        out.setSize ((size_t) inLen);
        for (int i = 0; i < inLen; ++i)
            out[(size_t) i] = (char) (in[i] ^ k[i % kLen]);
        return out.toBase64Encoding();
    }

    juce::String unXorCycle (const juce::String& b64, const juce::String& key)
    {
        juce::MemoryBlock raw;
        if (! raw.fromBase64Encoding (b64))
            return {};
        auto k = key.toRawUTF8();
        const auto kLen = juce::jmax (1, (int) key.getNumBytesAsUTF8());
        const auto n = (int) raw.getSize();
        juce::MemoryBlock out;
        out.setSize ((size_t) n);
        for (int i = 0; i < n; ++i)
            out[(size_t) i] = (char) (raw[(size_t) i] ^ k[i % kLen]);
        return out.toString();
    }

    juce::var loadAll()
    {
        auto f = secretsFile();
        if (! f.existsAsFile()) return juce::var (new juce::DynamicObject());
        auto parsed = juce::JSON::parse (f.loadFileAsString());
        if (! parsed.isObject()) return juce::var (new juce::DynamicObject());
        return parsed;
    }

    void saveAll (const juce::var& v)
    {
        auto f = secretsFile();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (juce::JSON::toString (v));
        // Lock down to owner read/write only.
        f.setReadOnly (false, false);
       #if JUCE_MAC || JUCE_LINUX || JUCE_BSD
        // juce::File has no chmod; shell out via the POSIX helper. 0600.
        chmod (f.getFullPathName().toRawUTF8(), 0600);
       #endif
    }
}

//==============================================================================
bool store (const juce::String& key, const juce::String& value)
{
    auto all = loadAll();
    auto* obj = all.getDynamicObject();
    if (obj == nullptr) return false;
    obj->setProperty (key, xorCycle (value, obfuscationKey()));
    saveAll (all);
    return true;
}

juce::String retrieve (const juce::String& key)
{
    auto all = loadAll();
    auto stored = all.getProperty (key, "").toString();
    if (stored.isEmpty()) return {};
    return unXorCycle (stored, obfuscationKey());
}

bool has (const juce::String& key)
{
    return retrieve (key).isNotEmpty();
}

void remove (const juce::String& key)
{
    auto all = loadAll();
    if (auto* obj = all.getDynamicObject())
    {
        obj->removeProperty (key);
        saveAll (all);
    }
}
}

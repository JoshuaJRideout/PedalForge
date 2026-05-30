#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// Minimal secrets storage for API keys / tokens (task #64).
//
// v1: a single secrets file at <dataRoot>/secrets.json with 0600
// permissions and device-keyed XOR obfuscation at rest. This file is
// deliberately NOT one of the directories a .pfsnapshot bundles, and
// SnapshotManager also excludes it by name — secrets must never travel
// in a snapshot (see project_snapshots).
//
// The 0600 file permission is the real protection (same model as
// ~/.aws/credentials and SSH keys). The XOR pass only raises the bar
// above casual plaintext reading.
//
// TODO (fast-follow): migrate to the macOS Keychain via the Security
// framework. Interface stays identical; only the impl changes.
//==============================================================================
namespace pf::secure
{
    /** Store a secret under a key (e.g. "anthropic.apiKey"). Overwrites
        any existing value. Returns false on I/O failure. */
    bool store (const juce::String& key, const juce::String& value);

    /** Retrieve a secret, or empty string if absent. */
    juce::String retrieve (const juce::String& key);

    /** True if a non-empty secret exists for this key. */
    bool has (const juce::String& key);

    /** Remove a secret. No-op if absent. */
    void remove (const juce::String& key);
}

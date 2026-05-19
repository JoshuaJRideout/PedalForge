#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <functional>

//==============================================================================
/**
 * Manages the on-disk asset library for PedalForge.
 *
 * Assets are organized by category in:
 *   ~/Library/Application Support/PedalForge/Library/<category>/
 *
 * Currently supported categories:
 *   - NAM       (Neural Amp Modeler .nam files)
 *   - IR        (Impulse Response .wav files)
 */
class AssetLibrary
{
public:
    //==========================================================================
    struct AssetItem
    {
        juce::String name;            // Display name (filename without extension)
        juce::String category;        // "NAM", "IR", etc.
        juce::File   file;            // Full path to the file
        juce::String extension;       // ".nam", ".wav", etc.
        juce::int64  sizeBytes = 0;
        juce::Time   dateAdded;
        juce::StringArray tags;       // Custom metatags
    };

    //==========================================================================
    AssetLibrary()
    {
        libraryRoot = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("PedalForge")
                          .getChildFile ("Library");
    }

    /** Get the library root directory. */
    juce::File getRoot() const { return libraryRoot; }

    /** Get the directory for a specific category, creating it if needed. */
    juce::File getCategoryDir (const juce::String& category) const
    {
        auto dir = libraryRoot.getChildFile (category);
        dir.createDirectory();
        return dir;
    }

    //==========================================================================
    /** Scan a category directory and return all matching assets. */
    std::vector<AssetItem> getAssets (const juce::String& category) const
    {
        std::vector<AssetItem> result;
        auto dir = libraryRoot.getChildFile (category);

        if (! dir.isDirectory())
            return result;

        auto wildcards = getWildcardsForCategory (category);

        for (const auto& entry : juce::RangedDirectoryIterator (dir, false, wildcards))
        {
            auto f = entry.getFile();
            AssetItem item;
            item.name = f.getFileNameWithoutExtension();
            item.category = category;
            item.file = f;
            item.extension = f.getFileExtension();
            item.sizeBytes = f.getSize();
            item.dateAdded = f.getLastModificationTime();
            loadMetadata(item);
            result.push_back (item);
        }

        // Sort alphabetically
        std::sort (result.begin(), result.end(),
                   [](const AssetItem& a, const AssetItem& b) { return a.name.compareIgnoreCase (b.name) < 0; });

        return result;
    }

    //==========================================================================
    /** Import a file into the library by copying it to the category directory.
        Returns the destination file on success, or an invalid File on failure. */
    juce::File importFile (const juce::File& source, const juce::String& category)
    {
        auto destDir = getCategoryDir (category);
        auto destFile = destDir.getChildFile (source.getFileName());

        // If file with same name exists, add a suffix
        if (destFile.existsAsFile())
        {
            int suffix = 1;
            while (destFile.existsAsFile())
            {
                destFile = destDir.getChildFile (source.getFileNameWithoutExtension()
                                                 + " (" + juce::String (suffix++) + ")"
                                                 + source.getFileExtension());
            }
        }

        if (source.copyFileTo (destFile))
            return destFile;

        return {};
    }

    /** Remove an asset from the library. */
    bool removeAsset (const AssetItem& item)
    {
        auto metaFile = getMetadataFile(item.file);
        if (metaFile.existsAsFile())
            metaFile.deleteFile();
            
        return item.file.deleteFile();
    }

    //==========================================================================
    /** Metadata Management */
    juce::File getMetadataFile(const juce::File& assetFile) const
    {
        return assetFile.withFileExtension(assetFile.getFileExtension() + ".meta");
    }

    void loadMetadata(AssetItem& item) const
    {
        auto metaFile = getMetadataFile(item.file);
        if (metaFile.existsAsFile())
        {
            auto json = juce::JSON::parse(metaFile);
            if (auto* obj = json.getDynamicObject())
            {
                if (auto* tagsArr = obj->getProperty("tags").getArray())
                {
                    for (auto& t : *tagsArr)
                        item.tags.add(t.toString());
                }
            }
        }
    }

    void saveMetadata(const AssetItem& item) const
    {
        auto metaFile = getMetadataFile(item.file);
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        
        juce::Array<juce::var> tagsArr;
        for (const auto& t : item.tags)
            tagsArr.add(t);
        obj->setProperty("tags", tagsArr);

        metaFile.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
    }

    //==========================================================================
    /** Get file wildcards for a given category. */
    static juce::String getWildcardsForCategory (const juce::String& category)
    {
        if (category == "NAM")   return "*.nam";
        if (category.startsWith("IR")) return "*.wav;*.aif;*.flac";
        if (category == "Images") return "*.png;*.jpg;*.jpeg;*.svg";
        return "*";
    }

    /** Get the OS file chooser filter string for a category. */
    static juce::String getFileFilterForCategory (const juce::String& category)
    {
        if (category == "NAM")   return "*.nam";
        if (category.startsWith("IR")) return "*.wav;*.aif;*.flac";
        if (category == "Images") return "*.png;*.jpg;*.jpeg;*.svg";
        return "*";
    }

private:
    juce::File libraryRoot;
};

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
        juce::String subcategory;     // User-defined subfolder (e.g. "Stencils")
        juce::File   file;            // Full path to the file
        juce::String extension;       // ".nam", ".wav", etc.
        juce::int64  sizeBytes = 0;
        juce::Time   dateAdded;
        juce::StringArray tags;       // Custom metatags
        juce::String tone3000Id;      // Non-empty if sourced from TONE3000 cloud
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
    std::vector<AssetItem> getAssets (const juce::String& category, const juce::String& subcategory = "") const
    {
        std::vector<AssetItem> result;
        auto dir = libraryRoot.getChildFile (category);

        if (! dir.isDirectory())
            return result;

        auto wildcards = getWildcardsForCategory (category);

        // Scan root of category
        auto scanDir = [&](const juce::File& scanTarget, const juce::String& subcat)
        {
            for (const auto& entry : juce::RangedDirectoryIterator (scanTarget, false, wildcards))
            {
                auto f = entry.getFile();
                AssetItem item;
                item.name = f.getFileNameWithoutExtension();
                item.category = category;
                item.subcategory = subcat;
                item.file = f;
                item.extension = f.getFileExtension();
                item.sizeBytes = f.getSize();
                item.dateAdded = f.getLastModificationTime();
                loadMetadata(item);
                // Metadata subcategory overrides directory-based one
                if (item.subcategory.isEmpty())
                    item.subcategory = subcat;
                result.push_back (item);
            }
        };

        if (subcategory.isNotEmpty())
        {
            // Scan only the specific subcategory folder
            auto subDir = dir.getChildFile (subcategory);
            if (subDir.isDirectory())
                scanDir (subDir, subcategory);
        }
        else
        {
            // Scan root
            scanDir (dir, "");
            // Scan all subdirectories
            for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findDirectories))
                scanDir (entry.getFile(), entry.getFile().getFileName());
        }

        // Sort alphabetically
        std::sort (result.begin(), result.end(),
                   [](const AssetItem& a, const AssetItem& b) { return a.name.compareIgnoreCase (b.name) < 0; });

        return result;
    }

    /** Get list of subcategory names (subdirectories) for a category. */
    juce::StringArray getSubcategories (const juce::String& category) const
    {
        juce::StringArray result;
        auto dir = libraryRoot.getChildFile (category);
        if (!dir.isDirectory()) return result;
        for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findDirectories))
            result.add (entry.getFile().getFileName());
        result.sort (true);
        return result;
    }

    /** Create a subcategory folder. */
    bool createSubcategory (const juce::String& category, const juce::String& name)
    {
        return libraryRoot.getChildFile(category).getChildFile(name).createDirectory();
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

    /** Rename an asset file on disk and return the new file. */
    juce::File renameAsset (const AssetItem& item, const juce::String& newName)
    {
        auto newFile = item.file.getParentDirectory().getChildFile (newName + item.extension);
        if (newFile.existsAsFile()) return {}; // name already taken
        
        // Rename the metadata file too
        auto oldMeta = getMetadataFile(item.file);
        auto newMeta = getMetadataFile(newFile);
        
        if (item.file.moveFileTo (newFile))
        {
            if (oldMeta.existsAsFile())
                oldMeta.moveFileTo (newMeta);
            return newFile;
        }
        return {};
    }

    /** Move an asset to a subcategory folder. */
    juce::File moveToSubcategory (const AssetItem& item, const juce::String& subcategory)
    {
        auto catDir = libraryRoot.getChildFile (item.category);
        juce::File destDir = subcategory.isEmpty() ? catDir : catDir.getChildFile (subcategory);
        destDir.createDirectory();
        auto destFile = destDir.getChildFile (item.file.getFileName());
        if (destFile == item.file) return item.file; // already there
        if (destFile.existsAsFile()) return {}; // conflict

        auto oldMeta = getMetadataFile(item.file);
        auto newMeta = getMetadataFile(destFile);

        if (item.file.moveFileTo (destFile))
        {
            if (oldMeta.existsAsFile())
                oldMeta.moveFileTo (newMeta);
            return destFile;
        }
        return {};
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
                if (obj->hasProperty("subcategory"))
                    item.subcategory = obj->getProperty("subcategory").toString();
                if (obj->hasProperty("tone3000Id"))
                    item.tone3000Id = obj->getProperty("tone3000Id").toString();
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
        
        if (item.subcategory.isNotEmpty())
            obj->setProperty("subcategory", item.subcategory);

        if (item.tone3000Id.isNotEmpty())
            obj->setProperty("tone3000Id", item.tone3000Id);

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

#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <mutex>
#include <vector>

//==============================================================================
/** Represents a single tone/model result from the TONE3000 API. */
struct ToneResult
{
    juce::String id;
    juce::String name;
    juce::String author;
    juce::String gearType;    // "amp", "pedal", "full-rig", "ir"
    juce::String platform;    // "nam", "aida-x", "ir"
    juce::String modelSize;   // "standard", "lite", "feather", "nano"
    int downloads = 0;
    juce::StringArray tags;
    juce::String downloadUrl; // URL to download the model file
};

//==============================================================================
/** Parameters for a search query against the TONE3000 API. */
struct ToneSearchParams
{
    juce::String query;
    juce::String gearFilter;  // underscore-separated: "amp_pedal_full-rig"
    juce::String platform;    // "nam" or "ir"
    juce::String sizeFilter;  // underscore-separated: "standard_lite"
    juce::String tags;
    juce::String sort;        // "trending", "best-match"
    int page = 1;
    int pageSize = 25;
};

//==============================================================================
/** Result container for a paginated search response. */
struct ToneSearchResult
{
    std::vector<ToneResult> tones;
    int totalPages = 0;
    int currentPage = 0;
    juce::String errorMessage;
};

//==============================================================================
/**
 * Asynchronous REST client for the TONE3000 API.
 *
 * All public methods are thread-safe. Work is enqueued and executed on a
 * background thread; results are delivered on the message thread via
 * juce::MessageManager::callAsync().
 */
class Tone3000Client : public juce::Thread
{
public:
    Tone3000Client();
    ~Tone3000Client() override;

    //==========================================================================
    /** Set the publishable API key used for Bearer authentication. */
    void setApiKey (const juce::String& key);


    //==========================================================================
    /** Search for tones matching the given parameters.
        Results are delivered asynchronously on the message thread. */
    void search (ToneSearchParams params,
                 std::function<void (ToneSearchResult)> callback);

    /** Download a tone's model file into the given directory.
        The callback is invoked on the message thread with the downloaded file
        and a success flag. */
    void downloadTone (const ToneResult& tone,
                       juce::File targetDir,
                       std::function<void (juce::File downloadedFile, bool success)> callback);

    /** Cancel all pending work items. */
    void cancelAll();

    //==========================================================================
    void run() override;

private:
    //==========================================================================
    /** A unit of work to be executed on the background thread. */
    struct WorkItem
    {
        std::function<void()> task;
    };

    /** Enqueue a work item and wake the background thread. */
    void enqueueWork (std::function<void()> task);

    /** Build a URL string, appending only non-empty query parameters. */
    static juce::String buildSearchUrl (const juce::String& baseUrl,
                                        const ToneSearchParams& params,
                                        const juce::String& apiKey);

    /** Parse a JSON tone object into a ToneResult. */
    static ToneResult parseToneObject (const juce::var& obj);

    /** Sanitise a string for use as a filename. */
    static juce::String sanitiseFilename (const juce::String& name);

    /** Create an input stream for a URL with appropriate headers. */
    std::unique_ptr<juce::InputStream> createStreamForUrl (const juce::String& urlString);

    //==========================================================================
    juce::String apiBaseUrl { "https://www.tone3000.com/api/v1" };
    juce::String publishableKey;

    mutable std::mutex workMutex;
    std::vector<WorkItem> workQueue;
    juce::WaitableEvent workAvailable { true }; // manual-reset event

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tone3000Client)
};

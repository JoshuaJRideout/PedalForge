#include "Tone3000Client.h"
#include <juce_events/juce_events.h>


//==============================================================================
Tone3000Client::Tone3000Client (Tone3000OAuth& o)
    : juce::Thread ("Tone3000Client"), oauth (o)
{
}

Tone3000Client::~Tone3000Client()
{
    signalThreadShouldExit();
    workAvailable.signal();
    stopThread (5000);
}


//==============================================================================
void Tone3000Client::enqueueWork (std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock (workMutex);
        workQueue.push_back ({ std::move (task) });
    }

    // Lazily start the background thread on first use
    if (! isThreadRunning())
        startThread();

    workAvailable.signal();
}

//==============================================================================
void Tone3000Client::run()
{
    while (! threadShouldExit())
    {
        workAvailable.wait (-1);

        if (threadShouldExit())
            break;

        // Drain the queue
        std::vector<WorkItem> itemsToProcess;
        {
            std::lock_guard<std::mutex> lock (workMutex);
            std::swap (itemsToProcess, workQueue);
            workAvailable.reset();
        }

        for (auto& item : itemsToProcess)
        {
            if (threadShouldExit())
                break;

            item.task();
        }
    }
}

//==============================================================================
void Tone3000Client::cancelAll()
{
    std::lock_guard<std::mutex> lock (workMutex);
    workQueue.clear();
}

//==============================================================================
juce::String Tone3000Client::buildSearchUrl (const juce::String& baseUrl,
                                              const ToneSearchParams& params)
{
    juce::String url = baseUrl + "/tones/search";
    juce::StringArray queryParts;

    if (params.query.isNotEmpty())
        queryParts.add ("query=" + juce::URL::addEscapeChars (params.query, true));

    queryParts.add ("page=" + juce::String (params.page));
    queryParts.add ("page_size=" + juce::String (params.pageSize));

    if (params.platform.isNotEmpty())
        queryParts.add ("platform=" + juce::URL::addEscapeChars (params.platform, true));

    if (params.gearFilter.isNotEmpty())
        queryParts.add ("gears=" + juce::URL::addEscapeChars (params.gearFilter, true));

    if (params.sizeFilter.isNotEmpty())
        queryParts.add ("sizes=" + juce::URL::addEscapeChars (params.sizeFilter, true));

    if (params.tags.isNotEmpty())
        queryParts.add ("tags=" + juce::URL::addEscapeChars (params.tags, true));

    if (params.sort.isNotEmpty())
        queryParts.add ("sort=" + juce::URL::addEscapeChars (params.sort, true));

    if (queryParts.size() > 0)
        url += "?" + queryParts.joinIntoString ("&");

    return url;
}

//==============================================================================
ToneResult Tone3000Client::parseToneObject (const juce::var& obj)
{
    ToneResult result;

    // ID — could be string or int
    result.id = obj.getProperty ("id", {}).toString();

    // Name — try "title" first, then "name"
    result.name = obj.getProperty ("title", "").toString();
    if (result.name.isEmpty())
        result.name = obj.getProperty ("name", "").toString();

    // Author — try "username" first, then "author"
    result.author = obj.getProperty ("username", "").toString();
    if (result.author.isEmpty())
        result.author = obj.getProperty ("author", "").toString();

    // Gear type — try "gear_type", then "gears"
    result.gearType = obj.getProperty ("gear_type", "").toString();
    if (result.gearType.isEmpty())
        result.gearType = obj.getProperty ("gears", "").toString();

    // Platform
    result.platform = obj.getProperty ("platform", "").toString();

    // Model size — try "model_size", then "size"
    result.modelSize = obj.getProperty ("model_size", "").toString();
    if (result.modelSize.isEmpty())
        result.modelSize = obj.getProperty ("size", "").toString();

    // Downloads — try "download_count", then "downloads"
    result.downloads = static_cast<int> (obj.getProperty ("download_count", 0));
    if (result.downloads == 0)
        result.downloads = static_cast<int> (obj.getProperty ("downloads", 0));

    // Tags
    if (auto* tagsArray = obj.getProperty ("tags", {}).getArray())
    {
        for (const auto& tag : *tagsArray)
            result.tags.add (tag.toString());
    }

    // Download URL — try "download_url", then "file_url", then "url"
    result.downloadUrl = obj.getProperty ("download_url", "").toString();
    if (result.downloadUrl.isEmpty())
        result.downloadUrl = obj.getProperty ("file_url", "").toString();
    if (result.downloadUrl.isEmpty())
        result.downloadUrl = obj.getProperty ("url", "").toString();

    return result;
}

//==============================================================================
juce::String Tone3000Client::sanitiseFilename (const juce::String& name)
{
    juce::String sanitised;
    const juce::String illegalChars = "/\\:*?\"<>|";

    for (auto ch : name)
    {
        if (illegalChars.containsChar (ch))
            sanitised += '_';
        else
            sanitised += ch;
    }

    // Collapse multiple underscores and trim
    while (sanitised.contains ("__"))
        sanitised = sanitised.replace ("__", "_");

    return sanitised.trim();
}

//==============================================================================
std::unique_ptr<juce::InputStream> Tone3000Client::createStreamForUrl (const juce::String& urlString)
{
    auto token = oauth.getAccessTokenRefreshingIfNeeded();
    if (token.isEmpty())
    {
        juce::Logger::writeToLog ("[Tone3000] No access token - sign-in required.");
        return nullptr;
    }

    juce::URL url (urlString);
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs (15000)
                       .withNumRedirectsToFollow (5)
                       .withExtraHeaders ("Authorization: Bearer " + token);

    juce::Logger::writeToLog ("[Tone3000] GET " + urlString);

    auto stream = url.createInputStream (options);

    if (stream == nullptr)
        juce::Logger::writeToLog ("[Tone3000] ERROR: Failed to create input stream for " + urlString);

    return stream;
}

//==============================================================================
void Tone3000Client::search (ToneSearchParams params,
                              std::function<void (ToneSearchResult)> callback)
{
    // Capture the base URL by value for thread safety
    auto baseUrl = apiBaseUrl;

    enqueueWork ([this, params = std::move (params), callback = std::move (callback), baseUrl]()
    {
        ToneSearchResult result;
        result.currentPage = params.page;

        auto urlString = buildSearchUrl (baseUrl, params);
        auto stream = createStreamForUrl (urlString);

        if (stream != nullptr)
        {
            auto responseText = stream->readEntireStreamAsString();
            juce::Logger::writeToLog ("[Tone3000] Search response: " + responseText.substring (0, 500));

            auto json = juce::JSON::parse (responseText);

            if (json.isObject())
            {
                if (json.hasProperty ("error"))
                {
                    result.errorMessage = json.getProperty ("error", "").toString();
                }

                // Parse tones from "data" array
                if (auto* dataArray = json.getProperty ("data", {}).getArray())
                {
                    for (const auto& item : *dataArray)
                        result.tones.push_back (parseToneObject (item));
                }

                // Parse pagination — try several common field names
                result.totalPages = static_cast<int> (json.getProperty ("total_pages",
                                        json.getProperty ("totalPages",
                                            json.getProperty ("last_page", 0))));

                result.currentPage = static_cast<int> (json.getProperty ("current_page",
                                         json.getProperty ("currentPage",
                                             json.getProperty ("page", params.page))));
            }

            juce::Logger::writeToLog ("[Tone3000] Parsed " + juce::String (result.tones.size())
                                      + " tones, page " + juce::String (result.currentPage)
                                      + "/" + juce::String (result.totalPages));
        }
        else
        {
            juce::Logger::writeToLog ("[Tone3000] Search request failed");
            result.errorMessage = "Failed to connect to TONE3000 cloud API server.";
        }

        // Deliver result on the message thread
        juce::MessageManager::callAsync ([callback, result = std::move (result)]()
        {
            if (callback)
                callback (result);
        });
    });
}

//==============================================================================
void Tone3000Client::downloadTone (const ToneResult& tone,
                                    juce::File targetDir,
                                    std::function<void (juce::File, bool)> callback)
{
    auto baseUrl = apiBaseUrl;
    auto toneId = tone.id;
    auto toneName = tone.name;
    auto tonePlatform = tone.platform;

    enqueueWork ([this, baseUrl, toneId, toneName, tonePlatform, targetDir, callback = std::move (callback)]()
    {
        juce::Logger::writeToLog ("[Tone3000] Downloading tone: " + toneName + " (id: " + toneId + ")");

        // Step 1: Enumerate the models attached to this tone. /tones/search returns
        // a tone-level summary (with counts) but no per-model download URLs; the
        // actual files live in /models, indexed by tone_id.
        auto modelsUrl = baseUrl + "/models?tone_id=" + toneId;
        auto modelsStream = createStreamForUrl (modelsUrl);

        if (modelsStream == nullptr)
        {
            juce::Logger::writeToLog ("[Tone3000] Failed to fetch model list for tone " + toneId);
            juce::MessageManager::callAsync ([callback] { if (callback) callback ({}, false); });
            return;
        }

        auto responseText = modelsStream->readEntireStreamAsString();
        juce::Logger::writeToLog ("[Tone3000] Models response: " + responseText.substring (0, 800));

        auto json = juce::JSON::parse (responseText);

        // Response may be either a bare array or { data: [...] }. Normalise.
        const juce::Array<juce::var>* arr = nullptr;
        if (auto* a = json.getArray()) arr = a;
        else if (auto* a = json.getProperty ("data", {}).getArray()) arr = a;

        if (arr == nullptr || arr->isEmpty())
        {
            juce::Logger::writeToLog ("[Tone3000] No models found for tone " + toneId);
            juce::MessageManager::callAsync ([callback] { if (callback) callback ({}, false); });
            return;
        }

        // Pick the first model that exposes a download URL.
        // Field name varies — try the common ones in priority order.
        juce::String downloadUrlStr, modelFilename;
        for (const auto& m : *arr)
        {
            for (const char* field : { "model_url", "download_url", "file_url", "url" })
            {
                downloadUrlStr = m.getProperty (field, "").toString();
                if (downloadUrlStr.isNotEmpty()) break;
            }
            if (downloadUrlStr.isNotEmpty())
            {
                modelFilename = m.getProperty ("filename", "").toString();
                if (modelFilename.isEmpty()) modelFilename = m.getProperty ("name", "").toString();
                break;
            }
        }

        if (downloadUrlStr.isEmpty())
        {
            juce::Logger::writeToLog ("[Tone3000] None of the " + juce::String (arr->size())
                                      + " models for tone " + toneId + " had a download URL.");
            juce::MessageManager::callAsync ([callback] { if (callback) callback ({}, false); });
            return;
        }

        // Step 2: Derive the file extension. Prefer the URL path (most reliable),
        // then the filename field, then platform-based default.
        auto extractExt = [] (const juce::String& s) -> juce::String
        {
            int dot = s.lastIndexOfChar ('.');
            int slash = s.lastIndexOfChar ('/');
            int qmark = s.indexOfChar ('?');
            int end = (qmark > 0) ? qmark : s.length();
            if (dot > slash && dot < end)
                return s.substring (dot, end).toLowerCase();
            return {};
        };
        juce::String extension = extractExt (downloadUrlStr);
        if (extension.isEmpty()) extension = extractExt (modelFilename);
        if (extension.isEmpty()) extension = tonePlatform.equalsIgnoreCase ("ir") ? ".wav" : ".nam";

        auto filename = sanitiseFilename (toneName) + extension;
        auto targetFile = targetDir.getChildFile (filename);

        juce::Logger::writeToLog ("[Tone3000] Downloading from: " + downloadUrlStr);
        juce::Logger::writeToLog ("[Tone3000] Saving to: " + targetFile.getFullPathName());

        auto downloadStream = createStreamForUrl (downloadUrlStr);

        if (downloadStream != nullptr)
        {
            targetDir.createDirectory();

            juce::FileOutputStream outputStream (targetFile);

            if (outputStream.openedOk())
            {
                outputStream.setPosition (0);
                outputStream.truncate();

                constexpr int bufferSize = 8192;
                juce::HeapBlock<char> buffer (bufferSize);
                int bytesRead;
                int64_t totalBytesWritten = 0;

                while ((bytesRead = downloadStream->read (buffer, bufferSize)) > 0)
                {
                    if (threadShouldExit())
                    {
                        juce::Logger::writeToLog ("[Tone3000] Download cancelled");
                        targetFile.deleteFile();

                        juce::MessageManager::callAsync ([callback]()
                        {
                            if (callback)
                                callback ({}, false);
                        });
                        return;
                    }

                    outputStream.write (buffer, static_cast<size_t> (bytesRead));
                    totalBytesWritten += bytesRead;
                }

                outputStream.flush();

                juce::Logger::writeToLog ("[Tone3000] Download complete: "
                                          + juce::String (totalBytesWritten) + " bytes written");

                juce::MessageManager::callAsync ([callback, targetFile]()
                {
                    if (callback)
                        callback (targetFile, true);
                });
            }
            else
            {
                juce::Logger::writeToLog ("[Tone3000] ERROR: Failed to open output file: "
                                          + targetFile.getFullPathName());

                juce::MessageManager::callAsync ([callback]()
                {
                    if (callback)
                        callback ({}, false);
                });
            }
        }
        else
        {
            juce::Logger::writeToLog ("[Tone3000] ERROR: Failed to download from: " + downloadUrlStr);

            juce::MessageManager::callAsync ([callback]()
            {
                if (callback)
                    callback ({}, false);
            });
        }
    });
}




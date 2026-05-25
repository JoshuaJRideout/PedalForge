#include "Tone3000OAuth.h"

#include <juce_cryptography/juce_cryptography.h>
#include <juce_events/juce_events.h>
#include <thread>

namespace
{
    // PedalForge's TONE3000 publishable key (client_id). Publishable keys are
    // *designed* to be embedded in client-side code — they only identify the
    // application, not a user. The corresponding secret key never ships.
    //
    // To rotate: generate a new publishable key in your TONE3000 dashboard
    // (Settings → API Keys) and replace the string below.
    //
    // To override locally without committing, set TONE3000_CLIENT_ID in your
    // environment before launching the app.
    constexpr const char* kBakedPublishableKey = "t3k_pub_EJ_9Ojq9H-G3crhM1mgu2jxMXlmnYXdE";

    constexpr const char* kAuthorizeUrl = "https://www.tone3000.com/api/v1/oauth/authorize";
    constexpr const char* kTokenUrl     = "https://www.tone3000.com/api/v1/oauth/token";

    juce::String resolvePublishableKey()
    {
        auto env = juce::SystemStats::getEnvironmentVariable ("TONE3000_CLIENT_ID", {});
        return env.isNotEmpty() ? env : juce::String (kBakedPublishableKey);
    }

    juce::int64 nowSeconds()
    {
        return juce::Time::getCurrentTime().toMilliseconds() / 1000;
    }
}

//==============================================================================
Tone3000OAuth::Tone3000OAuth()
{
    loadTokens();
}

//==============================================================================
bool Tone3000OAuth::isSignedIn() const
{
    std::lock_guard<std::mutex> lock (mu);
    return refreshToken.isNotEmpty();
}

juce::String Tone3000OAuth::getCachedAccessToken() const
{
    std::lock_guard<std::mutex> lock (mu);
    return accessToken;
}

bool Tone3000OAuth::isAccessTokenExpired() const
{
    std::lock_guard<std::mutex> lock (mu);
    // Treat tokens expiring in the next 30s as already expired to avoid races.
    return expiresAt == 0 || nowSeconds() + 30 >= expiresAt;
}

juce::String Tone3000OAuth::getAccessTokenRefreshingIfNeeded()
{
    if (! isSignedIn()) return {};
    if (! isAccessTokenExpired()) return getCachedAccessToken();

    juce::String err;
    if (! refresh (err))
        return {};
    return getCachedAccessToken();
}

//==============================================================================
void Tone3000OAuth::signOut()
{
    {
        std::lock_guard<std::mutex> lock (mu);
        accessToken.clear();
        refreshToken.clear();
        expiresAt = 0;
    }
    saveTokens();
    notifyStateChanged();
}

//==============================================================================
bool Tone3000OAuth::refresh (juce::String& outError)
{
    juce::String currentRefresh;
    {
        std::lock_guard<std::mutex> lock (mu);
        currentRefresh = refreshToken;
    }
    if (currentRefresh.isEmpty())
    {
        outError = "Not signed in.";
        return false;
    }

    juce::String form;
    form << "grant_type=refresh_token"
         << "&refresh_token=" << juce::URL::addEscapeChars (currentRefresh, true)
         << "&client_id="     << juce::URL::addEscapeChars (resolvePublishableKey(), true);

    if (! postTokenEndpoint (form, outError))
    {
        // invalid_grant means the refresh token is dead. Clear so the next call
        // forces a full sign-in.
        if (outError.containsIgnoreCase ("invalid_grant"))
        {
            signOut();
            outError = "Refresh token expired — please sign in again.";
        }
        return false;
    }
    return true;
}

//==============================================================================
bool Tone3000OAuth::signInBlocking (juce::String& outError)
{
    auto clientId = resolvePublishableKey();
    if (clientId.isEmpty() || clientId == "t3k_pub_REPLACE_ME")
    {
        outError = "TONE3000 publishable key not configured. Set TONE3000_CLIENT_ID or edit Tone3000OAuth.cpp.";
        return false;
    }

    auto verifier  = generateCodeVerifier();
    auto challenge = deriveCodeChallenge (verifier);
    auto state     = randomState();

    int port = 0;
    juce::String code, returnedState;

    // Start the listener BEFORE opening the browser, so the redirect can't race us.
    std::thread listenerThread ([&]
    {
        runCallbackServer (port, code, returnedState, state, outError);
    });

    // Spin briefly until the server has a port (or has errored).
    for (int i = 0; i < 200 && port == 0 && outError.isEmpty(); ++i)
        juce::Thread::sleep (10);

    if (port == 0)
    {
        if (outError.isEmpty()) outError = "Failed to start local OAuth listener.";
        listenerThread.join();
        return false;
    }

    juce::String redirectUri = "http://127.0.0.1:" + juce::String (port) + "/callback";

    juce::String authUrl;
    authUrl << kAuthorizeUrl
            << "?client_id="              << juce::URL::addEscapeChars (clientId, true)
            << "&response_type=code"
            << "&redirect_uri="           << juce::URL::addEscapeChars (redirectUri, true)
            << "&code_challenge="         << challenge
            << "&code_challenge_method=S256"
            << "&state="                  << state;

    juce::Logger::writeToLog ("[Tone3000OAuth] Opening browser: " + authUrl);
    juce::URL (authUrl).launchInDefaultBrowser();

    // Wait for the listener thread to finish (it captures the callback then exits).
    listenerThread.join();

    if (code.isEmpty())
    {
        if (outError.isEmpty()) outError = "Sign-in cancelled or no auth code received.";
        return false;
    }
    if (returnedState != state)
    {
        outError = "OAuth state mismatch — possible CSRF.";
        return false;
    }

    if (! exchangeCodeForTokens (code, verifier, redirectUri, outError))
        return false;

    notifyStateChanged();
    return true;
}

//==============================================================================
void Tone3000OAuth::signInAsync (std::function<void (bool, juce::String)> callback)
{
    std::thread ([this, callback = std::move (callback)]
    {
        juce::String err;
        bool ok = signInBlocking (err);
        juce::MessageManager::callAsync ([callback, ok, err]
        {
            if (callback) callback (ok, err);
        });
    }).detach();
}

//==============================================================================
bool Tone3000OAuth::exchangeCodeForTokens (const juce::String& code,
                                            const juce::String& codeVerifier,
                                            const juce::String& redirectUri,
                                            juce::String& outError)
{
    juce::String form;
    form << "grant_type=authorization_code"
         << "&code="          << juce::URL::addEscapeChars (code, true)
         << "&code_verifier=" << juce::URL::addEscapeChars (codeVerifier, true)
         << "&redirect_uri="  << juce::URL::addEscapeChars (redirectUri, true)
         << "&client_id="     << juce::URL::addEscapeChars (resolvePublishableKey(), true);

    return postTokenEndpoint (form, outError);
}

bool Tone3000OAuth::postTokenEndpoint (const juce::String& formBody, juce::String& outError)
{
    juce::URL url = juce::URL (kTokenUrl).withPOSTData (formBody);

    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                       .withConnectionTimeoutMs (15000)
                       .withExtraHeaders ("Content-Type: application/x-www-form-urlencoded")
                       .withHttpRequestCmd ("POST");

    auto stream = url.createInputStream (options);
    if (stream == nullptr)
    {
        outError = "Could not reach TONE3000 token endpoint.";
        return false;
    }

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse (response);

    if (! json.isObject())
    {
        outError = "Malformed token response: " + response.substring (0, 200);
        return false;
    }

    if (json.hasProperty ("error"))
    {
        outError = json.getProperty ("error", "").toString();
        auto desc = json.getProperty ("error_description", "").toString();
        if (desc.isNotEmpty()) outError += " — " + desc;
        return false;
    }

    juce::String at = json.getProperty ("access_token",  "").toString();
    juce::String rt = json.getProperty ("refresh_token", "").toString();
    int expiresIn   = (int) json.getProperty ("expires_in", 3600);

    if (at.isEmpty())
    {
        outError = "Token response missing access_token.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock (mu);
        accessToken  = at;
        if (rt.isNotEmpty()) refreshToken = rt;   // refresh response sometimes omits this
        expiresAt = nowSeconds() + expiresIn;
    }
    saveTokens();
    return true;
}

//==============================================================================
bool Tone3000OAuth::runCallbackServer (int& outPort,
                                        juce::String& outCode,
                                        juce::String& outState,
                                        const juce::String& expectedState,
                                        juce::String& outError)
{
    juce::ignoreUnused (expectedState);

    juce::StreamingSocket listener;
    if (! listener.createListener (0, "127.0.0.1"))
    {
        outError = "Could not bind local OAuth listener to 127.0.0.1";
        juce::Logger::writeToLog ("[Tone3000OAuth] createListener failed");
        return false;
    }

    outPort = listener.getBoundPort();
    juce::Logger::writeToLog ("[Tone3000OAuth] Listening on 127.0.0.1:" + juce::String (outPort));

    // Blocking accept — the listener thread parks here until the browser hits
    // /callback or the listener is closed from elsewhere. We previously polled
    // via waitUntilReady() but that returned -1 spuriously on macOS and
    // destroyed the listener mid-flow.
    std::unique_ptr<juce::StreamingSocket> connection (listener.waitForNextConnection());

    if (connection == nullptr)
    {
        outError = "OAuth listener closed before callback.";
        juce::Logger::writeToLog ("[Tone3000OAuth] waitForNextConnection returned null");
        return false;
    }

    juce::Logger::writeToLog ("[Tone3000OAuth] Accepted callback connection");

    // The browser's GET line may not have arrived by the time accept() returns,
    // so loop reading until we see end-of-headers (\r\n\r\n) or hit a 5-second
    // deadline. waitUntilReady on a connected socket is safe (unlike on listener
    // sockets in JUCE, where the readLock try-lock can spuriously fail).
    juce::String requestText;
    auto deadlineMs = juce::Time::getMillisecondCounter() + 5000;

    while (juce::Time::getMillisecondCounter() < deadlineMs)
    {
        int wait = connection->waitUntilReady (true, 500);
        if (wait < 0) { outError = "Connection error while reading callback."; return false; }
        if (wait == 0) continue;

        char chunk[2048];
        int n = connection->read (chunk, sizeof (chunk), false);
        if (n <= 0) { juce::Thread::sleep (10); continue; }

        requestText += juce::String (juce::CharPointer_UTF8 (chunk), (size_t) n);
        if (requestText.contains ("\r\n\r\n")) break;
    }

    juce::Logger::writeToLog ("[Tone3000OAuth] Read " + juce::String (requestText.length()) + " bytes from callback");
    if (requestText.isEmpty())
    {
        outError = "Empty callback request.";
        return false;
    }

    auto firstLine = requestText.upToFirstOccurrenceOf ("\r\n", false, false);
    juce::Logger::writeToLog ("[Tone3000OAuth] Callback request: " + firstLine);

    auto pathAndQuery = firstLine.fromFirstOccurrenceOf (" ", false, false)
                                 .upToFirstOccurrenceOf (" ", false, false);
    auto query = pathAndQuery.fromFirstOccurrenceOf ("?", false, false);

    juce::StringArray pairs;
    pairs.addTokens (query, "&", "");
    for (auto& p : pairs)
    {
        auto eq = p.indexOfChar ('=');
        if (eq < 0) continue;
        auto k = p.substring (0, eq);
        auto v = juce::URL::removeEscapeChars (p.substring (eq + 1));
        if      (k == "code")  outCode  = v;
        else if (k == "state") outState = v;
    }
    juce::Logger::writeToLog ("[Tone3000OAuth] Parsed code=" + outCode.substring (0, 8) + "...");

    // Respond with a friendly page and close.
    juce::String body =
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>PedalForge</title>"
        "<style>body{font-family:-apple-system,Segoe UI,sans-serif;background:#0F0F14;"
        "color:#E2E8F0;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}"
        "div{max-width:32rem;padding:2rem;background:#1E1E2E;border-radius:.75rem;}"
        "h1{margin-top:0;color:#6366F1;}</style></head><body><div>"
        "<h1>You're signed in</h1>"
        "<p>You can close this tab and return to PedalForge.</p>"
        "</div></body></html>";

    juce::String response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html; charset=utf-8\r\n"
             << "Content-Length: " << body.getNumBytesAsUTF8() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;

    auto utf8 = response.toRawUTF8();
    connection->write (utf8, (int) std::strlen (utf8));
    connection->close();

    return outCode.isNotEmpty();
}

//==============================================================================
juce::String Tone3000OAuth::generateCodeVerifier()
{
    // RFC 7636: 43-128 chars of URL-safe ASCII. 64 random bytes → 86-char base64url.
    juce::MemoryBlock random ((size_t) 64);
    juce::Random rng (juce::Time::currentTimeMillis());
    for (size_t i = 0; i < random.getSize(); ++i)
        ((juce::uint8*) random.getData())[i] = (juce::uint8) rng.nextInt (256);
    return base64UrlEncode (random);
}

juce::String Tone3000OAuth::deriveCodeChallenge (const juce::String& verifier)
{
    auto verifierUtf8 = verifier.toRawUTF8();
    juce::SHA256 hash (verifierUtf8, std::strlen (verifierUtf8));
    return base64UrlEncode (hash.getRawData());
}

juce::String Tone3000OAuth::base64UrlEncode (const juce::MemoryBlock& data)
{
    auto std = juce::Base64::toBase64 (data.getData(), data.getSize());
    std = std.replaceCharacters ("+/", "-_");
    while (std.endsWithChar ('=')) std = std.dropLastCharacters (1);
    return std;
}

juce::String Tone3000OAuth::randomState()
{
    juce::Random rng (juce::Time::currentTimeMillis());
    juce::String s;
    static const char* alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < 32; ++i)
        s += alphabet[rng.nextInt (62)];
    return s;
}

//==============================================================================
juce::File Tone3000OAuth::getSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PedalForge")
               .getChildFile ("settings.json");
}

void Tone3000OAuth::saveTokens()
{
    auto file = getSettingsFile();
    file.getParentDirectory().createDirectory();

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    // Preserve existing settings.json properties (e.g. any legacy keys).
    if (file.existsAsFile())
    {
        auto existing = juce::JSON::parse (file);
        if (auto* existingObj = existing.getDynamicObject())
            for (const auto& prop : existingObj->getProperties())
                obj->setProperty (prop.name, prop.value);
    }

    juce::String at, rt;
    juce::int64 ea;
    {
        std::lock_guard<std::mutex> lock (mu);
        at = accessToken; rt = refreshToken; ea = expiresAt;
    }
    obj->setProperty ("tone3000AccessToken",    at);
    obj->setProperty ("tone3000RefreshToken",   rt);
    obj->setProperty ("tone3000TokenExpiresAt", ea);

    file.replaceWithText (juce::JSON::toString (juce::var (obj.get())));
}

void Tone3000OAuth::loadTokens()
{
    auto file = getSettingsFile();
    if (! file.existsAsFile()) return;

    auto json = juce::JSON::parse (file);
    if (! json.isObject()) return;

    std::lock_guard<std::mutex> lock (mu);
    accessToken  = json.getProperty ("tone3000AccessToken",  "").toString();
    refreshToken = json.getProperty ("tone3000RefreshToken", "").toString();
    expiresAt    = (juce::int64) json.getProperty ("tone3000TokenExpiresAt", 0);
}

void Tone3000OAuth::notifyStateChanged()
{
    auto cb = onAuthStateChanged;
    if (cb)
    {
        juce::MessageManager::callAsync ([cb] { cb(); });
    }
}

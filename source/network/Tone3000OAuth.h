#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <mutex>

//==============================================================================
/**
 * OAuth 2.0 + PKCE manager for the TONE3000 API.
 *
 * TONE3000 issues per-app **publishable keys** (client_id, prefix t3k_pub_) and
 * per-user **secret keys**. PedalForge ships its own publishable key compiled in;
 * each user signs in once through a standard browser-based OAuth flow which
 * returns a per-user access token + refresh token. Access tokens are used as
 * Bearer credentials for all API requests.
 *
 * Flow (RFC 7636 Authorization Code + PKCE):
 *   1. App generates a random code_verifier and derives code_challenge = base64url(SHA256(verifier))
 *   2. App opens browser to https://www.tone3000.com/api/v1/oauth/authorize?...
 *      with response_type=code, code_challenge, code_challenge_method=S256, state,
 *      client_id=<publishable_key>, and a localhost redirect_uri
 *   3. User signs in / approves; TONE3000 redirects to http://127.0.0.1:<port>/callback?code=...&state=...
 *   4. A short-lived local HTTP listener captures the callback and extracts the auth code
 *   5. App POSTs the code + code_verifier to /api/v1/oauth/token; receives access + refresh tokens
 *   6. Tokens are persisted to settings.json and used as Bearer auth on subsequent requests
 *   7. When access tokens expire, refresh() exchanges the refresh token for a fresh pair
 */
class Tone3000OAuth
{
public:
    Tone3000OAuth();

    //==========================================================================
    /** True if a refresh token is present. The access token may still be expired —
        call refresh() to renew it before making API calls. */
    bool isSignedIn() const;

    /** Cached access token. May be empty (not signed in) or expired
        (check isAccessTokenExpired() and call refresh() if so). */
    juce::String getCachedAccessToken() const;

    /** Returns true if expiresAt is in the past. */
    bool isAccessTokenExpired() const;

    /** BLOCKING. Returns a valid access token, refreshing if necessary.
        Empty string means not signed in or refresh failed. Call from a
        background thread; the refresh path makes a synchronous HTTP request. */
    juce::String getAccessTokenRefreshingIfNeeded();

    //==========================================================================
    /** BLOCKING. Exchange the refresh token for a new access + refresh token pair.
        Returns true on success; false on network error or invalid_grant
        (in which case the user must sign in again — tokens are cleared). */
    bool refresh (juce::String& outError);

    /** BLOCKING. Run the full sign-in flow: browser launch, local callback,
        token exchange. Safe to call from a background thread.
        Returns true on success; false on cancel/error. */
    bool signInBlocking (juce::String& outError);

    /** Async wrapper around signInBlocking. Spawns a worker thread. The callback
        fires on the message thread when the flow completes. */
    void signInAsync (std::function<void (bool success, juce::String error)> callback);

    /** Clear all tokens and persist the (empty) state. */
    void signOut();

    /** Fires on the message thread whenever sign-in state changes. */
    std::function<void()> onAuthStateChanged;

private:
    //==========================================================================
    juce::String accessToken;
    juce::String refreshToken;
    juce::int64  expiresAt = 0;  // unix seconds; 0 = never had a token
    mutable std::mutex mu;

    void saveTokens();
    void loadTokens();
    void notifyStateChanged();

    bool exchangeCodeForTokens (const juce::String& code,
                                const juce::String& codeVerifier,
                                const juce::String& redirectUri,
                                juce::String& outError);

    bool postTokenEndpoint (const juce::String& formBody, juce::String& outError);

    bool runCallbackServer (int& outPort,
                            juce::String& outCode,
                            juce::String& outState,
                            const juce::String& expectedState,
                            juce::String& outError);

    static juce::String generateCodeVerifier();
    static juce::String deriveCodeChallenge (const juce::String& verifier);
    static juce::String base64UrlEncode (const juce::MemoryBlock& data);
    static juce::String randomState();

    static juce::File getSettingsFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tone3000OAuth)
};

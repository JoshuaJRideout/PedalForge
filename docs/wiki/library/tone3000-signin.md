# TONE3000 Sign-in

Browse and download community NAM amp models and IR files from [TONE3000](https://www.tone3000.com) directly inside PedalForge.

## At a glance

- **Where it lives in the UI:** Library tab → **TONE3000** toggle → **Sign in** button
- **Underlying type(s):** `Tone3000OAuth` (manages tokens), `Tone3000Client` (REST surface)
- **Persisted in:** `~/Library/Application Support/PedalForge/settings.json` — access token, refresh token, expiry timestamp
- **Auth standard:** OAuth 2.0 Authorization Code + PKCE (RFC 7636)
- **Related wiki:** [[overview]]

## How to use it

1. Open the **Library** tab.
2. Click the **☁ TONE3000** toggle in the top-right.
3. Click **Sign in**. Your default browser opens the TONE3000 authorize page.
4. Sign in to your TONE3000 account (free to create) and click **Authorize**.
5. The browser shows "You're signed in"; you can close that tab.
6. The Library tab shows search results. Use the filter buttons (Amps / Pedals / Full Rigs / IRs) and the search box to find models.
7. Click any model card to download it into your local NAM or IR library.

Tokens are cached, so sign-in only happens once. PedalForge silently refreshes the access token when it expires (every ~1 hour). If the refresh token also expires, you'll be prompted to sign in again — no manual re-entry of secret keys.

To sign out: click the **Sign out** button (which replaces **Sign in** while signed in).

## How the auth flow works

```
PedalForge                      Browser                     TONE3000
    │                              │                            │
    │  generate code_verifier      │                            │
    │  code_challenge = b64url(SHA256(verifier))                │
    │  start local listener on 127.0.0.1:<random_port>          │
    │                              │                            │
    │  open authorize URL  ───────►│                            │
    │                              │  GET /oauth/authorize ───► │
    │                              │   client_id=t3k_pub_...    │
    │                              │   code_challenge=...       │
    │                              │   redirect_uri=127.0.0.1   │
    │                              │   state=<random>           │
    │                              │                            │
    │                              │   user signs in / approves │
    │                              │◄── 302 to 127.0.0.1 ───────│
    │                              │   ?code=...&state=...      │
    │                              │                            │
    │◄─── GET /callback?code=… ────│                            │
    │                              │                            │
    │  POST /oauth/token ──────────────────────────────────────►│
    │   grant_type=authorization_code                           │
    │   code, code_verifier, redirect_uri                       │
    │◄── access_token + refresh_token + expires_in ─────────────│
    │                              │                            │
    │  save tokens to settings.json                             │
    │  use Bearer auth on every subsequent API call             │
```

## Configuration

PedalForge's publishable key (the OAuth `client_id`) is baked into the binary. Publishable keys are **designed to be embedded in client code** — they only identify the app, not a user. To rotate or override:

| How | Effect |
|-----|--------|
| Edit `kBakedPublishableKey` in [Tone3000OAuth.cpp](../../source/network/Tone3000OAuth.cpp) and rebuild | Permanent; ships with the binary |
| Set `TONE3000_CLIENT_ID=t3k_pub_…` in your env before launching | Per-run override, useful for local dev |

The **secret key** (also visible in TONE3000's dashboard) is never used by PedalForge and must never be shipped.

## Gotchas

- **Browser doesn't open.** PedalForge uses `juce::URL::launchInDefaultBrowser`. If you have no default browser configured (rare on macOS), copy the authorize URL from the log file and paste it manually.
- **Sign-in stuck after authorize.** The local callback server listens on `127.0.0.1:<random port>`. If your firewall blocks loopback traffic, the redirect can't reach the app. Allow PedalForge to accept incoming connections on localhost.
- **`Refresh token expired`.** TONE3000 rotates refresh tokens periodically. PedalForge will clear stored credentials; just click **Sign in** again.
- **Behind a corporate proxy.** PedalForge talks to `https://www.tone3000.com` directly. If your network forces HTTP proxying, that's not currently supported.

## See also

- [[overview]] — PedalForge architecture at a glance
- [TONE3000 API docs](https://www.tone3000.com/api)

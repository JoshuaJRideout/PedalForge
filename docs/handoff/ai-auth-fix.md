# In-app AI assistant — auth (401) hardening handoff

Status: **CODE COMPLETE & BEHAVIOUR-VERIFIED, UNCOMMITTED.** A fresh session
should review the diff, optionally eyeball the new button via computer-use, then
commit. Restart was chosen only to escape a degrading tool I/O channel — the work
itself is finished and the build is green.

Base: `main` @ `29d06df` (which added `scripts/ai-auth-doctor.sh`). The large
Phase 0/1/2a style-engine WIP also remains uncommitted on `main` — keep it
separate from this auth commit.

---

## 1. Confirmed diagnosis

The "Ask Claude" panel is bring-your-own-subscription: `ClaudeCodeProvider`
shells out to the local `claude` CLI and **strips** `ANTHROPIC_API_KEY` /
`ANTHROPIC_AUTH_TOKEN` (cpp ~line 216) to force the Pro/Max **OAuth
subscription** login. That token lives in the macOS login keychain (service
`"Claude Code-credentials"`); there is **no** `~/.claude/.credentials.json` here.

The keychain OAuth token **expired** (`plan: pro`). The headless `claude -p` the
app spawns **cannot self-refresh** it — reproduced by running the app's exact
invocation, which returns `api_error_status:401 "Invalid authentication
credentials"` and leaves the token unchanged. So graceful detect→relogin→retry
is the only correct UX; the app can't silently refresh.

Verify any time: `scripts/ai-auth-doctor.sh --check` · Repair:
`scripts/ai-auth-doctor.sh --login` (then relaunch the app).

## 2. What was implemented (6 files, all build-green on `PedalForge_All`)

- **`source/ai/AiTypes.h`** — `Response::authExpired` flag.
- **`source/ai/ClaudeCodeProvider.cpp`** — in `send()`'s `is_error` branch:
  detect auth failure (`api_error_status == 401` OR result text contains
  `authentication_error` / `invalid authentication credentials` /
  `not logged in` / `please run /login`), set `authExpired`, a friendly
  `error` message, call `resetSession()` (dead-auth sessions can't be resumed),
  and return. Non-auth errors keep the old generic path.
- **`source/ai/AiAgent.h` / `.cpp`** — new `onAuthExpired` callback; `run()`
  routes `resp.authExpired` to it instead of `onError`.
- **`source/ui/AiAssistantPanel.h` / `.cpp`** — new `signInBtn`
  ("Sign in to Claude", hidden by default, `addChildComponent`), `onAuthExpired`
  handler that expands the panel + shows the message + reveals the button,
  `launchClaudeLogin()` (macOS: osascript → Terminal runs `<bin> /login`; else
  `ChildProcess <bin> /login`), button onClick resets session + hides itself,
  `sendCurrent()` hides a stale button on a fresh attempt, and `resized()` lays
  the button out just above the input row only when visible.

`ClaudeCodeProvider.h` was NOT modified (confirmed via `git diff` — earlier
worry was a channel glitch).

## 3. Verification done

- `cmake --build build --target PedalForge_All --config Release` → **0 errors**
  (mojibake/UTF-8 guard target also passed — new string literals are ASCII).
- **Live end-to-end** via the app's remote bridge (`/tmp/pedalforge_ai_cmd.txt`
  → `/tmp/pedalforge_ai_response.txt`): with the token still expired, sending a
  real prompt to the live in-app agent produced
  `AUTH EXPIRED: Your Claude subscription login has expired…` + `[turn complete]`
  instead of the old raw 401 JSON. Confirms provider→agent→panel chain.
- **NOT yet eyeballed:** the visual "Sign in to Claude" button + the
  Terminal/osascript login flow (logic compiles & the path is exercised by the
  bridge, but the actual click wasn't screenshotted). Worth a computer-use
  glance per `CLAUDE.md` §12: re-login via `scripts/ai-auth-doctor.sh --login`
  so the token is valid, then force a 401 (let it expire / temporarily rename
  the keychain item) to see the button appear, click it, confirm Terminal opens.

## 4. To finish in the fresh session

1. `git diff -- source/ai source/ui/AiAssistantPanel.h source/ui/AiAssistantPanel.cpp`
   — review the 6-file change (clean, ~105 insertions).
2. (Optional) computer-use eyeball of the button per §3.
3. Commit JUST these 6 files + (optionally) this doc — do **not** sweep in the
   style-engine WIP. Suggested message:
   `ai: surface expired-login as a re-login prompt instead of a raw 401`
   with Co-Authored-By trailer.
4. Re-login (`scripts/ai-auth-doctor.sh --login`), relaunch, then return to the
   **style-engine** work: the original StyleTest visual verification in
   `docs/handoff/style-engine-handoff.md` §0 is still outstanding (it was
   blocked by exactly this 401).

## 5. Gotchas observed this session
- The tool I/O channel intermittently **fabricated** grep line numbers and
  truncated multi-line reads. Trust a read only if it looks coherent; re-issue if
  not. `Write` (new files) and single-line bash stayed reliable.
- `Edit` requires a real `Read` of the file first — a `cat`/`sed` dump does NOT
  satisfy it (the AiTypes.h edit failed the first time for this reason).
- Avoid `cd … && …` (zsh split it once); prefer `git -C <repo>`.
- Quote globs: `--include='*.cpp'` (unquoted aborts the whole zsh command).
- Don't chain `sleep` to poll; the harness blocks it. Use `run_in_background` or
  the bridge response file.

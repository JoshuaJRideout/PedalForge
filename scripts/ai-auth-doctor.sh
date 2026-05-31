#!/usr/bin/env bash
#
# ai-auth-doctor.sh — diagnose & repair the in-app AI assistant's Claude auth.
#
# WHY THIS EXISTS
# ---------------
# The PedalForge "Ask Claude" panel (source/ai/ClaudeCodeProvider.cpp) is a
# bring-your-own-subscription agent: it shells out to the user's local `claude`
# CLI and deliberately STRIPS ANTHROPIC_API_KEY / ANTHROPIC_AUTH_TOKEN so the
# request uses the Pro/Max SUBSCRIPTION login, not metered API billing.
#
# That subscription login is an OAuth token stored in the macOS login keychain
# (service "Claude Code-credentials"). When it expires, the *headless* `claude -p`
# the app spawns does NOT refresh it — it just fails with:
#
#   API Error: 401 {"type":"authentication_error",
#                   "message":"Invalid authentication credentials"}
#
# ...which surfaces in the panel as a scary raw-JSON error. The remedy is to
# re-login (an interactive flow the headless path can't perform itself).
#
# This script reports the exact auth state the app sees, optionally runs a live
# probe with the app's exact flags, and can launch the re-login flow.
#
# USAGE
#   scripts/ai-auth-doctor.sh            # read-only diagnosis (default)
#   scripts/ai-auth-doctor.sh --check    # diagnosis + LIVE probe (spawns claude -p)
#   scripts/ai-auth-doctor.sh --login    # re-authenticate (refreshes the token)
#   scripts/ai-auth-doctor.sh --model M  # probe with model alias M (default: sonnet)
#
# Exit codes: 0 = auth looks healthy, 1 = expired/missing/probe failed, 2 = no CLI.
#
set -uo pipefail

KEYCHAIN_SERVICE="Claude Code-credentials"
MODEL="sonnet"
DO_CHECK=0
DO_LOGIN=0

while [ $# -gt 0 ]; do
  case "$1" in
    --check)  DO_CHECK=1 ;;
    --login)  DO_LOGIN=1 ;;
    --model)  shift; MODEL="${1:-sonnet}" ;;
    -h|--help)
      sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *) echo "unknown arg: $1 (try --help)" >&2; exit 2 ;;
  esac
  shift
done

say()  { printf '%s\n' "$*"; }
ok()   { printf '  \033[32m✓\033[0m %s\n' "$*"; }
warn() { printf '  \033[33m!\033[0m %s\n' "$*"; }
bad()  { printf '  \033[31m✗\033[0m %s\n' "$*"; }

# --- 1. locate the claude binary (mirror ClaudeCodeProvider::findClaudeBinary) --
find_claude() {
  local c
  for c in "$HOME/.local/bin/claude" /usr/local/bin/claude \
           /opt/homebrew/bin/claude /home/linuxbrew/.linuxbrew/bin/claude; do
    [ -x "$c" ] && { printf '%s\n' "$c"; return 0; }
  done
  command -v claude 2>/dev/null && return 0
  return 1
}

CLAUDE_BIN="$(find_claude || true)"

say "== PedalForge AI auth doctor =="
if [ -z "${CLAUDE_BIN:-}" ]; then
  bad "No \`claude\` CLI found. Install it from https://claude.com/code and run"
  say "    \`claude\` once to log in with your subscription."
  exit 2
fi
ok "claude CLI: $CLAUDE_BIN"
ver="$("$CLAUDE_BIN" --version 2>/dev/null | head -1 || true)"
[ -n "$ver" ] && say "    version: $ver"

# --- 2. inspect the stored subscription token ---------------------------------
# Reads expiry only (never prints the token). Keychain is the source of truth on
# macOS; ~/.claude/.credentials.json is the Linux / fallback store.
read_token_expiry() {  # $1 = json blob -> prints "<expiresAt_ms> <subType>"
  printf '%s' "$1" | python3 - <<'PY' 2>/dev/null || true
import json, sys
try:
    d = json.load(sys.stdin)
except Exception:
    sys.exit(0)
o = d.get("claudeAiOauth", d)
print(o.get("expiresAt", ""), o.get("subscriptionType", "?"))
PY
}

NOW_MS=$(( $(date +%s) * 1000 ))
AUTH_STATE="unknown"   # healthy | expired | missing

# 2a. keychain (macOS)
KC_BLOB=""
if command -v security >/dev/null 2>&1; then
  KC_BLOB="$(security find-generic-password -s "$KEYCHAIN_SERVICE" -w 2>/dev/null || true)"
fi
# 2b. file fallback
FILE_CRED="$HOME/.claude/.credentials.json"
FILE_BLOB=""
[ -f "$FILE_CRED" ] && FILE_BLOB="$(cat "$FILE_CRED" 2>/dev/null || true)"

report_blob() {  # $1 label, $2 blob
  local label="$1" blob="$2" parsed exp sub delta
  [ -z "$blob" ] && { say "    $label: (none)"; return 1; }
  parsed="$(read_token_expiry "$blob")"
  exp="${parsed%% *}"; sub="${parsed##* }"
  if [ -z "$exp" ] || [ "$exp" = "None" ]; then
    warn "$label: present but no expiry field"; return 1
  fi
  delta=$(( (exp - NOW_MS) / 60000 ))
  if [ "$delta" -gt 0 ]; then
    ok "$label: valid for ${delta} min (plan: ${sub})"
    AUTH_STATE="healthy"; return 0
  else
    bad "$label: EXPIRED $(( -delta )) min ago (plan: ${sub})"
    [ "$AUTH_STATE" = "unknown" ] && AUTH_STATE="expired"
    return 1
  fi
}

say "token store:"
report_blob "keychain" "$KC_BLOB"  || true
report_blob "file    " "$FILE_BLOB" || true
[ -z "$KC_BLOB$FILE_BLOB" ] && AUTH_STATE="missing"

# --- 3. optional live probe (exact flags ClaudeCodeProvider::send uses) --------
if [ "$DO_CHECK" = 1 ]; then
  say "live probe (model: $MODEL) — spawning headless claude exactly as the app does..."
  out="$(cd /tmp && /usr/bin/env -u ANTHROPIC_API_KEY -u ANTHROPIC_AUTH_TOKEN \
        "$CLAUDE_BIN" -p "reply with the single word OK" \
        --model "$MODEL" --permission-mode dontAsk --tools "" \
        --setting-sources user --strict-mcp-config --output-format json 2>&1 || true)"
  status="$(printf '%s' "$out" | grep -oE '"api_error_status":[0-9]+' | head -1 | grep -oE '[0-9]+' || true)"
  is_err="$(printf '%s' "$out" | grep -oE '"is_error":(true|false)' | head -1 || true)"
  if printf '%s' "$out" | grep -q '"is_error":false'; then
    ok "probe PASSED — the in-app agent can authenticate."
    AUTH_STATE="healthy"
  elif [ "$status" = "401" ]; then
    bad "probe FAILED — 401 Invalid authentication credentials (token expired)."
    AUTH_STATE="expired"
  elif printf '%s' "$out" | grep -qi 'not logged in'; then
    bad "probe FAILED — not logged in."
    AUTH_STATE="missing"
  else
    warn "probe inconclusive ($is_err${status:+, status $status}):"
    printf '%s\n' "$out" | head -c 400 | sed 's/^/      /'
  fi
fi

# --- 4. repair ----------------------------------------------------------------
if [ "$DO_LOGIN" = 1 ]; then
  say ""
  say "re-authenticating — complete the login in your browser, then re-launch PedalForge."
  # `claude /login` jumps straight into the login flow on current CLIs; if your
  # version doesn't accept it, run `claude` and type /login at the prompt.
  if ! "$CLAUDE_BIN" /login; then
    warn "\`claude /login\` did not run cleanly. Run \`claude\` and type: /login"
    exit 1
  fi
  exit 0
fi

# --- 5. verdict / guidance ----------------------------------------------------
say ""
case "$AUTH_STATE" in
  healthy)
    ok "Auth looks healthy. If the panel still errors, fully quit & relaunch PedalForge."
    exit 0 ;;
  expired)
    bad "Subscription login expired. Refresh it with:"
    say "    scripts/ai-auth-doctor.sh --login"
    say "  then fully quit & relaunch PedalForge."
    exit 1 ;;
  missing)
    bad "No subscription login found. Sign in with:"
    say "    scripts/ai-auth-doctor.sh --login"
    exit 1 ;;
  *)
    warn "Could not determine auth state. Run with --check for a live probe."
    exit 1 ;;
esac

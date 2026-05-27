"""
VLM-as-judge semantic QA for the image pipeline.

The CV pre-filter in qa_process.py catches blurry / off-center / busy-background
images cheaply. This module handles the failure modes pure CV can't see:

  - Wrong viewing angle (3/4 / perspective / bottom-up when we need top-down)
  - Visible internal parts that should be hidden inside an enclosure
  - Wrong style (illustration / cartoon when we want photorealistic)
  - Wrong subject entirely

For each image we send the bytes plus a category-specific rubric to a vision
LLM and parse the structured JSON verdict back.

Defaults to OpenAI's gpt-4o-mini-vision (cheap, ~$0.005 / image). The provider
is pluggable — drop in Claude or Gemini by editing call_vlm() if you prefer.

Responses are cached by source-hash in out/vlm_cache.json so re-runs don't
re-pay for the same image.
"""

from __future__ import annotations

import base64
import json
import os
import re
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional

import yaml


# ─────────────────────────────────────────────────────────────────────────────
# Data model
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class CheckResult:
    verdict: str = "FAIL"      # PASS | FAIL
    reason:  str = ""

@dataclass
class VlmResult:
    overall: str = "REJECT"     # ACCEPT | REVIEW | REJECT
    summary: str = ""
    checks: dict[str, CheckResult] = field(default_factory=dict)
    model:  str = ""
    raw_response: str = ""      # for debugging / appeals
    error:  Optional[str] = None


# ─────────────────────────────────────────────────────────────────────────────
# Cache
# ─────────────────────────────────────────────────────────────────────────────

class VlmCache:
    def __init__(self, path: Path):
        self.path = path
        self.data: dict[str, dict] = {}
        if path.exists():
            try:
                self.data = json.loads(path.read_text())
            except Exception:
                self.data = {}

    def get(self, key: str) -> Optional[VlmResult]:
        d = self.data.get(key)
        if not d:
            return None
        try:
            checks = {k: CheckResult(**v) for k, v in d.get("checks", {}).items()}
            return VlmResult(
                overall=d.get("overall", "REJECT"),
                summary=d.get("summary", ""),
                checks=checks,
                model=d.get("model", ""),
                raw_response=d.get("raw_response", ""),
                error=d.get("error"),
            )
        except Exception:
            return None

    def put(self, key: str, result: VlmResult) -> None:
        self.data[key] = {
            "overall": result.overall,
            "summary": result.summary,
            "checks": {k: asdict(v) for k, v in result.checks.items()},
            "model":  result.model,
            "raw_response": result.raw_response,
            "error":  result.error,
        }
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(self.data, indent=2))


# ─────────────────────────────────────────────────────────────────────────────
# Prompt construction
# ─────────────────────────────────────────────────────────────────────────────

def build_prompt(prompts_doc: dict, category: str) -> str:
    """Render the category-specific rubric into a single string prompt."""
    preamble = prompts_doc.get("preamble", "").strip()
    cats = prompts_doc.get("categories", {})
    cat = cats.get(category) or cats.get("uncategorized")
    if not cat:
        raise ValueError(f"No prompt template for category '{category}'")

    lines = [preamble, ""]
    desc = cat.get("description", "")
    if desc:
        lines += [f"Expected subject: {desc}", ""]

    lines += ["Checks:"]
    for name, body in cat.get("checks", {}).items():
        body_str = body.strip().replace("\n", " ")
        lines += [f"  - {name}: {body_str}"]

    return "\n".join(lines)


# ─────────────────────────────────────────────────────────────────────────────
# Provider — OpenAI vision
# ─────────────────────────────────────────────────────────────────────────────

def encode_image_b64(path: Path) -> str:
    return base64.b64encode(path.read_bytes()).decode("ascii")


KEY_FALLBACK_PATH = Path.home() / ".config" / "pedalforge" / "openai_key"

def _load_openai_key() -> Optional[str]:
    """Env var wins; fall back to ~/.config/pedalforge/openai_key (0600,
    gitignored by living outside the repo). Returns None if neither set."""
    env = os.environ.get("OPENAI_API_KEY")
    if env:
        return env.strip()
    if KEY_FALLBACK_PATH.exists():
        try:
            return KEY_FALLBACK_PATH.read_text().strip()
        except OSError:
            return None
    return None


def call_openai_vision(image_path: Path, prompt: str, model: str) -> tuple[str, Optional[str]]:
    """Returns (raw_text, error)."""
    try:
        from openai import OpenAI
    except ImportError:
        return "", "openai package not installed (pip install openai)"

    api_key = _load_openai_key()
    if not api_key:
        return "", (f"OpenAI key not found. Set OPENAI_API_KEY in env or write "
                    f"the key to {KEY_FALLBACK_PATH}")

    client = OpenAI(api_key=api_key)

    img_b64 = encode_image_b64(image_path)
    img_url = f"data:image/png;base64,{img_b64}"

    try:
        resp = client.chat.completions.create(
            model=model,
            messages=[
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": prompt},
                        {"type": "image_url", "image_url": {"url": img_url, "detail": "low"}},
                    ],
                },
            ],
            max_tokens=600,
            temperature=0.0,
        )
        return resp.choices[0].message.content or "", None
    except Exception as exc:
        return "", f"API error: {exc}"


# ─────────────────────────────────────────────────────────────────────────────
# Response parsing
# ─────────────────────────────────────────────────────────────────────────────

JSON_FENCE_RE = re.compile(r"```(?:json)?\s*(\{.*?\})\s*```", re.DOTALL)

def parse_response(text: str) -> VlmResult:
    """Best-effort JSON extraction. VLMs sometimes wrap in markdown fences."""
    if not text:
        return VlmResult(error="empty response")

    candidate = text.strip()
    m = JSON_FENCE_RE.search(candidate)
    if m:
        candidate = m.group(1)
    else:
        # Find the outermost {...} block
        start = candidate.find("{")
        end   = candidate.rfind("}")
        if start >= 0 and end > start:
            candidate = candidate[start : end + 1]

    try:
        data = json.loads(candidate)
    except Exception as exc:
        return VlmResult(error=f"could not parse JSON: {exc}", raw_response=text)

    checks: dict[str, CheckResult] = {}
    for name, v in (data.get("checks") or {}).items():
        if isinstance(v, dict):
            checks[name] = CheckResult(
                verdict=str(v.get("verdict", "FAIL")).upper(),
                reason=str(v.get("reason", "")),
            )

    overall = str(data.get("overall", "REJECT")).upper()
    if overall not in ("ACCEPT", "REVIEW", "REJECT"):
        overall = "REJECT"

    return VlmResult(
        overall=overall,
        summary=str(data.get("summary", "")),
        checks=checks,
        raw_response=text,
    )


# ─────────────────────────────────────────────────────────────────────────────
# Public entry point
# ─────────────────────────────────────────────────────────────────────────────

def check_image(image_path: Path,
                category: str,
                source_hash: str,
                prompts_doc: dict,
                cache: VlmCache,
                model: str = "gpt-4o-mini") -> VlmResult:
    """Run the VLM check (cached). Returns a VlmResult."""
    cache_key = f"{model}:{category}:{source_hash}"
    hit = cache.get(cache_key)
    if hit is not None and hit.error is None:
        return hit

    prompt = build_prompt(prompts_doc, category)
    raw, err = call_openai_vision(image_path, prompt, model)
    if err:
        result = VlmResult(error=err, model=model, raw_response=raw)
    else:
        result = parse_response(raw)
        result.model = model

    cache.put(cache_key, result)
    return result


# ─────────────────────────────────────────────────────────────────────────────
# Category classification — used when the user runs --auto-category so each
# image gets routed to the right rubric instead of being judged against a
# global one. Separate cheap call (returns a single word).
# ─────────────────────────────────────────────────────────────────────────────

CLASSIFY_PROMPT = """\
You are classifying an AI-generated image into one of the following categories
for a guitar effect plugin's UI assets. Reply with ONLY the category name —
one lowercase word, no punctuation, no explanation.

CATEGORIES with their distinguishing visual features:

  led         - A light-emitting INDICATOR. ALWAYS has a transparent, translucent,
                or coloured-glass DOME at the centre (clear, frosted, red, green,
                blue, etc.). Often set in a metal or plastic BEZEL ring that
                surrounds the dome. Function is to emit light, NOT to be turned
                or pressed. Includes single LEDs and multi-LED bars / arrays.
                If you see a glass/plastic dome in a metal ring, it's an LED.

  knob        - A control CAP designed to be turned. Has tactile features:
                ridges, finger grips, a pointer/indicator line, or a dot mark
                showing position. Usually plastic body with a coloured top.
                No transparent dome. Function is to be twisted by hand.
                If you see grip ridges or an indicator line, it's a knob.

  footswitch  - A LARGE stomp button designed to be pressed by foot. Round,
                metallic, flat or slightly domed top. Significantly larger
                diameter than a knob. No glass dome (not an LED). No grip
                ridges (not a knob).

  chassis     - A complete guitar effect pedal — the full enclosure with
                multiple controls (knobs, switches, LEDs) mounted on it.
                Not a single component.

  screen      - A rectangular DISPLAY showing information: LCD, OLED, LED
                matrix, seven-segment numeric. Usually wider than tall.
                Distinguish from led: a screen shows VARIABLE content
                (text, numbers, patterns); an LED bar shows fixed dots that
                light up.

  pedalboard  - 3 or more complete pedals arranged together on a flat board.

  other       - Doesn't fit any above category (texture, artifact, unrelated
                subject, in-progress build, broken render).

AMBIGUITY GUIDE — if torn between two categories:
  - LED vs knob: a transparent/glass DOME at the centre → led.
                 Tactile grip ridges or an indicator line → knob.
  - LED vs screen: discrete light points in fixed positions → led.
                   Continuous display area with text or imagery → screen.
  - knob vs footswitch: small diameter, grip texture → knob.
                        large diameter, no grip → footswitch.

Reply with exactly one of: chassis, knob, led, footswitch, screen, pedalboard, other"""

VALID_CATEGORIES = {"chassis", "knob", "led", "footswitch", "screen", "pedalboard", "other"}


def classify_image(image_path: Path,
                   source_hash: str,
                   cache: VlmCache,
                   model: str = "gpt-4o-mini") -> tuple[str, Optional[str]]:
    """Returns (category, error). Cached separately so re-categorizing doesn't
    re-pay. Uses a tiny token budget — the model just needs to emit one word."""
    cache_key = f"classify:{model}:{source_hash}"
    if cache_key in cache.data:
        cached = cache.data[cache_key]
        return cached.get("category", "other"), cached.get("error")

    try:
        from openai import OpenAI
    except ImportError:
        return "other", "openai package not installed"

    api_key = _load_openai_key()
    if not api_key:
        return "other", "no OpenAI key available"

    client = OpenAI(api_key=api_key)
    img_b64 = base64.b64encode(image_path.read_bytes()).decode("ascii")

    try:
        resp = client.chat.completions.create(
            model=model,
            messages=[{
                "role": "user",
                "content": [
                    {"type": "text", "text": CLASSIFY_PROMPT},
                    {"type": "image_url",
                     "image_url": {"url": f"data:image/png;base64,{img_b64}",
                                   "detail": "low"}},
                ],
            }],
            max_tokens=8,
            temperature=0.0,
        )
        raw = (resp.choices[0].message.content or "").strip().lower()
    except Exception as exc:
        cache.data[cache_key] = {"category": "other", "error": str(exc)}
        cache.put(cache_key, VlmResult(error=str(exc)))   # touches disk
        return "other", str(exc)

    # Pick the first valid category word that appears in the response
    category = "other"
    for token in raw.replace(",", " ").split():
        if token in VALID_CATEGORIES:
            category = token
            break

    cache.data[cache_key] = {"category": category, "error": None}
    cache.path.write_text(json.dumps(cache.data, indent=2))
    return category, None


def load_prompts(path: Path) -> dict:
    return yaml.safe_load(path.read_text())

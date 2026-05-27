#!/usr/bin/env python3
"""
Generate images via OpenAI's image API from a prompt-library markdown file.

The prompt file format expected (matches what's in prompts/):
    ## Prompt
    ```
    <the actual prompt text>
    ```

Usage:
  .venv/bin/python generate.py --prompt prompts/shared_knob_brass_black.md
                               --output pedals/clean_boost/raw/knob/
                               --count 3

Reads the OpenAI key from $OPENAI_API_KEY or ~/.config/pedalforge/openai_key
(same fallback as vlm_qa.py).
"""

from __future__ import annotations

import argparse
import base64
import re
import sys
import time
from pathlib import Path

import vlm_qa


TOOL_DIR = Path(__file__).resolve().parent


def extract_prompt(md_path: Path) -> str:
    """Pull the first '## Prompt' fenced code block from a markdown file."""
    text = md_path.read_text(encoding="utf-8")
    m = re.search(r"## Prompt\s*\n+```\s*\n(.+?)\n```", text, re.DOTALL)
    if not m:
        raise ValueError(f"No '## Prompt' code block found in {md_path}")
    return m.group(1).strip()


# Rough public pricing snapshot for gpt-image-1 at the time of writing.
# Used only for the pre-flight cost preview — adjust if pricing moves.
COST_PER_IMAGE = {
    ("low",    "1024x1024"): 0.011,
    ("medium", "1024x1024"): 0.042,
    ("high",   "1024x1024"): 0.167,
    ("low",    "1024x1536"): 0.016,
    ("medium", "1024x1536"): 0.063,
    ("high",   "1024x1536"): 0.250,
    ("low",    "1536x1024"): 0.016,
    ("medium", "1536x1024"): 0.063,
    ("high",   "1536x1024"): 0.250,
}


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate images from a prompt-library markdown file")
    ap.add_argument("--prompt",  type=Path, required=True,
                    help="Path to a .md file containing a '## Prompt' fenced block")
    ap.add_argument("--output",  type=Path, required=True,
                    help="Directory to save generated PNGs into")
    ap.add_argument("--count",   type=int, default=3,
                    help="How many variants to generate (default: 3)")
    ap.add_argument("--model",   default="gpt-image-1",
                    help="OpenAI model (default: gpt-image-1)")
    ap.add_argument("--quality", default="high", choices=["low", "medium", "high", "auto"],
                    help="Image quality (default: high)")
    ap.add_argument("--size",    default="1024x1024",
                    choices=["1024x1024", "1024x1536", "1536x1024", "auto"],
                    help="Output dimensions (default: 1024x1024)")
    ap.add_argument("--dry-run", action="store_true",
                    help="Print the prompt + cost estimate and exit without calling the API")
    args = ap.parse_args()

    if not args.prompt.exists():
        print(f"ERROR: prompt file not found: {args.prompt}")
        return 1

    try:
        prompt_text = extract_prompt(args.prompt)
    except ValueError as exc:
        print(f"ERROR: {exc}")
        return 1

    est_cost = COST_PER_IMAGE.get((args.quality, args.size), 0.20) * args.count

    print(f"Prompt:  {args.prompt}")
    print(f"  preview: {prompt_text[:160]}{'…' if len(prompt_text) > 160 else ''}")
    print()
    print(f"Plan:    {args.count} × {args.model} @ {args.quality} quality, {args.size}")
    print(f"         estimated cost: ~${est_cost:.2f}")
    print(f"Output:  {args.output}")
    print()

    if args.dry_run:
        print("(dry run — no API call made)")
        return 0

    api_key = vlm_qa._load_openai_key()
    if not api_key:
        print(f"ERROR: no OpenAI key. Set OPENAI_API_KEY or write key to {vlm_qa.KEY_FALLBACK_PATH}")
        return 1

    try:
        from openai import OpenAI
    except ImportError:
        print("ERROR: openai package not installed (pip install openai)")
        return 1

    client = OpenAI(api_key=api_key)
    args.output.mkdir(parents=True, exist_ok=True)
    prompt_name = args.prompt.stem
    timestamp = time.strftime("%Y%m%d_%H%M%S")

    saved: list[Path] = []
    for i in range(args.count):
        idx = i + 1
        print(f"  [{idx}/{args.count}] generating…", end="", flush=True)
        t0 = time.time()
        try:
            resp = client.images.generate(
                model=args.model,
                prompt=prompt_text,
                n=1,
                size=args.size,
                quality=args.quality,
            )
            b64 = resp.data[0].b64_json
        except Exception as exc:
            elapsed = time.time() - t0
            print(f" FAILED after {elapsed:.1f}s: {exc}")
            return 1

        elapsed = time.time() - t0
        if not b64:
            print(f" no image data returned ({elapsed:.1f}s)")
            return 1

        img_bytes = base64.b64decode(b64)
        out_path = args.output / f"{prompt_name}_{timestamp}_{idx}.png"
        out_path.write_bytes(img_bytes)
        saved.append(out_path)
        print(f" → {out_path.name}  ({elapsed:.1f}s, {len(img_bytes) // 1024} KB)")

    print()
    print(f"Saved {len(saved)} image(s) to {args.output}")
    print(f"Next: .venv/bin/python qa_process.py --input {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

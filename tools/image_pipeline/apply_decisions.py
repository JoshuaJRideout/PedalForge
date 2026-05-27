#!/usr/bin/env python3
"""
Consume a `decisions.json` exported from the review HTML and copy
reviewer-approved processed PNGs into a final destination directory.

Workflow:
  1. Run qa_process.py — generates out/manifest.json + out/report.html
  2. Open out/report.html in a browser, click Pass/Fail on each card
  3. Click "Export Decisions" — downloads decisions.json
  4. Run this script — copies approved files to resources/images/processed/

Usage:
  python apply_decisions.py decisions.json                 # default dest
  python apply_decisions.py decisions.json --dest <path>   # custom dest
  python apply_decisions.py decisions.json --dry-run       # show what would happen
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOL_DIR  = Path(__file__).resolve().parent
DEFAULT_OUT = TOOL_DIR / "out"
DEFAULT_DEST = REPO_ROOT / "resources" / "images" / "processed"


def main() -> int:
    ap = argparse.ArgumentParser(description="Apply manual review decisions to processed images")
    ap.add_argument("decisions", type=Path, help="decisions.json exported from the review HTML")
    ap.add_argument("--source", type=Path, default=DEFAULT_OUT,
                    help=f"Directory holding the pipeline's out/ tree (default: {DEFAULT_OUT})")
    ap.add_argument("--dest", type=Path, default=DEFAULT_DEST,
                    help=f"Where to copy approved PNGs (default: {DEFAULT_DEST})")
    ap.add_argument("--dry-run", action="store_true",
                    help="Print what would happen, don't copy anything")
    args = ap.parse_args()

    if not args.decisions.exists():
        print(f"ERROR: decisions file not found: {args.decisions}")
        return 1

    decisions_doc = json.loads(args.decisions.read_text())
    decisions = decisions_doc.get("decisions", {})

    manifest_path = args.source / "manifest.json"
    if not manifest_path.exists():
        print(f"ERROR: manifest not found at {manifest_path}")
        print("  Did you run qa_process.py first?")
        return 1
    manifest = json.loads(manifest_path.read_text())
    by_hash = {m["source_hash"]: m for m in manifest}

    pass_count = 0
    fail_count = 0
    skip_count = 0
    missing    = []

    print(f"Reading decisions from {args.decisions}")
    print(f"Reading processed files from {args.source}")
    print(f"{'(dry run) ' if args.dry_run else ''}Copying to {args.dest}")
    print()

    for src_hash, d in decisions.items():
        verdict = d.get("verdict")
        if verdict == "fail":
            fail_count += 1
            continue
        if verdict != "pass":
            skip_count += 1
            continue

        manifest_entry = by_hash.get(src_hash)
        if not manifest_entry:
            missing.append(d.get("source_filename", src_hash))
            continue

        out_path = manifest_entry.get("output_path")
        if not out_path:
            # Pipeline didn't produce an output (e.g. CV-rejected) but the
            # reviewer marked it pass. We can copy the source instead — it
            # at least lands in dest for further work.
            print(f"  [no output] {d.get('source_filename', src_hash)} — no processed file (reviewer marked pass)")
            skip_count += 1
            continue

        processed = args.source / out_path
        if not processed.exists():
            missing.append(out_path)
            continue

        category = manifest_entry.get("category", "uncategorized")
        dest_dir = args.dest / category
        # Keep the original source filename if possible; otherwise hash.
        src_name = d.get("source_filename") or processed.name
        dest_path = dest_dir / Path(src_name).with_suffix(".png").name

        suffix = 2
        while dest_path.exists():
            base = Path(src_name).stem
            dest_path = dest_dir / f"{base}_{suffix}.png"
            suffix += 1

        if args.dry_run:
            print(f"  [pass] {processed.relative_to(args.source)} → {dest_path.relative_to(args.dest.parent)}")
        else:
            dest_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(processed, dest_path)
            print(f"  copied {processed.name} → {dest_path}")

        # Write notes alongside if present
        if d.get("notes") and not args.dry_run:
            (dest_path.with_suffix(".notes.txt")).write_text(d["notes"])

        pass_count += 1

    print()
    print(f"  passed:  {pass_count}{' (would copy)' if args.dry_run else ' copied'}")
    print(f"  failed:  {fail_count} (skipped)")
    print(f"  no verdict: {skip_count} (skipped)")
    if missing:
        print(f"  missing: {len(missing)} — files referenced in decisions but not found:")
        for m in missing[:5]:
            print(f"    {m}")
        if len(missing) > 5:
            print(f"    ... and {len(missing) - 5} more")

    return 0


if __name__ == "__main__":
    sys.exit(main())

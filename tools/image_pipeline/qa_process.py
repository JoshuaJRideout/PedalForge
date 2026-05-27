#!/usr/bin/env python3
"""
PedalForge image pipeline — QA + post-process.

Reads raw AI-generated images, scores them on objective quality metrics,
removes backgrounds, crops to subject, resizes to a category-standard, and
emits an HTML report you can scan to manually approve borderline outputs.

See README.md for the full workflow.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional

import cv2
import numpy as np
import yaml
from PIL import Image
from rembg import remove as rembg_remove, new_session as rembg_new_session

import vlm_qa


# Cached rembg session — model file is downloaded on first use, kept loaded
# across calls so we don't pay the load cost per image.
_rembg_session = None
_rembg_model_loaded: Optional[str] = None

def _get_rembg_session(model_name: str):
    """Lazily build a rembg Session for the chosen model and reuse it."""
    global _rembg_session, _rembg_model_loaded
    if _rembg_session is None or _rembg_model_loaded != model_name:
        _rembg_session = rembg_new_session(model_name)
        _rembg_model_loaded = model_name
    return _rembg_session


# ─────────────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────────────

REPO_ROOT  = Path(__file__).resolve().parents[2]
TOOL_DIR   = Path(__file__).resolve().parent
OUT_DIR    = TOOL_DIR / "out"
DEFAULT_INPUTS = [
    Path.home() / "Desktop" / "Background",
    Path.home() / "Desktop" / "Removed",
]

# Acceptance thresholds. Tuned conservatively — easier to nudge looser later
# than to find out we shipped blurry images.
SHARPNESS_MIN   = 80.0    # Laplacian variance — below this is visibly blurry
SUBJECT_MIN_FRAC = 0.30   # subject's bbox area / canvas area
PADDING_FRACTION = 0.06   # whitespace around the cropped subject

SUPPORTED_EXT = {".png", ".jpg", ".jpeg", ".webp"}


# ─────────────────────────────────────────────────────────────────────────────
# Data model
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class Scores:
    sharpness: float       = 0.0   # Laplacian variance — higher is sharper
    bg_purity: float       = 0.0   # 0..1, 1 = edge pixels are uniformly one colour
    subject_fraction: float = 0.0  # 0..1, subject bbox area / canvas area
    centeredness: float    = 0.0   # 0..1, 1 = subject is canvas-centred
    aspect_class: str      = ""    # "square" | "wide" | "tall"
    has_alpha: bool        = False

@dataclass
class Result:
    source_path: str
    source_hash: str
    decision: str = "approved"      # approved | review | rejected — optimistic default
    reason:   str = ""
    category: str = "uncategorized"
    output_path: Optional[str] = None
    scores: Scores = field(default_factory=Scores)
    vlm:      Optional[dict] = None  # VLM verdict (when --vlm enabled)


# ─────────────────────────────────────────────────────────────────────────────
# QA scoring
# ─────────────────────────────────────────────────────────────────────────────

def compute_sharpness(img_bgr: np.ndarray) -> float:
    """Laplacian variance — the standard cheap focus measure. Higher = sharper.

    For 1024×1024 product photos, anything under ~80 reads visibly soft; well-
    focused gens are usually 200+.
    """
    gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
    return float(cv2.Laplacian(gray, cv2.CV_64F).var())


def compute_bg_purity(img_bgr: np.ndarray) -> float:
    """How uniform is the edge ring? Used as a hint that the background is
    cleanly removable. 1.0 = perfectly uniform; <0.5 = busy background, likely
    rembg will leave artifacts.
    """
    h, w = img_bgr.shape[:2]
    edge_width = max(2, min(h, w) // 50)
    top    = img_bgr[:edge_width, :, :].reshape(-1, 3)
    bottom = img_bgr[-edge_width:, :, :].reshape(-1, 3)
    left   = img_bgr[:, :edge_width, :].reshape(-1, 3)
    right  = img_bgr[:, -edge_width:, :].reshape(-1, 3)
    edge_px = np.vstack([top, bottom, left, right]).astype(np.float32)
    # Variance per channel, normalised to a 0..1 score (max possible σ ≈ 127).
    std = edge_px.std(axis=0).mean()
    return float(max(0.0, 1.0 - (std / 60.0)))


def subject_bbox_from_alpha(img_rgba: np.ndarray) -> Optional[tuple[int, int, int, int]]:
    """Tight bbox around non-transparent pixels. None if alpha is empty/full."""
    alpha = img_rgba[:, :, 3]
    mask = alpha > 16
    if not mask.any():
        return None
    ys, xs = np.where(mask)
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def subject_bbox_from_bg(img_bgr: np.ndarray) -> Optional[tuple[int, int, int, int]]:
    """Heuristic bbox when there's no alpha channel — assume the background is
    the most common edge colour and the subject is everything different. Cheap
    and works for AI-generated product photos with mostly-clean backgrounds."""
    h, w = img_bgr.shape[:2]
    # Sample edge pixels to estimate the background colour
    edge_width = max(2, min(h, w) // 100)
    edge = np.vstack([
        img_bgr[:edge_width, :, :].reshape(-1, 3),
        img_bgr[-edge_width:, :, :].reshape(-1, 3),
        img_bgr[:, :edge_width, :].reshape(-1, 3),
        img_bgr[:, -edge_width:, :].reshape(-1, 3),
    ])
    bg_color = np.median(edge, axis=0)
    diff = np.linalg.norm(img_bgr.astype(np.float32) - bg_color, axis=2)
    mask = diff > 30  # 30 = a forgiving similarity threshold in RGB-distance
    if not mask.any():
        return None
    ys, xs = np.where(mask)
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def compute_subject_metrics(bbox: tuple[int, int, int, int], w: int, h: int) -> tuple[float, float]:
    """Returns (subject_fraction, centeredness) given a bbox and canvas size."""
    x0, y0, x1, y1 = bbox
    bw, bh = (x1 - x0), (y1 - y0)
    subject_fraction = (bw * bh) / float(w * h)

    bx_centre = (x0 + x1) * 0.5
    by_centre = (y0 + y1) * 0.5
    dx = (bx_centre - w * 0.5) / (w * 0.5)
    dy = (by_centre - h * 0.5) / (h * 0.5)
    centeredness = 1.0 - min(1.0, (dx * dx + dy * dy) ** 0.5)
    return float(subject_fraction), float(centeredness)


def classify_aspect(w: int, h: int) -> str:
    r = w / float(h)
    if 0.9 <= r <= 1.1:
        return "square"
    return "wide" if r > 1.0 else "tall"


# ─────────────────────────────────────────────────────────────────────────────
# Processing
# ─────────────────────────────────────────────────────────────────────────────

def load_image(path: Path) -> tuple[np.ndarray, bool]:
    """Load via PIL → BGR(A) numpy. Returns (image, has_alpha)."""
    pil = Image.open(path).convert("RGBA")
    arr = np.array(pil)
    # PIL gives RGBA; OpenCV expects BGRA for its functions
    bgra = cv2.cvtColor(arr, cv2.COLOR_RGBA2BGRA)
    alpha = bgra[:, :, 3]
    # If alpha is fully opaque (255 everywhere), treat as no-alpha — the source
    # actually has a background to remove.
    has_alpha = bool((alpha < 255).any())
    return bgra, has_alpha


def remove_background(src_path: Path,
                      model: str = "isnet-general-use",
                      alpha_matting: bool = False) -> np.ndarray:
    """Run rembg on the source path; return BGRA numpy array.

    Defaults to isnet-general-use (sharper edges than U2Net on hardware).
    alpha_matting is slower (~3x) but cleaner on hair / fuzzy edges — usually
    unnecessary for hardware product shots; left as an opt-in flag."""
    session = _get_rembg_session(model)
    kwargs = {"session": session}
    if alpha_matting:
        kwargs.update({
            "alpha_matting": True,
            "alpha_matting_foreground_threshold": 240,
            "alpha_matting_background_threshold": 10,
            "alpha_matting_erode_size": 5,
        })
    with src_path.open("rb") as fh:
        out_bytes = rembg_remove(fh.read(), **kwargs)
    pil = Image.open(io.BytesIO(out_bytes)).convert("RGBA")
    arr = np.array(pil)
    return cv2.cvtColor(arr, cv2.COLOR_RGBA2BGRA)


def fill_alpha_holes(img_bgra: np.ndarray, presence_threshold: int = 16) -> np.ndarray:
    """Fill transparent regions that are entirely surrounded by subject pixels.

    rembg's segmentation models occasionally classify the bright saturated
    core of a glowing element (LED, screen) as background, leaving a hole
    in the alpha mask. The subsequent edge hardening then makes that hole
    fully transparent — punching a literal void through the middle of the
    object.

    Fix: identify "true outside" via flood-fill from the corners. Any
    transparent region NOT connected to the outside is an interior hole;
    set its alpha to fully opaque so the source pixel colour survives."""
    alpha = img_bgra[:, :, 3]
    is_subject = (alpha > presence_threshold).astype(np.uint8) * 255

    # Pad by 1 so the flood always has a connected outside boundary
    padded = cv2.copyMakeBorder(is_subject, 1, 1, 1, 1, cv2.BORDER_CONSTANT, value=0)
    inverted = cv2.bitwise_not(padded)            # subject = 0, non-subject = 255
    flooded  = inverted.copy()
    # Flood from each corner — anything reachable is true background
    cv2.floodFill(flooded, None, (0, 0),                                   0)
    cv2.floodFill(flooded, None, (flooded.shape[1] - 1, 0),                0)
    cv2.floodFill(flooded, None, (0, flooded.shape[0] - 1),                0)
    cv2.floodFill(flooded, None, (flooded.shape[1] - 1, flooded.shape[0] - 1), 0)
    # What's still 255 in `flooded` is an interior hole.
    holes = flooded[1:-1, 1:-1]

    out = img_bgra.copy()
    out[:, :, 3] = np.where(holes > 0, 255, alpha)
    return out


def harden_alpha_edges(img_bgra: np.ndarray,
                       low: int = 32, high: int = 220, erode_px: int = 0) -> np.ndarray:
    """Sharpen the alpha channel to cut fuzzy halos left by rembg.

    Pixels with alpha < `low` → fully transparent
    Pixels with alpha > `high` → fully opaque
    Pixels in between → remapped to a steep ramp across [low, high]

    Optional `erode_px` erodes the alpha mask inward by N px to bite into any
    remaining edge halo. Use 1 for a typical fuzz reduction; 0 to skip."""
    alpha = img_bgra[:, :, 3].astype(np.int32)
    # Steep ramp: clip to [low, high], then linearly stretch to [0, 255]
    alpha = np.clip(alpha, low, high)
    alpha = ((alpha - low) * 255 // max(1, (high - low))).astype(np.uint8)

    if erode_px > 0:
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * erode_px + 1,) * 2)
        alpha = cv2.erode(alpha, kernel, iterations=1)

    out = img_bgra.copy()
    out[:, :, 3] = alpha
    return out


def crop_to_subject(img_bgra: np.ndarray, padding_fraction: float) -> np.ndarray:
    bbox = subject_bbox_from_alpha(img_bgra)
    if bbox is None:
        return img_bgra
    h, w = img_bgra.shape[:2]
    x0, y0, x1, y1 = bbox
    bw = x1 - x0
    bh = y1 - y0
    pad = int(max(bw, bh) * padding_fraction)
    x0 = max(0, x0 - pad)
    y0 = max(0, y0 - pad)
    x1 = min(w, x1 + pad + 1)
    y1 = min(h, y1 + pad + 1)
    return img_bgra[y0:y1, x0:x1].copy()


def resize_longest(img_bgra: np.ndarray, target: int) -> np.ndarray:
    h, w = img_bgra.shape[:2]
    scale = target / float(max(h, w))
    if abs(scale - 1.0) < 0.001:
        return img_bgra
    new_w = max(1, int(round(w * scale)))
    new_h = max(1, int(round(h * scale)))
    interp = cv2.INTER_AREA if scale < 1.0 else cv2.INTER_LANCZOS4
    return cv2.resize(img_bgra, (new_w, new_h), interpolation=interp)


def save_png(img_bgra: np.ndarray, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    rgba = cv2.cvtColor(img_bgra, cv2.COLOR_BGRA2RGBA)
    Image.fromarray(rgba).save(dest, "PNG", optimize=True)


# ─────────────────────────────────────────────────────────────────────────────
# Pipeline
# ─────────────────────────────────────────────────────────────────────────────

def hash_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()[:12]


def process_one(src_path: Path,
                category_cfg: dict,
                out_dir: Path,
                category: str = "uncategorized",
                vlm_ctx: Optional[dict] = None,
                rembg_model: str = "isnet-general-use",
                alpha_matting: bool = False,
                edge_erode_px: int = 0,
                fill_holes: bool = True) -> Result:
    src_hash = hash_file(src_path)
    res = Result(source_path=str(src_path), source_hash=src_hash, category=category)

    try:
        img_bgra, has_alpha = load_image(src_path)
    except Exception as exc:
        res.decision = "rejected"
        res.reason = f"failed to load: {exc}"
        return res

    h, w = img_bgra.shape[:2]
    res.scores.has_alpha = has_alpha
    res.scores.aspect_class = classify_aspect(w, h)
    res.scores.sharpness = compute_sharpness(cv2.cvtColor(img_bgra, cv2.COLOR_BGRA2BGR))

    if not has_alpha:
        res.scores.bg_purity = compute_bg_purity(cv2.cvtColor(img_bgra, cv2.COLOR_BGRA2BGR))

    # Sharpness gate first — cheap to compute, common failure mode.
    if res.scores.sharpness < SHARPNESS_MIN:
        res.decision = "rejected"
        res.reason = f"too soft (laplacian variance {res.scores.sharpness:.0f} < {SHARPNESS_MIN:.0f})"
        return res

    # Background removal (if needed) so the subject metrics work off real alpha.
    try:
        if not has_alpha:
            img_bgra = remove_background(src_path,
                                         model=rembg_model,
                                         alpha_matting=alpha_matting)
    except Exception as exc:
        res.decision = "rejected"
        res.reason = f"rembg failed: {exc}"
        return res

    # Fill any holes the segmentation model punched through the subject
    # (saturated LED cores, bright screen pixels). Has to happen BEFORE edge
    # hardening — otherwise the hardening turns the holes into transparent
    # voids and we lose the bright pixels entirely.
    if fill_holes:
        img_bgra = fill_alpha_holes(img_bgra)

    # Always harden alpha edges — rembg's output ranges from soft on U2Net to
    # already-decent on isnet, but a quick threshold pass kills any residual
    # halo on the inputs that come in with pre-existing alpha too.
    img_bgra = harden_alpha_edges(img_bgra, low=32, high=220, erode_px=edge_erode_px)

    bbox = subject_bbox_from_alpha(img_bgra)
    if bbox is None:
        res.decision = "rejected"
        res.reason = "no subject detected after bg removal"
        return res

    res.scores.subject_fraction, res.scores.centeredness = compute_subject_metrics(
        bbox, img_bgra.shape[1], img_bgra.shape[0])

    # ── VLM auto-categorization (optional) ───────────────────────────────
    # Runs BEFORE the category-dependent subject-fraction gate and resize so
    # those use the right thresholds for the detected category. When the
    # input batch is mixed (chassis + LEDs + switches + ...) this routes
    # each image to its own rubric instead of judging everything as one type.
    if vlm_ctx is not None and vlm_ctx.get("auto_category"):
        detected, cls_err = vlm_qa.classify_image(
            image_path=src_path,
            source_hash=res.source_hash,
            cache=vlm_ctx["cache"],
            model=vlm_ctx["classify_model"],
        )
        if not cls_err and detected:
            res.category = detected
            category_cfg = vlm_ctx["cats"].get(detected, category_cfg)

    min_frac = category_cfg.get("min_subject_fraction", SUBJECT_MIN_FRAC)
    if res.scores.subject_fraction < min_frac:
        # Below the firm threshold — review rather than reject. Sometimes the
        # subject just IS small and that's intentional (an LED, e.g.).
        res.decision = "review"
        res.reason = (f"subject covers {res.scores.subject_fraction:.0%} of canvas "
                      f"(category expects ≥ {min_frac:.0%})")

    cropped = crop_to_subject(img_bgra, PADDING_FRACTION)
    target_size = category_cfg.get("output_size", 1024)
    final = resize_longest(cropped, target_size)

    if res.decision == "approved" and res.scores.bg_purity > 0 and res.scores.bg_purity < 0.5:
        # Edges weren't clean — bump down to review. rembg may have left
        # halos that need a manual fix.
        res.decision = "review"
        res.reason = res.reason or f"messy bg edges (purity {res.scores.bg_purity:.2f})"

    # ── Semantic VLM check ────────────────────────────────────────────────
    # Pure CV can't tell a 3/4 view from a top-down, or spot an internal
    # mechanical part poking out where the enclosure should be. The VLM sees
    # the image and judges against a category-specific rubric.
    # Runs on the ORIGINAL source (not the cropped/resized output) so the
    # VLM judges what was actually generated, not what we've already touched.
    if vlm_ctx is not None and vlm_ctx.get("run_rubric"):
        vlm_result = vlm_qa.check_image(
            image_path=src_path,
            category=res.category,
            source_hash=res.source_hash,
            prompts_doc=vlm_ctx["prompts"],
            cache=vlm_ctx["cache"],
            model=vlm_ctx["model"],
        )
        res.vlm = {
            "overall": vlm_result.overall,
            "summary": vlm_result.summary,
            "checks":  {k: asdict(v) for k, v in vlm_result.checks.items()},
            "model":   vlm_result.model,
            "error":   vlm_result.error,
        }

        if vlm_result.error:
            # VLM errored out — don't auto-fail the image, just leave the CV
            # decision and surface the error in the report.
            pass
        elif vlm_result.overall == "REJECT":
            res.decision = "rejected"
            failing = [n for n, c in vlm_result.checks.items() if c.verdict == "FAIL"]
            res.reason = f"VLM: {vlm_result.summary}" + (f" (failed: {', '.join(failing)})" if failing else "")
        elif vlm_result.overall == "REVIEW":
            if res.decision == "approved":
                res.decision = "review"
            if not res.reason:
                res.reason = f"VLM review: {vlm_result.summary}"
        # ACCEPT → leave CV decision (approved or review-from-CV)

    dest_dir = out_dir / res.decision / res.category
    dest = dest_dir / f"{src_hash}.png"
    save_png(final, dest)
    res.output_path = str(dest.relative_to(out_dir))
    return res


def gather_inputs(inputs: list[Path]) -> list[Path]:
    files: list[Path] = []
    for d in inputs:
        if not d.exists():
            print(f"[skip] {d} (not found)")
            continue
        for p in sorted(d.iterdir()):
            if p.suffix.lower() in SUPPORTED_EXT and not p.name.startswith("."):
                files.append(p)
    return files


def write_report(results: list[Result], out_dir: Path) -> None:
    """Generate the interactive review page. localStorage-backed pass/fail
    per image, notes textarea, filter, export-as-JSON. Reusable across batches
    — opens whatever manifest.json sits next to it."""

    def card(r: Result) -> str:
        thumb_src = r.output_path or r.source_path
        src_disp  = Path(r.source_path).name
        score_lines = "\n".join(
            f"<li><b>{k}</b>: {v:.3f}" if isinstance(v, float) else f"<li><b>{k}</b>: {v}"
            for k, v in asdict(r.scores).items())

        vlm_block = ""
        if r.vlm:
            if r.vlm.get("error"):
                vlm_block = f'<div class="vlm-err">VLM error: {r.vlm["error"]}</div>'
            else:
                check_lines = ""
                for name, c in (r.vlm.get("checks") or {}).items():
                    klass = "ok" if c.get("verdict") == "PASS" else "fail"
                    check_lines += f'<li class="{klass}"><b>{name}</b>: {c.get("reason","")}'
                vlm_block = (
                    f'<div class="vlm"><b>VLM:</b> {r.vlm.get("overall","")} — '
                    f'{r.vlm.get("summary","")}'
                    f'<ul class="vlm-checks">{check_lines}</ul></div>'
                )

        # Reason chip from auto-QA (sharpness, subject-fraction, etc.) — still
        # advisory, but worth surfacing so the reviewer sees why it landed
        # where it did.
        reason_block = f'<div class="reason">{r.reason}</div>' if r.reason else ''

        return f"""
<div class="card auto-{r.decision}"
     data-hash="{r.source_hash}"
     data-filename="{src_disp}"
     data-category="{r.category}">
  <img src="{thumb_src}" loading="lazy"/>
  <div class="meta">
    <div class="row">
      <span class="chip chip-{r.decision}">{r.decision.upper()}</span>
      <span class="cat">{r.category}</span>
    </div>
    <div class="src">{src_disp}</div>
    {reason_block}
    {vlm_block}
    <ul class="scores">{score_lines}</ul>

    <div class="review">
      <div class="review-btns">
        <button class="btn-pass" data-action="pass" title="Approve (P)">Pass</button>
        <button class="btn-fail" data-action="fail" title="Reject (F)">Fail</button>
        <button class="btn-clear" data-action="clear" title="Clear (C)">Reset</button>
      </div>
      <textarea class="notes" placeholder="Notes (optional)"></textarea>
    </div>
  </div>
</div>"""

    # Order: pending CV-review first, then approved, then rejected.
    # Reviewer probably wants to triage uncertain ones first.
    sections = {"review": [], "approved": [], "rejected": []}
    for r in results:
        sections.setdefault(r.decision, []).append(r)

    body = ""
    for sec in ("review", "approved", "rejected"):
        items = sections.get(sec, [])
        if not items: continue
        body += f'<h2 data-auto="{sec}">{sec.title()} ({len(items)})</h2><div class="grid">'
        body += "\n".join(card(r) for r in items)
        body += "</div>"

    html = f"""<!doctype html><html><head><meta charset="utf-8"><title>Image Review</title>
<style>
  :root {{ --pass: #10b981; --fail: #ef4444; --warn: #f59e0b; --pend: #555; }}
  body {{ font-family: -apple-system, sans-serif; background: #0e0e10; color: #ddd; margin: 0; }}
  header {{ position: sticky; top: 0; z-index: 10; background: #181820; border-bottom: 1px solid #2c2c34; padding: 12px 20px; display: flex; align-items: center; gap: 16px; flex-wrap: wrap; }}
  header h1 {{ margin: 0; font-size: 18px; }}
  header .summary {{ color: #94a3b8; font-size: 13px; }}
  header button {{ background: #2c2c38; color: #ddd; border: 1px solid #3a3a4a; padding: 6px 12px; border-radius: 5px; cursor: pointer; font-size: 13px; }}
  header button:hover {{ background: #3a3a4a; }}
  header button.active {{ background: #6366f1; color: white; border-color: #6366f1; }}
  main {{ padding: 16px 20px 32px; }}
  h2 {{ color: #e2e8f0; font-size: 15px; margin-top: 24px; text-transform: uppercase; letter-spacing: 0.05em; opacity: 0.7; }}
  .grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 14px; }}
  .card {{ background: #1c1c24; border-radius: 8px; padding: 10px; border: 2px solid #2c2c38; transition: border-color 0.15s ease; }}
  .card.user-pass {{ border-color: var(--pass); box-shadow: 0 0 0 2px rgba(16, 185, 129, 0.15); }}
  .card.user-fail {{ border-color: var(--fail); box-shadow: 0 0 0 2px rgba(239, 68, 68, 0.15); opacity: 0.55; }}
  .card img {{ width: 100%; height: 220px; object-fit: contain; background: #2c2c38; border-radius: 4px; cursor: zoom-in; }}
  .row {{ display: flex; align-items: center; justify-content: space-between; margin-top: 8px; gap: 8px; }}
  .chip {{ font-size: 10px; font-weight: 700; letter-spacing: 0.05em; padding: 3px 7px; border-radius: 3px; background: #2c2c38; color: #94a3b8; }}
  .chip-approved {{ background: rgba(16, 185, 129, 0.2); color: #6ee7b7; }}
  .chip-review   {{ background: rgba(245, 158, 11, 0.2); color: #fbbf24; }}
  .chip-rejected {{ background: rgba(239, 68, 68, 0.2); color: #fca5a5; }}
  .cat {{ font-size: 11px; color: #94a3b8; }}
  .src {{ font-family: ui-monospace, monospace; color: #6b7280; font-size: 10px; word-break: break-all; margin-top: 4px; }}
  .reason {{ color: var(--warn); font-size: 11px; margin: 6px 0; }}
  .scores {{ font-size: 10px; color: #6b7280; padding-left: 1em; margin: 6px 0; max-height: 60px; overflow-y: auto; }}
  .vlm {{ font-size: 11px; color: #cbd5e1; margin-top: 6px; padding: 6px; background: #15151c; border-radius: 4px; }}
  .vlm-checks {{ padding-left: 1em; margin: 4px 0 0 0; font-size: 10px; }}
  .vlm-checks .ok   {{ color: #34d399; }}
  .vlm-checks .fail {{ color: #f87171; }}
  .vlm-err {{ color: #f87171; font-size: 10px; margin-top: 6px; }}
  .review {{ margin-top: 10px; padding-top: 10px; border-top: 1px solid #2c2c38; }}
  .review-btns {{ display: flex; gap: 4px; margin-bottom: 8px; }}
  .review-btns button {{ flex: 1; background: #2c2c38; color: #94a3b8; border: 1px solid #3a3a4a; padding: 5px; border-radius: 4px; cursor: pointer; font-size: 12px; font-weight: 600; }}
  .review-btns button:hover {{ background: #3a3a4a; color: #ddd; }}
  .card.user-pass .btn-pass {{ background: var(--pass); color: white; border-color: var(--pass); }}
  .card.user-fail .btn-fail {{ background: var(--fail); color: white; border-color: var(--fail); }}
  .notes {{ width: 100%; background: #15151c; color: #ddd; border: 1px solid #2c2c38; border-radius: 4px; padding: 6px; font-size: 11px; font-family: inherit; resize: vertical; min-height: 32px; box-sizing: border-box; }}
  .notes:focus {{ outline: none; border-color: #6366f1; }}
  body.filter-pending .card:not(.user-pending) {{ display: none; }}
  body.filter-pass    .card:not(.user-pass)    {{ display: none; }}
  body.filter-fail    .card:not(.user-fail)    {{ display: none; }}
</style></head>
<body>
<header>
  <h1>Image Review</h1>
  <span class="summary" id="summary">…</span>
  <span style="flex: 1"></span>
  <button data-filter="all"     class="active">All</button>
  <button data-filter="pending">Pending</button>
  <button data-filter="pass">Pass</button>
  <button data-filter="fail">Fail</button>
  <button id="export">Export Decisions</button>
  <button id="clear-all">Clear All</button>
</header>
<main>
{body}
</main>

<script>
const STORAGE_PREFIX = 'pfimg:';

function loadDecision(hash) {{
  try {{
    return JSON.parse(localStorage.getItem(STORAGE_PREFIX + hash) || 'null')
           || {{ verdict: null, notes: '' }};
  }} catch {{ return {{ verdict: null, notes: '' }}; }}
}}
function saveDecision(hash, decision) {{
  if (!decision.verdict && !decision.notes)
    localStorage.removeItem(STORAGE_PREFIX + hash);
  else
    localStorage.setItem(STORAGE_PREFIX + hash, JSON.stringify(decision));
  updateSummary();
}}

function applyCardState(card) {{
  const hash = card.dataset.hash;
  const d = loadDecision(hash);
  card.classList.remove('user-pass', 'user-fail', 'user-pending');
  if      (d.verdict === 'pass') card.classList.add('user-pass');
  else if (d.verdict === 'fail') card.classList.add('user-fail');
  else                            card.classList.add('user-pending');
  card.querySelector('.notes').value = d.notes || '';
}}

function updateSummary() {{
  const cards = document.querySelectorAll('.card');
  let p = 0, f = 0, pending = 0;
  cards.forEach(c => {{
    const d = loadDecision(c.dataset.hash);
    if      (d.verdict === 'pass') p++;
    else if (d.verdict === 'fail') f++;
    else                           pending++;
  }});
  document.getElementById('summary').textContent =
    `${{cards.length}} total · ${{p}} pass · ${{f}} fail · ${{pending}} pending`;
}}

document.querySelectorAll('.card').forEach(card => {{
  applyCardState(card);

  card.querySelectorAll('.review-btns button').forEach(btn => {{
    btn.addEventListener('click', () => {{
      const action = btn.dataset.action;
      const d = loadDecision(card.dataset.hash);
      if      (action === 'pass')  d.verdict = 'pass';
      else if (action === 'fail')  d.verdict = 'fail';
      else                          d.verdict = null;
      saveDecision(card.dataset.hash, d);
      applyCardState(card);
    }});
  }});

  let debounce;
  card.querySelector('.notes').addEventListener('input', e => {{
    clearTimeout(debounce);
    debounce = setTimeout(() => {{
      const d = loadDecision(card.dataset.hash);
      d.notes = e.target.value;
      saveDecision(card.dataset.hash, d);
    }}, 300);
  }});

  card.querySelector('img').addEventListener('click', () => {{
    window.open(card.querySelector('img').src, '_blank');
  }});
}});

// Filter
document.querySelectorAll('header button[data-filter]').forEach(btn => {{
  btn.addEventListener('click', () => {{
    document.querySelectorAll('header button[data-filter]').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    document.body.className = btn.dataset.filter === 'all' ? '' : 'filter-' + btn.dataset.filter;
  }});
}});

// Export
document.getElementById('export').addEventListener('click', () => {{
  const out = {{ version: 1, exported_at: new Date().toISOString(), decisions: {{}} }};
  document.querySelectorAll('.card').forEach(c => {{
    const d = loadDecision(c.dataset.hash);
    if (d.verdict || d.notes) {{
      out.decisions[c.dataset.hash] = {{
        verdict: d.verdict,
        notes: d.notes,
        source_filename: c.dataset.filename,
        category: c.dataset.category,
      }};
    }}
  }});
  const blob = new Blob([JSON.stringify(out, null, 2)], {{ type: 'application/json' }});
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'decisions.json';
  a.click();
}});

// Clear all
document.getElementById('clear-all').addEventListener('click', () => {{
  if (!confirm('Clear all pass/fail decisions and notes for this batch?')) return;
  document.querySelectorAll('.card').forEach(c => {{
    localStorage.removeItem(STORAGE_PREFIX + c.dataset.hash);
    applyCardState(c);
  }});
  updateSummary();
}});

// Keyboard shortcuts: hover a card + press P/F/C
let lastHover = null;
document.querySelectorAll('.card').forEach(c => {{
  c.addEventListener('mouseenter', () => lastHover = c);
}});
document.addEventListener('keydown', e => {{
  if (!lastHover) return;
  if (e.target.tagName === 'TEXTAREA') return;
  const map = {{ p: 'pass', f: 'fail', c: 'clear' }};
  const a = map[e.key.toLowerCase()];
  if (!a) return;
  e.preventDefault();
  const d = loadDecision(lastHover.dataset.hash);
  d.verdict = a === 'clear' ? null : a;
  saveDecision(lastHover.dataset.hash, d);
  applyCardState(lastHover);
}});

updateSummary();
</script>
</body></html>"""
    (out_dir / "report.html").write_text(html, encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(description="PedalForge image QA + processor")
    ap.add_argument("--input", action="append", type=Path,
                    help="Source folder (repeat for multiple). Default: ~/Desktop/Background + ~/Desktop/Removed")
    ap.add_argument("--out", type=Path, default=OUT_DIR, help="Output directory")
    ap.add_argument("--limit", type=int, default=None, help="Process at most N images")
    ap.add_argument("--category", default="uncategorized",
                    help="Category key from categories.yaml")
    ap.add_argument("--vlm", action="store_true",
                    help="Enable VLM semantic QA (requires OPENAI_API_KEY). "
                         "Catches wrong viewing angle, visible internals, etc. "
                         "Approx $0.005 per image with gpt-4o-mini.")
    ap.add_argument("--vlm-model", default="gpt-4o-mini",
                    help="Vision LLM to use (default: gpt-4o-mini)")
    ap.add_argument("--auto-category", action="store_true",
                    help="Have the VLM classify each image into a category "
                         "(chassis/knob/led/footswitch/screen/pedalboard/other) "
                         "before running its rubric. Use this when --input "
                         "contains a mixed batch. Adds one VLM call per image.")
    ap.add_argument("--classify-model", default=None,
                    help="Model to use for classification (defaults to "
                         "--vlm-model). Try gpt-4o for tougher visual cases "
                         "like LED-vs-knob ambiguity; gpt-4o-mini for cost.")
    ap.add_argument("--rembg-model", default="isnet-general-use",
                    help="rembg model. isnet-general-use (default) gives "
                         "sharper edges on hardware than u2net.")
    ap.add_argument("--alpha-matting", action="store_true",
                    help="Enable rembg alpha matting — slower (~3x) but "
                         "cleaner edges. Usually unnecessary for hard-edged "
                         "hardware shots.")
    ap.add_argument("--edge-erode", type=int, default=0,
                    help="Erode the alpha mask inward by N px after rembg. "
                         "Use 1 to bite into any residual halo, 0 to skip "
                         "(default).")
    args = ap.parse_args()

    inputs = args.input if args.input else DEFAULT_INPUTS

    cats = yaml.safe_load((TOOL_DIR / "categories.yaml").read_text())
    cat_cfg = cats.get(args.category, cats["uncategorized"])

    files = gather_inputs(inputs)
    if args.limit:
        files = files[: args.limit]

    if not files:
        print("No images found. Pointed at:")
        for d in inputs: print(f"  {d}")
        return 1

    print(f"Processing {len(files)} image(s) → {args.out}")
    args.out.mkdir(parents=True, exist_ok=True)

    vlm_ctx: Optional[dict] = None
    if args.vlm or args.auto_category:
        if not vlm_qa._load_openai_key():
            print("ERROR: --vlm/--auto-category need an OpenAI key. Set "
                  f"OPENAI_API_KEY in env or write it to {vlm_qa.KEY_FALLBACK_PATH}")
            return 1
        prompts_doc = vlm_qa.load_prompts(TOOL_DIR / "vlm_prompts.yaml")
        cache = vlm_qa.VlmCache(args.out / "vlm_cache.json")
        vlm_ctx = {
            "prompts": prompts_doc,
            "cache": cache,
            "model": args.vlm_model,
            "classify_model": args.classify_model or args.vlm_model,
            "auto_category": args.auto_category,
            "cats": cats,
            "run_rubric": args.vlm,
        }
        flags = []
        if args.auto_category: flags.append("auto-category")
        if args.vlm:           flags.append("rubric check")
        print(f"VLM enabled ({' + '.join(flags)}) — model {args.vlm_model}, "
              f"cache at {cache.path.name}")

    results: list[Result] = []
    for i, p in enumerate(files, 1):
        print(f"  [{i:>3}/{len(files)}] {p.name} ", end="", flush=True)
        r = process_one(p, cat_cfg, args.out,
                        category=args.category,
                        vlm_ctx=vlm_ctx,
                        rembg_model=args.rembg_model,
                        alpha_matting=args.alpha_matting,
                        edge_erode_px=args.edge_erode)
        results.append(r)
        print(f"→ {r.decision}" + (f" ({r.reason})" if r.reason else ""))

    # Manifest
    manifest = [asdict(r) for r in results]
    (args.out / "manifest.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8")

    write_report(results, args.out)
    print()
    print(f"Done. Open {args.out / 'report.html'} in a browser.")
    by_decision = {"approved": 0, "review": 0, "rejected": 0}
    for r in results:
        by_decision[r.decision] = by_decision.get(r.decision, 0) + 1
    for k, v in by_decision.items():
        print(f"  {k:>9}: {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

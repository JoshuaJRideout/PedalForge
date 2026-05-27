#!/usr/bin/env python3
"""
Quick visual composite of cover candidates over the chassis body.

Removes white background from both, scales the cover to fit the body's
lower section, and pastes it. Output is for eyeballing fit only — the
real composite in JUCE uses pixel-accurate coords from PedalDesign.
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageChops
from rembg import remove, new_session


HERE = Path(__file__).resolve().parent
BODY = HERE / "pedals/clean_boost/raw/chassis_body/shared_chassis_body_20260525_195359_3.png"
COVERS_DIR = HERE / "pedals/clean_boost/raw/cover"
OUT_DIR = HERE / "pedals/clean_boost/preview_composites"

# Clean Boost forest green. Picked at L≈48% so it reads like a real
# painted Boss-style finish, not a dark muddy shadow.
TINT_RGB = (0x4F, 0xA3, 0x72)


def rembg_rgba(path: Path, session) -> Image.Image:
    img = remove(Image.open(path), session=session)
    rgba = img.convert("RGBA")
    bbox = rgba.getbbox()
    if bbox is not None:
        w, h = rgba.size
        bx, by, bx2, by2 = bbox
        # If rembg kept the entire frame, fall back to chroma key. Happens
        # when the subject and background are both close to white (low
        # luminance contrast) — true of our near-white painted bodies.
        if bx == 0 and by == 0 and bx2 == w and by2 == h:
            return chroma_key(Image.open(path).convert("RGBA"))
    return rgba


def chroma_key(rgba: Image.Image, tol: int = 10) -> Image.Image:
    """Flood-fill from the four corners with a tolerance to identify the
    true background. Pixels reachable from a corner via similar-enough
    neighbours become transparent. Highlights on the subject are preserved.
    """
    import numpy as np
    from scipy import ndimage  # type: ignore

    arr = np.asarray(rgba, dtype=np.uint8)
    h, w = arr.shape[:2]
    corners = np.stack([
        arr[0, 0, :3].astype(np.int16),
        arr[0, w - 1, :3].astype(np.int16),
        arr[h - 1, 0, :3].astype(np.int16),
        arr[h - 1, w - 1, :3].astype(np.int16),
    ])
    bg = corners.mean(axis=0)
    dist = np.abs(arr[..., :3].astype(np.int16) - bg).max(axis=-1)
    bg_candidates = dist <= tol  # bool mask, True = "could be bg"

    # Seed flood fill from the four corners; keep only candidates connected
    # to a seed. Anything else (e.g. specular highlights on the body) stays.
    seeds = np.zeros_like(bg_candidates)
    seeds[0, 0] = seeds[0, -1] = seeds[-1, 0] = seeds[-1, -1] = True
    labelled, _ = ndimage.label(bg_candidates)
    seed_labels = set(labelled[seeds].tolist()) - {0}
    bg_mask = np.isin(labelled, list(seed_labels))

    alpha = np.where(bg_mask, 0, 255).astype(np.uint8)
    out = arr.copy()
    out[..., 3] = alpha
    return Image.fromarray(out, mode="RGBA")


def _rgb_to_hls(rgb: "np.ndarray") -> tuple["np.ndarray", "np.ndarray", "np.ndarray"]:
    """Vectorised RGB→HLS. rgb in [0,1] shape (...,3). Returns h,l,s each in [0,1]."""
    import numpy as np
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx, mn = np.maximum.reduce([r, g, b]), np.minimum.reduce([r, g, b])
    l = (mx + mn) / 2.0
    d = mx - mn
    safe = np.where(d == 0, 1.0, d)
    s = np.where(d == 0, 0.0,
                 np.where(l < 0.5, d / np.where(mx + mn == 0, 1.0, mx + mn),
                          d / np.where(2.0 - mx - mn == 0, 1.0, 2.0 - mx - mn)))
    rc = (mx - r) / safe
    gc = (mx - g) / safe
    bc = (mx - b) / safe
    h = np.where(r == mx, bc - gc,
        np.where(g == mx, 2.0 + rc - bc, 4.0 + gc - rc)) / 6.0
    h = np.mod(h, 1.0)
    h = np.where(d == 0, 0.0, h)
    return h, l, s


def _hls_to_rgb(h: "np.ndarray", l: "np.ndarray", s: "np.ndarray") -> "np.ndarray":
    """Vectorised HLS→RGB. All inputs shape (H,W) in [0,1]; output shape (H,W,3)."""
    import numpy as np

    def _hue_to_rgb(p, q, t):
        t = np.mod(t, 1.0)
        return np.where(t < 1 / 6, p + (q - p) * 6 * t,
               np.where(t < 1 / 2, q,
               np.where(t < 2 / 3, p + (q - p) * (2 / 3 - t) * 6, p)))

    q = np.where(l < 0.5, l * (1 + s), l + s - l * s)
    p = 2 * l - q
    r = _hue_to_rgb(p, q, h + 1 / 3)
    g = _hue_to_rgb(p, q, h)
    b = _hue_to_rgb(p, q, h - 1 / 3)
    return np.stack([r, g, b], axis=-1)


def hsl_tint(rgba: Image.Image, tint: tuple[int, int, int],
             lightness_strength: float = 1.0) -> Image.Image:
    """Repaint subject pixels in the tint's hue and saturation, with the
    body's relative lightness variation preserved around the tint's mean
    lightness. Photographically the same as 'painted with a single colour
    coat': highlights stay highlights, shadows stay shadows, but the paint
    colour replaces the underlying white.

    lightness_strength: 0 → flat tint colour, 1 → full body lightness range,
    >1 → exaggerated contrast (useful when source body has very subtle
    shading like our near-white chassis).
    """
    import numpy as np
    arr = np.asarray(rgba.convert("RGBA"), dtype=np.float32) / 255.0
    rgb = arr[..., :3]
    alpha = arr[..., 3]
    subject_mask = alpha > 0.5
    if not subject_mask.any():
        return rgba

    # Body's HLS, and mean lightness over subject pixels only
    body_h, body_l, body_s = _rgb_to_hls(rgb)
    body_mean_l = float(body_l[subject_mask].mean())

    # Tint's HLS
    tint01 = np.array(tint, dtype=np.float32) / 255.0
    th, tl, ts = _rgb_to_hls(tint01.reshape(1, 1, 3))
    tint_h, tint_l, tint_s = float(th[0, 0]), float(tl[0, 0]), float(ts[0, 0])

    # New lightness: tint mean ± body delta
    delta_l = body_l - body_mean_l
    new_l = np.clip(tint_l + delta_l * lightness_strength, 0.0, 1.0)

    new_h = np.full_like(body_h, tint_h)
    new_s = np.full_like(body_s, tint_s)
    new_rgb = _hls_to_rgb(new_h, new_l, new_s)

    out_rgb = np.where(subject_mask[..., None], new_rgb, rgb)
    out = np.concatenate([np.clip(out_rgb, 0, 1) * 255.0, alpha[..., None] * 255.0], axis=-1)
    return Image.fromarray(out.astype(np.uint8), mode="RGBA")


# Back-compat alias so the rest of the script doesn't change.
multiply_tint = hsl_tint


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    session = new_session("isnet-general-use")

    print(f"Removing bg from body: {BODY.name}")
    body_raw = rembg_rgba(BODY, session)
    body = multiply_tint(body_raw, TINT_RGB)
    print(f"  tinted body with RGB{TINT_RGB} (multiply blend)")
    body_bbox = body.getbbox()
    if body_bbox is None:
        print("ERROR: body image is empty after bg removal")
        return 1
    body_w = body_bbox[2] - body_bbox[0]
    body_h = body_bbox[3] - body_bbox[1]
    print(f"  body subject bbox: {body_bbox}  ({body_w}x{body_h})")

    covers = sorted(COVERS_DIR.glob("shared_chassis_cover_*.png"))
    if not covers:
        print(f"ERROR: no covers found in {COVERS_DIR}")
        return 1

    for cover_path in covers:
        print(f"\nProcessing cover: {cover_path.name}")
        cover_raw = rembg_rgba(cover_path, session)
        cover = multiply_tint(cover_raw, TINT_RGB)
        cover_bbox = cover.getbbox()
        if cover_bbox is None:
            print("  WARN: empty after bg removal, skipping")
            continue
        cover_cropped = cover.crop(cover_bbox)
        cw, ch = cover_cropped.size
        print(f"  cover subject: {cw}x{ch}  (aspect {cw/ch:.2f})")

        # Scale cover so its width matches ~78% of the body width (inset
        # margin matches a real Boss-style cover, which leaves visible
        # painted chassis on the left and right of the stomp plate).
        target_w = int(body_w * 0.78)
        scale = target_w / cw
        new_size = (target_w, int(ch * scale))
        cover_scaled = cover_cropped.resize(new_size, Image.LANCZOS)

        # Compose at the lower portion of the body. Cover bottom aligns
        # roughly with the body bottom (small inset).
        composite = body.copy()
        cover_x = body_bbox[0] + (body_w - target_w) // 2
        bottom_inset = int(body_h * 0.03)
        cover_y = body_bbox[3] - cover_scaled.height - bottom_inset
        composite.paste(cover_scaled, (cover_x, cover_y), cover_scaled)

        # Trim transparent border for a tighter preview crop
        cbox = composite.getbbox()
        if cbox:
            composite = composite.crop(cbox)

        # Place on a neutral mid-gray backdrop so transparency is visible
        backdrop = Image.new("RGBA", composite.size, (60, 60, 60, 255))
        backdrop.paste(composite, (0, 0), composite)

        # Use cover variant index 1/2/3 from filename suffix
        suffix = cover_path.stem.rsplit("_", 1)[-1]
        out_path = OUT_DIR / f"composite_cover_{suffix}.png"
        backdrop.save(out_path)
        print(f"  → {out_path}  ({backdrop.size[0]}x{backdrop.size[1]})")

    print(f"\nWrote previews to {OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

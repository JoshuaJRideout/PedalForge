# PedalForge Image Pipeline

Batch process AI-generated images into ship-ready skeuomorphic UI assets.

The pipeline is for the **author** (you), not the app — runs locally, produces
transparent PNGs that get committed to `resources/images/processed/` and shipped.

## How we use it (workflow)

Real-world experience showed that **VLM-as-judge is too unreliable** on
visually-ambiguous components (an LED in a metal bezel reads as either a knob
or a footswitch depending on the model). So the workflow leans on:

1. **Good prompts at generation time** — see [`prompts/`](prompts/) for a
   curated library. Refine prompts there as you find what works.
2. **Small batches** — 4–8 images per generation, not bulk runs.
3. **Pipeline does mechanical work** — background removal (rembg), auto-crop
   to subject, resize to standard size.
4. **Interactive review** — `out/report.html` is a pass/fail review tool with
   keyboard shortcuts and notes. Click through, export decisions.
5. **Apply decisions** — `apply_decisions.py` copies approved files to
   `resources/images/processed/`.

The VLM checks (`--vlm`, `--auto-category`) still exist as **advisory** —
useful for an initial classification pass on a large dump, but don't trust
them to gate quality. The manifest + HTML report capture the data either way.

### Full workflow

```bash
# 1. Drop your raw generations in ~/Desktop/Background/
#    (or anywhere — pass --input to point at it)

# 2. Run the pipeline (no flags = no VLM, just process + report)
cd tools/image_pipeline
.venv/bin/python qa_process.py

# 3. Review every output interactively
open out/report.html
#    - Click a card; hit P to pass, F to fail, C to clear
#    - Type notes in the textarea
#    - Filter by Pass/Fail/Pending in the header
#    - Click "Export Decisions" → downloads decisions.json

# 4. Apply your decisions (move decisions.json to image_pipeline/ first)
.venv/bin/python apply_decisions.py decisions.json --dry-run   # preview
.venv/bin/python apply_decisions.py decisions.json             # commit

# Approved PNGs land in resources/images/processed/<category>/
```

The review state lives in browser localStorage keyed by source-hash, so you
can close the tab and come back later — your in-progress decisions persist.
Export the decisions JSON when you're done with the batch.

## What it does

For each input image:

1. **QA scoring** — sharpness, background uniformity, subject centering,
   aspect-ratio sanity. Each gets a 0–1 score.
2. **Background removal** — runs [rembg](https://github.com/danielgatis/rembg)
   (U2Net) if the source has a background. Skips if the source is already RGBA
   with transparency.
3. **Auto-crop** to the subject's bounding box, with a small padding margin.
4. **Resize** to standard dimensions per asset category (1024² for chassis,
   512² for knobs, 256² for LEDs, etc.).
5. **Categorize** into `approved/`, `review/`, `rejected/` based on score.
6. **HTML report** lists everything with thumbnails + scores side-by-side for
   manual sign-off.

## Why this exists

Earlier attempts at hand-curating AI-generated assets failed because:

- **No automated quality filter** — many outputs were blurry, off-center, had
  unexpected props, or wrong style. Picking by eye across hundreds of gens is
  slow and inconsistent.
- **No standard output shape** — varying sizes, varying backgrounds, varying
  alpha treatment made the app-side code painful.

This pipeline addresses both. Generation itself stays in your existing
GPT-Image / Firefly / etc. workflow — the pipeline consumes the outputs.

## Setup (one-time)

```bash
cd tools/image_pipeline
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

`rembg` will download its U2Net model on first run (~170 MB).

## Run

```bash
# Default: processes ~/Desktop/Background and ~/Desktop/Removed
python qa_process.py

# Or point at any folder
python qa_process.py --input ~/some/other/folder

# Limit to N images (for fast iteration)
python qa_process.py --limit 5

# With VLM semantic QA (catches wrong viewing angle, visible internals, etc.)
export OPENAI_API_KEY=sk-...
python qa_process.py --vlm --category chassis --limit 5

# Different VLM
python qa_process.py --vlm --vlm-model gpt-4o --limit 5
```

### What `--vlm` does

Pure CV metrics (sharpness, bg purity, centering) can't see *semantic*
failures. The VLM inspects each image against a category-specific rubric and
flags problems like:

- Wrong viewing angle — 3/4 perspective, side, or bottom-up when we need
  top-down
- Visible internal parts — switch undersides, knob shafts, PCBs, wire leads
- Wrong style — illustration / cartoon when we want photorealistic

The CV gate runs first (cheap, free); VLM only sees images that survive it.
Responses are cached in `out/vlm_cache.json` by source-hash, so re-runs don't
re-pay for the same images.

Cost: about $0.005 per image with the default `gpt-4o-mini`.

Outputs:

- `out/processed/<category>/<hash>.png` — ready-to-ship transparent PNGs
- `out/review/<hash>.png` — borderline outputs to eyeball
- `out/rejected/<hash>.png` — failed QA; reason logged in manifest
- `out/manifest.json` — per-image: source path, scores, decision, reason
- `out/report.html` — open in browser for visual review

`out/` is gitignored. Copy approved outputs into `resources/images/processed/`
when you're happy with them (the script can do this with `--commit` once we
trust it).

## Adding asset categories

Edit `categories.yaml`:

```yaml
chassis:
  output_size: 1024
  aspect_ratio: any           # accepts wide, tall, square
  min_subject_fraction: 0.4   # subject must fill ≥40% of canvas

knob:
  output_size: 512
  aspect_ratio: square
  min_subject_fraction: 0.6
```

Currently the script doesn't know which category an input belongs to — for now
everything lands in `processed/uncategorized/`. Once you have prompt templates
(future work) the category gets tagged at generation time.

## What's not yet here

- **Generation step** — the pipeline reads existing outputs. Generation stays
  in your existing tool until we wire up an OpenAI/Firefly API path.
- **Prompt templates** — a `prompts/` directory with YAML-defined per-pedal
  specs that render into deterministic prompts is the natural next step.
- **CLIP semantic QA** — would catch "looks like a pedal" vs "looks like
  abstract art" beyond what shape/sharpness checks can. Defer until the simple
  checks aren't enough.
- **In-app effects (LED glow, knob rotation)** — these live in JUCE, not in
  this pipeline. Pre-baking them per state would explode the asset count.

## See also

- [`docs/wiki/library/tone3000-signin.md`](../../docs/wiki/library/tone3000-signin.md)
  — the comparable doc for the OAuth flow
- [TODO §18](../../TODO.md) — Factory Pedal Visual Polish, the launch gate this pipeline serves

# Feature Name

> [!NOTE]
> **Template — copy this file** to start a new feature page. Delete this notice when you're done.
> Save as `docs/wiki/<section>/<feature-slug>.md`. Add a link in [[index]].

One-paragraph overview: what this feature does and *why* it exists. If a user discovers this page from a search, this paragraph alone should tell them whether to keep reading.

## At a glance

- **Where it lives in the UI:** Which tab(s) and which buttons / overlays.
- **Underlying type(s):** The C++ class(es) or struct(s) — keep this short, link with the format `[ClassName](../../source/...)` if useful.
- **Persisted in:** JSON field on `PedalDesign` / `BoardConfig` / `PluginState`, or "not persisted" if ephemeral.
- **Related wiki:** `[[other-page]]`, `[[another-page]]`.

## How to use it

Numbered steps a user would actually follow. Show the result they should expect after each step.

1. …
2. …
3. …

## Script API

How to drive this feature from a script. Every feature should expose itself here — this is the long-term vision.

```
-- ExpressionVM / script-mode snippet
example()
```

If the feature isn't scriptable yet, write **"Not scriptable yet — see [[task-ref]]"** and link the relevant TODO. Don't skip the section.

## Examples

At least one minimal example, then one realistic example. Use fenced blocks; users copy-paste these directly.

## Settings & options

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `foo` | float | `0.5` | What this controls |

## Gotchas

Anything that surprised the author when implementing or that has tripped users up.

## See also

- `[[…]]`
- `[[…]]`

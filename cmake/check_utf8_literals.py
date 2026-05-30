#!/usr/bin/env python3
"""Guard against the mojibake bug class (see git history: the "â€¦" bug).

juce::String's (const char*) constructor decodes input as ASCII/Latin-1, so a
bare UTF-8 string literal such as "…" is split into separate Latin-1 chars and
renders as mojibake ("â€¦"). Any non-ASCII string literal MUST be wrapped in
juce::CharPointer_UTF8(...) (or written as a u8"" literal), which decodes UTF-8.

This script scans the C++ sources and FAILS the build if it finds a narrow
string literal containing raw non-ASCII bytes that is NOT wrapped in
CharPointer_UTF8 and is NOT a u8"" literal.

It is comment-aware (ignores // and /* */), and ignores \\xNN byte escapes
(those are plain ASCII bytes in the source and decode fine). Escaped UTF-8 like
CharPointer_UTF8("\\xe2\\x80\\xa6") is therefore never flagged.

Suppress a deliberate exception with a trailing  // utf8-ok  on the line.

Usage:  check_utf8_literals.py <source-dir> [<source-dir> ...]
Exit:   0 = clean, 1 = violations found (prints file:line + the offending line).
"""
import os
import sys

EXTS = (".cpp", ".h", ".hpp", ".cc", ".mm", ".cxx")


def find_violations(data: bytes):
    """Tokenise C++ bytes, returning [(line, text)] for offending literals."""
    lines = data.split(b"\n")
    line_has_cpu8 = [b"CharPointer_UTF8" in L for L in lines]
    line_has_suppress = [b"utf8-ok" in L for L in lines]

    NORMAL, LINE_COMMENT, BLOCK_COMMENT, STRING, CHAR = range(5)
    state = NORMAL
    i, n, line = 0, len(data), 1
    str_start = 0
    str_nonascii = False
    str_u8 = False
    violations = []

    while i < n:
        c = data[i]
        nxt = data[i + 1] if i + 1 < n else 0

        if c == 0x0A:  # newline
            line += 1
            if state == LINE_COMMENT:
                state = NORMAL
            i += 1
            continue

        if state == NORMAL:
            if c == 0x2F and nxt == 0x2F:        # //
                state = LINE_COMMENT; i += 2; continue
            if c == 0x2F and nxt == 0x2A:        # /*
                state = BLOCK_COMMENT; i += 2; continue
            if c == 0x22:                        # "
                state = STRING; str_start = line; str_nonascii = False
                str_u8 = i >= 2 and data[i - 2:i] == b"u8"
                i += 1; continue
            if c == 0x27:                        # '
                state = CHAR; i += 1; continue
            i += 1; continue

        if state == LINE_COMMENT:
            i += 1; continue

        if state == BLOCK_COMMENT:
            if c == 0x2A and nxt == 0x2F:        # */
                state = NORMAL; i += 2; continue
            i += 1; continue

        if state == STRING:
            if c == 0x5C:                        # backslash: skip escaped byte
                i += 2; continue
            if c == 0x22:                        # closing "
                if str_nonascii and not str_u8:
                    li = str_start - 1
                    ok = line_has_cpu8[li] or line_has_suppress[li]
                    if not ok:
                        text = lines[li].decode("utf-8", "replace").strip()
                        violations.append((str_start, text))
                state = NORMAL; i += 1; continue
            if c >= 0x80:
                str_nonascii = True
            i += 1; continue

        if state == CHAR:
            if c == 0x5C:
                i += 2; continue
            if c == 0x27:
                state = NORMAL; i += 1; continue
            i += 1; continue

    return violations


def signature(path, text):
    """Stable identity for a violation: path + the offending line text (NOT the
    line number, so it survives unrelated edits that shift line numbers)."""
    return path.replace(os.sep, "/") + "\t" + text


def scan(roots):
    out = []  # (path, line, text)
    for root in roots:
        if os.path.isfile(root):
            files = [root]
        else:
            files = [os.path.join(dp, f)
                     for dp, _d, fs in os.walk(root)
                     for f in fs if f.endswith(EXTS)]
        for p in files:
            try:
                with open(p, "rb") as fh:
                    data = fh.read()
            except OSError:
                continue
            for ln, text in find_violations(data):
                out.append((p, ln, text))
    return out


def main(argv):
    args = argv[1:]
    baseline_path = None
    write_baseline = False
    roots = []
    it = iter(args)
    for a in it:
        if a == "--baseline":
            baseline_path = next(it, None)
        elif a == "--write-baseline":
            baseline_path = next(it, None)
            write_baseline = True
        else:
            roots.append(a)
    if not roots:
        roots = ["source"]

    found = scan(roots)
    found_sigs = {signature(p, t): (p, ln, t) for (p, ln, t) in found}

    if write_baseline:
        with open(baseline_path, "w", encoding="utf-8") as fh:
            fh.write("# Auto-generated baseline of PRE-EXISTING un-wrapped non-ASCII\n")
            fh.write("# string literals (mojibake debt). The build guard fails only on\n")
            fh.write("# NEW violations not listed here. Burn this list down over time;\n")
            fh.write("# regenerate with: python3 cmake/check_utf8_literals.py source \\\n")
            fh.write("#                  --write-baseline cmake/utf8_baseline.txt\n")
            for sig in sorted(found_sigs):
                fh.write(sig + "\n")
        sys.stderr.write(f"[utf8-guard] wrote baseline of {len(found_sigs)} entries.\n")
        return 0

    baseline = set()
    if baseline_path and os.path.isfile(baseline_path):
        with open(baseline_path, encoding="utf-8") as fh:
            for line in fh:
                if line.strip() and not line.startswith("#"):
                    baseline.add(line.rstrip("\n"))

    new_sigs = [s for s in found_sigs if s not in baseline]

    if not new_sigs:
        return 0

    sys.stderr.write(
        "\n[utf8-guard] NEW un-wrapped non-ASCII string literal(s) — these render\n"
        "             as mojibake (e.g. \"…\" -> \"â€¦\") because juce::String(const\n"
        "             char*) decodes as ASCII/Latin-1. Fix by wrapping the literal\n"
        "             in juce::CharPointer_UTF8(...) (prefer \\xNN byte escapes),\n"
        "             or add a trailing  // utf8-ok  if the bytes are intentional.\n\n")
    for s in sorted(new_sigs):
        p, ln, t = found_sigs[s]
        sys.stderr.write(f"  {p}:{ln}: {t}\n")
    sys.stderr.write(f"\n[utf8-guard] {len(new_sigs)} new violation(s). "
                     "(Pre-existing debt is baselined in cmake/utf8_baseline.txt.)\n")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))

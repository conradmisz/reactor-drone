#!/usr/bin/env python3
"""Consistency check: every v2 sidecar's atlas exists and its frame grid fits.

Asserts, for each images/v2/*.json sidecar:
  - the referenced atlas PNG exists,
  - columns*frame_width and ceil(total_frames/columns)*frame_height fit inside it,
  - total_frames <= columns*rows,
  - every animation's [start_frame, start_frame+frame_count) stays in range.

Run: python test_manifest.py   (exit 0 = all good)
"""
from __future__ import annotations

import glob
import json
import math
import os
import sys

from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
V2 = os.path.join(ROOT, "images", "v2")


def check():
    sidecars = sorted(glob.glob(os.path.join(V2, "*.json")))
    assert sidecars, f"no sidecars found in {V2}"
    errors = []
    for sc in sidecars:
        d = json.load(open(sc))
        name = os.path.basename(sc)
        atlas_rel = d["atlas"]                      # e.g. images/v2/foo.png
        atlas = os.path.join(ROOT, "images", os.path.relpath(atlas_rel, "images"))
        if not os.path.exists(atlas):
            errors.append(f"{name}: atlas {atlas_rel} missing"); continue
        w, h = Image.open(atlas).size
        cols = d["columns"]; fw = d["frame_width"]; fh = d["frame_height"]
        total = d["total_frames"]
        rows = math.ceil(total / cols)
        if cols * fw > w:
            errors.append(f"{name}: {cols}*{fw} > atlas width {w}")
        if rows * fh > h:
            errors.append(f"{name}: {rows}*{fh} > atlas height {h}")
        if total > cols * rows:
            errors.append(f"{name}: total_frames {total} > grid {cols*rows}")
        for an, a in d.get("animations", {}).items():
            end = a["start_frame"] + a["frame_count"]
            if end > total:
                errors.append(f"{name}:{an}: frames end {end} > total {total}")
    if errors:
        print("MANIFEST FAIL:")
        for e in errors:
            print("  -", e)
        return 1
    print(f"manifest OK: {len(sidecars)} sidecars consistent")
    return 0


if __name__ == "__main__":
    sys.exit(check())

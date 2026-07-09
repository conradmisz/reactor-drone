"""Sidecar JSON + asset_manifest.json writers with provenance (R8).

Pure logic + file I/O; no ``pyrender`` dependency. Writes a per-entity sidecar
descriptor next to the atlas PNG and updates the shared asset manifest while
preserving all existing entries.

The sidecar maps directly onto the engine's ``SpriteSheet`` / ``Animation``
component fields (consumed in Gen-5, no engine change in Gen-1):

* ``atlas``        -> ``SpriteSheet.atlas_filename``
* ``frame_width``  -> ``SpriteSheet.frame_width`` (equals rendered frame size, R8.4)
* ``frame_height`` -> ``SpriteSheet.frame_height`` (equals rendered frame size, R8.4)
* ``columns``      -> ``SpriteSheet.columns``
* ``total_frames`` -> ``SpriteSheet.total_frames`` (equals rendered frame count, R8.5)
* ``animations.<clip>.start_frame`` / ``frame_count`` / ``frame_duration`` / ``looping``
  -> the matching ``Animation`` fields, with ``start_frame + frame_count <= total_frames`` (R8.6)
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from atlas import GridLayout
    from params import ClipParams, Provenance


# Key under which generator-produced atlases live in asset_manifest.json (R8.7).
_PROGRAMMATIC_KEY = "programmatic"


def _sidecar_name(atlas_name: str) -> str:
    """Return the sidecar filename for an atlas, e.g. ``reference.png`` ->
    ``reference.json``."""
    return Path(atlas_name).with_suffix(".json").name


def write_sidecar(
    out_dir: Path,
    atlas_name: str,
    layout: "GridLayout",
    animations: "dict[str, ClipParams]",
    facing_angles: "dict[str, list[float]] | None" = None,
) -> Path:
    """Write ``<entity>.json`` next to the atlas and return its path.

    The descriptor carries ``atlas``, ``frame_width``, ``frame_height``,
    ``columns``, ``total_frames`` and an ``animations`` object mapping each clip
    name to ``{start_frame, frame_count, frame_duration, looping}``.

    Clips are packed as **contiguous** frame ranges in iteration order: the first
    clip starts at frame 0, and each subsequent clip starts where the previous one
    ended. ``frame_count`` is taken from the clip's ``frames`` field.

    For every clip, ``start_frame + frame_count`` must be ``<= total_frames``
    (R8.6); a :class:`ValueError` is raised naming the offending clip otherwise.
    """  # R8.1-R8.6
    out_dir = Path(out_dir)
    total_frames = layout.total_frames

    clip_entries: dict[str, dict] = {}
    next_start = 0
    for clip_name, clip in animations.items():
        frame_count = clip.frames
        start_frame = next_start
        end = start_frame + frame_count
        if end > total_frames:
            raise ValueError(
                f"Animation clip '{clip_name}' is out of bounds: "
                f"start_frame ({start_frame}) + frame_count ({frame_count}) = {end} "
                f"exceeds total_frames ({total_frames}). "
                "Every clip must satisfy start_frame + frame_count <= total_frames (R8.6)."
            )
        clip_entries[clip_name] = {
            "start_frame": start_frame,
            "frame_count": frame_count,
            "frame_duration": clip.frame_duration,
            "looping": clip.looping,
        }
        # Directional ("facings") clips carry the per-frame on-screen barrel angle so the
        # engine can aim a tower by selecting the facing nearest the direction to its
        # target. Present only for full-circle spin-sweep clips (computed by the caller).
        if facing_angles and clip_name in facing_angles:
            clip_entries[clip_name]["facing_angles_deg"] = facing_angles[clip_name]
        next_start = end

    sidecar = {
        "atlas": atlas_name,
        "frame_width": layout.frame_width,
        "frame_height": layout.frame_height,
        "columns": layout.columns,
        "total_frames": total_frames,
        "animations": clip_entries,
    }

    sidecar_path = out_dir / _sidecar_name(atlas_name)
    sidecar_path.write_text(json.dumps(sidecar, indent=2) + "\n", encoding="utf-8")
    return sidecar_path


def update_asset_manifest(
    manifest_path: Path,
    entity_name: str,
    atlas_name: str,
    provenance: "Provenance",
) -> None:
    """Add or replace the target entity's entry in ``asset_manifest.json``.

    Only the entry whose ``filename`` matches ``atlas_name`` (under the existing
    ``programmatic`` section) is added or replaced; every other entry and every
    top-level key is preserved exactly. The entry records the model's source URL
    and license (R8.3) and is written back with stable key ordering (R8.7, R8.8).
    """  # R8.7, R8.8, R2.3
    manifest_path = Path(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    programmatic = manifest.get(_PROGRAMMATIC_KEY)
    if not isinstance(programmatic, list):
        programmatic = []
        manifest[_PROGRAMMATIC_KEY] = programmatic

    entry = {
        "filename": atlas_name,
        "sidecar": _sidecar_name(atlas_name),
        "description": (
            f"{entity_name} entity atlas generated by the Class-090 generator."
        ),
        "provenance": {
            "source_url": provenance.source_url,
            "license": provenance.license,
        },
    }

    # Replace the existing entry for this atlas in place, preserving order and any
    # extra keys (e.g. width/height) recorded by a prior run; otherwise append.
    for index, existing in enumerate(programmatic):
        if isinstance(existing, dict) and existing.get("filename") == atlas_name:
            merged = dict(existing)
            merged.update(entry)
            programmatic[index] = merged
            break
    else:
        programmatic.append(entry)

    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

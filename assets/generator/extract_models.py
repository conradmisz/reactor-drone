"""Python-only extractor for CC0 meshes from the local Kenney kit zip (Requirement 2).

The Kenney Tower Defense Kit zip is committed at
``2026/Class-090/assets/generator/kenney_tower-defense-kit.zip`` (CC0, Tower Defense
Kit 2.1). This module uses the standard-library :mod:`zipfile` (never ``curl`` or shell,
per R13.3) to extract named GLB meshes into ``models/``. No network download is required.

The extractor is intentionally reusable: it accepts a list of GLB *basenames*
(e.g. ``"enemy-ufo-a.glb"``) and locates each member under the kit's
``Models/GLB format/`` directory, so later specs (Gen-2..Gen-5) can pull tower, weapon,
and ammo GLBs by name without changing this module.

CLI usage (instructor-only)::

    python extract_models.py                 # extract the default reference set
    python extract_models.py enemy-ufo-a.glb weapon-turret.glb
    python extract_models.py --list          # list available GLB members in the kit

(R2.1, R2.2, R2.4, R13.1, R13.3)
"""

from __future__ import annotations

import argparse
import re
import sys
import zipfile
from pathlib import Path

# Directory inside the kit zip that holds the GLB meshes.
GLB_MEMBER_DIR = "Models/GLB format"

# The kit's GLB materials reference an EXTERNAL base-color texture by the relative
# URI ``Textures/colormap.png`` (a flat palette atlas shared by every mesh). The GLB
# bytes do not embed the image, so the texture must sit next to the extracted GLBs or
# trimesh cannot resolve it and every model renders untextured (white). We therefore
# always extract this companion texture into ``<out_dir>/Textures/colormap.png`` so the
# models/ directory is self-sufficient. (Dropping it was the original "white towers" bug.)
TEXTURE_MEMBER = f"{GLB_MEMBER_DIR}/Textures/colormap.png"
TEXTURE_RELPATH = Path("Textures") / "colormap.png"

# Location of the committed kit zip and the output models directory, relative to this
# file so the tool works regardless of the current working directory.
_GENERATOR_DIR = Path(__file__).resolve().parent
DEFAULT_ZIP_PATH = _GENERATOR_DIR / "kenney_tower-defense-kit.zip"
DEFAULT_MODELS_DIR = _GENERATOR_DIR / "models"

# The Gen-1 reference set: the single reference entity mesh proven end-to-end.
DEFAULT_REFERENCE_SET = ["enemy-ufo-a.glb"]

# Provenance recorded for every GLB extracted from the committed Kenney kit (R2.3, R2.4).
# All kit meshes share one source URL and the CC0-1.0 public-domain dedication.
PROVENANCE_FILENAME = "PROVENANCE.md"
KIT_SOURCE_URL = "https://kenney.nl/assets/tower-defense-kit"
KIT_LICENSE_ID = "CC0-1.0"

# Matches a data row of the "Extracted GLB files" table, capturing the GLB basename in the
# leading ``| `<name>.glb` |`` cell so existing entries are detected and never duplicated.
_GLB_ROW_RE = re.compile(r"^\|\s*`([^`]+\.glb)`\s*\|")

# Header used when no PROVENANCE.md exists yet (e.g. extracting into a fresh directory).
_PROVENANCE_HEADER = """# Model Provenance

This directory holds CC0 mesh files extracted from the committed Kenney Tower Defense Kit
zip by `../extract_models.py` (Python `zipfile`, no network download).

## Source

- **Kit:** Kenney Tower Defense Kit
- **Source URL:** https://kenney.nl/assets/tower-defense-kit
- **License:** CC0-1.0 (Creative Commons Zero 1.0 Universal, public domain dedication)

## Extracted GLB files

Every file below is sourced from the Kenney Tower Defense Kit (source URL
https://kenney.nl/assets/tower-defense-kit) under the `CC0-1.0` license:

| File | In-zip source | Source URL | License |
|------|----------------|------------|---------|"""


def _provenance_row(name: str) -> str:
    """Render one markdown table row for an extracted GLB basename (R2.3, R2.4)."""
    return (
        f"| `{name}` | `{GLB_MEMBER_DIR}/{name}` | "
        f"{KIT_SOURCE_URL} | {KIT_LICENSE_ID} |"
    )


def record_provenance(out_dir: Path, names: list[str]) -> Path:
    """Record provenance notes for the given GLB basenames in ``out_dir/PROVENANCE.md``.

    For each name, a row recording the Kenney source URL and the ``CC0-1.0`` license is
    merged into the "Extracted GLB files" table. Existing entries (e.g. the Gen-2 enemy
    rows) are preserved and never duplicated; new rows are inserted after the last existing
    table row so any trailing prose is kept intact. When the file does not yet exist, a
    minimal provenance document with the table header is created. (R2.3, R2.4)

    This is invoked only after a fully successful extraction, so a failed extraction writes
    no provenance entries (R2.6).
    """
    prov_path = Path(out_dir) / PROVENANCE_FILENAME
    if prov_path.is_file():
        lines = prov_path.read_text(encoding="utf-8").splitlines()
        created = False
    else:
        lines = _PROVENANCE_HEADER.splitlines()
        created = True

    existing: set[str] = set()
    last_row_idx: int | None = None
    for idx, line in enumerate(lines):
        match = _GLB_ROW_RE.match(line)
        if match:
            existing.add(match.group(1))
            last_row_idx = idx

    new_rows = [_provenance_row(name) for name in names if name not in existing]

    if new_rows:
        if last_row_idx is not None:
            lines[last_row_idx + 1:last_row_idx + 1] = new_rows
        else:
            # No table found in an existing file: append a fresh section.
            lines.extend(
                [
                    "",
                    "## Extracted GLB files",
                    "",
                    "| File | In-zip source | Source URL | License |",
                    "|------|----------------|------------|---------|",
                    *new_rows,
                ]
            )

    if new_rows or created:
        prov_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    return prov_path


def list_glb_members(zip_path: Path = DEFAULT_ZIP_PATH) -> list[str]:
    """Return the sorted basenames of every GLB member under ``Models/GLB format/``."""
    with zipfile.ZipFile(zip_path) as kit:
        prefix = GLB_MEMBER_DIR + "/"
        names = [
            name[len(prefix):]
            for name in kit.namelist()
            if name.startswith(prefix) and name.lower().endswith(".glb")
        ]
    return sorted(names)


def extract_models(
    zip_path: Path,
    names: list[str],
    out_dir: Path,
    *,
    write_provenance: bool = True,
) -> list[Path]:
    """Extract the named GLB files from the local CC0 kit zip into ``out_dir``.

    ``names`` is a list of GLB basenames (e.g. ``"enemy-ufo-a.glb"``); each is resolved
    to its ``Models/GLB format/<name>`` member inside the kit. Returns the list of
    extracted file paths. Uses :mod:`zipfile` only (R13.3).

    Extraction is **atomic** (R2.6): every requested member is verified to exist in the kit
    *before any file is written*. If a requested GLB is absent, a ``KeyError`` naming the
    absent basename is raised and **nothing** is written — no partially populated
    destination and no provenance entries.

    When ``write_provenance`` is true (the default), provenance notes for every extracted
    GLB (source URL + ``CC0-1.0`` license) are merged into ``out_dir/PROVENANCE.md`` only
    after the extraction fully succeeds (R2.3, R2.4).

    Raises ``FileNotFoundError`` if the zip itself is missing, or ``KeyError`` (naming the
    member) if a requested GLB is not present in the kit. (R2.1, R2.2, R2.3, R2.4, R2.6)
    """
    zip_path = Path(zip_path)
    out_dir = Path(out_dir)
    if not zip_path.is_file():
        raise FileNotFoundError(f"Kit zip not found: {zip_path}")

    extracted: list[Path] = []

    with zipfile.ZipFile(zip_path) as kit:
        available = set(kit.namelist())

        # --- Atomic pre-validation (R2.6) -------------------------------------------
        # Resolve and verify EVERY requested member before writing anything. If one is
        # absent, raise immediately so no partially populated destination is produced.
        members: list[tuple[str, str]] = []
        for name in names:
            member = f"{GLB_MEMBER_DIR}/{name}"
            if member not in available:
                raise KeyError(
                    f"GLB member not found in kit: {member!r} "
                    f"(requested basename {name!r})"
                )
            members.append((name, member))

        # The shared colormap texture every GLB references must also be present.
        if TEXTURE_MEMBER not in available:
            raise KeyError(f"Shared texture not found in kit: {TEXTURE_MEMBER!r}")

        # --- All members verified: now it is safe to write -------------------------
        out_dir.mkdir(parents=True, exist_ok=True)
        for name, member in members:
            # Flatten: write the member's bytes to out_dir/<basename>, dropping the
            # "Models/GLB format/" prefix so models/ holds plain GLB files.
            target = out_dir / Path(name).name
            with kit.open(member) as src:
                target.write_bytes(src.read())
            extracted.append(target)

        # Write the companion colormap texture into out_dir/Textures/ so the extracted
        # GLBs can resolve their `Textures/colormap.png` base-color URI (R: self-sufficient
        # models/ directory). Kept under the Textures/ subdir the GLB URIs expect.
        texture_target = out_dir / TEXTURE_RELPATH
        texture_target.parent.mkdir(parents=True, exist_ok=True)
        with kit.open(TEXTURE_MEMBER) as src:
            texture_target.write_bytes(src.read())

    # Provenance is recorded only AFTER a fully successful extraction (R2.3, R2.4); a
    # missing-member failure above returns via the raised KeyError, leaving no entries.
    if write_provenance:
        record_provenance(out_dir, [path.name for path in extracted])

    return extracted


def main(argv: list[str] | None = None) -> int:
    """CLI entry point. Extracts the default reference set or named GLBs."""
    parser = argparse.ArgumentParser(
        description=(
            "Extract CC0 GLB meshes from the local Kenney Tower Defense Kit zip "
            "into the generator's models/ directory (instructor-only)."
        )
    )
    parser.add_argument(
        "names",
        nargs="*",
        help=(
            "GLB basenames to extract (e.g. enemy-ufo-a.glb). "
            "Defaults to the Gen-1 reference set if omitted."
        ),
    )
    parser.add_argument(
        "--zip",
        type=Path,
        default=DEFAULT_ZIP_PATH,
        help="Path to the kit zip (defaults to the committed kit).",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=DEFAULT_MODELS_DIR,
        help="Output models directory (defaults to generator/models/).",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List available GLB members in the kit and exit.",
    )
    args = parser.parse_args(argv)

    if args.list:
        for name in list_glb_members(args.zip):
            print(name)
        return 0

    names = args.names or DEFAULT_REFERENCE_SET
    try:
        extracted = extract_models(args.zip, names, args.out)
    except (FileNotFoundError, KeyError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    for path in extracted:
        print(f"extracted {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""CLI entry point for the Class-090 atlas generator (R9, R10).

Parses ``--all``/``--entity NAME``/``--debug`` and orchestrates the pipeline
(load -> pose -> render -> pack -> write) for one or every entity. Each output
step is guarded so a single failure does not abort sibling outputs.

This is an instructor-only, standalone Python tool. It is NOT wired into the
C++ engine/game CMake/CTest build.

Integration paths (resolved relative to this file's directory):

* parameters file   -> ``<generator>/generator-parameters.json``
* entity model      -> ``<generator>/<entity.model>`` (e.g. ``models/enemy-ufo-a.glb``)
* output atlas dir  -> ``<generator>/../images`` (``2026/Class-090/assets/images/``)
* asset manifest    -> ``<generator>/../asset_manifest.json``

The render stages (``render.scene``) require a working ``pyrender`` offscreen
backend; if it is unavailable they raise :class:`render.scene.BackendUnavailableError`,
which the CLI catches and reports cleanly per entity (the entity fails, siblings
continue).
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np

# Sibling modules. Support both the package-relative import (``import
# generator.generate_atlas``) and the top-level import used when running the script
# directly from inside the generator directory (``python generate_atlas.py``) — the
# same shim pattern used in ``params.py`` and ``render/scene.py``.
try:  # pragma: no cover - import shim
    from . import atlas as atlas_module
    from . import manifest as manifest_module
    from . import params as params_module
    from .entities import effects as effects_module
    from .entities import towers as towers_module
    from .render import loader as loader_module
    from .render import poser as poser_module
    from .render import scene as scene_module
except ImportError:  # pragma: no cover - import shim
    import atlas as atlas_module
    import manifest as manifest_module
    import params as params_module
    from entities import effects as effects_module
    from entities import towers as towers_module
    from render import loader as loader_module
    from render import poser as poser_module
    from render import scene as scene_module

if TYPE_CHECKING:  # pragma: no cover - typing only
    from params import EntityParams, GeneratorParams


# --------------------------------------------------------------------------------------
# Integration paths (resolved relative to this file — the generator directory)
# --------------------------------------------------------------------------------------

GENERATOR_DIR = Path(__file__).resolve().parent
PARAMS_PATH = GENERATOR_DIR / "generator-parameters.json"
# ``<generator>/../`` is ``2026/Class-090/assets/`` (the design Integration Path).
ASSETS_DIR = GENERATOR_DIR.parent
IMAGES_DIR = ASSETS_DIR / "images"
MANIFEST_PATH = ASSETS_DIR / "asset_manifest.json"


@dataclass
class EntityResult:
    """Per-entity outcome of :func:`generate_entity`.

    ``atlas_path`` / ``sidecar_path`` are set only for outputs that were written
    successfully. ``failed_outputs`` names each output step that failed (one of
    ``"render"``, ``"atlas"``, ``"sidecar"``, ``"manifest"``) and ``errors`` carries
    the matching human-readable messages (R9.7).
    """

    name: str
    atlas_path: "Path | None" = None
    sidecar_path: "Path | None" = None
    failed_outputs: "list[str]" = field(default_factory=list)
    errors: "list[str]" = field(default_factory=list)

    @property
    def ok(self) -> bool:
        """True when every output step succeeded (full success for this entity)."""
        return not self.failed_outputs

    def _fail(self, output: str, exc: BaseException) -> None:
        self.failed_outputs.append(output)
        self.errors.append(f"{output}: {exc}")


def _build_frame_sequence(entity: "EntityParams") -> "list":
    """Build the per-frame pose matrices across all of the entity's clips, in
    contiguous iteration order.

    The clips are walked in their declared order; for a clip of ``N`` frames the
    poser is evaluated at frame indices ``0..N-1`` using that clip's ``pose``. The
    resulting sequence length equals the sum of every clip's frame count — matching
    the contiguous ``start_frame`` packing performed by
    :func:`manifest.write_sidecar`, so frame ``k`` in this list lands in atlas cell
    ``k`` and inside the clip range the sidecar declares.
    """
    poses = []
    for clip in entity.animations.values():
        frame_count = clip.frames
        for frame_index in range(frame_count):
            poses.append(
                poser_module.pose_for_frame(frame_index, frame_count, clip.pose)
            )
    return poses


def _render_atlas_image(entity: "EntityParams", debug: bool):
    """Run load -> pose -> render -> pack for one entity, returning ``(image, layout)``.

    All render-dependent work happens here; a failure (missing model, unreadable
    mesh, unavailable backend, pack error) propagates to the caller, which records it
    as the entity's ``"render"`` failure so no partial outputs are written.

    A modular entity (a tower — one that declares a ``weapon_model``) is routed
    through :func:`entities.towers.build_tower_frames`, which assembles the shared
    base + per-tower weapon as two scene nodes and poses only the weapon (R8.5). The
    helper returns the same ``(image, layout)`` shape as the single-mesh path, so the
    downstream packing/sidecar/manifest writers are identical (R6.1, R6.2). Every
    non-tower entity falls through to the single-mesh path, where the mesh source is
    chosen by data: a Primitive_Entity (one that declares a ``primitive`` block) has
    its geometry generated in code by :func:`entities.effects.build_primitive_mesh`
    (R2.1, R2.7); every sourced-mesh entity (the Gen-1 reference, the Gen-2 enemies)
    is loaded from its GLB by :func:`render.loader.load_mesh`. A primitive is a single
    mesh posed by a single node, so it is **never** routed through the Gen-3 modular
    base+weapon assembly path (R2.7).
    """
    if towers_module.is_modular(entity):
        return towers_module.build_tower_frames(entity, GENERATOR_DIR, debug=debug)

    # --- unchanged Gen-1/Gen-2 single-mesh path below (R5.6) ---
    # Mesh source: a primitive is generated in code (R2.1); a GLB entity is loaded
    # from disk. Both feed the identical single-mesh pose/render/pack tail below.
    if effects_module.is_primitive(entity):
        loaded = effects_module.build_primitive_mesh(entity.primitive)
    else:
        model_path = (GENERATOR_DIR / entity.model).resolve()
        loaded = loader_module.load_mesh(model_path)

    # Build the scene once; only the mesh pose changes per frame (R5.5).
    ctx = scene_module.build_context(loaded, entity)

    poses = _build_frame_sequence(entity)
    frames = [scene_module.render_frame(ctx, pose) for pose in poses]

    layout = atlas_module.choose_grid(len(frames))
    image = atlas_module.pack_atlas(frames, layout, debug=debug)
    return image, layout


def _facing_angles_for_clip(entity: "EntityParams", clip) -> "list[float] | None":
    """On-screen barrel angle (degrees; 0 = screen-right, CCW, screen-up = +90) for
    every frame of a full-circle spin sweep — i.e. a directional "facings" clip.

    Each facing's posed forward vector is projected through the SAME fixed render
    camera (azimuth/elevation) used to render the atlas, so the engine can aim a tower
    by picking the facing whose on-screen angle is closest to the direction to its
    target. The non-uniform spacing this produces is the correct foreshortening of a
    circle of facings viewed at the camera's elevation.

    Returns ``None`` for any clip that is not a pure 360-degree spin sweep (bob, sink,
    recoil and scale must all be at rest and ``spin_deg_per_frame * frames == 360``), so
    only directional clips carry a ``facing_angles_deg`` table.
    """
    p = clip.pose
    n = clip.frames
    spin = p.spin_deg_per_frame
    is_full_sweep = (
        spin != 0.0
        and abs(spin * n - 360.0) < 1e-6
        and p.bob_amplitude == 0.0
        and p.sink == 0.0
        and p.recoil == 0.0
        and p.scale == 1.0
    )
    if not is_full_sweep:
        return None

    cam = entity.camera
    azimuth = math.radians(cam.azimuth_deg)
    elevation = math.radians(cam.elevation_deg)
    # Camera basis (same look-at convention as render/camera.py); independent of the
    # AABB fit distance, so the screen right/up axes follow purely from the angles.
    direction = np.array([
        math.cos(elevation) * math.cos(azimuth),
        math.sin(elevation),
        math.cos(elevation) * math.sin(azimuth),
    ])
    z_axis = direction / np.linalg.norm(direction)
    up = np.array([0.0, 1.0, 0.0])
    x_axis = np.cross(up, z_axis)
    x_axis /= np.linalg.norm(x_axis)   # screen right
    y_axis = np.cross(z_axis, x_axis)  # screen up

    # The Kenney weapon meshes model their barrel pointing along +Z (NOT the glTF -Z
    # "forward"), so the barrel's world direction is the posed +Z axis. Using -Z here put
    # every baked facing angle 180 degrees off, making towers aim backwards in-game.
    forward0 = np.array([0.0, 0.0, 1.0])
    angles: "list[float]" = []
    for i in range(n):
        rot = poser_module.pose_for_frame(i, n, p)[:3, :3]
        v = rot @ forward0
        beta = math.degrees(math.atan2(float(v @ y_axis), float(v @ x_axis))) % 360.0
        angles.append(round(beta, 2))
    return angles


def generate_entity(
    params: "GeneratorParams", name: str, debug: bool
) -> EntityResult:
    """Run load -> pose -> render -> pack -> write for one entity.

    Each of the three committed outputs (atlas PNG, sidecar JSON, manifest entry) is
    guarded independently so a single failure does not abort the sibling outputs;
    the returned :class:`EntityResult` records which outputs failed (R9.7).
    """
    result = EntityResult(name=name)

    # Resolving the entity may raise MissingFieldError for an unknown name; the CLI
    # validates --entity up front, but guard here too for direct callers.
    entity = params_module.resolve_entity(params, name)

    atlas_name = f"{name}.png"

    # Render + pack the atlas in memory. If this fails, no output can be produced, so
    # record it as the entity's render failure and stop (siblings handled by caller).
    try:
        image, layout = _render_atlas_image(entity, debug)
    except Exception as exc:
        result._fail("render", exc)
        return result

    # Output 1: atlas PNG -> assets/images/<entity>.png (guarded).
    atlas_path = IMAGES_DIR / atlas_name
    try:
        IMAGES_DIR.mkdir(parents=True, exist_ok=True)
        image.save(atlas_path)
        result.atlas_path = atlas_path
    except Exception as exc:
        result._fail("atlas", exc)

    # Output 2: sidecar JSON next to the atlas (guarded, independent of output 1).
    try:
        facing_angles = {}
        for clip_name, clip in entity.animations.items():
            table = _facing_angles_for_clip(entity, clip)
            if table is not None:
                facing_angles[clip_name] = table
        result.sidecar_path = manifest_module.write_sidecar(
            IMAGES_DIR, atlas_name, layout, entity.animations,
            facing_angles=facing_angles,
        )
    except Exception as exc:
        result._fail("sidecar", exc)

    # Output 3: asset_manifest.json entry with provenance (guarded, independent).
    try:
        manifest_module.update_asset_manifest(
            MANIFEST_PATH, name, atlas_name, entity.provenance
        )
    except Exception as exc:
        result._fail("manifest", exc)

    return result


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="generate_atlas.py",
        description=(
            "Instructor-only Class-090 atlas generator. Renders CC0 meshes into "
            "texture atlases plus sidecar JSON and a manifest entry."
        ),
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="regenerate every entity defined in the parameters file",
    )
    parser.add_argument(
        "--entity",
        metavar="NAME",
        default=None,
        help="regenerate only the named entity",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="overlay per-frame index labels on the produced atlas",
    )
    return parser


def main(argv: "list[str]") -> int:
    """Parse args (``--all`` | ``--entity NAME``, ``--debug``), then run the pipeline.

    ``--all`` regenerates every entity; ``--entity NAME`` regenerates one. Prints the
    atlas + sidecar paths per successful entity (R9.6); prints usage and returns a
    non-zero code if neither selector is given (R9.5). An unknown ``--entity`` name is
    reported by name with a non-zero code (R9.4). Returns 0 only on full success;
    non-zero if any output failed (R9.7).
    """
    parser = _build_parser()
    args = parser.parse_args(argv)

    # R9.5: neither selector given -> show usage and fail.
    if not args.all and not args.entity:
        parser.print_help(sys.stderr)
        print(
            "\nerror: one of --all or --entity NAME is required.",
            file=sys.stderr,
        )
        return 2

    # Load + validate the parameters file (R1). A load failure is fatal.
    try:
        params = params_module.load_params(PARAMS_PATH)
    except params_module.ParamsError as exc:
        print(f"error: failed to load parameters '{PARAMS_PATH}': {exc}", file=sys.stderr)
        return 1

    # Resolve the target entity list.
    if args.all:
        names = list(params.entities.keys())
        if not names:
            print(
                f"error: no entities defined in '{PARAMS_PATH}'.",
                file=sys.stderr,
            )
            return 1
    else:
        # R9.4: unknown --entity NAME -> error naming the entity, non-zero exit.
        try:
            params_module.resolve_entity(params, args.entity)
        except params_module.MissingFieldError:
            known = ", ".join(sorted(params.entities)) or "(none)"
            print(
                f"error: unknown entity '{args.entity}'. "
                f"Known entities: {known}.",
                file=sys.stderr,
            )
            return 1
        names = [args.entity]

    overall_ok = True
    for name in names:
        result = generate_entity(params, name, args.debug)

        # R9.6: report any outputs that succeeded (even on partial success).
        if result.atlas_path is not None:
            print(f"[{name}] atlas:   {result.atlas_path}")
        if result.sidecar_path is not None:
            print(f"[{name}] sidecar: {result.sidecar_path}")

        # R9.7: report which output(s) failed; mark the run unsuccessful.
        if not result.ok:
            overall_ok = False
            for message in result.errors:
                print(f"[{name}] FAILED {message}", file=sys.stderr)

    # R9.7: 0 only on full success; non-zero if any output failed.
    return 0 if overall_ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

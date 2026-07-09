"""Modular base+weapon tower assembly policy (Gen-3, Requirement 8).

This is the one new module Gen-3 introduces (R8.5). The Gen-1/Gen-2 single-mesh
pipeline (`render/loader.py` -> `render/scene.py::build_context` ->
`render/scene.py::render_frame`) loads and combines geometry into a *single*
`trimesh.Trimesh` and poses a *single* scene node, so it physically cannot hold
one sub-mesh at rest while displacing another. Towers require exactly that: the
shared base module stays at its +X rest orientation while the per-tower weapon
module recoils/bobs/spins/scales (R8.2, R8.3, R8.6). This module owns the
tower-*policy* — which mesh is the static base, which is the posed weapon, how the
two AABBs combine for camera framing, and how the `idle`/`fire` clips map onto
per-frame weapon poses — and contains **no per-tower branching**: the four towers
differ purely in parameter data (R8.4).

The generic two-node rendering primitive lives in `render/scene.py`
(`build_assembly_context` / `render_assembly_frame`); this module orchestrates the
load -> assemble -> pose -> render -> pack flow, mirroring
`generate_atlas._render_atlas_image` / `_build_frame_sequence` so towers produce
exactly the same `(image, layout)` shape the CLI dispatch consumes, and pack
through the unchanged `atlas` packer.

This module is import-clean on machines without a render backend: `pyrender`/GL is
imported lazily inside the `render.scene` helpers, never here.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np

# Sibling modules. Support both the package-relative import (``import
# generator.entities.towers``) and the top-level import used when running from inside
# the generator directory — the same shim pattern used in ``params.py``,
# ``render/scene.py``, and ``generate_atlas.py``.
try:  # pragma: no cover - import shim
    from .. import atlas as atlas_module
    from ..render import loader as loader_module
    from ..render import poser as poser_module
    from ..render import scene as scene_module
except ImportError:  # pragma: no cover - import shim
    import atlas as atlas_module
    from render import loader as loader_module
    from render import poser as poser_module
    from render import scene as scene_module

if TYPE_CHECKING:  # pragma: no cover - typing only
    from pathlib import Path

    from PIL.Image import Image

    from ..atlas import GridLayout
    from ..params import EntityParams


def is_modular(entity: "EntityParams") -> bool:
    """True when the entity declares a Weapon_Module (i.e. it is a tower).

    A tower supplies both a shared Base_Module (``entity.model``) and a per-tower
    Weapon_Module (``entity.weapon_model``); single-mesh entities (the Gen-1
    reference and the Gen-2 enemies) leave ``weapon_model`` ``None``.  (R8.1)
    """
    return entity.weapon_model is not None


def _combined_bounds(
    base_loaded: "loader_module.LoadedMesh",
    weapon_loaded: "loader_module.LoadedMesh",
) -> "np.ndarray":
    """Element-wise combined AABB of the base and weapon meshes.

    Each ``LoadedMesh.bounds`` has shape ``(2, 3)`` = ``[min_xyz, max_xyz]``. The
    combined box takes the element-wise ``min`` of the two minima and the
    element-wise ``max`` of the two maxima so the fixed camera frames the whole
    assembled tower (base + weapon).
    """
    combined_min = np.minimum(base_loaded.bounds[0], weapon_loaded.bounds[0])
    combined_max = np.maximum(base_loaded.bounds[1], weapon_loaded.bounds[1])
    return np.array([combined_min, combined_max], dtype=float)


def build_tower_frames(
    entity: "EntityParams",
    generator_dir: "Path",
    debug: bool = False,
) -> "tuple[list[Image], GridLayout]":
    """Load the shared Base_Module + this tower's Weapon_Module, assemble them into
    one scene, render every frame of every clip (idle then fire), and pack the atlas.

    Returns ``(atlas_image, layout)`` — the same shape
    :func:`generate_atlas._render_atlas_image` returns for the single-mesh path, so
    the CLI dispatch and downstream sidecar/manifest writers are identical (R6.1,
    R6.2).

    Per frame: the base node is held at the constant +X rest orientation
    (:func:`poser.base_orientation`); only the weapon node is posed via
    :func:`poser.pose_for_frame`, so recoil/bob/spin/scale displace the weapon while
    the base stays at rest (R8.2, R8.3, R8.6).
    """
    # 1. Resolve the shared base + per-tower weapon paths and load each with the
    #    UNCHANGED single-mesh loader (two LoadedMesh). ``weapon_model`` is non-None
    #    for any entity routed here (see is_modular); guard defensively anyway.
    if entity.weapon_model is None:  # pragma: no cover - defensive
        raise ValueError(
            f"entity '{entity.name}' has no weapon_model; not a modular tower"
        )
    base_path = (generator_dir / entity.model).resolve()
    weapon_path = (generator_dir / entity.weapon_model).resolve()
    base_loaded = loader_module.load_mesh(base_path)
    weapon_loaded = loader_module.load_mesh(weapon_path)

    # 2. Combined AABB so the fixed camera frames the whole assembled tower.
    combined_bounds = _combined_bounds(base_loaded, weapon_loaded)

    # 3. Build the two-node assembly scene once; only the weapon pose changes per
    #    frame, the base is re-set to the same rest pose every frame (R8.3).
    ctx = scene_module.build_assembly_context(
        base_loaded, weapon_loaded, entity, combined_bounds
    )

    # 4. Walk every clip in declared order (idle then fire); for a clip of N frames
    #    evaluate the WEAPON pose at i in 0..N-1 and pair it with the CONSTANT base
    #    rest pose. This mirrors generate_atlas._build_frame_sequence so the total
    #    frame count equals the sum of every clip's frame count and frame k lands in
    #    atlas cell k inside the clip range the sidecar declares (R4.8, R6.3).
    base_pose = poser_module.base_orientation()
    frames: "list[Image]" = []
    for clip in entity.animations.values():
        frame_count = clip.frames
        for frame_index in range(frame_count):
            weapon_pose = poser_module.pose_for_frame(
                frame_index, frame_count, clip.pose
            )
            # 5. Render the (static base, posed weapon) pair (R8.2, R8.3).
            frames.append(
                scene_module.render_assembly_frame(ctx, base_pose, weapon_pose)
            )

    # 6. Pack via the UNCHANGED atlas packer exactly as the single-mesh path does.
    layout = atlas_module.choose_grid(len(frames))
    image = atlas_module.pack_atlas(frames, layout, debug=debug)
    return image, layout

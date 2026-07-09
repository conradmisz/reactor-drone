"""Procedural primitive geometry builder (Gen-4, Requirement 2).

This is the one new module Gen-4 introduces (R2.8). Every Gen-1/Gen-2/Gen-3 entity
obtains its mesh from a sourced GLB via ``render/loader.py::load_mesh``; a
Primitive_Entity has no file to load. Its geometry is generated in code via
``trimesh.creation`` constructors — an icosphere (or uv-sphere) for the Cannonball
and an expanding shell for the Explosion. Those calls are *code*, not parameter
data, so they cannot be expressed in ``generator-parameters.json`` alone; this
module owns the primitive-building *policy* (Primitive_Type -> ``trimesh.creation``
constructor -> ``LoadedMesh`` shape) and contains **no per-entity branching** — the
two primitives differ purely in parameter data (R2.1, R10.4).

The builder returns the SAME ``LoadedMesh`` shape (``.mesh`` ``trimesh.Trimesh`` +
``.bounds`` ``(2, 3)`` AABB + ``.source_path``) the single-mesh render path consumes
from ``loader.load_mesh``, so ``render/scene.py::build_context`` and
``render/camera.py::frame_pose`` consume a generated mesh with **zero** changes
(R2.2, R5.6). A primitive is a single mesh posed by a single node, so it is rendered
through the unchanged single-mesh path and is **never** routed through the Gen-3
modular base+weapon assembly path (R2.7).

This module is import-clean on machines without a render backend: it imports
``trimesh`` (geometry only, no GL) and ``numpy`` but **never** ``pyrender``.
"""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np
import trimesh

# Reuse the EXACT LoadedMesh contract build_context consumes. Support both the
# package-relative import (``import generator.entities.effects``) and the top-level
# import used when running from inside the generator directory — the same shim
# pattern used in ``entities/towers.py``, ``params.py``, and ``generate_atlas.py``.
try:  # pragma: no cover - import shim
    from ..render.loader import LoadedMesh
except ImportError:  # pragma: no cover - import shim
    from render.loader import LoadedMesh

if TYPE_CHECKING:  # pragma: no cover - typing only
    from ..params import EntityParams, PrimitiveSpec


class UnsupportedPrimitiveError(Exception):
    """Raised when a Primitive_Type is not one this builder supports (R2.9)."""


def is_primitive(entity: "EntityParams") -> bool:
    """True when the entity declares a Primitive_Spec (i.e. a Primitive_Entity).

    A Primitive_Entity (the Cannonball or the Explosion) sets ``entity.primitive``;
    every sourced-mesh entity (the Gen-1 reference, the Gen-2 enemies, the Gen-3
    towers) leaves it ``None``.  (R2.7)
    """
    return entity.primitive is not None


def build_primitive_mesh(spec: "PrimitiveSpec") -> "LoadedMesh":
    """Build a Primitive_Entity's mesh from its Primitive_Spec using
    ``trimesh.creation`` and return it in the SAME ``LoadedMesh`` shape (``.mesh`` +
    ``.bounds`` + ``.source_path``) the single-mesh render path consumes from
    ``loader.load_mesh`` (R2.1, R2.2).

    Mapping (no per-entity branching, only per-*type* construction, R2.1):

    * ``icosphere`` / ``shell`` -> ``trimesh.creation.icosphere(subdivisions, radius)``.
      The ``shell`` is generated as a sphere; its visible expansion comes from the
      Scale_Parameter across the ``expand`` clip, NOT from per-frame geometry
      (R1.4, R4.3).
    * ``uv_sphere`` -> ``trimesh.creation.uv_sphere(radius, count=[count, count])``.

    Both constructors are deterministic functions of their parameters, so two builds
    with identical Primitive_Spec parameters yield identical vertex/face counts
    (R2.5), and any valid spec yields a non-empty mesh (R2.6).

    Raises :class:`UnsupportedPrimitiveError` naming the unsupported Primitive_Type
    for any type outside {``icosphere``, ``uv_sphere``, ``shell``}, returning no mesh
    (R2.9).
    """
    if spec.type in ("icosphere", "shell"):
        mesh = trimesh.creation.icosphere(
            subdivisions=spec.subdivisions, radius=spec.radius
        )
    elif spec.type == "uv_sphere":
        mesh = trimesh.creation.uv_sphere(
            radius=spec.radius, count=[spec.count, spec.count]
        )
    else:
        raise UnsupportedPrimitiveError(
            f"Unsupported Primitive_Type '{spec.type}'; "
            f"expected one of icosphere, uv_sphere, shell"
        )

    # Same AABB contract as loader.load_mesh: shape (2, 3) = [min_xyz, max_xyz].
    bounds = np.asarray(mesh.bounds, dtype=float)
    return LoadedMesh(
        mesh=mesh,
        bounds=bounds,
        source_path=Path(f"primitive:{spec.type}"),
    )

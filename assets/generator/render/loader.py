"""trimesh-based model loader with AABB and provenance support (Requirements 2.6, 3).

Loads ``.glb``/``.gltf``/``.obj`` meshes into a single combined mesh and computes the
axis-aligned bounding box. Multi-part scenes are concatenated into one renderable mesh.

The single combined :class:`trimesh.Trimesh` plus its axis-aligned bounding box are the
inputs the camera and scene stages consume downstream.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np
import trimesh

if TYPE_CHECKING:  # pragma: no cover - typing only
    import numpy

SUPPORTED_EXTENSIONS = {".glb", ".gltf", ".obj"}  # R3.2


class LoaderError(Exception):
    """Base class for all model-loader errors."""


class MissingModelError(LoaderError):
    """Raised when the model file does not exist. Names the missing model file."""  # R2.6


class UnsupportedFormatError(LoaderError):
    """Raised when the file extension is unsupported. Names the extension."""  # R3.6


class UnreadableMeshError(LoaderError):
    """Raised when trimesh cannot parse the file. Identifies it as unreadable."""  # R3.7


class CombineError(LoaderError):
    """Raised when a multi-part Scene cannot be concatenated."""  # R3.4


@dataclass
class LoadedMesh:
    mesh: "trimesh.Trimesh"  # single combined mesh
    bounds: "numpy.ndarray"  # shape (2, 3): [min_xyz, max_xyz]   (R3.5)
    source_path: Path


def _combine_scene(scene: "trimesh.Scene", path: Path) -> "trimesh.Trimesh":
    """Concatenate every geometry in a multi-part scene into one Trimesh (R3.3).

    Raises :class:`CombineError` naming the file if concatenation fails or yields
    something that is not a single mesh.  (R3.4)
    """
    try:
        combined = scene.dump(concatenate=True)
    except Exception as exc:  # pragma: no cover - defensive
        raise CombineError(
            f"Failed to combine multi-part scene from '{path}': {exc}"
        ) from exc

    # scene.dump(concatenate=True) should return a single Trimesh. If the scene was
    # empty or trimesh handed back a list/Scene, fall back to util.concatenate and
    # validate the result is a single renderable mesh.
    if isinstance(combined, trimesh.Trimesh):
        return combined

    try:
        geometries = list(scene.geometry.values())
        combined = trimesh.util.concatenate(geometries)
    except Exception as exc:
        raise CombineError(
            f"Failed to combine multi-part scene from '{path}': {exc}"
        ) from exc

    if not isinstance(combined, trimesh.Trimesh):
        raise CombineError(
            f"Failed to combine multi-part scene from '{path}': "
            f"result was {type(combined).__name__}, not a single mesh"
        )
    return combined


def load_mesh(path: Path) -> LoadedMesh:
    """Load a ``.glb``/``.gltf``/``.obj`` mesh with trimesh into a single combined mesh
    and compute its axis-aligned bounding box.

    Raises :class:`MissingModelError`, :class:`UnsupportedFormatError`,
    :class:`UnreadableMeshError`, or :class:`CombineError`.  (R2.6, R3.1-3.7)
    """
    path = Path(path)

    # R2.6: missing model file.
    if not path.exists():
        raise MissingModelError(f"Model file not found: '{path}'")

    # R3.6: unsupported extension.
    extension = path.suffix.lower()
    if extension not in SUPPORTED_EXTENSIONS:
        supported = ", ".join(sorted(SUPPORTED_EXTENSIONS))
        raise UnsupportedFormatError(
            f"Unsupported model extension '{path.suffix}' for '{path}'. "
            f"Supported extensions: {supported}"
        )

    # R3.1 / R3.7: parse the file with trimesh; parse failures are unreadable.
    try:
        loaded = trimesh.load(path, force=None)
    except Exception as exc:
        raise UnreadableMeshError(
            f"Could not read mesh file '{path}': {exc}"
        ) from exc

    # R3.3: a multi-part Scene is concatenated into one renderable mesh.
    if isinstance(loaded, trimesh.Scene):
        if len(loaded.geometry) == 0:
            raise UnreadableMeshError(
                f"Mesh file '{path}' contains no geometry"
            )
        mesh = _combine_scene(loaded, path)
    elif isinstance(loaded, trimesh.Trimesh):
        mesh = loaded
    else:
        # trimesh may return a Path2D/PointCloud/list for non-mesh content.
        raise UnreadableMeshError(
            f"Mesh file '{path}' did not yield a renderable mesh "
            f"(got {type(loaded).__name__})"
        )

    if mesh.is_empty or len(mesh.vertices) == 0:
        raise UnreadableMeshError(
            f"Mesh file '{path}' produced an empty mesh"
        )

    # Bake any UV texture into per-vertex colors. The Kenney kit GLBs reference an
    # external base-color texture (`Textures/colormap.png`) — a flat palette where
    # each face samples a solid swatch. We sample that palette at each vertex UV and
    # store the result as vertex colors so the offscreen renderer draws color WITHOUT
    # uploading a GL texture (the pyrender/PyOpenGL texture path is unavailable in some
    # environments). For these low-poly flat-shaded models, vertex colors are visually
    # equivalent to the texture and keep golden-image renders deterministic across
    # machines. If anything goes wrong, the load still succeeds with the original visual.
    if isinstance(mesh.visual, trimesh.visual.TextureVisuals):
        try:
            baked = mesh.visual.to_color()
            if getattr(baked, "vertex_colors", None) is not None:
                mesh.visual = baked
        except Exception:  # pragma: no cover - defensive: never fail a load over color
            pass

    # R3.5: axis-aligned bounding box as shape (2, 3) -> [min_xyz, max_xyz].
    bounds = np.asarray(mesh.bounds, dtype=float)
    if bounds.shape != (2, 3) or not np.all(np.isfinite(bounds)):
        raise UnreadableMeshError(
            f"Mesh file '{path}' produced an invalid bounding box: {bounds!r}"
        )

    return LoadedMesh(mesh=mesh, bounds=bounds, source_path=path)

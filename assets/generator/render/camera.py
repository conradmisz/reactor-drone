"""Fixed three-quarter perspective camera (Requirement 5).

Provides a perspective camera positioned at the three-quarter view and a pose that frames
the mesh AABB so the whole bounding box fits within the rendered frame. The pose is
computed once per entity and held constant across all frames of a cycle (R5.5).

Coordinate conventions (matching ``render/poser.py`` and the course world):

* The **vertical / up axis** is world **+Y**; **+X** is right. The camera elevation is
  measured up from the horizontal XZ plane, and the azimuth is measured in the XZ plane
  about the vertical +Y axis (0deg along +X, increasing toward +Z) so a positive
  ``azimuth_deg`` together with a positive ``elevation_deg`` places the camera at a
  raised **front corner** for the three-quarter view.
* The returned pose is a standard **look-at** camera-to-world matrix: the camera sits at
  ``eye`` and looks toward the AABB center. pyrender cameras look down their local
  ``-Z`` axis with ``+Y`` up, so the matrix columns are ``[right, up, +Z=eye-center]``.

Framing math (R5.4): the AABB bounding-sphere radius is ``r = 0.5 * ||max - min||``.
For a perspective camera with vertical field of view ``yfov``, a sphere of radius ``r``
just fits the frame at distance ``r / sin(yfov / 2)``; multiplying by ``fit_margin``
(>= 1) leaves a small border, so ``distance = (r / sin(yfov / 2)) * fit_margin``.

Only :func:`make_camera` needs ``pyrender`` (the render backend); :func:`frame_pose` is
pure NumPy so the framing math can be exercised without a display.
"""

from __future__ import annotations

import math
from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:  # pragma: no cover - typing only
    import pyrender

    from ..params import EntityParams
    from .loader import LoadedMesh


def make_camera(entity: "EntityParams") -> "pyrender.PerspectiveCamera":
    """Build a perspective camera using the entity's camera params.  (R5.1)

    The vertical field of view ``yfov`` is taken from ``camera.yfov_deg`` (converted to
    radians, as pyrender expects). ``pyrender`` is imported lazily so that the pure
    framing math in :func:`frame_pose` remains importable without the render backend.
    """
    import pyrender  # local import: only needed when a real camera is built (R5.1)

    yfov = math.radians(entity.camera.yfov_deg)
    return pyrender.PerspectiveCamera(yfov=yfov)


def _aabb_center_and_radius(bounds: np.ndarray) -> tuple[np.ndarray, float]:
    """Return the AABB center and its bounding-sphere radius ``r = 0.5 * ||max - min||``."""
    bounds = np.asarray(bounds, dtype=float)
    min_xyz = bounds[0]
    max_xyz = bounds[1]
    center = 0.5 * (min_xyz + max_xyz)
    radius = 0.5 * float(np.linalg.norm(max_xyz - min_xyz))
    return center, radius


def _look_at(eye: np.ndarray, target: np.ndarray, up: np.ndarray) -> np.ndarray:
    """Return a 4x4 camera-to-world look-at matrix for a pyrender camera at ``eye``
    looking toward ``target`` with the given ``up`` hint.

    pyrender cameras look down their local ``-Z`` axis, so the camera's local ``+Z``
    points from the target back toward the eye.
    """
    eye = np.asarray(eye, dtype=float)
    target = np.asarray(target, dtype=float)
    up = np.asarray(up, dtype=float)

    forward = eye - target  # camera local +Z (points away from the target)
    norm = np.linalg.norm(forward)
    if norm == 0:
        raise ValueError("Camera eye and target coincide; cannot build a look-at matrix")
    z_axis = forward / norm

    x_unnorm = np.cross(up, z_axis)  # camera local +X (right)
    x_norm = np.linalg.norm(x_unnorm)
    if x_norm == 0:
        # up is parallel to the view direction; pick an alternate up hint.
        alt_up = np.array([0.0, 0.0, 1.0])
        x_unnorm = np.cross(alt_up, z_axis)
        x_norm = np.linalg.norm(x_unnorm)
    x_axis = x_unnorm / x_norm

    y_axis = np.cross(z_axis, x_axis)  # camera local +Y (up)

    pose = np.identity(4, dtype=float)
    pose[:3, 0] = x_axis
    pose[:3, 1] = y_axis
    pose[:3, 2] = z_axis
    pose[:3, 3] = eye
    return pose


def frame_pose(loaded: "LoadedMesh", entity: "EntityParams") -> np.ndarray:
    """Compute the 4x4 camera pose matrix for a fixed three-quarter view at the configured
    elevation, positioned so the mesh AABB fits within the frame. The same pose is reused
    for every frame of the cycle.  (R5.2-5.5)

    The camera is placed on a raised front-corner ray at ``camera.elevation_deg`` (default
    ~30deg, R5.2/R5.3) and ``camera.azimuth_deg``; the distance is derived from the AABB
    bounding-sphere radius and the camera ``yfov`` with ``fit_margin`` so the whole box
    fits with margin (R5.4). The camera aims at the AABB center, producing a standard
    look-at matrix. Computed once per entity and reused for every frame (R5.5).
    """
    cam = entity.camera

    center, radius = _aabb_center_and_radius(loaded.bounds)

    # Direction from the AABB center toward the camera on the front-corner ray.
    # Elevation is measured up from the horizontal XZ plane; azimuth is measured in the
    # XZ plane about +Y (0deg along +X, increasing toward +Z).  (R5.2, R5.3)
    elevation = math.radians(cam.elevation_deg)
    azimuth = math.radians(cam.azimuth_deg)
    horizontal = math.cos(elevation)
    direction = np.array(
        [
            horizontal * math.cos(azimuth),  # +X right
            math.sin(elevation),             # +Y up (vertical)
            horizontal * math.sin(azimuth),  # +Z
        ],
        dtype=float,
    )
    direction /= np.linalg.norm(direction)

    # Fit distance from the bounding-sphere radius and the vertical FOV (R5.4).
    yfov = math.radians(cam.yfov_deg)
    half_fov = yfov / 2.0
    sin_half = math.sin(half_fov)
    if radius <= 0.0:
        # Degenerate (single-point) AABB: fall back to a small fixed standoff so the
        # look-at matrix is still well-defined.
        distance = max(cam.fit_margin, 1.0)
    else:
        distance = (radius / sin_half) * cam.fit_margin

    eye = center + direction * distance
    up = np.array([0.0, 1.0, 0.0])  # vertical/up axis is +Y (matches poser.py)
    return _look_at(eye, center, up)

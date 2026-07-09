"""Procedural poser: bob and spin (Requirement 6).

Computes per-frame pose transforms for static meshes: a vertical bob offset and an
accumulated spin about the vertical axis, composed on top of the +X base orientation.
This module is pure NumPy math with no render-backend dependency.

Coordinate conventions (matching the course bottom-left world: +Y up, +X right):

* The **vertical axis** is world **Y**; both the bob translation and the spin rotation
  use Y (bob translates along +Y, spin rotates about Y).
* The **base orientation** ``R_base`` orients the mesh to face **+X** (right), as
  required by R6.4 so the engine's left-facing mirror rule reads correctly. glTF/GLB
  meshes export with their forward axis along **-Z**; a ``-90`` degree rotation about Y
  maps that forward (-Z) onto +X.

Correctness properties upheld here (see design "Correctness Properties"):

* **P1 (bob loop-closure):** ``bob(0, N, A) == bob(N, N, A) == 0`` because
  ``sin(0) == sin(2*pi) == 0``.
* **P2 (zero pose at frame 0):** ``bob(0, N, A) == 0`` and ``spin_angle(0, s) == 0``.
* **P3 (+X base orientation at frame 0):** ``pose_for_frame(0, N, spec)`` reduces to
  ``base_orientation()`` because the frame-0 transform is
  ``Translate(0,0,0) @ Rot_y(0) @ R_base == R_base``.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

# Base-orientation rotation (degrees, about the vertical Y axis) that maps the glTF/GLB
# forward axis (-Z) onto +X so the mesh faces right (R6.4).
_BASE_FACING_ANGLE_DEG = -90.0


@dataclass
class PoseSpec:
    bob_amplitude: float = 0.0  # vertical offset peak (world units)
    spin_deg_per_frame: float = 0.0  # rotation about vertical axis per frame
    scale: float = 1.0  # terminal uniform scale at final frame (no-op = 1.0)
    sink: float = 0.0  # terminal downward drop at final frame (no-op = 0.0)
    recoil: float = 0.0  # peak recoil displacement along -X firing axis (no-op = 0.0)


def _rotation_y(angle_rad: float) -> np.ndarray:
    """Return a 4x4 rotation matrix about the vertical (Y) axis."""
    c = math.cos(angle_rad)
    s = math.sin(angle_rad)
    m = np.identity(4, dtype=float)
    m[0, 0] = c
    m[0, 2] = s
    m[2, 0] = -s
    m[2, 2] = c
    return m


def _translation(x: float, y: float, z: float) -> np.ndarray:
    """Return a 4x4 translation matrix."""
    m = np.identity(4, dtype=float)
    m[0, 3] = x
    m[1, 3] = y
    m[2, 3] = z
    return m


def _scale(s: float) -> np.ndarray:
    """Return a 4x4 uniform-scale matrix ``diag(s, s, s, 1)``."""
    m = np.identity(4, dtype=float)
    m[0, 0] = s
    m[1, 1] = s
    m[2, 2] = s
    return m


def base_orientation() -> np.ndarray:
    """Return the 4x4 transform that orients the mesh facing +X (right).  (R6.4)"""
    return _rotation_y(math.radians(_BASE_FACING_ANGLE_DEG))


def bob_offset(frame_index: int, frame_count: int, amplitude: float) -> float:
    """Vertical offset ``= amplitude * sin(2*pi*frame_index/frame_count)``.

    Periodic with period ``frame_count``: value at ``frame_count`` == value at 0, and
    value at frame 0 == 0 (loop closure).  (R6.2, R6.5, R6.6)
    """
    if frame_count < 1:
        raise ValueError("frame_count must be >= 1")
    return amplitude * math.sin(2.0 * math.pi * frame_index / frame_count)


def spin_angle(frame_index: int, spin_deg_per_frame: float) -> float:
    """Accumulated rotation ``= frame_index * spin_deg_per_frame`` (degrees). Zero at
    frame 0.  (R6.3, R6.5)
    """
    return frame_index * spin_deg_per_frame


def clip_progress(frame_index: int, frame_count: int) -> float:
    """Normalized progress ``t`` in ``[0, 1]`` across a clip.

    ``t(i) = i / (frame_count - 1)`` for ``frame_count >= 2``; ``t = 0.0`` for
    ``frame_count == 1``. So ``t(0) == 0.0`` and ``t(frame_count - 1) == 1.0`` for
    multi-frame clips.  (R4.4, R4.5)
    """
    if frame_count < 1:
        raise ValueError("frame_count must be >= 1")
    if frame_count == 1:
        return 0.0
    return frame_index / (frame_count - 1)


def scale_factor(frame_index: int, frame_count: int, terminal_scale: float) -> float:
    """Uniform scale ``= lerp(1.0, terminal_scale, clip_progress(i, N))``.

    Equals ``1.0`` at frame 0; equals ``terminal_scale`` at the final frame.  (R5.4)
    """
    t = clip_progress(frame_index, frame_count)
    return 1.0 + (terminal_scale - 1.0) * t


def sink_offset(frame_index: int, frame_count: int, terminal_sink: float) -> float:
    """Downward drop ``= lerp(0.0, terminal_sink, clip_progress(i, N))``.

    Equals ``0.0`` at frame 0; equals ``terminal_sink`` at the final frame.  (R5.5)
    """
    t = clip_progress(frame_index, frame_count)
    return terminal_sink * t


def recoil_offset(frame_index: int, frame_count: int, recoil: float) -> float:
    """Non-negative recoil displacement *magnitude* along the firing axis.

    ``recoil_offset(i, N, R) = R * sin(pi * clip_progress(i, N))`` — a there-and-back
    half-sine ramp over the clip:

    * frame 0:        ``sin(pi * 0)   == 0``  -> ``0.0``      (R4.6, R5.3)
    * final frame:    ``sin(pi * 1)   == 0``  -> ``0.0``      (R4.7, R5.3)
    * midpoint t=0.5: ``sin(pi * 0.5) == 1``  -> ``R`` (peak) (R5.4)

    Returns ``0.0`` for ``frame_count < 2`` (``clip_progress`` is ``0.0`` there, so no
    interior frame; R5.7) and naturally ``0.0`` when ``recoil == 0.0``. Loop-safe (zero
    at both ends) and one-shot-safe (departs from and returns to rest).  (R5.3, R5.4)
    """
    t = clip_progress(frame_index, frame_count)
    return recoil * math.sin(math.pi * t)


def pose_for_frame(frame_index: int, frame_count: int, spec: PoseSpec) -> np.ndarray:
    """Compose base (+X) orientation, accumulated spin about the vertical axis, a uniform
    scale, the net vertical translation (bob minus sink), and a recoil displacement
    anti-parallel to the +X firing axis into a single 4x4 transform.
    (R6.1, R5.3, R5.4, R5.5, R5.6, R7.4)

    The composition order is
    ``Translate(-recoil, bob - sink, 0) @ Rot_vertical(spin) @ Scale(scale) @ R_base``,
    so frame 0 (bob == 0, spin == 0, scale == 1, sink == 0, recoil == 0) reduces to
    ``R_base`` (the pure +X base orientation). The recoil magnitude is directed along
    world -X (anti-parallel to the +X firing axis, R5.5). When ``spec.recoil == 0.0`` the
    X translation is ``0.0`` and the transform reduces bit-for-bit to the Gen-2
    ``Translate(0, bob - sink, 0) @ Rot_y(spin) @ Scale(scale) @ R_base`` (R5.2); when
    additionally ``spec.scale == 1.0`` and ``spec.sink == 0.0`` it reduces to the Gen-1
    ``Translate(0, bob, 0) @ Rot_y(spin) @ R_base``.
    """
    bob = bob_offset(frame_index, frame_count, spec.bob_amplitude)
    spin_deg = spin_angle(frame_index, spec.spin_deg_per_frame)
    scale = scale_factor(frame_index, frame_count, spec.scale)
    sink = sink_offset(frame_index, frame_count, spec.sink)
    recoil = recoil_offset(frame_index, frame_count, spec.recoil)

    translate = _translation(-recoil, bob - sink, 0.0)
    rot_vertical = _rotation_y(math.radians(spin_deg))
    scale_m = _scale(scale)
    r_base = base_orientation()

    return translate @ rot_vertical @ scale_m @ r_base

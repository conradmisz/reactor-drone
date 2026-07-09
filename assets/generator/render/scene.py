"""pyrender offscreen scene assembly and rendering (Requirement 4).

Assembles a pyrender scene (mesh + camera + lights) sized to the entity frame size and
renders each frame onto a fully transparent background via ``RenderFlags.RGBA``.

Pipeline contract (consumed by ``generate_atlas.py``):

* :func:`build_context` builds the scene **once** per entity — the mesh node, the fixed
  camera node (:func:`render.camera.make_camera` + :func:`render.camera.frame_pose`), and
  at least one :class:`pyrender.DirectionalLight` "key" light at the configured
  ``key_intensity``, plus scene ``ambient_light`` from ``ambient``. The
  :class:`pyrender.OffscreenRenderer` is created at the entity ``frame_size``; if the
  offscreen backend cannot initialize (no display / missing EGL/OSMesa), the failure is
  wrapped in :class:`BackendUnavailableError` (R4.1, R4.2, R4.5).
* :func:`render_frame` replaces **only** the mesh node's transform with the per-frame
  pose matrix and renders with ``RenderFlags.RGBA``, returning a Pillow ``RGBA`` image of
  size ``(frame_size, frame_size)`` whose background pixels are fully transparent. The
  camera and lights stay fixed across every frame of the cycle (R4.3, R4.4, R5.5).

``pyrender`` (and the heavy GL backend it pulls in) is imported lazily inside
:func:`build_context` so this module imports cleanly on machines without a render
backend; the pure-logic test suite never touches it, and the golden-image test (task
11.1) gates itself on :class:`BackendUnavailableError`.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Any

import numpy as np

# Sibling render module. Support both the package-relative import
# (``import render.scene``) and the top-level import used when running from inside the
# ``render`` directory — mirroring the shim in ``params.py``.
try:  # pragma: no cover - import shim
    from . import camera as camera_module
except ImportError:  # pragma: no cover - import shim
    import camera as camera_module

if TYPE_CHECKING:  # pragma: no cover - typing only
    import pyrender
    from PIL.Image import Image

    from ..params import EntityParams
    from .loader import LoadedMesh


# White RGB used for the ambient and key-light colors; intensities scale these.
_WHITE = np.array([1.0, 1.0, 1.0], dtype=float)


class BackendUnavailableError(Exception):
    """Raised when the pyrender offscreen backend cannot initialize."""  # R4.5


@dataclass
class RenderContext:
    """Reusable render state for one entity.

    The scene, camera, lights, and offscreen renderer are built once and reused for every
    frame; only ``mesh_node``'s transform changes per frame (see :func:`render_frame`).
    """

    renderer: "pyrender.OffscreenRenderer"
    camera_node: Any
    light_nodes: list
    base_scene: "pyrender.Scene"
    frame_size: int
    mesh_node: Any  # mesh node whose transform is replaced per frame (R4.3, R5.5)


def build_context(loaded: "LoadedMesh", entity: "EntityParams") -> RenderContext:
    """Assemble a pyrender Scene: mesh node + camera + >=1 light, sized to the entity
    ``frame_size``. Lighting key intensity and ambient come from params. Raises
    :class:`BackendUnavailableError` if the offscreen backend is unavailable.
    (R4.1, R4.2, R4.5)
    """
    # Lazy import: only needed when a real render backend is exercised. Keeping it here
    # means the module (and the pure-logic test suite) imports without pyrender/GL.
    try:
        import pyrender
    except Exception as exc:  # pragma: no cover - import-time backend failure
        raise BackendUnavailableError(
            f"pyrender is unavailable; cannot build an offscreen render scene: {exc}"
        ) from exc

    frame_size = int(entity.frame_size)

    # Transparent background + ambient term scaled from params (R4.2, R4.3). pyrender's
    # ``ambient_light`` is an RGB triple; ``bg_color`` is RGBA so background pixels can be
    # cleared to alpha 0.
    ambient_light = float(entity.lighting.ambient) * _WHITE
    scene = pyrender.Scene(
        bg_color=np.array([0.0, 0.0, 0.0, 0.0], dtype=float),  # transparent (R4.3)
        ambient_light=ambient_light,                            # R4.2
    )

    # Mesh node: keep the reference so its transform can be replaced per frame (R4.3).
    mesh = pyrender.Mesh.from_trimesh(loaded.mesh)
    mesh_node = scene.add(mesh)  # identity transform; pose set per frame in render_frame

    # Fixed three-quarter camera framed to the mesh AABB (R5.1-R5.5).
    camera = camera_module.make_camera(entity)
    camera_pose = camera_module.frame_pose(loaded, entity)
    camera_node = scene.add(camera, pose=camera_pose)

    # At least one directional "key" light at the configured intensity (R4.1, R4.2). The
    # key light shares the camera pose so it lights the side of the mesh facing the
    # viewer; pyrender directional lights emit along their local -Z, matching the camera's
    # view direction. Camera + lights are fixed for the whole cycle (R5.5).
    key_light = pyrender.DirectionalLight(
        color=_WHITE, intensity=float(entity.lighting.key_intensity)
    )
    key_light_node = scene.add(key_light, pose=camera_pose)
    light_nodes = [key_light_node]

    # Offscreen renderer sized to the entity frame size (R4.4). Construction selects the
    # platform backend (Pyglet/EGL/OSMesa); failure means no usable backend (R4.5).
    try:
        renderer = pyrender.OffscreenRenderer(
            viewport_width=frame_size, viewport_height=frame_size
        )
    except Exception as exc:
        raise BackendUnavailableError(
            "pyrender offscreen backend is unavailable "
            f"(could not create OffscreenRenderer at {frame_size}x{frame_size}): {exc}"
        ) from exc

    return RenderContext(
        renderer=renderer,
        camera_node=camera_node,
        light_nodes=light_nodes,
        base_scene=scene,
        frame_size=frame_size,
        mesh_node=mesh_node,
    )


def render_frame(ctx: RenderContext, pose: "np.ndarray") -> "Image":
    """Apply ``pose`` to the mesh node, render, and return an RGBA PIL image of size
    ``(frame_size, frame_size)`` whose background pixels are fully transparent.
    (R4.3, R4.4)

    Only the mesh node's transform changes; the camera and lights stay fixed across every
    frame of the cycle (R5.5). Rendering uses ``RenderFlags.RGBA`` so the cleared
    background carries alpha 0.
    """
    import pyrender  # backend already proven available by build_context
    from PIL import Image

    # Replace only the mesh node's transform with this frame's pose (R4.3, R5.5).
    pose = np.asarray(pose, dtype=float)
    ctx.base_scene.set_pose(ctx.mesh_node, pose)

    # RGBA flag yields a (frame_size, frame_size, 4) uint8 color buffer whose background
    # pixels have alpha 0 (R4.3). The viewport equals the entity frame size (R4.4).
    color, _depth = ctx.renderer.render(
        ctx.base_scene, flags=pyrender.RenderFlags.RGBA
    )

    color = np.ascontiguousarray(color, dtype=np.uint8)
    return Image.fromarray(color, mode="RGBA")


# ---------------------------------------------------------------------------
# Additive two-node modular assembly (Gen-3, Requirement 8).
#
# Towers are assembled from a shared static base module plus a per-tower weapon
# module that recoils/bobs/spins independently of the base. The single-mesh path
# (``build_context`` / ``render_frame`` above) poses a *single* node and therefore
# cannot hold one sub-mesh at rest while moving another, so Gen-3 adds a generic
# two-node rendering primitive here. These helpers REUSE the exact transparent
# background, ambient/key-light, offscreen, combined-AABB camera, and
# ``RenderFlags.RGBA`` idioms of ``build_context`` / ``render_frame`` so the modular
# path produces pixels the same way the single-mesh path does. The single-mesh path
# above is left byte-for-byte unchanged so every Gen-1/Gen-2 render stays identical.
# ---------------------------------------------------------------------------


class _BoundsProxy:
    """Minimal stand-in carrying a ``.bounds`` AABB for :func:`render.camera.frame_pose`.

    ``camera_module.frame_pose`` reads only ``loaded.bounds`` (shape ``(2, 3)`` =
    ``[min_xyz, max_xyz]``). For a two-module assembly the camera must frame the
    *combined* bounding box of base + weapon, so the modular path passes this tiny
    proxy carrying the combined AABB rather than either single mesh's bounds. This
    frames the whole assembled tower exactly the way the single-mesh path frames its
    one mesh.
    """

    __slots__ = ("bounds",)

    def __init__(self, bounds: "np.ndarray") -> None:
        self.bounds = np.asarray(bounds, dtype=float)


@dataclass
class AssemblyRenderContext:
    """Reusable render state for one modular (base + weapon) entity.

    The scene, camera, lights, and offscreen renderer are built once and reused for
    every frame. Across frames only ``weapon_node``'s transform changes meaningfully;
    ``base_node`` is re-set to the same constant rest pose so it never deviates from
    rest (R8.3). See :func:`render_assembly_frame`.
    """

    renderer: "pyrender.OffscreenRenderer"
    base_scene: "pyrender.Scene"
    frame_size: int
    base_node: Any    # static base module node held at rest every frame (R8.3)
    weapon_node: Any  # per-frame posed weapon module node (R8.2, R8.6)


def build_assembly_context(
    base_loaded: "LoadedMesh",
    weapon_loaded: "LoadedMesh",
    entity: "EntityParams",
    combined_bounds: "np.ndarray",
) -> AssemblyRenderContext:
    """Assemble a pyrender Scene with TWO mesh nodes (static base + posed weapon), a
    camera framed to the COMBINED AABB, and >=1 key light, sized to the entity
    ``frame_size``. Reuses the exact transparent-background, ambient/key-light, and
    offscreen idioms of :func:`build_context`. Raises :class:`BackendUnavailableError`
    if the offscreen backend is unavailable.  (R8.1, R7.5)
    """
    # Lazy import: only needed when a real render backend is exercised. Keeping it here
    # means the module (and the pure-logic test suite) imports without pyrender/GL.
    try:
        import pyrender
    except Exception as exc:  # pragma: no cover - import-time backend failure
        raise BackendUnavailableError(
            f"pyrender is unavailable; cannot build an offscreen render scene: {exc}"
        ) from exc

    frame_size = int(entity.frame_size)

    # Transparent background + ambient term scaled from params (R4.2, R4.3). pyrender's
    # ``ambient_light`` is an RGB triple; ``bg_color`` is RGBA so background pixels can be
    # cleared to alpha 0.
    ambient_light = float(entity.lighting.ambient) * _WHITE
    scene = pyrender.Scene(
        bg_color=np.array([0.0, 0.0, 0.0, 0.0], dtype=float),  # transparent (R4.3, R7.5)
        ambient_light=ambient_light,                            # R4.2
    )

    # Two mesh nodes: keep both references so their transforms can be set per frame. The
    # base node is held static (R8.3); the weapon node is posed per frame (R8.2, R8.6).
    base_mesh = pyrender.Mesh.from_trimesh(base_loaded.mesh)
    base_node = scene.add(base_mesh)  # identity transform; pose set per frame
    weapon_mesh = pyrender.Mesh.from_trimesh(weapon_loaded.mesh)
    weapon_node = scene.add(weapon_mesh)  # identity transform; pose set per frame

    # Fixed three-quarter camera framed to the COMBINED AABB so the whole assembled
    # tower fits (R5.1-R5.5). ``frame_pose`` reads only ``.bounds``, so a small proxy
    # carries the combined AABB exactly the way the single-mesh path passes its mesh.
    camera = camera_module.make_camera(entity)
    camera_pose = camera_module.frame_pose(_BoundsProxy(combined_bounds), entity)
    scene.add(camera, pose=camera_pose)

    # At least one directional "key" light at the configured intensity (R4.1, R4.2),
    # sharing the camera pose so it lights the side facing the viewer; pyrender
    # directional lights emit along their local -Z, matching the camera view direction.
    key_light = pyrender.DirectionalLight(
        color=_WHITE, intensity=float(entity.lighting.key_intensity)
    )
    scene.add(key_light, pose=camera_pose)

    # Offscreen renderer sized to the entity frame size (R4.4). Construction selects the
    # platform backend (Pyglet/EGL/OSMesa); failure means no usable backend (R4.5).
    try:
        renderer = pyrender.OffscreenRenderer(
            viewport_width=frame_size, viewport_height=frame_size
        )
    except Exception as exc:
        raise BackendUnavailableError(
            "pyrender offscreen backend is unavailable "
            f"(could not create OffscreenRenderer at {frame_size}x{frame_size}): {exc}"
        ) from exc

    return AssemblyRenderContext(
        renderer=renderer,
        base_scene=scene,
        frame_size=frame_size,
        base_node=base_node,
        weapon_node=weapon_node,
    )


def render_assembly_frame(
    ctx: AssemblyRenderContext,
    base_pose: "np.ndarray",
    weapon_pose: "np.ndarray",
) -> "Image":
    """Set the base node to ``base_pose`` (the constant +X rest orientation) and the
    weapon node to ``weapon_pose``, render with ``RenderFlags.RGBA``, and return an RGBA
    PIL image of size ``(frame_size, frame_size)`` whose background pixels are fully
    transparent.  (R8.2, R8.3, R7.5)

    Only the weapon node moves between frames: the base node is re-set to the same
    constant rest pose every frame, so its world position never deviates from rest
    (R8.3). The camera and lights stay fixed across every frame (R5.5).
    """
    import pyrender  # backend already proven available by build_assembly_context
    from PIL import Image

    # Base held at rest (R8.3); weapon posed per frame (R8.2, R8.6).
    base_pose = np.asarray(base_pose, dtype=float)
    weapon_pose = np.asarray(weapon_pose, dtype=float)
    ctx.base_scene.set_pose(ctx.base_node, base_pose)
    ctx.base_scene.set_pose(ctx.weapon_node, weapon_pose)

    # RGBA flag yields a (frame_size, frame_size, 4) uint8 color buffer whose background
    # pixels have alpha 0 (R4.3, R7.5). The viewport equals the entity frame size (R4.4).
    color, _depth = ctx.renderer.render(
        ctx.base_scene, flags=pyrender.RenderFlags.RGBA
    )

    color = np.ascontiguousarray(color, dtype=np.uint8)
    return Image.fromarray(color, mode="RGBA")

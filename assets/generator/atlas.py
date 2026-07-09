"""Atlas grid packer + stitcher + debug labels (R7).

Pure grid math + Pillow compositing; no ``pyrender`` dependency. Packs rendered
frames into a uniform grid laid out left-to-right, top-to-bottom, frame 0 at the
top-left, matching the engine's ``compute_source_rect`` convention
(``col = frame_index % columns``, ``row = frame_index // columns`` — see
``CPP/engine/ecs/sprite_sheet_math.hpp``).
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import TYPE_CHECKING

from PIL import Image, ImageDraw, ImageFont

if TYPE_CHECKING:
    import PIL.Image


class LabelRenderError(Exception):
    """Raised when a debug frame-index label cannot be rendered."""  # R7.8


@dataclass
class GridLayout:
    """Grid description for an atlas.

    ``columns`` and ``rows`` are computed by :func:`choose_grid`. The pixel
    dimensions (``frame_width``, ``frame_height``) and ``total_frames`` are
    derived from the rendered frames and are filled in by :func:`pack_atlas`
    (``choose_grid`` does not know the frame size, so it leaves them at 0).
    """

    columns: int
    rows: int
    frame_width: int = 0
    frame_height: int = 0
    total_frames: int = 0


def choose_grid(total_frames: int, frames_per_row: "int | None" = None) -> GridLayout:
    """Compute the grid columns/rows for ``total_frames`` frames.

    ``columns`` defaults to ``ceil(sqrt(total_frames))`` for a near-square,
    compact atlas, or uses the explicit ``frames_per_row`` override when given.
    ``rows = ceil(total_frames / columns)``. The resulting layout always
    satisfies the engine grid invariant ``total_frames <= columns * rows``.
    """  # R7.3, R7.6
    if total_frames < 1:
        raise ValueError(f"total_frames must be >= 1, got {total_frames}")

    if frames_per_row is not None:
        if frames_per_row < 1:
            raise ValueError(
                f"frames_per_row must be >= 1, got {frames_per_row}"
            )
        columns = frames_per_row
    else:
        columns = math.ceil(math.sqrt(total_frames))

    rows = math.ceil(total_frames / columns)

    return GridLayout(
        columns=columns,
        rows=rows,
        total_frames=total_frames,
    )


def pack_atlas(
    frames: "list[PIL.Image.Image]",
    layout: GridLayout,
    debug: bool = False,
) -> "PIL.Image.Image":
    """Stitch ``frames`` into one RGBA atlas image.

    Frames are placed left-to-right, top-to-bottom with frame 0 at the
    top-left. For frame ``i`` the pixel origin is ``(col * fw, row * fh)`` where
    ``col = i % columns`` and ``row = i // columns`` — identical to the engine's
    ``compute_source_rect`` convention so a frame written at cell ``(col, row)``
    is sliced back at the same cell. All frames must be identical in size and no
    frame extends past the atlas boundary. Unused trailing cells (when the frame
    count does not fill the final row) remain fully transparent.

    When ``debug`` is True, the integer frame index is overlaid on each frame
    cell using a default Pillow bitmap font; if a label cannot be rendered a
    :class:`LabelRenderError` is raised. When ``debug`` is False no labels are
    drawn.
    """  # R7.1, R7.2, R7.4, R7.5, R7.7-R7.9
    if not frames:
        raise ValueError("pack_atlas requires at least one frame")

    # All frames must share identical dimensions (R7.2 frame uniformity).
    frame_width, frame_height = frames[0].size
    for index, frame in enumerate(frames):
        if frame.size != (frame_width, frame_height):
            raise ValueError(
                f"frame {index} size {frame.size} differs from frame 0 size "
                f"{(frame_width, frame_height)}; all frames must be identical"
            )

    # Record the resolved pixel dimensions on the layout (choose_grid leaves
    # these at 0 because it has no knowledge of the frame size).
    layout.frame_width = frame_width
    layout.frame_height = frame_height
    layout.total_frames = len(frames)

    # Canvas dimensions: width = columns * fw, height = rows * fh (R7.3).
    atlas_width = layout.columns * frame_width
    atlas_height = layout.rows * frame_height

    # Fully transparent canvas so any unwritten (trailing) cell stays
    # transparent (R7.4).
    atlas = Image.new("RGBA", (atlas_width, atlas_height), (0, 0, 0, 0))

    draw = ImageDraw.Draw(atlas) if debug else None
    font = _load_default_font() if debug else None

    for index, frame in enumerate(frames):
        col = index % layout.columns
        row = index // layout.columns
        origin_x = col * frame_width
        origin_y = row * frame_height

        # Far corner must lie within the atlas bounds (R7.5). This holds by
        # construction since col <= columns - 1 and row <= rows - 1, but we
        # guard defensively.
        if origin_x + frame_width > atlas_width or origin_y + frame_height > atlas_height:
            raise ValueError(
                f"frame {index} at ({origin_x}, {origin_y}) exceeds atlas "
                f"bounds ({atlas_width}, {atlas_height})"
            )

        rgba_frame = frame if frame.mode == "RGBA" else frame.convert("RGBA")
        atlas.paste(rgba_frame, (origin_x, origin_y))

        if debug:
            _draw_frame_label(draw, font, index, origin_x, origin_y)

    return atlas


def _load_default_font() -> "ImageFont.ImageFont":
    """Load Pillow's built-in bitmap font (no external font file needed)."""
    try:
        return ImageFont.load_default()
    except Exception as exc:  # pragma: no cover - defensive
        raise LabelRenderError(
            f"failed to load default debug-label font: {exc}"
        ) from exc


def _draw_frame_label(draw, font, index: int, origin_x: int, origin_y: int) -> None:
    """Overlay the integer frame index in the top-left of a frame cell (R7.7).

    Raises :class:`LabelRenderError` if the label cannot be drawn (R7.8).
    """
    try:
        text = str(index)
        # Small inset from the cell corner; opaque white with a dark outline so
        # the index is legible over either light or dark frame content.
        draw.text(
            (origin_x + 2, origin_y + 2),
            text,
            fill=(255, 255, 255, 255),
            font=font,
            stroke_width=1,
            stroke_fill=(0, 0, 0, 255),
        )
    except Exception as exc:
        raise LabelRenderError(
            f"failed to render debug label for frame {index}: {exc}"
        ) from exc

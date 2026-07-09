"""Parameters file loader, validator, and default-merge (Requirement 1).

Loads and validates ``generator-parameters.json``, merging per-entity overrides on top
of the ``defaults`` section. This module is pure logic with no render-backend dependency.

Validation runs in a fixed order so reporting is deterministic (R1.6-R1.10):

1. parse JSON                -> :class:`MalformedJSONError` (R1.8)
2. required-field presence   -> :class:`MissingFieldError` (R1.6, R1.10)
3. value range checks        -> :class:`RangeError`        (R1.7)

Missing-field detection precedes range checks **unconditionally** (R1.10): a file that
is both missing a required field and out of range reports the missing field.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

# PoseSpec lives in render/poser.py — import it (do not redefine). Support both the
# package-relative import (``import generator.params``) and the top-level import used by
# the CLI / ``python -c "import params"`` when run from inside the generator directory.
try:  # pragma: no cover - import shim
    from .render.poser import PoseSpec
except ImportError:  # pragma: no cover - import shim
    from render.poser import PoseSpec


# --------------------------------------------------------------------------------------
# Error hierarchy (R1.6-R1.10)
# --------------------------------------------------------------------------------------


class ParamsError(Exception):
    """Base class for all parameter errors."""


class MalformedJSONError(ParamsError):
    """Raised when the file is not parseable JSON. Identifies the file."""  # R1.8


class MissingFieldError(ParamsError):
    """Raised when a required field is absent. Names the missing field."""  # R1.6, R1.10


class RangeError(ParamsError):
    """Raised when a value is outside its valid range. Names field + constraint."""  # R1.7


# --------------------------------------------------------------------------------------
# In-memory parameter objects (Data Models)
# --------------------------------------------------------------------------------------


@dataclass(frozen=True)
class CameraParams:
    type: str
    elevation_deg: float
    azimuth_deg: float
    yfov_deg: float
    fit_margin: float


@dataclass(frozen=True)
class LightingParams:
    key_intensity: float
    ambient: float


@dataclass(frozen=True)
class ClipParams:
    name: str
    frames: int
    frame_duration: float
    looping: bool
    pose: PoseSpec


@dataclass(frozen=True)
class Provenance:
    source_url: str
    license: str


@dataclass(frozen=True)
class PrimitiveSpec:
    """A resolved Primitive_Spec block for a Primitive_Entity (R1.2, R1.6-R1.9).

    ``type`` is one of {"icosphere", "uv_sphere", "shell"}; ``radius`` is > 0.0.
    ``subdivisions`` applies to icosphere/shell (int >= 0); ``count`` applies to
    uv_sphere (int >= 3). The unused field for a given type is left ``None``.
    """

    type: str
    radius: float
    subdivisions: "int | None" = None
    count: "int | None" = None


@dataclass(frozen=True)
class EntityParams:
    name: str
    model: "Path | None"  # None for a Primitive_Entity (exactly-one-of model/primitive)
    frame_size: int
    camera: CameraParams
    lighting: LightingParams
    background: str
    provenance: Provenance
    animations: dict[str, ClipParams]
    weapon_model: "Path | None" = None  # per-tower Weapon_Module (None => single-mesh)
    primitive: "PrimitiveSpec | None" = None  # set for a Primitive_Entity (R1.2, R1.6)


@dataclass(frozen=True)
class GeneratorParams:
    defaults: dict = field(default_factory=dict)
    entities: dict[str, EntityParams] = field(default_factory=dict)


# --------------------------------------------------------------------------------------
# Default values for optional fields (per the design field-reference table)
# --------------------------------------------------------------------------------------

_CAMERA_OPTIONAL_DEFAULTS = {"azimuth_deg": 45.0, "yfov_deg": 45.0, "fit_margin": 1.1}
_VALID_FRAME_SIZES = (128, 256, 512)
_VALID_PRIMITIVE_TYPES = ("icosphere", "uv_sphere", "shell")


# --------------------------------------------------------------------------------------
# Small validation primitives
# --------------------------------------------------------------------------------------


def _is_number(value: object) -> bool:
    """True for int/float but not bool (``True``/``False`` are not valid numbers here)."""
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _range_num(
    value: object,
    path: str,
    lo: float | None = None,
    hi: float | None = None,
    hi_inclusive: bool = True,
) -> None:
    if not _is_number(value):
        raise RangeError(f"Field '{path}' must be a number (got {value!r})")
    if lo is not None and value < lo:
        raise RangeError(
            f"Field '{path}' value {value} violates constraint: must be >= {lo}"
        )
    if hi is not None:
        if hi_inclusive and value > hi:
            raise RangeError(
                f"Field '{path}' value {value} violates constraint: must be <= {hi}"
            )
        if not hi_inclusive and value >= hi:
            raise RangeError(
                f"Field '{path}' value {value} violates constraint: must be < {hi}"
            )


def _range_int(value: object, path: str, lo: int | None = None) -> None:
    if not (isinstance(value, int) and not isinstance(value, bool)):
        raise RangeError(f"Field '{path}' must be an integer (got {value!r})")
    if lo is not None and value < lo:
        raise RangeError(
            f"Field '{path}' value {value} violates constraint: must be >= {lo}"
        )


def _range_frame_size(value: object, path: str) -> None:
    if isinstance(value, bool) or value not in _VALID_FRAME_SIZES:
        raise RangeError(
            f"Field '{path}' value {value!r} violates constraint: "
            f"must be one of {', '.join(str(s) for s in _VALID_FRAME_SIZES)}"
        )


def _non_empty_string(value: object, path: str) -> None:
    if not (isinstance(value, str) and value.strip()):
        raise RangeError(f"Field '{path}' must be a non-empty string (got {value!r})")


def _check_primitive_ranges(primitive: dict, prefix: str) -> None:
    """Range/type checks for a Primitive_Spec (R1.13, R1.14).

    Validity of the Primitive_Type is checked first (R1.13) because the
    type-appropriate geometry parameter (subdivisions vs. count) depends on it;
    then ``radius > 0.0`` and the type-appropriate integer minimum (R1.14).
    """
    ptype = primitive.get("type")
    if ptype not in _VALID_PRIMITIVE_TYPES:  # R1.13
        raise RangeError(
            f"Field '{prefix}.type' value {ptype!r} violates constraint: "
            f"must be one of {', '.join(_VALID_PRIMITIVE_TYPES)}"
        )
    # radius > 0.0 for every primitive type (R1.7, R1.8, R1.9, R1.14).
    _range_num(primitive.get("radius"), f"{prefix}.radius", lo=0.0)
    if primitive["radius"] == 0.0:
        raise RangeError(
            f"Field '{prefix}.radius' value 0.0 violates constraint: must be > 0.0"
        )
    if ptype in ("icosphere", "shell"):  # R1.7, R1.9
        _require(primitive, "subdivisions", f"{prefix}.subdivisions")
        _range_int(primitive["subdivisions"], f"{prefix}.subdivisions", lo=0)
    else:  # uv_sphere  (R1.8)
        _require(primitive, "count", f"{prefix}.count")
        _range_int(primitive["count"], f"{prefix}.count", lo=3)


# --------------------------------------------------------------------------------------
# Pass 1: required-field presence (R1.6, R1.10) — runs before any range check
# --------------------------------------------------------------------------------------


def _require(container: object, key: str, path: str) -> None:
    if not isinstance(container, dict) or key not in container:
        raise MissingFieldError(f"Missing required field: {path}")


def _check_required(data: object) -> None:
    if not isinstance(data, dict):
        raise MissingFieldError(
            "Missing required field: top-level object with 'defaults' and 'entities'"
        )
    _require(data, "defaults", "defaults")
    _require(data, "entities", "entities")

    defaults = data["defaults"]
    for key in ("frame_size", "camera", "lighting", "background", "frames_per_clip"):
        _require(defaults, key, f"defaults.{key}")
    for key in ("type", "elevation_deg"):
        _require(defaults["camera"], key, f"defaults.camera.{key}")
    for key in ("key_intensity", "ambient"):
        _require(defaults["lighting"], key, f"defaults.lighting.{key}")

    entities = data["entities"]
    if not isinstance(entities, dict):
        raise MissingFieldError("Missing required field: entities (object of entities)")
    for name, entity in entities.items():
        base = f"entities.{name}"
        # provenance + animations are required for every entity (R1.5).
        for key in ("provenance", "animations"):
            _require(entity, key, f"{base}.{key}")
        # Exactly-one-of(model, primitive): reject both-present and neither-present
        # before any range check, preserving the fixed precedence (R1.12).
        has_model = isinstance(entity, dict) and "model" in entity
        has_primitive = isinstance(entity, dict) and "primitive" in entity
        if has_model == has_primitive:  # both present, or neither
            raise MissingFieldError(
                f"Entity '{name}': exactly one of '{base}.model' or '{base}.primitive' "
                f"is required (got {'both' if has_model else 'neither'})"
            )
        if has_primitive:
            for key in ("type", "radius"):
                _require(entity["primitive"], key, f"{base}.primitive.{key}")
        for key in ("source_url", "license"):
            _require(entity["provenance"], key, f"{base}.provenance.{key}")
        animations = entity["animations"]
        if not isinstance(animations, dict):
            raise MissingFieldError(
                f"Missing required field: {base}.animations (object of clips)"
            )
        for clip_name, clip in animations.items():
            cbase = f"{base}.animations.{clip_name}"
            for key in ("frame_duration", "looping"):
                _require(clip, key, f"{cbase}.{key}")


# --------------------------------------------------------------------------------------
# Pass 2: value range checks (R1.7) — runs only after all required fields are present
# --------------------------------------------------------------------------------------


def _check_camera_ranges(camera: dict, prefix: str) -> None:
    if "type" in camera and camera["type"] != "perspective":
        raise RangeError(
            f"Field '{prefix}.type' value {camera['type']!r} violates constraint: "
            f"must be 'perspective'"
        )
    if "elevation_deg" in camera:
        _range_num(camera["elevation_deg"], f"{prefix}.elevation_deg", lo=0, hi=89)
    if "azimuth_deg" in camera:
        _range_num(
            camera["azimuth_deg"], f"{prefix}.azimuth_deg", lo=0, hi=360, hi_inclusive=False
        )
    if "yfov_deg" in camera:
        _range_num(
            camera["yfov_deg"], f"{prefix}.yfov_deg", lo=1, hi=180, hi_inclusive=False
        )
    if "fit_margin" in camera:
        _range_num(camera["fit_margin"], f"{prefix}.fit_margin", lo=1.0)


def _check_lighting_ranges(lighting: dict, prefix: str) -> None:
    if "key_intensity" in lighting:
        _range_num(lighting["key_intensity"], f"{prefix}.key_intensity", lo=0)
    if "ambient" in lighting:
        _range_num(lighting["ambient"], f"{prefix}.ambient", lo=0, hi=1)


def _check_ranges(data: dict) -> None:
    defaults = data["defaults"]
    _range_frame_size(defaults["frame_size"], "defaults.frame_size")

    camera = defaults["camera"]
    if not isinstance(camera, dict):
        raise RangeError("Field 'defaults.camera' must be an object")
    _check_camera_ranges(camera, "defaults.camera")

    lighting = defaults["lighting"]
    if not isinstance(lighting, dict):
        raise RangeError("Field 'defaults.lighting' must be an object")
    _check_lighting_ranges(lighting, "defaults.lighting")

    if defaults["background"] != "transparent":
        raise RangeError(
            f"Field 'defaults.background' value {defaults['background']!r} violates "
            f"constraint: must be 'transparent'"
        )
    _range_int(defaults["frames_per_clip"], "defaults.frames_per_clip", lo=1)

    for name, entity in data["entities"].items():
        base = f"entities.{name}"
        if "model" in entity:
            _non_empty_string(entity["model"], f"{base}.model")
        if "primitive" in entity:
            _check_primitive_ranges(entity["primitive"], f"{base}.primitive")
        if "weapon_model" in entity:
            _non_empty_string(entity["weapon_model"], f"{base}.weapon_model")
        if "frame_size" in entity:
            _range_frame_size(entity["frame_size"], f"{base}.frame_size")

        provenance = entity["provenance"]
        _non_empty_string(provenance["source_url"], f"{base}.provenance.source_url")
        _non_empty_string(provenance["license"], f"{base}.provenance.license")

        # Optional per-entity camera/lighting overrides are range-checked when present.
        if "camera" in entity and isinstance(entity["camera"], dict):
            _check_camera_ranges(entity["camera"], f"{base}.camera")
        if "lighting" in entity and isinstance(entity["lighting"], dict):
            _check_lighting_ranges(entity["lighting"], f"{base}.lighting")

        for clip_name, clip in entity["animations"].items():
            cbase = f"{base}.animations.{clip_name}"
            if "frames" in clip:
                _range_int(clip["frames"], f"{cbase}.frames", lo=1)
            fd = clip["frame_duration"]
            if not _is_number(fd):
                raise RangeError(f"Field '{cbase}.frame_duration' must be a number (got {fd!r})")
            if fd <= 0:
                raise RangeError(
                    f"Field '{cbase}.frame_duration' value {fd} violates constraint: must be > 0"
                )
            if not isinstance(clip["looping"], bool):
                raise RangeError(
                    f"Field '{cbase}.looping' must be a boolean (got {clip['looping']!r})"
                )
            if "pose" in clip and isinstance(clip["pose"], dict):
                pose = clip["pose"]
                if "bob_amplitude" in pose:
                    _range_num(pose["bob_amplitude"], f"{cbase}.pose.bob_amplitude", lo=0)
                if "spin_deg_per_frame" in pose and not _is_number(pose["spin_deg_per_frame"]):
                    raise RangeError(
                        f"Field '{cbase}.pose.spin_deg_per_frame' must be a number "
                        f"(got {pose['spin_deg_per_frame']!r})"
                    )
                if "scale" in pose:
                    _range_num(pose["scale"], f"{cbase}.pose.scale", lo=0.0)
                if "sink" in pose and not _is_number(pose["sink"]):
                    raise RangeError(
                        f"Field '{cbase}.pose.sink' must be a number "
                        f"(got {pose['sink']!r})"
                    )
                if "recoil" in pose:
                    _range_num(pose["recoil"], f"{cbase}.pose.recoil", lo=0.0)
                # R3.4 / R3.8: a looping 'idle' clip may not specify a nonzero recoil.
                # The non-negative check (R5.9) above runs first, so a negative idle
                # recoil reports the non-negative violation before this check.
                if clip_name == "idle" and float(pose.get("recoil", 0.0)) != 0.0:
                    raise RangeError(
                        f"Field '{cbase}.pose.recoil' is invalid for the looping idle "
                        f"clip: must be 0.0 (recoil applies only to one-shot fire clips)"
                    )


# --------------------------------------------------------------------------------------
# Default-merge construction (R1.5)
# --------------------------------------------------------------------------------------


def _build_camera(defaults_camera: dict, entity_camera: dict | None) -> CameraParams:
    """Merge the camera sub-object key-by-key: defaults (with optional-field defaults)
    overlaid by any entity-level overrides."""
    merged: dict = {
        "type": defaults_camera["type"],
        "elevation_deg": float(defaults_camera["elevation_deg"]),
        "azimuth_deg": float(
            defaults_camera.get("azimuth_deg", _CAMERA_OPTIONAL_DEFAULTS["azimuth_deg"])
        ),
        "yfov_deg": float(
            defaults_camera.get("yfov_deg", _CAMERA_OPTIONAL_DEFAULTS["yfov_deg"])
        ),
        "fit_margin": float(
            defaults_camera.get("fit_margin", _CAMERA_OPTIONAL_DEFAULTS["fit_margin"])
        ),
    }
    if entity_camera:
        for key in ("type", "elevation_deg", "azimuth_deg", "yfov_deg", "fit_margin"):
            if key in entity_camera:
                merged[key] = (
                    entity_camera[key] if key == "type" else float(entity_camera[key])
                )
    return CameraParams(**merged)


def _build_lighting(defaults_lighting: dict, entity_lighting: dict | None) -> LightingParams:
    """Merge the lighting sub-object key-by-key."""
    merged: dict = {
        "key_intensity": float(defaults_lighting["key_intensity"]),
        "ambient": float(defaults_lighting["ambient"]),
    }
    if entity_lighting:
        for key in ("key_intensity", "ambient"):
            if key in entity_lighting:
                merged[key] = float(entity_lighting[key])
    return LightingParams(**merged)


def _build_primitive(raw: dict) -> PrimitiveSpec:
    """Resolve a Primitive_Spec block into a :class:`PrimitiveSpec` (R1.6).

    The type-appropriate geometry parameter (subdivisions for icosphere/shell, count
    for uv_sphere) is carried; the unused field is left ``None``.
    """
    return PrimitiveSpec(
        type=str(raw["type"]),
        radius=float(raw["radius"]),
        subdivisions=(int(raw["subdivisions"]) if "subdivisions" in raw else None),
        count=(int(raw["count"]) if "count" in raw else None),
    )


def _build_clip(clip_name: str, clip: dict, defaults: dict) -> ClipParams:
    frames = clip.get("frames", defaults["frames_per_clip"])
    pose_raw = clip.get("pose") or {}
    pose = PoseSpec(
        bob_amplitude=float(pose_raw.get("bob_amplitude", 0.0)),
        spin_deg_per_frame=float(pose_raw.get("spin_deg_per_frame", 0.0)),
        scale=float(pose_raw.get("scale", 1.0)),
        sink=float(pose_raw.get("sink", 0.0)),
        recoil=float(pose_raw.get("recoil", 0.0)),
    )
    return ClipParams(
        name=clip_name,
        frames=int(frames),
        frame_duration=float(clip["frame_duration"]),
        looping=bool(clip["looping"]),
        pose=pose,
    )


def _build_entity(name: str, entity: dict, defaults: dict) -> EntityParams:
    """Build a fully-merged :class:`EntityParams`: omitted optional fields fall back to
    ``defaults`` and camera/lighting sub-objects merge key-by-key (R1.5)."""
    animations = {
        clip_name: _build_clip(clip_name, clip, defaults)
        for clip_name, clip in entity["animations"].items()
    }
    return EntityParams(
        name=name,
        model=(Path(entity["model"]) if "model" in entity else None),
        frame_size=int(entity.get("frame_size", defaults["frame_size"])),
        camera=_build_camera(defaults["camera"], entity.get("camera")),
        lighting=_build_lighting(defaults["lighting"], entity.get("lighting")),
        background=entity.get("background", defaults["background"]),
        provenance=Provenance(
            source_url=entity["provenance"]["source_url"],
            license=entity["provenance"]["license"],
        ),
        animations=animations,
        weapon_model=(Path(entity["weapon_model"]) if "weapon_model" in entity else None),
        primitive=(_build_primitive(entity["primitive"]) if "primitive" in entity else None),
    )


# --------------------------------------------------------------------------------------
# Public API
# --------------------------------------------------------------------------------------


def load_params(path: Path) -> GeneratorParams:
    """Read JSON, validate, return a :class:`GeneratorParams` object.

    Raises :class:`MalformedJSONError`, :class:`MissingFieldError`, or
    :class:`RangeError` in that fixed precedence order (R1.4, R1.6-R1.8, R1.10).
    """
    path = Path(path)
    text = path.read_text(encoding="utf-8")

    # 1. Parse JSON (R1.8) — first, before any field inspection.
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        raise MalformedJSONError(
            f"Parameters file '{path}' is not valid JSON: {exc}"
        ) from exc

    # 2. Required-field presence (R1.6, R1.10) — unconditionally before range checks.
    _check_required(data)

    # 3. Value range checks (R1.7).
    _check_ranges(data)

    # 4. Build the in-memory object with defaults merged into each entity (R1.5).
    defaults = data["defaults"]
    entities = {
        name: _build_entity(name, entity, defaults)
        for name, entity in data["entities"].items()
    }
    return GeneratorParams(defaults=defaults, entities=entities)


def resolve_entity(params: GeneratorParams, name: str) -> EntityParams:
    """Return the fully-merged :class:`EntityParams` for ``name`` (defaults applied for
    any omitted optional fields). Raises :class:`MissingFieldError` if ``name`` is absent
    (R1.5)."""
    if name not in params.entities:
        raise MissingFieldError(
            f"Unknown entity: '{name}' is not defined in the parameters file"
        )
    return params.entities[name]

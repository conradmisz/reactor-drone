"""Shared pytest fixtures for the Class-090 generator test suite (Requirement 11).

This suite runs with ``pytest`` on the instructor machine only and is excluded from the
student-facing engine and game Catch2 suites (R11.1, R11.2).

Provides:
- Bounded property-test iteration constants (``NUM_OUTER_TESTS = 10``,
  ``NUM_INNER_TESTS = 5``) per the course property-test-bounds policy.
- ``tmp_path``-based directory helpers for sidecar/manifest writer tests.
- A synthetic-RGBA-frame factory for mocking the render stages (``scene.render_frame``)
  when exercising the pure-logic atlas/manifest/CLI paths without a render backend.
"""

from __future__ import annotations

from pathlib import Path
from typing import Callable

import pytest

# --- Bounded property-test iteration counts (course policy) ------------------------
# Keep these low so the suite stays fast; students/instructors may raise them locally.
NUM_OUTER_TESTS = 10  # number of different outer subjects (amplitudes, frame counts, ...)
NUM_INNER_TESTS = 5  # number of different inner values per subject


@pytest.fixture
def num_outer_tests() -> int:
    """Bounded outer iteration count for property-based tests."""
    return NUM_OUTER_TESTS


@pytest.fixture
def num_inner_tests() -> int:
    """Bounded inner iteration count for property-based tests."""
    return NUM_INNER_TESTS


@pytest.fixture
def generator_dir() -> Path:
    """Absolute path to the generator package root (parent of the tests directory)."""
    return Path(__file__).resolve().parent.parent


@pytest.fixture
def out_dir(tmp_path: Path) -> Path:
    """A clean, writable output directory for sidecar/manifest/atlas writer tests."""
    d = tmp_path / "out"
    d.mkdir(parents=True, exist_ok=True)
    return d


@pytest.fixture
def make_frame() -> Callable[..., "object"]:
    """Factory that builds a synthetic RGBA frame for mocking the render stages.

    Returns a callable ``make_frame(size=128, color=(0, 0, 0, 0))`` producing a Pillow
    ``RGBA`` image of ``size x size``. Pillow is imported lazily so importing this
    conftest never requires the render-backend dependencies.
    """

    def _make_frame(size: int = 128, color: tuple[int, int, int, int] = (0, 0, 0, 0)):
        from PIL import Image  # imported lazily; only needed when a frame is created

        return Image.new("RGBA", (size, size), color)

    return _make_frame

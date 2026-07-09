"""Entity-policy subpackage for the Class-090 atlas generator.

Gen-3 introduces this package for the one capability the data-driven single-mesh
pipeline cannot express from parameter data alone: modular base+weapon assembly,
where a shared static base mesh is held at rest while a per-tower weapon mesh is
posed (recoil/bob/spin/scale) independently of the base (R8.5).

See `.kiro/specs/090-13-generator-spec/` for the requirements, design, and tasks.
"""

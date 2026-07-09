# Model Provenance

This directory holds CC0 mesh files used by the Class-090 procedural entity atlas
generator. Each mesh is extracted from the committed Kenney kit zip by
`../extract_models.py` (Python `zipfile`, no network download).

## Source

- **Kit:** Kenney Tower Defense Kit (2.1)
- **Author / distributor:** Kenney (https://www.kenney.nl)
- **Source URL:** https://kenney.nl/assets/tower-defense-kit
- **Local archive:** `../kenney_tower-defense-kit.zip`

## License

- **License:** CC0-1.0 (Creative Commons Zero 1.0 Universal, public domain dedication)
- **License URL:** http://creativecommons.org/publicdomain/zero/1.0/
- **Confirmation:** The kit's bundled `License.txt` states: "License: (Creative Commons
  Zero, CC0)". The content may be used for personal, educational, and commercial
  purposes. Crediting "Kenney" or "www.kenney.nl" is appreciated but not required.

## Extracted GLB files

Extracted from `Models/GLB format/<name>.glb` inside the kit zip. Every file below is
sourced from the Kenney Tower Defense Kit (source URL
https://kenney.nl/assets/tower-defense-kit) under the `CC0-1.0` license:

| File | In-zip source | Source URL | License |
|------|----------------|------------|---------|
| `enemy-ufo-a.glb` | `Models/GLB format/enemy-ufo-a.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `enemy-ufo-b.glb` | `Models/GLB format/enemy-ufo-b.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `enemy-ufo-c.glb` | `Models/GLB format/enemy-ufo-c.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `enemy-ufo-d.glb` | `Models/GLB format/enemy-ufo-d.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `tower-round-base.glb` | `Models/GLB format/tower-round-base.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `weapon-cannon.glb` | `Models/GLB format/weapon-cannon.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `weapon-turret.glb` | `Models/GLB format/weapon-turret.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `weapon-catapult.glb` | `Models/GLB format/weapon-catapult.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |
| `weapon-ballista.glb` | `Models/GLB format/weapon-ballista.glb` | https://kenney.nl/assets/tower-defense-kit | CC0-1.0 |

`enemy-ufo-a.glb` is the Gen-1 **reference entity** mesh referenced by
`../generator-parameters.json` (`entities.reference.model`). The four `enemy-ufo-{a,b,c,d}.glb`
meshes back the Gen-2 enemy roster — Runner, Fast, Armored, and Boss respectively.

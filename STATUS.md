# PersonaFoil status

## Current target

PersonaFoil v0.1.0 on branch `feature/persona-identity`.

## Implemented

- Centralized native/persona identity abstraction.
- Upstream-compatible native CID → SHA-256 → uppercase UID flow.
- Persistent 16-byte persona seeds generated with libnx `randomGet`.
- Versioned `identity.json` with guarded parsing and backup-based safe writes.
- Native fallback for missing or deleted active personas.
- Create, activate, rename, delete, fingerprint, and diagnostics UI under Settings → Identity.
- All four known UID-emitting request paths routed through `GetActiveUid()`.
- Separate PersonaFoil NRO name, SD directory, configuration path, logs, branding, update endpoint, CI, and release ZIP layout.
- Host tests, controlled UID echo server, identity/testing/upstream documentation, GPLv3 and upstream attribution.

## Verified locally

- `make host-test`: pass.
- `make clean && make -j2 RELEASE=1` in `devkitpro/devkita64:latest`: pass; produced `personafoil.nro`.
- GitHub Actions push and pull-request runs: host tests and devkitPro release builds passed.
- No PersonaFoil-specific compiler warnings were observed. The pinned Plutonium/upstream build still emits existing warnings.

Local release artifacts:

- `personafoil.nro`: 14,635,004 bytes; SHA-256 `d6e5d6163d247c085ae3247f29612f457e3fbcd14f77aba57aa8160d47a5c945`.
- `personafoil.zip`: 6,736,473 bytes; SHA-256 `c4776742f9f40da4fff92a35cf7ac5784da37ab3db713ca56310cde1c4ad30cc`.
- ZIP payload: `switch/PersonaFoil/personafoil.nro`.

## Not yet verified

- Nintendo Switch hardware launch and UI interaction.
- On-device persistence across restart.
- Native UID equality against the same console running unmodified CyberFoil.
- Persona A/B stability against the controlled echo server.

These remain release-candidate gates and must not be reported as passed until measured.

## Safety statement

PersonaFoil does not write PRODINFO/NAND, alter the physical CID, modify certificates or serials, install a sysmodule, import third-party hardware identity, randomize per request, or add telemetry.

## Repository state

- Upstream: `https://github.com/luketanti/CyberFoil.git`
- Origin: `git@github.com:stupidgiraffe/PersonaFoil.git`
- Published branch: `feature/persona-identity`
- Draft pull request: <https://github.com/stupidgiraffe/PersonaFoil/pull/1>
- CI runs: [push](https://github.com/stupidgiraffe/PersonaFoil/actions/runs/33065417853) and [pull request](https://github.com/stupidgiraffe/PersonaFoil/actions/runs/33065461317)

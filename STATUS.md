# PersonaFoil Status

## Current target

Pre-release v0.1.0 on `feature/persona-identity`, tracked by draft PR #1.

## Core implementation

- Native mode preserves CyberFoil's eMMC-CID → SHA-256 → uppercase UID behavior.
- Persona mode uses persistent local 16-byte seeds with the same UID format.
- Persona create/activate/rename/delete, Native fallback, diagnostics, CI, packaging, and controlled echo-server tooling are implemented.
- **New Identity** creates and activates one saved persona in a single persisted transaction.
- The self-updater uses stable semantic versions, exact official GitHub Release assets, SHA-256 verification, actual launch-path detection, backup, and rollback handling.

## Real hardware observations

| Check | Status |
|---|---|
| PersonaFoil launches on Switch | observed |
| Persona creation / UID derivation | observed |
| Persona activation changes displayed UID fingerprint | observed |
| Outgoing HTTP UID changes Native → Persona | **not yet validated** |
| Persona UID persists across restart | **not yet validated** |
| Returning to Native reproduces original outgoing UID | **not yet validated** |
| Verified release-to-release updater | **not yet validated** |

## Remaining release gates

Controlled endpoint:

```text
Native -> UID A
Persona 1 -> UID B
restart -> UID B
Persona 2 -> UID C
Native -> UID A
```

Expected: A, B, and C are distinct; Persona 1 is stable across restart; Native returns exactly to A.

Updater: after two real releases exist, install the older release, update in-app to the newer release, relaunch, and confirm version/config/personas/Remotes/offline DB remain intact.

## Release discipline

Do not merge PR #1, create a version tag, or publish a release until explicitly approved.

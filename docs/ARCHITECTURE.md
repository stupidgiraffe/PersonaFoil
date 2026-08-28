# PersonaFoil architecture

PersonaFoil keeps identity selection separate from networking and preserves the CyberFoil-derived application around it.

```text
HTTP request code
    → identity::GetActiveUid()
        ├─ Native Switch → util::ComputeUidFromMmcCid()
        └─ Persona       → ComputeUidFromIdentityBytes(seed)
    → UID request header
```

## Components

- `identity_core` contains portable SHA-256 formatting, schema serialization/parsing, validation, fingerprint formatting, and pure state mutations. It is compiled by both the Switch application and host tests.
- `identity` owns the in-memory active state, libnx random generation, persistence, and the native/persona provider choice.
- `util/uid` retains the only physical eMMC CID access. It clears the temporary CID buffer after success or failure and exposes only the derived UID.
- The Settings Identity section consumes the identity API; it never accesses the CID or identity file directly.
- Networking modules consume only `GetActiveUid()`. They do not branch on identity mode.

## Persistence boundary

General CyberFoil-derived settings remain in `config.json`. Persona state is isolated in versioned `identity.json`, allowing stricter validation and preventing legacy configuration rewrites from damaging persona data.

The store writes a complete temporary file, moves the last good file to `.bak`, and then renames the temporary file into place. Parse failures do not trigger a write. Mutating operations are blocked after a malformed/unsupported load so the original data is not silently replaced.

## Threading

Identity state is protected by one mutex. Request code snapshots only the active persona seed while locked, derives the UID after unlocking, and clears the snapshot. Native UID derivation retains upstream one-time caching.

## Scope

This architecture virtualizes only the `UID` value emitted by PersonaFoil. It does not modify global Horizon services, PRODINFO, NAND, certificates, console serials, or other applications.

## Verified updater boundary

Update discovery and parsing are separated from Switch filesystem installation. Portable `update_core` logic owns stable semantic-version parsing, exact asset selection, trusted release-URL checks, and strict `SHA256SUMS.txt` parsing. The Switch-specific updater obtains the launched NRO path from process argv, downloads only official stable GitHub Release assets, verifies SHA-256 before touching the installed executable, stages `.new`, preserves `.bak`, and attempts rollback on final replacement failure.

Persona/config/offline-database/Remote data are outside the updater transaction.

## Diagnostic reports

Diagnostic export is privacy-minimizing: it records build/environment/state fingerprints useful for support but excludes physical CID, full UID, persona seed material, passwords, Remote credentials/URLs, Authorization headers, and authentication tokens.

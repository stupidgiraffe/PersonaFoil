# Identity model

## Native Switch

```text
physical eMMC CID
    → SHA-256
    → uppercase 64-character native UID
```

This is CyberFoil's original behavior. PersonaFoil opens the libnx filesystem device operator, reads the 16-byte eMMC CID into a short-lived buffer, calculates the UID, clears the buffer, and returns only the derived string. Native mode remains available at all times.

## Persona

```text
locally generated persistent 16-byte persona seed
    → SHA-256
    → uppercase 64-character persona UID
```

libnx `randomGet` supplies the seed from its OS-seeded ChaCha generator. A persona seed is generated once, saved locally, and reused across requests and application restarts. PersonaFoil never derives entropy from timestamps, counters, `rand()`, or request timing.

Each persona has:

- a random 128-bit internal ID;
- a user-visible name;
- a random 128-bit seed;
- a creation timestamp;
- schema version information in persisted state.

## Selection invariant

At any instant the active identity is either `native` or an existing persona ID. Parsing a missing active ID and deleting the active persona both fall back to Native Switch. The networking layer asks for the active UID and has no hardware/persona conditionals of its own.

## Security and privacy properties

- The real CID is never persisted or shown.
- Persona seeds never modify physical console state.
- A persona is stable; no per-request randomization exists.
- Persona state is local and works offline.
- No telemetry or identity upload is added.
- Full derived UIDs are sent only where the CyberFoil-compatible request protocol already requires `UID`; the normal UI shows a shortened fingerprint.
- Malformed or future-schema identity files are preserved and mutations are blocked.

The seed is not a physical CID, but it is private material: copying it reproduces the same persona UID.

## Non-goals

PersonaFoil does not virtualize Nintendo accounts, certificates, serial numbers, console services, other homebrew, or Horizon globally. It does not implement service-specific bypass, quota evasion, ban evasion, third-party identity import, PRODINFO/NAND writes, or an Atmosphère sysmodule.

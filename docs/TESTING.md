# Testing PersonaFoil

## Automated host tests

Run:

```bash
make host-test
```

The suite verifies SHA-256 formatting, uppercase 64-character output, a known native-compatible 16-byte vector, deterministic and distinct persona seeds, configuration round trips, malformed configuration handling, missing-active fallback, active-persona deletion, and duplicate-ID rejection.

## Build validation

With devkitPro/devkitA64 and dependencies installed:

```bash
make clean
make -j"$(nproc)" RELEASE=1
test -s personafoil.nro
```

Compilation is necessary but is not hardware validation.

## Controlled UID endpoint

On a developer-owned machine reachable from the Switch LAN:

```bash
python3 tools/uid_echo_server.py --bind 0.0.0.0 --port 8080
```

Configure a PersonaFoil Remote with that machine's LAN address, for example `http://192.168.1.50:8080`, and trigger a request. Each line contains only:

```json
{"timestamp":"...","client":"...","uid":"...","path":"/..."}
```

Authorization and unrelated headers are deliberately excluded.

## Required stability procedure

Record the full UID from the controlled server, not from a third-party service.

1. Select **Native Switch** and request `/native-1`. Record `UID A`.
2. Create and activate **Persona 1**. Request `/persona-1-first`. Record `UID B`.
3. Exit PersonaFoil normally and launch it again.
4. Confirm **Persona 1** is still active. Request `/persona-1-restart`. Confirm the UID is still `B`.
5. Create and activate **Persona 2**. Request `/persona-2`. Record `UID C`.
6. Select **Native Switch**. Request `/native-2`. Confirm the UID is again `A`.

Expected invariants:

```text
A != B
A != C
B != C
Persona 1 before restart == Persona 1 after restart
Native before personas == Native after returning
```

Also inspect `sdmc:/switch/PersonaFoil/identity.json` and confirm it contains persona seeds but no physical CID, passwords, authorization headers, or shop credentials.

## UI and failure checks

- Rename a persona and confirm its fingerprint does not change.
- Delete an inactive persona and confirm the active selection remains unchanged.
- Delete the active persona and confirm Native Switch becomes active.
- Launch with no `identity.json` and confirm Native Switch plus an empty list.
- Place malformed JSON at `identity.json`; confirm diagnostics reports failure, the file remains unchanged, and persona mutations are rejected.
- Restore a valid file and confirm the saved active persona returns.

## Reporting

Record firmware, Atmosphère, libnx/devkitPro build environment, application/full-mode launch method, test-server version, observed fingerprints, and whether the application was restarted. Do not publish raw persona seeds, credentials, Authorization headers, or the physical CID.

## Current real-hardware status

Observed: PersonaFoil launches; persona creation/derivation executes; activating a persona changes the displayed UID fingerprint.

Still required before outgoing identity is called hardware validated:

```text
Native -> controlled endpoint -> UID A
Persona 1 -> controlled endpoint -> UID B
restart -> controlled endpoint -> UID B
Persona 2 -> controlled endpoint -> UID C
Native -> controlled endpoint -> UID A
```

Require `A != B`, `A != C`, `B != C`, Persona 1 stability across restart, and exact Native equality before/after.

The in-app updater is not hardware validated until an actual older stable release successfully verifies/installs a newer stable release and preserves PersonaFoil user state.

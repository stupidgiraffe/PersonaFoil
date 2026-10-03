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

## v0.1.2 candidate acceptance

`make host-test` now exercises the actual HTTP client against local HTTP/TLS
servers, plus release metadata, aggregation, compatibility, schema migration,
input gating and transactional catalog cache logic. Python pipeline tests use
synthetic feeds. None establishes real Switch acceptance.

Test this candidate on a real Switch and record observations individually:

1. Launch in full-memory mode and confirm normal rendering.
2. Existing personas and names remain intact.
3. Activate Native; run the controlled UID echo sequence below.
4. Existing saved Remotes retain endpoints, credentials, names and favorites.
5. Add OpenNX through Discover Shops without typing endpoint fields.
6. Load OpenNX while some children fail; usable results and source warnings remain.
7. Connect a modern CyberFoil Remote, including icons, cheats and save-sync.
8. Connect a plaintext/encrypted custom index with Auto; check its request profile.
9. Confirm private credentials/UAUTH never reach a different child/redirect origin.
10. Network/config errors are silent; optional navigation/success audio follows Sound.
11. Rapid/repeated B from forms, dialogs and nested layouts never exits the app.
12. B on home and the Exit tile both ask before exit; Plus/Minus do not quit home.
13. Against the current stable release, update check reports Up to date on equal/older versions.
14. Update an older build to a newer published version: SHA-256, `.new`, `.bak`,
    relaunch and preserved personas/config/Remotes/offline DB. Exercise failed
    replacement rollback separately on controlled storage.
15. Browse last-good/bundled catalog offline; failed or corrupt refresh retains it.

Controlled identity sequence: Native → UID A; Persona 1 → UID B; restart → UID B;
Persona 2 → UID C; Native → UID A. Assert A, B and C differ, B persists, and Native
restores A exactly. Do not include full UID/seed/credential values in public reports.

Additional platform checks: packaged CA bundle opens through Switch libcurl,
verified GitHub asset redirects succeed, bad certificates fail, console time is
valid, large indexes stay within memory limits and all inherited install methods
(SD, USB, HDD, network, Remote, MTP) retain expected behavior.

Optional live service checks use the same host transport/parser binaries:
`build-host/http_tests --github` checks the official stable release endpoint.
`build-host/catalog_tests <downloaded-catalog> <temporary-cache-directory> <entry-count>`
validates published pipeline output through the C++ client and its transactional
cache, including corruption rejection and backup recovery. These checks establish
host integration and do not replace the Switch matrix.

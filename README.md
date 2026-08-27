# PersonaFoil

[![PersonaFoil CI](https://github.com/stupidgiraffe/PersonaFoil/actions/workflows/personafoil-ci.yml/badge.svg)](https://github.com/stupidgiraffe/PersonaFoil/actions/workflows/personafoil-ci.yml)
[![GPLv3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

PersonaFoil is a Nintendo Switch homebrew client for application-level identity virtualization in privacy, interoperability, development, and controlled testing. It is derived from [CyberFoil](https://github.com/luketanti/CyberFoil) and preserves CyberFoil's native UID behavior while adding persistent, selectable local personas.

PersonaFoil does not alter the console's physical identity, PRODINFO, NAND, certificates, or serial number. It is intended for user-owned and self-hosted services, not service-specific bypasses, quota circumvention, or ban evasion.

> Screenshots will be added after real-hardware UI validation. The v0.1.0 interface exposes persona management under **Settings → Identity**.

## Features

- Native Switch mode compatible with CyberFoil's existing UID derivation.
- Persistent personas backed by locally generated 16-byte random seeds.
- Create, activate, rename, inspect, and delete personas in the application.
- Stable, shortened UID fingerprints without displaying the physical eMMC CID.
- Safe fallback to Native Switch when an active persona is missing or deleted.
- Versioned, application-specific identity configuration with guarded writes.
- Identity diagnostics that omit credentials, authorization headers, and raw hardware identifiers.
- Host-side identity and persistence tests.
- A minimal controlled UID echo server for A/B identity verification.
- CI output as both `personafoil.nro` and an SD-ready ZIP.

## Identity model

Native mode retains the upstream algorithm:

```text
physical 16-byte eMMC CID → SHA-256 → uppercase 64-character UID
```

Persona mode changes only the 16-byte input:

```text
persistent local persona seed → SHA-256 → uppercase 64-character UID
```

The selected UID is used by the legacy-compatible `UID` request header. HAUTH, UAUTH, Authorization, TLS behavior, language, version, revision, and the CyberFoil-compatible default User-Agent remain otherwise unchanged.

See [docs/IDENTITY_MODEL.md](docs/IDENTITY_MODEL.md) for the architecture and safety properties.

## Installation

1. Download `personafoil.zip` from a release or CI artifact.
2. Extract it to the root of the SD card.
3. Launch PersonaFoil from the Homebrew Menu, preferably in full application mode.

The package installs to:

```text
switch/PersonaFoil/personafoil.nro
```

This path is separate from CyberFoil, so both applications can be installed side by side.

## Persona management

Open **Settings → Identity**. The page shows the current identity, a shortened UID fingerprint, Native Switch, every saved persona, creation and diagnostic actions.

- Select **Native Switch** to restore upstream hardware-derived behavior.
- Select a persona to activate, rename, delete, or view its fingerprint.
- Select **Create new persona** to generate a persistent identity using libnx's OS-seeded random generator.
- Deleting the active persona returns safely to Native Switch.

PersonaFoil does not offer per-request randomization or hardware-CID import.

## Configuration

PersonaFoil uses its own SD-card directory:

```text
sdmc:/switch/PersonaFoil/config.json
sdmc:/switch/PersonaFoil/identity.json
```

`identity.json` uses schema version 1 and stores the selected persona ID plus each persona's ID, name, 16-byte seed encoded as hexadecimal, and creation time. It never stores the physical eMMC CID.

Writes use a temporary file and preserve the previous state as `identity.json.bak`. If `identity.json` is malformed or uses an unsupported schema, PersonaFoil preserves it, uses Native Switch in memory, and blocks persona mutations until the file is repaired or moved.

Treat `identity.json` as private: possession of a persona seed reproduces that persona UID. It is not a hardware identity and should not be shared casually.

## Privacy model

- Persona generation and selection work completely offline.
- No PersonaFoil telemetry, analytics, automatic identity upload, or project-controlled service is used.
- The raw physical CID is held only long enough to calculate the native SHA-256 UID and is then cleared.
- Normal UI and diagnostics display only shortened derived-UID fingerprints.
- Existing Remote credentials remain in CyberFoil's existing configuration architecture; PersonaFoil does not duplicate them into identity storage.

## Controlled testing

Run the local echo endpoint on a developer-owned LAN machine:

```bash
python3 tools/uid_echo_server.py --bind 0.0.0.0 --port 8080
```

Point a PersonaFoil Remote at that machine and request a path. The server prints only UTC timestamp, User-Agent, UID, and request path; it intentionally omits Authorization and all other headers.

Follow the complete Native → Persona A → restart → Persona B → Native procedure in [docs/TESTING.md](docs/TESTING.md).

## Building

Install devkitPro/devkitA64, libnx, the existing CyberFoil dependencies, and initialize submodules. Then run:

```bash
git submodule update --init --recursive
make host-test
make -j"$(nproc)" RELEASE=1
```

The NRO is written to `personafoil.nro`. CI uses `devkitpro/devkita64:latest`, installs `switch-ntfs-3g` and `switch-lwext4`, runs host tests, builds the release NRO, and packages `switch/PersonaFoil/personafoil.nro` in `personafoil.zip`.

## Compatibility

- Derived from CyberFoil 1.4.6-era upstream code.
- Native UID formatting is regression-tested against a known 16-byte SHA-256 vector.
- PersonaFoil is an application-level client feature and requires no Atmosphère sysmodule or reboot.
- Remote compatibility can depend on server behavior beyond the UID header; personas do not virtualize accounts, credentials, certificates, console services, or global hardware identity.

## Troubleshooting

- **Identity configuration reports an error:** move `identity.json` aside for diagnosis or repair it manually. PersonaFoil will not overwrite a malformed file automatically.
- **A persona is not active:** check **Settings → Identity → Diagnostics** and confirm its fingerprint before testing.
- **The UID echo server is unreachable:** confirm both devices are on the same LAN, use the host machine's LAN address instead of `127.0.0.1`, and allow the selected port through the host firewall.
- **The app fails in Applet Mode:** launch Homebrew Menu while holding `R` over an installed title, then start PersonaFoil.
- **A server still associates requests together:** the service may use credentials or other signals in addition to UID. PersonaFoil intentionally changes only the UID input.

## Limitations

- v0.1.0 does not virtualize identity outside PersonaFoil.
- No per-request randomization or third-party CID import is provided.
- Real Switch UI, persistence, and controlled endpoint testing must still be performed before calling v0.1.0 hardware-validated.
- The default legacy-compatible `cyberfoil` User-Agent is preserved to isolate UID behavior.

## Upstream and modifications

PersonaFoil is based on [luketanti/CyberFoil](https://github.com/luketanti/CyberFoil). The upstream developer does not necessarily endorse PersonaFoil.

Meaningful changes include the centralized identity abstraction, persistent persona store, Settings UI, UID-header integration, diagnostics, host tests, controlled test server, separate application/configuration paths, PersonaFoil branding, and release packaging. See [docs/UPSTREAM_SYNC.md](docs/UPSTREAM_SYNC.md) before integrating upstream changes.

Existing upstream and third-party copyright notices and license files are retained.

## License

PersonaFoil is distributed under the GNU General Public License v3.0. See [LICENSE](LICENSE). Bundled components retain their applicable notices in the repository's other `*.LICENSE` files.

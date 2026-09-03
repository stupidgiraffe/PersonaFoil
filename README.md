<p align="center">
  <img src="docs/assets/personafoil-logo.png" alt="PersonaFoil" width="520">
</p>

<h1 align="center">PersonaFoil</h1>

<p align="center"><strong>Application-level identity profiles for Nintendo Switch homebrew.</strong></p>
<p align="center">Persistent personas • Native fallback • Verified updates • No NAND identity changes</p>

<p align="center">
  <a href="https://github.com/stupidgiraffe/PersonaFoil/releases">Releases</a> ·
  <a href="docs/IDENTITY_MODEL.md">Identity model</a> ·
  <a href="docs/TESTING.md">Testing</a> ·
  <a href="https://github.com/stupidgiraffe/PersonaFoil/issues">Report a bug</a> ·
  <a href="https://buymeacoffee.com/stupidgiraffe">☕ Support PersonaFoil</a>
</p>

> **Pre-release:** v0.1.0 has not been published yet, so there are intentionally no `releases/latest/download/...` links on this page. Once the first release exists, GitHub Releases will provide the standalone NRO, SD-ready ZIP, and checksums.

## What PersonaFoil does

PersonaFoil is derived from [CyberFoil](https://github.com/luketanti/CyberFoil) and adds persistent, selectable **application-level personas**. It virtualizes the UID that PersonaFoil sends in CyberFoil-compatible requests while leaving unrelated authentication and platform identity mechanisms unchanged.

### Native mode

```text
physical eMMC CID
      ↓
   SHA-256
      ↓
CyberFoil-compatible UID
```

### Persona mode

```text
persistent local 16-byte persona seed
      ↓
   SHA-256
      ↓
Persona UID
```

The selected UID is used by PersonaFoil's centralized request identity layer.

## Why PersonaFoil?

- Create persistent local personas and switch between them.
- Return to **Native Switch** at any time.
- Persona seeds remain local on the SD card.
- No per-request randomization.
- No telemetry added by PersonaFoil.
- Stable-release updater verifies SHA-256 before replacing the NRO.
- Diagnostic reports deliberately omit sensitive identity/authentication material.

## What PersonaFoil does **not** modify

PersonaFoil does not write or alter:

- PRODINFO
- NAND identity data
- the physical eMMC CID
- console serial number
- device certificates
- Nintendo Account identity

Persona selection also does not automatically virtualize HAUTH, UAUTH, Authorization credentials, TLS identity, or other third-party account state.

## Quick start

1. Install `personafoil.nro` as `sdmc:/switch/PersonaFoil/personafoil.nro` (or use the SD-ready ZIP once v0.1.0 is released).
2. Launch PersonaFoil in full-memory mode when practical.
3. Open **Settings → Identity**.
4. Choose **New Identity** to create and activate one persistent persona.
5. Choose **Use Native Switch** whenever you want PersonaFoil's original native UID behavior.

Persona state is stored at:

```text
sdmc:/switch/PersonaFoil/identity.json
```

PersonaFoil uses temporary/backup writes and does not persist the raw physical CID.

## Current hardware validation

Observed on a real Nintendo Switch:

- PersonaFoil launches successfully.
- Persona creation/UID derivation executes successfully.
- Activating a persona changes the UID fingerprint shown in PersonaFoil.

Still pending as a controlled release gate:

```text
Native -> UID A
Persona 1 -> UID B
restart -> UID B
Persona 2 -> UID C
Native -> UID A
```

This controlled echo-endpoint sequence is required before claiming outgoing UID persistence/fallback hardware validation. The updater likewise requires a real release-to-release hardware test.

## Updates

**Settings → System → Check Updates** checks only the official stable GitHub Release endpoint. A valid update requires exact `personafoil.nro` and `SHA256SUMS.txt` assets, semantic-version comparison, SHA-256 verification, and a safe `.new` / `.bak` replacement transaction.

**Auto Update** is an optional startup *check*, off by default. It never silently installs an update. See [Updating](docs/UPDATING.md).

## Diagnostics and troubleshooting

**Settings → Identity → Export Diagnostic Report** writes a timestamped report under:

```text
sdmc:/switch/PersonaFoil/diagnostics/
```

The report excludes physical CID, full UID, persona seeds, passwords, Remote credentials, Authorization headers, and authentication tokens.

When reporting a problem, attach the diagnostic report if useful and remove anything you do not want public.

## Documentation

- [Identity model](docs/IDENTITY_MODEL.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing](docs/TESTING.md)
- [Updating](docs/UPDATING.md)
- [v0.1.0 release draft](docs/RELEASE_V0.1.0.md)
- [Contributing](CONTRIBUTING.md)
- [Security](SECURITY.md)
- [Roadmap](ROADMAP.md)
- [Changelog](CHANGELOG.md)

## Development

```sh
make host-test
make RELEASE=1
```

CI performs host tests and a clean devkitA64 build/package. PersonaFoil remains GPLv3 and retains upstream CyberFoil attribution.

## Support PersonaFoil

If PersonaFoil is useful to you, you can support continued development:

<p align="center">
  <a href="https://buymeacoffee.com/stupidgiraffe"><strong>☕ Buy Me a Coffee</strong></a>
</p>

<p align="center">
  <a href="https://buymeacoffee.com/stupidgiraffe"><img src="docs/assets/buy-me-a-coffee-qr.png" alt="Buy Me a Coffee QR code" width="300"></a>
</p>

## Credits and license

PersonaFoil is derived from [CyberFoil](https://github.com/luketanti/CyberFoil) and incorporates code under the licenses preserved in this repository. PersonaFoil itself remains distributed under the [GNU GPL v3](LICENSE).

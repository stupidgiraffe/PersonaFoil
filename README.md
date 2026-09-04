<p align="center">
  <img src="romfs/images/personafoil-logo.png" alt="PersonaFoil" width="520">
</p>

<h1 align="center">PersonaFoil</h1>

<p align="center"><strong>Persistent application-level identities for Nintendo Switch homebrew.</strong></p>
<p align="center">Create identities • Switch instantly • Return to Native • Update in-app</p>

<p align="center">
  <a href="https://github.com/stupidgiraffe/PersonaFoil/releases/latest/download/personafoil.nro"><strong>⬇ Download NRO</strong></a>
  &nbsp;•&nbsp;
  <a href="https://github.com/stupidgiraffe/PersonaFoil/releases/latest/download/personafoil.zip"><strong>⬇ SD-ready ZIP</strong></a>
  &nbsp;•&nbsp;
  <a href="https://github.com/stupidgiraffe/PersonaFoil/releases/latest"><strong>Latest Release</strong></a>
</p>

<p align="center">
  <a href="docs/IDENTITY_MODEL.md">Identity model</a>
  &nbsp;•&nbsp;
  <a href="docs/TESTING.md">Testing</a>
  &nbsp;•&nbsp;
  <a href="https://github.com/stupidgiraffe/PersonaFoil/issues">Report a bug</a>
  &nbsp;•&nbsp;
  <a href="https://buymeacoffee.com/stupidgiraffe">☕ Buy Me a Coffee</a>
</p>

## What PersonaFoil does

PersonaFoil is derived from [CyberFoil](https://github.com/luketanti/CyberFoil) and adds persistent, selectable application-level identities called **personas**.

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

## Highlights

- Create persistent local identities with **New Identity**.
- Newly created identities activate immediately.
- Switch between saved identities at any time.
- Return to **Native Switch** whenever you want the original CyberFoil-compatible UID behavior.
- Persona seeds stay local on the SD card.
- No per-request randomization.
- No telemetry added by PersonaFoil.
- In-app updates use official GitHub Releases and verify SHA-256 before replacing the NRO.
- Diagnostic reports deliberately omit sensitive identity/authentication material.

## Install

### Standalone NRO

Download:

**[personafoil.nro](https://github.com/stupidgiraffe/PersonaFoil/releases/latest/download/personafoil.nro)**

Copy it to:

```text
sdmc:/switch/PersonaFoil/personafoil.nro
```

### SD-ready ZIP

Download:

**[personafoil.zip](https://github.com/stupidgiraffe/PersonaFoil/releases/latest/download/personafoil.zip)**

Extract it to the root of the Switch SD card.

Checksums:

**[SHA256SUMS.txt](https://github.com/stupidgiraffe/PersonaFoil/releases/latest/download/SHA256SUMS.txt)**

## Quick start

1. Install PersonaFoil.
2. Launch it in full-memory mode when practical.
3. Open **Settings → Identity**.
4. Choose **New Identity**.
5. PersonaFoil creates, saves, and activates the new identity.
6. Choose **Use Native Switch** whenever you want to return to native UID behavior.

Persona state is stored at:

```text
sdmc:/switch/PersonaFoil/identity.json
```

## What PersonaFoil does not change

PersonaFoil does not write or alter:

- PRODINFO
- NAND identity data
- physical eMMC CID
- console serial number
- device certificates
- Nintendo Account identity

Persona selection also does not automatically virtualize HAUTH, UAUTH, Authorization credentials, TLS identity, or other third-party account state.

## Hardware status

Confirmed on a real Nintendo Switch:

- PersonaFoil launches successfully.
- Persona creation and UID derivation run successfully.
- Activating a persona changes the UID fingerprint displayed by PersonaFoil.

Still being validated with the controlled echo test:

```text
Native -> UID A
Persona 1 -> UID B
restart -> UID B
Persona 2 -> UID C
Native -> UID A
```

The updater also still needs its first real release-to-release hardware test.

## Updates

**Settings → System → Check Updates** checks the official stable GitHub Release endpoint.

A valid update requires the exact `personafoil.nro` and `SHA256SUMS.txt` assets, semantic-version comparison, SHA-256 verification, and the safe `.new` / `.bak` replacement transaction.

**Auto Update** is an optional startup update check and is off by default. It does not silently install updates.

See [Updating](docs/UPDATING.md).

## Diagnostics and troubleshooting

**Settings → Identity → Export Diagnostic Report** writes a timestamped report under:

```text
sdmc:/switch/PersonaFoil/diagnostics/
```

The report excludes physical CID, full UID, persona seeds, passwords, Remote credentials, Authorization headers, and authentication tokens.

## Documentation

- [Identity model](docs/IDENTITY_MODEL.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing](docs/TESTING.md)
- [Updating](docs/UPDATING.md)
- [Contributing](CONTRIBUTING.md)
- [Security](SECURITY.md)
- [Roadmap](ROADMAP.md)
- [Changelog](CHANGELOG.md)

## Development

```sh
make host-test
make RELEASE=1
```

CI performs host tests and a clean devkitA64 release build/package.

## Support PersonaFoil

If PersonaFoil is useful to you, you can support continued development:

<p align="center">
  <a href="https://buymeacoffee.com/stupidgiraffe"><strong>☕ Buy Me a Coffee</strong></a>
</p>

GitHub's repository funding configuration also points to the same Buy Me a Coffee page.

## Credits and license

PersonaFoil is derived from [CyberFoil](https://github.com/luketanti/CyberFoil) and incorporates code under the licenses preserved in this repository. PersonaFoil itself remains distributed under the [GNU GPL v3](LICENSE).

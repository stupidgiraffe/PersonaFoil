# Updating PersonaFoil

PersonaFoil uses only stable releases from `stupidgiraffe/PersonaFoil` on GitHub.

## Manual check

Open **Settings → System → Check Updates**. When a newer stable release exists, PersonaFoil displays current/latest versions and offers **View Changelog**, **Download & Install**, or **Cancel**.

## Verification

A valid release must provide:

- `personafoil.nro`
- `personafoil.zip`
- `SHA256SUMS.txt`

The updater selects assets by exact filename, downloads `personafoil.nro`, verifies its SHA-256 against the checksum file, and only then replaces the running NRO. It preserves the previous executable as `<running>.bak` and attempts rollback if final replacement fails.

PersonaFoil refuses automatic installation when it cannot safely determine the actual launched NRO path. It does not update `identity.json`, `config.json`, the offline database, Remotes, credentials, or persona seeds.

## Automatic checks

**Auto Update** means “check for updates at startup.” It is off by default and never silently installs an update. Network failures do not block normal startup.

## Hardware validation status

The verified updater still requires a real release-to-release hardware test before it should be described as hardware validated.

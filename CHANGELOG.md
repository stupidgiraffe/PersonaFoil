# Changelog

All notable PersonaFoil-specific changes are documented here.

## [Unreleased]

## [0.1.1] - 2026-09-05

### Changed
- Version bump to provide the first real in-app updater validation path from development/public v0.1.0 installs.
- No identity, networking, or updater behavior changes from the validated v0.1.0 codebase.

## [0.1.0] - 2026-09-05

### Added
- Persistent application-level personas using locally generated 16-byte seeds.
- Native Switch fallback preserving CyberFoil's original UID derivation.
- Identity management, diagnostics, controlled UID echo testing, CI, and release packaging.
- Verified GitHub Release updater architecture with semantic-version checks, exact asset selection, SHA-256 verification, and backup/rollback handling.
- Safe diagnostic report export and GitHub project/community infrastructure.

### Changed
- Persona creation now creates and activates a new persistent identity in one saved transaction.
- Automatic app-update checking defaults to off and never silently installs an update.

### Security
- Updater rejects drafts/prereleases, untrusted asset URLs, missing/malformed checksums, and unknown/unsafe running NRO paths.

## 0.1.2 candidate

- Repair no-range HTTP requests and distinguish updater HTTP/network/release states.
- Preserve SHA-256/staged rollback and verify official-service HTTPS with bundled CAs.
- Load custom-index aggregates partially, with bounded traversal and source warnings.
- Default Remotes to Auto compatibility; preserve old profile/config formats.
- Add off-device public shop discovery, validated catalog cache and OpenNX presets.
- Add Discover Shops with provenance, health, Add/Test/Saved actions.
- Confirm exit centrally and consume input across navigation/dialog/keyboard transitions.
- Silence error audio; keep navigation/success audio under the global preference.
- Promote current identity, active personas, fingerprints and New Persona in Settings.
- Expand host regression tests; real Switch acceptance remains required.

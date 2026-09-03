# Changelog

All notable PersonaFoil-specific changes are documented here.

## [Unreleased]

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

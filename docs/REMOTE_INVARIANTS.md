# PersonaFoil v0.1.2 Remote invariants

These invariants describe the network, configuration and installer contracts established by
PersonaFoil v0.1.2. New source types (PersonaBridge, Telegram-backed catalogs, DBI-compatible
HTTP repositories, and future providers) extend these contracts. They do not replace them.

## HTTP transport

- `inst::http::Request::range` is optional. An absent range never emits a Range header.
- A ranged streaming request succeeds only with HTTP 206. Error/redirect bodies never flow into
  install writers.
- Response size, timeout and redirect behavior stay bounded.
- Official PersonaFoil GitHub/catalog traffic verifies TLS using the packaged CA bundle.
- Credentials and custom headers are scoped to an exact origin: scheme, host and effective port.
  They are stripped before a cross-origin redirect.
- A network failure is represented as structured transport/HTTP state, not inferred from an empty
  response body.

## Remote negotiation

- New saved Remotes default to `Auto`.
- Auto may negotiate Modern `/api/remote/sections`, Modern `/api/shop/sections`, or a custom
  index at the configured root.
- Manual Modern/Tinfoil compatibility remains an advanced override.
- Capability caching is keyed by endpoint, credentials, compatibility and active identity and is
  short-lived. A cached result must never move credentials to another origin.
- Legacy/Tinfoil request headers are retried only when compatibility evidence requires them; they
  are not sent to arbitrary public hosts by default.

## Custom-index traversal

- Root failure is fatal. Child failure is a warning when usable content remains.
- Traversal is cycle-safe and bounded by depth, source count, item count, bytes and wall-clock time.
- Cross-origin children do not inherit Basic auth, custom headers or provider API keys.
- Effective redirect URLs participate in cycle/deduplication checks.
- A failed child is staged independently and cannot partially mutate the accepted aggregate.
- Remote content cannot silently add, rename or delete saved Remote profiles.

## Configuration

- Existing endpoint, title, favorite, username/password and custom settings survive migration.
- The new `compatibility` field takes precedence; legacy `legacyMode` remains readable/writable
  for backward compatibility where required.
- Secrets remain local configuration. Discovery/catalog metadata never contains credentials or
  token-bearing URLs.

## Catalog discovery

- Discovery uses a validated last-good cache with bundled fallback.
- Refresh is transactional: stage, parse/validate, read back, replace, retain recovery copy.
- Catalog corruption detection is not a publisher signature; HTTPS and the official publication
  origin establish transport trust.
- Discovery describes public endpoints and health only. It is not an install-content mirror.

## Installer boundary

- Existing NSP/NSZ/XCI/XCZ install paths remain authoritative.
- New source layers provide metadata and byte streams; they do not create a second installer.
- JBOD/chunk helpers in `network_util.cpp` are part of the compatibility surface and must not be
  removed during HTTP cleanup.
- Source-specific authentication, retry and caching belong before the installer writer. The writer
  receives only the selected successful content stream.

## Identity boundary

- Native mode remains physical eMMC CID -> SHA-256 -> uppercase 64-character UID.
- Persona mode remains persistent local random seed -> SHA-256 -> uppercase 64-character UID.
- Source expansion must not persist or expose the physical CID or persona seed.
- Provider sessions (for example Telegram MTProto credentials) must never be stored on the Switch.

## Release gate

Merging the v0.1.2 implementation does not mark it stable. Real-Switch acceptance in
`docs/TESTING.md` remains the release gate, including controller/touch behavior, updater flow,
OpenNX behavior and actual install paths.

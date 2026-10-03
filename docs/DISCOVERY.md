# Remote compatibility and shop discovery

`Auto` is the default for new Remotes. Existing `legacyMode: true` profiles load as
`Tinfoil`; false profiles load as `Auto`. The new `compatibility` field takes
precedence. Saved credentials, titles, favorites, endpoints and filenames remain
local user configuration. Older builds still receive a compatible `legacyMode`
key when a profile is saved.

## Negotiation and aggregation

PersonaFoil probes modern `/api/remote/sections` and `/api/shop/sections` endpoints,
then the configured root index. Explicit JSON feeds are tried directly. It detects
modern sections and custom `files`, `paths`, `titledb` or `directories` schemas.
A challenged source can retry with the Tinfoil request profile; unrelated public
hosts do not receive legacy headers during the first probe. Capability results
expire after 15 minutes and are keyed by endpoint, credentials, compatibility and
active identity. Manual Modern/Tinfoil overrides remain under advanced settings.

Root failures are fatal. Child failures are collected as warnings; usable content
continues loading. The Remote header summarizes partial results and `+ Sources`
opens failure details. Item provenance records the supplying manifest.
Traversal limits are 6 levels, 32 manifests, 100,000 items and 64 MiB cumulative
response data, with a 90-second traversal budget and 5-second child requests.
Individual responses and decoded payloads are limited to 16 MiB; JSON nesting is
limited to 64 levels. Canonical endpoints are visited once, including redirect
aliases. A failed child is staged separately and cannot leave partial items.

Private Basic authentication is restricted to the configured origin (scheme,
host and effective port). Cross-origin children do not inherit credentials,
API keys or custom download headers. Redirects strip custom headers and Basic
authentication before crossing an origin. Custom-index `locations` no longer
silently add, rename or delete saved Remotes; catalog additions require user action.

## Discover Shops

Settings → Remote → Discover Shops loads a validated local cache immediately,
falling back to the bundled OpenNX CyberFoil and Tinfoil feed entries. No network
is required to browse or add those presets. Inspect shows endpoint, provenance,
health, HTTP result, authentication requirements and check time. Add fills the
existing RemoteProfile fields and asks only for account credentials when required.
An already-saved endpoint opens the existing profile for editing; discovery never
overwrites its credentials, custom name, favorite or modified endpoint.
Test connection probes format compatibility without changing the active Remote.

Refresh is user-triggered. Catalog connectivity and the health of each endpoint
are separate: a failed refresh keeps the last-good catalog. `.new` staging,
read-back validation and `.bak` recovery protect the cache. The catalog is fetched
only from the official repository's `catalog-data` branch over verified HTTPS.
Until that branch is first published, the bundled catalog remains usable.

## Catalog v1

`catalog-v1.json` has `schema_version: 1`, `generated_at`, `entries`, and `sha256`.
The checksum covers the UTF-8 encoding of compact, recursively key-sorted JSON
for `entries`, with unescaped Unicode. It detects corruption; it is not a signature.
HTTPS and the official source establish transport trust.

Each entry contains: `id`, `title`, `protocol`, `host`, `port`, `path`,
`compatibility`, `authentication`, `source_url`, `provenance`, `discovered_at`,
`last_checked_at`, `health`, `http_result`, `redirect_target`, and `tags`.
Health is `online`, `authentication_required`, `degraded`, `offline`,
`invalid_response`, or `unknown`. An empty check time means never probed.
Limits: 256 entries and 256 KiB total. Credentials and token-bearing URLs are
rejected. Discovery metadata is distinct from saved Remote configuration.

`tools/catalog_sources.json` is the curated input. `tools/build_catalog.py` reads
public feeds, discovers directory endpoint metadata, deduplicates canonical
addresses and performs at most 64 child probes. It never publishes file lists,
content, credentials or persona identity. It rejects nonpublic network addresses
and unsafe redirects. Failed feeds retain previous entries. Community additions
can be submitted as reviewed changes to the curated input.

The daily/manual GitHub Action validates output and atomically advances only the
`catalog-data` ref through GitHub's Git API. It never commits to master. A failed
run leaves the previously published ref intact. The workflow becomes scheduled
only after it is merged to the default branch; the implementation PR is not merged
automatically.

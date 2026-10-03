# PersonaBridge protocol v1

PersonaBridge is a companion content service. PersonaFoil remains the Switch UI and installer.
The bridge owns provider sessions, catalog indexing, source health, transfer scheduling and
read-through caching.

The protocol is intentionally provider-neutral. Telegram and DBI-compatible HTTP repositories must
normalize into the same wire model.

## Transport

- HTTP/1.1 or later.
- JSON responses use `application/json; charset=utf-8`.
- Content delivery supports standard byte ranges and returns 206 for ranged success.
- Pairing/authentication uses a revocable bridge token. Provider credentials are never sent to or
  stored by PersonaFoil.
- The Switch must impose its own response/time/entry limits even when the bridge is trusted.

## Endpoint contract

### GET /v1/health

Returns bridge status and per-source health. It must not include provider credentials or raw
authentication errors that contain secrets.

### GET /v1/catalog

Returns:

- `schema_version`: 1
- `generated_at`: RFC3339 timestamp or empty string
- `sources`: normalized source metadata
- `releases`: normalized installable content

A release has a stable bridge release ID, title ID, explicit content type, version, size, source ID
and opaque download ID. Optional display metadata may include name, hash, region and language tags.

### POST /v1/reconcile

Request:

- `schema_version`: 1
- `installed`: installed content records from Horizon

Response:

- `schema_version`: 1
- `actions`: updates or missing content
- each action contains ordered source alternatives for the same logical release
- `warnings`: non-fatal provider/index issues safe to show in diagnostics

PersonaBridge must not request a downgrade. Equivalent releases from multiple sources remain
alternatives rather than being collapsed without provenance.

### GET /v1/content/{download_id}

Returns the selected content stream.

- `Range: bytes=a-b` must be supported.
- Range success must be HTTP 206 with a valid `Content-Range`.
- A provider may be Telegram, DBI HTTP or another source; the Switch does not need to know.
- The bridge may serve cached bytes and may resume/fill sparse cached ranges.
- Cross-provider fallback may occur only between releases PersonaBridge has determined equivalent.

### GET /v1/jobs

Returns durable transfer/index job state. Job control endpoints are reserved for a later protocol
minor version; v1 clients must tolerate unknown extra fields.

## Normalized enums

### content_type

- `base`
- `update`
- `dlc`
- `multi`

### source health

- `unknown`
- `online`
- `degraded`
- `offline`
- `auth_required`

### reconcile reason

- `update`
- `missing`

## Source model

A source is metadata only:

- `id`: stable bridge identifier
- `name`: display name
- `kind`: e.g. `telegram`, `dbi_http`, `local_http`
- `health`
- `priority`: lower values are preferred when health and content are equivalent

No source object may expose passwords, Telegram authorization/session material, cookies, API keys
or token-bearing URLs.

## Release identity and deduplication

`id` identifies one bridge release record. `download_id` is an opaque fetch handle and may change
when a provider object is reindexed.

Logical equivalence requires at minimum the same:

- title ID
- content type
- content version

Size/hash may strengthen equivalence but a mismatch must prevent transparent fallback until the
bridge has reconciled it explicitly.

The Switch never guesses equivalence from filenames.

## Bounds required by the Switch parser

The v1 PersonaFoil parser rejects documents exceeding:

- 64 sources
- 20,000 releases
- 4,096 reconciliation actions
- 8 alternatives per action
- 64 warnings
- 256-byte normal identifiers/names
- 1,024-byte warning text
- 16 language tags per release

Server implementations should keep normal responses far below these ceilings.

## Implementation order

1. Stabilize this wire contract and its fixtures.
2. Implement PersonaBridge storage/adapter registry and these endpoints.
3. Add Telegram and DBI HTTP adapters independently against the same source interface.
4. Add sparse range cache/scheduler.
5. Add reconciliation/fallback policy.
6. Build the controller-first Library/Updates/DLC/Downloads/Sources UI over this protocol.

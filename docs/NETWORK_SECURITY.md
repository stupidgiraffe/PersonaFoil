# Network trust and request boundaries

Official GitHub updater metadata, checksum/NRO downloads and discovery catalogs
use peer and hostname verification. A Mozilla CA bundle from curl's CA Extract
service is packaged as `romfs:/cacert.pem`; no console certificate changes are
made. The shipped bundle SHA-256 is:

`a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`

Source: https://curl.se/docs/caextract.html and https://curl.se/ca/cacert.pem
License: Mozilla Public License 2.0, as stated by the CA Extract service.
The bundle includes its generation date and source attribution. Refresh it from
that source and verify the published checksum during future maintenance.
The Switch curl backend's bundled-CA handling and console-clock behavior still
require hardware acceptance. Host tests reject a self-signed TLS server.

Existing general homebrew Remotes, media and install sources retain the upstream
TLS compatibility setting. Those connections do **not** authenticate their TLS
peer. This preserves existing servers without assuming their certificates work
with the Switch port libraries. A global Remote TLS change needs real-device and
ecosystem validation; it is not necessary for verified official-service traffic.

The shared GET client allows HTTP(S) only, rejects URL-embedded credentials,
limits redirects and response size, and forbids HTTPS downgrade for verified
requests. Each redirect is evaluated before the next request. Basic authentication
and custom headers stay at the original credential origin. curl-owned metadata is
copied before handle cleanup. Mutation/save-sync requests do not follow redirects,
so private upload bodies and identity headers cannot be replayed to another host.

The updater still requires stable semantic versions, exact `personafoil.nro` and
`SHA256SUMS.txt` assets under official PersonaFoil GitHub release URLs, matching
SHA-256, a safe running NRO path, `.new` staging and `.bak` rollback. SHA-256
verification is separate from TLS verification. Catalog checksums likewise are
corruption checks, not publisher signatures.

Diagnostics omit secrets. Tests use explicitly synthetic credentials only.

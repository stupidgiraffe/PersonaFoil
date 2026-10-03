# PersonaFoil status

## Implementation candidate

Target: v0.1.2, branch `feature/personafoil-next`.
Baseline: public master `b6f85e93cd4a8cb0d0dac693e4107ba10683e21f` (v0.1.1).
Upstream inspected: CyberFoil 1.4.6, `2089fd169e6df5879bb87c87dd555f44451c626e`.
The baseline already includes upstream Remote/install/input changes through that
commit. No blind merge or source replacement was performed.

Implemented: typed bounded HTTP responses/ranges, release-check states, verified
HTTPS for official services, resilient custom-index aggregation, compatibility
negotiation/config migration, discovery pipeline/cache/browser/OpenNX presets,
central exit confirmation/input release gate, silent error handling and improved
persona presentation. Existing seed/UID storage and Native derivation are unchanged.

Validated implementation commit: `02020d4bbe197da18b9e227dc0850ce995f14571`.
Draft PR: [#5](https://github.com/stupidgiraffe/PersonaFoil/pull/5).
Both [push CI](https://github.com/stupidgiraffe/PersonaFoil/actions/runs/37119784801)
and [PR CI](https://github.com/stupidgiraffe/PersonaFoil/actions/runs/37119787324)
passed the host suite and Switch release build. No local Switch toolchain was
installed. The reviewed source retains Native/persona derivation and all install
methods; target compiler repairs preserve the libnx SHA-256 API and JBOD helpers.

Host checks cover identity, updater, actual HTTP/TLS/range/redirect/streaming
behavior, aggregation/negotiation/limits, saved-config migration, input release,
catalog integrity/cache retention and the discovery pipeline. A live C++ GitHub
release check returned Up to date against v0.1.1. The published 13-entry catalog
passed the same C++ parser and transactional-cache tests.

Candidate package: `dist/candidates/0.1.2-02020d4b/` contains `personafoil.nro`,
`personafoil.zip` and `SHA256SUMS.txt`. Both checksums verify. NRO magic, ZIP SD
layout (`switch/PersonaFoil/personafoil.nro`), identical executable bytes and
embedded CA/catalog assets were verified. NRO SHA-256:
`cb6074b7119dc206210286620b587f7a3ab25bae7bc58e86469dc6e52f2218c9`.
[CI artifact](https://github.com/stupidgiraffe/PersonaFoil/actions/runs/37119784801/artifacts/11272942014).

Public catalog branch `catalog-data` now contains 13 endpoints at
`cdd2263c6a15d52f67b09cfdbc944b2615a10e32`. Its downloaded payload and checksum
match the published source. Current host probes classify 2 online, 1 degraded,
4 offline and 6 invalid responses; these are dated metadata observations, not
real-Switch install acceptance. Scheduled refresh awaits a reviewed default-branch
workflow merge.

## Hardware acceptance required

The prior v0.1.1 observations do not validate this candidate. All 15 checks in
`docs/TESTING.md` remain real-Switch gates, including outgoing identity persistence,
OpenNX partial loading, private credentials, controller behavior, catalog cache
and release-to-release updates. Host fixtures establish software behavior only.

## Delivery discipline

Draft PR and installable candidate are delivered. No merge, tag or stable release
was performed. Plane PFOIL records implementation evidence; PFOIL-11 remains in
progress for real-Switch acceptance. Documentation-only completion commits do
not rebuild the already verified executable.

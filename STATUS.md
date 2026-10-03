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

Host validation passed for identity, updater, HTTP/TLS/redirect/range behavior,
aggregation/negotiation/limits, migration, input gating, catalog integrity/cache
retention and discovery publishing data. Target build and final review are pending.
No local Switch toolchain is installed; CI provides the target build.

## Hardware acceptance required

The prior v0.1.1 observations do not validate this candidate. All 15 checks in
`docs/TESTING.md` remain real-Switch gates, including outgoing identity persistence,
OpenNX partial loading, private credentials, controller behavior, catalog cache
and release-to-release updates. Host fixtures establish software behavior only.

## Delivery discipline

Prepare a draft PR and CI installable artifact. Do not merge, tag or publish a
stable release automatically. Plane PFOIL is the task system of record.

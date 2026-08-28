# PersonaFoil v0.1.0 release draft

> Draft only. Do not publish until controlled outgoing-UID validation and release readiness checks are complete.

## Download

Release assets will be `personafoil.nro`, `personafoil.zip`, and `SHA256SUMS.txt`.

## Highlights

- Persistent local application-level identities (“personas”)
- One-action **New Identity** creation and activation
- Explicit **Use Native Switch** fallback
- Verified in-app GitHub Release updates
- Safe diagnostic report export
- No PRODINFO, NAND, certificate, serial, or eMMC-CID modification

## Install

Copy `personafoil.nro` to `sdmc:/switch/PersonaFoil/personafoil.nro`, or extract the SD-ready ZIP to the SD-card root. Launch in full-memory mode when possible.

## Known validation boundary

Real Switch launch and displayed persona UID changes have been observed. Controlled outgoing HTTP UID persistence/fallback testing and a real release-to-release updater test remain release gates.

# Contributing to PersonaFoil

PersonaFoil is a GPLv3 Nintendo Switch homebrew project derived from CyberFoil. Its identity feature is intentionally application-level, local, persistent, and reversible.

## Development setup

Use devkitPro/devkitA64 with the dependencies already described by CI. Clone submodules recursively. The main local checks are:

```sh
make host-test
make RELEASE=1
git diff --check
```

CI is the authoritative clean devkitA64 build when your local environment differs.

## Pull requests

Keep changes focused, explain privacy/security impact, and include tests for portable logic. UI changes should include real screenshots when practical. Preserve CyberFoil attribution and GPL obligations.

## Identity and privacy constraints

Do not add physical-CID persistence/logging, persona-seed logging, PRODINFO/NAND/certificate mutation, or per-request randomization without a separate architecture discussion. Never include passwords, tokens, Authorization headers, or private Remote credentials in issues or fixtures.

## Translations

Keep language files valid JSON and use concise strings that fit the Switch UI. English fallback is preferable to malformed or guessed translations.

## Hardware reports

State exactly what was observed. Distinguish displayed UID changes from controlled outgoing-header validation, and do not claim updater validation until an actual release-to-release update has succeeded.

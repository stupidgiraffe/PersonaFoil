# Upstream synchronization

PersonaFoil retains these remotes:

```text
origin   https://github.com/stupidgiraffe/PersonaFoil.git
upstream https://github.com/luketanti/CyberFoil.git
```

Fetch and review upstream before integrating it:

```bash
git fetch upstream
git log --oneline --left-right feature/persona-identity...upstream/master
git diff feature/persona-identity...upstream/master -- source/util/uid.cpp source/util/curl.cpp source/util/network_util.cpp source/remoteInstall.cpp source/util/save_sync.cpp
```

Merge or rebase only on an explicit integration branch according to the repository's current policy. Never rewrite upstream history.

Identity-sensitive conflicts require special review:

1. Preserve upstream native `fsDeviceOperatorGetMmcCid` semantics.
2. Keep all UID request paths routed through `identity::GetActiveUid()`.
3. Re-run the repository-wide searches for `UID`, `Uid`, `uid`, `ComputeUidFromMmcCid`, and `fsDeviceOperatorGetMmcCid`.
4. Run `make host-test` and a clean release NRO build.
5. Repeat controlled Native/Persona A/Persona B/restart testing before release.

Do not resolve conflicts by moving hardware CID access into networking or by duplicating persona conditionals across request implementations.

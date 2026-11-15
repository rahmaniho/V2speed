# V2Speed First Browser Prototype

## What is implemented

The prototype is served at `/` by `shared/smart-connect/web-demo/server.py`. It uses the supplied V2Speed wordmark without cropping, provides Persian RTL UI, fetches the three user-specified subscription files from fixed GitHub API routes, parses supported VLESS/VMess/Trojan/Shadowsocks rows, displays endpoint and source attribution, filters the in-memory list, stores favorite URI hashes (never URI credentials) in the browser's local storage, copies individual configs only on request, and provides each source's subscription URL for manual import into compatible third-party clients. The `/csv` route preserves the separate experimental observation-ranking tool.

The browser client never receives full credentials for every node up front. The server holds the returned subscription data in process memory for up to five minutes, limits forced refreshes to one every three minutes to stay within GitHub's unauthenticated API quota, and copies a selected URI only on an explicit per-node request. Uploads for the CSV tool are written to a unique temporary file and removed after ranking. No database or persistent storage was introduced. HTTPS deployments also register a PWA manifest and service worker using proportional 192/512 derivatives of the supplied app icon. The worker caches static app-shell assets only; `/api/` subscription and config responses are network-only and never persisted by the browser.

## Requested subscription sources

The supplied raw links are fetched through the equivalent fixed GitHub API content endpoints, because the hosted environment permits `api.github.com` rather than `raw.githubusercontent.com`:

| Source | User-provided raw file | API content endpoint | Inspected snapshot |
|---|---|---|---|
| iboxz | `free-v2ray-collector/main/main/mix` | `api.github.com/repos/iboxz/free-v2ray-collector/contents/main/mix?ref=main` | `ada22e4fe09844984cab44a1985679f4c747097d` |
| ThomasJasperthecat | `sub/main/sublist1.txt` | `api.github.com/repos/ThomasJasperthecat/sub/contents/sublist1.txt?ref=main` | `db1e4b6f02def3800ad161481a4571cf679a2b58` |
| youfoundamin | `V2rayCollector/main/mixed_iran.txt` | `api.github.com/repos/youfoundamin/V2rayCollector/contents/mixed_iran.txt?ref=main` | `1d9b9ddafef831b7bb48f46d8f1a5903259242c3` |

At inspection, the first file contained mixed plaintext URI rows, the second was a Base64-encoded Shadowsocks list, and the third was mixed plaintext URI rows. Their contents are dynamic; displayed counts are parsed at runtime and must not be treated as stable or as probe results. The app does no DNS, TCP, handshake, throughput, or VPN-tunnel testing.

## Licensing, safety, and attribution

These public configuration lists can contain reusable credentials and point at third-party servers. They are untrusted external input; the app displays only a parsed endpoint and label, and never automatically connects or executes the entries. User action is required to copy a full URI. The app warns that availability, security, privacy and authorization are unverified.

No license file was present at the inspected shallow snapshots of these three repositories. The iboxz README describes its configs as free to use without subscription, but also includes an “All rights reserved” notice. The other two repositories provide no clear license grant in the inspected README/snapshot. No list contents were copied into this repository; source links are attributed in the UI. Legal review is required before bundling, redistributing, or using the lists in a commercial/proprietary release.

## Verification scope and remaining blockers

- Thirteen standard-library Python tests cover plaintext/base64 subscriptions, SS whole-authority Base64, VMess JSON, supported schemes, malformed/unknown inputs, accepted/duplicate/skipped row accounting, credential-free public summaries, PWA icon mapping, service-worker API-cache exclusion, source-refresh throttling, and source-link copy UI. The app displays those feed-parser counts so format drift is visible instead of silently dropping rows. Run with `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s shared/smart-connect/web-demo -p 'test_*.py'`.
- Live source fetch/listing and selected-config copy endpoints were exercised against the currently accessible GitHub API snapshots in the development environment; counts at that check were 336, 17, and 591 distinct accepted rows respectively and may change on refresh.
- Browser/API ranking smoke test, C domain tests, sanitizers, CMake/CTest, and static analysis remain separate evidence.
- This is a browser prototype, not Android/iOS. It has no V2Ray/Xray engine, tunnel, connection state, real probe, latency or speed test, and it does not validate that a copied config works.
- Native Android/iOS packaging, engine and protocol licensing review, device testing, privacy/security review, and operational authorization of the third-party endpoints remain required.

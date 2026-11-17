# V2Speed NEXT — Phase 0 Engineering Audit

**Audit date:** 2026-11-14
**Repository:** `rahmaniho/V2speed`
**Branch / PR:** `arena/107e3c42-v2speed` / PR #2 (open)
**Audited head:** `a80f29fc453b251561dc01a95dedadf9d6425b0b`
**Scope:** repository contents, existing Android spike, portable Smart Connect domain, CI, supplied brand assets, license notices, and the product/architecture requirements supplied for V2Speed NEXT.

> **Audit conclusion:** This repository is an early engineering prototype, not the requested dual-source production VPN platform. It contains a native Android VPN-service spike, a feed parser/config generator, a small portable adaptive-scoring domain, browser catalog experiments, and brand assets. It contains **no جاویدنامان وطن-operated VPN fleet, no production registry/API, no user-owned profile manager, no iOS app, no admin panel, and no backend**. A successful Android APK build proves compilation only; the tunnel has not been verified on a physical device. Phase 0 is complete as a repository audit, subject to the evidence and open questions below. No implementation code was changed during this audit.

## 1. Audit method and boundaries

Inspected tracked files and recent Git history; Android Gradle and manifest configuration; the Android activity, parser, VPN service and Xray adapter; CI workflows; portable C and Python test/build definitions; existing architecture, licensing, platform, feature-parity, and risk documents; third-party notices; and supplied brand-image inventory. Ran the repository's brand-asset verifier, portable C test suite, browser-prototype Python tests, and shell syntax validation. Queried the open PR/head status.

This is an engineering/product audit, **not** a legal opinion, a full upstream source-code audit, an SBOM, a penetration test, an infrastructure assessment, or a device test. The sandbox has no Android/JDK/Gradle/ADB, Go, Xcode/Swift, or Kotlin compiler, so native builds and device execution could not be repeated locally. GitHub Actions is the available Android build evidence.

## 2. Repository inventory

| Area | What exists | Audit status |
|---|---|---|
| Android app | `android/` Gradle application; Kotlin activity, parser, `VpnService`, Xray adapter, one parser unit-test file | Builds in GitHub CI. Not device/runtime verified. UI is programmatic Android Views, not Jetpack Compose. |
| Android VPN engine | Digest-pinned AndroidLibXrayLite AAR fetched by CI/local script; typed `CoreController`/`Libv2ray` calls and a reflective callback proxy | Generated API *shape* is recorded in CI. TUN/core lifecycle, method semantics, ABIs, traffic flow and failure handling remain unverified. |
| Connection parser | VLESS, VMess, Trojan and Shadowsocks URI/feed parsing and a narrow Xray JSON generator | Parser has tests, but no personal-profile import or persistent profile model; config compatibility with the pinned runtime has not been tested. |
| Smart Connect | Portable C scorer, state reducer, fallback planner, session coordinator, observation aggregator, and bounded probe-plan model under `shared/smart-connect/` | Pure domain logic only. It consumes supplied observations; it does not probe, connect, check server health/capacity, switch a live tunnel, or verify outcomes. |
| Browser/catalog prototype | Standard-library Python server and web UI that read the existing fixed public feed sources; separate experimental CSV ranker | Not a VPN client, not a جاویدنامان وطن server source, and not a production backend. CSV fixture data is synthetic. |
| Backend / admin | No Go module, API service, schema/migrations, Redis/PostgreSQL config, container/deployment manifests, admin UI, RBAC, or operational telemetry | Absent. No operating infrastructure or server inventory is present in this checkout. |
| iOS | Supplied App Store icon source only; no Xcode project, Swift package, Network Extension provider, app-group configuration, entitlement, signing setup or CI | Absent and blocked on Apple toolchain, entitlement, engine spike and device verification. |
| Brand | Original Lion and Sun app icon and wordmark plus Android/iOS/UI PNG derivatives in `docs/brand-assets/` | Verifier passes. Android launcher currently contains only xxxhdpi icon files; the native home screen uses a text crown/seal rather than the supplied Lion and Sun artwork. No Vazirmatn font is tracked. |
| Project/legal docs | Product, architecture, feature parity, risks, tests, compatibility, engine and license notes | Useful baseline, but several implementation-status statements predate the recent Android binding/UI work; see §9. |

There are no tracked `go.mod`, `Package.swift`, `.xcodeproj`, `Podfile`, Docker Compose, SQL schema or conventional JS package manifest files.

## 3. Current behavior — confirmed in source

### Android

- Gradle: Android Gradle Plugin `8.10.1`, Kotlin Android plugin `2.1.20`, Java/Kotlin target 17, compile/target SDK 35, `minSdk 29`, version `0.1.0`; CI requests Gradle `8.11.1` and builds a debug APK.
- Manifest requests Internet, foreground-service, special-use foreground-service and notifications. It declares a non-exported, `BIND_VPN_SERVICE` service and a launcher activity. Cleartext traffic and Android backup are disabled.
- The activity downloads only three hardcoded HTTPS feed addresses, with connection/read timeouts and a 1 MB body cap, parses supported nodes, keeps the full selected URI in process memory, and lets the user select a feed node and request Android VPN consent.
- The VPN service establishes a full-route IPv4/IPv6 `VpnService.Builder` tunnel, configures two DNS addresses, excludes the app itself, creates an Xray config and calls the pinned wrapper. It emits state broadcasts and exposes a notification stop action.
- The current home UI has a power button, server list, settings and placeholder diagnostic tiles. Speed displays dashes; the UI marks speed test, DNS leak checking and Kill Switch as unimplemented. A crown affordance currently says a “special” section is not active; that should be removed or made clearly non-commercial to satisfy the no-upsell requirement.
- No account, payment, billing client or subscription purchase/renewal implementation was found in the production Android source/build files. “Subscription” in current code/docs refers to configuration feeds, not paid subscriptions.

### Portable adaptive domain

The C library has deterministic scoring and data-contract primitives for quality observations, connection-state transitions, ranked fallback plans, session coordination, timestamp-aware aggregation and bounded probe scheduling. APIs are designed to be called by platform code and explicitly do not perform network I/O, timers, cancellation, OS tunnel management or authenticity checks. A ranking is only as trustworthy as measurements and eligibility supplied by a caller. No Android or iOS binding currently calls this library.

### Existing sources are not the required two source domains

The Android app and web demo contain three fixed public subscription-feed URLs. They are not identified as جاویدنامان وطن-owned, leased or authorized infrastructure, and they do not expose verified health or capacity. They are also not the user's own profile store. Keep these concepts separate: **جاویدنامان وطن registry**, **local personal profiles**, and existing public-feed catalog are distinct ownership/trust domains. The current repository/user constraints say to retain only the three already specified public sources; this audit does not add any more public feed sources or make redistribution claims about the current sources.

## 4. Critical technical findings and risks

### P0 — do not label the tunnel production-ready

1. `VpnService` passes `pfd.fd` to `startLoop(config, int)`, but the CI `javap` report establishes only that the method has a `String, int` signature. It does **not** establish the integer's meaning or show that the wrapper connects that descriptor to Xray's TUN implementation. The generated runtime/API and TUN integration must be traced and validated against the exact pinned AAR/source before relying on this call.
2. `V2SpeedVpnService` emits `CONNECTED` after `startLoop` returns. The code has no independent test request through the tunnel, traffic proof, readiness handshake, or live tunnel/core callback contract. A successful method return is not proof of working internet. Keep user-facing language conservative until an end-to-end probe succeeds.
3. The lifecycle has not been exercised for start/stop races, repeated start, permission revocation, process death, network handover, descriptor ownership, background restrictions or native-core errors. The single-thread command executor plus native call behavior needs verification to ensure stop cannot be stuck behind a blocking start.
4. Routes include both IPv4 and IPv6 defaults, and DNS addresses are configured, but DNS leak prevention, IPv6 behavior, lockdown/Kill Switch, fail-closed semantics and traffic escaping the tunnel have not been measured. No security/no-leak claim is justified.
5. Xray API inspection was added to CI after the first typed integration; callback method signatures required compatibility changes. Build success still does not establish wrapper semantics, supported ABI coverage, protocol behavior or safe runtime shutdown.

### P0 — dual-source product is not implemented

- There is no جاویدنامان وطن-owned/authorized server inventory, health agent, capacity signal, API, signature/key rotation scheme, registry client or control-plane. No managed endpoint is configured. The current public lists cannot stand in for managed service availability.
- There is no local personal-profile domain/repository or CRUD flow. UI selection comes from the public-feed list only. URL, QR, clipboard, manual, file, subscription import, duplicate handling, profile organization, export/backup, editing and deletion are absent.
- There is no user-controlled `جاویدنامان وطن Only / Personal Only / Smart Hybrid` mode, no unified source-selection policy, and no source-aware failover. The pure C fallback planner does not operate a real connection and must not cause silent ownership-source changes.

### P0 — platform and operations missing

- iOS and the `NEPacketTunnelProvider` extension do not exist. Android-only build status cannot satisfy Android+iOS acceptance.
- No backend, admin panel, health monitoring, capacity management, operational analytics, incident management, abuse controls, RBAC/audit, cost accounting or infrastructure deployment exists.
- No real جاویدنامان وطن server credentials, authorized infrastructure details, data-processing policy or test endpoints are in the repository. Operational features cannot be truthfully implemented/tested without authorized infrastructure and explicit operator policies. Credentials must never be pasted into source or chat; later access should use the approved secret-management path.

### P0 — privacy and release gates

- There is no persistent personal-profile store today. This means encrypted local storage is not yet required by existing behavior, but it is a hard prerequisite before importing/persisting user secrets. Android Keystore-backed key protection and iOS Keychain integration do not exist.
- No backend upload of profile contents exists. Preserve this local-first property. Avoid adding telemetry of profile/config data; obtain explicit consent before any optional sync or identifiable diagnostics.
- No full SBOM, native ABI inventory, reproducible release package, signing/release process, device matrix, penetration/security review or legal clearance exists.

## 5. Dependencies, licensing and data provenance

### Directly observed Android dependencies

- App: Android platform APIs; Kotlin/Android Gradle plugins.
- Runtime artifact: AndroidLibXrayLite release `v26.11.9`, source tag commit `b5a377087e74eaa34e86718710b04c50f4b767f`, AAR publisher-declared SHA-256 `cdfef146218233a31a8b52c8b1016557730d3b63fc121d4c84cacea50e669155`; declared LGPL-3.0.
- Xray-core version recorded in notices: `v1.260327.1-0.20261109052709-676eaf66f8f0`, source commit `676eaf66f8f05d4d558ee7a74b4736623d3feeb9`; MPL-2.0.
- Test-only: JUnit `4.13.2` and Robolectric `4.14.1`.
- `android/LICENSE` declares MIT for the Android project; the root has no separate repository-level LICENSE. Third-party notices and LGPL/GPL/MPL texts are present. These are preliminary notices, not proof of complete compliance.

### Open legal/data questions

- No transitive Go module, native ABI, generated asset/data or test/runtime dependency SBOM is tracked. The wrapper's exact source-to-AAR reproducibility and corresponding-source delivery must be verified before distribution. The repo already records MPL/LGPL obligations as open and does not claim legal clearance.
- Existing public feed repositories have unresolved redistribution/licensing provenance according to `docs/WEB_APP_TRIAL.md` and `README.md`. Fetching/attributing a URL does not grant the right to republish its content. The requested official جاویدنامان وطن registry should not be populated from these feeds unless a clear authorization/data policy separately permits it.
- Supplied brand originals are present, byte-preserved and pass the PNG verifier. Confirm usage/derivative rights and final design approval before app-store release; image presence alone is not an integration or clearance record.

## 6. Product-principle and UX gaps

| Requirement | Current state | Gap / implication |
|---|---|---|
| Free, no tiers/payment | No billing code found; no payment dependency | Remove/repurpose the crown “special section” affordance; keep automated checks preventing paid-upgrade UI/API from entering scope. |
| No fake metrics | Speed values are `—`, diagnostic tiles state unavailable | Good current constraint. Connection duration, measured throughput, latency and protocol are not yet supplied from live core/runtime counters. |
| No misleading connected status | UI distinguishes connecting/error and service reports unverified internet | Still not sufficient: `CONNECTED` is emitted from core method return without an actual verified end-to-end path. Tighten status model before calling the VPN production-usable. |
| Source ownership clarity | Current rows display public feed/source name | No جاویدنامان وطن vs Personal identity or policy control; existing three feeds must not be relabeled as جاویدنامان وطن. |
| Brand | Lion/Sun originals and derivative pack exist; current screen uses a generic crown/seal | Integrate supplied mark only after design/layout review; use approved Vazirmatn or document a supported alternative. |
| Persian RTL / English | Persian-first programmatic Views; no string-resource/localization system found | English localization, RTL/LTR audit, accessibility, TalkBack, scalable text and typography remain open. |
| Required screens | Home/server list/settings only at a rudimentary level | Onboarding, Smart Connect modes, personal configs, server details, adaptive explanation, security, diagnostics, real stats, support and About are absent. |

## 7. Existing test/build evidence

### Executed during this audit

- `python3 scripts/verify_brand_assets.py` — **passed**, validating 10 PNG files.
- `make -C shared/smart-connect test` — **passed**: 17 scorer, 14 state, 11 fallback planner, 18 session, 15 measurement aggregation, and 14 probe-plan tests (89 named tests total, with generated cases documented by the suite).
- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s shared/smart-connect/web-demo -p 'test_*.py' -v` — **passed**, 15 tests.
- `sh -n android/fetch-xray-core.sh` — **passed**.
- Prior CI on audited code head: Android debug APK assembly and `smart-connect-tests` both reported successful; the CI artifact exists for its configured retention period. Prior `gh run download` attempts failed with `EOF`, so no APK was retrieved into this workspace.
- `git status` was clean at the start of the audit. No Android source was compiled or executed locally in this audit because all relevant native toolchains are absent.

### Not evidenced

No physical Android or iOS device test; no measured VPN throughput, latency, jitter, loss, session duration, battery or memory; no DNS/leak/kill-switch proof; no infrastructure health/capacity proof; no backend/admin tests; no protocol compatibility matrix from live servers; no signing or store release. Do not convert unit-test counts or CI assembly into any of these claims.

## 8. Suggested target architecture (audit recommendation; not implemented)

1. **Native clients:** Kotlin + Jetpack Compose with Android `VpnService`; Swift + SwiftUI and a separate Network Extension provider on iOS. Keep UI and OS lifecycle adapters native. Create explicit capability/version contracts for protocol options that differ by core/platform.
2. **Shared domain:** retain the existing C Smart Connect primitives as a candidate shared deterministic policy module, behind stable Kotlin/Swift adapters and golden test vectors. Extend only from real measured observations; do not claim that the current scheduler or planner performs I/O. Define source ownership and connection state independently of UI.
3. **Personal source:** local-first profile repository with typed profiles, import validation, stable IDs, encrypted secrets and OS-backed key protection. Keep secrets out of requests/logs/analytics. Build export as an explicit user action. No cloud sync by default.
4. **جاویدنامان وطن control plane:** Go modular monolith with versioned REST endpoints, PostgreSQL for durable registry/operations data, and Redis only when measured caching/coordination needs justify it. Authenticated/authorized control plane; signed/versioned server catalogue; health/capacity eligibility and safe freshness/expiry behavior. Separate it from the VPN data plane. The repository currently has none of this.
5. **Server/data plane:** only independently operated or explicitly authorized nodes. Health/capacity reports must be server-agent-authenticated and freshness-bounded. Define thresholds by measured hardware/profile, availability draining and recovery. A management API cannot create the actual VPN server capacity; operators must provision and own the data plane.
6. **Admin/monitoring plane:** role-scoped dashboard/API, immutable security audit for sensitive operations, incident controls and cost reporting. Operational telemetry should be aggregate/minimal and must never contain traffic contents or personal profile secrets.
7. **Connection policy:** a user-owned explicit mode setting governs source choice. Hybrid selection only chooses among sources the user permits. Explain each connection/retry/switch. Separate failover policies for managed nodes and personal profiles; no cross-owner switch unless the user explicitly enabled Hybrid.

This is a proposal for the next architecture decision; it is not a commitment to a database schema, API contract, cloud vendor or operator policy until product decisions and infrastructure evidence are available.

## 9. Existing documentation drift

The following files should be reconciled as part of the next documentation/release-hardening pass; their claims are stale or incomplete relative to the current source:

- `README.md` still calls the first Android CI build pending even though a later CI build passed; its overview also predates the latest main-screen redesign.
- `android/README.md`, `docs/ANDROID_NATIVE_MVP.md`, `docs/PLATFORM_COMPATIBILITY.md`, and `docs/FEATURE_PARITY_MATRIX.md` describe a reflective adapter or say no native app was implemented. The adapter now imports the generated binding types directly but uses a reflective callback proxy; the app/service now exist. Device/runtime verification remains absent as stated.
- Recent status text must not be “corrected” to imply the tunnel works: only the generated API surface and APK compilation have been observed.

## 10. Prioritized implementation gates and acceptance criteria

### Gate A — Android truth and lifecycle vertical slice (before personal profiles or polished connection claims)

- Trace exact pinned wrapper source and prove `startLoop` argument/TUN ownership semantics; verify native library ABIs and corresponding source.
- Connect one controlled, authorized test node on a real Android 10+ device; pass a deterministic in-tunnel DNS/HTTPS probe and verify observed public egress/expected routing.
- Verify stop, repeated start/stop, revoke, process death, network changes, IPv4/IPv6 and notification lifecycle. State transitions must be based on a real readiness probe/core callbacks, and stop must release the descriptor/core.
- Add instrumentation tests/fake core adapter tests without using fakes as a substitute for the device acceptance test.

### Gate B — source policy and personal profiles

- Specify typed domain contracts for `جاویدنامان وطن`, `Personal`, `SmartHybrid`, source ownership, connection state, protocol capabilities, error category and measured observations.
- Implement local-only profile CRUD/import validation for user-selected formats; test malformed input, duplicate IDs, credential redaction, size bounds, encrypted persistence, migration/restore/delete and safe explicit export.
- Add source/mode UI and source-aware retry behavior. Smart Hybrid can use only currently eligible sources and must disclose each source transition.
- Acceptance: profile content never appears in logs, normal registry API requests or telemetry; persistence tests verify confidentiality controls and migration rollback.

### Gate C — جاویدنامان وطن control/data/monitoring planes

- Obtain confirmed authority and a real inventory of managed/leased servers, protocol capabilities, regions, capacity signals, monitoring agents and operator contacts. Do not substitute internet subscription feeds.
- Produce threat model, API/auth/signature/rotation design, health freshness, capacity thresholds and incident/fair-use policies before endpoint development.
- Implement registry/health/capacity API and authenticated agents; test invalid/stale/tampered reports, overloaded/offline eligibility, drain/recovery, rate limits, RBAC and audit.
- Acceptance: only verified healthy/available candidates are offered; a down/over-capacity server is not selectable; existing sessions drain gracefully; no claimed scale/capacity exceeds measured fleet data.

### Gate D — Adaptive, security, iOS and release

- Wire real latency/jitter/loss/throughput/session outcomes into explainable conservative scoring and anti-flap/hysteresis. Test with controlled scenarios, actual device/server benchmarks and failure injection. No private traffic content used.
- Implement and test DNS protection/Kill Switch only where platform semantics support verifiable fail-closed behavior. Do not expose a working toggle before it is operational.
- Build iOS app and provider under valid Network Extension entitlement, with real core compatibility, signed distribution and physical-device lifecycle tests.
- Complete SBOM/license review, privacy policy, accessibility/localization, release signing, reproducible build/release process, security audit, incident process and operational cost/budget monitoring.

## 11. Blockers / human or operational decisions

1. Confirm the legal entity/authority that operates جاویدنامان وطن-managed servers and arrange secure, authorized infrastructure access (never share passwords/tokens in chat or commit them).
2. Approve what “official servers” means: personally owned vs leased vs explicitly authorized, supported locations/protocols and target availability/service policy.
3. Agree which personal configuration formats/protocols are in scope for the first import release; the current parser supports only a narrow subset and has no profile lifecycle.
4. Obtain legal review for exact AndroidLibXrayLite/Xray distribution and the three public-feed data sources; repository license texts are not legal clearance.
5. Provide Apple Developer team/Network Extension entitlement and a macOS/Xcode CI/device path before promising iOS.
6. Agree privacy/retention, opt-in telemetry, abuse response and fair-use/capacity policy before collecting operational/user data.

No secrets are requested here. Any later access should be provisioned through the project's authorized secret manager/infrastructure integration.

## 12. Phase 0 exit decision

**Exit status: PASS — repository audit/report produced.**
**Production readiness: FAIL / NOT ASSESSED FOR RELEASE.** This is expected at Phase 0, not a defect in the audit. The next safe step is an architecture decision record translating the product into explicit source/profile/server contracts and the Android tunnel vertical-slice acceptance tests. Do not build a fictional managed-server catalog, claim a working production VPN, or begin a paid-subscription subsystem; the latter is expressly out of scope.

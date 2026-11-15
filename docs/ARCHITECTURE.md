# V2Speed Architecture (target; initial shared scoring core implemented)

## Context and trust boundaries

```text
Android Compose / iOS SwiftUI
            │ platform-neutral user actions + state
            ▼
Profile & policy domain ── Smart Connect / deterministic scoring
            │ validated immutable engine request
            ▼
Platform Connection Coordinator ── serialized state machine / retry policy
            │ stable adapter contract
            ├── Android VpnService + selected core adapter + tun2socks if needed
            └── iOS Packet Tunnel Provider + selected core adapter

Local encrypted profile store / OS secure key store
No control-plane backend is required for local profiles or a tunnel.
```

## Module boundaries

1. **Presentation:** native navigation, accessible simple/advanced surfaces, localization, and view state only. It cannot declare a tunnel connected without coordinator confirmation.
2. **Domain:** profile IDs and validated protocol-neutral envelope, import/export contracts, routing policy model, state machine, retry/anti-flap policy, metrics model. The deterministic scorer, generation-safe state reducer, bounded fallback planner, session coordinator, timestamp-aware observation aggregator and bounded probe scheduling plan and scheduler-result-to-observation bridge are implemented in `shared/smart-connect/` as portable C with a bounded C ABI and no platform/network I/O. The scheduler returns timeout/cancellation decisions but does not stop real tasks; the bridge maps failed/time-out states to no-metric failure attempts and trusts caller measurements for successes; aggregation trusts caller-provided observations. Both native clients can consume this domain logic, while real probes, connection verification and platform state transitions still require adapters and tests.
3. **Storage:** local profile metadata and encrypted secrets, atomic profile/relationship updates, versioned migrations. Keystore/Keychain protects encryption key material; stored configuration never logs.
4. **Connection coordinator:** one serialized connection operation, cancellation ownership, state reconciliation after process restart, network-change response, structured errors and bounded retries.
5. **Engine adapter:** translates validated profile/policy into the selected core's config, starts/stops the core and exposes readiness, stats and diagnostics. Strict capability report per platform/protocol. No UI dependency.
6. **Platform tunnel:** Android `VpnService`, notifications, network callbacks and file descriptor lifecycle; iOS `NETunnelProviderManager` / `NEPacketTunnelProvider`, app-group handoff, entitlement and extension constraints.
7. **Backend (future, optional):** authentication/subscriptions/server registry only if justified; segregated from VPN data plane. No database/cache selected now.

## Connection-state contract

`Disconnected → Fetching → Benchmarking → Connecting → Connected`. Any active phase can go to `Error` or `Disconnected`; `Connected` can transition to `Reconnecting` or `Switching`, but the coordinator serializes operations and rejects/replaces stale request IDs. A state is committed only with platform/core acknowledgement. Every operation carries a generation ID; late callbacks from prior generations cannot overwrite current state. Cancellation is a distinct outcome, not success. Retry policy is capped and observable.

## Smart Connect contract

- Inputs are candidate IDs and validated engine capability, measurement timestamps, measurement confidence, network fingerprint scope (privacy-preserving and short-lived), and success history.
- Probe concurrency, bytes, time and cancellation are explicit.
- Output includes chosen profile ID, per-feature evidence/normalized values, score, missing data, and rationale. No credentials in result/logs.
- A ranking is not a connection. The coordinator must establish and verify the selected tunnel, then fallback to the next ranked eligible candidate subject to policy.
- Successful selection and failure history update only after observed outcomes. Avoid machine-learning claims; first model is deterministic.

## Data and privacy

- Default profile storage remains on device. Secrets use platform key stores and are never included in diagnostics.
- No analytics/telemetry until data inventory, purpose, retention, opt-in, jurisdiction and user-facing notice are approved.
- Connection history and raw logs have bounded, configurable retention and redaction. Export requires explicit user action and review.
- Subscription refresh sends a request to the URL chosen by the user. Treat response as untrusted input; enforce TLS, timeouts, byte limits and redirect policy.
- No backend/data-plane coupling; no Redis/PostgreSQL planned.

## Failure discipline

All asynchronous operations have an owner and cancellation path. Every failure has typed category, safe message, recovery action, and structured redacted diagnostic. Do not swallow cancellation, convert failure to connected, or allow stale state callbacks. Resource cleanup is tested on every exit path.

## Build/test boundary

Keep domain and ranking tests deterministic with fake clocks, measurements and engine adapters. Native platform tests cover permission/lifecycle and real tunnel integration separately. Shared JSON test vectors verify Kotlin/Swift scoring and state semantics. Release APK/IPA, signing, entitlements and device tunnel tests are separate acceptance evidence.

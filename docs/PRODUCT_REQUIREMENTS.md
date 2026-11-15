# V2Speed 1.0 — Product Requirements (Milestone 1 draft)

**Status:** requirements baseline; not a claim of implementation or release readiness.
**Product:** V2Speed — Smart VPN
**Positioning:** “One Tap. Smart Connect. Best Speed.”
**Platforms:** Android and iOS, native user experience; availability depends on platform entitlements and verified engine support.

## Product goal

Deliver a consumer-simple VPN client with an advanced mode that retains meaningful configuration, routing, diagnostics, and lifecycle controls. Both modes must use the same production tunnel and configuration path. A disconnected UI, fabricated speed/ping, or simulated tunnel is not acceptable as a substitute for a working VPN.

## Personas and jobs

- **Everyday user:** connect with one action, know whether the tunnel is genuinely active, see the selected server and recover from a failure without reading protocol jargon.
- **Power user:** import, inspect, edit, group, test, route, back up, and diagnose supported profiles.
- **Privacy-conscious user:** keep configuration and secrets on-device unless a separately justified, consented account service is introduced; understand any data collection and retention.
- **Support engineer (future):** receive explicitly user-exported, redacted diagnostic bundles. No automatic upload of traffic, credentials, or full profiles.

## Experience requirements

### Simple mode

- Smart Connect is the primary action. It selects only from valid candidates using recent, measured, explainable evidence; it then establishes a real tunnel and verifies its state.
- Display explicit lifecycle states: disconnected, fetching, benchmarking, connecting, connected, reconnecting, switching, and error. Only show progress backed by actual work.
- Connected metrics originate from the tunnel/core counters and measured latency; absent data is “—”, never a fabricated zero or estimate presented as actual.
- Show selected server, connection duration, and understandable retry/recovery action.

### Advanced mode

- Manual configuration/server selection, supported protocol-specific fields, profile validation, routing, DNS, per-app routing where platform permits, import/export, subscription refresh, logs, statistics, and diagnostics.
- Unsupported fields/formats fail closed with actionable error information; no “best effort” conversion that silently changes security or routing semantics.
- The advanced layer and simple layer share the same profile store, connection state machine, tunnel engine, and policy validation.

## Functional acceptance criteria

1. Import supported configuration text/file/QR; validate and show what will be stored before committing. Duplicate detection must use canonical normalized data without leaking secrets.
2. Add/edit/delete/duplicate/group/favorite/reorder profiles. Persist with stable identifiers and handle interrupted writes atomically.
3. Refresh supported subscription URLs with bounded timeouts, TLS verification, size limits, safe redirects, and failure-preserving behavior (keep prior valid profiles when refresh fails).
4. Benchmark with bounded concurrency, cancellation, deadlines, protocol-appropriate reachability and a tunnel throughput probe; ICMP alone is not sufficient.
5. Smart Connect ranks only measured or explicitly uncertainty-adjusted candidates, attempts a real connection, verifies tunnel establishment, falls back under a retry cap, and prevents flapping.
6. Android implements permission consent, `VpnService`, foreground notification and lifecycle cleanup. iOS implements `NetworkExtension` / `NEPacketTunnelProvider`, provider configuration and approved entitlements.
7. Routing/DNS behavior has controlled tests for IPv4/IPv6, DNS paths and network transitions; platform coverage gaps are documented rather than claimed away.
8. All diagnostics are structured and redact credentials, private keys, subscription tokens, and raw profile URIs.
9. English and Persian, RTL/LTR, system text sizing, keyboard/screen-reader semantics, reduced-motion behavior, and minimum platform touch sizes are verified on each native UI.

## Smart Connect starting policy

Initial evidence-backed score (weights are a testable starting point, not learned/model-based): throughput 30%, latency 25%, reliability 20%, stability 15%, recent success 10%. Normalize by cohort and window; represent missing evidence explicitly; do not assign a perfect score to an unmeasured candidate. Cap probes and retries. Weights and normalization must be calibrated on a controlled server set before being called optimal.

## Non-functional requirements

- Secure secret storage using platform capabilities; no plaintext secrets in logs, crash reports, exports by default, or telemetry.
- TLS validation is on by default. Any exception is explicit, reviewable, scoped, and never silently enabled.
- No backend dependency for local profile management or local tunnel use. Authentication, device sync, server registry or paid subscription only after a concrete product need and privacy/legal review.
- Performance is measured on named devices/networks with at least three comparable runs; UI progress must reflect actual operations.
- Accessibility and localization are launch criteria, not post-release polish.

## Out of scope for the first local foundation

- A VPN server fleet, billing/account service, admin console, analytics collection, or Redis/PostgreSQL until requirements establish a need.
- Claiming anonymity, guaranteed leak prevention, “AI” server selection, release readiness, iOS support, or complete v2rayNG parity without the corresponding verification evidence.

## Open acceptance gates

- Confirm intended distribution/license model with counsel before core or source reuse.
- Complete transitive dependency license review and iOS engine feasibility experiment.
- Confirm developer accounts, Apple Network Extension entitlement and test devices.
- Select exact supported protocol subset per platform/core version.
- Set numeric startup, connect-time, battery and throughput targets after establishing physical-device baselines.

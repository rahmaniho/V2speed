# V2Speed Milestone 1 — Risk Register

| ID | Risk | Severity | Evidence / state | Mitigation / exit condition |
|---|---|---|---|---|
| R1 | GPL-3.0 reference code conflicts with proprietary distribution intent | Critical | Reference LICENSE is GPL-3.0; distribution intent not specified | Do not copy/adapt; get counsel's written compatibility decision and clean-room boundary before reuse |
| R2 | Core/wrapper/transitive dependency licenses incomplete | Critical | Xray MPL-2.0, Android wrapper LGPL-3.0 and HEV MIT observed; full SBOM absent | Pin candidate; audit all transitive/native/data components and release artifacts before shipping |
| R3 | No iOS VPN engine validated | Critical | Reference is Android-only; no Xcode/entitlement available | Spike iOS NetworkExtension with actual core and Apple entitlement/device before promising feature parity |
| R4 | Platform toolchains unavailable | High | JDK, Gradle executable, Go, Android SDK/adb and Xcode absent in workspace | Provision CI and physical devices; run clean native builds before code validation claims |
| R5 | Protocol parser ≠ runtime protocol support | High | Reference contains multiple parsers and recent formats; runtime depends on versioned core | Capability matrix per OS/core/transport, fixtures and real integration tests; reject unsupported configs |
| R6 | VPN state, lifecycle and networking complexity | Critical | Must coordinate OS/provider, engine, UI and network changes | Typed serialized coordinator, generation IDs, fake engine tests and real-device fault scenarios |
| R7 | DNS/IPv6 leaks or policy mismatch | Critical | Reference config includes DNS, fake DNS, routes; no leak proof | Controlled v4/v6/DNS testing and independent privacy review; never guarantee zero leaks without evidence |
| R8 | Smart Connect burdens servers/drains battery or flaps | High | User requests parallel measured probes and adaptive ranking | Concurrency/bytes/deadlines/TTL/retry caps, cancellation, anti-flap threshold, monitored physical baseline |
| R9 | Subscription URL attacks or destructive updates | High | Profiles are remote untrusted content | TLS, redirect/size/timeout limits, parser sandbox, atomic update/rollback, no secret logs |
| R10 | Brand imagery unsuitable as launcher/icon or dark UI background | Medium | Supplied app icon is 1254x1254 RGB; wordmark 1536x1024 RGBA; raw originals retained | Create non-destructive platform-specific derivatives with safe areas; verify on devices; preserve source files |
| R11 | Product backend becomes unnecessary privacy burden | Medium | No justified account/device service requirement identified | Keep local-only by default; add server only with product need, minimization, retention and security review |
| R12 | Persian/RTL and accessibility omitted by late redesign | High | Both are launch requirements, no UI yet | Add English/Persian strings and semantic/keyboard/focus checks from first UI implementation |
| R13 | Audit misses version drift in live upstream | Medium | Audit pinned to current snapshot dates/SHAs | Re-audit and record exact immutable upstream release when implementation starts |
| R14 | Shared C scorer integration diverges or fails in Kotlin/Swift bindings | Medium | Standalone scorer is GCC-tested; no Android NDK, JNI, SwiftPM or Xcode build exists yet | Keep the ABI small/stable; add Android and iOS binding contract tests and compile the same source for each target before app integration |

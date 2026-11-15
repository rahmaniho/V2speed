# Platform Compatibility Matrix (provisional)

No supported version is declared until an engine is selected and actual builds/device tests exist.

| Capability | Android target | iOS target | Current status / required proof |
|---|---|---|---|
| UI | Kotlin + Jetpack Compose, native | Swift + SwiftUI, native | Target decision only; source/build absent |
| VPN API | `VpnService`; user consent; foreground service notification; network callbacks; correct fd teardown | `NetworkExtension` with `NEPacketTunnelProvider` and `NETunnelProviderManager`; app-group/provider configuration | Not implemented; must test permission denial, active interruption, restart, stop and network transition on devices |
| Engine | Adapter around a pinned, approved mobile core; candidate Xray-core, no final selection | Same protocol contract with independently verified iOS-compatible core adapter | Unselected; Android AAR and Go wrapper are not ported evidence |
| Configuration | Capability-gated, versioned config model; Android-specific per-app include/exclude if adopted | Capability-gated settings and provider-safe delivery | Not implemented; app-specific routing semantics differ by platform |
| DNS/IP | Explicit IPv4/IPv6 routes, DNS servers, leak behavior tests | Packet tunnel IPv4/IPv6/DNS settings; platform routing limitations | Not implemented; no no-leak claim |
| Background operation | Foreground-service and platform policy restrictions; notifications | Packet tunnel extension lifecycle and Apple-managed constraints | Not implemented; Android behavior cannot be assumed to translate |
| Secure storage | Android Keystore-backed key wrapping/protection, app-private storage | Keychain and appropriately scoped access group | Not implemented; hardware-backed protection availability varies |
| Minimum OS / SDK | TBD after engine/SDK support study; user-requested v2rayNG ref minimum API 24 is not inherited | TBD after Xcode, Network Extension and core support review | Unset, not release ready |
| Distribution | Android signing, Play/F-Droid policy and native ABI/package matrix | Apple Developer account, Network Extension entitlement, provisioning/signing | Not verified; no Android SDK/JDK or Xcode in this environment |
| Locale/accessibility | Persian + English, RTL/LTR, TalkBack, dynamic text | Persian + English, RTL/LTR, VoiceOver, Dynamic Type | Requirements captured; no UI or accessibility audit yet |

## Required per-platform verification

- Exact min/target OS, CPU architectures, core version and supported protocol/options.
- Debug/release build; signing and package contents; complete SBOM and license report.
- Permission denied, accepted, revoked, app/process restart while tunnel active, explicit disconnect, OS stop, low-memory/resource cleanup, Wi-Fi↔cellular transition, no-network and captive portal scenarios.
- Real device routes and DNS on IPv4-only, IPv6-only and dual-stack test networks; no “verified” status based solely on compilation or emulator UI.
- Android per-app VPN and iOS app-level behavior documented separately; no false cross-platform parity.
- iOS entitlement approved and signed archive inspected before describing iOS as release ready.

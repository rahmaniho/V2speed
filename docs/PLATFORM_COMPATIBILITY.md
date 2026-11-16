# Platform Compatibility Matrix (provisional)

Product target chosen as Android 10+ (API 29), but no Android release compatibility is claimed until engine/build validation and physical-device tests exist.

| Capability | Android target | iOS target | Current status / required proof |
|---|---|---|---|
| UI | Native Kotlin Views prototype, Android 10+ / API 29+ for MVP | Swift + SwiftUI, native | User selected Android 10+ and open-source distribution; first screen is Persian-first, not yet accessibility/localization audited |
| VPN API | `VpnService`; Android consent; foreground service and stop action; dual-stack TUN setup | `NetworkExtension` with `NEPacketTunnelProvider` and `NETunnelProviderManager`; app-group/provider configuration | Android spike added but not built/run here; permission denial, interruption, lifecycle, descriptor cleanup and network transitions need device tests |
| Engine | Reflective adapter spike around pinned AndroidLibXrayLite v26.11.9 / Xray AAR | Same protocol contract with independently verified iOS-compatible core adapter | Android candidate only; actual binding invocation, release provenance, license/SBOM and ABI package inspection remain open |
| Configuration | In-memory VLESS/VMess/Trojan/Shadowsocks-to-Xray converter prototype; own app excluded from its tunnel | Capability-gated settings and provider-safe delivery | Android parser/config mapping is not yet validated against a running core; app-specific routing semantics differ by platform |
| DNS/IP | Explicit IPv4/IPv6 routes, DNS servers, leak behavior tests | Packet tunnel IPv4/IPv6/DNS settings; platform routing limitations | Not implemented; no no-leak claim |
| Background operation | Foreground-service and platform policy restrictions; notifications | Packet tunnel extension lifecycle and Apple-managed constraints | Not implemented; Android behavior cannot be assumed to translate |
| Secure storage | Android Keystore-backed key wrapping/protection, app-private storage | Keychain and appropriately scoped access group | Not implemented; hardware-backed protection availability varies |
| Minimum OS / SDK | MVP decision: Android 10+ (API 29), per user. Not a promise for all Android versions; selected engine may support older API 24 but this product target is 29+ | TBD after Xcode, Network Extension and core support review | Android target decided; native build and device verification absent |
| Distribution | Open-source debug APK via CI; universal ABI target only if the pinned core contains each ABI; Play/F-Droid policy and release signing still require review | Apple Developer account, Network Extension entitlement, provisioning/signing | Android CI build pending; this sandbox has no Android SDK/JDK and no release key |
| Locale/accessibility | Persian + English, RTL/LTR, TalkBack, dynamic text | Persian + English, RTL/LTR, VoiceOver, Dynamic Type | Requirements captured; no UI or accessibility audit yet |

## Required per-platform verification

- Exact min/target OS, CPU architectures, core version and supported protocol/options.
- Debug/release build; signing and package contents; complete SBOM and license report.
- Permission denied, accepted, revoked, app/process restart while tunnel active, explicit disconnect, OS stop, low-memory/resource cleanup, Wi-Fi↔cellular transition, no-network and captive portal scenarios.
- Real device routes and DNS on IPv4-only, IPv6-only and dual-stack test networks; no “verified” status based solely on compilation or emulator UI.
- Android per-app VPN and iOS app-level behavior documented separately; no false cross-platform parity.
- iOS entitlement approved and signed archive inspected before describing iOS as release ready.

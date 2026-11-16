# Native Android VPN MVP — agreed scope and gates

## Product decisions

- **Platform:** native Android app, minimum Android 10 / API 29 for the first release. “All Android versions” is not a technically supportable promise; OEM background restrictions and core ABI support also vary. The minimum can be reconsidered only after device tests.
- **Distribution model:** open-source, with upstream copyright/license notices and any required corresponding source. No proprietary-only claim.
- **Product surface:** original Persian-first UI informed by the simplicity of SpeedTop/v2rayNG; do not copy their branding, assets, or GPL application code.
- **Feed sources:** only the three already configured subscription sources in `server.py`. Treat every URI as untrusted third-party data; no automatic connection without a user-selected node and Android VPN consent.
- **Candidate core:** Xray via the AndroidLibXrayLite Go-Mobile binding. This is a candidate for an independently implemented V2Speed client, not yet a shipping selection. The observed licenses are LGPL-3.0 for the Android wrapper and MPL-2.0 for Xray-core; exact release provenance, full dependency/license inventory, ABI contents, and distribution obligations still need verification.

## Functional acceptance criteria

1. A native Android project produces a debug-installable universal APK (arm64-v8a, armeabi-v7a, x86_64 and x86 only where the pinned core supplies those ABIs) with minSdk 29. No APK is considered complete until an Android CI build and artifact inspection pass.
2. The app fetches only the three configured feeds over TLS with bounded response size/time, safely parses supported links, and never logs full credentials. It handles malformed/unsupported entries without crashing.
3. The user chooses one node and explicitly starts the VPN. Android's `VpnService.prepare()` consent is required. A foreground notification provides a working stop action.
4. `Disconnected`, `Connecting`, `Connected`, `Reconnecting`, and `Error` states reflect actual service/core callbacks. The UI must never say connected until the core and tunnel have acknowledged startup.
5. Stop, permission denial, invalid config, core failure, process death, network switch, and repeated start/stop have deterministic cleanup; no stale TUN descriptor or false connected state.
6. Tunnel routing, IPv4/IPv6 policy, DNS behavior, and whether apps outside the tunnel can escape are documented and tested on physical Android 10+ devices. No no-leak claim before those tests.
7. Source and binary notices, pinned versions, SBOM, APK SHA-256, and reproducible build instructions accompany a release candidate.
8. “Fastest”, “most stable”, and speed results are not claimed until a real, repeatable benchmark across representative devices and networks exists. A working VPN client also cannot make third-party public nodes reliable or safe.

## Current status

A first native Gradle module, feed parser, Android `VpnService`, and digest-checked Xray AAR fetch script have now been added. This sandbox still has neither Java nor Android SDK, so only the GitHub Actions build can validate compilation and produce a debug APK. The gomobile API is called through a narrow reflective adapter and has not yet been verified against a running Android device. The current code is an engineering spike, not a tested release: tunnel startup, parser coverage, IPv4/IPv6 routing, DNS behavior, network transitions, stop/revoke cleanup, and the exact native ABI set still require CI and physical-device evidence. Do not describe the current spike as a verified high-speed/stable VPN or distribute it as a release.

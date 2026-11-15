# V2Speed — Smart VPN

**One Tap. Smart Connect. Best Speed.**

V2Speed is a greenfield Android + iOS VPN client intended to combine a simple one-tap experience with an advanced configuration layer. A connected state or speed metric must always come from a real, verified tunnel/core; a mock interface is not an implementation.

## Current delivery status

Milestone 1 source audits are complete; a small **Milestone 2 domain foundation** is now implemented and tested. The Android/iOS application, VPN engine, server fleet and backend do not yet exist. No feature parity, tunnel connectivity, Android/iOS build, signing, device test, or release readiness is claimed.

Milestone 1 outputs and current implementation:

- [Milestone 1 audit and verification report](docs/MILESTONE_1_REPORT.md)
- [Smart Connect scoring core and verification record](docs/SMART_CONNECT_ENGINE.md)
- [Product requirements](docs/PRODUCT_REQUIREMENTS.md)
- [v2rayNG feature parity matrix](docs/FEATURE_PARITY_MATRIX.md)
- [License compliance gate](docs/LICENSE_COMPLIANCE.md)
- [Skill resource audit](docs/SKILL_RESOURCE_AUDIT.md)
- [ADR-001](docs/ADR-001.md) and [architecture](docs/ARCHITECTURE.md)
- [Platform compatibility](docs/PLATFORM_COMPATIBILITY.md) and [engine selection](docs/ENGINE_SELECTION.md)
- [Test strategy](docs/TEST_STRATEGY.md), [silent-failure review](docs/SILENT_FAILURE_REVIEW.md), and [risk register](docs/RISK_REGISTER.md)
- [Supplied V2Speed brand asset pack](docs/brand-assets/README.md)

## Brand assets

The two images supplied by the user are preserved as the original Lion and Sun app icon and internal wordmark under `docs/brand-assets/source/`; a proportional derivative pack is provided for design/build integration. Run `python3 scripts/verify_brand_assets.py` to verify PNG signatures, dimensions and source file sizes. The same check is wired into `.github/workflows/brand-assets.yml`. These assets are **not yet integrated into native launcher/splash/home screens** because no platform app projects exist.

## Engineering and legal gates

- Do not copy or adapt v2rayNG code before legal review: its repository is GPL-3.0. The inspected Android wrapper, Xray core, HEV tunnel and v2fly core have distinct license terms; see the report. No reference source was copied.
- Xray-core is a candidate, not a selected or cleared production engine. iOS compatibility, transitive licenses, protocol parity and real-device performance are unverified.
- Deterministic Smart Connect scoring, a generation-safe state reducer, a bounded ranked fallback planner, a session coordinator, and a timestamp-aware aggregator for caller-supplied probe observations are implemented and tested in `shared/smart-connect/`; run `make -C shared/smart-connect test`. The coordinator only accepts caller-reported verified-tunnel outcomes; real probes, tunnel establishment/verification and engine-driven fallback are still unimplemented. No ICMP-only ranking or invented metrics.
- Local configuration management must not require a backend. No database/cache is selected.

## Tooling / verification limitations

At this checkout, GCC, Python 3 and Node.js are available; GCC was used for the portable domain core. Java/JDK, Gradle executable, Kotlin compiler, Android SDK/ADB, Go, Xcode/Swift and Apple Network Extension entitlements are not available. Therefore native client build/runtime verification cannot be performed here. The exact missing test evidence is recorded rather than presented as complete.

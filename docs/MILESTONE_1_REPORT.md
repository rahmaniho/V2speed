# Milestone 1 — Audit and Feasibility Report

**Date:** 2026-11-14 UTC
**Branch:** `arena/4039209b-v2speed`
**Milestone 1 result:** source-level reference and skill audits completed; legal/technical feasibility gates remain open. Follow-on implementation has begun with a tested portable scoring primitive and CI scaffolding. No mobile app, VPN engine or platform build is claimed.

## Work completed

1. Inspected the upstream repositories at pinned snapshots:
   - `2dust/v2rayNG`, `141059d98402bb353400a42fce9d2ec8447cd70f`.
   - `AndroidLibXrayLite`, submodule `b5a377087e74eaa34e86718710b04c50f4b767f`.
   - `hev-socks5-tunnel`, submodule `c5584feca8f6df70ee53eb2879e034b849a3d15a`.
   - `rahmaniho/Skill`, `d712250d6b2378f3d32371fda2177518b8f7c9d9`.
2. Read v2rayNG README, root and scoped `AGENTS.md` instructions, `CR.md`, `LICENSE`, `.gitmodules`, Android Gradle configuration/catalog, Android manifest, build/release workflows, test inventory, and relevant Kotlin source including profile format handlers, `CoreVpnService`, core config generation, profile/subscription persistence, subscription updates, speed tests, routing/DNS, stats, backup/import/export and UI settings.
3. Initialized the reference repository's pinned submodules for inspection. Read Android wrapper source/module declaration and direct license files. Independently inspected direct Xray-core and v2fly-core repository licenses; this does not replace a dependency-wide license scan.
4. Inspected Skill repository README, `.agents/`, `.claude/`, `agent/skills/`, lock file, docs, tests and workflows. No V2Speed-relevant reusable skill was imported.
5. Created the feature matrix, PRD, license report, skill audit, ADR, architecture outline, platform compatibility matrix, engine assessment, test strategy and risk register.
6. Preserved both actual supplied V2Speed images byte-for-byte, created proportional brand derivatives without cropping or erasing artwork, and added a standard-library asset verifier.
7. Started the foundation/CI work with a GitHub Actions workflow that runs the brand-asset validation and verifier syntax check on relevant changes.
8. Began the shared mobile-domain foundation with a portable C scoring library and generation-safe connection-state reducer, strict input/state validation, stable ranking/tie handling, 17 scorer tests (including 1,000 generated batches), 14 reducer tests (including 10,000 generated events), 11 fallback-planner tests (including 250 generated plans), a 10-test session coordinator suite, and an 8-test bounded probe-observation aggregation suite, a `make` runner, and a dedicated GitHub Actions workflow. The coordinator snapshots ranked IDs and serializes domain attempt decisions, but these components perform no networking and do not prove a tunnel exists.

## Audit conclusions

- The reference is an Android client. Its code and README describe profile import/protocol parsing, Android VPN/proxy/root modes, core config generation, subscriptions, routing/DNS/assets, per-app proxy, connection tests, statistics, logs, backups and settings. See `FEATURE_PARITY_MATRIX.md` for implementation file paths and verification status by feature.
- Top-level observed licenses are not one uniform license: v2rayNG GPL-3.0; AndroidLibXrayLite LGPL-3.0; Xray-core MPL-2.0; HEV tunnel MIT; v2fly-core MIT. Transitive module/native/data licenses, shipped AAR notices and combined-work implications are not fully audited. No source code was copied. Commercial/proprietary release remains blocked on counsel and a complete SBOM/license review.
- The Skill repository contains only Google Maps scraping application guidance, which is unrelated. None of the requested named agent commands or engineering skills was available as executable tooling; none is falsely reported as run.
- Proposed product architecture uses native Android/iOS UI and tunnel APIs with an engine-isolated domain. Xray-core is only a candidate. The iOS core/Network Extension path is unproven and blocks parity or iOS release claims.
- No backend is justified at this stage. Local profile management/tunnel use should not depend on authentication, a database, cache, or management API.

## Actual verification performed

- `python3 scripts/verify_brand_assets.py` — **passed**; 8 source/derivative PNGs validated for signature, dimensions, color type and original source byte sizes.
- `python3 -m py_compile scripts/verify_brand_assets.py` — **passed**.
- `make -C shared/smart-connect test` — **passed**: 17 scorer, 14 connection-state, 11 fallback-planner, 10 session-coordinator and 8 measurement-aggregation tests.
- All five C test executables compiled with `-Wall -Wextra -Werror -pedantic -fsanitize=address,undefined`; all tests **passed** with leak detection enabled.
- All five production C sources compiled with GCC `-fanalyzer -Wall -Wextra -Werror -pedantic` — **passed**.
- GCC/gcov scoring coverage — **100% source lines and branches executed**, **96.04% branch outcomes taken at least once**. Connection reducer — **100% source lines and branches executed (67/67 lines, 56/56 branches)**, **100% branch outcomes taken at least once**. Fallback planner — **100% source lines and branches (53/53 lines, 58/58 branches)**, **91.38% branch outcomes taken at least once**. Coverage files were temporary.
- Both `.github/workflows/*.yml` workflows — parsed locally. GitHub Actions passed both jobs (`smart-connect-tests`, `verify-brand-assets`) on PR head `b5b797b`, including the measurement-aggregator change.
- CMake 4.4.5 configure/build and CTest — **passed** all five domain suites; the tool was installed temporarily under `/tmp`.
- GCC/gcov measurement aggregator coverage — **98.80% of 83 lines**, **100.00% of branches executed**, and **90.91% branch outcomes taken at least once**.
- GCC/gcov session coordinator coverage — **86.84% of 114 lines**, **98.06% of branches executed**, and **66.99% branch outcomes taken at least once**. Prior scorer/reducer/planner coverage figures are recorded separately.
- `git diff --check` — **passed**.
- Android/iOS app builds, real benchmark/tunnel scenarios, signing, and iOS entitlement checks — **Not run**.

## Remaining Milestone 1 gates

- Legal counsel decision on GPL/LGPL/MPL distribution and intended business model.
- Complete dependency, native binary, AAR, GeoIP/GeoSite and data provenance license inventory.
- Confirm protocol requirements and whether a single core can support both Android and iOS at acceptable feature/performance levels.
- Obtain Android SDK/JDK/NDK build environment and Apple developer account, approved Network Extension entitlement and macOS/Xcode device runner.

## Smallest unblocker for Milestone 2

Provision CI/engineering hosts with JDK 21, Android SDK/Build Tools and NDK; Go toolchain matching the candidate core; an iOS macOS/Xcode runner; and an Apple Developer account with Network Extension entitlement. First implement/build a minimal native tunnel vertical slice on Android and an entitled iOS provider spike. Before core integration or distribution, obtain written legal approval of the selected component and source strategy. The current workspace does not include those toolchains or entitlements, so a simulated native success would be misleading.

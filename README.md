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
- [First browser prototype and subscription-source review](docs/WEB_APP_TRIAL.md)

## Brand assets

The two images supplied by the user are preserved as the original Lion and Sun app icon and internal wordmark under `docs/brand-assets/source/`; a proportional derivative pack is provided for design/build integration. Run `python3 scripts/verify_brand_assets.py` to verify PNG signatures, dimensions and source file sizes. The same check is wired into `.github/workflows/brand-assets.yml`. These assets are **not yet integrated into native launcher/splash/home screens** because no platform app projects exist.

## Engineering and legal gates

- Do not copy or adapt v2rayNG code before legal review: its repository is GPL-3.0. The inspected Android wrapper, Xray core, HEV tunnel and v2fly core have distinct license terms; see the report. No reference source was copied.
- Xray-core is a candidate, not a selected or cleared production engine. iOS compatibility, transitive licenses, protocol parity and real-device performance are unverified.
- Deterministic Smart Connect scoring, a generation-safe state reducer, a bounded ranked fallback planner, a session coordinator, timestamp-aware observation aggregation, a bounded probe scheduling plan, and a scheduler-result-to-observation bridge are implemented and tested in `shared/smart-connect/`; run `make -C shared/smart-connect test`.
- The session coordinator can aggregate caller-owned per-server observation batches before ranking. The bridge maps scheduler `FAILED`/`TIMED_OUT` states to failed observations with no metric mask; a `SUCCEEDED` state still requires caller-supplied measurement data. The aggregator and scheduler read no clock and perform no network I/O or task cancellation. Tunnel success still depends on a caller-reported verified outcome; real probes, tunnel establishment/verification and engine-driven fallback remain unimplemented. No ICMP-only ranking or invented metrics.
- Local configuration management must not require a backend. No database/cache is selected.

## Experimental CSV ranking tool

A small command-line experiment is available for testing the current Smart Connect C ranking and aggregation logic against measurements exported from your own probe runner. It **does not run probes, contact servers, or verify a VPN tunnel**; it only processes caller-supplied data. The repository's demo fixture is synthetic, not real measurement data.

Build it with GCC or Clang:

```sh
make -C shared/smart-connect demo
NOW_MS=$(python3 -c 'import time; print(time.monotonic_ns() // 1_000_000)')
/tmp/v2speed-csv-rank observations.csv "$NOW_MS" 60000 10000
```

The CSV header must be exactly:

```text
server_id,timestamp_ms,success,measured_mask,throughput_mbps,latency_ms
```

`measured_mask` is a bit mask: `1` = throughput (Mbps), `2` = latency (ms), and `3` = both. Failed attempts use `success=0` and `measured_mask=0`; successful attempts set `success=1`. Use a consistent monotonic millisecond timebase for all rows. Supply `NOW_MS` from that same timebase so stale and future samples can be rejected correctly; do not use wall-clock timestamps. The remaining command-line numbers are maximum sample age and recent-success window in milliseconds, with the recent window no greater than the maximum age. If measurements and the command run on the same host using Python's monotonic clock, the example command obtains a compatible current timestamp. Unmeasured values should be `0` and are ignored according to the mask. IDs must be unique per server, at most 63 bytes, and cannot contain commas or quotes. Limits are 256 servers and 64 rows per server.

All imported servers are treated as eligible candidates; exclude unauthorized, expired or incompatible endpoints before importing. The result is an experimental ranking from caller-reported observations, not an independent confirmation of their authenticity or of real-world VPN performance. Run `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s shared/smart-connect/web-demo -p 'test_*.py'` for parser tests, `make -C shared/smart-connect test` for the domain suites and CLI smoke cases, or CMake/CTest for the seven C tests.

For a no-terminal first browser prototype, `shared/smart-connect/web-demo/` contains a standard-library Python app that fetches the three requested subscription files through fixed GitHub API paths, decodes VLESS/VMess/Trojan/Shadowsocks entries, and lets the user filter, favorite nodes locally by non-secret URI hash, and copy a chosen URI. It uses the supplied V2Speed wordmark. Public source files are fetched live and cached for five minutes; config contents are kept server-side until a user requests copy, and never saved to the repository. No connection or probe is run. The `/csv` page remains the separate caller-measurement ranking tool.

Run the browser prototype in a trusted local environment with `python3 shared/smart-connect/web-demo/server.py`, then open the shown address. On HTTPS it can be added to the home screen as a PWA; only the static app shell is cached, never the subscription/config APIs. This is an installable catalog shell, not a VPN client. The server needs outbound access to `api.github.com`; it binds to `0.0.0.0` for the hosted Arena preview. Subscription data has no clear redistribution license: the first source README says configs are free to use but also asserts all rights reserved; no license file was found in the inspected snapshots of the other two. We fetch rather than bundle the lists, attribute their sources, and require legal review before redistributing them in a production/proprietary app.

## Tooling / verification limitations

At this checkout, GCC, Python 3 and Node.js are available; GCC was used for the portable domain core. Java/JDK, Gradle executable, Kotlin compiler, Android SDK/ADB, Go, Xcode/Swift and Apple Network Extension entitlements are not available. Therefore native client build/runtime verification cannot be performed here. The exact missing test evidence is recorded rather than presented as complete.

# Silent-Failure Review

**Review date:** 2026-11-14 UTC
**Scope:** current portable C Smart Connect scorer, connection reducer, fallback planner, session coordinator, and their public headers. There is no Android/iOS app, engine adapter, probe implementation, UI, or logging subsystem in this repository yet; this review is not a whole-product security assessment.

## Findings

| Area | Observation | Disposition |
|---|---|---|
| Scorer input and output | Public APIs return typed status codes. `out_count` is set to zero before batch validation; the complete input batch is validated before ranking output is copied, so malformed data does not leave a partial ranking. Ineligible or unmeasured candidates are intentionally omitted, not reported as scoring failures. | Tested for invalid input, output capacity, duplicate IDs, missing metrics, no eligible results, and transactional output behavior. Callers must inspect the status and candidate count. |
| Connection state | Invalid transitions and stale generations return explicit statuses without silently advancing the state. Duplicate starts return `BUSY`; explicit disconnect invalidates callbacks by advancing the generation when possible. | Tested in reducer suite, including generated event sequences and stale callbacks. The reducer does not synchronize with OS or UI state. |
| Fallback planner | Invalid plans, busy attempts, exhaustion, cancellation, and verification are distinct outcomes. Failed attempts advance in deterministic rank order; the planner does not retry silently. | Tested, including generated plans and retry-cap/exhaustion paths. The caller supplies the outcome; the planner does not connect or verify a tunnel. |
| Session coordinator | Fetch, ranking, candidate selection, and attempt completion return explicit `V2SSessionStatus` values. Invalid measurements and no eligible candidates move the reducer to `ERROR`; cancellation and stale generations are not treated as success. A `CONNECTED` transition depends solely on caller input `V2S_ATTEMPT_TUNNEL_VERIFIED`. | Tested for failure, cancellation, exhaustion, stale generation, invalid data, caller mutation, and busy-attempt behavior. A dishonest or incorrect native caller can still claim verification; no independent proof exists here. |
| Return-value handling | Review of all four production C sources found no ignored non-void domain/API return values on the normal/error paths. Explicit `(void)` casts are used for `memset`/`memcpy`, which return no status. No return value is available from OS/core operations because none are implemented. | Scoped source review; repeat when native adapters are introduced. |
| User-facing errors and diagnostics | No UI, localization, notification, log, telemetry, or diagnostic export exists, so no user-facing error-loss or secret-redaction claim can be made. | Blocked on the mobile app and adapter. New adapter errors must be typed, surfaced accessibly, and tested for secret redaction; raw profile text and credentials must not be logged. |
| CI signals | The local `make` and CMake/CTest test paths pass. Workflow YAML parses locally, but GitHub Actions has not been executed, so remote CI status is unknown. | Keep release/merge checks marked unverified until an actual Actions run is observed. |

## Verification evidence

- `make -C shared/smart-connect test`: passed all 4 suites (17 scorer, 14 reducer, 11 fallback, 10 session tests).
- CMake 4.4.5 configure/build plus CTest: passed all 4 suites.
- All four test executables passed AddressSanitizer and UndefinedBehaviorSanitizer with leak detection enabled; GCC `-fanalyzer` passed for the four production sources.
- This evidence covers deterministic domain logic only. It does not establish real measurements, tunnel readiness, platform lifecycle synchronization, or safe error rendering.

## Required re-review triggers

Repeat this review when adding network probes, engine/core bindings, Android `VpnService`, iOS `NetworkExtension`, profile import/storage, logs/diagnostics, UI error mapping, or automatic retry/background behavior. Add tests ensuring each failure is observable, cleans up owned resources, preserves prior valid data where applicable, and cannot be displayed as connected or successful without platform/core acknowledgement.

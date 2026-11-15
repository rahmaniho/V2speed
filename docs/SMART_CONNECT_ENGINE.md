# Smart Connect and Connection Domain Core

**Status:** deterministic scoring, a generation-safe state reducer, a bounded fallback planner, and a session coordinator are implemented in portable C and locally tested. The coordinator joins ranking, ordered attempts, and reducer transitions; these modules are not a benchmark client, VPN engine, Android/iOS adapter, or complete Smart Connect feature.

## Files and commands

- Scoring C ABI: `shared/smart-connect/include/v2speed/smart_connect.h`
- Connection state C ABI: `shared/smart-connect/include/v2speed/connection_state.h`
- Session coordinator C ABI: `shared/smart-connect/include/v2speed/smart_connect_session.h`
- Implementations: `shared/smart-connect/src/`
- Deterministic tests: `shared/smart-connect/tests/`
- Run: `make -C shared/smart-connect test`
- CI: `.github/workflows/smart-connect.yml`

The code performs no network access, storage, threading, clock reads, memory allocation, logging, packet processing, or secret handling. Scoring callers must provide validated, freshly measured values from appropriate probes. The function cannot establish that a value came from a real network measurement.

## Scoring contract

| Metric | Weight | Direction / allowed range |
|---|---:|---|
| Actual tunnel throughput | 30% | Higher is better; finite Mbps ≥ 0 |
| Latency | 25% | Lower is better; finite milliseconds ≥ 0 |
| Reliability | 20% | Higher is better; `[0,1]` |
| Stability | 15% | Higher is better; `[0,1]` |
| Recent success | 10% | Higher is better; `[0,1]` |

For each metric, normalize observed eligible candidates against the batch min/max. Equal or single observations map to neutral `0.5`, not 1.0. Missing measurements contribute neither score nor evidence coverage; scores are **not** renormalized to available weights. Therefore an unmeasured candidate cannot earn a perfect score simply because its one observed metric is best. `evidence_coverage` is the sum of measured feature weights and makes partial evidence visible. A server must have at least one measurement and be marked eligible to enter the result.

Ordering is deterministic: score descending, evidence coverage descending, then stable server ID ascending. The caller is responsible for excluding unhealthy, expired, unauthorized, or protocol-incompatible profiles before marking them eligible.

Weights are configurable per ranking call through a supplied five-byte vector; it must sum to 100. The initial/default policy is the table above. No dynamic/adaptive weight learning is implemented.

Input batches are capped at 256. IDs must be non-empty, NUL-terminated within a fixed buffer, and unique. Only fields marked measured are validated; measured numeric values must be finite and in range. The API validates the full batch before modifying result storage, uses caller-provided output capacity, and reports explicit status codes.

## Connection state contract

`V2SConnectionMachine` provides a pure transition model for `DISCONNECTED`, `FETCHING`, `BENCHMARKING`, `CONNECTING`, `CONNECTED`, `RECONNECTING`, and `ERROR`.

- `begin_smart` is accepted only from disconnected/error, increments a 64-bit generation and enters fetching. A repeated start while busy returns `BUSY` without changing generation.
- `FETCH_COMPLETED → BENCHMARKING → BENCHMARK_COMPLETED → CONNECTING → TUNNEL_ESTABLISHED → CONNECTED` is an explicit path; any late callback from another generation returns `STALE_GENERATION`.
- A network-loss event from connected moves to reconnecting; a successful tunnel acknowledgement can restore connected.
- Explicit reconnect increments generation so callbacks from the old attempt cannot overwrite it.
- Operation failure is observable as error. User disconnect invalidates in-flight callbacks and returns to disconnected. Repeated disconnect is idempotent.
- Invalid transitions do not mutate machine state. Generation overflow is explicit; disconnect remains possible even at `UINT64_MAX`, and the disconnected machine cannot be advanced by stale events.

This is a small state reducer, not a platform state synchronizer: it has no mutex and must be called from a serialized native coordinator. Its transitions do not prove an OS tunnel exists; the platform adapter must send `TUNNEL_ESTABLISHED` only after OS/core confirmation.

## Bounded fallback planner

`fallback_planner.h/.c` copies the ranked candidate indices into caller-owned planner state so mutation/lifetime of the original ranking buffer cannot reorder later attempts. It permits one in-flight attempt at a time, caps requested attempts at 8, chooses candidates strictly in rank order, advances only after `FAILED`, and becomes terminal on `CANCELLED` or `TUNNEL_VERIFIED`. Exhaustion is explicit. The caller must report `TUNNEL_VERIFIED` only after both the selected core and OS tunnel confirm readiness. The planner itself performs no connection and cannot verify that claim.

## Session coordinator

`smart_connect_session.h/.c` coordinates a generation-scoped session across fetch completion, ranking, serial candidate requests, attempt results, and reducer transitions. It snapshots candidate IDs rather than retaining caller-owned measurement storage, returns `BUSY` when an attempt is already in flight, rejects stale generations, and exposes exhaustion/errors. It enters `CONNECTED` only after the caller reports `V2S_ATTEMPT_TUNNEL_VERIFIED`. That enum is an external assertion, not evidence checked by this library: no probes run, no tunnel is established or verified, and the coordinator is not thread-safe. Native callers must serialize access and report verified success only after core and OS confirmation.

## TDD and verification record

### RED

The score suite first ran against an empty scorer stub; its actual non-zero output included:

```text
FAIL shared/smart-connect/tests/test_smart_connect.c:43 [complete_measurements_rank_best_candidate_first]: count == 2
FAIL shared/smart-connect/tests/test_smart_connect.c:59 [missing_measurements_reduce_score_and_coverage]: count == 2
8 test(s) failed.
```

The connection suite ran against an unimplemented state-transition stub; the captured output included:

```text
FAIL shared/smart-connect/tests/test_connection_state.c:22 [successful_smart_connect_lifecycle]: v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK
10 connection-state test(s) failed.
```

The configurable-weight test was then added first and failed against its temporary `INVALID_WEIGHTS` stub before the weighted implementation was added. The fallback-planner suite was test-first too; actual output included:

```text
FAIL shared/smart-connect/tests/test_fallback_planner.c:29 [attempts_follow_rank_order_and_fallback_after_failure]: v2s_fallback_init(&planner, ranked, 3, 3) == V2S_FALLBACK_OK
8 fallback-planner tests failed.
```

An initial connection-state compile caught a missing standard header; it was corrected before capturing the actual test-level RED. The session coordinator was also introduced test-first: its initial nine-test suite failed against an unimplemented stub before implementation; a tenth test was later added for caller-owned input snapshotting and preservation of output when a concurrent candidate request returns `BUSY`. The final ten-test suite passes. No failure output is invented.

An intermediate score-test run caught a flawed test fixture: unopposed single reliability/history observations correctly normalize to neutral. The fixture was corrected by adding comparison samples; scoring behavior was not changed to satisfy an incorrect expected result.

### GREEN / refactor / regression

- `make -C shared/smart-connect test` — **passed**: 17 scoring, 14 connection-state, 11 bounded fallback-planner, and 10 session-coordinator tests. Generated checks cover 1,000 ranking batches, 10,000 reducer events, and 250 fallback plans.
- All four C test executables compiled with `-Wall -Wextra -Werror -pedantic -fsanitize=address,undefined`; all **passed** with leak detection enabled.
- GCC `-fanalyzer` compiled all four production C sources with strict warnings — **passed**.
- GCC/gcov session-coordinator coverage: **86.84% of lines (114 total)**, **98.06% of branches executed**, and **66.99% of branch outcomes taken at least once**. Coverage output was temporary.
- CMake 4.4.5 configure/build and CTest — **passed** all 4 suites (scorer, connection state, fallback planner, session coordinator). CMake was installed into `/tmp` for this run; no build artifacts were added to the repository. The gcov figures below for scorer, reducer, and fallback planner remain from their earlier coverage run.
- GCC/gcov scoring source: **100.00% source lines executed (102/102)**, **100.00% branches executed (101/101)**, **96.04% branch outcomes taken at least once**.
- GCC/gcov connection-state source: **100.00% source lines executed (67/67)**, **100.00% branches executed (56/56)** and **100.00% branch outcomes taken at least once**. Fallback planner: **100.00% lines (53/53)**, **100.00% branches (58/58)** and **91.38% branch outcomes taken at least once**.
- Coverage binaries/data were stored under temporary directories only.

Tests include 1,000 generated ranking batches, 10,000 generated state events, 250 generated fallback plans, normalization, missing data, custom weights, comparative latency, equal/single observations, evidence and ID tiebreaks, health filtering, invalid inputs and IDs, duplicate detection, bounded candidate count, transactional output behavior, connection lifecycle, failed attempt/retry, duplicate starts, stale generations, network loss/recovery, invalid transitions, repeated disconnect, generation exhaustion, rank-plan snapshotting, serial fallback, verified-stop, cancellation, attempt caps and exhaustion.

## Explicitly not implemented or verified

- Server discovery, protocol-aware reachability, handshake/connection-time checks, actual tunnel throughput measurement, packet loss, stability sampling, historical outcomes, cache expiry, adaptive weights, concurrency limits and cancellation.
- Attempting selected servers, verifying that the OS tunnel is active, engine-driven fallback after real connection failure, retry scheduling, anti-flap switching and network-change recovery.
- Kotlin/JNI or Swift integration, Android/NDK/Xcode app builds, app UI, mobile runtime or physical-device VPN tests. The portable CMake/CTest path passed; this does not validate a native mobile build.
- Any performance improvement claim. These domain functions have no product-device benchmark.

No machine-learning claim is made. The weights are an initial deterministic policy, not an empirically optimized model.

# Smart Connect and Connection Domain Core

**Status:** deterministic scoring, timestamp-aware observation aggregation, a generation-safe state reducer, bounded fallback and probe-scheduling plans, and a session coordinator are implemented in portable C and locally tested. These modules define domain behavior only; they are not real probe executors, a VPN engine, Android/iOS adapters, or a complete Smart Connect feature.

## Files and commands

- Scoring C ABI: `shared/smart-connect/include/v2speed/smart_connect.h`
- Connection state C ABI: `shared/smart-connect/include/v2speed/connection_state.h`
- Session coordinator C ABI: `shared/smart-connect/include/v2speed/smart_connect_session.h`
- Measurement aggregation and scheduler-observation bridge C ABI: `shared/smart-connect/include/v2speed/smart_connect_measurements.h`
- Probe scheduling plan C ABI: `shared/smart-connect/include/v2speed/smart_connect_probe_plan.h`
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

## Bounded probe-observation aggregation

`smart_connect_measurements.h/.c` converts up to 64 caller-reported observations for one server into a `V2SServerMeasurement`. The caller supplies monotonic timestamps, a success outcome, and optional observed throughput/latency. Samples outside `max_age_ms` are ignored; future timestamps or malformed observations reject the whole batch. Throughput and latency use medians of fresh successful samples. Reliability is successful fresh attempts divided by all fresh attempts; recent success is the same ratio within a caller-defined recent time window. Metrics with no samples remain unmeasured, not synthetic zeroes. With at least two throughput samples at distinct timestamps, stability is the bounded value `1 / (1 + CV)`, where CV is population standard deviation divided by the mean; an all-zero sample set yields 1.0. This is an explicit initial policy, not empirically calibrated. Stability remains unmeasured with fewer than two throughput samples or without two distinct timestamps.

The API validates the entire batch before writing output and returns `NO_FRESH_ATTEMPTS` if nothing fresh remains. It reads no clock and performs no networking. Timestamps must use one monotonic timebase, and the caller's success/metric assertions are not independently verifiable. The resulting `V2SServerMeasurement` can be passed directly to the scorer; an integration test covers that ranking handoff. A real protocol-aware probe and tunnel-throughput measurement still need to provide these observations.

## Bounded probe scheduling plan

`smart_connect_probe_plan.h/.c` is a pure dispatch-state model for up to 256 candidate probes and a caller-selected concurrency limit capped at 16. It records monotonic start times, per-probe timeouts, and a global deadline. The caller calls `expire(now_ms)` to collect timed-out in-flight indices and free slots. At the global deadline, unfinished attempts become `TIMED_OUT` and never-started candidates become `NOT_STARTED`, not failed measurements. Cancellation returns active indices so the caller can cancel owned work; the planner itself does not stop tasks. It performs no I/O or clock reads and is not thread-safe; the adapter must serialize calls and enforce the returned plan.

The API distinguishes `BUSY`, `DEADLINE_REACHED`, `TIMED_OUT`, `COMPLETE`, and `CANCELLED`; boundary checks are inclusive and output buffers are transactional. When an adapter records a timeout in the observation aggregator, it must report a failed attempt with no metric mask; it must not fabricate zero throughput/latency. `v2s_measurement_observation_from_plan` bridges a terminal plan task into the observation aggregator: failed/time-out tasks become unsuccessful attempts with no measured metrics, and a succeeded task requires caller-supplied observed metrics; pending, active, never-started and cancelled states are not counted as attempts. The caller supplies the timestamp and remains responsible for the authenticity of both observations and any tunnel-verification claim.

This verifies a bounded scheduling policy and a domain-state-to-observation mapping only—actual concurrent probes, cancellation propagation, and deadlines on network/core operations remain unimplemented.

Scorer input batches are capped at 256. IDs must be non-empty, NUL-terminated within a fixed buffer, and unique. Only fields marked measured are validated; measured numeric values must be finite and in range. The API validates the full batch before modifying result storage, uses caller-provided output capacity, and reports explicit status codes.

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

The probe-plan suite was run against an unimplemented stub; the captured RED run failed all nine tests at initialization. The bounded scheduler was then implemented and its full suite passed. The observation-aggregator suite was run against a stub that returned `INVALID_ARGUMENT`; the actual initial run failed the seven then-existing tests, including the medians/rates, stale-sample, failed-attempt, future/malformed, and no-fresh-window cases. After the initial aggregator passed, a stability test was added and actually failed because repeated throughput samples did not yet yield a stability metric. The formula was implemented and checked for repeated, single, same-timestamp, all-zero, and extreme finite samples. A generated-window invariant test and scheduler-to-observation bridge tests were later added; the bridge tests initially failed to link because the conversion API was not implemented. The final 15 measurement tests pass, including 500 generated windows and 500 generated scheduler handoffs. An initial connection-state compile caught a missing standard header; it was corrected before capturing the actual test-level RED. The session coordinator was also introduced test-first: its initial nine-test suite failed against an unimplemented stub before implementation; a tenth test was later added for caller-owned input snapshotting and preservation of output when a concurrent candidate request returns `BUSY`. The final ten-test suite passes. No failure output is invented.

An intermediate score-test run caught a flawed test fixture: unopposed single reliability/history observations correctly normalize to neutral. The fixture was corrected by adding comparison samples; scoring behavior was not changed to satisfy an incorrect expected result.

### GREEN / refactor / regression

- `make -C shared/smart-connect test` — **passed**: 17 scoring, 14 connection-state, 11 bounded fallback-planner, 10 session-coordinator, 15 measurement-aggregation, and 14 probe-plan tests. Generated checks cover 1,000 ranking batches, 10,000 reducer events, 250 fallback plans, 500 observation windows, 500 scheduler-to-observation handoffs, and 500 generated probe schedules at varied limits, including the 256-candidate/16-concurrency maximum, plus concurrency caps, timeout/deadline boundaries, cancellation and transactional expiry results.
- All six C test executables compiled with `-Wall -Wextra -Werror -pedantic -fsanitize=address,undefined`; all **passed** with leak detection enabled.
- GCC `-fanalyzer` compiled all six production C sources with strict warnings — **passed**.
- GCC/gcov measurement-aggregator coverage: **99.25% of 133 lines**, **100.00% of branches executed**, and **93.18% of branch outcomes taken at least once**.
- GCC/gcov probe-plan coverage: **97.66% of 128 lines**, **100% of branches executed**, and **83.72% of branch outcomes taken at least once**.
- GCC/gcov session-coordinator coverage: **86.84% of lines (114 total)**, **98.06% of branches executed**, and **66.99% of branch outcomes taken at least once**. Coverage output was temporary.
- CMake 4.4.5 configure/build and CTest — **passed** all 6 suites (scorer, connection state, fallback planner, session coordinator, observation aggregation, probe plan). CMake was installed into `/tmp` for local verification; GitHub Actions runs both the Make suite and CMake build/CTest, plus ASan/UBSan-instrumented tests and GCC `-fanalyzer` on all production sources. `CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' make -C shared/smart-connect test` is the local sanitizer command. No build artifacts were added to the repository.
- GCC/gcov scoring source: **100.00% source lines executed (102/102)**, **100.00% branches executed (101/101)**, **96.04% branch outcomes taken at least once**.
- GCC/gcov connection-state source: **100.00% source lines executed (67/67)**, **100.00% branches executed (56/56)** and **100.00% branch outcomes taken at least once**. Fallback planner: **100.00% lines (53/53)**, **100.00% branches (58/58)** and **91.38% branch outcomes taken at least once**.
- Coverage binaries/data were stored under temporary directories only.

Tests include 1,000 generated ranking batches, 10,000 generated state events, 250 generated fallback plans, fresh/stale timestamp policy, median aggregation, missing measurements, successful/failed attempt ratios, normalization, missing data, custom weights, comparative latency, equal/single observations, evidence and ID tiebreaks, health filtering, invalid inputs and IDs, duplicate detection, bounded candidate count, transactional output behavior, connection lifecycle, failed attempt/retry, duplicate starts, stale generations, network loss/recovery, invalid transitions, repeated disconnect, generation exhaustion, rank-plan snapshotting, serial fallback, verified-stop, probe concurrency bounds, per-probe/global deadlines, late results, never-started expiry, cancellation index lists, attempt caps and exhaustion.

## Explicitly not implemented or verified

- Server discovery, protocol-aware reachability, handshake/connection-time checks, real tunnel throughput measurement, packet loss, protocol-aware longitudinal stability sampling, historical outcomes, cache expiry, adaptive weights, adapter-enforced concurrency and cancellation propagation.
- Attempting selected servers, verifying that the OS tunnel is active, engine-driven fallback after real connection failure, retry scheduling, anti-flap switching and network-change recovery.
- Kotlin/JNI or Swift integration, Android/NDK/Xcode app builds, app UI, mobile runtime or physical-device VPN tests. The portable CMake/CTest path passed; this does not validate a native mobile build.
- Any performance improvement claim. These domain functions have no product-device benchmark.

No machine-learning claim is made. The weights are an initial deterministic policy, not an empirically optimized model.

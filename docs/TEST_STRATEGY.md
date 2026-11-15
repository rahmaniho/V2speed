# Test Strategy and Critical Behavior Inventory

**Status:** critical behavior plan plus an initial tested Smart Connect scoring component. The pure C scoring library is not a VPN engine or mobile app. The named `/...` commands, agents and skill packages are unavailable in this environment; native VPN flows remain untested.

## TDD workflow for code changes

For each behavior: write a focused failing test; actually run it and retain the output; implement minimal code; rerun that test; refactor; rerun all relevant tests. If build tooling is missing, mark the behavior `Not run` and block its verification claim. Do not fabricate red output.

## Critical behavior inventory

| # | Behavior | Business / security / data / access risk | Test level and isolated data | Expected result / failure conditions |
|---:|---|---|---|---|
| 1 | VPN permission denial | Access; no user consent must mean no tunnel | Platform UI/integration; fake permission result | Remain disconnected; accessible explanation and recover action; never report connected |
| 2 | Successful tunnel establishment | Business/security | Adapter contract unit + controlled network integration + physical device | Core and OS acknowledge active tunnel; expected routes/DNS apply |
| 3 | Failed tunnel establishment | Business/security | Fake engine failure then controlled integration | Typed error surfaced, resources closed, connected state never emitted |
| 4 | Correct disconnect | Security/data | Coordinator unit + platform lifecycle | Traffic path/tunnel stopped, fd/core released, state synchronized |
| 5 | State synchronization | Business/access | Portable reducer unit tests pass for state transitions/generation IDs; platform callbacks not yet wired | Reducer rejects stale events; actual UI/notification/provider synchronization remains unverified |
| 6 | Invalid config rejection | Security/data | Parser property/unit test with synthetic fixtures | Reject before save/connect; redact secret content from error |
| 7 | Valid config parsing | Business/data | Protocol golden fixtures and round-trip tests | Normalized fields preserved; unsupported option explicitly rejected |
| 8 | Subscription update failure | Data/business | Local isolated HTTPS server / injected transport | Prior profiles intact, bounded error; no destructive partial replacement |
| 9 | Duplicate prevention | Data | Repository test with canonical synthetic fixtures | Same logical configuration not silently duplicated; distinct secrets not conflated |
| 10 | Smart Connect ranking | Business | Implemented portable C scoring and bounded raw-observation aggregation; 17 scorer tests plus 10 aggregation tests; sanitizer run | Correct normalized ordering; unmeasured data not perfect score. Scorer unit tests pass; real measurement and connection flow remain unverified |
| 11 | Benchmark timeout | Availability | Fake clock/probe with deadline | Probe times out within policy; candidate marked unavailable/unmeasured |
| 12 | Benchmark cancellation | Battery/availability | Controlled coroutine/task cancellation | Work promptly stops; cancellation not logged/reported as success |
| 13 | Fallback | Reliability | Bounded planner unit tests use synthetic ranked candidate indices and simulated outcomes | Failure advances in rank order; only caller-reported tunnel verification stops retries. Real engine fallback remains unverified |
| 14 | Retry cap | Availability/battery | Planner test with controlled candidate count and max attempts | Attempts bounded at configured limit (hard cap 8); exhaustion is explicit. Time-based retry scheduling remains unimplemented |
| 15 | Network transition | Reliability/security | Platform callback fake + device Wi-Fi/cellular scenarios | Old probes canceled; reconnect per policy; no stale success |
| 16 | App restart with active tunnel | Reliability/access | Process recreation/platform integration | Reconcile actual OS/provider/core state; no duplicate tunnel start |
| 17 | DNS routing | Privacy/security | Controlled DNS servers and packet capture/network test | Queries use configured route or report failure; no silent fallback |
| 18 | IPv4 and IPv6 routing | Privacy/security | v4-only, v6-only, dual-stack controlled network | Route expectations verified independently for both address families |
| 19 | Secure credential storage | Security/data | Platform storage unit/instrumentation with test key | Secrets encrypted/protected; no plaintext logs/export |
| 20 | Unauthorized profile access | Access/security | Store authorization boundary test | Unauthorized caller cannot enumerate/read profile material |
| 21 | Subscription entitlement | Business/access | Fake entitlement issuer and expired/revoked fixtures | Reject invalid/expired entitlements without trusting UI state |
| 22 | Sensitive log redaction | Security/privacy | Structured logger tests with sentinel passwords/tokens/keys/URLs | No secret sentinel appears in any log/report path |
| 23 | Concurrent connect requests | Reliability | Reducer rejects a duplicate start; session coordinator returns `BUSY` for a second candidate request during an in-flight attempt. Thread-level races are not tested | Duplicate operations rejected in the tested serialized domain API; native synchronization still required |
| 24 | Connection state races | Reliability/access | State reducer tests cover stale generation IDs; concurrent platform callbacks not exercised | Late-generation event cannot overwrite state; native callback race testing remains pending |
| 25 | Statistics correctness | Data/trust | Fake monotonically increasing counters/reset/overflow | Deltas, rates, session duration correct; unavailable counter shown missing |

## Test levels and isolation

- **Pure domain unit tests:** scoring, normalized metrics, state transitions, retry/anti-flap rules, config validation. Inject clock, random source, IDs, and probes.
- **Adapter contract tests:** fake engine and controlled callbacks; assert cancellation, cleanup, error mapping and generation handling.
- **Network integration:** local isolated HTTP(S), DNS and proxy/tunnel test servers with synthetic credentials; never production endpoints.
- **Platform integration:** permission, service/extension lifecycle, notifications and secure storage on OS runtime; emulator where supported and physical device for tunnel proof.
- **End-to-end:** separate test fleet/account with explicit authorization; validate actual routed traffic, DNS, fallback and network transitions. Never destructive against production VPN servers.
- **Security/privacy:** static scans/SBOM plus targeted secret-redaction, config fuzzing, TLS verification and leak scenarios. A clean scan alone is not a security proof.

## Planned quality gates

- Ranking, configuration validation and connection state-machine domain logic: target >=90% branch-relevant coverage; meaningful edge-case assertions mandatory. Scorer/reducer/planner GCC/gcov results are recorded below; session orchestration and platform adapters are not covered by gcov.
- Other shared domain logic: target >=80%; UI tests cover critical states/actions and accessibility semantics rather than unstable screenshot snapshots.
- Each threshold needs a configured report and baseline before it can be described as measured.

## Intentionally excluded test types

- Uncontrolled public server pings/throughput as deterministic unit tests: network and server load are nondeterministic; use controlled probes and separate real-device benchmark runs.
- Tests of third-party core internals outside V2Speed ownership: use documented adapter contracts and pinned compatibility integration tests.
- Pure visual pixel snapshots without stable acceptance criteria: they do not prove accessibility, state, routing or tunnel behavior. Use focused rendering/semantics checks.
- Tests requiring inaccessible Apple entitlements in this environment: do not fake them; mark `Not run` until an entitled Xcode/device runner exists.
- Language-specific skill workflows for languages not selected: do not add a language just to activate a skill.

## Execution status

Behaviors 5, 10, 13, 14, 23 and 24 have portable unit coverage only: 17 scoring, 14 state-machine, 11 fallback-planner, 10 session-coordinator and 10 observation-aggregation tests pass under GCC, AddressSanitizer and UndefinedBehaviorSanitizer. Generated tests cover 1,000 ranking batches, 10,000 reducer events, 250 fallback plans and 500 observation windows. GCC `-fanalyzer` passed on all five production C sources. `gcov` measured 100% source-line and branch execution for scorer, reducer and planner; branch outcomes taken at least once are scorer 96.04%, state reducer 100%, fallback planner 91.38%. Observation-aggregation gcov reported 99.06% of 106 lines, 100.00% of branches executed, and 91.51% of branch outcomes taken at least once. Session-coordinator gcov reported 86.84% of 114 lines, 98.06% of branches executed, and 66.99% of branch outcomes taken at least once. CMake 4.4.5 configure/build and CTest passed all five suites; the tool was installed into `/tmp` for verification. This verifies deterministic synthetic-input ranking, timestamp-aware aggregation of caller-supplied synthetic observations, serialized reducer/session orchestration and a bounded retry/fallback decision plan only. The coordinator accepts caller-reported tunnel verification; no real connection or tunnel is verified. Native thread races, UI/OS state synchronization, protocol-aware probes, measured throughput, actual engine connection attempts, tunnel verification and engine-driven fallback remain unimplemented. Behaviors 1–4, 6–9, 11–12, 15–22 and 25 remain **Not run** because the mobile app/engine, Android SDK/JDK, Go toolchain, Xcode/Apple entitlement, devices and VPN test infrastructure are absent.

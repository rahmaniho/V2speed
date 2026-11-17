# VPN Engine Selection

**Decision status:** Android MVP target is Android 10+ (API 29) and open-source distribution, per user. Xray through AndroidLibXrayLite is the preferred technical candidate for a native spike, not yet approved for production. The implementation gate remains open pending a reproducible Android build, license/SBOM review, API/ABI verification, and device tunnel tests. See `ANDROID_NATIVE_MVP.md`.

## Candidate assessment

| Candidate | Evidence from audit | Advantages | Risks / work remaining |
|---|---|---|---|
| Xray-core via AndroidLibXrayLite | `XTLS/Xray-core` observed at `5d17b872edde3024a5b906d9cd6c7a3ad4973d81`, MPL-2.0 `LICENSE`; wrapper `AndroidLibXrayLite` is LGPL-3.0 and Android-oriented | Broad protocol ecosystem; upstream wrapper exposes Android CoreController and a TUN file descriptor entry point | Preferred Android spike, not production-approved. Verify exact source commit/core version resolved by the wrapper, full Go dependency licenses/SBOM, Android ABIs, protocol parity, performance and crash behavior. MPL/LGPL obligations still apply; not legal clearance. No source copied. |
| v2fly/v2ray-core | Direct repository inspected at `ec5ce8ba78e8a70167b5b1f003287f1340ecc19a`, MIT `LICENSE`; v2rayNG README describes support | Established V2Ray implementation and permissive top-level license | Current checked v2rayNG CI evidence uses Xray AAR rather than proving v2fly packaged; check modern feature/protocol support, Android/iOS bindings, transitive dependencies and maintenance. MIT top-level license does not settle all dependency terms. |
| Platform-specific maintained cores | No candidate selected | Could fit platform APIs/entitlement/performance better | Can cause protocol/behavior divergence and double security maintenance; must compare using identical contract fixtures and real-network tests. |
| Handwritten V2Speed core | None | Full ownership | Rejected for initial product: high protocol/security scope and unacceptable time/risk; do not hand-roll cryptographic transports. |

## Selection gate / spike

1. Product/legal counsel approves distribution and component license strategy.
2. Produce SBOM and license report for exact core, wrapper, native libraries, and data files.
3. Build minimal Android and iOS prototypes that open the platform tunnel and pass a controlled protocol test through a core; no mock-only pass.
4. Confirm parser formats and runtime capabilities by protocol/version/transport/security field. Expose unsupported capabilities explicitly.
5. Test TLS verification, credential handling, v4/v6 routes, DNS, cancellation, network transition, and process/extension lifecycle.
6. Measure 3+ controlled runs per device/network/server for startup, tunnel establishment, throughput, memory, CPU and battery.
7. Decide shared versus platform-specific core based on measured results, license obligations and maintenance capacity; record version and upgrade cadence.

Until these gates pass, Smart Connect can be specified/tested as pure ranking logic against controlled measurements, but no production VPN connection can be claimed.

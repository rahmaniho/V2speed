# License Compliance and Reuse Gate

**Status: preliminary source-level audit only; not legal advice and not an approval to ship.** No code from the reference repositories or core repositories was copied into V2Speed in this milestone. Obtain qualified legal review before commercial distribution or any decision to make a proprietary/closed-source distribution.

## Audited repositories and directly observed licenses

| Component | Audited revision / evidence | Observed license | Implication / open work |
|---|---|---|---|
| `2dust/v2rayNG` | `141059d98402bb353400a42fce9d2ec8447cd70f`; repository `LICENSE` | GNU GPL-3.0, full license text present | Adapting/copying app code makes GPLv3 obligations central to the combined distribution. Preserve notices, provide corresponding source as required, and legal-review whether intended distribution is compatible. Do not copy code until approval. |
| `2dust/AndroidLibXrayLite` | submodule `b5a377087e74eaa34e86718710b04c50f4b767f`; `LICENSE` | GNU LGPL-3.0 text | Wrapper source has LGPL terms; LGPL notices/source and relinking/modification requirements need review. It depends on Xray-core. The project fetches its AAR in reference CI, so inspect the exact release artifact and accompanying notices, not only source. |
| `XTLS/Xray-core` | direct repository inspected at `5d17b872edde3024a5b906d9cd6c7a3ad4973d81`; `LICENSE` | Mozilla Public License 2.0 | MPL is file-level copyleft with source/notice obligations for covered files; distribution as part of a larger application can be possible subject to conditions. Verify source version resolved by wrapper's `go.mod` and all transitive dependencies, plus GoMobile/static-link implications. |
| `heiher/hev-socks5-tunnel` | submodule `c5584feca8f6df70ee53eb2879e034b849a3d15a`; `LICENSE` | MIT | Preserve copyright and permission notice in copies/substantial portions; inspect nested submodules/native build artifacts and their notices. |
| `v2fly/v2ray-core` | direct repository inspected at `ec5ce8ba78e8a70167b5b1f003287f1340ecc19a`; `LICENSE` | MIT | The reference README describes v2fly and Xray support, but current inspected Android CI builds the Xray wrapper. Do not infer that the app bundles or tests both. Any direct use still requires dependency/transitive notices review. |

## Distribution implications

1. The reference Android app is GPLv3. Commercial distribution is not forbidden by GPLv3, but distributing a derivative under a proprietary-only license is generally incompatible with GPLv3 source/distribution terms. Exact analysis requires counsel and a determination of whether the intended client is a derivative/combined work.
2. The Android wrapper is LGPLv3 and wraps an MPL-2.0 core; it is not one homogenous license. The HEV tunnel is MIT. Include and satisfy the correct notices for each shipped component and version.
3. Xray-core is a candidate for an independently written integration because its observed license differs from v2rayNG's GPL. This is **not clearance**: verify all transitive modules and copied/generated assets, AAR packaging, mobile bindings, iOS build/link mode, and exact release notices.
4. Protocol names/standards are not themselves a license grant. GeoIP/GeoSite data files and user-provided configurations need independent provenance/terms review.
5. No source, assets, code, or test data from `2dust/v2rayNG` has been imported. The supplied V2Speed images are user-provided product assets and are retained as raw originals (see `brand-assets/README.md`).

## Gate before implementation reuse / release

- [ ] Legal counsel confirms intended commercial/proprietary/open-source distribution and GPLv3/LGPLv3/MPL2 compatibility.
- [ ] Pin chosen core and wrapper to immutable version/commit, inspect their actual licenses and distribution artifacts.
- [ ] Generate full Android/iOS SBOM and dependency license report, including Go transitive modules, native libraries, generated files, and data files.
- [ ] Establish notice bundle and source-offer/source publication workflow for each license obligation; preserve upstream copyrights.
- [ ] Decide clean-room boundary. Only implement from public protocol specs and independently written adapter/interfaces until approved; do not paste or translate GPL code.
- [ ] Verify app store rules, cryptography/export requirements, trademark/artwork permission, privacy/legal obligations and network service jurisdictions.
- [ ] Re-run license tooling on release artifacts and review exclusions manually.

## Findings not closed in this audit

- The reference repository's Gradle dependencies and the complete transitive Xray Go module graph were not license-scanned. A published AAR may differ from repository source.
- `AndroidLibXrayLite` contains GeoIP/GeoSite data assets; their independent source/license/provenance was not audited.
- Nested third-party/native components in the HEV submodule and any AAR/native static link relationships require inventory.
- No lawyer reviewed the intended distribution model.
